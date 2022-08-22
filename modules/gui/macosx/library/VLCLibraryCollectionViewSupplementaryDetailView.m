/*****************************************************************************
 * VLCLibraryCollectionViewSupplementaryDetailView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <claudio.cambra@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCLibraryCollectionViewSupplementaryDetailView.h"

static const CGFloat kArrowHeight = 20.;
static const CGFloat kArrowWidth = 50.;
static const CGFloat kArrowTipRadius = 2.5;
static const CGFloat kBackgroundCornerRadius = 10.;

@interface VLCLibraryCollectionViewSupplementaryDetailView ()
{
    NSSize _arrowSize;
}
@end

@implementation VLCLibraryCollectionViewSupplementaryDetailView

- (void)drawRect:(NSRect)dirtyRect {
    if(NSEqualSizes(_arrowSize, NSZeroSize)) {
        _arrowSize = NSMakeSize(kArrowWidth, kArrowHeight);
    }

    NSPoint itemCenterPoint = NSMakePoint(NSMinX(_selectedItemFrame) + NSWidth(_selectedItemFrame) / 2,
                                          NSMinY(_selectedItemFrame) + NSHeight(_selectedItemFrame) / 2);
    NSRect backgroundRect = NSMakeRect(NSMinX(self.bounds),
                                       NSMinY(self.bounds), NSWidth(self.bounds) + 2,
                                       NSHeight(self.bounds) - _arrowSize.height);
    CGFloat backgroundTop = NSMaxY(backgroundRect);
    CGFloat backgroundLeft = NSMinX(backgroundRect);

    NSPoint arrowLeftPoint = NSMakePoint(itemCenterPoint.x - _arrowSize.width / 2, backgroundTop);
    NSPoint arrowTopPoint = NSMakePoint(itemCenterPoint.x, backgroundTop - 1);
    NSPoint arrowRightPoint = NSMakePoint(itemCenterPoint.x + _arrowSize.width / 2, backgroundTop);

    NSPoint topLeftCorner = NSMakePoint(backgroundLeft, backgroundTop);
    NSPoint topLeftCornerAfterCurve = NSMakePoint(backgroundLeft + kBackgroundCornerRadius, backgroundTop);

    NSBezierPath *backgroundPath = [NSBezierPath bezierPathWithRoundedRect:backgroundRect xRadius:kBackgroundCornerRadius yRadius:kBackgroundCornerRadius];
    backgroundPath.flatness = 1.0;
    backgroundPath.lineJoinStyle = NSLineJoinStyleRound;
    backgroundPath.lineWidth = 0.5;

    [backgroundPath moveToPoint:topLeftCornerAfterCurve];
    [backgroundPath lineToPoint:arrowLeftPoint];
    [backgroundPath curveToPoint:arrowTopPoint
                   controlPoint1:NSMakePoint(itemCenterPoint.x - _arrowSize.width / 6, backgroundTop)
                   controlPoint2:NSMakePoint(itemCenterPoint.x - kArrowTipRadius, backgroundTop + _arrowSize.height)];
    [backgroundPath curveToPoint:arrowRightPoint
                   controlPoint1:NSMakePoint(itemCenterPoint.x + kArrowTipRadius, backgroundTop + _arrowSize.height)
                   controlPoint2:NSMakePoint(itemCenterPoint.x + _arrowSize.width / 6, backgroundTop)];

    [backgroundPath closePath];

    //[[NSColor.gridColor colorWithAlphaComponent:self.container.alphaValue] setFill];
    [NSColor.gridColor setFill];
    [backgroundPath fill];

    //[[NSColor.gridColor colorWithAlphaComponent:self.container.alphaValue] setStroke];
    [NSColor.gridColor setStroke];
    [backgroundPath stroke];
}

@end

