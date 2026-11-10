/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; -*- */

/* AbiSource Program Utilities
 * Copyright (C) 2002-2004 Hubert Figuiere
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

/*
 * Toolbars used to live in borderless panels pinned below the menu bar and
 * shared by all document windows. They are now hosted inside each document
 * window, in a titlebar accessory, so they move, resize and go full screen
 * with the window and pick up the system titlebar material.
 */

#include "ut_debugmsg.h"
#include "ut_assert.h"

#include "ev_CocoaToolbar.h"

#include "xap_CocoaFrameImpl.h"
#include "xap_CocoaToolbarWindow.h"

static XAP_CocoaToolbarWindow_Controller * s_pSharedToolbar = nil;

static const CGFloat kToolbarHPadding = 8.0;
static const CGFloat kToolbarVPadding = 3.0;
static const CGFloat kToolbarGap      = 12.0;

/*!
 * Lays the toolbar views out left to right and wraps them to a new row
 * when the window is too narrow, like the GTK frontend does.
 */
@interface XAP_CocoaToolbarContainer : NSView
{
	NSArray *	m_toolbars;
	BOOL		m_inLayout;
}
- (void)setToolbars:(NSArray *)toolbars;
- (CGFloat)heightForWidth:(CGFloat)width;
- (CGFloat)minimumWidth;
@end

@implementation XAP_CocoaToolbarContainer

- (BOOL)isFlipped
{
	return YES;
}


- (void)setToolbars:(NSArray *)toolbars
{
	for (NSView * view in [[self subviews] copy]) {
		if (![toolbars containsObject:view]) {
			[view removeFromSuperview];
		}
	}
	m_toolbars = [toolbars copy];
	for (NSView * view in m_toolbars) {
		if ([view superview] != self) {
			[view removeFromSuperview];
			[view setAutoresizingMask:NSViewNotSizable];
			[self addSubview:view];
		}
	}
	[self layoutToolbars];
}

/* Returns the needed height; positions the views when apply is YES. */
- (CGFloat)layoutForWidth:(CGFloat)width apply:(BOOL)apply
{
	if ([m_toolbars count] == 0) {
		return 0.0;
	}
	CGFloat x = kToolbarHPadding;
	CGFloat y = kToolbarVPadding;
	CGFloat rowHeight = 0.0;
	for (NSView * view in m_toolbars) {
		NSSize size = [view frame].size;
		if (x > kToolbarHPadding && x + size.width > width - kToolbarHPadding) {
			x = kToolbarHPadding;
			y += rowHeight;
			rowHeight = 0.0;
		}
		if (apply) {
			[view setFrameOrigin:NSMakePoint(x, y)];
		}
		x += size.width + kToolbarGap;
		rowHeight = MAX(rowHeight, size.height);
	}
	return y + rowHeight + kToolbarVPadding;
}

- (CGFloat)heightForWidth:(CGFloat)width
{
	return [self layoutForWidth:width apply:NO];
}

- (CGFloat)minimumWidth
{
	CGFloat width = 0.0;
	for (NSView * view in m_toolbars) {
		width = MAX(width, [view frame].size.width);
	}
	return width + 2.0 * kToolbarHPadding;
}

- (void)layoutToolbars
{
	if (m_inLayout) {
		return;
	}
	m_inLayout = YES;
	NSRect frame = [self frame];
	CGFloat height = [self layoutForWidth:frame.size.width apply:YES];
	if (fabs(height - frame.size.height) > 0.5 || fabs(height - [[self superview] frame].size.height) > 0.5) {
		// With automaticallyAdjustsSize off, the titlebar accessory takes
		// its height from this view's frame. Changing it from inside
		// AppKit's titlebar layout is ignored, so apply it afterwards.
		[NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(applyHeight) object:nil];
		[self performSelector:@selector(applyHeight) withObject:nil afterDelay:0.0];
	}
	m_inLayout = NO;
}

- (void)applyHeight
{
	NSRect frame = [self frame];
	frame.size.height = [self heightForWidth:frame.size.width];
	m_inLayout = YES;
	[self setFrame:frame];
	m_inLayout = NO;
}

- (void)setFrameSize:(NSSize)newSize
{
	[super setFrameSize:newSize];
	[self layoutToolbars];
}

@end


@implementation XAP_CocoaToolbarWindow_Controller

+ (XAP_CocoaToolbarWindow_Controller *)sharedToolbar
{
	if (!s_pSharedToolbar)
	{
		s_pSharedToolbar = [[XAP_CocoaToolbarWindow_Controller alloc] initWithWindow:nil];
	}
	return s_pSharedToolbar;
}


- (id)initWithWindow:(NSWindow *)window
{
	if (![super initWithWindow:window])
	{
		return nil;
	}
	NSNotificationCenter * NC = [NSNotificationCenter defaultCenter];
	[NC addObserver:self selector:@selector(showToolbarNotification:) name:(XAP_CocoaFrameImpl::XAP_FrameNeedToolbar)    object:nil];
	[NC addObserver:self selector:@selector(hideToolbarNotification:) name:(XAP_CocoaFrameImpl::XAP_FrameReleaseToolbar) object:nil];
	return self;
}

- (void)dealloc
{
	[[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)removeAllToolbars
{
	m_current = nil;
}

- (void)autoResize
{
}

- (void)redisplayToolbars:(XAP_CocoaFrameController *)frame
{
	if (!m_lock)
	{
		[self _showAllToolbars:frame];
	}
}

static NSTitlebarAccessoryViewController * s_findToolbarAccessory(NSWindow * window)
{
	for (NSTitlebarAccessoryViewController * accessory in [window titlebarAccessoryViewControllers]) {
		if ([[accessory view] isKindOfClass:[XAP_CocoaToolbarContainer class]]) {
			return accessory;
		}
	}
	return nil;
}

- (void)_showAllToolbars:(XAP_CocoaFrameController *)wframe
{
	UT_ASSERT(wframe);
	NSWindow * window = [wframe window];
	if (!window) {
		return;
	}

	NSArray * toolbars = [wframe getToolbars];

	NSTitlebarAccessoryViewController * accessory = s_findToolbarAccessory(window);
	if (!accessory) {
		if ([toolbars count] == 0) {
			return;
		}
		CGFloat width = [[window contentView] frame].size.width;
		XAP_CocoaToolbarContainer * container =
			[[XAP_CocoaToolbarContainer alloc] initWithFrame:NSMakeRect(0, 0, width, EV_CocoaToolbar::getToolbarHeight())];
		// size it for the current window before it becomes the accessory
		[container setToolbars:toolbars];
		accessory = [[NSTitlebarAccessoryViewController alloc] init];
		[accessory setLayoutAttribute:NSLayoutAttributeBottom];
		// The row count (and so the height) changes with the window width.
		[accessory setAutomaticallyAdjustsSize:NO];
		[accessory setView:container];
		[window addTitlebarAccessoryViewController:accessory];
		// The toolbar replaces the titlebar separator line.
		[window setTitlebarSeparatorStyle:NSTitlebarSeparatorStyleLine];
	}

	XAP_CocoaToolbarContainer * container = (XAP_CocoaToolbarContainer *)[accessory view];
	[container setToolbars:toolbars];
	[accessory setHidden:([toolbars count] == 0)];

	// Never let the window get narrower than the widest toolbar row.
	NSSize minSize = [window contentMinSize];
	minSize.width = MAX([container minimumWidth], 480.0);
	[window setContentMinSize:minSize];

	NSRect contentRect = [window contentRectForFrameRect:[window frame]];
	if (contentRect.size.width < minSize.width) {
		contentRect.size.width = minSize.width;
		NSRect frame = [window frameRectForContentRect:contentRect];
		NSScreen * screen = [window screen] ? [window screen] : [NSScreen mainScreen];
		NSRect visible = [screen visibleFrame];
		if (NSMaxX(frame) > NSMaxX(visible)) {
			frame.origin.x = MAX(NSMinX(visible), NSMaxX(visible) - frame.size.width);
		}
		[window setFrame:frame display:YES];
	}
}

- (float)height
{
	// Toolbars no longer take space at the top of the screen.
	return 0.0f;
}

- (void)lock
{
	UT_ASSERT(m_lock == NO);
	m_lock = YES;
}

- (void)unlock
{
	UT_ASSERT(m_lock);
	m_lock = NO;
}

- (void)showToolbarNotification:(NSNotification*)notif
{
	XAP_CocoaFrameController * frame = [notif object];
	if (frame == m_current) {
		return;
	}
	m_current = frame;
	[self _showAllToolbars:frame];
}

- (void)hideToolbarNotification:(NSNotification*)notif
{
	if (m_current == [notif object]) {
		m_current = nil;
	}
}

@end
