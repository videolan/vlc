/*****************************************************************************
 * VLCFullVideoViewWindow.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCFullVideoViewWindow.h"

#import "main/VLCMain.h"

@interface VLCFullVideoViewWindow ()
{
    BOOL _autohideTitlebar;
    NSTimer *_hideTitlebarTimer;
}
@end

@implementation VLCFullVideoViewWindow

- (void)setup
{
    [super setup];
    _autohideTitlebar = NO;

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(shouldShowFullscreenController:)
                               name:VLCVideoWindowShouldShowFullscreenController
                             object:nil];

    self.titleVisibility = NSWindowTitleHidden;
}

- (void)stopTitlebarAutohideTimer
{
    [_hideTitlebarTimer invalidate];
}

- (void)startTitlebarAutohideTimer
{
    /* Do nothing if timer is already in place */
    if (_hideTitlebarTimer.valid) {
        return;
    }

    /* Get timeout and make sure it is not lower than 1 second */
    long long timeToKeepVisibleInSec = MAX(var_CreateGetInteger(getIntf(), "mouse-hide-timeout") / 1000, 1);

    _hideTitlebarTimer = [NSTimer scheduledTimerWithTimeInterval:timeToKeepVisibleInSec
                                                          target:self
                                                        selector:@selector(hideTitleBar:)
                                                        userInfo:nil
                                                         repeats:NO];
}

- (void)showTitleBar
{
    [self stopTitlebarAutohideTimer];

    NSView *titlebarView =  [self standardWindowButton:NSWindowCloseButton].superview;
    if (!titlebarView.hidden && !_autohideTitlebar) {
        return;
    }

    titlebarView.hidden = NO;

    if (_autohideTitlebar) {
        [self startTitlebarAutohideTimer];
    }
}

- (void)hideTitleBar:(id)sender
{
    [self stopTitlebarAutohideTimer];
    [self standardWindowButton:NSWindowCloseButton].superview.hidden = YES;
}

- (void)enableVideoTitleBarMode
{
    self.toolbar.visible = NO;
    self.styleMask |= NSWindowStyleMaskFullSizeContentView;
    self.titlebarAppearsTransparent = YES;

    _autohideTitlebar = YES;
    [self showTitleBar];
}

- (void)disableVideoTitleBarMode
{
    self.toolbar.visible = YES;
    self.styleMask &= ~NSWindowStyleMaskFullSizeContentView;
    self.titlebarAppearsTransparent = NO;

    _autohideTitlebar = NO;
    [self showTitleBar];
}

- (void)shouldShowFullscreenController:(NSNotification *)aNotification
{
    [self showTitleBar];
}

@end
