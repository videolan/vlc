/*****************************************************************************
 * VLCLibraryPlaylistViewController.m: MacOS X interface module
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

#import "VLCLibraryPlaylistViewController.h"

#import "extensions/NSView+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryMasterDetailViewTableViewDelegate.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryTwoPaneSplitViewDelegate.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "library/audio-library/VLCLibraryAudioViewController.h"

#import "library/playlist-library/VLCLibraryPlaylistDataSource.h"

#import "library/video-library/VLCLibraryVideoViewController.h"

#import "main/VLCMain.h"


#import "windows/video/VLCMainVideoViewController.h"

@interface VLCLibraryPlaylistViewController ()
{
    NSArray<NSLayoutConstraint *> *_internalPlaceholderImageViewSizeConstraints;
}
@end

@implementation VLCLibraryPlaylistViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];

    if (self) {
        _dataSource = [[VLCLibraryPlaylistDataSource alloc] init];
        _splitViewDelegate = [[VLCLibraryTwoPaneSplitViewDelegate alloc] init];

        [self setupPlaylistCollectionView];
        [self setupPlaylistTableView];
        [self setupPlaylistLibraryContainerView];
        [self setupPlaylistPlaceholderView];

        NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelPlaylistAdded
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelPlaylistDeleted:)
                                   name:VLCLibraryModelPlaylistDeleted
                                 object:nil];

        NSString * const playlistListResetLongLoadStartNotification = [VLCLibraryModelPlaylistAdded stringByAppendingString:VLCLongNotificationNameStartSuffix];
        NSString * const playlistListResetLongLoadFinishNotification = [VLCLibraryModelPlaylistAdded stringByAppendingString:VLCLongNotificationNameFinishSuffix];
        NSString * const playlistDeletedLongLoadStartNotification = [VLCLibraryModelPlaylistDeleted stringByAppendingString:VLCLongNotificationNameStartSuffix];
        NSString * const playlistDeletedLongLoadFinishNotification = [VLCLibraryModelPlaylistDeleted stringByAppendingString:VLCLongNotificationNameFinishSuffix];

        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadStarted:)
                                   name:playlistListResetLongLoadStartNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadFinished:)
                                   name:playlistListResetLongLoadFinishNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadStarted:)
                                   name:playlistDeletedLongLoadStartNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadFinished:)
                                   name:playlistDeletedLongLoadFinishNotification
                                 object:nil];
    }

    return self;
}

- (void)setupPlaylistCollectionView
{
    _libraryView = [[NSView alloc] init];
    _collectionViewScrollView = 
        [[NSScrollView alloc] initWithFrame:self.libraryWindow.libraryTargetView.frame];
    _collectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
    _collectionView = [[VLCLibraryCollectionView alloc] init];

    _collectionViewScrollView.documentView = _collectionView;
    _collectionViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _collectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _collectionViewScrollView.contentInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    _collectionViewScrollView.scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _collectionView.delegate = _collectionViewDelegate;
    _collectionView.collectionViewLayout = VLCLibraryCollectionViewFlowLayout.standardLayout;
    _collectionView.selectable = YES;
    _collectionView.allowsMultipleSelection = NO;
    _collectionView.allowsEmptySelection = YES;

    self.dataSource.collectionViews = @[self.collectionView];
}

- (void)setupPlaylistTableView
{
    _masterTableViewScrollView = [[NSScrollView alloc] init];
    _detailTableViewScrollView = [[NSScrollView alloc] init];
    _tableViewDelegate = [[VLCLibraryMasterDetailViewTableViewDelegate alloc] init];
    _masterTableView = [[VLCLibraryTableView alloc] init];
    _detailTableView = [[VLCLibraryTableView alloc] init];
    _listViewSplitView = [[NSSplitView alloc] init];

    self.masterTableViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.detailTableViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.listViewSplitView.translatesAutoresizingMaskIntoConstraints = NO;
    self.masterTableView.translatesAutoresizingMaskIntoConstraints = NO;
    self.detailTableView.translatesAutoresizingMaskIntoConstraints = NO;

    const NSEdgeInsets defaultInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    self.masterTableViewScrollView.hasHorizontalScroller = NO;
    self.masterTableViewScrollView.borderType = NSNoBorder;
    self.masterTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    self.masterTableViewScrollView.contentInsets = defaultInsets;
    self.masterTableViewScrollView.scrollerInsets = scrollerInsets;

    self.detailTableViewScrollView.hasHorizontalScroller = NO;
    self.detailTableViewScrollView.borderType = NSNoBorder;
    self.detailTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    self.detailTableViewScrollView.contentInsets = defaultInsets;
    self.detailTableViewScrollView.scrollerInsets = scrollerInsets;

    self.masterTableViewScrollView.documentView = self.masterTableView;
    self.detailTableViewScrollView.documentView = self.detailTableView;

    self.listViewSplitView.vertical = YES;
    self.listViewSplitView.dividerStyle = NSSplitViewDividerStyleThin;
    self.listViewSplitView.delegate = self.splitViewDelegate;
    [self.listViewSplitView addArrangedSubview:self.masterTableViewScrollView];
    [self.listViewSplitView addArrangedSubview:self.detailTableViewScrollView];

    NSTableColumn * const masterColumn = [[NSTableColumn alloc] initWithIdentifier:@"playlists"];
    NSTableColumn * const detailColumn =
        [[NSTableColumn alloc] initWithIdentifier:@"selectedPlaylist"];

    [self.masterTableView addTableColumn:masterColumn];
    [self.detailTableView addTableColumn:detailColumn];

    NSNib * const tableCellViewNib =
        [[NSNib alloc] initWithNibNamed:NSStringFromClass(VLCLibraryTableCellView.class)
                                 bundle:nil];
    [self.masterTableView registerNib:tableCellViewNib
                        forIdentifier:@"VLCLibraryTableViewCellIdentifier"];
    [self.detailTableView registerNib:tableCellViewNib
                        forIdentifier:@"VLCLibraryTableViewCellIdentifier"];

    self.masterTableView.headerView = nil;
    self.detailTableView.headerView = nil;

    self.masterTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
    self.detailTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;

    self.masterTableView.delegate = self.tableViewDelegate;
    self.detailTableView.delegate = self.tableViewDelegate;

    self.dataSource.masterTableView = self.masterTableView;
    self.dataSource.detailTableView = self.detailTableView;

    self.masterTableView.dataSource = self.dataSource;
    self.detailTableView.dataSource = self.dataSource;
}

- (void)setupPlaylistPlaceholderView
{
    _internalPlaceholderImageViewSizeConstraints = @[
        [NSLayoutConstraint constraintWithItem:self.libraryWindow.placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
        [NSLayoutConstraint constraintWithItem:self.libraryWindow.placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:149.f],
    ];
}

- (NSArray<NSLayoutConstraint *> *)placeholderImageViewSizeConstraints
{
    return _internalPlaceholderImageViewSizeConstraints;
}

- (id<VLCLibraryDataSource>)currentDataSource
{
    return self.dataSource;
}

- (void)presentPlaceholderPlaylistLibraryView
{
    const vlc_ml_playlist_type_t playlistType = self.dataSource.playlistType;
    NSString *placeholderPlaylistsString = nil;
    switch (playlistType) {
        case VLC_ML_PLAYLIST_TYPE_ALL:
            placeholderPlaylistsString =
                _NS("Your playlists will appear here.\n"
                    "Go to the Browse section to add playlists you love.");
            break;
        case VLC_ML_PLAYLIST_TYPE_AUDIO:
        case VLC_ML_PLAYLIST_TYPE_AUDIO_ONLY:
            placeholderPlaylistsString =
                _NS("Your music playlists will appear here.\n"
                    "Go to the Browse section to add playlists you love.");
            break;
        case VLC_ML_PLAYLIST_TYPE_VIDEO:
        case VLC_ML_PLAYLIST_TYPE_VIDEO_ONLY:
            placeholderPlaylistsString =
                _NS("Your video playlists will appear here.\n"
                    "Go to the Browse section to add playlists you love.");
            break;
    }

    [self.libraryWindow displayLibraryPlaceholderViewWithImage:[NSImage imageNamed:@"placeholder-group2"]
                                              usingConstraints:self.placeholderImageViewSizeConstraints
                                             displayingMessage:placeholderPlaylistsString];
}

- (void)presentPlaylistLibraryView
{
    [self.libraryWindow displayLibraryView:self.libraryView];
    const VLCLibraryViewModeSegment viewModeSegment =
        VLCLibraryWindowPersistentPreferences.sharedInstance.playlistLibraryViewMode;

    if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        self.listViewSplitView.hidden = YES;
        self.collectionViewScrollView.hidden = NO;
    } else {
        self.listViewSplitView.hidden = NO;
        self.collectionViewScrollView.hidden = YES;
        [self.splitViewDelegate resetDefaultSplitForSplitView:self.listViewSplitView];
    }
}

- (void)setupPlaylistLibraryContainerView
{
    self.collectionViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.listViewSplitView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.libraryView addSubview:self.collectionViewScrollView];
    [self.libraryView addSubview:self.listViewSplitView];

    [self.collectionViewScrollView applyConstraintsToFillSuperview];
    [self.listViewSplitView applyConstraintsToFillSuperview];
}

- (void)updatePresentedView
{
    const vlc_ml_playlist_type_t playlistType = self.dataSource.playlistType;
    VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    if ([libraryModel numberOfPlaylistsOfType:playlistType] > 0) {
        [self presentPlaylistLibraryView];
    } else if (self.dataSource.libraryModel.filterString.length > 0) {
        [self.libraryWindow displayNoResultsMessage];
    } else {
        [self presentPlaceholderPlaylistLibraryView];
    }
}

- (void)presentPlaylistsView
{
    [self updatePresentedView];
}

- (void)presentPlaylistsViewForPlaylistType:(enum vlc_ml_playlist_type_t)playlistType
{
    self.dataSource.playlistType = playlistType;
    [self presentPlaylistsView];
}

- (void)libraryModelUpdated:(NSNotification *)notification
{
    VLCLibraryModel * const model = VLCMain.sharedInstance.libraryController.libraryModel;
    const vlc_ml_playlist_type_t playlistType = self.dataSource.playlistType;
    const size_t numberOfPlaylists = [model numberOfPlaylistsOfType:playlistType];

    if (self.libraryWindow.librarySegmentType == VLCLibraryPlaylistsSegmentType &&
        ((numberOfPlaylists == 0 && ![self.libraryWindow.libraryTargetView.subviews containsObject:self.libraryWindow.emptyLibraryView]) ||
         (numberOfPlaylists > 0 && ![self.libraryWindow.libraryTargetView.subviews containsObject:_collectionViewScrollView])) &&
        !self.libraryWindow.embeddedVideoPlaybackActive) {

        [self updatePresentedView];
    }
}

- (void)libraryModelPlaylistDeleted:(NSNotification *)notification
{
    NSParameterAssert(notification);
    if (self.libraryWindow.librarySegmentType == VLCLibraryPlaylistsSegmentType &&
        !self.libraryWindow.embeddedVideoPlaybackActive) {
        [self updatePresentedView];
    }
}

- (void)libraryModelLongLoadStarted:(NSNotification *)notification
{
    if (self.connected) {
        [self.dataSource disconnect];
    }
    [self.libraryWindow showLoadingOverlay];
}

- (void)libraryModelLongLoadFinished:(NSNotification *)notification
{
    if (self.connected) {
        [self.dataSource connect];
    }
    [self.libraryWindow hideLoadingOverlay];
}

@end
