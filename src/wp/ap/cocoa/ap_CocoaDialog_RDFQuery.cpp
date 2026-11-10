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

#include <algorithm>

#include "xap_App.h"
#include "xap_CocoaDialog_Utilities.h"
#include "ap_Strings.h"
#include "ap_CocoaDialog_RDFQuery.h"

@interface AP_CocoaDialog_RDFQueryController : NSObject <NSWindowDelegate, NSTableViewDataSource>
{
@public
	AP_CocoaDialog_RDFQuery * _xap;
	NSPanel * _window;
	NSTextView * _query;
	NSTableView * _results;
	NSTextField * _status;
}
- (instancetype)initWithOwner:(AP_CocoaDialog_RDFQuery *)owner;
- (void)reloadColumns;
@end

/*****************************************************************/

XAP_Dialog * AP_CocoaDialog_RDFQuery::static_constructor(XAP_DialogFactory * pFactory,
                                                         XAP_Dialog_Id dlgid)
{
	return new AP_CocoaDialog_RDFQuery(pFactory, dlgid);
}

AP_CocoaDialog_RDFQuery::AP_CocoaDialog_RDFQuery(XAP_DialogFactory * pDlgFactory,
                                                 XAP_Dialog_Id dlgid)
	: AP_Dialog_RDFQuery(pDlgFactory, dlgid),
	  m_dlg(nil)
{
}

AP_CocoaDialog_RDFQuery::~AP_CocoaDialog_RDFQuery(void)
{
}

void AP_CocoaDialog_RDFQuery::_setColumns(const std::vector<std::string> & cols)
{
	m_columns = cols;
	[m_dlg reloadColumns];
}

void AP_CocoaDialog_RDFQuery::_updateTitle()
{
	ConstructWindowName();
	m_dlg->_window.title = [NSString stringWithUTF8String:m_WindowName.c_str()];
}

void AP_CocoaDialog_RDFQuery::runModeless(XAP_Frame * /*pFrame*/)
{
	m_dlg = [[AP_CocoaDialog_RDFQueryController alloc] initWithOwner:this];

	const XAP_StringSet * pSS = m_pApp->getStringSet();
	std::string s, p, o;
	pSS->getValueUTF8(AP_STRING_ID_DLG_RDF_Query_Column_Subject, s);
	pSS->getValueUTF8(AP_STRING_ID_DLG_RDF_Query_Column_Predicate, p);
	pSS->getValueUTF8(AP_STRING_ID_DLG_RDF_Query_Column_Object, o);
	_setColumns({ s, p, o });

	_updateTitle();
	[m_dlg->_window center];
	[m_dlg->_window makeKeyAndOrderFront:nil];
}

void AP_CocoaDialog_RDFQuery::notifyActiveFrame(XAP_Frame * /*pFrame*/)
{
	_updateTitle();
}

void AP_CocoaDialog_RDFQuery::activate(void)
{
	_updateTitle();
	[m_dlg->_window makeKeyAndOrderFront:nil];
}

void AP_CocoaDialog_RDFQuery::destroy(void)
{
	modeless_cleanup();
	if (AP_CocoaDialog_RDFQueryController * dlg = m_dlg) {
		m_dlg = nil;
		bool bWindowClosing = (dlg->_xap == nullptr); // from windowWillClose:
		dlg->_xap = nullptr;
		dlg->_results.dataSource = nil;
		if (!bWindowClosing) {
			[dlg->_window close];
		}
		// We may be inside the controller's windowWillClose: release it later.
		dispatch_async(dispatch_get_main_queue(), ^{ (void)dlg; });
	}
}

void AP_CocoaDialog_RDFQuery::clear()
{
	AP_Dialog_RDFQuery::clear();
	m_rows.clear();
	[m_dlg->_results reloadData];
}

void AP_CocoaDialog_RDFQuery::setupBindingsView(std::map<std::string, std::string> & b)
{
	std::vector<std::string> cols;
	for (const auto & binding : b) {
		cols.push_back(binding.first);
	}
	// Like the Gtk dialog, show the usual triple columns first.
	for (const char * hot : { "o", "p", "s", "object", "predicate", "subject" }) {
		auto it = std::find(cols.begin(), cols.end(), hot);
		if (it != cols.end()) {
			std::rotate(cols.begin(), it, it + 1);
		}
	}
	_setColumns(cols);
}

void AP_CocoaDialog_RDFQuery::addBinding(std::map<std::string, std::string> & b)
{
	AP_Dialog_RDFQuery::addBinding(b);
	std::vector<std::string> row;
	for (const std::string & col : m_columns) {
		auto it = b.find(col);
		row.push_back(it != b.end() ? uriToPrefixed(it->second) : std::string());
	}
	m_rows.push_back(row);
	[m_dlg->_results reloadData];
}

void AP_CocoaDialog_RDFQuery::setStatus(const std::string & msg)
{
	m_dlg->_status.stringValue = [NSString stringWithUTF8String:msg.c_str()];
}

void AP_CocoaDialog_RDFQuery::setQueryString(const std::string & sparql)
{
	m_dlg->_query.string = [NSString stringWithUTF8String:sparql.c_str()];
}

void AP_CocoaDialog_RDFQuery::event_Execute()
{
	executeQuery(m_dlg->_query.string.UTF8String);
}

void AP_CocoaDialog_RDFQuery::event_ShowAll()
{
	showAllRDF();
}

/*****************************************************************/

@implementation AP_CocoaDialog_RDFQueryController

- (instancetype)initWithOwner:(AP_CocoaDialog_RDFQuery *)owner
{
	if (!(self = [super init])) {
		return nil;
	}
	_xap = owner;
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();

	NSScrollView * queryScroll = [NSTextView scrollableTextView];
	_query = (NSTextView *)queryScroll.documentView;
	_query.richText = NO;
	_query.automaticQuoteSubstitutionEnabled = NO;
	_query.font = [NSFont userFixedPitchFontOfSize:0];
	_query.string = LocalizedString(pSS, AP_STRING_ID_DLG_RDF_Query_Comment);
	[queryScroll.heightAnchor constraintEqualToConstant:110].active = YES;

	NSButton * showAll = [NSButton buttonWithTitle:LocalizedLabel(pSS, AP_STRING_ID_DLG_RDF_Query_ShowAll)
	                                        target:self action:@selector(showAllAction:)];
	NSButton * execute = [NSButton buttonWithTitle:LocalizedLabel(pSS, AP_STRING_ID_DLG_RDF_Query_Execute)
	                                        target:self action:@selector(executeAction:)];
#if !WITH_REDLAND
	// Queries need Redland, as in the Gtk dialog.
	showAll.enabled = NO;
	execute.enabled = NO;
#endif
	NSStackView * buttons = [NSStackView stackViewWithViews:@[showAll, execute]];

	_results = [[NSTableView alloc] initWithFrame:NSZeroRect];
	_results.dataSource = self;
	_results.allowsMultipleSelection = YES;
	_results.usesAlternatingRowBackgroundColors = YES;
	NSScrollView * resultsScroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
	resultsScroll.documentView = _results;
	resultsScroll.hasVerticalScroller = YES;
	resultsScroll.hasHorizontalScroller = YES;
	resultsScroll.borderType = NSBezelBorder;
	[resultsScroll.heightAnchor constraintGreaterThanOrEqualToConstant:200].active = YES;

	_status = [NSTextField labelWithString:@""];

	NSStackView * content = [NSStackView stackViewWithViews:@[queryScroll, buttons, resultsScroll, _status]];
	content.orientation = NSUserInterfaceLayoutOrientationVertical;
	content.alignment = NSLayoutAttributeLeading;
	content.spacing = 10;
	content.edgeInsets = NSEdgeInsetsMake(16, 16, 16, 16);
	for (NSView * v in @[queryScroll, resultsScroll]) {
		[v.leadingAnchor constraintEqualToAnchor:content.leadingAnchor constant:16].active = YES;
		[v.trailingAnchor constraintEqualToAnchor:content.trailingAnchor constant:-16].active = YES;
	}

	_window = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 560, 460)
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

- (void)reloadColumns
{
	while (_results.tableColumns.count) {
		[_results removeTableColumn:_results.tableColumns.lastObject];
	}
	NSInteger i = 0;
	for (const std::string & name : _xap->columns()) {
		NSTableColumn * col = [[NSTableColumn alloc] initWithIdentifier:[NSString stringWithFormat:@"%ld", (long)i++]];
		col.title = [NSString stringWithUTF8String:name.c_str()];
		col.width = 170;
		col.resizingMask = NSTableColumnUserResizingMask;
		col.editable = NO;
		[_results addTableColumn:col];
	}
	[_results reloadData];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
	UT_UNUSED(tableView);
	return _xap ? static_cast<NSInteger>(_xap->rows().size()) : 0;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
	UT_UNUSED(tableView);
	size_t c = static_cast<size_t>(column.identifier.integerValue);
	const auto & r = _xap->rows()[static_cast<size_t>(row)];
	return c < r.size() ? [NSString stringWithUTF8String:r[c].c_str()] : @"";
}

- (void)showAllAction:(id)sender
{
	UT_UNUSED(sender);
	_xap->event_ShowAll();
}

- (void)executeAction:(id)sender
{
	UT_UNUSED(sender);
	_xap->event_Execute();
}

- (void)windowWillClose:(NSNotification *)notification
{
	UT_UNUSED(notification);
	if (AP_CocoaDialog_RDFQuery * xap = _xap) {
		_xap = nullptr;
		xap->destroy();
	}
}

@end
