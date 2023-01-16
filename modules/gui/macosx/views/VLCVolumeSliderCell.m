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
}
@end

@implementation VLCVolumeSliderCell

- (instancetype)init
{
    self = [super init];
    if (self) {
        _isRTL = ([self userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft);
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        _isRTL = ([self userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft);
    }
    return self;
}

- (void)drawBarInside:(NSRect)rect flipped:(BOOL)flipped
{
    // Empty Track Drawing
    NSBezierPath* emptyTrackPath = [NSBezierPath bezierPathWithRoundedRect:rect xRadius:1 yRadius:1];

    NSColor *emptyColor = [NSColor lightGrayColor];
    if (@available(macOS 10.14, *)) {
        emptyColor = [NSColor unemphasizedSelectedContentBackgroundColor];
    }

    // Calculate filled track
    NSRect leadingTrackRect = rect;
    NSRect knobRect = [self knobRectFlipped:NO];
    CGFloat sliderCenter = knobRect.origin.x  + (self.knobThickness / 2);

    leadingTrackRect.size.width = sliderCenter;
    if (_isRTL) {
        leadingTrackRect.origin.x = sliderCenter;
    }

    CGFloat leadingTrackCornerRadius = 2;
    NSBezierPath* leadingTrackPath = [NSBezierPath bezierPathWithRoundedRect:leadingTrackRect
                                                                     xRadius:leadingTrackCornerRadius
                                                                     yRadius:leadingTrackCornerRadius];
    NSColor *filledColor = [NSColor VLCAccentColor];

    [emptyColor setFill];
    [emptyTrackPath fill];
    [filledColor setFill];
    [leadingTrackPath fill];
}

@end
