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
#import "library/VLCLibraryAudioDataSource.h"
#import "library/VLCLibraryVideoDataSource.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibrarySortingMenuController.h"
#import "library/VLCLibraryAlbumTableCellView.h"

#import "media-source/VLCMediaSourceBaseDataSource.h"

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
const CGFloat VLCLibraryWindowLargePlaylistRowHeight = 60.;
const CGFloat VLCLibraryWindowSmallPlaylistRowHeight = 45.;
const CGFloat VLCLibraryWindowSmallRowHeight = 24.;
const CGFloat VLCLibraryWindowLargeRowHeight = 50.;
const CGFloat VLCLibraryWindowDefaultPlaylistWidth = 340.;

@interface VLCLibraryWindow () <VLCDragDropTarget, NSSplitViewDelegate>
{
    VLCPlaylistDataSource *_playlistDataSource;
    VLCLibraryVideoDataSource *_libraryVideoDataSource;
    VLCLibraryAudioDataSource *_libraryAudioDataSource;
    VLCLibraryGroupDataSource *_libraryAudioGroupDataSource;
    VLCLibrarySortingMenuController *_librarySortingMenuController;
    VLCMediaSourceBaseDataSource *_mediaSourceDataSource;
    VLCPlaylistSortingMenuController *_playlistSortingMenuController;

    VLCPlaylistController *_playlistController;

    NSRect _windowFrameBeforePlayback;
    CGFloat _lastPlaylistWidthBeforeCollaps;

    VLCFSPanelController *_fspanel;
}
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

@implementation VLCLibraryWindow

- (void)awakeFromNib
{
    VLCMain *mainInstance = [VLCMain sharedInstance];
    _playlistController = [mainInstance playlistController];

    libvlc_int_t *libvlc = vlc_object_instance(getIntf());
    var_AddCallback(libvlc, "intf-toggle-fscontrol", ShowFullscreenController, (__bridge void *)self);
    var_AddCallback(libvlc, "intf-show", ShowController, (__bridge void *)self);

    self.videoView = [[VLCVoutView alloc] initWithFrame:self.mainSplitView.frame];
    self.videoView.hidden = YES;
    self.videoView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:self.videoView];
    [self.contentView addConstraint:[NSLayoutConstraint constraintWithItem:self.videoView attribute:NSLayoutAttributeWidth relatedBy:NSLayoutRelationEqual toItem:self.mainSplitView attribute:NSLayoutAttributeWidth multiplier:1. constant:1.]];
    [self.contentView addConstraint:[NSLayoutConstraint constraintWithItem:self.videoView attribute:NSLayoutAttributeHeight relatedBy:NSLayoutRelationEqual toItem:self.mainSplitView attribute:NSLayoutAttributeHeight multiplier:1. constant:1.]];
    [self.contentView addConstraint:[NSLayoutConstraint constraintWithItem:self.videoView attribute:NSLayoutAttributeCenterX relatedBy:NSLayoutRelationEqual toItem:self.mainSplitView attribute:NSLayoutAttributeCenterX multiplier:1. constant:1.]];
    [self.contentView addConstraint:[NSLayoutConstraint constraintWithItem:self.videoView attribute:NSLayoutAttributeCenterY relatedBy:NSLayoutRelationEqual toItem:self.mainSplitView attribute:NSLayoutAttributeCenterY multiplier:1. constant:1.]];

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
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelAudioMediaListUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelVideoMediaListUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateLibraryRepresentation:)
                               name:VLCLibraryModelRecentMediaListUpdated
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

    if (@available(macOS 10_14, *)) {
        [[NSApplication sharedApplication] addObserver:self
                                            forKeyPath:@"effectiveAppearance"
                                               options:0
                                               context:nil];
    }

    _fspanel = [[VLCFSPanelController alloc] init];
    [_fspanel showWindow:self];

    _segmentedTitleControl.segmentCount = 4;
    [_segmentedTitleControl setTarget:self];
    [_segmentedTitleControl setAction:@selector(segmentedControlAction:)];
    [_segmentedTitleControl setLabel:_NS("Video") forSegment:0];
    [_segmentedTitleControl setLabel:_NS("Music") forSegment:1];
    [_segmentedTitleControl setLabel:_NS("Local Network") forSegment:2];
    [_segmentedTitleControl setLabel:_NS("Internet") forSegment:3];
    [_segmentedTitleControl sizeToFit];
    [_segmentedTitleControl setSelectedSegment:0];

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

    _libraryVideoDataSource = [[VLCLibraryVideoDataSource alloc] init];
    _libraryVideoDataSource.libraryModel = mainInstance.libraryController.libraryModel;
    _libraryVideoDataSource.recentMediaCollectionView = _recentVideoLibraryCollectionView;
    _libraryVideoDataSource.libraryMediaCollectionView = _videoLibraryCollectionView;
    _videoLibraryCollectionView.dataSource = _libraryVideoDataSource;
    _videoLibraryCollectionView.delegate = _libraryVideoDataSource;
    [_videoLibraryCollectionView registerClass:[VLCLibraryCollectionViewItem class] forItemWithIdentifier:VLCLibraryCellIdentifier];
    [_videoLibraryCollectionView registerClass:[VLCLibraryCollectionViewSupplementaryElementView class]
               forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                           withIdentifier:VLCLibrarySupplementaryElementViewIdentifier];
    [(NSCollectionViewFlowLayout *)_videoLibraryCollectionView.collectionViewLayout setHeaderReferenceSize:[VLCLibraryCollectionViewSupplementaryElementView defaultHeaderSize]];
    _recentVideoLibraryCollectionView.dataSource = _libraryVideoDataSource;
    _recentVideoLibraryCollectionView.delegate = _libraryVideoDataSource;
    [_recentVideoLibraryCollectionView registerClass:[VLCLibraryCollectionViewItem class] forItemWithIdentifier:VLCLibraryCellIdentifier];
    [_videoLibraryCollectionView setDraggingSourceOperationMask:NSDragOperationCopy forLocal:NO];

    _libraryAudioDataSource = [[VLCLibraryAudioDataSource alloc] init];
    _libraryAudioDataSource.libraryModel = mainInstance.libraryController.libraryModel;
    _libraryAudioDataSource.collectionSelectionTableView = _audioCollectionSelectionTableView;
    _libraryAudioDataSource.groupSelectionTableView = _audioGroupSelectionTableView;
    _libraryAudioDataSource.segmentedControl = self.audioSegmentedControl;
    _libraryAudioDataSource.collectionView = self.audioLibraryCollectionView;
    [_libraryAudioDataSource setupAppearance];
    _audioCollectionSelectionTableView.dataSource = _libraryAudioDataSource;
    _audioCollectionSelectionTableView.delegate = _libraryAudioDataSource;
    _audioCollectionSelectionTableView.rowHeight = VLCLibraryWindowLargeRowHeight;
    _libraryAudioGroupDataSource = [[VLCLibraryGroupDataSource alloc] init];
    _libraryAudioDataSource.groupDataSource = _libraryAudioGroupDataSource;
    _audioGroupSelectionTableView.dataSource = _libraryAudioGroupDataSource;
    _audioGroupSelectionTableView.delegate = _libraryAudioGroupDataSource;
    _audioGroupSelectionTableView.rowHeight = [VLCLibraryAlbumTableCellView defaultHeight];

    _mediaSourceDataSource = [[VLCMediaSourceBaseDataSource alloc] init];
    _mediaSourceDataSource.collectionView = _mediaSourceCollectionView;
    _mediaSourceDataSource.collectionViewScrollView = _mediaSourceCollectionViewScrollView;
    _mediaSourceDataSource.homeButton = _mediaSourceHomeButton;
    _mediaSourceDataSource.pathControl = _mediaSourcePathControl;
    _mediaSourceDataSource.gridVsListSegmentedControl = _gridVsListSegmentedControl;
    _mediaSourceTableView.rowHeight = VLCLibraryWindowLargeRowHeight;
    _mediaSourceDataSource.tableView = _mediaSourceTableView;
    [_mediaSourceDataSource setupViews];

    self.upNextLabel.font = [NSFont VLClibrarySectionHeaderFont];
    self.upNextLabel.stringValue = _NS("Playlist");
    [self updateColorsBasedOnAppearance];
    self.openMediaButton.title = _NS("Open media...");

    _mainSplitView.delegate = self;
    _lastPlaylistWidthBeforeCollaps = VLCLibraryWindowDefaultPlaylistWidth;

    [self segmentedControlAction:nil];
    [self repeatStateUpdated:nil];
    [self shuffleStateUpdated:nil];
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    if (@available(macOS 10_14, *)) {
        [[NSApplication sharedApplication] removeObserver:self forKeyPath:@"effectiveAppearance"];
    }

    libvlc_int_t *libvlc = vlc_object_instance(getIntf());
    var_DelCallback(libvlc, "intf-toggle-fscontrol", ShowFullscreenController, (__bridge void *)self);
    var_DelCallback(libvlc, "intf-show", ShowController, (__bridge void *)self);
}

#pragma mark - appearance setters

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey,id> *)change
                       context:(void *)context
{
    [self updateColorsBasedOnAppearance];
}

- (void)updateColorsBasedOnAppearance
{
    if (self.contentView.shouldShowDarkAppearance) {
        self.upNextLabel.textColor = [NSColor VLClibraryDarkTitleColor];
        self.upNextSeparator.borderColor = [NSColor VLClibrarySeparatorDarkColor];
        self.clearPlaylistSeparator.borderColor = [NSColor VLClibrarySeparatorDarkColor];
    } else {
        self.upNextLabel.textColor = [NSColor VLClibraryLightTitleColor];
        self.upNextSeparator.borderColor = [NSColor VLClibrarySeparatorLightColor];
        self.clearPlaylistSeparator.borderColor = [NSColor VLClibrarySeparatorLightColor];
    }
}

- (void)updateViewCellDimensionsBasedOnSetting:(NSNotification *)aNotification
{
    _playlistTableView.rowHeight = config_GetInt("macosx-large-text") ? VLCLibraryWindowLargePlaylistRowHeight : VLCLibraryWindowSmallPlaylistRowHeight;
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

- (void)segmentedControlAction:(id)sender
{
    switch (_segmentedTitleControl.selectedSegment) {
        case 0:
            [self showVideoLibrary];
            break;

        case 1:
            [self showAudioLibrary];
            break;

        default:
            [self showMediaSourceAppearance];
            break;
    }
}

- (void)showVideoLibrary
{
    if (_mediaSourceView.superview != nil) {
        [_mediaSourceView removeFromSuperview];
    }
    if (_audioLibraryView.superview != nil) {
        [_audioLibraryView removeFromSuperview];
    }
    if (_videoLibraryStackView.superview == nil) {
        _videoLibraryStackView.translatesAutoresizingMaskIntoConstraints = NO;
        [_libraryTargetView addSubview:_videoLibraryStackView];
        NSDictionary *dict = NSDictionaryOfVariableBindings(_videoLibraryStackView);
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_videoLibraryStackView(>=572.)]|" options:0 metrics:0 views:dict]];
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_videoLibraryStackView(>=444.)]|" options:0 metrics:0 views:dict]];
    }
    [_videoLibraryCollectionView reloadData];
    [_recentVideoLibraryCollectionView reloadData];
    _librarySortButton.hidden = NO;
    _audioSegmentedControl.hidden = YES;
}

- (void)showAudioLibrary
{
    if (_mediaSourceView.superview != nil) {
        [_mediaSourceView removeFromSuperview];
    }
    if (_videoLibraryStackView.superview != nil) {
        [_videoLibraryStackView removeFromSuperview];
    }
    if (_audioLibraryView.superview == nil) {
        _audioLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
        [_libraryTargetView addSubview:_audioLibraryView];
        NSDictionary *dict = NSDictionaryOfVariableBindings(_audioLibraryView);
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_audioLibraryView(>=572.)]|" options:0 metrics:0 views:dict]];
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_audioLibraryView(>=444.)]|" options:0 metrics:0 views:dict]];
    }
    _librarySortButton.hidden = NO;
    _audioSegmentedControl.hidden = NO;

    if (self.gridVsListSegmentedControl.selectedSegment == 0) {
        _audioLibrarySplitView.hidden = YES;
        _audioCollectionViewScrollView.hidden = NO;
        [_libraryAudioDataSource reloadAppearance];
    } else {
        _audioLibrarySplitView.hidden = NO;
        _audioCollectionViewScrollView.hidden = YES;
        [_libraryAudioDataSource reloadAppearance];
        [_audioCollectionSelectionTableView reloadData];
    }
    self.gridVsListSegmentedControl.target = self;
    self.gridVsListSegmentedControl.action = @selector(segmentedControlAction:);
}

- (void)showMediaSourceAppearance
{
    if (_videoLibraryStackView.superview != nil) {
        [_videoLibraryStackView removeFromSuperview];
    }
    if (_audioLibraryView.superview != nil) {
        [_audioLibraryView removeFromSuperview];
    }
    if (_mediaSourceView.superview == nil) {
        _mediaSourceView.translatesAutoresizingMaskIntoConstraints = NO;
        [_libraryTargetView addSubview:_mediaSourceView];
        NSDictionary *dict = NSDictionaryOfVariableBindings(_mediaSourceView);
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_mediaSourceView(>=572.)]|" options:0 metrics:0 views:dict]];
        [_libraryTargetView addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_mediaSourceView(>=444.)]|" options:0 metrics:0 views:dict]];
    }
    _mediaSourceDataSource.mediaSourceMode = _segmentedTitleControl.selectedSegment == 2 ? VLCMediaSourceModeLAN : VLCMediaSourceModeInternet;
    _librarySortButton.hidden = YES;
    _audioSegmentedControl.hidden = YES;
    [_mediaSourceDataSource reloadViews];
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

        for (NSUInteger i = 0; i < valueCount; i++) {
            VLCOpenInputMetadata *inputMetadata;
            char *psz_uri = vlc_path2uri([values[i] UTF8String], "file");
            if (!psz_uri)
                continue;
            inputMetadata = [[VLCOpenInputMetadata alloc] init];
            inputMetadata.MRLString = toNSStr(psz_uri);
            free(psz_uri);
            [metadataArray addObject:inputMetadata];
        }
        [_playlistController addPlaylistItems:metadataArray];

        return YES;
    }

    return NO;
}

#pragma mark - split view delegation

- (CGFloat)splitView:(NSSplitView *)splitView constrainMinCoordinate:(CGFloat)proposedMinimumPosition ofSubviewAt:(NSInteger)dividerIndex
{
    switch (dividerIndex) {
        case 0:
            return VLCLibraryWindowMinimalWidth;
            break;

        case 1:
            return VLCLibraryWindowDefaultPlaylistWidth;
            break;

        default:
            break;
    }

    return proposedMinimumPosition;
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
}

- (void)togglePlaylist
{
    [_mainSplitView adjustSubviews];
    CGFloat splitViewWidth = _mainSplitView.frame.size.width;
    if ([_mainSplitView isSubviewCollapsed:_playlistView]) {
        [_mainSplitView setPosition:splitViewWidth - _lastPlaylistWidthBeforeCollaps ofDividerAtIndex:0];
    } else {
        [_mainSplitView setPosition:splitViewWidth ofDividerAtIndex:0];
    }
}

- (IBAction)showAndHidePlaylist:(id)sender
{
    [self togglePlaylist];
}

#pragma mark - video output controlling

- (void)videoPlaybackWillBeStarted
{
    if (!self.fullscreen)
        _windowFrameBeforePlayback = [self frame];
}

- (void)enableVideoPlaybackAppearance
{
    [_mediaSourceView removeFromSuperviewWithoutNeedingDisplay];
    [_videoLibraryStackView removeFromSuperviewWithoutNeedingDisplay];
    [_audioLibraryView removeFromSuperviewWithoutNeedingDisplay];

    [self.videoView setHidden:NO];

    if (self.nativeFullscreenMode) {
        if ([self hasActiveVideo] && [self fullscreen]) {
            [self hideControlsBar];
            [_fspanel shouldBecomeActive:nil];
        }
    }
}

- (void)disableVideoPlaybackAppearance
{
    if (!self.nonembedded
        && (!self.nativeFullscreenMode || (self.nativeFullscreenMode && !self.fullscreen))
        && _windowFrameBeforePlayback.size.width > 0
        && _windowFrameBeforePlayback.size.height > 0) {

        // only resize back to minimum view of this is still desired final state
        CGFloat f_threshold_height = VLCVideoWindowCommonMinimalHeight + [self.controlsBar height];
        if (_windowFrameBeforePlayback.size.height > f_threshold_height) {
            if ([[VLCMain sharedInstance] isTerminating]) {
                [self setFrame:_windowFrameBeforePlayback display:YES];
            } else {
                [[self animator] setFrame:_windowFrameBeforePlayback display:YES];
            }
        }
    }

    _windowFrameBeforePlayback = NSMakeRect(0, 0, 0, 0);

    [self makeFirstResponder: _playlistTableView];
    [[[VLCMain sharedInstance] voutProvider] updateWindowLevelForHelperWindows: NSNormalWindowLevel];

    // restore alpha value to 1 for the case that macosx-opaqueness is set to < 1
    [self setAlphaValue:1.0];
    [self.videoView setHidden:YES];
    [self segmentedControlAction:nil];

    if (self.nativeFullscreenMode) {
        [self showControlsBar];
        [_fspanel shouldBecomeInactive:nil];
    }
}

#pragma mark - library representation and interaction
- (void)updateLibraryRepresentation:(NSNotification *)aNotification
{
    [_videoLibraryCollectionView reloadData];
    [_recentVideoLibraryCollectionView reloadData];
}

#pragma mark -
#pragma mark respond to core events

- (void)shouldShowController:(NSNotification *)aNotification
{
    [self makeKeyAndOrderFront:nil];
}

- (void)shouldShowFullscreenController:(NSNotification *)aNotification
{
    id currentWindow = [NSApp keyWindow];
    if ([currentWindow respondsToSelector:@selector(hasActiveVideo)] && [currentWindow hasActiveVideo]) {
        if ([currentWindow respondsToSelector:@selector(fullscreen)] && [currentWindow fullscreen] && ![[currentWindow videoView] isHidden]) {
            if ([_playlistController.playerController activeVideoPlayback]) {
                [_fspanel fadeIn];
            }
        }
    }
}

@end

@implementation VLCLibraryWindowController

- (instancetype)initWithLibraryWindow
{
    self = [super initWithWindowNibName:@"VLCLibraryWindow"];
    return self;
}

- (void)windowDidLoad
{
    VLCLibraryWindow *window = (VLCLibraryWindow *)self.window;
    [window setRestorable:NO];
    [window setExcludedFromWindowsMenu:YES];
    [window setAcceptsMouseMovedEvents:YES];
    [window setContentMinSize:NSMakeSize(VLCLibraryWindowMinimalWidth, VLCLibraryWindowMinimalHeight)];
}

@end
