/*****************************************************************************
 * VLCSubScrollView.h: MacOS X interface module
 *****************************************************************************
 *
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

#import "VLCSubScrollView.h"

@implementation VLCSubScrollView

- (instancetype)init
{
    self = [super init];

    if(self) {
        [self setup];
    }

    return self;
}

- (void)awakeFromNib
{
    [self setup];
}

- (void)setup
{
    _scrollParentX = NO;
    _scrollParentY = NO;
    _scrollSelf = YES;
}

- (void)scrollWheel:(NSEvent *)event
{
    if (!_scrollSelf) {
        [self.nextResponder scrollWheel:event];
        return;
    }

    if (!_scrollParentX && !_scrollParentY) {
        [super scrollWheel:event];
        return;
    }

    const NSRect documentVisibleRect = self.documentVisibleRect;
    const NSRect documentFrame = self.documentView.frame;

    const BOOL isViewAtYStartAndScrollUp = NSMinY(documentVisibleRect) <= NSMinY(documentFrame) + 1.0 && event.deltaY > 0;
    const BOOL isViewAtYEndAndScrollDown = NSMaxY(documentVisibleRect) >= NSMaxY(documentFrame) - 1.0 && event.deltaY < 0;
    const BOOL isViewAtXStartAndScrollLeft = NSMinX(documentVisibleRect) <= NSMinX(documentFrame) + 1.0 && event.deltaX > 0;
    const BOOL isViewAtXEndAndScrollRight = NSMaxX(documentVisibleRect) >= NSMaxX(documentFrame) - 1.0 && event.deltaX < 0;

    const BOOL isSubScrollViewScrollableY = NSHeight(documentFrame) > NSHeight(documentVisibleRect);
    const BOOL isSubScrollViewScrollableX = NSWidth(documentFrame) > NSWidth(documentVisibleRect);

    const BOOL shouldScrollParentY = _scrollParentY && (!isSubScrollViewScrollableY || isViewAtYStartAndScrollUp || isViewAtYEndAndScrollDown);
    const BOOL shouldScrollParentX = _scrollParentX && (!isSubScrollViewScrollableX || isViewAtXStartAndScrollLeft || isViewAtXEndAndScrollRight);

    if (shouldScrollParentY || shouldScrollParentX) {
        [self.nextResponder scrollWheel:event];
    } else {
        [super scrollWheel:event];
    }
}

- (BOOL)hasVerticalScroller
{
    if (_forceHideVerticalScroller) {
        return NO;
    }

    return [super hasVerticalScroller];
}

- (BOOL)hasHorizontalScroller
{
    if (_forceHideHorizontalScroller) {
        return NO;
    }

    return [super hasHorizontalScroller];
}

@end
