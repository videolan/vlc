/*****************************************************************************
 * VLCDefaultValueSliderCell.m: SliderCell subclass for VLCDefaultValueSlider
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 -at- gmail -dot- com>
 *
 * This file uses parts of code from the GNUstep NSSliderCell code:
 * 
 *   Copyright (C) 1996,1999 Free Software Foundation, Inc.
 *   Author: Ovidiu Predescu <ovidiu@net-community.com>
 *   Date: September 1997
 *   Rewrite: Richard Frith-Macdonald <richard@brainstorm.co.uk>
 *   Date: 1999
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

#import "VLCDefaultValueSliderCell.h"

#import "main/CompatibilityFixes.h"

@interface VLCDefaultValueSliderCell (){
    BOOL _isRTL;
    BOOL _isFlipped;
    double _defaultValue;
    double _normalizedDefaultValue;
    NSColor *_defaultTickMarkColor;
}
@end

@implementation VLCDefaultValueSliderCell

#pragma mark -
#pragma mark Public interface

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setupSelf];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setupSelf];
    }
    return self;
}

- (void)setDefaultValue:(double)value
{
    if (value > self.maxValue || value < self.minValue)
        value = DBL_MAX;

    if (_defaultValue == DBL_MAX && value != DBL_MAX) {
        _drawTickMarkForDefault = YES;
        _snapsToDefault = YES;
    } else if (value == DBL_MAX) {
        _drawTickMarkForDefault = NO;
        _snapsToDefault = NO;
    }
    _defaultValue = value;
    _normalizedDefaultValue = (value == DBL_MAX) ? DBL_MAX : [self normalizedValue:_defaultValue];
    [[self controlView] setNeedsDisplay:YES];
}

- (double)defaultValue
{
    return _defaultValue;
}

- (void)setDefaultTickMarkColor:(NSColor *)color
{
    _defaultTickMarkColor = color;
    [[self controlView] setNeedsDisplay:YES];

}

- (NSColor *)defaultTickMarkColor
{
    return _defaultTickMarkColor;
}

- (void)drawDefaultTickMarkWithFrame:(NSRect)rect
{
    [_defaultTickMarkColor setFill];
    NSRectFill(rect);
}

#pragma mark -
#pragma mark Internal helpers

- (void)setupSelf
{
    _defaultValue = DBL_MAX;
    _normalizedDefaultValue = DBL_MAX;
    _isRTL = ([self userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft);
    _isFlipped = [[self controlView] isFlipped];
    _defaultTickMarkColor = [NSColor grayColor];
}

/*
 * Calculates the knobRect for a given position
 * This is later used to draw the default tick mark in the center of
 * where the knob would be, when it is at the default value.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"

- (NSRect)knobRectFlipped:(BOOL)flipped forValue:(double)doubleValue
 {
     NSRect resultRect;
     NSRect trackRect = self.trackRect;
     double val = [self normalizedValue:doubleValue] / 100;

     if (self.isVertical) {
         resultRect.origin.x = -1;
         resultRect.origin.y = (NSHeight(trackRect) - self.knobThickness) * val;
         if (_isRTL)
             resultRect.origin.y = (NSHeight(trackRect) - self.knobThickness) - resultRect.origin.y;
     } else {
         resultRect.origin.x = (NSWidth(trackRect) - self.knobThickness) * val;
         resultRect.origin.y = -1;
         if (_isRTL)
             resultRect.origin.x = (NSWidth(trackRect) - self.knobThickness) - resultRect.origin.x;
     }

     resultRect.size.height = self.knobThickness;
     resultRect.size.width = self.knobThickness;

     return [self.controlView backingAlignedRect:resultRect options:NSAlignAllEdgesNearest];
 }

#pragma mark -
#pragma mark Overwritten super methods

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    // Do all other drawing
    [super drawWithFrame:cellFrame inView:controlView];

    // Default tick mark
    if (_drawTickMarkForDefault && _defaultValue != DBL_MAX) {

        // Calculate rect for default tick mark
        CGFloat tickThickness = 1.0;

        NSRect tickFrame = [self knobRectFlipped:_isFlipped
                                        forValue:_defaultValue];

        if ([self isVertical]) {
            CGFloat mid = NSMidY(tickFrame);
            tickFrame.origin.x = cellFrame.origin.x;
            tickFrame.origin.y = mid - tickThickness/2.0;
            tickFrame.size.width = cellFrame.size.width;
            tickFrame.size.height = tickThickness;
        } else {
            CGFloat mid = NSMidX(tickFrame);
            // Ugly workaround
            // Corrects minor alignment issue on non-retina
            CGFloat scale = [[[self controlView] window] backingScaleFactor];
            tickFrame.size.height = cellFrame.size.height;
            tickFrame.origin.y = cellFrame.origin.y;
            if (scale > 1.0) {
                tickFrame.origin.x = mid;
            } else {
                tickFrame.origin.x = mid - tickThickness;
                tickFrame.size.height = cellFrame.size.height - 1;
                tickFrame.origin.y = cellFrame.origin.y - 1;
            }
            tickFrame.size.width = tickThickness;
        }

        NSAlignmentOptions alignOpts = NSAlignMinXOutward | NSAlignMinYOutward |
                                       NSAlignWidthOutward | NSAlignMaxYOutward;
        NSRect alignedRect = [[self controlView] backingAlignedRect:tickFrame options:alignOpts];

        // Draw default tick mark
        [self drawDefaultTickMarkWithFrame:alignedRect];
    }

    // Redraw knob
    [super drawKnob];
}
#pragma clang diagnostic pop

- (double)normalizedValue:(double)value
{
    double min = [self minValue];
    double max = [self maxValue];

    max -= min;
    value -= min;

    return (value / max) * 100;
}

- (BOOL)continueTracking:(NSPoint)lastPoint at:(NSPoint)currentPoint inView:(NSView *)controlView
{
    double oldValue = [self normalizedValue:self.doubleValue];
    BOOL result = [super continueTracking:lastPoint at:currentPoint inView:controlView];
    double newValue = [self normalizedValue:self.doubleValue];

    // If no change, nothing to do.
    if (newValue == oldValue)
        return result;

    // Determine in which direction slider is moving
    BOOL sliderMovingForward = (oldValue > newValue) ? NO : YES;

    // Claculate snap-threshhold
    double thresh = 100 * (self.knobThickness/3) / self.trackRect.size.width;

    // Snap to default value
    if (_snapsToDefault && ABS(newValue - _normalizedDefaultValue) < thresh) {
        if (sliderMovingForward && newValue > _normalizedDefaultValue) {
            [self setDoubleValue:_defaultValue];
        } else if (!sliderMovingForward && newValue < _normalizedDefaultValue) {
            [self setDoubleValue:_defaultValue];
        }
    }

    return result;
}

@end
