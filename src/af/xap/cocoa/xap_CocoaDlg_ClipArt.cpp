/* AbiWord
 * Copyright (C) 2000 AbiSource, Inc.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  
 * 02110-1301 USA.
 */

#include <stdlib.h>
#include <time.h>

#include "ut_string.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"

// This header defines some functions for Cocoa dialogs,
// like centering them, measuring them, etc.
// #include "xap_CocoaDialogHelper.h"

#include "xap_App.h"
#include "xap_CocoaApp.h"
#include "xap_CocoaFrame.h"

#include "xap_Strings.h"
#include "xap_Dialog_Id.h"
#include "xap_Dlg_ClipArt.h"
#include "xap_CocoaDlg_ClipArt.h"
#include "xap_CocoaDialog_Utilities.h"

/*****************************************************************/

XAP_Dialog * XAP_CocoaDialog_ClipArt::static_constructor(XAP_DialogFactory * pFactory,
													 XAP_Dialog_Id id)
{
	XAP_CocoaDialog_ClipArt * p = new XAP_CocoaDialog_ClipArt(pFactory,id);
	return p;
}

XAP_CocoaDialog_ClipArt::XAP_CocoaDialog_ClipArt(XAP_DialogFactory * pDlgFactory,
										 XAP_Dialog_Id id)
	: XAP_Dialog_ClipArt(pDlgFactory,id)
{
}

XAP_CocoaDialog_ClipArt::~XAP_CocoaDialog_ClipArt(void)
{
}

/*****************************************************************/

/* One clip art item: image above its name. Double-click inserts. */
@interface XAP_CocoaClipArtButton : NSButton
@end

@implementation XAP_CocoaClipArtButton
- (void)mouseDown:(NSEvent *)event
{
	[super mouseDown:event];
	if ([event clickCount] == 2) {
		[NSApp stopModalWithCode:NSModalResponseOK];
	}
}
@end

@interface XAP_CocoaClipArtGrid : NSView
@end

@implementation XAP_CocoaClipArtGrid
- (BOOL)isFlipped
{
	return YES;
}
@end

@interface XAP_CocoaClipArtController : NSWindowController <NSWindowDelegate>
{
	NSArray *		m_paths;
	NSMutableArray *	m_buttons;
	NSInteger		m_selected;
	NSButton *		m_insertBtn;
}
- (id)initWithPaths:(NSArray *)paths;
- (NSString *)selectedPath;
@end

@implementation XAP_CocoaClipArtController

static const CGFloat kCellWidth = 112.0;
static const CGFloat kCellHeight = 108.0;
static const NSInteger kColumns = 5;
static const CGFloat kMargin = 20.0;

- (id)initWithPaths:(NSArray *)paths
{
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	const CGFloat gridWidth = kColumns * kCellWidth;
	const CGFloat scrollHeight = 3.3 * kCellHeight;
	NSRect contentRect = NSMakeRect(0, 0, gridWidth + 2 * kMargin + 2, scrollHeight + 2 * kMargin + 44);

	NSPanel * panel = [[NSPanel alloc] initWithContentRect:contentRect
		styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
		backing:NSBackingStoreBuffered defer:YES];
	if (![super initWithWindow:panel]) {
		return nil;
	}
	m_paths = [paths copy];
	m_buttons = [[NSMutableArray alloc] init];
	m_selected = -1;

	[panel setTitle:LocalizedString(pSS, XAP_STRING_ID_DLG_CLIPART_Title)];
	[panel setDelegate:self];
	NSView * content = [panel contentView];

	// buttons
	NSButton * cancel = [NSButton buttonWithTitle:@"Cancel" target:self action:@selector(cancelAction:)];
	LocalizeControl(cancel, pSS, XAP_STRING_ID_DLG_Cancel);
	[cancel setKeyEquivalent:@"\033"];
	m_insertBtn = [NSButton buttonWithTitle:@"Insert" target:self action:@selector(insertAction:)];
	LocalizeControl(m_insertBtn, pSS, XAP_STRING_ID_DLG_Insert);
	[m_insertBtn setKeyEquivalent:@"\r"];
	[m_insertBtn setEnabled:NO];
	for (NSButton * b in @[cancel, m_insertBtn]) {
		[b sizeToFit];
		NSRect f = [b frame];
		f.size.width = MAX(f.size.width, 96.0);
		[b setFrame:f];
	}
	NSRect insertFrame = [m_insertBtn frame];
	insertFrame.origin = NSMakePoint(NSMaxX(contentRect) - kMargin - insertFrame.size.width, kMargin - 4);
	[m_insertBtn setFrame:insertFrame];
	NSRect cancelFrame = [cancel frame];
	cancelFrame.origin = NSMakePoint(NSMinX(insertFrame) - 12 - cancelFrame.size.width, kMargin - 4);
	[cancel setFrame:cancelFrame];
	[content addSubview:cancel];
	[content addSubview:m_insertBtn];

	// grid of images
	NSRect scrollFrame = NSMakeRect(kMargin, kMargin + 40, gridWidth + 2, scrollHeight);
	NSScrollView * scroll = [[NSScrollView alloc] initWithFrame:scrollFrame];
	[scroll setHasVerticalScroller:YES];
	[scroll setBorderType:NSBezelBorder];
	[scroll setDrawsBackground:YES];
	[scroll setBackgroundColor:[NSColor controlBackgroundColor]];

	NSUInteger count = [m_paths count];
	NSInteger rows = (count + kColumns - 1) / kColumns;
	NSRect gridFrame = NSMakeRect(0, 0, gridWidth, MAX(scrollHeight - 2, rows * kCellHeight));
	XAP_CocoaClipArtGrid * grid = [[XAP_CocoaClipArtGrid alloc] initWithFrame:gridFrame];

	for (NSUInteger i = 0; i < count; i++) {
		NSString * path = [m_paths objectAtIndex:i];
		NSImage * image = [[NSImage alloc] initWithContentsOfFile:path];
		NSRect cell = NSMakeRect((i % kColumns) * kCellWidth + 4, (i / kColumns) * kCellHeight + 4,
								 kCellWidth - 8, kCellHeight - 8);
		XAP_CocoaClipArtButton * button = [[XAP_CocoaClipArtButton alloc] initWithFrame:cell];
		[button setButtonType:NSButtonTypePushOnPushOff];
		[button setBezelStyle:NSBezelStyleSmallSquare];
		[button setBordered:YES];
		[button setShowsBorderOnlyWhileMouseInside:YES];
		[button setImage:image];
		[button setImageScaling:NSImageScaleProportionallyDown];
		[button setImagePosition:NSImageAbove];
		[button setTitle:[[path lastPathComponent] stringByDeletingPathExtension]];
		[button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
		[button setToolTip:[path lastPathComponent]];
		[button setTag:(NSInteger)i];
		[button setTarget:self];
		[button setAction:@selector(itemAction:)];
		[grid addSubview:button];
		[m_buttons addObject:button];
	}
	if (count == 0) {
		NSTextField * empty = [NSTextField wrappingLabelWithString:LocalizedString(pSS, XAP_STRING_ID_DLG_CLIPART_Error)];
		[empty setAlignment:NSTextAlignmentCenter];
		[empty setTextColor:[NSColor secondaryLabelColor]];
		[empty setFrame:NSMakeRect(20, scrollHeight / 2 - 20, gridWidth - 40, 40)];
		[grid addSubview:empty];
	}
	[scroll setDocumentView:grid];
	[content addSubview:scroll];
	return self;
}


- (void)itemAction:(id)sender
{
	m_selected = [sender tag];
	for (NSButton * button in m_buttons) {
		BOOL on = ([button tag] == m_selected);
		[button setState:(on ? NSControlStateValueOn : NSControlStateValueOff)];
		// keep the selection visible after the mouse leaves
		[button setShowsBorderOnlyWhileMouseInside:!on];
	}
	[m_insertBtn setEnabled:(m_selected >= 0)];
}

- (void)insertAction:(id)sender
{
	UT_UNUSED(sender);
	if (m_selected >= 0) {
		[NSApp stopModalWithCode:NSModalResponseOK];
	}
}

- (void)cancelAction:(id)sender
{
	UT_UNUSED(sender);
	[NSApp stopModalWithCode:NSModalResponseCancel];
}

/* the close button must end the modal session too */
- (void)windowWillClose:(NSNotification *)notification
{
	UT_UNUSED(notification);
	if ([NSApp modalWindow] == [self window]) {
		[NSApp stopModalWithCode:NSModalResponseCancel];
	}
}

- (NSString *)selectedPath
{
	if (m_selected < 0 || m_selected >= (NSInteger)[m_paths count]) {
		return nil;
	}
	return [m_paths objectAtIndex:m_selected];
}

@end

/*!
 * The images of a clip art folder, sorted by name (sub-folders skipped,
 * like the GTK dialog).
 */
static NSArray * s_clipArtFiles(NSString * dir)
{
	NSMutableArray * files = [NSMutableArray array];
	NSArray * names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:dir error:nil];
	NSSet * extensions = [NSSet setWithArray:@[@"png", @"jpg", @"jpeg", @"gif", @"bmp", @"tif", @"tiff", @"svg"]];
	for (NSString * name in [names sortedArrayUsingSelector:@selector(localizedStandardCompare:)]) {
		if ([extensions containsObject:[[name pathExtension] lowercaseString]]) {
			[files addObject:[dir stringByAppendingPathComponent:name]];
		}
	}
	return files;
}

void XAP_CocoaDialog_ClipArt::runModal(XAP_Frame * /*pFrame*/)
{
	setAnswer(XAP_Dialog_ClipArt::a_CANCEL);

	NSArray * files = nil;
	if (const char * initial = getInitialDir()) {
		files = s_clipArtFiles([NSString stringWithUTF8String:initial]);
	}
	if ([files count] == 0) {
		// installed inside the application bundle (--enable-clipart)
		NSString * bundled = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"clipart"];
		files = s_clipArtFiles(bundled);
	}

	XAP_CocoaClipArtController * controller = [[XAP_CocoaClipArtController alloc] initWithPaths:files];
	NSWindow * window = [controller window];
	[window center];
	NSModalResponse response = [NSApp runModalForWindow:window];
	NSString * path = [controller selectedPath];
	[window orderOut:nil];

	if (response == NSModalResponseOK && path) {
		// like the GTK dialog, hand back a file URI
		setGraphicName([[[NSURL fileURLWithPath:path] absoluteString] UTF8String]);
		setAnswer(XAP_Dialog_ClipArt::a_OK);
	}
}
