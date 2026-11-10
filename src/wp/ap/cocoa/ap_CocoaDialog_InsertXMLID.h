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

#include "ap_Dialog_InsertXMLID.h"

/*****************************************************************/

/* Insert > RDF Link: name the xml:id to insert (or delete). */
class AP_CocoaDialog_InsertXMLID : public AP_Dialog_InsertXMLID
{
public:
	AP_CocoaDialog_InsertXMLID(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id dlgid);
	virtual ~AP_CocoaDialog_InsertXMLID(void);

	virtual void runModal(XAP_Frame * pFrame) override;

	static XAP_Dialog * static_constructor(XAP_DialogFactory *, XAP_Dialog_Id dlgid);
};
