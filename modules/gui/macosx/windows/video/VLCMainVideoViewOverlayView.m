/*****************************************************************************
 * VLCMainVideoViewOverlayView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCMainVideoViewOverlayView.h"

@interface VLCMainVideoViewOverlayView ()
{
    NSColor *_darkestGradientColor;
}
@end

@implementation VLCMainVideoViewOverlayView

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    // Drawing code here.
    if (_darkestGradientColor == nil) {
        _darkestGradientColor = [NSColor colorWithWhite:0 alpha:0.8];
    }

    const CGFloat bottomBarTopPositionRelative = (_bottomBarTopBoundary * 1.25) / self.frame.size.height;
    NSGradient *gradient = [[NSGradient alloc] initWithColorsAndLocations:_darkestGradientColor, 0,
                                                                          [NSColor clearColor], bottomBarTopPositionRelative,
                                                                          nil];
    [gradient drawInRect:self.frame angle:90];
}

@end
