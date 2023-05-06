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
    BOOL _mouseIn;
}
@end

@implementation VLCTrackingView

- (void)handleMouseExit
{
    _mouseIn = NO;

    if (self.animatesTransition) {
        [self.viewToHide setAlphaValue:1.0];
        [self.viewToShow setAlphaValue:.0];
        [self.viewToShow setHidden:NO];

        __weak typeof(self.viewToHide) weakViewToHide = self.viewToHide;
        __weak typeof(self.viewToShow) weakViewToShow = self.viewToShow;

        [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context){
            [[NSAnimationContext currentContext] setDuration:0.9];
            [[weakViewToHide animator] setAlphaValue:0.0];
            [[weakViewToShow animator] setAlphaValue:1.0];
        } completionHandler:^{
            [weakViewToHide setHidden:YES];
        }];
    } else {
        self.viewToHide.hidden = YES;
        self.viewToShow.hidden = NO;
    }
}

- (void)handleMouseEnter
{
    _mouseIn = YES;

    if (self.animatesTransition) {
        [self.viewToHide setAlphaValue:.0];
        [self.viewToHide setHidden:NO];

        __weak typeof(self.viewToHide) weakViewToHide = self.viewToHide;
        __weak typeof(self.viewToShow) weakViewToShow = self.viewToShow;

        [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context){
            [[NSAnimationContext currentContext] setDuration:0.9];
            [[weakViewToHide animator] setAlphaValue:1.0];
            [[weakViewToShow animator] setAlphaValue:.0];
        } completionHandler:^{
            [weakViewToShow setHidden:YES];
        }];
    } else {
        self.viewToHide.hidden = NO;
        self.viewToShow.hidden = YES;
    }
}

- (void)mouseExited:(NSEvent *)event
{
    [self handleMouseExit];
}

- (void)mouseEntered:(NSEvent *)event
{
    [self handleMouseEnter];
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

    // Once tracking area updated, check if the cursor is still inside the tracking view.
    // This prevents situations where the mouseEntered/mouseExited is not called because the view
    // itself has moved but the cursor has not (e.g. when this view is inside a scrollview and the
    // user scrolls)
    const NSPoint mouseLocation = [self convertPoint:self.window.mouseLocationOutsideOfEventStream fromView:self.window.contentView];
    const BOOL mouseInsideView = [self mouse:mouseLocation inRect:self.frame];
    if (mouseInsideView && !_mouseIn) {
        [self handleMouseEnter];
    } else if (!mouseInsideView && _mouseIn) {
        [self handleMouseExit];
    }
}

@end
