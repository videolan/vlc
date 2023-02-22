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

    [self setDisplayLibraryControls:NO];
    [self updatePlaylistToggleState];
    [self updateLibraryControlsTopConstraint];

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
    [self updatePlaylistToggleState];
    [self updateLibraryControlsTopConstraint];

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

- (void)updateLibraryControlsTopConstraint
{
    if (!_displayLibraryControls) {
        return;
    }

    const NSWindow * const viewWindow = self.view.window;
    const NSView * const titlebarView = [viewWindow standardWindowButton:NSWindowCloseButton].superview;
    const CGFloat windowTitlebarHeight = titlebarView.frame.size.height;

    const BOOL windowFullscreen = [(VLCWindow*)viewWindow isInNativeFullscreen] || [(VLCWindow*)viewWindow fullscreen];
    const CGFloat spaceToTitlebar = viewWindow.titlebarAppearsTransparent ? [VLCLibraryUIUnits smallSpacing] : [VLCLibraryUIUnits mediumSpacing];
    const CGFloat topSpaceWithTitlebar = windowTitlebarHeight + spaceToTitlebar;

    // Since the close/maximise/minimise buttons go on the left, we want to make sure the return
    // button does not overlap. But for the playlist button, as long as the toolbar and titlebar
    // appears fully transparent, it looks nicer to leave it at the top
    const CGFloat returnButtonTopSpace = titlebarView.hidden || windowFullscreen ?
        [VLCLibraryUIUnits mediumSpacing] : topSpaceWithTitlebar;
    const CGFloat playlistButtonTopSpace = viewWindow.toolbar.visible && viewWindow.titlebarAppearsTransparent && !windowFullscreen ?
        topSpaceWithTitlebar : [VLCLibraryUIUnits mediumSpacing];

    _returnButtonTopConstraint.constant = returnButtonTopSpace;
    _playlistButtonTopConstraint.constant = playlistButtonTopSpace;
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
