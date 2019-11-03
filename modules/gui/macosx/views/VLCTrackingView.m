/*****************************************************************************
 * VLCTrackingView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCTrackingView.h"

@interface VLCTrackingView ()
{
    NSTrackingArea *_trackingArea;
}
@end

@implementation VLCTrackingView

- (void)mouseExited:(NSEvent *)event
{
    if (self.animatesTransition) {
        [self.viewToHide setAlphaValue:1.0];
        __weak typeof(self.viewToHide) weakViewToHide = self.viewToHide;
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context){
            [[NSAnimationContext currentContext] setDuration:0.9];
            [[weakViewToHide animator] setAlphaValue:0.0];
        } completionHandler:^{
            [weakViewToHide setHidden:YES];
        }];
    } else {
        self.viewToHide.hidden = YES;
    }
}

- (void)mouseEntered:(NSEvent *)event
{
    if (self.animatesTransition) {
        [self.viewToHide setAlphaValue:.0];
        [self.viewToHide setHidden:NO];
        __weak typeof(self.viewToHide) weakViewToHide = self.viewToHide;
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context){
            [[NSAnimationContext currentContext] setDuration:0.9];
            [[weakViewToHide animator] setAlphaValue:1.0];
        } completionHandler:nil];
    } else {
        self.viewToHide.hidden = NO;
    }
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    if(_trackingArea != nil) {
        [self removeTrackingArea:_trackingArea];
    }

    NSTrackingAreaOptions trackingAreaOptions = (NSTrackingMouseEnteredAndExited | NSTrackingActiveAlways);
    _trackingArea = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                 options:trackingAreaOptions
                                                   owner:self
                                                userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

@end
