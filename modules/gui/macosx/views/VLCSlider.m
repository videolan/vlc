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

+ (Class)cellClass
{
    return VLCSliderCell.class;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];

    if (self) {
        NSAssert([self.cell isKindOfClass:[VLCSlider cellClass]], 
                 @"VLCSlider cell is not VLCSliderCell");
        self.scrollable = YES;
        if (@available(macOS 10.14, *)) {
            [self viewDidChangeEffectiveAppearance];
        } else {
            [(VLCSliderCell*)self.cell setSliderStyleLight];
        }

    }
    return self;
}

- (void)scrollWheel:(NSEvent *)event
{
    if (!self.scrollable) {
        return [super scrollWheel:event];
    }

    double increment;
    CGFloat deltaY = [event scrollingDeltaY];
    double range = [self maxValue] - [self minValue];

    // Scroll less for high precision, else it's too fast
    if (event.hasPreciseScrollingDeltas) {
        increment = (range * 0.002) * deltaY;
    } else {
        if (deltaY == 0.0) {
            return;
        }
        increment = (range * 0.01 * deltaY);
    }

    // If scrolling is inversed, increment in other direction
    if (!event.isDirectionInvertedFromDevice) {
        increment = -increment;
    }

    self.doubleValue = self.doubleValue - increment;
    [self sendAction:self.action to:self.target];
}

// Workaround for 10.7
// http://stackoverflow.com/questions/3985816/custom-nsslidercell
- (void)setNeedsDisplayInRect:(NSRect)invalidRect
{
    [super setNeedsDisplayInRect:self.bounds];
}

- (BOOL)indefinite
{
    return [(VLCSliderCell*)self.cell indefinite];
}

- (void)setIndefinite:(BOOL)indefinite
{
    [(VLCSliderCell*)self.cell setIndefinite:indefinite];
}

- (BOOL)knobHidden
{
    return [(VLCSliderCell*)self.cell knobHidden];
}

- (void)setKnobHidden:(BOOL)knobHidden
{
    [(VLCSliderCell*)self.cell setKnobHidden:knobHidden];
}

- (BOOL)isFlipped
{
    return NO;
}

- (void)viewDidChangeEffectiveAppearance
{
    if (self.shouldShowDarkAppearance) {
        [(VLCSliderCell*)self.cell setSliderStyleDark];
    } else {
        [(VLCSliderCell*)self.cell setSliderStyleLight];
    }
}

@end
