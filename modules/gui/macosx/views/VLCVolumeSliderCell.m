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
#import "extensions/NSColor+VLCAdditions.h"
#import "main/CompatibilityFixes.h"

@interface VLCVolumeSliderCell () {
    BOOL _isRTL;
    NSColor *_emptySliderBackgroundColor;
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
    _emptySliderBackgroundColor = [NSColor colorWithCalibratedWhite:0.5 alpha:0.5];
}

- (void)setSliderStyleDark
{
    _emptySliderBackgroundColor = [NSColor colorWithCalibratedWhite:1 alpha:0.2];
}

- (void)drawBarInside:(NSRect)rect flipped:(BOOL)flipped
{
    const CGFloat trackBorderRadius = 1;

    // Empty Track Drawing
    NSBezierPath* emptyTrackPath = [NSBezierPath bezierPathWithRoundedRect:rect
                                                                   xRadius:trackBorderRadius
                                                                   yRadius:trackBorderRadius];

    // Calculate filled track
    NSRect filledTrackRect = rect;
    NSRect knobRect = [self knobRectFlipped:NO];
    CGFloat sliderCenter = knobRect.origin.x  + (self.knobThickness / 2);

    if (_isRTL) {
        filledTrackRect.size.width = rect.origin.x + rect.size.width - sliderCenter;
        filledTrackRect.origin.x = sliderCenter;
    } else {
        filledTrackRect.size.width = sliderCenter;
    }

    NSBezierPath* filledTrackPath = [NSBezierPath bezierPathWithRoundedRect:filledTrackRect
                                                                    xRadius:trackBorderRadius
                                                                    yRadius:trackBorderRadius];
    NSColor *filledColor = [NSColor VLCAccentColor];

    [_emptySliderBackgroundColor setFill];
    [emptyTrackPath fill];
    [filledColor setFill];
    [filledTrackPath fill];
}

@end
