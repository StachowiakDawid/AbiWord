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

#include <array>
#include <string>
#include <vector>

#import <Cocoa/Cocoa.h>

#include "ap_Dialog_RDFEditor.h"

@class AP_CocoaDialog_RDFEditorController;

/*****************************************************************/

/* RDF > Edit RDF Triples: list, add, copy, edit and delete triples. */
class AP_CocoaDialog_RDFEditor : public AP_Dialog_RDFEditor
{
public:
	typedef std::array<std::string, 3> Row; // subject, predicate, object (prefixed)

	AP_CocoaDialog_RDFEditor(XAP_DialogFactory * pDlgFactory, XAP_Dialog_Id dlgid);
	virtual ~AP_CocoaDialog_RDFEditor(void);

	static XAP_Dialog * static_constructor(XAP_DialogFactory *, XAP_Dialog_Id dlgid);

	virtual void runModeless(XAP_Frame * pFrame) override;
	virtual void notifyActiveFrame(XAP_Frame * pFrame) override;
	virtual void activate(void) override;
	virtual void destroy(void) override;

	virtual void clear() override;
	virtual void addStatement(const PD_RDFStatement & st) override;
	virtual void setStatus(const std::string & msg) override;
	virtual void removeStatement(const PD_RDFStatement & st) override;
	virtual std::list<PD_RDFStatement> getSelection() override;
	virtual void setSelection(const std::list<PD_RDFStatement> & l) override;
	virtual void hideRestrictionXMLID(bool v) override;

	const std::vector<Row> & rows() const { return m_rows; }

	void event_Delete();
	void event_CellEdited(size_t row, size_t column, const std::string & text);
	void event_SelectionChanged();
	void event_RestrictionChanged(const std::string & xmlid);

protected:
	using AP_Dialog_RDFEditor::setSelection; // keep the single-statement overload visible

private:
	PD_RDFStatement _rowToStatement(const Row & r);
	ssize_t _findRow(const PD_RDFStatement & st);
	void _updateTitle();

	std::vector<Row> m_rows;
	AP_CocoaDialog_RDFEditorController * m_dlg;
};
