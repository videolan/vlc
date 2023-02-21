/*****************************************************************************
 * VLCMainVideoViewController.m: MacOS X interface module
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

#import "VLCMainVideoViewController.h"

#import "library/VLCLibraryWindow.h"

#import "main/VLCMain.h"

#import "views/VLCBottomBarView.h"

#import "windows/video/VLCVideoWindowCommon.h"

#import <vlc_common.h>

@interface VLCMainVideoViewController()
{
    NSTimer *_hideControlsTimer;
}
@end

@implementation VLCMainVideoViewController

- (instancetype)init
{
    self = [super initWithNibName:@"VLCMainVideoView" bundle:nil];
    return self;
}

- (void)viewDidLoad
{
    _autohideControls = YES;
    _controlsBar.bottomBarView.blendingMode = NSVisualEffectBlendingModeWithinWindow;

    [self setDisplayLibraryControls:[self.view.window class] == [VLCLibraryWindow class]];

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(shouldShowControls:)
                               name:VLCVideoWindowShouldShowFullscreenController
                             object:nil];
}

- (void)stopAutohideTimer
{
    [_hideControlsTimer invalidate];
}

- (void)startAutohideTimer
{
    /* Do nothing if timer is already in place */
    if (_hideControlsTimer.valid) {
        return;
    }

    /* Get timeout and make sure it is not lower than 1 second */
    long long timeToKeepVisibleInSec = MAX(var_CreateGetInteger(getIntf(), "mouse-hide-timeout") / 1000, 1);

    _hideControlsTimer = [NSTimer scheduledTimerWithTimeInterval:timeToKeepVisibleInSec
                                                          target:self
                                                        selector:@selector(hideControls:)
                                                        userInfo:nil
                                                         repeats:NO];
}

- (void)hideControls:(id)sender
{
    [self stopAutohideTimer];
    _mainControlsView.hidden = YES;
}

- (void)setAutohideControls:(BOOL)autohide
{
    if (_autohideControls == autohide) {
        return;
    }

    _autohideControls = autohide;

    if (autohide) {
        [self startAutohideTimer];
    } else {
        [self showControls];
    }
}

- (void)shouldShowControls:(NSNotification *)aNotification
{
    [self showControls];
}

- (void)showControls
{
    [self stopAutohideTimer];

    if (!_mainControlsView.hidden && !_autohideControls) {
        return;
    }

    _mainControlsView.hidden = NO;

    if (_autohideControls) {
        [self startAutohideTimer];
    }
}

- (void)setDisplayLibraryControls:(BOOL)displayLibraryControls
{
    if (_displayLibraryControls == displayLibraryControls) {
        return;
    }
    
    _displayLibraryControls = displayLibraryControls;

    _returnButton.hidden = !displayLibraryControls;
    _playlistButton.hidden = !displayLibraryControls;
}

@end
