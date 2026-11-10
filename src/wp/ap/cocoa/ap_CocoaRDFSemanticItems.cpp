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

/*
 * Cocoa counterpart of ap_RDFSemanticItemFactoryGTK.cpp and
 * ap_RDFSemanticItemGTKInjected.cpp: the semantic item (contact, event,
 * location) factory and the RDF dialogs that are not dialog-factory
 * dialogs (RDF > Semantic Item > Create Reference, RDF > Stylesheet >
 * Settings). Without the factory the document has no semantic items.
 * The Gtk editors for single items are not ported yet: createEditor()
 * returns nothing, as PD_RDFSemanticItem::showEditorWindow() does.
 */

#import <Cocoa/Cocoa.h>

#include <string>
#include <vector>

#include "xap_App.h"
#include "xap_CocoaDialog_Utilities.h"
#include "pd_Document.h"
#include "pd_DocumentRDF.h"
#include "fv_View.h"
#include "ap_Strings.h"
#include "ap_RDFContact.h"
#include "ap_RDFEvent.h"
#include "ap_RDFLocation.h"

namespace {

/*****************************************************************/
/* semantic items                                                */
/*****************************************************************/

class AP_RDFContactCocoa : public AP_RDFContact
{
public:
	AP_RDFContactCocoa(PD_DocumentRDFHandle rdf, PD_ResultBindings_t::iterator & it)
		: AP_RDFContact(rdf, it) {}
	virtual void * createEditor() override { return nullptr; }
	virtual void updateFromEditorData(PD_DocumentRDFMutationHandle) override {}
};

class AP_RDFEventCocoa : public AP_RDFEvent
{
public:
	AP_RDFEventCocoa(PD_DocumentRDFHandle rdf, PD_ResultBindings_t::iterator & it)
		: AP_RDFEvent(rdf, it) {}
	virtual void * createEditor() override { return nullptr; }
	virtual void updateFromEditorData(PD_DocumentRDFMutationHandle) override {}
};

class AP_RDFLocationCocoa : public AP_RDFLocation
{
public:
	AP_RDFLocationCocoa(PD_DocumentRDFHandle rdf, PD_ResultBindings_t::iterator & it, bool isGeo84)
		: AP_RDFLocation(rdf, it, isGeo84) {}
	virtual void * createEditor() override { return nullptr; }
	virtual void updateFromEditorData(PD_DocumentRDFMutationHandle) override {}
};

class AP_SemanticItemFactoryCocoa : public PD_SemanticItemFactory
{
public:
	AP_SemanticItemFactoryCocoa() { PD_DocumentRDF::setSemanticItemFactory(this); }

	virtual PD_RDFContact * createContact(PD_DocumentRDFHandle rdf, PD_ResultBindings_t::iterator it) override
	{
		return new AP_RDFContactCocoa(rdf, it);
	}
	virtual PD_RDFEvent * createEvent(PD_DocumentRDFHandle rdf, PD_ResultBindings_t::iterator it) override
	{
		return new AP_RDFEventCocoa(rdf, it);
	}
	virtual PD_RDFLocation * createLocation(PD_DocumentRDFHandle rdf, PD_ResultBindings_t::iterator it,
	                                        bool isGeo84) override
	{
		return new AP_RDFLocationCocoa(rdf, it, isGeo84);
	}
};

/*****************************************************************/
/* stylesheets                                                   */
/*****************************************************************/

struct Stylesheet
{
	XAP_String_Id label;
	const char * name;
};

struct StylesheetClass
{
	const char * itemClass;
	XAP_String_Id label;
	std::vector<Stylesheet> stylesheets;
	NSInteger index; // remembered between dialogs, as in the Gtk version
};

StylesheetClass s_classes[] = {
	{ "Contact", AP_STRING_ID_DLG_RDF_SemanticStylesheets_Contacts, {
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_CONTACT_NAME, RDF_SEMANTIC_STYLESHEET_CONTACT_NAME },
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_CONTACT_NICK, RDF_SEMANTIC_STYLESHEET_CONTACT_NICK },
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_CONTACT_NAME_PHONE, RDF_SEMANTIC_STYLESHEET_CONTACT_NAME_PHONE },
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_CONTACT_NICK_PHONE, RDF_SEMANTIC_STYLESHEET_CONTACT_NICK_PHONE },
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_CONTACT_NAME_HOMEPAGE_PHONE, RDF_SEMANTIC_STYLESHEET_CONTACT_NAME_HOMEPAGE_PHONE },
	}, 0 },
	{ "Event", AP_STRING_ID_DLG_RDF_SemanticStylesheets_Events, {
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_EVENT_NAME, RDF_SEMANTIC_STYLESHEET_EVENT_NAME },
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_EVENT_SUMMARY, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY },
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_EVENT_SUMMARY_LOCATION, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY_LOCATION },
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_EVENT_SUMMARY_LOCATION_TIMES, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY_LOCATION_TIMES },
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_EVENT_SUMMARY_TIMES, RDF_SEMANTIC_STYLESHEET_EVENT_SUMMARY_TIMES },
	}, 0 },
	{ "Location", AP_STRING_ID_DLG_RDF_SemanticStylesheets_Locations, {
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_LOCATION_NAME, RDF_SEMANTIC_STYLESHEET_LOCATION_NAME },
		{ AP_STRING_ID_MENU_LABEL_RDF_SEMITEM_STYLESHEET_LOCATION_NAME_LATLONG, RDF_SEMANTIC_STYLESHEET_LOCATION_NAME_LATLONG },
	}, 0 },
};

/* Same as ApplySemanticStylesheets() in the Gtk front end. */
void applySemanticStylesheets(const std::string & semItemClass, const std::string & ssName, bool reflow)
{
	std::list<AD_Document *> dl = XAP_App::getApp()->getDocuments();
	for (AD_Document * ad : dl) {
		PD_Document * pDoc = dynamic_cast<PD_Document *>(ad);
		if (!pDoc) {
			continue;
		}
		pDoc->beginUserAtomicGlob();
		PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();
		for (PD_RDFSemanticItemHandle si : rdf->getAllSemanticObjects(semItemClass)) {
			PD_RDFSemanticStylesheetHandle ss = si->findStylesheetByName(
				PD_RDFSemanticStylesheet::stylesheetTypeSystem(), ssName);
			for (const std::string & xmlid : si->getXMLIDs()) {
				PD_RDFSemanticItemViewSite vs(si, xmlid);
				vs.setStylesheetWithoutReflow(ss);
			}
		}
		pDoc->endUserAtomicGlob();
	}

	if (!reflow) {
		return;
	}
	for (AD_Document * ad : dl) {
		PD_Document * pDoc = dynamic_cast<PD_Document *>(ad);
		if (!pDoc) {
			continue;
		}
		std::list<AV_View *> vl = pDoc->getAllViews();
		FV_View * pView = vl.empty() ? nullptr : dynamic_cast<FV_View *>(vl.front());
		if (!pView) {
			continue;
		}
		pDoc->beginUserAtomicGlob();
		pDoc->notifyPieceTableChangeStart();
		pDoc->setDontImmediatelyLayout(true);
		PD_DocumentRDFHandle rdf = pDoc->getDocumentRDF();
		for (PD_RDFSemanticItemHandle si : rdf->getAllSemanticObjects(semItemClass)) {
			for (const std::string & xmlid : si->getXMLIDs()) {
				PD_RDFSemanticItemViewSite vs(si, xmlid);
				vs.reflowUsingCurrentStylesheet(pView);
			}
		}
		pDoc->setDontImmediatelyLayout(false);
		pDoc->notifyPieceTableChangeEnd();
		pDoc->endUserAtomicGlob();
	}
}

std::string selectedStylesheet(const StylesheetClass & c, NSPopUpButton * popup)
{
	NSInteger i = popup.indexOfSelectedItem;
	if (i < 0 || static_cast<size_t>(i) >= c.stylesheets.size()) {
		i = 0;
	}
	return c.stylesheets[static_cast<size_t>(i)].name;
}

NSPanel * makePanel(NSView * content, XAP_String_Id title)
{
	content.translatesAutoresizingMaskIntoConstraints = NO;
	NSView * container = [[NSView alloc] initWithFrame:NSZeroRect];
	[container addSubview:content];
	[NSLayoutConstraint activateConstraints:@[
		[content.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:20],
		[content.trailingAnchor constraintEqualToAnchor:container.trailingAnchor constant:-20],
		[content.topAnchor constraintEqualToAnchor:container.topAnchor constant:20],
		[content.bottomAnchor constraintEqualToAnchor:container.bottomAnchor constant:-20],
	]];
	NSPanel * panel = [[NSPanel alloc] initWithContentRect:NSZeroRect
	                                             styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
	                                               backing:NSBackingStoreBuffered
	                                                 defer:YES];
	panel.releasedWhenClosed = NO;
	panel.contentView = container;
	panel.title = LocalizedLabel(XAP_App::getApp()->getStringSet(), title);
	[panel center];
	return panel;
}

}

/*****************************************************************/
/* controllers                                                   */
/*****************************************************************/

@interface AP_CocoaSemanticStylesheetsController : NSObject <NSWindowDelegate>
{
@public
	NSPanel * _window;
	NSMutableArray<NSPopUpButton *> * _popups;
}
@end

@implementation AP_CocoaSemanticStylesheetsController

- (instancetype)init
{
	if (!(self = [super init])) {
		return nil;
	}
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	_popups = [NSMutableArray array];

	NSTextField * explanation = [NSTextField wrappingLabelWithString:
		LocalizedLabel(pSS, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Explanation)];
	[explanation.widthAnchor constraintEqualToConstant:460].active = YES;

	NSMutableArray * rows = [NSMutableArray array];
	NSInteger tag = 0;
	for (const StylesheetClass & c : s_classes) {
		NSPopUpButton * popup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
		for (const Stylesheet & ss : c.stylesheets) {
			[popup addItemWithTitle:LocalizedLabel(pSS, ss.label)];
		}
		[popup selectItemAtIndex:c.index];
		[_popups addObject:popup];
		NSButton * set = [NSButton buttonWithTitle:LocalizedLabel(pSS, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Set)
		                                    target:self action:@selector(setAction:)];
		set.tag = tag++;
		[rows addObject:@[[NSTextField labelWithString:LocalizedLabel(pSS, c.label)], popup, set]];
	}
	NSGridView * grid = [NSGridView gridViewWithViews:rows];
	[grid columnAtIndex:0].xPlacement = NSGridCellPlacementTrailing;
	grid.rowAlignment = NSGridRowAlignmentFirstBaseline;

	// "set all" and OK, as in the Gtk dialog
	NSButton * setAll = [NSButton buttonWithTitle:LocalizedLabel(pSS, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Set)
	                                       target:self action:@selector(setAllAction:)];
	NSButton * ok = [NSButton buttonWithTitle:LocalizedLabel(pSS, XAP_STRING_ID_DLG_OK)
	                                   target:self action:@selector(okAction:)];
	ok.keyEquivalent = @"\r";
	NSStackView * buttons = [NSStackView stackViewWithViews:@[setAll, ok]];

	NSStackView * content = [NSStackView stackViewWithViews:@[explanation, grid, buttons]];
	content.orientation = NSUserInterfaceLayoutOrientationVertical;
	content.alignment = NSLayoutAttributeLeading;
	content.spacing = 16;
	[buttons.trailingAnchor constraintEqualToAnchor:content.trailingAnchor].active = YES;

	_window = makePanel(content, AP_STRING_ID_DLG_RDF_SemanticStylesheets_Title);
	_window.delegate = self;
	return self;
}

- (void)setAction:(NSButton *)sender
{
	const StylesheetClass & c = s_classes[sender.tag];
	applySemanticStylesheets(c.itemClass, selectedStylesheet(c, _popups[static_cast<NSUInteger>(sender.tag)]), true);
}

- (void)setAllAction:(id)sender
{
	UT_UNUSED(sender);
	NSUInteger i = 0;
	for (const StylesheetClass & c : s_classes) {
		applySemanticStylesheets(c.itemClass, selectedStylesheet(c, _popups[i++]), true);
	}
}

- (void)okAction:(id)sender
{
	UT_UNUSED(sender);
	// Remember the choices and make them current, without reflowing.
	NSUInteger i = 0;
	for (StylesheetClass & c : s_classes) {
		NSPopUpButton * popup = _popups[i++];
		c.index = popup.indexOfSelectedItem;
		applySemanticStylesheets(c.itemClass, selectedStylesheet(c, popup), false);
	}
	[NSApp stopModal];
}

- (void)windowWillClose:(NSNotification *)notification
{
	UT_UNUSED(notification);
	[NSApp stopModal];
}

@end

@interface AP_CocoaInsertReferenceController : NSObject <NSWindowDelegate, NSTableViewDataSource>
{
@public
	NSPanel * _window;
	NSTableView * _table;
	std::vector<std::string> _names;
	FV_View * _view;
}
- (instancetype)initWithView:(FV_View *)view;
@end

@implementation AP_CocoaInsertReferenceController

- (instancetype)initWithView:(FV_View *)view
{
	if (!(self = [super init])) {
		return nil;
	}
	_view = view;
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();
	for (PD_RDFContactHandle c : view->getDocument()->getDocumentRDF()->getContacts()) {
		_names.push_back(c->name());
	}

	_table = [[NSTableView alloc] initWithFrame:NSZeroRect];
	NSTableColumn * col = [[NSTableColumn alloc] initWithIdentifier:@"name"];
	col.title = LocalizedLabel(pSS, AP_STRING_ID_DLG_RDF_SemanticItemInsert_Column_Refdlg);
	col.width = 320;
	[_table addTableColumn:col];
	_table.dataSource = self;
	_table.target = self;
	_table.doubleAction = @selector(insertAction:);
	NSScrollView * scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
	scroll.documentView = _table;
	scroll.hasVerticalScroller = YES;
	scroll.borderType = NSBezelBorder;
	[scroll.widthAnchor constraintEqualToConstant:340].active = YES;
	[scroll.heightAnchor constraintEqualToConstant:220].active = YES;

	NSButton * cancel = [NSButton buttonWithTitle:LocalizedLabel(pSS, XAP_STRING_ID_DLG_Cancel)
	                                       target:self action:@selector(cancelAction:)];
	cancel.keyEquivalent = @"\033";
	NSButton * insert = [NSButton buttonWithTitle:LocalizedLabel(pSS, AP_STRING_ID_DLG_RDF_SemanticItemInsert_Ok)
	                                       target:self action:@selector(insertAction:)];
	insert.keyEquivalent = @"\r";
	NSStackView * buttons = [NSStackView stackViewWithViews:@[cancel, insert]];

	NSStackView * content = [NSStackView stackViewWithViews:@[scroll, buttons]];
	content.orientation = NSUserInterfaceLayoutOrientationVertical;
	content.alignment = NSLayoutAttributeTrailing;
	content.spacing = 12;

	_window = makePanel(content, AP_STRING_ID_DLG_RDF_SemanticItemInsert_Title);
	_window.delegate = self;
	if (!_names.empty()) {
		[_table selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
	}
	return self;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
	UT_UNUSED(tableView);
	return static_cast<NSInteger>(_names.size());
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)column row:(NSInteger)row
{
	UT_UNUSED(tableView);
	UT_UNUSED(column);
	return [NSString stringWithUTF8String:_names[static_cast<size_t>(row)].c_str()];
}

- (void)insertAction:(id)sender
{
	UT_UNUSED(sender);
	NSInteger row = _table.selectedRow;
	if (row < 0) {
		return;
	}
	// Same as OnInsertReferenceBase() in the Gtk front end.
	const std::string & name = _names[static_cast<size_t>(row)];
	for (PD_RDFContactHandle c : _view->getDocument()->getDocumentRDF()->getContacts()) {
		if (c->name() == name) {
			c->insert(_view);
			break;
		}
	}
	[NSApp stopModal];
}

- (void)cancelAction:(id)sender
{
	UT_UNUSED(sender);
	[NSApp stopModal];
}

- (void)windowWillClose:(NSNotification *)notification
{
	UT_UNUSED(notification);
	[NSApp stopModal];
}

@end

/*****************************************************************/
/* the dialogs entry points                                      */
/*****************************************************************/

namespace {

class PD_RDFDialogsCocoa : public PD_RDFDialogs
{
public:
	PD_RDFDialogsCocoa() { PD_DocumentRDF::setRDFDialogs(this); }

	virtual void runSemanticStylesheetsDialog(FV_View * /*pView*/) override
	{
		AP_CocoaSemanticStylesheetsController * dlg = [[AP_CocoaSemanticStylesheetsController alloc] init];
		[NSApp runModalForWindow:dlg->_window];
		dlg->_window.delegate = nil;
		[dlg->_window close];
	}

	virtual std::pair<PT_DocPosition, PT_DocPosition> runInsertReferenceDialog(FV_View * pView) override
	{
		AP_CocoaInsertReferenceController * dlg = [[AP_CocoaInsertReferenceController alloc] initWithView:pView];
		[NSApp runModalForWindow:dlg->_window];
		dlg->_window.delegate = nil;
		[dlg->_window close];
		return std::pair<PT_DocPosition, PT_DocPosition>(); // as in the Gtk version
	}
};

AP_SemanticItemFactoryCocoa s_semanticItemFactory;
PD_RDFDialogsCocoa s_rdfDialogs;

}
