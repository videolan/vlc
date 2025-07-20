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

#import <QuartzCore/QuartzCore.h>


@implementation VLCMainVideoViewOverlayView
{
    CAGradientLayer *_gradientLayer;
}

- (void)awakeFromNib
{
    self.darkestGradientColor = [NSColor colorWithCalibratedWhite:0 alpha:0.4];

    self.wantsLayer = YES;
    self.layer = [CALayer layer];

    _gradientLayer = [CAGradientLayer layer];
    _gradientLayer.frame = self.bounds;
    _gradientLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
    
    [self.layer addSublayer:_gradientLayer];
    [self updateGradient];
}

- (void)setFrame:(NSRect)frame
{
    [super setFrame:frame];
    _gradientLayer.frame = self.bounds;
}

- (void)setBounds:(NSRect)bounds
{
    [super setBounds:bounds];
    _gradientLayer.frame = self.bounds;
}

- (void)setDrawGradientForTopControls:(BOOL)drawGradientForTopControls
{
    if (_drawGradientForTopControls != drawGradientForTopControls) {
        _drawGradientForTopControls = drawGradientForTopControls;
        [self updateGradient];
    }
}

- (void)setDarkestGradientColor:(NSColor *)darkestGradientColor
{
    _darkestGradientColor = darkestGradientColor;
    [self updateGradient];
}

- (void)updateGradient
{
    if (!_gradientLayer)
        return;

    const CGColorRef darkColor = self.darkestGradientColor.CGColor;
    const CGColorRef clearColor = NSColor.clearColor.CGColor;

    if (self.drawGradientForTopControls) {
        _gradientLayer.colors = @[(__bridge id)darkColor, 
                                  (__bridge id)clearColor, 
                                  (__bridge id)darkColor];
        _gradientLayer.locations = @[@0.0, @0.5, @1.0];
    } else {
        _gradientLayer.colors = @[(__bridge id)darkColor, 
                                  (__bridge id)clearColor];
        _gradientLayer.locations = @[@0.0, @1.0];
    }

    _gradientLayer.startPoint = CGPointMake(0.5, 0.0);
    _gradientLayer.endPoint = CGPointMake(0.5, 1.0);
}

@end
