/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:t -*- */

/* AbiSource Application Framework
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2001-2016 Hubert Figuiere
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

#include "ut_assert.h"
#include "xap_CocoaDlg_MessageBox.h"
#include "xap_CocoaApp.h"
#include "xap_CocoaFrame.h"

#include "xap_CocoaDialog_Utilities.h"


/*****************************************************************/
XAP_Dialog * XAP_CocoaDialog_MessageBox::static_constructor(XAP_DialogFactory * pFactory,
														 XAP_Dialog_Id dlgid)
{
	XAP_CocoaDialog_MessageBox * p = new XAP_CocoaDialog_MessageBox(pFactory, dlgid);
	return p;
}

XAP_CocoaDialog_MessageBox::XAP_CocoaDialog_MessageBox(XAP_DialogFactory * pDlgFactory,
												   XAP_Dialog_Id dlgid)
	: XAP_Dialog_MessageBox(pDlgFactory, dlgid)
{
}

XAP_CocoaDialog_MessageBox::~XAP_CocoaDialog_MessageBox(void)
{
}

/*****************************************************************/

void XAP_CocoaDialog_MessageBox::runModal(XAP_Frame * /*pFrame*/)
{
	XAP_App * pApp = XAP_App::getApp();
	const XAP_StringSet * pSS = pApp->getStringSet();

	// The buttons in display order; the first one is the default (Return).
	struct Choice { tAnswer answer; XAP_String_Id label; };
	std::vector<Choice> choices;
	switch (m_buttons) {
	case b_O:
		choices = { { a_OK, XAP_STRING_ID_DLG_OK } };
		break;
	case b_OC:
		choices = { { a_OK, XAP_STRING_ID_DLG_OK }, { a_CANCEL, XAP_STRING_ID_DLG_Cancel } };
		break;
	case b_YN:
		choices = { { a_YES, XAP_STRING_ID_DLG_MB_Yes }, { a_NO, XAP_STRING_ID_DLG_MB_No } };
		break;
	case b_YNC:
		choices = { { a_YES, XAP_STRING_ID_DLG_MB_Yes }, { a_NO, XAP_STRING_ID_DLG_MB_No },
					{ a_CANCEL, XAP_STRING_ID_DLG_Cancel } };
		break;
	default:
		UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
		m_answer = a_OK;
		return;
	}
	// honour the requested default answer
	for (size_t i = 1; i < choices.size(); i++) {
		if (choices[i].answer == m_defaultAnswer) {
			std::swap(choices[0], choices[i]);
			break;
		}
	}

	NSAlert * alert = [[NSAlert alloc] init];
	[alert setMessageText:[NSString stringWithUTF8String:m_message.c_str()]];
	[alert setInformativeText:[NSString stringWithUTF8String:m_secondaryMessage.c_str()]];
	for (size_t i = 0; i < choices.size(); i++) {
		NSButton * button = [alert addButtonWithTitle:@""];
		LocalizeControl(button, pSS, choices[i].label);	// strips the & mnemonic
		// Esc cancels; the first button keeps Return
		if (i > 0 && choices[i].answer == a_CANCEL) {
			[button setKeyEquivalent:@"\033"];
		}
	}

	NSModalResponse response = [alert runModal];
	NSInteger index = response - NSAlertFirstButtonReturn;
	if (index >= 0 && index < (NSInteger)choices.size()) {
		m_answer = choices[index].answer;
	} else {
		UT_ASSERT(UT_SHOULD_NOT_HAPPEN);
		m_answer = choices.back().answer;
	}
}

