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
#include "VLCLibraryDataTypes.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSColor+VLCAdditions.h"
#import "extensions/NSView+VLCAdditions.h"
#import "main/VLCMain.h"

#import "playlist/VLCPlayerController.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlaylistDataSource.h"
#import "playlist/VLCPlaylistSortingMenuController.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibrarySortingMenuController.h"
#import "library/VLCLibraryNavigationStack.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindowAutohideToolbar.h"

#import "library/video-library/VLCLibraryVideoCollectionViewsStackViewController.h"
#import "library/video-library/VLCLibraryVideoTableViewDataSource.h"
#import "library/video-library/VLCLibraryVideoViewController.h"

#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryAudioViewController.h"

#import "media-source/VLCMediaSourceBaseDataSource.h"
#import "media-source/VLCLibraryMediaSourceViewController.h"

#import "views/VLCCustomWindowButton.h"
#import "views/VLCDragDropView.h"
#import "views/VLCRoundedCornerTextField.h"

#import "windows/mainwindow/VLCControlsBarCommon.h"
#import "windows/video/VLCFSPanelController.h"
#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCVideoOutputProvider.h"
#import "windows/VLCOpenWindowController.h"
#import "windows/VLCOpenInputMetadata.h"

#import <vlc_common.h>
#import <vlc_url.h>

const CGFloat VLCLibraryWindowMinimalWidth = 604.;
const CGFloat VLCLibraryWindowMinimalHeight = 307.;
const CGFloat VLCLibraryWindowDefaultPlaylistWidth = 340.;
const CGFloat VLCLibraryWindowMinimalPlaylistWidth = 170.;
const NSUserInterfaceItemIdentifier VLCLibraryWindowIdentifier = @"VLCLibraryWindow";

@interface VLCLibraryWindow () <VLCDragDropTarget, NSSplitViewDelegate>
{
    CGFloat _lastPlaylistWidthBeforeCollaps;
    
    NSInteger _currentSelectedSegment;
    NSInteger _currentSelectedViewModeSegment;

    NSTimer *_hideToolbarTimer;
}

- (IBAction)goToBrowseSection:(id)sender;

@end

static int ShowFullscreenController(vlc_object_t *p_this, const char *psz_variable,
                                    vlc_value_t old_val, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [[NSNotificationCenter defaultCenter] postNotificationName:VLCVideoWindowShouldShowFullscreenController
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
            [[NSNotificationCenter defaultCenter] postNotificationName:VLCWindowShouldShowController
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
    self.identifier = VLCLibraryWindowIdentifier;

    if(@available(macOS 10.12, *)) {
        self.tabbingMode = NSWindowTabbingModeDisallowed;
    }

    self.toolbar.allowsUserCustomization = NO;
    
    VLCMain *mainInstance = [VLCMain sharedInstance];
    _playlistController = [mainInstance playlistController];

    libvlc_int_t *libvlc = vlc_object_instance(getIntf());
    var_AddCallback(libvlc, "intf-toggle-fscontrol", ShowFullscreenController, (__bridge void *)self);
    var_AddCallback(libvlc, "intf-show", ShowController, (__bridge void *)self);

    self.navigationStack = [[VLCLibraryNavigationStack alloc] init];
    self.navigationStack.delegate = self;

    self.videoView = [[VLCVoutView alloc] initWithFrame:self.mainSplitView.frame];
    self.videoView.hidden = YES;
    [self hideControlsBar];

    [self.gridVsListSegmentedControl setToolTip: _NS("Grid View or List View")];
    [self.librarySortButton setToolTip: _NS("Select Sorting Mode")];
    [self.playQueueToggle setToolTip: _NS("Toggle Playqueue")];

    [self.gridVsListSegmentedControl setHidden:NO];
    [self.librarySortButton setHidden:NO];
    [self.librarySearchField setEnabled:YES];

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(shouldShowFullscreenController:)
                               name:VLCVideoWindowShouldShowFullscreenController
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(shouldShowController:)
                               name:VLCWindowShouldShowController
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(shuffleStateUpdated:)
                               name:VLCPlaybackOrderChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(repeatStateUpdated:)
                               name:VLCPlaybackRepeatChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateViewCellDimensionsBasedOnSetting:)
                               name:VLCConfigurationChangedNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerStateChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playerStateChanged:)
                               name:VLCPlayerStateChanged
                             object:nil];

    if (@available(macOS 10.14, *)) {
        [[NSApplication sharedApplication] addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                               options:NSKeyValueObservingOptionNew
                                               context:nil];
        
        _mediaToolBar.centeredItemIdentifier = _segmentedTitleControlToolbarItem.itemIdentifier;
    }

    _fspanel = [[VLCFSPanelController alloc] init];
    [_fspanel showWindow:self];

    _currentSelectedSegment = -1; // To enforce action on the selected segment
    _segmentedTitleControl.segmentCount = 4;
    [_segmentedTitleControl setTarget:self];
    [_segmentedTitleControl setLabel:_NS("Video") forSegment:VLCLibraryVideoSegment];
    [_segmentedTitleControl setLabel:_NS("Music") forSegment:VLCLibraryMusicSegment];
    [_segmentedTitleControl setLabel:_NS("Browse") forSegment:VLCLibraryBrowseSegment];
    [_segmentedTitleControl setLabel:_NS("Streams") forSegment:VLCLibraryStreamsSegment];
    [_segmentedTitleControl sizeToFit];

    _playlistDragDropView.dropTarget = self;
    _playlistCounterTextField.useStrongRounding = YES;
    _playlistCounterTextField.font = [NSFont VLCplaylistSelectedItemLabelFont];
    _playlistCounterTextField.textColor = [NSColor VLClibraryAnnotationColor];
    _playlistCounterTextField.hidden = YES;

    _playlistDataSource = [[VLCPlaylistDataSource alloc] init];
    _playlistDataSource.playlistController = _playlistController;
    _playlistDataSource.tableView = _playlistTableView;
    _playlistDataSource.dragDropView = _playlistDragDropView;
    _playlistDataSource.counterTextField = _playlistCounterTextField;
    [_playlistDataSource prepareForUse];
    _playlistController.playlistDataSource = _playlistDataSource;

    _playlistTableView.dataSource = _playlistDataSource;
    _playlistTableView.delegate = _playlistDataSource;
    [self updateViewCellDimensionsBasedOnSetting:nil];
    [_playlistTableView reloadData];

    _libraryVideoViewController = [[VLCLibraryVideoViewController alloc] initWithLibraryWindow:self];
    _libraryAudioViewController = [[VLCLibraryAudioViewController alloc] initWithLibraryWindow:self];
    _libraryMediaSourceViewController = [[VLCLibraryMediaSourceViewController alloc] initWithLibraryWindow:self];

    self.upNextLabel.font = [NSFont VLClibrarySectionHeaderFont];
    self.upNextLabel.stringValue = _NS("Playlist");
    self.openMediaButton.title = _NS("Open media...");
    self.dragDropImageBackgroundBox.fillColor = [NSColor VLClibrarySeparatorLightColor];

    [self updateColorsBasedOnAppearance:self.effectiveAppearance];

    _mainSplitView.delegate = self;
    _lastPlaylistWidthBeforeCollaps = VLCLibraryWindowDefaultPlaylistWidth;

    [self setViewForSelectedSegment];
    [self repeatStateUpdated:nil];
    [self shuffleStateUpdated:nil];

    // HACK: The size of the segmented title buttons is not always correctly calculated
    // especially when the text we are setting differs from what is set in the storyboard.
    // Hiding and showing the toolbar again must trigger something that causes the width
    // of the buttons to be correctly recalculated, working around this issue
    [self toggleToolbarShown:self];
    [self toggleToolbarShown:self];

    // The playlist toggle button's default state is OFF so we set it to ON if the playlist
    // is not collapsed when we open the library window
    if (![_mainSplitView isSubviewCollapsed:_playlistView]) {
        _playQueueToggle.state = NSControlStateValueOn;
    }
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (@available(macOS 10.14, *)) {
        [[NSApplication sharedApplication] removeObserver:self forKeyPath:@"effectiveAppearance"];
    }

    libvlc_int_t *libvlc = vlc_object_instance(getIntf());
    var_DelCallback(libvlc, "intf-toggle-fscontrol", ShowFullscreenController, (__bridge void *)self);
    var_DelCallback(libvlc, "intf-show", ShowController, (__bridge void *)self);
}

- (void)encodeRestorableStateWithCoder:(NSCoder *)coder
{
    [super encodeRestorableStateWithCoder:coder];
    [coder encodeInteger:_segmentedTitleControl.selectedSegment forKey:@"macosx-library-selected-segment"];
    [coder encodeInteger:_gridVsListSegmentedControl.selectedSegment forKey:@"macosx-library-view-mode-selected-segment"];
    [coder encodeInteger:_audioSegmentedControl.selectedSegment forKey:@"macosx-library-audio-view-selected-segment"];
}

#pragma mark - appearance setters

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    if ([keyPath isEqualToString:@"effectiveAppearance"]) {
        NSAppearance *effectiveAppearance = change[NSKeyValueChangeNewKey];
        [self updateColorsBasedOnAppearance:effectiveAppearance];
    }
}

- (void)updateColorsBasedOnAppearance:(NSAppearance*)appearance
{
    NSParameterAssert(appearance);
    BOOL isDark = NO;
    if (@available(macOS 10.14, *)) {
        isDark = [appearance.name isEqualToString:NSAppearanceNameDarkAqua] || [appearance.name isEqualToString:NSAppearanceNameVibrantDark];
    }

    // If we try to pull the view's effectiveAppearance we are going to get the previous appearance's name despite
    // responding to the effectiveAppearance change (???) so it is a better idea to pull from the general system
    // theme preference, which is always up-to-date
    if (isDark) {
        self.upNextLabel.textColor = [NSColor VLClibraryDarkTitleColor];
        self.upNextSeparator.borderColor = [NSColor VLClibrarySeparatorDarkColor];
        self.clearPlaylistSeparator.borderColor = [NSColor VLClibrarySeparatorDarkColor];
        self.dragDropImageBackgroundBox.hidden = NO;
    } else {
        self.upNextLabel.textColor = [NSColor VLClibraryLightTitleColor];
        self.upNextSeparator.borderColor = [NSColor VLClibrarySeparatorLightColor];
        self.clearPlaylistSeparator.borderColor = [NSColor VLClibrarySeparatorLightColor];
        self.dragDropImageBackgroundBox.hidden = YES;
    }
}

- (void)updateViewCellDimensionsBasedOnSetting:(NSNotification *)aNotification
{
    _playlistTableView.rowHeight = config_GetInt("macosx-large-text") ?
        [VLCLibraryUIUnits largeTableViewRowHeight] :
        [VLCLibraryUIUnits mediumTableViewRowHeight];
}

#pragma mark - playmode state display and interaction

- (IBAction)shuffleAction:(id)sender
{
    if (_playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL) {
        _playlistController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM;
    } else {
        _playlistController.playbackOrder = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;
    }
}

- (void)shuffleStateUpdated:(NSNotification *)aNotification
{
    if (_playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL) {
        self.shufflePlaylistButton.image = [NSImage imageNamed:@"shuffleOff"];
    } else {
        self.shufflePlaylistButton.image = [NSImage imageNamed:@"shuffleOn"];
    }
}

- (IBAction)repeatAction:(id)sender
{
    enum vlc_playlist_playback_repeat currentRepeatState = _playlistController.playbackRepeat;
    switch (currentRepeatState) {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
            break;

        default:
            _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;
            break;
    }
}

- (void)repeatStateUpdated:(NSNotification *)aNotification
{
    enum vlc_playlist_playback_repeat currentRepeatState = _playlistController.playbackRepeat;
    switch (currentRepeatState) {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            self.repeatPlaylistButton.image = [NSImage imageNamed:@"repeatAll"];
            break;
        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            self.repeatPlaylistButton.image = [NSImage imageNamed:@"repeatOne"];
            break;

        default:
            self.repeatPlaylistButton.image = [NSImage imageNamed:@"repeatOff"];
            break;
    }
}

#pragma mark - misc. user interactions

- (void)setViewForSelectedSegment
{
    _currentSelectedSegment = _segmentedTitleControl.selectedSegment;
    _currentSelectedViewModeSegment = _gridVsListSegmentedControl.selectedSegment;

    VLCLibrarySegment selectedLibrarySegment = _segmentedTitleControl.selectedSegment;
    switch (selectedLibrarySegment) {
        case VLCLibraryVideoSegment:
            [self showVideoLibrary];
            break;
        case VLCLibraryMusicSegment:
            [self showAudioLibrary];
            break;
        case VLCLibraryBrowseSegment:
        case VLCLibraryStreamsSegment:
            [self showMediaSourceLibraryWithSegment:selectedLibrarySegment];
            break;
        default:
            break;
    }
}

- (IBAction)segmentedControlAction:(id)sender
{
    if (_segmentedTitleControl.selectedSegment == _currentSelectedSegment && 
        _gridVsListSegmentedControl.selectedSegment == _currentSelectedViewModeSegment) {
        return;
    }

    [self setViewForSelectedSegment];
    [self invalidateRestorableState];
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

    [self insertToolbarItem:_backwardsToolbarItem inFrontOf:@[]];
    [self insertToolbarItem:_forwardsToolbarItem inFrontOf:@[_backwardsToolbarItem]];
}

- (void)setSortOrderToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:_sortOrderToolbarItem];
        return;
    }

    [self insertToolbarItem:_sortOrderToolbarItem
                  inFrontOf:@[_libraryViewModeToolbarItem, _forwardsToolbarItem, _backwardsToolbarItem]];
}

- (void)setLibrarySearchToolbarItemVisible:(BOOL)visible
{
    if (!visible) {
        [self hideToolbarItem:_librarySearchToolbarItem];
        _librarySearchField.stringValue = @"";
        [VLCMain.sharedInstance.libraryController filterByString:@""];
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

- (void)showVideoLibrary
{
    [self setForwardsBackwardsToolbarItemsVisible:NO];
    [self setSortOrderToolbarItemVisible:YES];
    [self setLibrarySearchToolbarItemVisible:YES];
    _optionBarView.hidden = YES;

    _gridVsListSegmentedControl.target = self;
    _gridVsListSegmentedControl.action = @selector(segmentedControlAction:);

    [_libraryVideoViewController presentVideoView];
}

- (void)showAudioLibrary
{
    [self setForwardsBackwardsToolbarItemsVisible:NO];
    [self setSortOrderToolbarItemVisible:YES];
    [self setLibrarySearchToolbarItemVisible:YES];
    _optionBarView.hidden = NO;

    _gridVsListSegmentedControl.target = self;
    _gridVsListSegmentedControl.action = @selector(segmentedControlAction:);

    [_libraryAudioViewController presentAudioView];
}

- (void)showMediaSourceLibraryWithSegment:(VLCLibrarySegment)segment
{
    NSParameterAssert(segment == VLCLibraryBrowseSegment || segment == VLCLibraryStreamsSegment);

    [self.navigationStack clear];
    [self setForwardsBackwardsToolbarItemsVisible:YES];
    [self setSortOrderToolbarItemVisible:NO];
    [self setLibrarySearchToolbarItemVisible:NO];
    _optionBarView.hidden = YES;

    if (segment == VLCLibraryBrowseSegment) {
        [_libraryMediaSourceViewController presentBrowseView];
    } else if (segment == VLCLibraryStreamsSegment) {
        [_libraryMediaSourceViewController presentStreamsView];
    }
}

- (IBAction)playlistDoubleClickAction:(id)sender
{
    NSInteger selectedRow = self.playlistTableView.selectedRow;
    if (selectedRow == -1)
        return;

    [[[VLCMain sharedInstance] playlistController] playItemAtIndex:selectedRow];
}

- (IBAction)clearPlaylist:(id)sender
{
    [_playlistController clearPlaylist];
}

- (IBAction)sortPlaylist:(id)sender
{
    if (!_playlistSortingMenuController) {
        _playlistSortingMenuController = [[VLCPlaylistSortingMenuController alloc] init];
    }
    [NSMenu popUpContextMenu:_playlistSortingMenuController.playlistSortingMenu withEvent:[NSApp currentEvent] forView:sender];
}

- (IBAction)sortLibrary:(id)sender
{
    if (!_librarySortingMenuController) {
        _librarySortingMenuController = [[VLCLibrarySortingMenuController alloc] init];
    }
    [NSMenu popUpContextMenu:_librarySortingMenuController.librarySortingMenu withEvent:[NSApp currentEvent] forView:sender];
}

- (IBAction)filterLibrary:(id)sender
{
    [[[VLCMain sharedInstance] libraryController] filterByString:_librarySearchField.stringValue];
}

- (void)clearLibraryFilterString
{
    _librarySearchField.stringValue = @"";
    [self filterLibrary:self];
}

- (IBAction)openMedia:(id)sender
{
    [[[VLCMain sharedInstance] open] openFileGeneric];
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
    [_segmentedTitleControl setSelected:YES forSegment:2];
    [self segmentedControlAction:_segmentedTitleControl];
}

#pragma mark - split view delegation

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMinimumPosition ofSubviewAt:(NSInteger)dividerIndex
{
    switch (dividerIndex) {
        case 0:
            return VLCLibraryWindowMinimalWidth;
        default:
            break;
    }

    return proposedMinimumPosition;
}

- (CGFloat)splitView:(NSSplitView *)splitView constrainMaxCoordinate:(CGFloat)proposedMaximumPosition ofSubviewAt:(NSInteger)dividerIndex
{
    switch (dividerIndex) {
        case 0:
            return splitView.frame.size.width - VLCLibraryWindowMinimalPlaylistWidth;
        default:
            break;
    }

    return proposedMaximumPosition;
}

- (BOOL)splitView:(NSSplitView *)splitView canCollapseSubview:(NSView *)subview
{
    return [subview isEqual:_playlistView];
}

- (BOOL)splitView:(NSSplitView *)splitView shouldCollapseSubview:(NSView *)subview forDoubleClickOnDividerAtIndex:(NSInteger)dividerIndex
{
    return [subview isEqual:_playlistView];
}

- (void)splitViewDidResizeSubviews:(NSNotification *)notification
{
    _lastPlaylistWidthBeforeCollaps = [_playlistView frame].size.width;

    if (![_mainSplitView isSubviewCollapsed:_playlistView]) {
        _playQueueToggle.state = NSControlStateValueOn;
    } else {
        _playQueueToggle.state = NSControlStateValueOff;
    }
}

- (void)togglePlaylist
{
    [_mainSplitView adjustSubviews];
    CGFloat splitViewWidth = _mainSplitView.frame.size.width;
    if ([_mainSplitView isSubviewCollapsed:_playlistView]) {
        [_mainSplitView setPosition:splitViewWidth - _lastPlaylistWidthBeforeCollaps ofDividerAtIndex:0];
        _playQueueToggle.state = NSControlStateValueOn;
    } else {
        [_mainSplitView setPosition:splitViewWidth ofDividerAtIndex:0];
        _playQueueToggle.state = NSControlStateValueOff;
    }
}

- (IBAction)showAndHidePlaylist:(id)sender
{
    [self togglePlaylist];
}

- (IBAction)backwardsNavigationAction:(id)sender
{
    self.videoView.hidden ? [_navigationStack backwards] : [self disableVideoPlaybackAppearance];
}

- (IBAction)forwardsNavigationAction:(id)sender
{
    [_navigationStack forwards];
}

#pragma mark - video output controlling

- (void)setHasActiveVideo:(BOOL)hasActiveVideo
{
    [super setHasActiveVideo:hasActiveVideo];
    hasActiveVideo ? [self enableVideoPlaybackAppearance] : [self disableVideoPlaybackAppearance];
}

- (void)playerStateChanged:(NSNotification *)notification
{
    if (_playlistController.playerController.playerState == VLC_PLAYER_STATE_STOPPED) {
        [self hideControlsBar];
        return;
    }

    if(_playlistController.playerController.playerState == VLC_PLAYER_STATE_PLAYING) {
        [self reopenVideoView];
    }

    if (self.videoView.isHidden) {
        [self showControlsBar];
    }
}

// This handles reopening the video view when the user has closed it.
- (void)reopenVideoView
{
    if(!self.hasActiveVideo) {
        return;
    }

    VLCMediaLibraryMediaItem *mediaItem = [VLCMediaLibraryMediaItem mediaItemForURL:_playlistController.playerController.URLOfCurrentMediaItem];

    if(mediaItem == nil || mediaItem.mediaType != VLC_ML_MEDIA_TYPE_VIDEO) {
        return;
    }

    [self enableVideoPlaybackAppearance];
}

- (void)hideControlsBar
{
    [super hideControlsBar];
    _splitViewBottomConstraintToBottomBar.priority = 1;
    _splitViewBottomConstraintToSuperView.priority = 999;

}

- (void)showControlsBar
{
    [super showControlsBar];
    _splitViewBottomConstraintToBottomBar.priority = 999;
    _splitViewBottomConstraintToSuperView.priority = 1;
}

- (void)presentVideoView
{
    for (NSView *subview in _libraryTargetView.subviews) {
        [subview removeFromSuperview];
    }
    
    NSLog(@"Presenting video view in main library window.");
    
    VLCVoutView *videoView = self.videoView;
    videoView.translatesAutoresizingMaskIntoConstraints = NO;
    videoView.hidden = NO;

    [_libraryTargetView addSubview:videoView];
    NSDictionary *dict = NSDictionaryOfVariableBindings(videoView);
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[videoView(>=572.)]|"
                                                                               options:0
                                                                               metrics:0
                                                                                 views:dict]];
    [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[videoView(>=444.)]|"
                                                                               options:0
                                                                               metrics:0
                                                                                 views:dict]];
}

- (void)enableVideoPlaybackAppearance
{
    [self presentVideoView];

    [self.segmentedTitleControl setHidden:YES];
    [self.optionBarView setHidden:YES];
    [self.forwardsNavigationButton setHidden:YES];
    [self.gridVsListSegmentedControl setHidden:YES];
    [self.librarySortButton setHidden:YES];
    [self.librarySearchField setEnabled:NO];
    [self clearLibraryFilterString];

    // Make sure the back button is visible...
    [self insertToolbarItem:_backwardsToolbarItem inFrontOf:@[]];
    // And repurpose it to hide the video view
    [self.backwardsNavigationButton setEnabled:YES];

    if (self.isInNativeFullscreen && [self hasActiveVideo] && [self fullscreen]) {
        [self hideControlsBar];
        [_fspanel shouldBecomeActive:nil];
    }

    [(VLCLibraryWindowAutohideToolbar *)self.toolbar setAutohide:YES];
    [self hideControlsBar];
}

- (void)disableVideoPlaybackAppearance
{
    [self makeFirstResponder: _playlistTableView];
    [[[VLCMain sharedInstance] voutProvider] updateWindowLevelForHelperWindows: NSNormalWindowLevel];

    // restore alpha value to 1 for the case that macosx-opaqueness is set to < 1
    [self setAlphaValue:1.0];
    self.videoView.hidden = YES;

    [self.segmentedTitleControl setHidden:NO];
    [self.forwardsNavigationButton setHidden:NO];
    [self.gridVsListSegmentedControl setHidden:NO];
    [self.librarySortButton setHidden:NO];
    [self.librarySearchField setEnabled:YES];

    // Reset the back button to navigation state
    [self.backwardsNavigationButton setEnabled:_navigationStack.backwardsAvailable];

    [self setViewForSelectedSegment];

    if (self.isInNativeFullscreen) {
        [self showControlsBar];
        [_fspanel shouldBecomeInactive:nil];
    }

    [(VLCLibraryWindowAutohideToolbar *)self.toolbar setAutohide:NO];
    [self showControlsBar];
}

- (void)mouseMoved:(NSEvent *)o_event
{
    if (!self.videoView.hidden) {
        NSPoint mouseLocation = [o_event locationInWindow];
        NSRect windowRectWithFrame = [self frameRectForContentRect:self.contentView.frame];

        if ([self.contentView mouse:mouseLocation inRect:windowRectWithFrame]) {
            [[NSNotificationCenter defaultCenter] postNotificationName:VLCVideoWindowShouldShowFullscreenController
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

    if (self.videoView.isHidden) {
        [self showControlsBar];
    }
}

- (void)shouldShowFullscreenController:(NSNotification *)aNotification
{
    id currentWindow = [NSApp keyWindow];
    if ([currentWindow respondsToSelector:@selector(hasActiveVideo)] && [currentWindow hasActiveVideo]) {
        if ([currentWindow respondsToSelector:@selector(fullscreen)] && [currentWindow fullscreen] &&
            [currentWindow respondsToSelector:@selector(videoView)] && ![[currentWindow videoView] isHidden]) {
            if ([_playlistController.playerController activeVideoPlayback]) {
                [_fspanel fadeIn];
            }
        }
    }
}

@end
