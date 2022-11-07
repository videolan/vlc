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
        _scrollParentX = NO;
        _scrollParentY = NO;
    }

    return self;
}

- (void)scrollWheel:(NSEvent *)event
{
    [super scrollWheel:event];

    if(_parentScrollView == nil || (!_scrollParentX && !_scrollParentY)) {
        return;
    }

    // Sometimes scroll views initialise with the Y value being almost 0, but not quite (e.g. 0.000824)
    const BOOL isViewAtYStartAndScrollUp = self.verticalScroller.floatValue <= 0.01 && event.deltaY > 0;
    const BOOL isViewAtYEndAndScrollDown = self.verticalScroller.floatValue >= 0.99 && event.deltaY < 0;
    const BOOL isViewAtXStartAndScrollLeft = self.horizontalScroller.floatValue <= 0.01 && event.deltaX > 0;
    const BOOL isViewAtXEndAndScrollRight = self.horizontalScroller.floatValue >= 0.99 && event.deltaX < 0;

    const BOOL isSubScrollViewScrollableY = self.documentView.frame.size.height > self.documentVisibleRect.size.height;
    const BOOL isSubScrollViewScrollableX = self.documentView.frame.size.width > self.documentVisibleRect.size.width;

    const BOOL shouldScrollParentY = _scrollParentY && (!isSubScrollViewScrollableY || isViewAtYStartAndScrollUp || isViewAtYEndAndScrollDown);
    const BOOL shouldScrollParentX = _scrollParentX && (!isSubScrollViewScrollableX || isViewAtXStartAndScrollLeft || isViewAtXEndAndScrollRight);

    if(shouldScrollParentY || shouldScrollParentX) {
        [_parentScrollView scrollWheel:event];
    }
}

@end
