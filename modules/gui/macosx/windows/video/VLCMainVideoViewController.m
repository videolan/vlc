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
#import "library/VLCLibraryUIUnits.h"

#import "main/VLCMain.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

#import "views/VLCBottomBarView.h"

#import "windows/video/VLCVideoWindowCommon.h"

#import <vlc_common.h>

@interface VLCMainVideoViewController()
{
    NSTimer *_hideControlsTimer;
    NSLayoutConstraint *_returnButtonBottomConstraint;
    NSLayoutConstraint *_playlistButtonBottomConstraint;
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

    [self setDisplayLibraryControls:NO];
    [self updatePlaylistToggleState];
    [self updateLibraryControls];

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(shouldShowControls:)
                               name:VLCVideoWindowShouldShowFullscreenController
                             object:nil];

    _returnButtonBottomConstraint = [NSLayoutConstraint constraintWithItem:_returnButton
                                                                 attribute:NSLayoutAttributeBottom
                                                                 relatedBy:NSLayoutRelationEqual
                                                                    toItem:_fakeTitleBar
                                                                 attribute:NSLayoutAttributeBottom
                                                                multiplier:1.
                                                                  constant:0];
    _playlistButtonBottomConstraint = [NSLayoutConstraint constraintWithItem:_playlistButton
                                                                   attribute:NSLayoutAttributeBottom
                                                                   relatedBy:NSLayoutRelationEqual
                                                                      toItem:_fakeTitleBar
                                                                   attribute:NSLayoutAttributeBottom
                                                                  multiplier:1.
                                                                    constant:0];

    _returnButtonBottomConstraint.active = NO;
    _playlistButtonBottomConstraint.active = NO;
}

- (BOOL)mouseOnControls
{
    NSPoint mousePos = [self.view.window mouseLocationOutsideOfEventStream];

    return [_centralControlsStackView mouse:mousePos inRect:_centralControlsStackView.frame] ||
        [_controlsBar.bottomBarView mouse:mousePos inRect: _controlsBar.bottomBarView.frame] ||
        [_returnButton mouse:mousePos inRect: _returnButton.frame] ||
        [_playlistButton mouse:mousePos inRect: _playlistButton.frame];
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

    NSPoint mousePos = [self.view.window mouseLocationOutsideOfEventStream];

    if ([self mouseOnControls] ||
        VLCMain.sharedInstance.playlistController.playerController.playerState == VLC_PLAYER_STATE_PAUSED) {
        [self showControls];
        return;
    }

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
    [self updatePlaylistToggleState];
    [self updateLibraryControls];

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
    _displayLibraryControls = displayLibraryControls;

    _returnButton.hidden = !displayLibraryControls;
    _playlistButton.hidden = !displayLibraryControls;
}

- (void)updatePlaylistToggleState
{
    VLCLibraryWindow *libraryWindow = (VLCLibraryWindow*)self.view.window;
    if (libraryWindow != nil && _displayLibraryControls) {
        _playlistButton.state = [libraryWindow.mainSplitView isSubviewCollapsed:libraryWindow.playlistView] ?
            NSControlStateValueOff : NSControlStateValueOn;
    }
}

- (NSRect)windowButtonsRect
{
    NSWindow * const window = self.view.window;
    NSRect buttonBox = NSZeroRect;

    NSButton * const closeButton = [window standardWindowButton:NSWindowCloseButton];
    if (closeButton) {
        buttonBox = NSUnionRect(buttonBox, closeButton.frame);
    }

    NSButton * const minimizeButton = [window standardWindowButton:NSWindowMiniaturizeButton];
    if (minimizeButton) {
        buttonBox = NSUnionRect(buttonBox, minimizeButton.frame);
    }

    NSButton * const zoomButton = [window standardWindowButton:NSWindowZoomButton];
    if (zoomButton) {
        buttonBox = NSUnionRect(buttonBox, zoomButton.frame);
    }

    return buttonBox;
}

- (void)updateLibraryControls
{
    if (!_displayLibraryControls) {
        return;
    }

    const NSWindow * const viewWindow = self.view.window;
    const NSView * const titlebarView = [viewWindow standardWindowButton:NSWindowCloseButton].superview;
    const CGFloat windowTitlebarHeight = titlebarView.frame.size.height;

    const BOOL windowFullscreen = [(VLCWindow*)viewWindow isInNativeFullscreen] ||
                                  [(VLCWindow*)viewWindow fullscreen];
    const BOOL placeInFakeToolbar = viewWindow.titlebarAppearsTransparent &&
                                    !windowFullscreen;

    const CGFloat buttonTopSpace = placeInFakeToolbar ? 0 : [VLCLibraryUIUnits largeSpacing];

    _fakeTitleBarHeightConstraint.constant = windowFullscreen ? 0 : windowTitlebarHeight;

    _returnButtonTopConstraint.constant = buttonTopSpace;
    _playlistButtonTopConstraint.constant = buttonTopSpace;
    _returnButtonBottomConstraint.active = placeInFakeToolbar;
    _playlistButtonBottomConstraint.active = placeInFakeToolbar;

    const NSRect windowButtonBox = [self windowButtonsRect];

    _returnButtonLeadingConstraint.constant = placeInFakeToolbar ? windowButtonBox.size.width + [VLCLibraryUIUnits mediumSpacing] : [VLCLibraryUIUnits largeSpacing];
    _playlistButtonTrailingConstraint.constant = placeInFakeToolbar ? 0. : [VLCLibraryUIUnits largeSpacing];
}

- (IBAction)togglePlaylist:(id)sender
{
    VLCLibraryWindow *libraryWindow = (VLCLibraryWindow*)self.view.window;
    if (libraryWindow != nil) {
        [libraryWindow togglePlaylist];
    }
}

- (IBAction)returnToLibrary:(id)sender
{
    VLCLibraryWindow *libraryWindow = (VLCLibraryWindow*)self.view.window;
    if (libraryWindow != nil) {
        [libraryWindow disableVideoPlaybackAppearance];
    }
}

@end
