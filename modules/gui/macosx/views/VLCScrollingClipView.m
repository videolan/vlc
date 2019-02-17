/*****************************************************************************
 * VLCScrollingClipView.m: NSClipView subclass that automatically scrolls
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
 *
 * Author: Marvin Scholz <epirat07@gmail.com>
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

#import "VLCScrollingClipView.h"

@implementation VLCScrollingClipView {
    NSTimer *scrollTimer;
    NSTimeInterval startInterval;
}

- (void)startScrolling {
    // Start our timer unless running
    if (!scrollTimer) {
        scrollTimer = [NSTimer scheduledTimerWithTimeInterval:0.025
                                                       target:self
                                                     selector:@selector(scrollTick:)
                                                     userInfo:nil
                                                      repeats:YES];
    }
}

- (void)stopScrolling {
    // Stop timer unless stopped
    if (scrollTimer) {
        [scrollTimer invalidate];
        scrollTimer = nil;
    }
}

- (void)resetScrolling {
    startInterval = 0;
    [self scrollToPoint:NSMakePoint(0.0, 0.0)];
    if (_parentScrollView) {
        [_parentScrollView reflectScrolledClipView:self];
    }
}

- (void)scrollWheel:(NSEvent *)theEvent {
    // Stop auto-scrolling if the user scrolls
    [self stopScrolling];
    [super scrollWheel:theEvent];
}

- (void)scrollTick:(NSTimer *)timer {
    CGFloat maxHeight = self.documentRect.size.height;
    CGFloat scrollPosition = self.documentVisibleRect.origin.y;

    // Delay start
    if (scrollPosition == 0.0 && !startInterval) {
        startInterval = [NSDate timeIntervalSinceReferenceDate] + 2.0;
    }

    if ([NSDate timeIntervalSinceReferenceDate] >= startInterval) {
        // Reset if we are past the end
        if (scrollPosition > maxHeight) {
            [self resetScrolling];
            return;
        }

        // Scroll the view a bit on each tick
        [self scrollToPoint:NSMakePoint(0.0, scrollPosition + 0.5)];

        // Update the parent ScrollView
        if (_parentScrollView) {
            [_parentScrollView reflectScrolledClipView:self];
        }
    }
}

@end
