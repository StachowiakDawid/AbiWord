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
 * Cocoa counterpart of ap_RDFSemanticItemFactoryGTK.cpp,
 * ap_RDFContactGTK.cpp, ap_RDFEventGTK.cpp, ap_RDFLocationGTK.cpp and
 * ap_RDFSemanticItemGTKInjected.*: the semantic item (contact, event,
 * location) factory with the item editors, and the RDF dialogs that are
 * not dialog-factory dialogs (RDF > Semantic Item > Create Reference,
 * RDF > Stylesheet > Settings). Without the factory the document has no
 * semantic items.
 */

#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <MapKit/MapKit.h>

#include <string>
#include <vector>

#include "ut_std_string.h"
#include "ut_conversion.h"
#include "xap_App.h"
#include "xap_CocoaDialog_Utilities.h"
#include "pd_Document.h"
#include "pd_DocumentRDF.h"
#include "fv_View.h"
#include "ap_Strings.h"
#include "ap_RDFContact.h"
#include "ap_RDFEvent.h"
#include "ap_RDFLocation.h"

/* Ends the modal session of an editor panel with the clicked button's tag. */
@interface AP_CocoaSemItemPanelTarget : NSObject <NSWindowDelegate>
@end

@implementation AP_CocoaSemItemPanelTarget
- (void)buttonAction:(NSButton *)sender
{
	[NSApp stopModalWithCode:sender.tag];
}
- (void)windowWillClose:(NSNotification *)notification
{
	UT_UNUSED(notification);
	[NSApp stopModalWithCode:NSModalResponseCancel];
}
@end

/* The location editor's map: moving it puts its centre in the Lat and
   Long fields, as the Gtk (champlain) map does. */
@interface AP_CocoaLocationMapDelegate : NSObject <MKMapViewDelegate>
@property (nonatomic, weak) NSTextField * latField;
@property (nonatomic, weak) NSTextField * longField;
@end

@implementation AP_CocoaLocationMapDelegate
- (void)mapView:(MKMapView *)mapView regionDidChangeAnimated:(BOOL)animated
{
	UT_UNUSED(animated);
	if (!mapView.window) {
		return; // the first region, set before the map is shown
	}
	self.latField.stringValue = [NSString stringWithUTF8String:tostr(mapView.centerCoordinate.latitude).c_str()];
	self.longField.stringValue = [NSString stringWithUTF8String:tostr(mapView.centerCoordinate.longitude).c_str()];
}
@end

namespace {

/*****************************************************************/
/* semantic item editors                                         */
/*****************************************************************/

/* Runs a modal panel around content with an OK button; true if OK was
   pressed, false if the window was closed. */
bool runEditorPanel(NSView * content, const std::string & title)
{
	AP_CocoaSemItemPanelTarget * target = [[AP_CocoaSemItemPanelTarget alloc] init];
	NSButton * ok = [NSButton buttonWithTitle:LocalizedLabel(XAP_App::getApp()->getStringSet(), XAP_STRING_ID_DLG_OK)
	                                   target:target action:@selector(buttonAction:)];
	ok.tag = NSModalResponseOK;
	ok.keyEquivalent = @"\r";

	NSStackView * stack = [NSStackView stackViewWithViews:@[content, ok]];
	stack.orientation = NSUserInterfaceLayoutOrientationVertical;
	stack.alignment = NSLayoutAttributeTrailing;
	stack.spacing = 16;
	stack.translatesAutoresizingMaskIntoConstraints = NO;
	NSView * container = [[NSView alloc] initWithFrame:NSZeroRect];
	[container addSubview:stack];
	[NSLayoutConstraint activateConstraints:@[
		[stack.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:20],
		[stack.trailingAnchor constraintEqualToAnchor:container.trailingAnchor constant:-20],
		[stack.topAnchor constraintEqualToAnchor:container.topAnchor constant:20],
		[stack.bottomAnchor constraintEqualToAnchor:container.bottomAnchor constant:-20],
	]];

	NSPanel * panel = [[NSPanel alloc] initWithContentRect:NSZeroRect
	                                             styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
	                                               backing:NSBackingStoreBuffered
	                                                 defer:YES];
	panel.releasedWhenClosed = NO;
	panel.contentView = container;
	panel.title = [NSString stringWithUTF8String:title.c_str()];
	panel.delegate = target;
	[panel center];
	NSModalResponse response = [NSApp runModalForWindow:panel];
	panel.delegate = nil;
	[panel close];
	return response == NSModalResponseOK;
}

/* The fields of one item editor, as a label | text field grid. */
class EditorFields
{
public:
	NSTextField * add(NSString * label, const std::string & value)
	{
		NSTextField * field = [NSTextField textFieldWithString:[NSString stringWithUTF8String:value.c_str()]];
		[field.widthAnchor constraintGreaterThanOrEqualToConstant:300].active = YES;
		[m_rows addObject:@[[NSTextField labelWithString:label], field]];
		return field;
	}
	NSView * view()
	{
		NSGridView * grid = [NSGridView gridViewWithViews:m_rows];
		[grid columnAtIndex:0].xPlacement = NSGridCellPlacementTrailing;
		grid.rowAlignment = NSGridRowAlignmentFirstBaseline;
		return grid;
	}
private:
	NSMutableArray * m_rows = [NSMutableArray array];
};

std::string text(NSTextField * field)
{
	return field ? std::string(field.stringValue.UTF8String) : std::string();
}

/* Same role as AP_RDFSemanticItemGTKInjected: editor windows, import
   completion and file choosers for the semantic item classes. */
template <class ParentClass>
class AP_RDFSemanticItemCocoaInjected : public ParentClass
{
public:
	using ParentClass::ParentClass;

	virtual void showEditorWindow(const PD_RDFSemanticItemHandle & c) override
	{
		NSView * editor = (__bridge NSView *)c->createEditor();
		// Like the Gtk dialog, closing the window also keeps the edits.
		runEditorPanel(editor, c->getDisplayLabel());
		c->updateFromEditorData();
	}

	virtual void showEditorWindow(const PD_RDFSemanticItems & cl) override
	{
		NSTabView * tabs = [[NSTabView alloc] initWithFrame:NSZeroRect];
		for (const PD_RDFSemanticItemHandle & c : cl) {
			NSTabViewItem * tab = [[NSTabViewItem alloc] init];
			tab.label = [NSString stringWithUTF8String:c->getDisplayLabel().c_str()];
			tab.view = (__bridge NSView *)c->createEditor();
			[tabs addTabViewItem:tab];
		}
		// Like the Gtk dialog, closing the window drops the edits.
		if (runEditorPanel(tabs, "")) {
			for (const PD_RDFSemanticItemHandle & c : cl) {
				c->updateFromEditorData();
			}
		}
	}

	virtual void importFromDataComplete(std::istream & /*iss*/, PD_DocumentRDFHandle /*rdf*/,
	                                    PD_DocumentRDFMutationHandle m,
	                                    PD_DocumentRange * /*pDocRange*/ = nullptr) override
	{
		// The editor is filled from the imported member values; committing
		// it writes them to the document RDF.
		this->createEditor();
		this->updateFromEditorData(m);
	}

	virtual std::string getImportFromFileName(const std::string & filename_const,
	                                          std::list<std::pair<std::string, std::string>> types) const override
	{
		NSOpenPanel * panel = [NSOpenPanel openPanel];
		panel.allowedContentTypes = contentTypes(types);
		if ([panel runModal] != NSModalResponseOK) {
			return filename_const;
		}
		return panel.URL.path.UTF8String;
	}

	virtual std::string getExportToFileName(const std::string & filename_const, std::string defaultExtension,
	                                        std::list<std::pair<std::string, std::string>> types) const override
	{
		if (!filename_const.empty()) {
			return filename_const;
		}
		NSSavePanel * panel = [NSSavePanel savePanel];
		panel.allowedContentTypes = contentTypes(types);
		if ([panel runModal] != NSModalResponseOK) {
			return filename_const;
		}
		std::string filename = panel.URL.path.UTF8String;
		if (!ends_with(filename, defaultExtension)) {
			filename += defaultExtension;
		}
		return filename;
	}

private:
	static NSArray<UTType *> * contentTypes(const std::list<std::pair<std::string, std::string>> & types)
	{
		NSMutableArray<UTType *> * ret = [NSMutableArray array];
		for (const auto & t : types) {
			if (UTType * type = [UTType typeWithFilenameExtension:[NSString stringWithUTF8String:t.second.c_str()]]) {
				[ret addObject:type];
			}
		}
		return ret;
	}
};

class AP_RDFContactCocoa : public AP_RDFSemanticItemCocoaInjected<AP_RDFContact>
{
public:
	using AP_RDFSemanticItemCocoaInjected::AP_RDFSemanticItemCocoaInjected;

	virtual void * createEditor() override
	{
		// Same fields and labels as pd_RDFContact.ui.
		EditorFields f;
		w_name = f.add(@"Name", m_name);
		w_nick = f.add(@"Nick", m_nick);
		w_email = f.add(@"EMail", m_email);
		w_homePage = f.add(@"Home Page", m_homePage);
		w_imageUrl = f.add(@"Image", m_imageUrl);
		w_phone = f.add(@"Phone", m_phone);
		m_editor = f.view();
		return (__bridge void *)m_editor;
	}

	virtual void updateFromEditorData(PD_DocumentRDFMutationHandle m) override
	{
		// Same as AP_RDFContactGTK::updateFromEditorData().
		if (m_linkingSubject.toString().empty()) {
			m_linkingSubject = XAP_App::getApp()->createUUIDString();
		}
		std::string predBase = "http://xmlns.com/foaf/0.1/";
		setRDFType(m, predBase + "Person");
		updateTriple(m, m_name, text(w_name), predBase + "name");
		updateTriple(m, m_nick, text(w_nick), predBase + "nick");
		updateTriple(m, m_email, text(w_email), predBase + "mbox");
		updateTriple(m, m_homePage, text(w_homePage), predBase + "homepage");
		updateTriple(m, m_imageUrl, text(w_imageUrl), predBase + "image");
		updateTriple(m, m_phone, text(w_phone), predBase + "phone");
		// No Jabber ID field: the Gtk editor has none either.
	}

private:
	NSView * m_editor = nil;
	NSTextField * w_name = nil;
	NSTextField * w_nick = nil;
	NSTextField * w_email = nil;
	NSTextField * w_homePage = nil;
	NSTextField * w_imageUrl = nil;
	NSTextField * w_phone = nil;
};

class AP_RDFEventCocoa : public AP_RDFSemanticItemCocoaInjected<AP_RDFEvent>
{
public:
	using AP_RDFSemanticItemCocoaInjected::AP_RDFSemanticItemCocoaInjected;

	virtual void * createEditor() override
	{
		// Same fields and labels as pd_RDFEvent.ui.
		EditorFields f;
		w_summary = f.add(@"Summary", m_summary);
		w_location = f.add(@"Location", m_location);
		w_desc = f.add(@"Description", m_desc);
		w_dtstart = f.add(@"Start", toTimeString(m_dtstart));
		w_dtend = f.add(@"End", toTimeString(m_dtend));
		m_editor = f.view();
		return (__bridge void *)m_editor;
	}

	virtual void updateFromEditorData(PD_DocumentRDFMutationHandle m) override
	{
		// Same as AP_RDFEventGTK::updateFromEditorData().
		if (m_linkingSubject.toString().empty()) {
			m_linkingSubject = XAP_App::getApp()->createUUIDString();
		}
		std::string predBase = "http://www.w3.org/2002/12/cal/icaltzd#";
		setRDFType(m, predBase + "Vevent");
		updateTriple(m, m_uid, std::string(m_uid), predBase + "uid");
		updateTriple(m, m_summary, text(w_summary), predBase + "summary");
		updateTriple(m, m_location, text(w_location), predBase + "location");
		updateTriple(m, m_desc, text(w_desc), predBase + "description");
		updateTriple(m, m_dtstart, parseTimeString(text(w_dtstart)), predBase + "dtstart");
		updateTriple(m, m_dtend, parseTimeString(text(w_dtend)), predBase + "dtend");
	}

private:
	NSView * m_editor = nil;
	NSTextField * w_summary = nil;
	NSTextField * w_location = nil;
	NSTextField * w_desc = nil;
	NSTextField * w_dtstart = nil;
	NSTextField * w_dtend = nil;
};

class AP_RDFLocationCocoa : public AP_RDFSemanticItemCocoaInjected<AP_RDFLocation>
{
public:
	using AP_RDFSemanticItemCocoaInjected::AP_RDFSemanticItemCocoaInjected;

	virtual void * createEditor() override
	{
		// Same fields and labels as pd_RDFLocation.ui, the map below them.
		EditorFields f;
		w_name = f.add(@"Name", m_name);
		w_desc = f.add(@"Description", m_desc);
		w_dlat = f.add(@"Lat", tostr(m_dlat));
		w_dlong = f.add(@"Long", tostr(m_dlong));

		MKMapView * map = [[MKMapView alloc] initWithFrame:NSMakeRect(0, 0, 640, 480)];
		map.translatesAutoresizingMaskIntoConstraints = NO;
		[map.widthAnchor constraintEqualToConstant:640].active = YES;
		[map.heightAnchor constraintEqualToConstant:480].active = YES;
		CLLocationCoordinate2D centre = { m_dlat, m_dlong };
		if (fabs(m_dlat) <= 90 && fabs(m_dlong) <= 180) {
			// About champlain's zoom level 8 at this size.
			[map setRegion:MKCoordinateRegionMake(centre, MKCoordinateSpanMake(2.0, 3.5)) animated:NO];
		}
		m_mapDelegate = [[AP_CocoaLocationMapDelegate alloc] init];
		m_mapDelegate.latField = w_dlat;
		m_mapDelegate.longField = w_dlong;
		map.delegate = m_mapDelegate;

		NSStackView * stack = [NSStackView stackViewWithViews:@[f.view(), map]];
		stack.orientation = NSUserInterfaceLayoutOrientationVertical;
		stack.alignment = NSLayoutAttributeLeading;
		stack.spacing = 12;
		m_editor = stack;
		return (__bridge void *)m_editor;
	}

	virtual void updateFromEditorData(PD_DocumentRDFMutationHandle m) override
	{
		// Same as AP_RDFLocationGTK::updateFromEditorData().
		std::string dcBase = "http://purl.org/dc/elements/1.1/";
		std::string rdfBase = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
		if (m_linkingSubject.toString().empty()) {
			m_linkingSubject = XAP_App::getApp()->createUUIDString();
		}
		if (!m_isGeo84 && m_joiner.empty()) {
			m_joiner = PD_Object(XAP_App::getApp()->createUUIDString());
			m->add(linkingSubject(), PD_URI(rdfBase + "rest"), m_joiner);
		}
		updateTriple(m, m_desc, text(w_desc), dcBase + "title");
		double newLat = toType<double>(text(w_dlat));
		double newLong = toType<double>(text(w_dlong));
		if (m_isGeo84) {
			std::string wgs84Base = "http://www.w3.org/2003/01/geo/wgs84_pos#";
			updateTriple(m, m_dlat, newLat, wgs84Base + "lat");
			updateTriple(m, m_dlong, newLong, wgs84Base + "long");
		} else {
			updateTriple(m, m_dlat, newLat, rdfBase + "first", linkingSubject());
			updateTriple(m, m_dlong, newLong, rdfBase + "first", m_joiner);
		}
	}

private:
	NSView * m_editor = nil;
	AP_CocoaLocationMapDelegate * m_mapDelegate = nil;
	NSTextField * w_name = nil;
	NSTextField * w_desc = nil;
	NSTextField * w_dlat = nil;
	NSTextField * w_dlong = nil;
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
