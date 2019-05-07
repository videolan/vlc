/*****************************************************************************
 * VLCSlider.m
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

#import "VLCSlider.h"

#import "extensions/NSView+VLCAdditions.h"
#import "views/VLCSliderCell.h"

@implementation VLCSlider

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];

    if (self) {
        NSAssert([self.cell isKindOfClass:[VLCSliderCell class]],
                 @"VLCSlider cell is not VLCSliderCell");
        _isScrollable = YES;
        if (@available(macOS 10.14, *)) {
            [self viewDidChangeEffectiveAppearance];
        } else {
            [self setSliderStyleLight];
        }
    }
    return self;
}

+ (Class)cellClass
{
    return [VLCSliderCell class];
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

// Workaround for 10.7
// http://stackoverflow.com/questions/3985816/custom-nsslidercell
- (void)setNeedsDisplayInRect:(NSRect)invalidRect {
    [super setNeedsDisplayInRect:[self bounds]];
}

- (BOOL)getIndefinite
{
    return [(VLCSliderCell*)[self cell] indefinite];
}

- (void)setIndefinite:(BOOL)indefinite
{
    [(VLCSliderCell*)[self cell] setIndefinite:indefinite];
}

- (BOOL)getKnobHidden
{
    return [(VLCSliderCell*)[self cell] isKnobHidden];
}

- (void)setKnobHidden:(BOOL)isKnobHidden
{
    [(VLCSliderCell*)[self cell] setKnobHidden:isKnobHidden];
}

- (BOOL)isFlipped
{
    return NO;
}

- (void)setSliderStyleLight
{
    [(VLCSliderCell*)[self cell] setSliderStyleLight];
}

- (void)setSliderStyleDark
{
    [(VLCSliderCell*)[self cell] setSliderStyleDark];
}

- (void)viewDidChangeEffectiveAppearance
{
    if (self.shouldShowDarkAppearance) {
        [self setSliderStyleDark];
    } else {
        [self setSliderStyleLight];
    }
}

@end
