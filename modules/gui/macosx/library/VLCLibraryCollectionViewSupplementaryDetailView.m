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

#import "library/VLCLibraryUIUnits.h"
#import "views/VLCSubScrollView.h"

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

- (void)mouseDown:(NSEvent *)event
{
    // Do not propagate the event as this will lead to subject collection view item being deselected
}

- (void)drawRect:(NSRect)dirtyRect
{
    if(NSEqualSizes(_arrowSize, NSZeroSize)) {
        _arrowSize = NSMakeSize(kArrowWidth, kArrowHeight);
    }

    if (_layoutScrollDirection == NSCollectionViewScrollDirectionVertical) {
        [self drawBackgroundWithTopArrow];
    } else if (_layoutScrollDirection == NSCollectionViewScrollDirectionHorizontal) {
        [self drawBackgroundWithLeftArrow];
    }
}

- (void)drawBackgroundWithTopArrow
{
    const NSRect selectedItemFrame = _selectedItem.view.frame;
    const NSPoint itemCenterPoint = NSMakePoint(NSMinX(selectedItemFrame) + NSWidth(selectedItemFrame) / 2,
                                                NSMinY(selectedItemFrame) + NSHeight(selectedItemFrame) / 2);
    const NSRect backgroundRect = NSMakeRect(NSMinX(self.bounds),
                                             NSMinY(self.bounds),
                                             NSWidth(self.bounds) + 2,
                                             NSHeight(self.bounds) - _arrowSize.height);
    const CGFloat backgroundTop = NSMaxY(backgroundRect);
    const CGFloat backgroundLeft = NSMinX(backgroundRect);

    const NSPoint arrowLeftPoint = NSMakePoint(itemCenterPoint.x - _arrowSize.width / 2, backgroundTop);
    const NSPoint arrowTopPoint = NSMakePoint(itemCenterPoint.x, backgroundTop + kArrowHeight - 1);
    const NSPoint arrowRightPoint = NSMakePoint(itemCenterPoint.x + _arrowSize.width / 2, backgroundTop);

    const NSPoint topLeftCorner = NSMakePoint(backgroundLeft, backgroundTop);
    const NSPoint topLeftCornerAfterCurve = NSMakePoint(backgroundLeft + kBackgroundCornerRadius, backgroundTop);

    const NSBezierPath *backgroundPath = [NSBezierPath bezierPathWithRoundedRect:backgroundRect xRadius:kBackgroundCornerRadius yRadius:kBackgroundCornerRadius];

    [backgroundPath moveToPoint:topLeftCornerAfterCurve];
    [backgroundPath lineToPoint:arrowLeftPoint];
    [backgroundPath curveToPoint:arrowTopPoint
                   controlPoint1:NSMakePoint(itemCenterPoint.x - _arrowSize.width / 6, backgroundTop)
                   controlPoint2:NSMakePoint(itemCenterPoint.x - kArrowTipRadius, backgroundTop + _arrowSize.height)];
    [backgroundPath curveToPoint:arrowRightPoint
                   controlPoint1:NSMakePoint(itemCenterPoint.x + kArrowTipRadius, backgroundTop + _arrowSize.height)
                   controlPoint2:NSMakePoint(itemCenterPoint.x + _arrowSize.width / 6, backgroundTop)];

    [backgroundPath closePath];
    [self colorBackground:backgroundPath];
}

- (void)drawBackgroundWithLeftArrow
{
    const NSRect selectedItemFrame = _selectedItem.view.frame;
    const NSPoint itemCenterPoint = NSMakePoint(NSMinX(selectedItemFrame) + NSWidth(selectedItemFrame) / 2,
                                                NSMinY(selectedItemFrame) + NSHeight(selectedItemFrame) / 2);
    const NSRect backgroundRect = NSMakeRect(NSMinX(self.bounds) + _arrowSize.height,
                                             NSMinY(self.bounds),
                                             NSWidth(self.bounds) - _arrowSize.height,
                                             NSHeight(self.bounds));
    const CGFloat backgroundBottom = NSMinY(backgroundRect);
    const CGFloat backgroundLeft = NSMinX(backgroundRect);

    const NSPoint arrowBottomPoint = NSMakePoint(backgroundLeft, itemCenterPoint.y + _arrowSize.width / 2);
    const NSPoint arrowLeftMostPoint = NSMakePoint(backgroundLeft - kArrowHeight + 1, itemCenterPoint.y);
    const NSPoint arrowTopPoint = NSMakePoint(backgroundLeft, itemCenterPoint.y - _arrowSize.width / 2);

    const NSPoint bottomLeftCorner = NSMakePoint(backgroundLeft, backgroundBottom);
    const NSPoint bottomLeftCornerAfterCurve = NSMakePoint(backgroundLeft, backgroundBottom + kBackgroundCornerRadius);

    const NSBezierPath *backgroundPath = [NSBezierPath bezierPathWithRoundedRect:backgroundRect xRadius:kBackgroundCornerRadius yRadius:kBackgroundCornerRadius];

    [backgroundPath moveToPoint:bottomLeftCornerAfterCurve];
    [backgroundPath lineToPoint:arrowBottomPoint];
    [backgroundPath curveToPoint:arrowLeftMostPoint
                   controlPoint1:NSMakePoint(backgroundLeft, itemCenterPoint.y + _arrowSize.width / 6)
                   controlPoint2:NSMakePoint(backgroundLeft - _arrowSize.height, itemCenterPoint.y + kArrowTipRadius)];
    [backgroundPath curveToPoint:arrowTopPoint
                   controlPoint1:NSMakePoint(backgroundLeft - _arrowSize.height, itemCenterPoint.y - kArrowTipRadius)
                   controlPoint2:NSMakePoint(backgroundLeft, itemCenterPoint.y - _arrowSize.width / 6)];

    [backgroundPath closePath];
    [self colorBackground:backgroundPath];
}

- (void)colorBackground:(const NSBezierPath*)backgroundPath
{
    //[[NSColor.gridColor colorWithAlphaComponent:self.container.alphaValue] setFill];
    [NSColor.gridColor setFill];
    [backgroundPath fill];

    //[[NSColor.gridColor colorWithAlphaComponent:self.container.alphaValue] setStroke];
    [NSColor.gridColor setStroke];
    [backgroundPath stroke];
}

- (NSScrollView *)parentScrollView
{
    if(_internalScrollView == nil) {
        return nil;
    }

    return _internalScrollView.parentScrollView;
}

- (void)setParentScrollView:(NSScrollView *)parentScrollView
{
    if(_internalScrollView == nil) {
        NSLog(@"Library collection view supplementary view has no internal scroll view -- cannot set parent scrollview.");
        return;
    }

    _internalScrollView.parentScrollView = parentScrollView;
}

- (BOOL)validConstraintProps
{
    return _contentViewTopConstraint != nil &&
           _contentViewLeftConstraint != nil &&
           _contentViewRightConstraint != nil &&
           _contentViewBottomConstraint != nil;
}

- (void)setLayoutScrollDirection:(NSCollectionViewScrollDirection)layoutScrollDirection
{
    _layoutScrollDirection = layoutScrollDirection;

    if (_layoutScrollDirection == NSCollectionViewScrollDirectionVertical && [self validConstraintProps]) {
        _contentViewTopConstraint.constant = kArrowHeight + [VLCLibraryUIUnits mediumSpacing];
        _contentViewBottomConstraint.constant = [VLCLibraryUIUnits mediumSpacing];
        _contentViewLeftConstraint.constant = [VLCLibraryUIUnits mediumSpacing];
        _contentViewRightConstraint.constant = [VLCLibraryUIUnits mediumSpacing];
    } else if (_layoutScrollDirection == NSCollectionViewScrollDirectionHorizontal  && [self validConstraintProps]) {
        _contentViewTopConstraint.constant = [VLCLibraryUIUnits mediumSpacing];
        _contentViewBottomConstraint.constant = [VLCLibraryUIUnits mediumSpacing];
        _contentViewLeftConstraint.constant = kArrowHeight + [VLCLibraryUIUnits mediumSpacing];
        _contentViewRightConstraint.constant = [VLCLibraryUIUnits mediumSpacing];
    }

    self.needsDisplay = YES;
}

- (void)updateRepresentation
{
    [self doesNotRecognizeSelector:_cmd];
    return;
}

- (void)setRepresentedItem:(VLCLibraryRepresentedItem *)representedItem
{
    _representedItem = representedItem;
    [self updateRepresentation];
}

@end
