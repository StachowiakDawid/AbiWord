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

#include "ie_exp.h"

/* PDF export through the Cocoa printing code, the counterpart of the
   Cairo-based exporter of the Gtk build. */
class ABI_EXPORT IE_Exp_PDF_Cocoa_Sniffer : public IE_ExpSniffer
{
	friend class IE_Exp;

public:
	IE_Exp_PDF_Cocoa_Sniffer();
	virtual ~IE_Exp_PDF_Cocoa_Sniffer();

	virtual UT_Confidence_t supportsMIME(const char * szMIME) override;
	virtual bool recognizeSuffix(const char * szSuffix) override;
	virtual bool getDlgLabels(const char ** szDesc,
	                          const char ** szSuffixList,
	                          IEFileType * ft) override;
	virtual UT_Error constructExporter(PD_Document * pDocument,
	                                   IE_Exp ** ppie) override;
};
