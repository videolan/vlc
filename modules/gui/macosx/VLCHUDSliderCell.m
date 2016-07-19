/*****************************************************************************
 * VLCHUDSliderCell.m: Custom slider cell UI for dark HUD Panels
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Marvin Scholz <epirat07 -at- gmail -dot- com>
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

#import "VLCHUDSliderCell.h"

@implementation VLCHUDSliderCell

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        // Custom colors for the slider
        _sliderColor            = [NSColor colorWithCalibratedRed:0.318 green:0.318 blue:0.318 alpha:0.6];
        _disabledSliderColor    = [NSColor colorWithCalibratedRed:0.318 green:0.318 blue:0.318 alpha:0.2];
        _strokeColor            = [NSColor colorWithCalibratedRed:0.749 green:0.761 blue:0.788 alpha:1.0];
        _disabledStrokeColor    = [NSColor colorWithCalibratedRed:0.749 green:0.761 blue:0.788 alpha:0.2];

        // Custom knob gradients
        _knobGradient           = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251 green:0.251 blue:0.255 alpha:1.0]
                                                                endingColor:[NSColor colorWithDeviceRed:0.118 green:0.118 blue:0.118 alpha:1.0]];
        _disableKnobGradient    = [[NSGradient alloc] initWithStartingColor:[NSColor colorWithDeviceRed:0.251 green:0.251 blue:0.255 alpha:1.0]
                                                                endingColor:[NSColor colorWithDeviceRed:0.118 green:0.118 blue:0.118 alpha:1.0]];
    }
    return self;
}

NSAffineTransform* RotationTransform(const CGFloat angle, const NSPoint point)
{
    NSAffineTransform*	transform = [NSAffineTransform transform];
    [transform translateXBy:point.x yBy:point.y];
    [transform rotateByRadians:angle];
    [transform translateXBy:-(point.x) yBy:-(point.y)];
    return transform;
}

- (void) drawKnob:(NSRect)smallRect
{
    NSBezierPath *path = [NSBezierPath bezierPath];
    // Inset rect to have enough room for the stroke
    smallRect = NSInsetRect(smallRect, 0.5, 0.5);

    // Get min/max/mid coords for shape calculations
    CGFloat minX = NSMinX(smallRect);
    CGFloat minY = NSMinY(smallRect);
    CGFloat maxX = NSMaxX(smallRect);
    CGFloat maxY = NSMaxY(smallRect);
    CGFloat midX = NSMidX(smallRect);
    CGFloat midY = NSMidY(smallRect);

    // Draw the knobs shape
    if (self.numberOfTickMarks > 0) {
        // We have tickmarks, draw an arrow-like shape
        if (self.isVertical) {
            // For some reason the rect is not alligned correctly at
            // tickmarks and clipped, so this ugly thing is necessary:
            maxY = maxY - 2;
            midY = midY - 1;

            // Right pointing arrow
            [path moveToPoint:NSMakePoint(minX + 3, minY)];
            [path lineToPoint:NSMakePoint(midX + 2, minY)];
            [path lineToPoint:NSMakePoint(maxX, midY)];
            [path lineToPoint:NSMakePoint(midX + 2, maxY)];
            [path lineToPoint:NSMakePoint(minX + 3, maxY)];
            [path appendBezierPathWithArcFromPoint:NSMakePoint(minX, maxY)
                                           toPoint:NSMakePoint(minX, maxY - 3)
                                            radius:2.5f];
            [path lineToPoint:NSMakePoint(minX, maxY - 3)];
            [path lineToPoint:NSMakePoint(minX, minY + 3)];
            [path appendBezierPathWithArcFromPoint:NSMakePoint(minX, minY)
                                           toPoint:NSMakePoint(minX + 3, minY)
                                            radius:2.5f];
            [path closePath];
        } else {
            // Down pointing arrow
            [path moveToPoint:NSMakePoint(minX + 3, minY)];
            [path lineToPoint:NSMakePoint(maxX - 3, minY)];
            [path appendBezierPathWithArcFromPoint:NSMakePoint(maxX, minY)
                                           toPoint:NSMakePoint(maxX, minY + 3)
                                            radius:2.5f];
            [path lineToPoint:NSMakePoint(maxX, minY + 3)];
            [path lineToPoint:NSMakePoint(maxX, midY + 2)];
            [path lineToPoint:NSMakePoint(midX, maxY)];
            [path lineToPoint:NSMakePoint(minX, midY + 2)];
            [path lineToPoint:NSMakePoint(minX, minY + 3)];
            [path appendBezierPathWithArcFromPoint:NSMakePoint(minX, minY)
                                           toPoint:NSMakePoint(minX + 3, minY)
                                            radius:2.5f];
            [path closePath];
        }

        // Rotate our knob if needed to the correct position
        if (self.tickMarkPosition == NSTickMarkAbove) {
            NSAffineTransform *transform = nil;
            transform = RotationTransform(M_PI, NSMakePoint(midX, midY));
            [path transformUsingAffineTransform:transform];
        }
    } else {
        // We have no tickmarks, draw a round knob
        [path appendBezierPathWithOvalInRect:NSInsetRect(smallRect, 0.5, 0.5)];
    }

    // Draw the knobs background
    if (self.isEnabled && self.isHighlighted) {
        [_knobGradient drawInBezierPath:path angle:270.0f];
    } else if (self.isEnabled) {
        [_knobGradient drawInBezierPath:path angle:90.0f];
    } else {
        [_disableKnobGradient drawInBezierPath:path angle:90.0f];
    }

    // Draw white stroke around the knob
    if (self.isEnabled) {
        [_strokeColor setStroke];
    } else {
        [_disabledStrokeColor setStroke];
    }

    [path setLineWidth:1.0];
    [path stroke];
}

- (void)drawBarInside:(NSRect)fullRect flipped:(BOOL)flipped
{
    NSBezierPath *path;

    // Determine current position of knob
    CGFloat knobPosition = (self.doubleValue - self.minValue) / (self.maxValue - self.minValue);

    // Copy rect
    NSRect activeRect = fullRect;

    // Do not draw disabled part for sliders with tickmarks
    if (self.numberOfTickMarks == 0) {
        if (self.isVertical) {
            // Calculate active rect (bottom part of slider)
            if (flipped) {
                activeRect.origin.y = (1 - knobPosition) * activeRect.size.height;
                activeRect.size.height -= activeRect.origin.y - 1;
            } else {
                activeRect.size.height -= (1 - knobPosition) * activeRect.size.height - 1;
            }
        } else {
            // Calculate active rect (left part of slider)
            activeRect.size.width = knobPosition * (self.controlView.frame.size.width - 1.0);
        }

        // Draw inactive bar
        [_disabledSliderColor setFill];
        path = [NSBezierPath bezierPathWithRoundedRect:fullRect xRadius:2.0 yRadius:2.0];
        [path fill];
    }

    // Draw active bar
    [_sliderColor setFill];
    path = [NSBezierPath bezierPathWithRoundedRect:activeRect xRadius:2.0 yRadius:2.0];
    [path fill];
}

- (void)drawTickMarks
{
    for (int i = 0; i < self.numberOfTickMarks; i++) {
        NSRect tickMarkRect = [self rectOfTickMarkAtIndex:i];
        if (self.isEnabled) {
            [_strokeColor setFill];
        } else {
            [_disabledStrokeColor setFill];
        }
        NSRectFill(tickMarkRect);
    }
}

@end
