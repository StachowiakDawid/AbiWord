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

#include "ut_std_string.h"
#include "ut_locale.h"
#include "pp_PropertyMap.h"

#include "gr_CocoaGraphics.h"
#include "xap_CocoaDialog_Utilities.h"
#include "xap_CocoaFrame.h"
#include "xap_App.h"

#include "ap_Strings.h"
#include "ap_CocoaDialog_Border_Shading.h"

// Same order as the Gtk dialog and PP_PropertyMap::TypeLineStyle - 1.
static const char * s_borderStyles[BORDER_SHADING_NUMOFSTYLES] = {
	"0", // none
	"1", // solid
	"2", // dotted
	"3", // dashed
};

static const char * s_sizes[] = {
	"1/4 pt", "1/2 pt", "3/4 pt", "1 pt", "1 1/2 pt", "2 1/4 pt", "3 pt", "4 1/2 pt", "6 pt",
};

static const NSSize s_previewSize = { 220, 160 };

@interface AP_CocoaDialog_Border_ShadingController : NSObject <NSWindowDelegate>
{
@public
	AP_CocoaDialog_Border_Shading * _xap;
	NSPanel * _window;
	NSButton * _topBtn;
	NSButton * _bottomBtn;
	NSButton * _leftBtn;
	NSButton * _rightBtn;
	NSColorWell * _borderColorWell;
	NSPopUpButton * _thicknessPopup;
	NSPopUpButton * _stylePopup;
	NSButton * _shadingCheck;
	NSColorWell * _shadingColorWell;
	NSPopUpButton * _offsetPopup;
	XAP_CocoaNSView * _preview;
}
- (instancetype)initWithOwner:(AP_CocoaDialog_Border_Shading *)owner;
@end

/*****************************************************************/

XAP_Dialog * AP_CocoaDialog_Border_Shading::static_constructor(XAP_DialogFactory * pFactory,
                                                               XAP_Dialog_Id dlgid)
{
	return new AP_CocoaDialog_Border_Shading(pFactory, dlgid);
}

AP_CocoaDialog_Border_Shading::AP_CocoaDialog_Border_Shading(XAP_DialogFactory * pDlgFactory,
                                                             XAP_Dialog_Id dlgid)
	: AP_Dialog_Border_Shading(pDlgFactory, dlgid),
	  m_pPreviewWidget(nullptr),
	  m_dlg(nil)
{
}

AP_CocoaDialog_Border_Shading::~AP_CocoaDialog_Border_Shading(void)
{
	DELETEP(m_pPreviewWidget);
}

void AP_CocoaDialog_Border_Shading::runModeless(XAP_Frame * /*pFrame*/)
{
	m_dlg = [[AP_CocoaDialog_Border_ShadingController alloc] initWithOwner:this];

	DELETEP(m_pPreviewWidget);
	GR_CocoaAllocInfo ai(m_dlg->_preview);
	m_pPreviewWidget = static_cast<GR_CocoaGraphics *>(XAP_App::getApp()->newGraphics(ai));
	m_pPreviewWidget->init3dColors();
	_createPreviewFromGC(m_pPreviewWidget,
	                     static_cast<UT_uint32>(s_previewSize.width),
	                     static_cast<UT_uint32>(s_previewSize.height));
	m_dlg->_preview.drawable = m_pBorderShadingPreview;

	setAllSensitivities();
	m_pBorderShadingPreview->queueDraw();

	[m_dlg->_window center];
	[m_dlg->_window makeKeyAndOrderFront:nil];
	startUpdater();
}

void AP_CocoaDialog_Border_Shading::setSensitivity(bool /*bSens*/)
{
	// Like the Gtk dialog: reflect the current block's borders in the toggles.
	m_dlg->_topBtn.state = getTopToggled() ? NSControlStateValueOn : NSControlStateValueOff;
	m_dlg->_bottomBtn.state = getBottomToggled() ? NSControlStateValueOn : NSControlStateValueOff;
	m_dlg->_leftBtn.state = getLeftToggled() ? NSControlStateValueOn : NSControlStateValueOff;
	m_dlg->_rightBtn.state = getRightToggled() ? NSControlStateValueOn : NSControlStateValueOff;

	bool bShading = (m_dlg->_shadingCheck.state == NSControlStateValueOn);
	m_dlg->_shadingColorWell.enabled = bShading;
	m_dlg->_offsetPopup.enabled = bShading;
}

void AP_CocoaDialog_Border_Shading::setBorderStyleInGUI(const std::string & sStyle)
{
	NSInteger index = PP_PropertyMap::linestyle_type(sStyle.c_str()) - 1;
	if (index >= 0 && index < BORDER_SHADING_NUMOFSTYLES) {
		[m_dlg->_stylePopup selectItemAtIndex:index];
	}
}

void AP_CocoaDialog_Border_Shading::setBorderThicknessInGUI(const std::string & sThick)
{
	[m_dlg->_thicknessPopup selectItemAtIndex:_findClosestThickness(sThick.c_str())];
}

void AP_CocoaDialog_Border_Shading::setBorderColorInGUI(const UT_RGBColor & clr)
{
	m_dlg->_borderColorWell.color = GR_CocoaGraphics::_utRGBColorToNSColor(clr);
}

void AP_CocoaDialog_Border_Shading::setShadingColorInGUI(const UT_RGBColor & clr)
{
	m_dlg->_shadingColorWell.color = GR_CocoaGraphics::_utRGBColorToNSColor(clr);
}

void AP_CocoaDialog_Border_Shading::setShadingPatternInGUI(const std::string & sPattern)
{
	bool bEnable = (sPattern != BORDER_SHADING_SHADING_DISABLE);
	m_dlg->_shadingCheck.state = bEnable ? NSControlStateValueOn : NSControlStateValueOff;
	m_dlg->_shadingColorWell.enabled = bEnable;
	m_dlg->_offsetPopup.enabled = bEnable;
}

void AP_CocoaDialog_Border_Shading::setShadingOffsetInGUI(const std::string & sOffset)
{
	[m_dlg->_offsetPopup selectItemAtIndex:_findClosestOffset(sOffset.c_str())];
}

void AP_CocoaDialog_Border_Shading::event_Close(void)
{
	m_answer = AP_Dialog_Border_Shading::a_CLOSE;
	destroy();
}

void AP_CocoaDialog_Border_Shading::event_previewInvalidate(void)
{
	if (m_pBorderShadingPreview) {
		m_pBorderShadingPreview->queueDraw();
	}
}

void AP_CocoaDialog_Border_Shading::event_BorderThicknessChanged(NSInteger index)
{
	if (index < 0 || index >= BORDER_SHADING_NUMTHICKNESS) {
		return;
	}
	std::string sThickness;
	{
		UT_LocaleTransactor t(LC_NUMERIC, "C");
		sThickness = UT_std_string_sprintf("%fin", m_dThickness[index]);
	}
	setBorderThickness(sThickness);
	event_previewInvalidate();
}

void AP_CocoaDialog_Border_Shading::event_BorderStyleChanged(NSInteger index)
{
	if (index >= 0 && index < BORDER_SHADING_NUMOFSTYLES) {
		setBorderStyle(s_borderStyles[index]);
		event_previewInvalidate();
	}
}

void AP_CocoaDialog_Border_Shading::event_ShadingOffsetChanged(NSInteger index)
{
	if (index < 0 || index >= BORDER_SHADING_NUMOFFSETS) {
		return;
	}
	std::string sOffset;
	{
		UT_LocaleTransactor t(LC_NUMERIC, "C");
		sOffset = UT_std_string_sprintf("%fin", m_dShadingOffset[index]);
	}
	setShadingOffset(sOffset);
	event_previewInvalidate();
}

void AP_CocoaDialog_Border_Shading::event_ShadingEnableChanged(bool bEnable)
{
	setShadingPattern(bEnable ? BORDER_SHADING_SHADING_ENABLE : BORDER_SHADING_SHADING_DISABLE);
	m_dlg->_shadingColorWell.enabled = bEnable;
	m_dlg->_offsetPopup.enabled = bEnable;
}

void AP_CocoaDialog_Border_Shading::destroy(void)
{
	finalize();
	if (m_dlg) {
		m_dlg->_preview.drawable = nullptr;
		bool bWindowClosing = (m_dlg->_xap == nullptr); // from windowWillClose:
		m_dlg->_xap = nullptr; // closing the window must not call back
		if (!bWindowClosing) {
			[m_dlg->_window close];
		}
		// We may be inside one of the controller's methods: release it
		// (and its window) only once the current event is done.
		AP_CocoaDialog_Border_ShadingController * dlg = m_dlg;
		m_dlg = nil;
		dispatch_async(dispatch_get_main_queue(), ^{ (void)dlg; });
	}
	DELETEP(m_pPreviewWidget);
}

void AP_CocoaDialog_Border_Shading::activate(void)
{
	ConstructWindowName();
	m_dlg->_window.title = [NSString stringWithUTF8String:getWindowName()];
	[m_dlg->_window makeKeyAndOrderFront:nil];
	setAllSensitivities();
}

void AP_CocoaDialog_Border_Shading::notifyActiveFrame(XAP_Frame * /*pFrame*/)
{
	ConstructWindowName();
	m_dlg->_window.title = [NSString stringWithUTF8String:getWindowName()];
	setAllSensitivities();
}

/*****************************************************************/

@implementation AP_CocoaDialog_Border_ShadingController

static NSTextField * s_label(const XAP_StringSet * pSS, XAP_String_Id sid)
{
	return [NSTextField labelWithString:LocalizedLabel(pSS, sid)];
}

static NSTextField * s_header(const XAP_StringSet * pSS, XAP_String_Id sid)
{
	NSTextField * label = s_label(pSS, sid);
	label.font = [NSFont boldSystemFontOfSize:[NSFont systemFontSize]];
	return label;
}

static NSPopUpButton * s_sizePopup(id target, SEL action)
{
	NSPopUpButton * popup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
	for (const char * size : s_sizes) {
		[popup addItemWithTitle:[NSString stringWithUTF8String:size]];
	}
	popup.target = target;
	popup.action = action;
	return popup;
}

- (NSButton *)borderToggle:(NSString *)toolbarIcon symbol:(NSString *)symbol action:(SEL)action
{
	NSButton * btn = [NSButton buttonWithImage:XAP_CocoaSymbolImage(toolbarIcon, symbol, nil)
	                                    target:self action:action];
	[btn setButtonType:NSButtonTypePushOnPushOff];
	btn.bezelStyle = NSBezelStyleFlexiblePush;
	return btn;
}

- (instancetype)initWithOwner:(AP_CocoaDialog_Border_Shading *)owner
{
	if (!(self = [super init])) {
		return nil;
	}
	_xap = owner;
	const XAP_StringSet * pSS = XAP_App::getApp()->getStringSet();

	_topBtn = [self borderToggle:@"tb_LineTop" symbol:@"rectangle.tophalf.inset.filled" action:@selector(topAction:)];
	_bottomBtn = [self borderToggle:@"tb_LineBottom" symbol:@"rectangle.bottomhalf.inset.filled" action:@selector(bottomAction:)];
	_leftBtn = [self borderToggle:@"tb_LineLeft" symbol:@"rectangle.lefthalf.inset.filled" action:@selector(leftAction:)];
	_rightBtn = [self borderToggle:@"tb_LineRight" symbol:@"rectangle.righthalf.inset.filled" action:@selector(rightAction:)];
	NSStackView * toggles = [NSStackView stackViewWithViews:@[_topBtn, _bottomBtn, _leftBtn, _rightBtn]];

	_borderColorWell = [NSColorWell colorWellWithStyle:NSColorWellStyleMinimal];
	_borderColorWell.target = self;
	_borderColorWell.action = @selector(borderColorAction:);
	_thicknessPopup = s_sizePopup(self, @selector(thicknessAction:));

	_stylePopup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
	AppendLocalizedMenuItem(_stylePopup, pSS, AP_STRING_ID_DLG_BorderShading_Border_Style_None, 0);
	AppendLocalizedMenuItem(_stylePopup, pSS, AP_STRING_ID_DLG_BorderShading_Border_Style_Solid, 1);
	AppendLocalizedMenuItem(_stylePopup, pSS, AP_STRING_ID_DLG_BorderShading_Border_Style_Dotted, 2);
	AppendLocalizedMenuItem(_stylePopup, pSS, AP_STRING_ID_DLG_BorderShading_Border_Style_Dashed, 3);
	_stylePopup.target = self;
	_stylePopup.action = @selector(styleAction:);

	_shadingCheck = [NSButton checkboxWithTitle:LocalizedLabel(pSS, AP_STRING_ID_DLG_BorderShading_Use_Shading)
	                                     target:self action:@selector(shadingAction:)];
	_shadingColorWell = [NSColorWell colorWellWithStyle:NSColorWellStyleMinimal];
	_shadingColorWell.target = self;
	_shadingColorWell.action = @selector(shadingColorAction:);
	_offsetPopup = s_sizePopup(self, @selector(offsetAction:));

	NSGridView * grid = [NSGridView gridViewWithViews:@[
		@[s_header(pSS, AP_STRING_ID_DLG_BorderShading_Borders), [NSGridCell emptyContentView]],
		@[[NSGridCell emptyContentView], toggles],
		@[s_label(pSS, AP_STRING_ID_DLG_BorderShading_Border_Color), _borderColorWell],
		@[s_label(pSS, AP_STRING_ID_DLG_BorderShading_Thickness), _thicknessPopup],
		@[s_label(pSS, AP_STRING_ID_DLG_BorderShading_Border_Style), _stylePopup],
		@[s_header(pSS, AP_STRING_ID_DLG_BorderShading_Shading), [NSGridCell emptyContentView]],
		@[[NSGridCell emptyContentView], _shadingCheck],
		@[s_label(pSS, AP_STRING_ID_DLG_BorderShading_Shading_Color), _shadingColorWell],
		@[s_label(pSS, AP_STRING_ID_DLG_BorderShading_Offset), _offsetPopup],
	]];
	[grid columnAtIndex:0].xPlacement = NSGridCellPlacementTrailing;
	grid.yPlacement = NSGridCellPlacementCenter; // color wells have no baseline
	// section headers sit at the leading edge, above their rows
	const NSInteger headerRows[] = { 0, 5 };
	for (NSInteger row : headerRows) {
		[grid cellAtColumnIndex:0 rowIndex:row].xPlacement = NSGridCellPlacementLeading;
		[grid rowAtIndex:row].topPadding = (row ? 8 : 0);
	}

	_preview = [[XAP_CocoaNSView alloc] initWith:nullptr
	                                    andFrame:NSMakeRect(0, 0, s_previewSize.width, s_previewSize.height)
	                                     andName:@"Border and shading preview"];
	_preview.translatesAutoresizingMaskIntoConstraints = NO;
	[_preview.widthAnchor constraintEqualToConstant:s_previewSize.width].active = YES;
	[_preview.heightAnchor constraintEqualToConstant:s_previewSize.height].active = YES;
	NSStackView * previewBox = [NSStackView stackViewWithViews:@[
		s_header(pSS, AP_STRING_ID_DLG_BorderShading_Preview), _preview]];
	previewBox.orientation = NSUserInterfaceLayoutOrientationVertical;
	previewBox.alignment = NSLayoutAttributeLeading;

	NSStackView * body = [NSStackView stackViewWithViews:@[grid, previewBox]];
	body.alignment = NSLayoutAttributeTop;
	body.spacing = 20;

	NSButton * closeBtn = [NSButton buttonWithTitle:LocalizedLabel(pSS, XAP_STRING_ID_DLG_Close)
	                                         target:self action:@selector(closeAction:)];
	closeBtn.keyEquivalent = @"\033";
	NSButton * applyBtn = [NSButton buttonWithTitle:LocalizedLabel(pSS, XAP_STRING_ID_DLG_Apply)
	                                         target:self action:@selector(applyAction:)];
	applyBtn.keyEquivalent = @"\r";
	NSStackView * buttons = [NSStackView stackViewWithViews:@[closeBtn, applyBtn]];

	NSStackView * content = [NSStackView stackViewWithViews:@[body, buttons]];
	content.orientation = NSUserInterfaceLayoutOrientationVertical;
	content.alignment = NSLayoutAttributeTrailing;
	content.spacing = 16;
	content.translatesAutoresizingMaskIntoConstraints = NO;

	NSView * container = [[NSView alloc] initWithFrame:NSZeroRect];
	[container addSubview:content];
	[NSLayoutConstraint activateConstraints:@[
		[content.leadingAnchor constraintEqualToAnchor:container.leadingAnchor constant:20],
		[content.trailingAnchor constraintEqualToAnchor:container.trailingAnchor constant:-20],
		[content.topAnchor constraintEqualToAnchor:container.topAnchor constant:20],
		[content.bottomAnchor constraintEqualToAnchor:container.bottomAnchor constant:-20],
	]];

	_window = [[NSPanel alloc] initWithContentRect:NSZeroRect
	                                     styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
	                                       backing:NSBackingStoreBuffered
	                                         defer:YES];
	_window.releasedWhenClosed = NO;
	_window.hidesOnDeactivate = YES;
	_window.contentView = container;
	_window.delegate = self;
	owner->ConstructWindowName();
	_window.title = [NSString stringWithUTF8String:owner->getWindowName()];
	return self;
}

- (void)windowWillClose:(NSNotification *)notification
{
	UT_UNUSED(notification);
	// Closed from the title bar: let the XAP dialog finalize. destroy()
	// clears _xap first, so this does not recurse.
	if (AP_CocoaDialog_Border_Shading * xap = _xap) {
		_xap = nullptr;
		xap->event_Close();
	}
}

- (void)toggle:(AP_Dialog_Border_Shading::toggle_button)which from:(NSButton *)sender
{
	if (_xap) {
		_xap->toggleLineType(which, sender.state == NSControlStateValueOn);
		_xap->event_previewInvalidate();
	}
}

- (void)topAction:(id)sender    { [self toggle:AP_Dialog_Border_Shading::toggle_top from:sender]; }
- (void)bottomAction:(id)sender { [self toggle:AP_Dialog_Border_Shading::toggle_bottom from:sender]; }
- (void)leftAction:(id)sender   { [self toggle:AP_Dialog_Border_Shading::toggle_left from:sender]; }
- (void)rightAction:(id)sender  { [self toggle:AP_Dialog_Border_Shading::toggle_right from:sender]; }

- (void)borderColorAction:(NSColorWell *)sender
{
	UT_RGBColor clr;
	GR_CocoaGraphics::_utNSColorToRGBColor(sender.color, clr);
	_xap->setBorderColor(clr);
	_xap->event_previewInvalidate();
}

- (void)shadingColorAction:(NSColorWell *)sender
{
	UT_RGBColor clr;
	GR_CocoaGraphics::_utNSColorToRGBColor(sender.color, clr);
	_xap->setShadingColor(clr);
	_xap->event_previewInvalidate();
}

- (void)thicknessAction:(NSPopUpButton *)sender { _xap->event_BorderThicknessChanged(sender.indexOfSelectedItem); }
- (void)styleAction:(NSPopUpButton *)sender     { _xap->event_BorderStyleChanged(sender.indexOfSelectedItem); }
- (void)offsetAction:(NSPopUpButton *)sender    { _xap->event_ShadingOffsetChanged(sender.indexOfSelectedItem); }

- (void)shadingAction:(NSButton *)sender
{
	_xap->event_ShadingEnableChanged(sender.state == NSControlStateValueOn);
	_xap->event_previewInvalidate();
}

- (void)applyAction:(id)sender
{
	UT_UNUSED(sender);
	_xap->applyChanges();
}

- (void)closeAction:(id)sender
{
	UT_UNUSED(sender);
	_xap->event_Close();
}

@end
