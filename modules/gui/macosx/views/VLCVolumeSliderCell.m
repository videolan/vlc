/*****************************************************************************
 * VLCVolumeSliderCell.m
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCVolumeSliderCell.h"

#import "extensions/NSGradient+VLCAdditions.h"
#import "main/CompatibilityFixes.h"

@interface VLCVolumeSliderCell () {
    BOOL _isRTL;
}
@end

@implementation VLCVolumeSliderCell

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setSliderStyleLight];
        _isRTL = ([self userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft);
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setSliderStyleLight];
        _isRTL = ([self userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft);
    }
    return self;
}

- (void)setSliderStyleLight
{
    // Color Declarations
    _gradientColor = [NSColor colorWithCalibratedRed: 0.663 green: 0.663 blue: 0.663 alpha: 1];
    _gradientColor2 = [NSColor colorWithCalibratedRed: 0.749 green: 0.749 blue: 0.753 alpha: 1];
    _trackStrokeColor = [NSColor colorWithCalibratedRed: 0.619 green: 0.624 blue: 0.623 alpha: 1];
    _filledTrackColor = [NSColor colorWithCalibratedRed: 0.55 green: 0.55 blue: 0.55 alpha: 1];
    _knobFillColor = [NSColor colorWithCalibratedRed: 1 green: 1 blue: 1 alpha: 1];
    _activeKnobFillColor = [NSColor colorWithCalibratedRed: 0.95 green: 0.95 blue: 0.95 alpha: 1];
    _shadowColor = [NSColor colorWithCalibratedRed: 0.32 green: 0.32 blue: 0.32 alpha: 1];
    _knobStrokeColor = [NSColor colorWithCalibratedRed: 0.592 green: 0.596 blue: 0.596 alpha: 1];

    // Gradient Declarations
    _trackGradient = [[NSGradient alloc] initWithColorsAndLocations:
                      _gradientColor, 0.0,
                      [_gradientColor blendedColorWithFraction:0.5 ofColor:_gradientColor2], 0.60,
                      _gradientColor2, 1.0, nil];
    _knobGradientAngleHighlighted = 270;
    _knobGradientAngle = 90;

    // Shadow Declarations
    _knobShadow = [[NSShadow alloc] init];
    _knobShadow.shadowColor = _shadowColor;
    _knobShadow.shadowOffset = NSMakeSize(0, 0);
    _knobShadow.shadowBlurRadius = 2;

    _highlightBackground = [NSColor colorWithCalibratedRed:0.20 green:0.55 blue:0.91 alpha:1.0];
    NSColor *highlightAccent = [NSColor colorWithCalibratedRed:0.4588235294 green:0.7254901961 blue:0.9882352941 alpha:1.0];
    _highlightGradient = [[NSGradient alloc] initWithColors:@[
                                                              _highlightBackground,
                                                              highlightAccent,
                                                              _highlightBackground
                                                              ]];
}

- (void)setSliderStyleDark
{
    // Color Declarations
    if (@available(macOS 10.14, *)) {
        _gradientColor = [NSColor colorWithCalibratedRed: 0.20 green: 0.20 blue: 0.20 alpha: 1];
        _knobFillColor = [NSColor colorWithCalibratedRed: 0.81 green: 0.81 blue: 0.81 alpha: 1];
        _activeKnobFillColor = [NSColor colorWithCalibratedRed: 0.76 green: 0.76 blue: 0.76 alpha: 1];
        _knobStrokeColor = [NSColor colorWithCalibratedRed:0.29 green:0.29 blue:0.29 alpha:1];
        _knobGradientAngleHighlighted = 90;
        _knobGradientAngle = 270;
    } else {
        _gradientColor = [NSColor colorWithCalibratedRed: 0.24 green: 0.24 blue: 0.24 alpha: 1];
        _knobFillColor = [NSColor colorWithCalibratedRed:0.7 green:0.7 blue:0.7 alpha: 1];
        _activeKnobFillColor = [NSColor colorWithCalibratedRed: 0.95 green: 0.95 blue: 0.95 alpha: 1];
        _knobStrokeColor = [NSColor colorWithCalibratedRed:0 green:0 blue:0 alpha:1];
        _knobGradientAngleHighlighted = 270;
        _knobGradientAngle = 90;
    }
    _gradientColor2 = [NSColor colorWithCalibratedRed: 0.15 green: 0.15 blue: 0.15 alpha: 1];
    _trackStrokeColor = [NSColor colorWithCalibratedRed: 0.23 green: 0.23 blue: 0.23 alpha: 1];
    _filledTrackColor = [NSColor colorWithCalibratedRed: 0.15 green: 0.15 blue: 0.15 alpha: 1];
    _shadowColor = [NSColor colorWithCalibratedRed: 0.32 green: 0.32 blue: 0.32 alpha: 1];

    NSColor* knobGradientColor = [NSColor colorWithSRGBRed: 0.15 green: 0.15 blue: 0.15 alpha: 1];
    NSColor* knobGradientColor2 = [NSColor colorWithSRGBRed: 0.30 green: 0.30 blue: 0.30 alpha: 1];

    // Gradient Declarations
    _trackGradient = [[NSGradient alloc] initWithColorsAndLocations:
                      _gradientColor, 0.0,
                      [_gradientColor blendedColorWithFraction:0.5 ofColor:_gradientColor2], 0.60,
                      _gradientColor2, 1.0, nil];

    _knobGradient = [[NSGradient alloc] initWithStartingColor:knobGradientColor
                                                  endingColor:knobGradientColor2];


    // Shadow Declarations
    _knobShadow = [[NSShadow alloc] init];
    _knobShadow.shadowColor = _shadowColor;
    _knobShadow.shadowOffset = NSMakeSize(0, 0);
    _knobShadow.shadowBlurRadius = 2;

    _highlightBackground = [NSColor colorWithCalibratedRed:0.20 green:0.55 blue:0.91 alpha:1.0];
    NSColor *highlightAccent = [NSColor colorWithCalibratedRed:0.4588235294 green:0.7254901961 blue:0.9882352941 alpha:1.0];
    _highlightGradient = [[NSGradient alloc] initWithColors:@[
                                                              _highlightBackground,
                                                              highlightAccent,
                                                              _highlightBackground
                                                              ]];
}

- (void)drawKnob:(NSRect)knobRect
{
    // Draw knob
    NSBezierPath* knobPath = [NSBezierPath bezierPathWithOvalInRect:NSInsetRect(knobRect, 1.0, 1.0)];
    if (self.isHighlighted) {
        if (_knobGradient) {
            [_knobGradient vlc_safeDrawInBezierPath:knobPath angle:_knobGradientAngleHighlighted];
        } else {
            [_activeKnobFillColor setFill];
        }
    } else {
        if (_knobGradient) {
            [_knobGradient vlc_safeDrawInBezierPath:knobPath angle:_knobGradientAngle];
        } else {
            [_knobFillColor setFill];
        }
    }

    if (!_knobGradient)
        [knobPath fill];

    [_knobStrokeColor setStroke];
    knobPath.lineWidth = 0.5;

    [NSGraphicsContext saveGraphicsState];
    if (self.isHighlighted)
        [_knobShadow set];
    [knobPath stroke];
    [NSGraphicsContext restoreGraphicsState];
}

- (void)drawBarInside:(NSRect)rect flipped:(BOOL)flipped
{
    // Inset rect
    rect = NSInsetRect(rect, 1.0, 1.0);

    // Empty Track Drawing
    NSBezierPath* emptyTrackPath = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:1 yRadius:1];

    // Calculate filled track
    NSRect leadingTrackRect = rect;
    NSRect knobRect = [self knobRectFlipped:NO];
    CGFloat sliderCenter = knobRect.origin.x  + (self.knobThickness / 2);

    leadingTrackRect.size.width = sliderCenter;

    // Filled Track Drawing
    CGFloat leadingTrackCornerRadius = 2;
    NSBezierPath* leadingTrackPath = [NSBezierPath bezierPathWithRoundedRect:leadingTrackRect
                                                                     xRadius:leadingTrackCornerRadius
                                                                     yRadius:leadingTrackCornerRadius];

    if (_isRTL) {
        // In RTL mode, first fill the whole slider,
        // then only redraw the empty part.

        // Empty part drawing
        [_filledTrackColor setFill];
        [emptyTrackPath fill];

        // Filled part drawing
        [_trackGradient vlc_safeDrawInBezierPath:leadingTrackPath angle:-90];
    } else {
        // Empty part drawing
        [_trackGradient vlc_safeDrawInBezierPath:emptyTrackPath angle:-90];

        // Filled part drawing
        [_filledTrackColor setFill];
        [leadingTrackPath fill];
    }

    [_trackStrokeColor setStroke];
    emptyTrackPath.lineWidth = 1;
    [emptyTrackPath stroke];
}

@end
