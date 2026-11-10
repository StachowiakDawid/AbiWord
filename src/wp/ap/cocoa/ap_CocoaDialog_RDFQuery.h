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

#include <string>
#include <vector>

#import <Cocoa/Cocoa.h>

#include "ap_Dialog_RDFQuery.h"

@class AP_CocoaDialog_RDFQueryController;

/*****************************************************************/

/* RDF > SPARQL Query: run a query on the document RDF, show the bindings. */
class AP_CocoaDialog_RDFQuery : public AP_Dialog_RDFQuery
{
public:
	AP_CocoaDialog_RDFQuery(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id dlgid);
	virtual ~AP_CocoaDialog_RDFQuery(void);

	static XAP_Dialog * static_constructor(XAP_DialogFactory *, XAP_Dialog_Id dlgid);

	virtual void runModeless(XAP_Frame * pFrame) override;
	virtual void notifyActiveFrame(XAP_Frame * pFrame) override;
	virtual void activate(void) override;
	virtual void destroy(void) override;

	virtual void clear() override;
	virtual void setupBindingsView(std::map<std::string, std::string> & b) override;
	virtual void addBinding(std::map<std::string, std::string> & b) override;
	virtual void setStatus(const std::string & msg) override;
	virtual void setQueryString(const std::string & sparql) override;

	// table data, read by the controller
	const std::vector<std::string> & columns() const { return m_columns; }
	const std::vector<std::vector<std::string>> & rows() const { return m_rows; }

	void event_Execute();
	void event_ShowAll();

private:
	void _setColumns(const std::vector<std::string> & cols);
	void _updateTitle();

	std::vector<std::string> m_columns;  // binding names, in display order
	std::vector<std::vector<std::string>> m_rows;
	AP_CocoaDialog_RDFQueryController * m_dlg;
};
