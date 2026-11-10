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

#pragma once

#import <Cocoa/Cocoa.h>

#include "ap_Dialog_Border_Shading.h"

class GR_CocoaGraphics;
@class AP_CocoaDialog_Border_ShadingController;

/*****************************************************************/

class AP_CocoaDialog_Border_Shading : public AP_Dialog_Border_Shading
{
public:
	AP_CocoaDialog_Border_Shading(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id dlgid);
	virtual ~AP_CocoaDialog_Border_Shading(void);

	virtual void runModeless(XAP_Frame * pFrame) override;

	static XAP_Dialog * static_constructor(XAP_DialogFactory *, XAP_Dialog_Id dlgid);

	// callbacks can fire these events
	void event_Close(void);
	void event_previewInvalidate(void);
	void event_BorderThicknessChanged(NSInteger index);
	void event_BorderStyleChanged(NSInteger index);
	void event_ShadingOffsetChanged(NSInteger index);
	void event_ShadingEnableChanged(bool bEnable);

	virtual void setSensitivity(bool bSens) override;
	virtual void setBorderStyleInGUI(const std::string & sStyle) override;
	virtual void setBorderThicknessInGUI(const std::string & sThick) override;
	virtual void setBorderColorInGUI(const UT_RGBColor & clr) override;
	virtual void setShadingColorInGUI(const UT_RGBColor & clr) override;
	virtual void setShadingPatternInGUI(const std::string & sPattern) override;
	virtual void setShadingOffsetInGUI(const std::string & sOffset) override;

	virtual void destroy(void) override;
	virtual void activate(void) override;
	virtual void notifyActiveFrame(XAP_Frame * pFrame) override;

	const char * getWindowName() const { return m_WindowName; }

private:
	GR_CocoaGraphics * m_pPreviewWidget;
	AP_CocoaDialog_Border_ShadingController * m_dlg;
};
