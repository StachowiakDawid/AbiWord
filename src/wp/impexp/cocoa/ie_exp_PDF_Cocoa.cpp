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

#include <memory>

#include "ie_exp_PDF_Cocoa.h"
#include "pd_Document.h"
#include "fl_DocLayout.h"
#include "fv_View.h"
#include "xap_App.h"
#include "gr_CocoaGraphics.h"
#include "xap_PrintingNSView.h"
#include "ap_PrintingDelegate.h"

namespace {

class IE_Exp_PDF_Cocoa : public IE_Exp
{
public:
	explicit IE_Exp_PDF_Cocoa(PD_Document * pDocument)
		: IE_Exp(pDocument)
	{
	}

protected:
	virtual UT_Error _writeDocument() override
	{
		const fp_PageSize & ps = getDoc()->m_docPageSize;
		// Cocoa printing works in points.
		NSSize paper = NSMakeSize(ps.Width(DIM_IN) * 72.0, ps.Height(DIM_IN) * 72.0);

		XAP_PrintingNSView * view =
			[[XAP_PrintingNSView alloc] initWithFrame:NSMakeRect(0, 0, paper.width, paper.height)];
		GR_CocoaAllocInfo ai(view);
		std::unique_ptr<GR_CocoaGraphics> graphics(
			static_cast<GR_CocoaGraphics *>(XAP_App::getApp()->newGraphics(ai)));
		if (!graphics) {
			return UT_ERROR;
		}
		graphics->setIsPrinting(true);
		graphics->setPortrait(ps.isPortrait());

		FL_DocLayout * layout = new FL_DocLayout(getDoc(), graphics.get());
		FV_View * printView = new FV_View(XAP_App::getApp(), nullptr, layout);
		layout->setView(printView);
		layout->fillLayouts();
		layout->formatAll();
		layout->recalculateTOCFields();

		[view setPrintingDelegate:new AP_PrintingDelegate(printView)];

		// Run a regular print operation, which paginates through the view's
		// knowsPageRange:, and have it save the job as a PDF file.
		NSURL * tmp = [NSURL fileURLWithPath:[NSTemporaryDirectory()
			stringByAppendingPathComponent:[[NSUUID UUID].UUIDString stringByAppendingString:@".pdf"]]];
		NSPrintInfo * info = [[NSPrintInfo alloc] initWithDictionary:@{
			NSPrintJobSavingURL : tmp,
		}];
		info.jobDisposition = NSPrintSaveJob;
		info.paperSize = paper;
		info.topMargin = info.bottomMargin = info.leftMargin = info.rightMargin = 0;

		NSPrintOperation * op = [NSPrintOperation printOperationWithView:view printInfo:info];
		op.showsPrintPanel = NO;
		op.showsProgressPanel = NO;
		BOOL ok = [op runOperation];
		NSData * pdf = ok ? [NSData dataWithContentsOfURL:tmp] : nil;
		[[NSFileManager defaultManager] removeItemAtURL:tmp error:nil];

		// The delegate refers to printView: detach it before printView goes.
		[view setPrintingDelegate:nullptr];
		// Same teardown order as the other exporters that lay out a document.
		DELETEP(layout);
		DELETEP(printView);

		if (!ok || pdf.length == 0) {
			return UT_ERROR;
		}
		write(static_cast<const char *>(pdf.bytes), static_cast<UT_uint32>(pdf.length));
		return UT_OK;
	}
};

}

IE_Exp_PDF_Cocoa_Sniffer::IE_Exp_PDF_Cocoa_Sniffer()
	: IE_ExpSniffer("application/pdf", false)
{
}

IE_Exp_PDF_Cocoa_Sniffer::~IE_Exp_PDF_Cocoa_Sniffer()
{
}

UT_Confidence_t IE_Exp_PDF_Cocoa_Sniffer::supportsMIME(const char * szMIME)
{
	return g_ascii_strcasecmp(szMIME, "application/pdf") ? UT_CONFIDENCE_ZILCH
	                                                     : UT_CONFIDENCE_PERFECT;
}

bool IE_Exp_PDF_Cocoa_Sniffer::recognizeSuffix(const char * szSuffix)
{
	return !g_ascii_strcasecmp(szSuffix, ".pdf");
}

bool IE_Exp_PDF_Cocoa_Sniffer::getDlgLabels(const char ** szDesc,
                                            const char ** szSuffixList,
                                            IEFileType * ft)
{
	*szDesc = "Portable Document Format (.pdf)";
	*szSuffixList = "*.pdf";
	*ft = getFileType();
	return true;
}

UT_Error IE_Exp_PDF_Cocoa_Sniffer::constructExporter(PD_Document * pDocument,
                                                     IE_Exp ** ppie)
{
	*ppie = new IE_Exp_PDF_Cocoa(pDocument);
	return UT_OK;
}
