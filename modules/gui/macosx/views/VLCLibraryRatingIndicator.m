/*****************************************************************************
 * VLCLibraryRatingIndicator.m
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * Authors: Bob Moriasi <official.bobmoriasi@gmail.com>
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

#import "VLCLibraryRatingIndicator.h"

#import "vlc_common.h"

@implementation VLCLibraryRatingIndicator

- (void)awakeFromNib
{
    [super awakeFromNib];
    [self updateTrackingAreas];

    self.originalValue = self.doubleValue;
}

- (void)mouseEntered:(NSEvent *)event
{
    [super mouseEntered:event];

    self.isHovering = YES;
    self.originalValue = self.doubleValue;
}

- (void)mouseExited:(NSEvent *)event
{
    [super mouseExited:event];

    self.isHovering = NO;
    self.doubleValue = self.originalValue;
}

- (void)mouseMoved:(NSEvent *)event
{
    [super mouseMoved:event];

    if (self.isHovering) {
        NSPoint locationInView = [self convertPoint:event.locationInWindow
                                           fromView:nil];

        CGFloat hoverValue =
            (locationInView.x / self.bounds.size.width) * self.maxValue;
        hoverValue = ceil(hoverValue);
        hoverValue = VLC_CLIP(hoverValue, 0, self.maxValue);

        self.doubleValue = hoverValue;
    }
}

- (void)mouseUp:(NSEvent *)event
{
    [super mouseUp:event];

    if (self.isHovering) {
        self.originalValue = self.doubleValue;
    }
}

- (void)setDoubleValue:(double)doubleValue
{
    [super setDoubleValue:doubleValue];

    if (!self.isHovering) {
        self.originalValue = self.doubleValue;
    }
}

- (void)updateTrackingAreas
{
    if (self.trackingArea) {
        [self removeTrackingArea:self.trackingArea];
    }

    self.trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:NSTrackingActiveInKeyWindow |
                     NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved
               owner:self
            userInfo:nil];
    [self addTrackingArea:self.trackingArea];
}

@end
