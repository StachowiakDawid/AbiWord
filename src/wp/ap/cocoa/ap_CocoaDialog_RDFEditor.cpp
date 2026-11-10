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

#include <set>

#include "xap_App.h"
#include "xap_CocoaDialog_Utilities.h"
#include "fv_View.h"
#include "ap_Strings.h"
#include "ap_CocoaDialog_RDFEditor.h"

@interface AP_CocoaDialog_RDFEditorController : NSObject <NSWindowDelegate, NSTableViewDataSource, NSTableViewDelegate>
{
@public
	AP_CocoaDialog_RDFEditor * _xap;
	NSPanel * _window;
	NSStackView * _restrictRow;
	NSPopUpButton * _restrictPopup;
	NSTableView * _table;
	NSTextField * _status;
}
- (instancetype)initWithOwner:(AP_CocoaDialog_RDFEditor *)owner;
@end

/*****************************************************************/

XAP_Dialog * AP_CocoaDialog_RDFEditor::static_constructor(XAP_DialogFactory * pFactory,
                                                          XAP_Dialog_Id dlgid)
{
	return new AP_CocoaDialog_RDFEditor(pFactory, dlgid);
}

AP_CocoaDialog_RDFEditor::AP_CocoaDialog_RDFEditor(XAP_DialogFactory * pDlgFactory,
                                                   XAP_Dialog_Id dlgid)
	: AP_Dialog_RDFEditor(pDlgFactory, dlgid),
	  m_dlg(nil)
{
}

AP_CocoaDialog_RDFEditor::~AP_CocoaDialog_RDFEditor(void)
{
}

void AP_CocoaDialog_RDFEditor::_updateTitle()
{
	ConstructWindowName();
	m_dlg->_window.title = [NSString stringWithUTF8String:m_WindowName.c_str()];
}

void AP_CocoaDialog_RDFEditor::runModeless(XAP_Frame * /*pFrame*/)
{
	m_dlg = [[AP_CocoaDialog_RDFEditorController alloc] initWithOwner:this];

	// Same restriction setup as the Gtk dialog: limit the triples to the
	// RDF links at the insertion point, with a chooser if there are several.
	m_dlg->_restrictRow.hidden = YES;
	if (m_hideRestrictionXMLID) {
		setRestrictedXMLID("");
	} else if (PD_DocumentRDFHandle rdf = getRDF()) {
		std::set<std::string> xmlids;
		rdf->addRelevantIDsForPosition(xmlids, getView()->getPoint());
		std::string combined;
		for (const std::string & id : xmlids) {
			combined += (combined.empty() ? "" : ",") + id;
		}
		setRestrictedXMLID(combined);
		if (xmlids.size() > 1) {
			[m_dlg->_restrictPopup addItemWithTitle:[NSString stringWithUTF8String:combined.c_str()]];
			for (const std::string & id : xmlids) {
				[m_dlg->_restrictPopup addItemWithTitle:[NSString stringWithUTF8String:id.c_str()]];
			}
			m_dlg->_restrictRow.hidden = NO;
		}
	}

	_updateTitle();
	showAllRDF();
	[m_dlg->_window center];
	[m_dlg->_window makeKeyAndOrderFront:nil];
}

void AP_CocoaDialog_RDFEditor::notifyActiveFrame(XAP_Frame * /*pFrame*/)
{
	_updateTitle();
}

void AP_CocoaDialog_RDFEditor::activate(void)
{
	_updateTitle();
	[m_dlg->_window makeKeyAndOrderFront:nil];
}

void AP_CocoaDialog_RDFEditor::destroy(void)
{
	modeless_cleanup();
	if (AP_CocoaDialog_RDFEditorController * dlg = m_dlg) {
		m_dlg = nil;
		bool bWindowClosing = (dlg->_xap == nullptr); // from windowWillClose:
		dlg->_xap = nullptr;
		dlg->_table.dataSource = nil;
		dlg->_table.delegate = nil;
		if (!bWindowClosing) {
			[dlg->_window close];
		}
		// We may be inside the controller's windowWillClose: release it later.
		dispatch_async(dispatch_get_main_queue(), ^{ (void)dlg; });
	}
}

void AP_CocoaDialog_RDFEditor::clear()
{
	AP_Dialog_RDFEditor::clear();
	m_rows.clear();
	[m_dlg->_table reloadData];
}

void AP_CocoaDialog_RDFEditor::addStatement(const PD_RDFStatement & stc)
{
	AP_Dialog_RDFEditor::addStatement(stc);
	PD_RDFStatement st = stc.uriToPrefixed(getModel());
	m_rows.push_back({ st.getSubject().toString(),
	                   st.getPredicate().toString(),
	                   st.getObject().toString() });
	[m_dlg->_table reloadData];
}

PD_RDFStatement AP_CocoaDialog_RDFEditor::_rowToStatement(const Row & r)
{
	return PD_RDFStatement(getModel(), PD_URI(r[0]), PD_URI(r[1]), PD_Object(r[2]));
}

ssize_t AP_CocoaDialog_RDFEditor::_findRow(const PD_RDFStatement & st)
{
	for (size_t i = 0; i < m_rows.size(); ++i) {
		if (_rowToStatement(m_rows[i]) == st) {
			return static_cast<ssize_t>(i);
		}
	}
	return -1;
}

void AP_CocoaDialog_RDFEditor::removeStatement(const PD_RDFStatement & st)
{
	ssize_t i = _findRow(st);
	if (i >= 0) {
		m_rows.erase(m_rows.begin() + i);
		[m_dlg->_table reloadData];
	}
}

std::list<PD_RDFStatement> AP_CocoaDialog_RDFEditor::getSelection()
{
	std::list<PD_RDFStatement> ret;
	NSIndexSet * sel = m_dlg->_table.selectedRowIndexes;
	for (NSUInteger i = sel.firstIndex; i != NSNotFound; i = [sel indexGreaterThanIndex:i]) {
		if (i < m_rows.size()) {
			ret.push_back(_rowToStatement(m_rows[i]));
		}
	}
	return ret;
}

void AP_CocoaDialog_RDFEditor::setSelection(const std::list<PD_RDFStatement> & l)
{
	NSMutableIndexSet * rows = [NSMutableIndexSet indexSet];
	for (const PD_RDFStatement & st : l) {
		ssize_t i = _findRow(st);
		if (i >= 0) {
			[rows addIndex:static_cast<NSUInteger>(i)];
		}
	}
	[m_dlg->_table selectRowIndexes:rows byExtendingSelection:NO];
	if (rows.count) {
		[m_dlg->_table scrollRowToVisible:static_cast<NSInteger>(rows.firstIndex)];
	}
}

void AP_CocoaDialog_RDFEditor::setStatus(const std::string & msg)
{
	m_dlg->_status.stringValue = [NSString stringWithUTF8String:msg.c_str()];
}

void AP_CocoaDialog_RDFEditor::hideRestrictionXMLID(bool v)
{
	AP_Dialog_RDFEditor::hideRestrictionXMLID(v);
	if (!m_dlg) {
		return;
	}
	if (v) {
		m_dlg->_restrictRow.hidden = YES;
	} else {
		PD_RDFModelHandle model;
		setRestrictedModel(model);
	}
}

void AP_CocoaDialog_RDFEditor::event_Delete()
{
	std::list<PD_RDFStatement> l = getSelection();
	if (l.empty()) {
		return;
	}
	// Like the Gtk dialog: after deleting one triple, select the next one.
	ssize_t next = (l.size() == 1) ? _findRow(l.front()) : -1;
	PD_DocumentRDFMutationHandle m = getModel()->createMutation();
	for (const PD_RDFStatement & st : l) {
		m->remove(st);
		removeStatement(st);
		m_count--;
	}
	m->commit();
	if (next >= 0 && static_cast<size_t>(next) < m_rows.size()) {
		setSelection(_rowToStatement(m_rows[next]));
	}
	statusIsTripleCount();
}

void AP_CocoaDialog_RDFEditor::event_CellEdited(size_t row, size_t column, const std::string & text)
{
	if (row >= m_rows.size() || column > 2) {
		return;
	}
	PD_URI n(text);
	n = n.prefixedToURI(getModel());
	PD_RDFStatement oldst = _rowToStatement(m_rows[row]);
	PD_RDFStatement newst;
	switch (column) {
	case 0:
		newst = PD_RDFStatement(n, oldst.getPredicate(), oldst.getObject());
		break;
	case 1:
		newst = PD_RDFStatement(oldst.getSubject(), n, oldst.getObject());
		break;
	default:
		newst = PD_RDFStatement(oldst.getSubject(), oldst.getPredicate(), PD_Object(n.toString()));
		break;
	}
	PD_DocumentRDFMutationHandle m = getModel()->createMutation();
	if (m->add(newst)) {
		m->remove(oldst);
		m->commit();
		m_rows[row][column] = text;
	}
	[m_dlg->_table reloadData];
}

void AP_CocoaDialog_RDFEditor::event_SelectionChanged()
{
	// Select the document text that the chosen triples are linked to.
	PD_URI pkg_idref("http://docs.oasis-open.org/opendocument/meta/package/common#idref");
	PD_DocumentRDFHandle rdf = getRDF();
	PD_RDFModelHandle model = getModel();
	for (const PD_RDFStatement & st : getSelection()) {
		PD_ObjectList ul = model->getObjects(st.getSubject(), pkg_idref);
		for (const PD_Object & obj : ul) {
			std::pair<PT_DocPosition, PT_DocPosition> range = rdf->getIDRange(obj.toString());
			getView()->cmdSelect(range);
		}
	}
}

void AP_CocoaDialog_RDFEditor::event_RestrictionChanged(const std::string & xmlid)
{
	setRestrictedXMLID(xmlid);
}

/*****************************************************************/

@implementation AP_CocoaDialog_RDFEditorController

- (instancetype)initWithOwner:(AP_CocoaDialog_RDFEditor *)owner
{
	if (!(self = [super init])) {
		return nil;
	}
	_xap = owner;
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();

	_restrictPopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
	_restrictPopup.target = self;
	_restrictPopup.action = @selector(restrictAction:);
	_restrictRow = [NSStackView stackViewWithViews:@[
		[NSTextField labelWithString:LocalizedLabel(pSS, AP_STRING_ID_DLG_RDF_Editor_Restrict)],
		_restrictPopup]];

	_table = [[NSTableView alloc] initWithFrame:NSZeroRect];
	_table.dataSource = self;
	_table.delegate = self;
	_table.allowsMultipleSelection = YES;
	_table.usesAlternatingRowBackgroundColors = YES;
	const XAP_String_Id titles[] = { AP_STRING_ID_DLG_RDF_Query_Column_Subject,
	                                 AP_STRING_ID_DLG_RDF_Query_Column_Predicate,
	                                 AP_STRING_ID_DLG_RDF_Query_Column_Object };
	for (int i = 0; i < 3; ++i) {
		NSTableColumn * col = [[NSTableColumn alloc] initWithIdentifier:[NSString stringWithFormat:@"%d", i]];
		col.title = LocalizedLabel(pSS, titles[i]);
		col.width = 200;
		col.resizingMask = NSTableColumnUserResizingMask;
		col.editable = YES;
		[_table addTableColumn:col];
	}
	NSScrollView * scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
	scroll.documentView = _table;
	scroll.hasVerticalScroller = YES;
	scroll.hasHorizontalScroller = YES;
	scroll.borderType = NSBezelBorder;
	[scroll.heightAnchor constraintGreaterThanOrEqualToConstant:240].active = YES;

	// The Gtk dialog has these in a Triple menu, with the same untranslated
	// New and Copy labels; its Open/Save (RDF/XML) need Redland.
	NSButton * newBtn = [NSButton buttonWithTitle:@"New" target:self action:@selector(newAction:)];
	NSButton * copyBtn = [NSButton buttonWithTitle:@"Copy" target:self action:@selector(copyAction:)];
	NSButton * delBtn = [NSButton buttonWithTitle:LocalizedLabel(pSS, XAP_STRING_ID_DLG_Delete)
	                                       target:self action:@selector(deleteAction:)];
	NSButton * update = [NSButton buttonWithTitle:LocalizedLabel(pSS, AP_STRING_ID_DLG_RDF_Editor_ShowAll)
	                                       target:self action:@selector(updateAction:)];
	_status = [NSTextField labelWithString:@""];
	NSStackView * bottom = [NSStackView stackViewWithViews:@[newBtn, copyBtn, delBtn, _status, update]];
	[bottom setCustomSpacing:16 afterView:delBtn];
	[_status setContentHuggingPriority:NSLayoutPriorityDefaultLow
	                    forOrientation:NSLayoutConstraintOrientationHorizontal];

	NSStackView * content = [NSStackView stackViewWithViews:@[_restrictRow, scroll, bottom]];
	content.orientation = NSUserInterfaceLayoutOrientationVertical;
	content.alignment = NSLayoutAttributeLeading;
	content.spacing = 10;
	content.edgeInsets = NSEdgeInsetsMake(16, 16, 16, 16);
	for (NSView * v in @[scroll, bottom]) {
		[v.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:16].active = YES;
		[v.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-16].active = YES;
	}

	_window = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 660, 400)
	                                     styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
	                                                | NSWindowStyleMaskResizable)
	                                       backing:NSBackingStoreBuffered
	                                         defer:YES];
	_window.releasedWhenClosed = NO;
	_window.hidesOnDeactivate = YES;
	_window.contentView = content;
	_window.delegate = self;
	return self;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
	UT_UNUSED(tableView);
	return _xap ? static_cast<NSInteger>(_xap->rows().size()) : 0;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
	UT_UNUSED(tableView);
	const auto & r = _xap->rows()[static_cast<size_t>(row)];
	return [NSString stringWithUTF8String:r[static_cast<size_t>(column.identifier.integerValue)].c_str()];
}

- (void)tableView:(NSTableView *)tableView setObjectValue:(id)value forTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
	UT_UNUSED(tableView);
	_xap->event_CellEdited(static_cast<size_t>(row), static_cast<size_t>(column.identifier.integerValue),
	                       [value description].UTF8String);
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
	UT_UNUSED(notification);
	if (_xap) {
		_xap->event_SelectionChanged();
	}
}

- (void)newAction:(id)sender    { UT_UNUSED(sender); _xap->createStatement(); }
- (void)copyAction:(id)sender   { UT_UNUSED(sender); _xap->copyStatement(); }
- (void)deleteAction:(id)sender { UT_UNUSED(sender); _xap->event_Delete(); }
- (void)updateAction:(id)sender { UT_UNUSED(sender); _xap->showAllRDF(); }

- (void)restrictAction:(NSPopUpButton *)sender
{
	_xap->event_RestrictionChanged(sender.titleOfSelectedItem.UTF8String);
}

- (void)windowWillClose:(NSNotification *)notification
{
	UT_UNUSED(notification);
	if (AP_CocoaDialog_RDFEditor * xap = _xap) {
		_xap = nullptr;
		xap->destroy();
	}
}

@end
