/*****************************************************************************
 * VLCDefaultValueSliderCell.m: SliderCell subclass for VLCDefaultValueSlider
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
 * $Id$
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

@interface VLCDefaultValueSliderCell (){
    BOOL _isRTL;
    BOOL _isFlipped;
    double _defaultValue;
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
    if (value > _maxValue || value < _minValue)
        value = DBL_MAX;

    if (_defaultValue == DBL_MAX && value != DBL_MAX) {
        _drawTickMarkForDefault = YES;
        _snapsToDefault = YES;
    } else if (value == DBL_MAX) {
        _drawTickMarkForDefault = NO;
        _snapsToDefault = NO;
    }
    _defaultValue = value;
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
    _isRTL = ([self userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft);
    _isFlipped = [[self controlView] isFlipped];
    _defaultTickMarkColor = [NSColor grayColor];
}

/*
 * Adapted from GNUstep NSSliderCell
 * - (NSRect)knobRectFlipped:(BOOL)flipped
 *
 * Calculates the knobRect for a given position
 * This is later used to draw the default tick mark in the center of
 * where the knob would be, when it is at the default value.
 */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpartial-availability"
- (NSRect)knobRectFlipped:(BOOL)flipped forValue:(double)doubleValue
{
    NSRect superRect = [super knobRectFlipped:flipped];
    NSPoint	origin = _trackRect.origin;
    NSSize size = superRect.size;

    if ([self isVertical] && flipped) {
        doubleValue = _maxValue + _minValue - doubleValue;
    }

    doubleValue = (doubleValue - _minValue) / (_maxValue - _minValue);

    if ([self isVertical] == YES) {
        origin.x = superRect.origin.x;
        origin.y += (_trackRect.size.height - size.height) * doubleValue;
    } else {
        origin.x += ((_trackRect.size.width - size.width) * doubleValue);
        origin.y = superRect.origin.y;
    }

    return NSMakeRect(origin.x, origin.y, size.width, size.height);
}

#pragma mark -
#pragma mark Overwritten super methods

- (NSRect)knobRectFlipped:(BOOL)flipped
{
    return [self knobRectFlipped:flipped forValue:[self doubleValue]];
}

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
            tickFrame.origin.x = mid - tickThickness/2.0;
            tickFrame.origin.y = cellFrame.origin.y;
            tickFrame.size.width = tickThickness;
            tickFrame.size.height = cellFrame.size.height;
        }

        // Draw default tick mark
        [self drawDefaultTickMarkWithFrame:tickFrame];
    }

    // Redraw knob
    [super drawKnob];
}
#pragma clang diagnostic pop

- (BOOL)continueTracking:(NSPoint)lastPoint at:(NSPoint)currentPoint inView:(NSView *)controlView
{
    double oldValue = [self doubleValue];
    BOOL result = [super continueTracking:lastPoint at:currentPoint inView:controlView];
    double newValue = [self doubleValue];

    // If no change, nothing to do.
    if (newValue == oldValue)
        return result;

    // Determine in which direction slider is moving
    BOOL sliderMovingForward = (oldValue > newValue) ? NO : YES;

    // Calculate snap-threshhold
    double range = self.maxValue - self.minValue;
    double thresh = (range * 0.01) * 7;

    // Snap to default value
    if (ABS(newValue - _defaultValue) < thresh && _snapsToDefault) {
        if (sliderMovingForward && newValue > _defaultValue) {
            [self setDoubleValue:_defaultValue];
        } else if (!sliderMovingForward && newValue < _defaultValue) {
            [self setDoubleValue:_defaultValue];
        }
    }

    return result;
}

@end
