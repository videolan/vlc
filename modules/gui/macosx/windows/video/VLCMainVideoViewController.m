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

#import "extensions/NSWindow+VLCAdditions.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPlaylistSidebarViewController.h"
#import "library/VLCLibraryWindowSplitViewController.h"

#import "main/VLCMain.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"

#import "views/VLCBottomBarView.h"

#import "windows/controlsbar/VLCMainVideoViewControlsBar.h"

#import "windows/video/VLCMainVideoViewAudioMediaDecorativeView.h"
#import "windows/video/VLCMainVideoViewOverlayView.h"
#import "windows/video/VLCVideoWindowCommon.h"

#import <vlc_common.h>

@interface VLCMainVideoViewController()
{
    NSTimer *_hideControlsTimer;
    NSLayoutConstraint *_returnButtonBottomConstraint;
    NSLayoutConstraint *_playlistButtonBottomConstraint;

    BOOL _isFadingIn;
}
@end

@implementation VLCMainVideoViewController

- (instancetype)init
{
    self = [super initWithNibName:@"VLCMainVideoView" bundle:nil];
    if (self) {
        _isFadingIn = NO;

        NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(playerCurrentMediaItemChanged:)
                                   name:VLCPlayerCurrentMediaItemChanged
                                 object:nil];
    }
    return self;
}

- (void)setupAudioDecorativeView
{
    _audioDecorativeView = [VLCMainVideoViewAudioMediaDecorativeView fromNibWithOwner:self];
    _audioDecorativeView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addConstraints:@[
        [NSLayoutConstraint constraintWithItem:_audioDecorativeView
                                     attribute:NSLayoutAttributeTop
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self.view
                                     attribute:NSLayoutAttributeTop
                                    multiplier:1.
                                      constant:0.
        ],
        [NSLayoutConstraint constraintWithItem:_audioDecorativeView
                                     attribute:NSLayoutAttributeBottom
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self.view
                                     attribute:NSLayoutAttributeBottom
                                    multiplier:1.
                                      constant:0.
        ],
        [NSLayoutConstraint constraintWithItem:_audioDecorativeView
                                     attribute:NSLayoutAttributeLeft
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self.view
                                     attribute:NSLayoutAttributeLeft
                                    multiplier:1.
                                      constant:0.
        ],
        [NSLayoutConstraint constraintWithItem:_audioDecorativeView
                                     attribute:NSLayoutAttributeRight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self.view
                                     attribute:NSLayoutAttributeRight
                                    multiplier:1.
                                      constant:0.
        ],
    ]];

    [self.view addSubview:_audioDecorativeView positioned:NSWindowAbove relativeTo:_voutView];
    _audioDecorativeView.hidden = YES;
}

- (void)viewDidLoad
{
    _autohideControls = YES;

    [self setDisplayLibraryControls:NO];
    [self updatePlaylistToggleState];
    [self updateLibraryControls];

    NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
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

    [self setupAudioDecorativeView];
    [self.controlsBar update];
}

- (void)playerCurrentMediaItemChanged:(NSNotification *)notification
{
    NSParameterAssert(notification);
    VLCPlayerController * const controller = notification.object;
    NSAssert(controller != nil, @"Player current media item changed notification should carry a valid player controller");

    VLCMediaLibraryMediaItem * const mediaItem = [VLCMediaLibraryMediaItem mediaItemForURL:controller.URLOfCurrentMediaItem];

    const BOOL decorativeViewVisible = mediaItem != nil && mediaItem.mediaType == VLC_ML_MEDIA_TYPE_AUDIO;
    _audioDecorativeView.hidden = !decorativeViewVisible;

    if (decorativeViewVisible) {
        [self setAutohideControls:NO];
    } else {
        [self setAutohideControls:YES];
    }
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
                                                        selector:@selector(shouldHideControls:)
                                                        userInfo:nil
                                                         repeats:NO];
}

- (void)shouldHideControls:(id)sender
{
    [self hideControls];
    [NSCursor setHiddenUntilMouseMoves: YES];
}

- (void)hideControls
{
    [self stopAutohideTimer];

    NSPoint mousePos = [self.view.window mouseLocationOutsideOfEventStream];

    if ([self mouseOnControls]) {
        [self showControls];
        return;
    }

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * _Nonnull context) {
        [context setDuration:VLCLibraryUIUnits.controlsFadeAnimationDuration];
        [self->_mainControlsView.animator setAlphaValue:0.0f];
    } completionHandler:nil];
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

    if (!_autohideControls) {
        _mainControlsView.alphaValue = 1.0f;
        return;
    }

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * _Nonnull context) {
        self->_isFadingIn = YES;
        [context setDuration:VLCLibraryUIUnits.controlsFadeAnimationDuration];
        [self->_mainControlsView.animator setAlphaValue:1.0f];
    } completionHandler:^{
        self->_isFadingIn = NO;
        [self startAutohideTimer];
    }];
}

- (void)setDisplayLibraryControls:(BOOL)displayLibraryControls
{
    _displayLibraryControls = displayLibraryControls;

    _returnButton.hidden = !displayLibraryControls;
    _playlistButton.hidden = !displayLibraryControls;
}

- (void)updatePlaylistToggleState
{
    VLCLibraryWindow * const libraryWindow = (VLCLibraryWindow*)self.view.window;
    if (libraryWindow != nil && _displayLibraryControls) {
        NSView * const playlistView =
            libraryWindow.splitViewController.playlistSidebarViewController.view;
        self.playlistButton.state = [libraryWindow.mainSplitView isSubviewCollapsed:playlistView] ?
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
    const CGFloat windowTitlebarHeight = viewWindow.titlebarHeight;

    const BOOL windowFullscreen = [(VLCWindow*)viewWindow isInNativeFullscreen] ||
                                  [(VLCWindow*)viewWindow fullscreen];
    const BOOL placeInFakeToolbar = viewWindow.titlebarAppearsTransparent &&
                                    !windowFullscreen;

    const CGFloat buttonTopSpace = placeInFakeToolbar ? 0 : VLCLibraryUIUnits.largeSpacing;

    _fakeTitleBarHeightConstraint.constant = windowFullscreen ? 0 : windowTitlebarHeight;

    _returnButtonTopConstraint.constant = buttonTopSpace;
    _playlistButtonTopConstraint.constant = buttonTopSpace;
    _returnButtonBottomConstraint.active = placeInFakeToolbar;
    _playlistButtonBottomConstraint.active = placeInFakeToolbar;

    NSControlSize buttonSize = NSControlSizeRegular;

    if (@available(macOS 11.0, *)) {
        if (!placeInFakeToolbar) {
            buttonSize = NSControlSizeLarge;
        } else if (viewWindow.toolbarStyle != NSWindowToolbarStyleUnified) {
            buttonSize = NSControlSizeMini;
        }
    } else if (placeInFakeToolbar) {
        buttonSize = NSControlSizeMini;
    }

    NSControlSize previousButtonSize = _playlistButton.controlSize;
    _returnButton.controlSize = buttonSize;
    _playlistButton.controlSize = buttonSize;

    // HACK: Upon changing the control size the actual highlight of toggled/hovered buttons doesn't change
    // properly, at least for recessed buttons. This is most obvious on the toggleable playlist button.
    // So reset the state and then retoggle once done.
    if (previousButtonSize != buttonSize) {
        NSControlStateValue returnButtonControlState = _returnButton.state;
        NSControlStateValue playlistButtonControlState = _playlistButton.state;
        _returnButton.state = NSControlStateValueOff;
        _playlistButton.state = NSControlStateValueOff;
        _returnButton.state = returnButtonControlState;
        _playlistButton.state = playlistButtonControlState;
    }

    const CGFloat realButtonSpace = (windowTitlebarHeight - _playlistButton.cell.cellSize.height) / 2;
    const NSRect windowButtonBox = [self windowButtonsRect];

    _returnButtonLeadingConstraint.constant = placeInFakeToolbar ? windowButtonBox.size.width + VLCLibraryUIUnits.mediumSpacing + realButtonSpace : VLCLibraryUIUnits.largeSpacing;
    _playlistButtonTrailingConstraint.constant = placeInFakeToolbar ? realButtonSpace: VLCLibraryUIUnits.largeSpacing;

    _overlayView.drawGradientForTopControls = !placeInFakeToolbar;
    [_overlayView setNeedsDisplay:YES];
}

- (IBAction)togglePlaylist:(id)sender
{
    VLCLibraryWindow * const libraryWindow = (VLCLibraryWindow*)self.view.window;
    if (libraryWindow != nil) {
        [libraryWindow.splitViewController togglePlaylistSidebar:self];
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
