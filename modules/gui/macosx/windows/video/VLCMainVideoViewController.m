/*****************************************************************************
 * VLCMainVideoViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *          Maxime Chapelet <umxprime at videolabs dot io>
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

#import "extensions/NSView+VLCAdditions.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowSidebarRootViewController.h"
#import "library/VLCLibraryWindowSplitViewController.h"

#import "main/VLCMain.h"

#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayerController.h"

#import "views/VLCBottomBarView.h"

#import "windows/controlsbar/VLCMainVideoViewControlsBar.h"

#import "windows/video/VLCMainVideoViewAudioMediaDecorativeView.h"
#import "windows/video/VLCMainVideoViewOverlayView.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "windows/video/VLCVideoWindowCommon.h"

#import <vlc_common.h>

#import "private/PIPSPI.h"

@interface PIPVoutViewController : NSViewController
@end

@implementation PIPVoutViewController

- (void)setView:(NSView *)view {
    [super setView:view];
}

- (void)viewDidLoad {
    [super viewDidLoad];
}

- (void)viewWillAppear {
    [super viewWillAppear];

    if (self.view.superview) {
        [self.view applyConstraintsToFillSuperview];
    }
}

- (void)viewDidAppear {
    [super viewDidAppear];
}
@end

@interface VLCMainVideoViewController() <PIPViewControllerDelegate>
{
    NSTimer *_hideControlsTimer;
    NSLayoutConstraint *_returnButtonBottomConstraint;
    NSLayoutConstraint *_playQueueButtonBottomConstraint;
    PIPViewController *_pipViewController;
    PIPVoutViewController *_voutViewController;

    BOOL _isFadingIn;
}

@property NSWindow *retainedWindow;
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
        [notificationCenter addObserver:self
                               selector:@selector(playerCurrentItemTrackListChanged:)
                                   name:VLCPlayerTrackListChanged
                                 object:nil];
        [notificationCenter addObserver:self
                                   selector:@selector(playerBufferChanged:)
                                   name:VLCPlayerBufferChanged
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(floatOnTopChanged:)
                                   name:VLCWindowFloatOnTopChangedNotificationName
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(shouldShowControls:)
                                   name:VLCVideoWindowShouldShowFullscreenController
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(pictureInPictureChanged:)
                                   name:VLCPlayerPictureInPictureChanged
                                 object:nil];

        Class PIPViewControllerClass = NSClassFromString(@"PIPViewController");
        _pipViewController = [[PIPViewControllerClass alloc] init];
        _pipViewController.delegate = self;
        _pipViewController.userCanResize = true;
    }
    return self;
}

- (void)setupAudioDecorativeView
{
    _audioDecorativeView = [VLCMainVideoViewAudioMediaDecorativeView fromNibWithOwner:self];
    _audioDecorativeView.translatesAutoresizingMaskIntoConstraints = NO;

    _bottomButtonStackViewConstraint =
        [self.bottomBarView.topAnchor constraintEqualToAnchor:self.centralControlsStackView.bottomAnchor];

    VLCPlayerController * const controller =
        VLCMain.sharedInstance.playQueueController.playerController;
    [self updateDecorativeViewVisibilityOnControllerChange:controller];
}

- (void)viewDidLoad
{
    self.loadingIndicator.hidden = YES;

    BOOL floatOnTopActive = NO;
    VLCVideoWindowCommon * const window = (VLCVideoWindowCommon *)self.view.window;
    vout_thread_t * const p_vout = window.videoViewController.voutView.voutThread;
    if (p_vout) {
        floatOnTopActive = var_GetBool(p_vout, "video-on-top");
        vout_Release(p_vout);
    }
    self.floatOnTopIndicatorImageView.hidden = !floatOnTopActive;

    _autohideControls = YES;

    [self setDisplayLibraryControls:NO];
    [self updatePlayQueueToggleState];
    [self updateLibraryControls];

    _returnButtonBottomConstraint = [NSLayoutConstraint constraintWithItem:_returnButton
                                                                 attribute:NSLayoutAttributeBottom
                                                                 relatedBy:NSLayoutRelationEqual
                                                                    toItem:_fakeTitleBar
                                                                 attribute:NSLayoutAttributeBottom
                                                                multiplier:1.
                                                                  constant:0];
    _playQueueButtonBottomConstraint = [NSLayoutConstraint constraintWithItem:_playQueueButton
                                                                    attribute:NSLayoutAttributeBottom
                                                                    relatedBy:NSLayoutRelationEqual
                                                                       toItem:_fakeTitleBar
                                                                    attribute:NSLayoutAttributeBottom
                                                                   multiplier:1.
                                                                     constant:0];

    _returnButtonBottomConstraint.active = NO;
    _playQueueButtonBottomConstraint.active = NO;

    [self setupAudioDecorativeView];
    [self.controlsBar update];
    [self updateFloatOnTopIndicator];
}

- (void)updateDecorativeViewVisibilityOnControllerChange:(VLCPlayerController *)controller
{
    VLCMediaLibraryMediaItem * const mediaItem = 
        [VLCMediaLibraryMediaItem mediaItemForURL:controller.URLOfCurrentMediaItem];

    BOOL decorativeViewVisible = NO;
    if (mediaItem != nil) {
        decorativeViewVisible = mediaItem.mediaType == VLC_ML_MEDIA_TYPE_AUDIO;
    } else {
        VLCInputItem * const inputItem = controller.currentMedia;
        decorativeViewVisible = inputItem != nil && controller.videoTracks.count == 0;
    }

    NSView * const targetView = decorativeViewVisible ? self.audioDecorativeView : self.voutView;
    self.voutContainingView.subviews = @[targetView];
    [targetView applyConstraintsToFillSuperview];

    if (decorativeViewVisible) {
        [self setAutohideControls:NO];
        self.centerButtonStackInViewConstraint.active = NO;
        self.bottomButtonStackViewConstraint.active = YES;
        self.prevButtonSizeConstraint.constant = VLCLibraryUIUnits.smallPlaybackControlButtonSize;
        self.playButtonSizeConstraint.constant = VLCLibraryUIUnits.smallPlaybackControlButtonSize;
        self.nextButtonSizeConstraint.constant = VLCLibraryUIUnits.smallPlaybackControlButtonSize;
        [self applyAudioDecorativeViewForegroundCoverArtViewConstraints];
    } else {
        [self setAutohideControls:YES];
        self.bottomButtonStackViewConstraint.active = NO;
        self.centerButtonStackInViewConstraint.active = YES;
        self.prevButtonSizeConstraint.constant = VLCLibraryUIUnits.mediumPlaybackControlButtonSize;
        self.playButtonSizeConstraint.constant = VLCLibraryUIUnits.largePlaybackControlButtonSize;
        self.nextButtonSizeConstraint.constant = VLCLibraryUIUnits.mediumPlaybackControlButtonSize;
    }
}

- (void)applyAudioDecorativeViewForegroundCoverArtViewConstraints
{
    if (![self.voutContainingView.subviews containsObject:self.audioDecorativeView]) {
        return;
    }

    NSView * const foregroundCoverArtView = self.audioDecorativeView.foregroundCoverArtView;
    [NSLayoutConstraint activateConstraints:@[
        [self.centralControlsStackView.topAnchor constraintGreaterThanOrEqualToAnchor:foregroundCoverArtView.bottomAnchor constant:VLCLibraryUIUnits.largeSpacing],
        [self.fakeTitleBar.bottomAnchor constraintLessThanOrEqualToAnchor:foregroundCoverArtView.topAnchor constant:-VLCLibraryUIUnits.largeSpacing]
    ]];
}

- (void)playerCurrentMediaItemChanged:(NSNotification *)notification
{
    NSParameterAssert(notification);
    VLCPlayerController * const controller = notification.object;
    NSAssert(controller != nil, 
             @"Player current media item changed notification should have valid player controller");
    [self updateDecorativeViewVisibilityOnControllerChange:controller];
}

- (void)playerCurrentItemTrackListChanged:(NSNotification *)notification
{
    NSParameterAssert(notification);
    VLCPlayerController * const controller = notification.object;
    NSAssert(controller != nil, 
             @"Player current item track list changed notification should have valid player controller");
    [self updateDecorativeViewVisibilityOnControllerChange:controller];
}

- (void)playerBufferChanged:(NSNotification *)notification
{
    NSParameterAssert(notification);
    NSParameterAssert(notification.userInfo != nil);

    NSNumber * const bufferFillNumber = notification.userInfo[VLCPlayerBufferFill];
    NSAssert(bufferFillNumber != nil, @"Buffer fill number should not be nil");

    const float bufferFill = bufferFillNumber.floatValue;
    self.loadingIndicator.hidden = bufferFill == 1.0;
    if (self.loadingIndicator.hidden) {
        [self.loadingIndicator stopAnimation:self];
    } else {
        [self.loadingIndicator startAnimation:self];
    }
}

- (void)floatOnTopChanged:(NSNotification *)notification
{
    VLCVideoWindowCommon * const videoWindow = (VLCVideoWindowCommon *)notification.object;
    NSAssert(videoWindow != nil, @"Received video window should not be nil!");
    VLCVideoWindowCommon * const selfVideoWindow = (VLCVideoWindowCommon *)self.view.window;

    if (videoWindow != selfVideoWindow) {
        return;
    }

    [self updateFloatOnTopIndicator];
}

- (void)updateFloatOnTopIndicator
{
    vout_thread_t * const voutThread = self.voutView.voutThread;
    if (voutThread == NULL) {
        return;
    }

    const bool floatOnTopEnabled = var_GetBool(voutThread, "video-on-top");
    self.floatOnTopIndicatorImageView.hidden = !floatOnTopEnabled;
}

- (BOOL)mouseOnControls
{
    NSPoint mousePos = [self.view.window mouseLocationOutsideOfEventStream];

    return [_centralControlsStackView mouse:mousePos inRect:_centralControlsStackView.frame] ||
        [_controlsBar.bottomBarView mouse:mousePos inRect: _controlsBar.bottomBarView.frame] ||
        [_returnButton mouse:mousePos inRect: _returnButton.frame] ||
        [_playQueueButton mouse:mousePos inRect: _playQueueButton.frame];
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
    [self updatePlayQueueToggleState];
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
    _playQueueButton.hidden = !displayLibraryControls;
}

- (void)updatePlayQueueToggleState
{
    VLCLibraryWindow * const libraryWindow = (VLCLibraryWindow*)self.view.window;
    if (libraryWindow != nil && _displayLibraryControls) {
        NSView * const sidebarView =
            libraryWindow.splitViewController.multifunctionSidebarViewController.view;
        self.playQueueButton.state = [libraryWindow.mainSplitView isSubviewCollapsed:sidebarView] ?
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
    _playQueueButtonTopConstraint.constant = buttonTopSpace;
    _returnButtonBottomConstraint.active = placeInFakeToolbar;
    _playQueueButtonBottomConstraint.active = placeInFakeToolbar;

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

    NSControlSize previousButtonSize = _playQueueButton.controlSize;
    _returnButton.controlSize = buttonSize;
    _playQueueButton.controlSize = buttonSize;

    // HACK: Upon changing the control size the actual highlight of toggled/hovered buttons doesn't change
    // properly, at least for recessed buttons. This is most obvious on the toggleable playlist button.
    // So reset the state and then retoggle once done.
    if (previousButtonSize != buttonSize) {
        const NSControlStateValue returnButtonControlState = _returnButton.state;
        const NSControlStateValue playQueueButtonControlState = _playQueueButton.state;
        _returnButton.state = NSControlStateValueOff;
        _playQueueButton.state = NSControlStateValueOff;
        _returnButton.state = returnButtonControlState;
        _playQueueButton.state = playQueueButtonControlState;
    }

    const CGFloat realButtonSpace = (windowTitlebarHeight - _playQueueButton.cell.cellSize.height) / 2;
    const NSRect windowButtonBox = [self windowButtonsRect];

    _returnButtonLeadingConstraint.constant = placeInFakeToolbar ? windowButtonBox.size.width + VLCLibraryUIUnits.mediumSpacing + realButtonSpace : VLCLibraryUIUnits.largeSpacing;
    _playQueueButtonTrailingConstraint.constant = placeInFakeToolbar ? realButtonSpace: VLCLibraryUIUnits.largeSpacing;

    _overlayView.drawGradientForTopControls = !placeInFakeToolbar;
    [_overlayView setNeedsDisplay:YES];
}

- (BOOL)pipIsActive
{
    return _voutViewController != nil;
}

- (void)pictureInPictureChanged:(NSNotification *)notification
{
    if (self.pipIsActive) {
        return;
    }

    NSWindow * const window = self.view.window;
    [window orderOut:window];
    self.retainedWindow = window;

    _voutViewController = [PIPVoutViewController new];
    _voutViewController.view = self.voutContainingView;
    VLCPlayerController * const controller = notification.object;
    _pipViewController.playing = controller.playerState == VLC_PLAYER_STATE_PLAYING;
    
    VLCInputItem * const item = controller.currentMedia;
    input_item_t * const p_input = item.vlcInputItem;
    vlc_mutex_lock(&p_input->lock);
    const struct input_item_es *item_es;
    vlc_vector_foreach_ref(item_es, &p_input->es_vec) {
        if (item_es->es.i_cat != VIDEO_ES) {
            continue;
        }
        const video_format_t * const fmt = &item_es->es.video;
        unsigned int width = fmt->i_visible_width;
        unsigned int height = fmt->i_visible_height;
        if (fmt->i_sar_num && fmt->i_sar_den) {
            height = (height * fmt->i_sar_den) / fmt->i_sar_num;
        }
        _pipViewController.aspectRatio = CGSizeMake(width, height);
        break;
    }
    vlc_mutex_unlock(&p_input->lock);
    _pipViewController.title = window.title;
    [_pipViewController presentViewControllerAsPictureInPicture:_voutViewController];
    
    if ([window isKindOfClass:VLCLibraryWindow.class]) {
        [self returnToLibrary:self];
    }
}

- (IBAction)togglePlayQueue:(id)sender
{
    VLCLibraryWindow * const libraryWindow = (VLCLibraryWindow*)self.view.window;
    if (libraryWindow != nil) {
        [libraryWindow.splitViewController toggleMultifunctionSidebar:self];
    }
}

- (IBAction)returnToLibrary:(id)sender
{
    VLCLibraryWindow *libraryWindow = (VLCLibraryWindow*)self.view.window;
    if (libraryWindow != nil) {
        [libraryWindow disableVideoPlaybackAppearance];
    }
}
#pragma mark - PIPViewControllerDelegate

- (BOOL)pipShouldClose:(PIPViewController *)pip
{
    return YES;
}

- (void)pipWillClose:(PIPViewController *)pip
{
    NSWindow * const window = self.retainedWindow;
    pip.replacementWindow = window;
    pip.replacementRect = self.view.frame;
    if ([window isKindOfClass:VLCLibraryWindow.class]) {
        [(VLCLibraryWindow *)window enableVideoPlaybackAppearance];
    }
    [window makeKeyAndOrderFront:window];
    self.retainedWindow = nil;
}

- (void)pipDidClose:(PIPViewController *)pip
{
    [self.voutContainingView removeFromSuperview];
    [self.view addSubview:self.voutContainingView
               positioned:NSWindowBelow
               relativeTo:self.mainControlsView];
    [self.voutContainingView applyConstraintsToFillSuperview];
    _voutViewController = nil;
    [self applyAudioDecorativeViewForegroundCoverArtViewConstraints];
}

- (void)pipActionPlay:(PIPViewController *)pip
{
    VLCPlayerController * const controller =
        VLCMain.sharedInstance.playQueueController.playerController;
    if (controller.playerState == VLC_PLAYER_STATE_PAUSED) {
        [controller resume];
    } else {
        [controller start];
    }
}

- (void)pipActionStop:(PIPViewController *)pip
{
    VLCPlayerController * const controller =
        VLCMain.sharedInstance.playQueueController.playerController;
    [controller pause];
}

- (void)pipActionPause:(PIPViewController *)pip
{
    VLCPlayerController * const controller =
        VLCMain.sharedInstance.playQueueController.playerController;
    [controller pause];
}

@end
