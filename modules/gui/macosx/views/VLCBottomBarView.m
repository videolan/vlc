/*****************************************************************************
 * VLCBottomBarView.m
 *****************************************************************************
 * Copyright (C) 2017-2018 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "VLCBottomBarView.h"

#import "extensions/NSView+VLCAdditions.h"
#import "extensions/NSGradient+VLCAdditions.h"

@interface VLCBottomBarView () {
    NSBezierPath *_rectanglePath;
    NSBezierPath *_separatorPath;
    NSRect _oldBounds;
}

@end

@implementation VLCBottomBarView

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];

    if (self) {
        [self commonInit];
    }

    return self;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];

    if (self) {
        [self commonInit];
    }

    return self;
}

- (instancetype)init
{
    self = [super init];

    if (self ) {
        [self commonInit];
    }

    return self;
}

- (void)commonInit
{
    _lightGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithSRGBRed:0.90 green:0.90 blue:0.90 alpha:1.0]
                                                   endingColor:[NSColor colorWithSRGBRed:0.82 green:0.82 blue:0.82 alpha:1.0]];
    _lightStroke = [NSColor colorWithSRGBRed:0.65 green:0.65 blue:0.65 alpha:1.0];

    if (@available(macOS 10.14, *)) {
        _darkGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithSRGBRed:0.27 green:0.27 blue:0.27 alpha:1.0]
                                                      endingColor:[NSColor colorWithSRGBRed:0.22 green:0.22 blue:0.22 alpha:1.0]];
        _darkStroke = [NSColor colorWithSRGBRed:0.17 green:0.17 blue:0.18 alpha:1.0];
        [self viewDidChangeEffectiveAppearance];
    } else {
        _darkGradient = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithSRGBRed:0.24 green:0.24 blue:0.24 alpha:1.0]
                                                      endingColor:[NSColor colorWithSRGBRed:0.07 green:0.07 blue:0.07 alpha:1.0]];
        _darkStroke = [NSColor blackColor];
    }
}

- (void)calculatePaths
{
    if (CGRectEqualToRect(_oldBounds, self.bounds))
        return;

    _oldBounds = self.bounds;

    NSRect barRect = _oldBounds;
    CGFloat rectangleCornerRadius = 4;

    // Calculate bottom bar path
    NSRect rectangleInnerRect = NSInsetRect(barRect, rectangleCornerRadius, rectangleCornerRadius);

    _rectanglePath = [NSBezierPath bezierPath];
    [_rectanglePath appendBezierPathWithArcWithCenter: NSMakePoint(NSMinX(rectangleInnerRect), NSMinY(rectangleInnerRect)) radius: rectangleCornerRadius startAngle: 180 endAngle: 270];
    [_rectanglePath appendBezierPathWithArcWithCenter: NSMakePoint(NSMaxX(rectangleInnerRect), NSMinY(rectangleInnerRect)) radius: rectangleCornerRadius startAngle: 270 endAngle: 360];
    [_rectanglePath lineToPoint: NSMakePoint(NSMaxX(barRect), NSMaxY(barRect))];
    [_rectanglePath lineToPoint: NSMakePoint(NSMinX(barRect), NSMaxY(barRect))];
    [_rectanglePath closePath];

    // Calculate bottom bar separator stroke
    _separatorPath = [NSBezierPath bezierPath];
    [_separatorPath moveToPoint:NSMakePoint(NSMinX(barRect), NSMaxY(barRect) - 0.5)];
    [_separatorPath lineToPoint:NSMakePoint(NSMaxX(barRect), NSMaxY(barRect) - 0.5)];
    [_separatorPath setLineWidth:1.0];
    [_separatorPath stroke];
}

- (void)drawRect:(NSRect)dirtyRect
{
    [self calculatePaths];

    NSRect barRect = self.bounds;

    if (NSIsEmptyRect(barRect))
        return;

    [[NSColor clearColor] setFill];
    NSRectFill(barRect);

    if (_isDark) {
        [_darkGradient vlc_safeDrawInBezierPath:_rectanglePath angle:270.0];
        [_darkStroke setStroke];
    } else {
        [_lightGradient vlc_safeDrawInBezierPath:_rectanglePath angle:270.0];
        [_lightStroke setStroke];
    }

    [_separatorPath stroke];
}

- (BOOL)isFlipped
{
    return NO;
}

- (void)viewDidChangeEffectiveAppearance
{
    [self setDark:self.shouldShowDarkAppearance];
    [self setNeedsDisplay:YES];
}

@end
