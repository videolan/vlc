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

#import "extensions/NSAnimationContext+VLCAdditions.h"

@interface VLCTrackingView ()
{
    NSTrackingArea *_trackingArea;
    BOOL _mouseIn;
}
@end

@implementation VLCTrackingView

- (instancetype)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        [self setupProperties];
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];
    if (self) {
        [self setupProperties];
    }
    return self;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setupProperties];
    }
    return self;
}

- (void)setupProperties
{
    _mouseIn = NO;
    _animatesTransition = YES;
    _enabled = YES;
}

- (void)setViewToHide:(nullable NSView *)view
{
    self.viewsToHide = view ? @[view] : nil;
}

- (void)setViewToShow:(nullable NSView *)view
{
    self.viewsToShow = view ? @[view] : nil;
}

- (void)performTransition
{
    if (self.animatesTransition) {
        const BOOL hideVTH = !_mouseIn;
        const BOOL hideVTS = _mouseIn;
        const BOOL startMouseIn = _mouseIn;

        __weak typeof(self) weakSelf = self;

        for (NSView * const view in self.viewsToHide) {
            view.hidden = NO;
        }
        for (NSView * const view in self.viewsToShow) {
            view.hidden = NO;
        }

        [NSAnimationContext runAnimationRespectingPreferencesWithDuration:0.3
                                                                  changes:^(NSAnimationContext * const context) {
            for (NSView * const view in weakSelf.viewsToHide) {
                view.animator.alphaValue = hideVTH ? 0.0 : 1.0;
            }
            for (NSView * const view in weakSelf.viewsToShow) {
                view.animator.alphaValue = hideVTS ? 0.0 : 1.0;
            }
        } completionHandler:^{
            __strong typeof(weakSelf) strongSelf = weakSelf;
            if (!strongSelf || startMouseIn != strongSelf->_mouseIn) {
                return;
            }
            for (NSView * const view in strongSelf.viewsToHide) {
                view.hidden = hideVTH;
            }
            for (NSView * const view in strongSelf.viewsToShow) {
                view.hidden = hideVTS;
            }
        }];
    } else {
        for (NSView * const view in self.viewsToHide) {
            view.hidden = !_mouseIn;
        }
        for (NSView * const view in self.viewsToShow) {
            view.hidden = _mouseIn;
        }
    }
}

- (void)handleMouseEnter
{
    if (!self.enabled) {
        return;
    }

    _mouseIn = YES;
    [self performTransition];
    if (self.mouseEnteredBlock) {
        self.mouseEnteredBlock();
    }
}

- (void)handleMouseExit
{
    if (!self.enabled) {
        return;
    }

    _mouseIn = NO;
    [self performTransition];
    if (self.mouseExitedBlock) {
        self.mouseExitedBlock();
    }
}

- (void)mouseEntered:(NSEvent *)event
{
    [self handleMouseEnter];
}

- (void)mouseExited:(NSEvent *)event
{
    [self handleMouseExit];
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
