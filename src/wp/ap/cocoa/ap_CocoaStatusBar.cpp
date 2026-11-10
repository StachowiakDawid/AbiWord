/* AbiWord
 * Copyright (C) 1998 AbiSource, Inc.
 * Copyright (C) 2001-2004 Hubert Figuiere
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
#include "ut_types.h"
#include "ut_assert.h"
#include "ut_debugmsg.h"
#include "xap_Frame.h"
#include "xap_CocoaFrame.h"
#include "ap_CocoaFrame.h"
#include "ap_CocoaStatusBar.h"
#import "xap_CocoaFrameImpl.h"

#define FIELD_SPACING 1.0f	/* room for the divider line */
#define FIELD_PADDING 8.0f	/* text inset on each side of a field */

class ap_csb_TextListener : public AP_StatusBarFieldListener
{
public:
	/* policy is as usual retain the object */
	ap_csb_TextListener(AP_StatusBarField *pStatusBarField, NSTextField *pLabel) 
		: AP_StatusBarFieldListener(pStatusBarField) 
	{
		m_pLabel = pLabel; 
	}
	virtual ~ap_csb_TextListener()
	{
	}
	virtual void notify(); 

protected:
	NSTextField *m_pLabel;
};

void ap_csb_TextListener::notify()
{
	UT_ASSERT(m_pLabel);

	AP_StatusBarField_TextInfo * textInfo = ((AP_StatusBarField_TextInfo *)m_pStatusBarField);

	NSString* str = [[NSString alloc] initWithUTF8String:textInfo->getBuf().c_str()];
	[m_pLabel setStringValue:str];

	// we conditionally update the size request, if the representative string (or an earlier
	// size) wasn't large enough, if the element uses the representative string method
	// and is aligned with the center
	if (textInfo->getFillMethod() == REPRESENTATIVE_STRING && 
			textInfo->getAlignmentMethod() == CENTER) {
		// TODO FIXME
	/*
		GtkRequisition requisition;
		gint iOldWidthRequest, iOldHeightRequest;
		gtk_widget_get_size_request(m_pLabel, &iOldWidthRequest, &iOldHeightRequest);
		gtk_widget_set_size_request(m_pLabel, -1, -1);		
		gtk_widget_size_request(m_pLabel, &requisition);
		if (requisition.width > iOldWidthRequest)			
			gtk_widget_set_size_request(m_pLabel, requisition.width, -1);		
		else
			gtk_widget_set_size_request(m_pLabel, iOldWidthRequest, -1);	
	*/
	}
}

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

AP_CocoaStatusBar::AP_CocoaStatusBar(XAP_Frame * pFrame)
	: AP_StatusBar(pFrame),
		m_wStatusBar(nil),
		m_hidden(false),
		m_requestedWidth(0),
		m_numMaxWidth(0)
{
	/* fetch the widget from the controller */
	m_wStatusBar = [(XAP_CocoaFrameController *)(static_cast<XAP_CocoaFrameImpl*>(m_pFrame->getFrameImpl()))->_getController() getStatusBar];
	[m_wStatusBar setPostsFrameChangedNotifications:YES];
	[m_wStatusBar setPostsBoundsChangedNotifications:YES];
	UT_DEBUGMSG(("XAP_CocoaNSStatusBar did subscribe to resize notification\n"));
	[m_wStatusBar setXAPOwner:this];
//	NSRect frame = [m_wStatusBar frame];
//	UT_DEBUGMSG(("m_wStatusBar x=%f y=%f w=%f h=%f\n", frame.origin.x, frame.origin.y,
//      frame.size.width, frame.size.height));
	[[NSNotificationCenter defaultCenter] addObserver:m_wStatusBar 
				selector:@selector(statusBarDidResize:) 
				name:NSViewFrameDidChangeNotification
				object:m_wStatusBar];
}

AP_CocoaStatusBar::~AP_CocoaStatusBar(void)
{
	if ((m_hidden) && ([m_wStatusBar superview] == nil))
	{
		[m_wStatusBar setXAPOwner:nil];
		[[NSNotificationCenter defaultCenter] removeObserver:m_wStatusBar];
	}
}


void AP_CocoaStatusBar::setView(AV_View * pView)
{
	AP_StatusBar::setView(pView);
}

XAP_CocoaNSStatusBar * AP_CocoaStatusBar::createWidget(void)
{
	UT_ASSERT(m_wStatusBar);
	float currentX = 0.0;
	float height = [m_wStatusBar frame].size.height;
	NSString* str;
	m_requestedWidth = 0;
	m_numMaxWidth = 0;
	
	NSFont* font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
	
	for (UT_sint32 k=0; k<getFields()->getItemCount(); k++) {
 		AP_StatusBarField * pf = (AP_StatusBarField *)m_vecFields.getNthItem(k);
		UT_ASSERT(pf); // we should NOT have null elements
		if (pf->getFillMethod() == REPRESENTATIVE_STRING || (pf->getFillMethod() == MAX_POSSIBLE)){
		  AP_StatusBarField_TextInfo *pf_TextInfo = static_cast<AP_StatusBarField_TextInfo*>(pf);
		  NSRect frame = NSMakeRect (currentX, 0, 100, height);
		  NSTextField *pStatusBarElementLabel = [[NSTextField alloc] initWithFrame:frame];
		  pf->setListener(static_cast<AP_StatusBarFieldListener *>(new ap_csb_TextListener
				(pf_TextInfo, pStatusBarElementLabel)));
		  // Plain labels, like the bottom bar of native macOS document windows.
		  [pStatusBarElementLabel setFont:font];
		  [pStatusBarElementLabel setEditable:NO];
		  [pStatusBarElementLabel setBezeled:NO];
		  [pStatusBarElementLabel setBordered:NO];
		  [pStatusBarElementLabel setDrawsBackground:NO];
		  [pStatusBarElementLabel setSelectable:NO];
		  [pStatusBarElementLabel setTextColor:[NSColor secondaryLabelColor]];
		  [[pStatusBarElementLabel cell] setLineBreakMode:NSLineBreakByTruncatingTail];
		// align text
		  switch (pf_TextInfo->getAlignmentMethod()) {
		  case LEFT:
		        [pStatusBarElementLabel setAlignment:NSTextAlignmentNatural];
			break;
		  case CENTER:
			[pStatusBarElementLabel setAlignment:NSTextAlignmentCenter];
			break;
		  default:
			UT_ASSERT_NOT_REACHED();
		  }
		
			
		// size and place
		  switch (pf_TextInfo->getFillMethod()) {
		  case REPRESENTATIVE_STRING:
			str = [[NSString alloc] initWithUTF8String:pf_TextInfo->getRepresentativeString()];
			[pStatusBarElementLabel setStringValue:str];
			[pStatusBarElementLabel sizeToFit];
			[pStatusBarElementLabel setStringValue:@""];
			frame = [pStatusBarElementLabel bounds];
			frame.size.width += 2.0f * FIELD_PADDING;
			m_requestedWidth += frame.size.width + FIELD_SPACING;
			[pStatusBarElementLabel setTag:lrintf(frame.size.width)];
			UT_DEBUGMSG(("New size is: w=%f h=%f\n", frame.size.width, frame.size.height));
			break;
		  case MAX_POSSIBLE:
			[pStatusBarElementLabel setTag:-1];
			m_numMaxWidth++;
			break;
		  default:
			UT_ASSERT_NOT_REACHED();
		  }
		  [m_wStatusBar addSubview:pStatusBarElementLabel];
		  UT_DEBUGMSG(("added status bar element. Frame = %f %f %f %f\n",
				frame.origin.x, frame.origin.y, frame.size.width, frame.size.height));

		  currentX += frame.size.width + FIELD_SPACING;
		}
	}
	_repositionFields([m_wStatusBar subviews]);
	
	return m_wStatusBar;
}

void AP_CocoaStatusBar::_repositionFields(NSArray *fields)
{
	NSRect barBounds = [m_wStatusBar bounds];
	float maxWidth = barBounds.size.width;
	float freeWidth = maxWidth - m_requestedWidth;
	float prevX = 0;

    NSEnumerator* e = [fields objectEnumerator];
    while(NSTextField *obj = [e nextObject])
    {
        int tag = [obj tag];
        // slot = the field's share of the bar; the label is inset in it
        float slotWidth = (tag == -1) ? (freeWidth / (float)m_numMaxWidth) - FIELD_SPACING : (float)tag;
        float textHeight = [[obj cell] cellSize].height;
        NSRect frame;
        frame.origin.x = prevX + FIELD_PADDING;
        frame.size.width = MAX(0.0f, slotWidth - 2.0f * FIELD_PADDING);
        frame.size.height = textHeight;
        frame.origin.y = rint((barBounds.size.height - textHeight) / 2.0);
        prevX += slotWidth + FIELD_SPACING;
        [obj setFrame:frame];
        UT_DEBUGMSG(("resized status bar element. Frame = %f %f %f %f\n",
                    frame.origin.x, frame.origin.y, frame.size.width, frame.size.height));
    }
    [m_wStatusBar setNeedsDisplay:YES];
}



void AP_CocoaStatusBar::show(void)
{
	UT_DEBUGMSG(("AP_CocoaStatusBar::show(void)\n"));
	if ([m_wStatusBar superview] == nil) {
		[m_superView addSubview:m_wStatusBar];
	}
	m_hidden = false;
}

void AP_CocoaStatusBar::hide(void)
{
	UT_DEBUGMSG(("AP_CocoaStatusBar::hide(void)\n"));
	if ([m_wStatusBar superview] != nil) {
		m_superView = [m_wStatusBar superview];
		UT_ASSERT (m_superView);
		[m_wStatusBar removeFromSuperview];
	}
	// TODO Check about resizing / layout changes
	m_hidden = true;
}

@implementation XAP_CocoaNSStatusBar

- (id)initWithFrame:(NSRect)frame
{
	if(self = [super initWithFrame:frame]) {

	}
	return self;
}

- (void)setXAPOwner:(AP_CocoaStatusBar*)owner
{
	_xap = owner;
}

- (void)drawRect:(NSRect)dirtyRect
{
	UT_UNUSED(dirtyRect);
	NSRect bounds = [self bounds];
	[[NSColor windowBackgroundColor] setFill];
	NSRectFill(bounds);

	[[NSColor separatorColor] setFill];
	// hairline separating the bar from the document area
	NSRectFill(NSMakeRect(0, NSMaxY(bounds) - 1.0, bounds.size.width, 1.0));
	// short dividers between the fields
	NSArray * fields = [self subviews];
	NSUInteger count = [fields count];
	for (NSUInteger i = 0; i + 1 < count; i++) {
		NSRect f = [[fields objectAtIndex:i] frame];
		CGFloat x = rint(NSMaxX(f) + FIELD_PADDING);
		NSRectFill(NSMakeRect(x, 4.0, 1.0, bounds.size.height - 8.0));
	}
}

- (void)statusBarDidResize:(NSNotification *)notification
{
	UT_UNUSED(notification);
	if (_xap) {
		UT_DEBUGMSG(("-[XAP_CocoaNSStatusBar statusBarDidResize:]\n"));
		_xap->_repositionFields([self subviews]);
	}
}

@end
