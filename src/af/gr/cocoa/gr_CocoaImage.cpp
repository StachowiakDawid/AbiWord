/* -*- mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode:nil; -*- */
/* AbiWord
 * Copyright (C) 2001-2002, 2009-2021 Hubert Figuière
 * Copyright (C) 1998 AbiSource, Inc.
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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "ut_assert.h"
#include "ut_bytebuf.h"
#include "ut_debugmsg.h"
#include "ut_std_string.h"

#import "xap_CocoaAbiConversions.h"

#include "gr_CocoaImage.h"
#include "gr_Graphics.h"

GR_CocoaImage::GR_CocoaImage(const char* szName)
    : m_grtype(GRT_Raster) // Probably the safest default.
    , m_image(nil)
    , m_bitmap(nil)
{
    setName(szName ? szName : "CocoaImage");
}

GR_CocoaImage::~GR_CocoaImage()
{
}

void GR_CocoaImage::_setImage(NSImage* image)
{
    m_image = image;
    m_bitmap = nil;
}

NSBitmapImageRep* GR_CocoaImage::_bitmap() const
{
    if (!m_bitmap && m_image) {
        CGImageRef cgImage = [m_image CGImageForProposedRect:nullptr context:nil hints:nil];
        if (cgImage) {
            m_bitmap = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
        }
    }
    return m_bitmap;
}

/*!
 * Store the image as PNG, like the other platforms do.
 */
bool GR_CocoaImage::convertToBuffer(UT_ConstByteBufPtr& pBB) const
{
    pBB.reset();
    NSBitmapImageRep* bitmap = _bitmap();
    UT_return_val_if_fail(bitmap, false);

    NSData* png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    UT_return_val_if_fail(png, false);

    UT_ByteBufPtr bb(new UT_ByteBuf);
    bb->append(static_cast<const UT_Byte*>([png bytes]), static_cast<UT_uint32>([png length]));
    pBB = bb;
    return true;
}

void GR_CocoaImage::setFromImageRep(NSImageRep* imageRep)
{
    NSSize size = imageRep.size;
    NSImage* image = [[NSImage alloc] initWithSize:size];
    [image addRepresentation:imageRep];
    _setImage(image);
    setDisplaySize((UT_sint32)lrintl(size.width), (UT_sint32)lrintl(size.height));
}

GR_Image* GR_CocoaImage::createImageSegment(GR_Graphics* pG, const UT_Rect& rec)
{
    UT_sint32 x = pG->tdu(rec.left);
    UT_sint32 y = pG->tdu(rec.top);
    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    UT_sint32 width = pG->tdu(rec.width);
    UT_sint32 height = pG->tdu(rec.height);
    UT_sint32 dH = getDisplayHeight();
    UT_sint32 dW = getDisplayWidth();
    if (height > dH) {
        height = dH;
    }
    if (width > dW) {
        width = dW;
    }
    if (x + width > dW) {
        width = dW - x;
    }
    if (y + height > dH) {
        height = dH - y;
    }
    if (width < 0) {
        x = dW - 1;
        width = 1;
    }
    if (height < 0) {
        y = dH - 1;
        height = 1;
    }
    std::string sName;
    getName(sName);
    std::string sSub = UT_std_string_sprintf("_segemnt_%d_%d_%d_%d", x, y, width, height);
    sName += sSub;

    GR_CocoaImage* image = new GR_CocoaImage(sName.c_str());
    if (!m_image || width <= 0 || height <= 0) {
        return image;
    }
    // Draw the requested part (display units, top-left origin) into a new
    // bitmap; the old code drew into an image that was never created.
    NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
        pixelsWide:width pixelsHigh:height bitsPerSample:8 samplesPerPixel:4
        hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bytesPerRow:0 bitsPerPixel:0];
    if (rep) {
        NSGraphicsContext* context = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:context];
        // whole image at display size, shifted so the segment lands at the origin
        NSRect dest = NSMakeRect(-x, y + height - dH, dW, dH);
        [m_image drawInRect:dest fromRect:NSZeroRect operation:NSCompositingOperationCopy fraction:1.0];
        [NSGraphicsContext restoreGraphicsState];
        image->setFromImageRep(rep);
    }
    return image;
}

/*! 
 * Returns true if pixel at point (x,y) in device units is transparent.
 * See gr_UnixImage.cpp for how it's done in GTK.
 */
bool GR_CocoaImage::isTransparentAt(UT_sint32 x, UT_sint32 y)
{
    if (!hasAlpha()) {
        return false;
    }
    NSBitmapImageRep* bitmap = _bitmap();
    UT_sint32 dW = getDisplayWidth();
    UT_sint32 dH = getDisplayHeight();
    UT_return_val_if_fail(bitmap && dW > 0 && dH > 0, false);
    UT_return_val_if_fail((x >= 0) && (x < dW) && (y >= 0) && (y < dH), false);

    // x, y are at display size; the bitmap has the image's own pixel size.
    NSInteger px = (NSInteger)((double)x * [bitmap pixelsWide] / dW);
    NSInteger py = (NSInteger)((double)y * [bitmap pixelsHigh] / dH);
    NSColor* color = [bitmap colorAtX:px y:py];
    return color && [color alphaComponent] == 0.0;
}

/*!
 * Returns true if there is any transparency in the image.
 */
bool GR_CocoaImage::hasAlpha(void) const
{
    NSBitmapImageRep* bitmap = _bitmap();
    return bitmap && [bitmap hasAlpha];
}

bool GR_CocoaImage::convertFromBuffer(const UT_ConstByteBufPtr& pBB, const std::string& mimetype,
    UT_sint32 iDisplayWidth, UT_sint32 iDisplayHeight)
{
    // We explicitely do not scale the image on load, since cairo can do
    // that during rendering. Scaling during loading will result in lost
    // image information, which in turns results in bugs like
    // in bugs like #12183.
    // So simply remember the display size for drawing later.
    setDisplaySize(iDisplayWidth, iDisplayHeight);

    if (mimetype == "image/svg+xml") {
        m_grtype = GRT_Vector;
        return true;
    }

    // PNG, JPEG, ... : anything NSImage can decode. Copy the bytes: NSImage
    // may decode lazily, after the byte buffer is gone.
    NSData* data = [NSData dataWithBytes:pBB->getPointer(0) length:pBB->getLength()];
    NSImage* image = [[NSImage alloc] initWithData:data];
    if (!image) {
        return false;
    }
    m_grtype = GRT_Raster;
    _setImage(image);
    return true;
}

bool GR_CocoaImage::render(GR_Graphics* /*pGR*/, UT_sint32 /*iDisplayWidth*/, UT_sint32 /*iDisplayHeight*/)
{
    UT_DEBUGMSG(("Choosing not to render what can't be a raster image!\n"));

    return false;
}
