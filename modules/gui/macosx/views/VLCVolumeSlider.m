/*****************************************************************************
 * VLCVolumeSlider.m
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

#import "VLCVolumeSlider.h"

#import "extensions/NSView+VLCAdditions.h"
#import "views/VLCVolumeSliderCell.h"

@implementation VLCVolumeSlider

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];

    if (self) {
        NSAssert([self.cell isKindOfClass:[VLCVolumeSliderCell class]],
                 @"VLCVolumeSlider cell is not VLCVolumeSliderCell");
        if (@available(macOS 10.14, *)) {
            [self viewDidChangeEffectiveAppearance];
        } else {
            [(VLCVolumeSliderCell*)self.cell setSliderStyleLight];
        }
    }
    return self;
}

+ (Class)cellClass
{
    return [VLCVolumeSliderCell class];
}

// Workaround for 10.7
// http://stackoverflow.com/questions/3985816/custom-nsslidercell
- (void)setNeedsDisplayInRect:(NSRect)invalidRect {
    [super setNeedsDisplayInRect:[self bounds]];
}

- (void)viewDidChangeEffectiveAppearance
{
    if (self.shouldShowDarkAppearance) {
        [(VLCVolumeSliderCell*)self.cell setSliderStyleDark];
    } else {
        [(VLCVolumeSliderCell*)self.cell setSliderStyleLight];
    }
}

@end
