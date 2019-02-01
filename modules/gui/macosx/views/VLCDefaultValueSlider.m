/*****************************************************************************
 * VLCDefaultValueSlider.m: Custom NSSlider which allows a defaultValue
 *****************************************************************************
 * Copyright (C) 2016 VLC authors and VideoLAN
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

#import "VLCDefaultValueSlider.h"

#import "views/VLCDefaultValueSliderCell.h"

@implementation VLCDefaultValueSlider

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        NSAssert([self.cell isKindOfClass:[VLCDefaultValueSliderCell class]],
                 @"VLCDefaultSlider cell is not VLCDefaultValueSliderCell");
        _isScrollable = YES;
    }
    return self;
}

+ (Class)cellClass
{
    return [VLCDefaultValueSliderCell class];
}

- (void)scrollWheel:(NSEvent *)event
{
    if (!_isScrollable)
        return [super scrollWheel:event];
    double increment;
    CGFloat deltaY = [event scrollingDeltaY];
    double range = [self maxValue] - [self minValue];

    // Scroll less for high precision, else it's too fast
    if (event.hasPreciseScrollingDeltas) {
        increment = (range * 0.002) * deltaY;
    } else {
        if (deltaY == 0.0)
            return;
        increment = (range * 0.01 * deltaY);
    }

    // If scrolling is inversed, increment in other direction
    if (!event.isDirectionInvertedFromDevice)
        increment = -increment;

    [self setDoubleValue:self.doubleValue - increment];
    [self sendAction:self.action to:self.target];
}

- (void)setDefaultValue:(double)value
{
    [(VLCDefaultValueSliderCell *)self.cell setDefaultValue:value];
}

- (double)defaultValue
{
    return [(VLCDefaultValueSliderCell *)self.cell defaultValue];
}

@end
