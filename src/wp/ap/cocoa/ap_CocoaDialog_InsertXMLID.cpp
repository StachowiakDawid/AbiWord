/* AbiWord
 * Copyright (C) 2026 AbiWord contributors
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

#import <Cocoa/Cocoa.h>

#include "xap_App.h"
#include "xap_CocoaDialog_Utilities.h"
#include "ap_Strings.h"
#include "ap_CocoaDialog_InsertXMLID.h"

namespace {
enum { kInsert = 1, kDelete = 2, kCancel = 3 };
}

/* Ends the modal session with the clicked button's tag. */
@interface AP_CocoaDialog_InsertXMLIDTarget : NSObject
- (void)buttonAction:(NSButton *)sender;
@end

@implementation AP_CocoaDialog_InsertXMLIDTarget
- (void)buttonAction:(NSButton *)sender
{
	[NSApp stopModalWithCode:sender.tag];
}
@end

XAP_Dialog * AP_CocoaDialog_InsertXMLID::static_constructor(XAP_DialogFactory * pFactory,
                                                            XAP_Dialog_Id dlgid)
{
	return new AP_CocoaDialog_InsertXMLID(pFactory, dlgid);
}

AP_CocoaDialog_InsertXMLID::AP_CocoaDialog_InsertXMLID(XAP_DialogFactory * pDlgFactory,
                                                       XAP_Dialog_Id dlgid)
	: AP_Dialog_InsertXMLID(pDlgFactory, dlgid)
{
}

AP_CocoaDialog_InsertXMLID::~AP_CocoaDialog_InsertXMLID(void)
{
}

void AP_CocoaDialog_InsertXMLID::runModal(XAP_Frame * /*pFrame*/)
{
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	AP_CocoaDialog_InsertXMLIDTarget * target = [[AP_CocoaDialog_InsertXMLIDTarget alloc] init];

	NSTextField * label = [NSTextField labelWithString:LocalizedLabel(pSS, AP_STRING_ID_DLG_InsertXMLID_Msg)];
	NSTextField * field = [NSTextField textFieldWithString:@""];
	[field.widthAnchor constraintGreaterThanOrEqualToConstant:280].active = YES;

	auto button = [&](XAP_String_Id sid, NSInteger tag) {
		NSButton * b = [NSButton buttonWithTitle:LocalizedLabel(pSS, sid)
		                                  target:target action:@selector(buttonAction:)];
		b.tag = tag;
		return b;
	};
	NSButton * cancelBtn = button(XAP_STRING_ID_DLG_Cancel, kCancel);
	cancelBtn.keyEquivalent = @"\033";
	NSButton * deleteBtn = button(XAP_STRING_ID_DLG_Delete, kDelete);
	NSButton * insertBtn = button(AP_STRING_ID_DLG_InsertButton, kInsert);
	insertBtn.keyEquivalent = @"\r";
	NSStackView * buttons = [NSStackView stackViewWithViews:@[cancelBtn, deleteBtn, insertBtn]];

	NSStackView * content = [NSStackView stackViewWithViews:@[label, field, buttons]];
	content.orientation = NSUserInterfaceLayoutOrientationVertical;
	content.alignment = NSLayoutAttributeLeading;
	content.spacing = 12;
	content.translatesAutoresizingMaskIntoConstraints = NO;
	[buttons.trailingAnchor constraintEqualToAnchor:content.trailingAnchor].active = YES;
	[field.trailingAnchor constraintEqualToAnchor:content.trailingAnchor].active = YES;

	NSView * container = [[NSView alloc] initWithFrame:NSZeroRect];
	[container addSubview:content];
	[NSLayoutConstraint activateConstraints:@[
		[content.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:20],
		[content.trailingAnchor constraintEqualToAnchor:container.trailingAnchor constant:-20],
		[content.topAnchor constraintEqualToAnchor:container.topAnchor constant:20],
		[content.bottomAnchor constraintEqualToAnchor:container.bottomAnchor constant:-20],
	]];

	NSPanel * panel = [[NSPanel alloc] initWithContentRect:NSZeroRect
	                                             styleMask:NSWindowStyleMaskTitled
	                                               backing:NSBackingStoreBuffered
	                                                 defer:YES];
	panel.releasedWhenClosed = NO;
	panel.title = LocalizedLabel(pSS, AP_STRING_ID_DLG_InsertXMLID_Title);
	panel.contentView = container;
	panel.initialFirstResponder = field;
	[panel center];

	NSModalResponse response = [NSApp runModalForWindow:panel];
	std::string name = field.stringValue.UTF8String;
	[panel close];

	switch (response) {
	case kInsert:
		// Like the Gtk dialog, inserting an empty name is a cancel.
		if (!name.empty()) {
			setString(name);
			setAnswer(AP_Dialog_InsertXMLID::a_OK);
		} else {
			setAnswer(AP_Dialog_InsertXMLID::a_CANCEL);
		}
		break;
	case kDelete:
		setString(name);
		setAnswer(AP_Dialog_InsertXMLID::a_DELETE);
		break;
	default:
		setAnswer(AP_Dialog_InsertXMLID::a_CANCEL);
		break;
	}
}
