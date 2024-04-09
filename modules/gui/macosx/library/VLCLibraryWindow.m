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

#import "main/VLCMain.h"
#import "menus/VLCMainMenu.h"

#import "playlist/VLCPlayerController.h"
#import "playlist/VLCPlaylistController.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryNavigationStack.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibrarySortingMenuController.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindowNavigationSidebarViewController.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"
#import "library/VLCLibraryWindowPlaylistSidebarViewController.h"
#import "library/VLCLibraryWindowSplitViewController.h"
#import "library/VLCLibraryWindowToolbarDelegate.h"

#import "library/home-library/VLCLibraryHomeViewController.h"

#import "library/video-library/VLCLibraryVideoDataSource.h"
#import "library/video-library/VLCLibraryVideoViewController.h"

#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryAudioViewController.h"
#import "library/audio-library/VLCLibraryAudioDataSource.h"

#import "media-source/VLCMediaSourceBaseDataSource.h"
#import "media-source/VLCLibraryMediaSourceViewController.h"

#import "menus/renderers/VLCRendererMenuController.h"

#import "views/VLCBottomBarView.h"
#import "views/VLCCustomWindowButton.h"
#import "views/VLCDragDropView.h"
#import "views/VLCRoundedCornerTextField.h"

#import "windows/mainwindow/VLCControlsBarCommon.h"

#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "windows/video/VLCMainVideoViewController.h"

#import "windows/VLCOpenWindowController.h"
#import "windows/VLCOpenInputMetadata.h"

#import <vlc_common.h>
#import <vlc_configuration.h>
#import <vlc_url.h>

const CGFloat VLCLibraryWindowMinimalWidth = 604.;
const CGFloat VLCLibraryWindowMinimalHeight = 307.;
const NSUserInterfaceItemIdentifier VLCLibraryWindowIdentifier = @"VLCLibraryWindow";

@interface VLCLibraryWindow ()
{
    NSInteger _librarySegmentType;
    NSInteger _currentSelectedViewModeSegment;
}

@property NSTimer *searchInputTimer;

@end

static int ShowFullscreenController(vlc_object_t *p_this, const char *psz_variable,
                                    vlc_value_t old_val, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSNotificationCenter.defaultCenter postNotificationName:VLCVideoWindowShouldShowFullscreenController
                                                                object:nil];
        });

        return VLC_SUCCESS;
    }
}

static int ShowController(vlc_object_t *p_this, const char *psz_variable,
                          vlc_value_t old_val, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [NSNotificationCenter.defaultCenter postNotificationName:VLCWindowShouldShowController
                                                                object:nil];
        });

        return VLC_SUCCESS;
    }
}

static void addShadow(NSImageView *__unsafe_unretained imageView)
{
    NSShadow *buttonShadow = [[NSShadow alloc] init];

    buttonShadow.shadowBlurRadius = 15.0f;
    buttonShadow.shadowOffset = CGSizeMake(0.0f, -5.0f);
    buttonShadow.shadowColor = [NSColor blackColor];

    imageView.wantsLayer = YES;
    imageView.shadow = buttonShadow;
}

@implementation VLCLibraryWindow

- (void)awakeFromNib
{
    [super awakeFromNib];
    self.identifier = VLCLibraryWindowIdentifier;
    self.minSize = NSMakeSize(VLCLibraryWindowMinimalWidth, VLCLibraryWindowMinimalHeight);

    if(@available(macOS 10.12, *)) {
        self.tabbingMode = NSWindowTabbingModeDisallowed;
    }

    _toolbarDelegate = [[VLCLibraryWindowToolbarDelegate alloc] initWithLibraryWindow:self];
    self.toolbar.delegate = _toolbarDelegate;
    self.toolbar.allowsUserCustomization = NO;

    if (@available(macOS 11.0, *)) {
        const NSInteger navSidebarToggleToolbarItemIndex = [self.toolbar.items indexOfObject:self.toggleNavSidebarToolbarItem];
        NSAssert(navSidebarToggleToolbarItemIndex != NSNotFound, @"Could not find navigation sidebar toggle toolbar item!");

        const NSInteger trackingSeparatorItemIndex = navSidebarToggleToolbarItemIndex + 1;
        [self.toolbar insertItemWithItemIdentifier:VLCLibraryWindowTrackingSeparatorToolbarItemIdentifier
                                           atIndex:trackingSeparatorItemIndex];
        self.trackingSeparatorToolbarItem = [self.toolbar.items objectAtIndex:trackingSeparatorItemIndex];
    }


    VLCMain *mainInstance = VLCMain.sharedInstance;
    _playlistController = [mainInstance playlistController];

    libvlc_int_t *libvlc = vlc_object_instance(getIntf());
    var_AddCallback(libvlc, "intf-toggle-fscontrol", ShowFullscreenController, (__bridge void *)self);
    var_AddCallback(libvlc, "intf-show", ShowController, (__bridge void *)self);

    _libraryTargetView = [[NSView alloc] init];

    self.navigationStack = [[VLCLibraryNavigationStack alloc] init];
    self.navigationStack.delegate = self;

    self.videoViewController.view.frame = self.mainSplitView.frame;
    self.videoViewController.view.hidden = YES;
    self.videoViewController.displayLibraryControls = YES;
    [self hideControlsBarImmediately];

    [self.gridVsListSegmentedControl setToolTip: _NS("Grid View or List View")];
    [self.librarySortButton setToolTip: _NS("Select Sorting Mode")];
    [self.playQueueToggle setToolTip: _NS("Toggle Playqueue")];

    [self.gridVsListSegmentedControl setHidden:NO];
    [self.librarySortButton setHidden:NO];
    [self.librarySearchField setEnabled:YES];

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
                           selector:@selector(playerStateChanged:)
                               name:VLCPlayerStateChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(renderersChanged:)
                               name:VLCRendererAddedNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(renderersChanged:)
                               name:VLCRendererRemovedNotification
                             object:nil];

    _libraryHomeViewController = [[VLCLibraryHomeViewController alloc] initWithLibraryWindow:self];
    _libraryVideoViewController = [[VLCLibraryVideoViewController alloc] initWithLibraryWindow:self];
    _libraryAudioViewController = [[VLCLibraryAudioViewController alloc] initWithLibraryWindow:self];
    _libraryMediaSourceViewController = [[VLCLibraryMediaSourceViewController alloc] initWithLibraryWindow:self];

    [self setViewForSelectedSegment];

    // Hide renderers toolbar item at first. Start discoveries and wait for notifications about
    // renderers being added or removed to keep hidden or show depending on outcome
    [self hideToolbarItem:_renderersToolbarItem];
    [VLCMain.sharedInstance.mainMenu.rendererMenuController startRendererDiscoveries];

    [self updatePlayqueueToggleState];
}

- (void)dealloc
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    if (@available(macOS 10.14, *)) {
        [NSApplication.sharedApplication removeObserver:self forKeyPath:@"effectiveAppearance"];
    }

    libvlc_int_t *libvlc = vlc_object_instance(getIntf());
    var_DelCallback(libvlc, "intf-toggle-fscontrol", ShowFullscreenController, (__bridge void *)self);
    var_DelCallback(libvlc, "intf-show", ShowController, (__bridge void *)self);
}

- (void)encodeRestorableStateWithCoder:(NSCoder *)coder
{
    [super encodeRestorableStateWithCoder:coder];
    [coder encodeInteger:_librarySegmentType forKey:@"macosx-library-selected-segment"];
}

#pragma mark - misc. user interactions

- (void)updateGridVsListViewModeSegmentedControl
{
    VLCLibraryWindowPersistentPreferences * const preferences = VLCLibraryWindowPersistentPreferences.sharedInstance;

    switch (_librarySegmentType) {
    case VLCLibraryHomeSegment:
        _currentSelectedViewModeSegment = preferences.homeLibraryViewMode;
    case VLCLibraryVideoSegment:
        _currentSelectedViewModeSegment = preferences.videoLibraryViewMode;
        break;
    case VLCLibraryMusicSegment:
    case VLCLibraryArtistsMusicSubSegment:
        _currentSelectedViewModeSegment = preferences.artistLibraryViewMode;
        break;
    case VLCLibraryGenresMusicSubSegment:
        _currentSelectedViewModeSegment = preferences.genreLibraryViewMode;
        break;
    case VLCLibraryAlbumsMusicSubSegment:
        _currentSelectedViewModeSegment = preferences.albumLibraryViewMode;
        break;
    case VLCLibrarySongsMusicSubSegment:
        _currentSelectedViewModeSegment = preferences.songsLibraryViewMode;
        break;
    case VLCLibraryBrowseSegment:
        _currentSelectedViewModeSegment = preferences.browseLibraryViewMode;
        break;
    case VLCLibraryStreamsSegment:
        _currentSelectedViewModeSegment = preferences.streamLibraryViewMode;
        break;
    default:
        break;
    }

    _gridVsListSegmentedControl.selectedSegment = _currentSelectedViewModeSegment;
}

- (void)setViewForSelectedSegment
{
    switch (_librarySegmentType) {
    case VLCLibraryHomeSegment:
        [self showHomeLibrary];
        break;
    case VLCLibraryVideoSegment:
        [self showVideoLibrary];
        break;
    case VLCLibraryMusicSegment:
    case VLCLibraryArtistsMusicSubSegment:
    case VLCLibraryAlbumsMusicSubSegment:
    case VLCLibrarySongsMusicSubSegment:
    case VLCLibraryGenresMusicSubSegment:
        [self showAudioLibrary];
        break;
    case VLCLibraryBrowseSegment:
    case VLCLibraryStreamsSegment:
        [self showMediaSourceLibrary];
        break;
    default:
        break;
    }

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

    VLCLibraryWindowPersistentPreferences * const preferences = VLCLibraryWindowPersistentPreferences.sharedInstance;

    switch (_librarySegmentType) {
    case VLCLibraryHomeSegment:
        preferences.homeLibraryViewMode = _currentSelectedViewModeSegment;
        break;
    case VLCLibraryVideoSegment:
        preferences.videoLibraryViewMode = _currentSelectedViewModeSegment;
        break;
    case VLCLibraryMusicSegment:
    case VLCLibraryArtistsMusicSubSegment:
        preferences.artistLibraryViewMode = _currentSelectedViewModeSegment;
        break;
    case VLCLibraryGenresMusicSubSegment:
        preferences.genreLibraryViewMode = _currentSelectedViewModeSegment;
        break;
    case VLCLibraryAlbumsMusicSubSegment:
        preferences.albumLibraryViewMode = _currentSelectedViewModeSegment;
        break;
    case VLCLibrarySongsMusicSubSegment:
        preferences.songsLibraryViewMode = _currentSelectedViewModeSegment;
        break;
    case VLCLibraryBrowseSegment:
        preferences.browseLibraryViewMode = _currentSelectedViewModeSegment;
        break;
    case VLCLibraryStreamsSegment:
        preferences.streamLibraryViewMode = _currentSelectedViewModeSegment;
        break;
    default:
        break;
    }

    [self setViewForSelectedSegment];
}

- (void)hideToolbarItem:(NSToolbarItem *)toolbarItem
{
    NSInteger toolbarItemIndex = [[self.toolbar items] indexOfObject:toolbarItem];
    if (toolbarItemIndex != NSNotFound) {
        [self.toolbar removeItemAtIndex:toolbarItemIndex];
    }
}

/*
 * Try to insert the toolbar item ahead of a group of possible toolbar items.
 * "items" should contain items sorted from the trailing edge of the toolbar to leading edge.
 * "toolbarItem" will be inserted as close to the trailing edge as possible.
 *
 * If you have: | item1 | item2 | item3 | item4 |
 * and the "items" parameter is an array containing @[item6, item5, item2, item1]
 * then the "toolbarItem" provided to this function will place toolbarItem thus:
 * | item1 | item2 | toolbarItem | item3 | item4 |
*/

- (void)insertToolbarItem:(NSToolbarItem *)toolbarItem inFrontOf:(NSArray<NSToolbarItem *> *)items
{
    NSParameterAssert(toolbarItem != nil && items != nil && toolbarItem.itemIdentifier.length > 0);

    NSInteger toolbarItemIndex = [[self.toolbar items] indexOfObject:toolbarItem];
    if (toolbarItemIndex != NSNotFound) {
        return;
    }

    for (NSToolbarItem *item in items) {
        NSInteger itemIndex = [[self.toolbar items] indexOfObject:item];

        if (itemIndex != NSNotFound) {
            [self.toolbar insertItemWithItemIdentifier:toolbarItem.itemIdentifier atIndex:itemIndex + 1];
            return;
        }
    }

    [self.toolbar insertItemWithItemIdentifier:toolbarItem.itemIdentifier atIndex:0];
}

- (void)setForwardsBackwardsToolbarItemsVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:_forwardsToolbarItem];
        [self hideToolbarItem:_backwardsToolbarItem];
        return;
    }

    [self insertToolbarItem:_backwardsToolbarItem inFrontOf:@[_trackingSeparatorToolbarItem,
                                                              _toggleNavSidebarToolbarItem]];
    [self insertToolbarItem:_forwardsToolbarItem inFrontOf:@[_backwardsToolbarItem,
                                                             _trackingSeparatorToolbarItem,
                                                             _toggleNavSidebarToolbarItem]];
}

- (void)setSortOrderToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:_sortOrderToolbarItem];
        return;
    }

    [self insertToolbarItem:_sortOrderToolbarItem
                  inFrontOf:@[_libraryViewModeToolbarItem,
                              _forwardsToolbarItem,
                              _backwardsToolbarItem,
                              _trackingSeparatorToolbarItem,
                              _toggleNavSidebarToolbarItem]];
}

- (void)setLibrarySearchToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:_librarySearchToolbarItem];
        [self stopSearchTimer];
        _librarySearchField.stringValue = @"";
        [self updateFilterString];
        return;
    }

    // Display as far to the right as possible, but not in front of the playlist toggle button
    NSMutableArray<NSToolbarItem *> *currentToolbarItems = [NSMutableArray arrayWithArray:self.toolbar.items];
    if (currentToolbarItems.lastObject == _togglePlaylistToolbarItem) {
        [currentToolbarItems removeLastObject];
    }

    NSArray *reversedCurrentToolbarItems = [[currentToolbarItems reverseObjectEnumerator] allObjects];
    [self insertToolbarItem:_librarySearchToolbarItem inFrontOf:reversedCurrentToolbarItems];
}

- (void)setViewModeToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:self.libraryViewModeToolbarItem];
        return;
    }

    [self insertToolbarItem:self.libraryViewModeToolbarItem inFrontOf:@[
        self.toggleNavSidebarToolbarItem,
        self.trackingSeparatorToolbarItem,
        self.forwardsToolbarItem,
        self.backwardsToolbarItem
    ]];
}

- (void)updatePlayqueueToggleState
{
    NSView * const playlistView = self.splitViewController.playlistSidebarViewController.view;
    _playQueueToggle.state = [self.mainSplitView isSubviewCollapsed:playlistView] ?
        NSControlStateValueOff : NSControlStateValueOn;
}

- (void)showHomeLibrary
{
    // Only collection view mode
    [self setForwardsBackwardsToolbarItemsVisible:NO];
    [self setSortOrderToolbarItemVisible:YES];
    [self setLibrarySearchToolbarItemVisible:YES];
    [self setViewModeToolbarItemVisible:NO];

    [_libraryHomeViewController presentHomeView];
}

- (void)showVideoLibrary
{
    [self setForwardsBackwardsToolbarItemsVisible:NO];
    [self setSortOrderToolbarItemVisible:YES];
    [self setLibrarySearchToolbarItemVisible:YES];
    [self setViewModeToolbarItemVisible:YES];

    [_libraryVideoViewController presentVideoView];
}

- (void)showAudioLibrary
{
    [self setForwardsBackwardsToolbarItemsVisible:NO];
    [self setSortOrderToolbarItemVisible:YES];
    [self setLibrarySearchToolbarItemVisible:YES];
    [self setViewModeToolbarItemVisible:YES];

    self.libraryAudioViewController.currentSegmentType = self.librarySegmentType;
}

- (void)showMediaSourceLibrary
{
    [self.navigationStack clear];
    [self setForwardsBackwardsToolbarItemsVisible:YES];
    [self setSortOrderToolbarItemVisible:NO];
    [self setLibrarySearchToolbarItemVisible:NO];
    [self setViewModeToolbarItemVisible:YES];

    const VLCLibrarySegmentType segmentType = self.librarySegmentType;
    if (segmentType == VLCLibraryBrowseSegment) {
        [_libraryMediaSourceViewController presentBrowseView];
    } else if (segmentType == VLCLibraryStreamsSegment) {
        [_libraryMediaSourceViewController presentStreamsView];
    }
}

- (void)presentAudioLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    [self showAudioLibrary];
    [self.libraryAudioViewController presentLibraryItem:libraryItem];
}

- (void)presentVideoLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    [self showVideoLibrary];
    [self.libraryVideoViewController presentLibraryItem:libraryItem];
}

- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    const BOOL isAudioGroup = [libraryItem isKindOfClass:VLCMediaLibraryAlbum.class] ||
                              [libraryItem isKindOfClass:VLCMediaLibraryArtist.class] ||
                              [libraryItem isKindOfClass:VLCMediaLibraryGenre.class];

    if (isAudioGroup) {
        [self presentAudioLibraryItem:libraryItem];
        return;
    }

    VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)libraryItem;
    const BOOL validMediaItem = mediaItem != nil;
    if (validMediaItem && mediaItem.mediaType == VLC_ML_MEDIA_TYPE_AUDIO) {
        [self presentAudioLibraryItem:libraryItem];
        return;
    } else if (validMediaItem && mediaItem.mediaType == VLC_ML_MEDIA_TYPE_VIDEO) {
        [self presentVideoLibraryItem:libraryItem];
        return;
    }

    NSLog(@"Unknown kind of library item provided, cannot present library view for it: %@", libraryItem.displayString);
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
}

- (void)updateFilterString
{
    [VLCMain.sharedInstance.libraryController filterByString:_librarySearchField.stringValue];
}

- (void)clearLibraryFilterString
{
    [self stopSearchTimer];
    _librarySearchField.stringValue = @"";
    [self updateFilterString];
}

- (BOOL)handlePasteBoardFromDragSession:(NSPasteboard *)paste
{
    id propertyList = [paste propertyListForType:NSFilenamesPboardType];
    if (propertyList == nil) {
        return NO;
    }

    NSArray *values = [propertyList sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
    NSUInteger valueCount = [values count];
    if (valueCount > 0) {
        NSMutableArray *metadataArray = [NSMutableArray arrayWithCapacity:valueCount];

        for (NSString *filepath in values) {
            VLCOpenInputMetadata *inputMetadata;

            inputMetadata = [VLCOpenInputMetadata inputMetaWithPath:filepath];
            if (!inputMetadata)
                continue;

            [metadataArray addObject:inputMetadata];
        }
        [_playlistController addPlaylistItems:metadataArray];

        return YES;
    }

    return NO;
}

- (IBAction)goToBrowseSection:(id)sender
{
    [self.splitViewController.navSidebarViewController selectSegment:VLCLibraryBrowseSegment];
}

- (IBAction)backwardsNavigationAction:(id)sender
{
    self.videoViewController.view.hidden ? [_navigationStack backwards] : [self disableVideoPlaybackAppearance];
}

- (IBAction)forwardsNavigationAction:(id)sender
{
    [_navigationStack forwards];
}

#pragma mark - video output controlling

- (void)setHasActiveVideo:(BOOL)hasActiveVideo
{
    [super setHasActiveVideo:hasActiveVideo];
    if (hasActiveVideo) {
        [self enableVideoPlaybackAppearance];
    } else if (!self.videoViewController.view.hidden) {
        // If we are switching to audio media then keep the active main video view open
        NSURL * const currentMediaUrl = _playlistController.playerController.URLOfCurrentMediaItem;
        VLCMediaLibraryMediaItem * const mediaItem = [VLCMediaLibraryMediaItem mediaItemForURL:currentMediaUrl];
        const BOOL decorativeViewVisible = mediaItem != nil && mediaItem.mediaType == VLC_ML_MEDIA_TYPE_AUDIO;

        if (!decorativeViewVisible) {
            [self disableVideoPlaybackAppearance];
        }
    } else {
        [self disableVideoPlaybackAppearance];
    }
}

- (void)playerStateChanged:(NSNotification *)notification
{
    if (_playlistController.playerController.playerState == VLC_PLAYER_STATE_STOPPED) {
        [self hideControlsBar];
        return;
    }

    if (self.videoViewController.view.isHidden) {
        [self showControlsBar];
    }
}

// This handles reopening the video view when the user has closed it.
- (void)reopenVideoView
{
    [self enableVideoPlaybackAppearance];
}

- (void)hideControlsBarImmediately
{
    self.controlsBarHeightConstraint.constant = 0;
}

- (void)hideControlsBar
{
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
        context.duration = VLCLibraryUIUnits.controlsFadeAnimationDuration;
        self.controlsBarHeightConstraint.animator.constant = 0;
    } completionHandler:nil];
}

- (void)showControlsBarImmediately
{
    self.controlsBarHeightConstraint.constant = VLCLibraryUIUnits.libraryWindowControlsBarHeight;
}

- (void)showControlsBar
{
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext * const context) {
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn];
        context.duration = VLCLibraryUIUnits.controlsFadeAnimationDuration;
        self.controlsBarHeightConstraint.animator.constant = VLCLibraryUIUnits.libraryWindowControlsBarHeight;
    } completionHandler:nil];
}

- (void)presentExternalWindows
{
    VLCVideoOutputProvider * const voutProvider = VLCMain.sharedInstance.voutProvider;
    NSArray<NSWindow *> * const voutWindows = voutProvider.voutWindows.allValues;
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

    NSView *videoView = self.videoViewController.view;
    videoView.translatesAutoresizingMaskIntoConstraints = NO;
    videoView.hidden = NO;

    [_libraryTargetView addSubview:videoView];
    NSDictionary *dict = NSDictionaryOfVariableBindings(videoView);
    [_libraryTargetView addConstraints:@[
        [NSLayoutConstraint constraintWithItem:videoView
                                     attribute:NSLayoutAttributeTop
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:_libraryTargetView
                                     attribute:NSLayoutAttributeTop
                                    multiplier:1.
                                      constant:0.],
        [NSLayoutConstraint constraintWithItem:videoView
                                     attribute:NSLayoutAttributeBottom
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:_libraryTargetView
                                     attribute:NSLayoutAttributeBottom
                                    multiplier:1.
                                      constant:0.],
        [NSLayoutConstraint constraintWithItem:videoView
                                     attribute:NSLayoutAttributeLeft
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:_libraryTargetView
                                     attribute:NSLayoutAttributeLeft
                                    multiplier:1.
                                      constant:0.],
        [NSLayoutConstraint constraintWithItem:videoView
                                     attribute:NSLayoutAttributeRight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:_libraryTargetView
                                     attribute:NSLayoutAttributeRight
                                    multiplier:1.
                                      constant:0.]
    ]];
}

- (void)enableVideoPlaybackAppearance
{
    const BOOL isEmbedded = var_InheritBool(getIntf(), "embedded-video");
    if (!isEmbedded) {
        [self presentExternalWindows];
        return;
    }

    [self presentVideoView];

    [self.forwardsNavigationButton setHidden:YES];
    [self.gridVsListSegmentedControl setHidden:YES];
    [self.librarySortButton setHidden:YES];
    [self.librarySearchField setEnabled:NO];
    [self clearLibraryFilterString];

    // Make sure the back button is visible...
    [self insertToolbarItem:_backwardsToolbarItem inFrontOf:@[_trackingSeparatorToolbarItem, _toggleNavSidebarToolbarItem]];
    // And repurpose it to hide the video view
    [self.backwardsNavigationButton setEnabled:YES];

    [self enableVideoTitleBarMode];
    [self hideControlsBarImmediately];
    [self.videoViewController showControls];

    self.splitViewController.playlistSidebarViewController.mainVideoModeEnabled = YES;
}

- (void)disableVideoPlaybackAppearance
{
    [self makeFirstResponder:self.splitViewController.playlistSidebarViewController.view];
    [VLCMain.sharedInstance.voutProvider updateWindowLevelForHelperWindows: NSNormalWindowLevel];

    // restore alpha value to 1 for the case that macosx-opaqueness is set to < 1
    [self setAlphaValue:1.0];
    self.videoViewController.view.hidden = YES;

    [self.forwardsNavigationButton setHidden:NO];
    [self.gridVsListSegmentedControl setHidden:NO];
    [self.librarySortButton setHidden:NO];
    [self.librarySearchField setEnabled:YES];

    // Reset the back button to navigation state
    [self.backwardsNavigationButton setEnabled:_navigationStack.backwardsAvailable];

    [self setViewForSelectedSegment];

    [self disableVideoTitleBarMode];
    [self showControlsBarImmediately];
    self.splitViewController.playlistSidebarViewController.mainVideoModeEnabled = NO;
}

- (void)mouseMoved:(NSEvent *)o_event
{
    if (!self.videoViewController.view.hidden) {
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

    if (!self.videoViewController.view.hidden) {
        [self hideControlsBar];
    }
}

- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
    [super windowDidEnterFullScreen:notification];
    if (!self.videoViewController.view.hidden) {
        [self showControlsBar];
    }
}

#pragma mark -
#pragma mark respond to renderers

- (void)renderersChanged:(NSNotification *)notification
{
    const NSUInteger rendererCount = VLCMain.sharedInstance.mainMenu.rendererMenuController.rendererItems.count;
    const BOOL rendererToolbarItemVisible = [self.toolbar.items containsObject:_renderersToolbarItem];

    if (rendererCount > 0 && !rendererToolbarItemVisible) {
        [self insertToolbarItem:_renderersToolbarItem
                      inFrontOf:@[_sortOrderToolbarItem, _libraryViewModeToolbarItem, _forwardsToolbarItem, _backwardsToolbarItem]];
    } else if (rendererCount == 0 && rendererToolbarItemVisible) {
        [self hideToolbarItem:_renderersToolbarItem];
    }
}

- (void)rendererControlAction:(id)sender
{
    [NSMenu popUpContextMenu:VLCMain.sharedInstance.mainMenu.rendererMenu
                   withEvent:NSApp.currentEvent
                     forView:sender];
}

@end
