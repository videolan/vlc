/*****************************************************************************
 * VLCLibraryWindow.m: MacOS X interface module
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

#import "VLCLibraryWindow.h"

#import "VLCLibraryDataTypes.h"

#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSImage+VLCAdditions.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"
#import "extensions/NSWindow+VLCAdditions.h"

#import "main/VLCApplication.h"
#import "main/VLCMain.h"
#import "menus/VLCMainMenu.h"

#import "playqueue/VLCPlayerController.h"
#import "playqueue/VLCPlayQueueController.h"

#import "library/VLCInputItem.h"
#import "library/VLCLibraryAbstractMediaLibrarySegmentViewController.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibrarySortingMenuController.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindowChaptersSidebarViewController.h"
#import "library/VLCLibraryWindowNavigationSidebarViewController.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"
#import "library/VLCLibraryWindowSidebarRootViewController.h"
#import "library/VLCLibraryWindowSplitViewController.h"
#import "library/VLCLibraryWindowToolbarDelegate.h"

#import "library/groups-library/VLCLibraryGroupsViewController.h"

#import "library/home-library/VLCLibraryHomeViewController.h"

#import "library/video-library/VLCLibraryVideoDataSource.h"
#import "library/video-library/VLCLibraryVideoViewController.h"

#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryAudioViewController.h"
#import "library/audio-library/VLCLibraryAudioDataSource.h"

#import "library/playlist-library/VLCLibraryPlaylistViewController.h"

#import "media-source/VLCMediaSourceBaseDataSource.h"
#import "media-source/VLCLibraryMediaSourceViewController.h"
#import "media-source/VLCLibraryMediaSourceViewNavigationStack.h"

#import "views/VLCBottomBarView.h"
#import "views/VLCCustomWindowButton.h"
#import "views/VLCDragDropView.h"
#import "views/VLCImageView.h"
#import "views/VLCLoadingOverlayView.h"
#import "views/VLCNoResultsLabel.h"
#import "views/VLCPlaybackEndViewController.h"
#import "views/VLCRoundedCornerTextField.h"
#import "views/VLCSnowEffectView.h"
#import "views/VLCTrackingView.h"

#import "windows/controlsbar/VLCMainWindowControlsBar.h"

#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "windows/video/VLCMainVideoViewController.h"

#import "windows/VLCDetachedAudioWindow.h"
#import "windows/VLCOpenWindowController.h"
#import "windows/VLCOpenInputMetadata.h"

#import <vlc_common.h>
#import <vlc_configuration.h>
#import <vlc_media_library.h>
#import <vlc_url.h>

const CGFloat VLCLibraryWindowMinimalWidth = 604.;
const CGFloat VLCLibraryWindowMinimalHeight = 307.;
const NSUserInterfaceItemIdentifier VLCLibraryWindowIdentifier = @"VLCLibraryWindow";
NSString * const VLCLibraryWindowEmbeddedVideoPlaybackActiveKey = @"embeddedVideoPlaybackActive";

@interface VLCLibraryWindow () <NSControlTextEditingDelegate>
{
    NSInteger _currentSelectedViewModeSegment;
    VLCVideoWindowCommon *_temporaryAudioDecorativeWindow;
    NSView *_acquiredVideoView;
}

@property NSTimer *searchInputTimer;

@end

static int ShowFullscreenController(vlc_object_t * __unused p_this,
                                    const char * __unused psz_variable,
                                    vlc_value_t __unused old_val,
                                    vlc_value_t __unused new_val,
                                    void * __unused param)
{
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSNotificationCenter.defaultCenter postNotificationName:VLCVideoWindowShouldShowFullscreenController
                                                                object:nil];
        });

        return VLC_SUCCESS;
    }
}

static int ShowController(vlc_object_t * __unused p_this,
                          const char * __unused psz_variable,
                          vlc_value_t __unused old_val,
                          vlc_value_t __unused new_val,
                          void * __unused param)
{
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSNotificationCenter.defaultCenter postNotificationName:VLCWindowShouldShowController
                                                                object:nil];
        });

        return VLC_SUCCESS;
    }
}

@interface VLCLibraryWindow ()

@property BOOL presentLoadingOverlayOnVideoPlaybackHide;

@end

@implementation VLCLibraryWindow

- (void)awakeFromNib
{
    [super awakeFromNib];
    self.identifier = VLCLibraryWindowIdentifier;
    self.minSize = NSMakeSize(VLCLibraryWindowMinimalWidth, VLCLibraryWindowMinimalHeight);

    if(@available(macOS 10.12, *)) {
        self.tabbingMode = NSWindowTabbingModeDisallowed;
    }

    VLCMain *mainInstance = VLCMain.sharedInstance;
    _playQueueController = [mainInstance playQueueController];

    libvlc_int_t *libvlc = vlc_object_instance(getIntf());
    var_AddCallback(libvlc, "intf-toggle-fscontrol", ShowFullscreenController, (__bridge void *)self);
    var_AddCallback(libvlc, "intf-show", ShowController, (__bridge void *)self);

    _libraryTargetView = [[NSView alloc] init];

    self.videoViewController.view.frame = self.mainSplitView.frame;
    self.videoViewController.view.hidden = YES;
    self.videoViewController.displayLibraryControls = YES;
    [self hideControlsBarImmediately];

    NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(shouldShowController:)
                               name:VLCWindowShouldShowController
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerStateChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerCurrentMediaItemChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerStateChanged:)
                               name:VLCPlayerStateChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerTrackSelectionChanged:)
                               name:VLCPlayerTrackSelectionChanged
                             object:nil];

    [self setViewForSelectedSegment];
    [self setupLoadingOverlayView];

    self.librarySearchField.delegate = self;
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    libvlc_int_t *libvlc = vlc_object_instance(getIntf());
    var_DelCallback(libvlc, "intf-toggle-fscontrol", ShowFullscreenController, (__bridge void *)self);
    var_DelCallback(libvlc, "intf-show", ShowController, (__bridge void *)self);
}

- (void)encodeRestorableStateWithCoder:(NSCoder *)coder
{
    [super encodeRestorableStateWithCoder:coder];
    [coder encodeInteger:_librarySegmentType forKey:@"macosx-library-selected-segment"];
}

- (void)setupLoadingOverlayView
{
    _loadingOverlayView = [[VLCLoadingOverlayView alloc] init];
    self.loadingOverlayView.translatesAutoresizingMaskIntoConstraints = NO;
}

#pragma mark - misc. user interactions

- (void)updateGridVsListViewModeSegmentedControl
{
    _currentSelectedViewModeSegment =
        [VLCLibrarySegment segmentWithSegmentType:self.librarySegmentType].viewMode;
    _gridVsListSegmentedControl.selectedSegment = _currentSelectedViewModeSegment;
}

- (void)setViewForSelectedSegment
{
    const VLCLibrarySegmentType segmentType = self.librarySegmentType;
    VLCLibrarySegment * const segment = [VLCLibrarySegment segmentWithSegmentType:segmentType];
    [self applySegmentView:segment];
}

- (void)applySegmentView:(VLCLibrarySegment *)segment
{
    [self.toolbarDelegate applyVisiblityFlags:segment.toolbarDisplayFlags];
    if (![self.librarySegmentViewController isKindOfClass:segment.libraryViewControllerClass]) {
        _librarySegmentViewController = [segment newLibraryViewController];
    }
    [segment presentLibraryViewUsingController:self.librarySegmentViewController];
    [self invalidateRestorableState];
}

- (void)setLibrarySegmentType:(NSInteger)segmentType
{
    if (segmentType == _librarySegmentType) {
        return;
    }

    _librarySegmentType = segmentType;
    [self setViewForSelectedSegment];
    [self updateGridVsListViewModeSegmentedControl];
}

- (IBAction)gridVsListSegmentedControlAction:(id)sender
{
    if (_gridVsListSegmentedControl.selectedSegment == _currentSelectedViewModeSegment) {
        return;
    }

    _currentSelectedViewModeSegment = _gridVsListSegmentedControl.selectedSegment;
    [VLCLibrarySegment segmentWithSegmentType:self.librarySegmentType].viewMode = _currentSelectedViewModeSegment;
    [self setViewForSelectedSegment];
}

- (void)displayLibraryView:(NSView *)view
{
    view.translatesAutoresizingMaskIntoConstraints = NO;
    if ([self.libraryTargetView.subviews containsObject:self.loadingOverlayView]) {
        self.libraryTargetView.subviews = @[view, self.loadingOverlayView];
    } else {
        self.libraryTargetView.subviews = @[view];
    }

    [NSLayoutConstraint activateConstraints:@[
        [view.topAnchor constraintEqualToAnchor:self.libraryTargetView.topAnchor],
        [view.bottomAnchor constraintEqualToAnchor:self.libraryTargetView.bottomAnchor],
        [view.leftAnchor constraintEqualToAnchor:self.libraryTargetView.leftAnchor],
        [view.rightAnchor constraintEqualToAnchor:self.libraryTargetView.rightAnchor]
    ]];

    if (VLCMain.sharedInstance.metalLibrary && ((VLCApplication *)NSApplication.sharedApplication).winterHolidaysTheming) {
        VLCSnowEffectView * const snowView =
            [[VLCSnowEffectView alloc] initWithFrame:self.contentView.bounds];
        [self.libraryTargetView addSubview:snowView];
        snowView.translatesAutoresizingMaskIntoConstraints = NO;
        [snowView applyConstraintsToFillSuperview];
    }
}

- (void)displayLibraryPlaceholderViewWithImage:(NSImage *)image
                              usingConstraints:(NSArray<NSLayoutConstraint *> *)constraints
                             displayingMessage:(NSString *)message
{
    for (NSLayoutConstraint * const constraint in self.placeholderImageViewConstraints) {
        constraint.active = NO;
    }
    _placeholderImageViewConstraints = constraints;
    for (NSLayoutConstraint * const constraint in constraints) {
        constraint.active = YES;
    }

    [self displayLibraryView:self.emptyLibraryView];
    self.placeholderImageView.image = image;
    self.placeholderLabel.stringValue = message;
}

- (void)displayNoResultsMessage
{
    if (self.noResultsLabel == nil) {
        _noResultsLabel = [[VLCNoResultsLabel alloc] init];
        _noResultsLabel.translatesAutoresizingMaskIntoConstraints = NO;
    }
    
    if ([self.libraryTargetView.subviews containsObject:self.loadingOverlayView]) {
        self.libraryTargetView.subviews = @[self.noResultsLabel, self.loadingOverlayView];
    } else {
        self.libraryTargetView.subviews = @[_noResultsLabel];
    }

    [NSLayoutConstraint activateConstraints:@[
        [self.noResultsLabel.centerXAnchor constraintEqualToAnchor:self.libraryTargetView.centerXAnchor],
        [self.noResultsLabel.centerYAnchor constraintEqualToAnchor:self.libraryTargetView.centerYAnchor]
    ]];
}

- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    VLCLibrarySegment * const segment = [VLCLibrarySegment segmentForLibraryItem:libraryItem];
    [self applySegmentView:segment];
    if ([self.librarySegmentViewController conformsToProtocol:@protocol(VLCLibraryItemPresentingCapable)]) {
        [(VLCLibraryAbstractSegmentViewController<VLCLibraryItemPresentingCapable> *)self.librarySegmentViewController presentLibraryItem:libraryItem];
    }
}

- (void)goToLocalFolderMrl:(NSString *)mrl
{
    [self goToBrowseSection:self];
    [(VLCLibraryMediaSourceViewController *)self.librarySegmentViewController presentLocalFolderMrl:mrl];
}

- (IBAction)sortLibrary:(id)sender
{
    if (!_librarySortingMenuController) {
        _librarySortingMenuController = [[VLCLibrarySortingMenuController alloc] init];
    }
    [NSMenu popUpContextMenu:_librarySortingMenuController.librarySortingMenu withEvent:[NSApp currentEvent] forView:sender];
}

- (void)stopSearchTimer
{
    [self.searchInputTimer invalidate];
    self.searchInputTimer = nil;
}

- (IBAction)filterLibrary:(id)sender
{
    [self stopSearchTimer];
    self.searchInputTimer = [NSTimer scheduledTimerWithTimeInterval:0.3
                                                            target:self
                                                           selector:@selector(updateFilterString)
                                                           userInfo:nil
                                                            repeats:NO];
    
    // Trigger completions
    NSText * const fieldEditor = self.librarySearchField.currentEditor;
    if (fieldEditor) {
        [fieldEditor complete:sender];
    }
}

- (void)updateFilterString
{
    [VLCMain.sharedInstance.libraryController filterByString:_librarySearchField.stringValue];
}

- (void)clearFilterString
{
    [self stopSearchTimer];
    _librarySearchField.stringValue = @"";
    [self updateFilterString];
}

- (BOOL)handlePasteBoardFromDragSession:(NSPasteboard *)paste
{
    return [VLCFileDragRecognisingView handlePasteboardFromDragSessionAsPlayQueueItems:paste];
}

- (IBAction)goToBrowseSection:(id)sender
{
    [self.splitViewController.navSidebarViewController selectSegment:VLCLibraryBrowseSegmentType];
}

- (IBAction)backwardsNavigationAction:(id)sender
{
    !self.embeddedVideoPlaybackActive
        ? [((VLCLibraryMediaSourceViewController *)self.librarySegmentViewController).navigationStack backwards]
        : [self disableVideoPlaybackAppearance];
}

- (IBAction)forwardsNavigationAction:(id)sender
{
    [((VLCLibraryMediaSourceViewController *)self.librarySegmentViewController).navigationStack forwards];
}

#pragma mark - video output controlling

- (void)playerStateChanged:(NSNotification *)notification
{
    if (_playQueueController.playerController.playerState == VLC_PLAYER_STATE_STOPPED) {
        [self hideControlsBar];
        return;
    }

    if (self.videoViewController.view.isHidden) {
        [self showControlsBar];
    }
}

- (void)playerCurrentMediaItemChanged:(NSNotification *)notification
{
    NSParameterAssert(notification);
    VLCPlayerController * const controller = notification.object;
    NSAssert(controller != nil,
             @"Player current media item changed notification should have valid player controller");

    // Live video playback in controls bar artwork button handling
    if (self.splitViewController.mainVideoModeEnabled) {
        return;
    }

    if (controller.currentMediaIsAudioOnly && _acquiredVideoView) {
        [self.videoViewController returnVideoView:_acquiredVideoView];
        _acquiredVideoView = nil;
    } else if (!controller.currentMediaIsAudioOnly && _acquiredVideoView == nil) {
        [self configureArtworkButtonLiveVideoView];
    }
}

- (void)playerTrackSelectionChanged:(NSNotification *)notification
{
    [self updateArtworkButtonEnabledState];
}

- (void)updateArtworkButtonEnabledState
{
    VLCPlayerController * const playerController = self.playerController;
    const BOOL videoTrackDisabled =
        !playerController.videoTracksEnabled || !playerController.selectedVideoTrack.selected;
    const BOOL audioTrackDisabled =
        !playerController.audioTracksEnabled || !playerController.selectedAudioTrack.selected;
    const BOOL currentItemIsAudio =
        playerController.videoTracks.count == 0 && playerController.audioTracks.count > 0;
    const BOOL pipOpen = self.videoViewController.pipIsActive;
    const BOOL artworkButtonDisabled =
        (videoTrackDisabled && audioTrackDisabled) ||
        (videoTrackDisabled && !currentItemIsAudio) ||
        pipOpen;
    self.artworkButton.enabled = !artworkButtonDisabled;
    self.artworkButton.hidden = artworkButtonDisabled;
    self.controlsBar.thumbnailTrackingView.enabled = !artworkButtonDisabled;
    self.controlsBar.thumbnailTrackingView.viewToHide.hidden = artworkButtonDisabled;
}

- (void)configureArtworkButtonLiveVideoView
{
    VLCPlayerController * const playerController = self.playerController;
    const BOOL videoTrackDisabled =
        !playerController.videoTracksEnabled || !playerController.selectedVideoTrack.selected;
        
    if (videoTrackDisabled || !var_InheritBool(getIntf(), "embedded-video"))
        return;

    _acquiredVideoView = [self.videoViewController acquireVideoView];
    if (_acquiredVideoView) {
        [self.controlsBar.artworkImageView addSubview:_acquiredVideoView
                                           positioned:NSWindowBelow
                                           relativeTo:self.artworkButton];
        [_acquiredVideoView applyConstraintsToFillSuperview];
    }
}

- (void)hideControlsBarImmediately
{
    self.controlsBar.bottomBarView.hidden = YES;
    self.controlsBar.bottomBarView.alphaValue = 0;
}

- (void)hideControlsBar
{
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
        context.duration = VLCLibraryUIUnits.controlsFadeAnimationDuration;
        self.controlsBar.bottomBarView.animator.alphaValue = 0;
    } completionHandler:^{
        self.controlsBar.bottomBarView.hidden = self.controlsBar.bottomBarView.alphaValue == 0;
    }];
}

- (void)showControlsBarImmediately
{
    self.controlsBar.bottomBarView.hidden = NO;
    self.controlsBar.bottomBarView.alphaValue = 1;
}

- (void)showControlsBar
{
    self.controlsBar.bottomBarView.hidden = NO;
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn];
        context.duration = VLCLibraryUIUnits.controlsFadeAnimationDuration;
        self.controlsBar.bottomBarView.animator.alphaValue = 1;
    } completionHandler:nil];
}

- (void)presentExternalWindows
{
    VLCVideoOutputProvider * const voutProvider = VLCMain.sharedInstance.voutProvider;
    NSArray<NSWindow *> * const voutWindows = voutProvider.voutWindows.allValues;

    if (voutWindows.count == 0 && self.playerController.videoTracks.count == 0) {
        // If we have no video windows in the video provider but are being asked to present a window
        // then we are dealing with an audio item and the user wants to see the decorative artwork
        // window for said audio
        [VLCMain.sharedInstance.detachedAudioWindow makeKeyAndOrderFront:self];
        return;
    }

    for (NSWindow * const window in voutWindows) {
        [window makeKeyAndOrderFront:self];
    }
}

- (void)presentVideoView
{
    for (NSView *subview in _libraryTargetView.subviews) {
        [subview removeFromSuperview];
    }

    NSLog(@"Presenting video view in main library window.");

    NSView * const videoView = self.videoViewController.view;
    videoView.translatesAutoresizingMaskIntoConstraints = NO;
    videoView.hidden = NO;

    [_libraryTargetView addSubview:videoView];
    [videoView applyConstraintsToFillSuperview];
}

- (void)enableVideoPlaybackAppearance
{
    VLCPlayerController * const playerController = self.playerController;
    const BOOL videoTrackDisabled =
        !playerController.videoTracksEnabled || !playerController.selectedVideoTrack.selected;
    const BOOL audioTrackDisabled =
        !playerController.audioTracksEnabled || !playerController.selectedAudioTrack.selected;
    const BOOL currentItemIsAudio =
        playerController.videoTracks.count == 0 && playerController.audioTracks.count > 0;
    if ((videoTrackDisabled && audioTrackDisabled) || (videoTrackDisabled && !currentItemIsAudio)) {
        return;
    }

    const BOOL isEmbedded = var_InheritBool(getIntf(), "embedded-video");
    if (!isEmbedded) {
        [self presentExternalWindows];
        return;
    }

    NSAppearance *darkAppearance = nil;
    if (@available(macOS 10.14, *)) {
        darkAppearance = [NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    } else {
        darkAppearance = [NSAppearance appearanceNamed:NSAppearanceNameVibrantDark];
    }

    if (darkAppearance) {
        self.appearance = darkAppearance;
    }

    if (_acquiredVideoView) {
        [self.videoViewController returnVideoView:_acquiredVideoView];
        _acquiredVideoView = nil;
    }

    [self presentVideoView];
    [self enableVideoTitleBarMode];
    [self hideControlsBarImmediately];
    [self.videoViewController showControls];

    self.splitViewController.mainVideoModeEnabled = YES;

    [self willChangeValueForKey:VLCLibraryWindowEmbeddedVideoPlaybackActiveKey];
    _embeddedVideoPlaybackActive = YES;
    [self didChangeValueForKey:VLCLibraryWindowEmbeddedVideoPlaybackActiveKey];

    if ([self.librarySegmentViewController isKindOfClass:VLCLibraryAbstractMediaLibrarySegmentViewController.class]) {
        [(VLCLibraryAbstractMediaLibrarySegmentViewController *)self.librarySegmentViewController disconnect];
    }
}

- (void)disableVideoPlaybackAppearance
{
    self.appearance = nil;

    [self makeFirstResponder:self.splitViewController.multifunctionSidebarViewController.view];
    [VLCMain.sharedInstance.voutProvider updateWindowLevelForHelperWindows:NSNormalWindowLevel];

    // restore alpha value to 1 for the case that macosx-opaqueness is set to < 1
    self.alphaValue = 1.0;
    [self setViewForSelectedSegment];
    [self disableVideoTitleBarMode];
    [self showControlsBarImmediately];
    [self updateArtworkButtonEnabledState];

    if (!self.playQueueController.playerController.currentMediaIsAudioOnly) {
        [self configureArtworkButtonLiveVideoView];
    }

    self.splitViewController.mainVideoModeEnabled = NO;

    [self willChangeValueForKey:VLCLibraryWindowEmbeddedVideoPlaybackActiveKey];
    _embeddedVideoPlaybackActive = NO;
    [self didChangeValueForKey:VLCLibraryWindowEmbeddedVideoPlaybackActiveKey];

    if (self.presentLoadingOverlayOnVideoPlaybackHide) {
        [self showLoadingOverlay];
    }

    if ([self.librarySegmentViewController isKindOfClass:VLCLibraryAbstractMediaLibrarySegmentViewController.class]) {
        [(VLCLibraryAbstractMediaLibrarySegmentViewController *)self.librarySegmentViewController connect];
    }
}

- (void)showLoadingOverlay
{
    if ([self.libraryTargetView.subviews containsObject:self.loadingOverlayView]) {
        return;
    } else if ([self.libraryTargetView.subviews containsObject:self.videoViewController.view]) {
        self.presentLoadingOverlayOnVideoPlaybackHide = YES;
        return;
    }

    self.loadingOverlayView.wantsLayer = YES;
    self.loadingOverlayView.alphaValue = 0.0;

    NSArray * const views = [self.libraryTargetView.subviews arrayByAddingObject:self.loadingOverlayView];
    self.libraryTargetView.subviews = views;
    [self.loadingOverlayView applyConstraintsToFillSuperview];

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
        context.duration = 0.5;
        self.loadingOverlayView.animator.alphaValue = 1.0;
    } completionHandler:nil];
    [self.loadingOverlayView.indicator startAnimation:self];

}

- (void)hideLoadingOverlay
{
    self.presentLoadingOverlayOnVideoPlaybackHide = NO;

    if (![self.libraryTargetView.subviews containsObject:self.loadingOverlayView]) {
        return;
    }

    self.loadingOverlayView.wantsLayer = YES;
    self.loadingOverlayView.alphaValue = 1.0;

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
        context.duration = 1.0;
        self.loadingOverlayView.animator.alphaValue = 0.0;
    } completionHandler:^{
        NSMutableArray * const views = self.libraryTargetView.subviews.mutableCopy;
        [views removeObject:self.loadingOverlayView];
        self.libraryTargetView.subviews = views.copy;
        [self.loadingOverlayView.indicator stopAnimation:self];
    }];
}

- (void)mouseMoved:(NSEvent *)o_event
{
    if (self.embeddedVideoPlaybackActive) {
        NSPoint mouseLocation = [o_event locationInWindow];
        NSView *videoView = self.videoViewController.view;
        NSRect videoViewRect = [videoView convertRect:videoView.frame toView:self.contentView];

        if ([self.contentView mouse:mouseLocation inRect:videoViewRect]) {
            [NSNotificationCenter.defaultCenter postNotificationName:VLCVideoWindowShouldShowFullscreenController
                                                                object:self];
        }
    }

    [super mouseMoved:o_event];
}

#pragma mark - NSControlTextEditingDelegate

- (NSArray<NSString *> *)control:(NSControl *)control
                         textView:(NSTextView *)textView
                      completions:(NSArray<NSString *> *)words
              forPartialWordRange:(NSRange)charRange
              indexOfSelectedItem:(NSInteger *)index
{
    // Get the partial word being completed
    NSString * const currentText = textView.string;
    NSString * const partialWord = [currentText substringWithRange:charRange];
    
    if (partialWord.length == 0) {
        return @[];
    }
    
    // Get cached titles from library model
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    if (!libraryModel) {
        return @[];
    }
    
    NSArray<NSString *> * const allTitles = libraryModel.listOfMediaTitles;
    NSMutableArray<NSString *> * const matchingTitles = [NSMutableArray array];
    
    // Filter titles that match the partial word
    for (NSString * const title in allTitles) {
        if (![title.lowercaseString hasPrefix:partialWord.lowercaseString]) {
            continue;
        }
        [matchingTitles addObject:title];
        if (matchingTitles.count >= 5) {
            break;
        }
    }
    
    *index = -1;
    return matchingTitles;
}

#pragma mark -
#pragma mark respond to core events

- (void)shouldShowController:(NSNotification *)aNotification
{
    [self makeKeyAndOrderFront:nil];

    if (self.videoViewController.view.isHidden) {
        [self showControlsBar];
        NSView *standardWindowButtonsSuperView = [self standardWindowButton:NSWindowCloseButton].superview;
        standardWindowButtonsSuperView.hidden = NO;
    }
}

- (void)windowWillEnterFullScreen:(NSNotification *)notification
{
    [super windowWillEnterFullScreen:notification];

    if (self.splitViewController.mainVideoModeEnabled) {
        [self hideControlsBar];
    }

    if (self.splitViewController.mainVideoModeEnabled) {
        self.splitViewController.multifunctionSidebarItem.animator.collapsed = YES;
    }
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    [super windowDidEnterFullScreen:notification];
    if (!self.splitViewController.mainVideoModeEnabled) {
        [self showControlsBar];
    }
}

@end
