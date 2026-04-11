/*****************************************************************************
 * VLCLibraryVideoViewController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#import "VLCLibraryVideoViewController.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibrarySegment.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"

#import "library/audio-library/VLCLibraryAudioViewController.h"

#import "library/home-library/VLCLibraryHomeViewVideoContainerViewDataSource.h"

#import "library/playlist-library/VLCLibraryPlaylistViewController.h"

#import "library/video-library/VLCLibraryShowsDataSource.h"
#import "library/video-library/VLCLibraryVideoDataSource.h"
#import "library/video-library/VLCLibraryVideoTableViewDelegate.h"
#import "library/video-library/VLCLibraryMoviesDataSource.h"

#import "main/VLCMain.h"

#import "windows/video/VLCVoutView.h"
#import "windows/video/VLCMainVideoViewController.h"

@interface VLCLibraryVideoViewController ()
{
    VLCLibraryVideoTableViewDelegate *_videoLibraryTableViewDelegate;
    VLCLibraryCollectionViewDelegate *_collectionViewDelegate;
    VLCLibraryCollectionViewFlowLayout *_collectionViewLayout;

    id<VLCMediaLibraryItemProtocol> _awaitingPresentingLibraryItem;

    NSArray<NSLayoutConstraint *> *_internalPlaceholderImageViewSizeConstraints;
}
@end

@implementation VLCLibraryVideoViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];

    if(self) {
        _videoLibraryTableViewDelegate = [[VLCLibraryVideoTableViewDelegate alloc] init];

        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupTableView];
        [self setupCollectionView];
        [self setupVideoPlaceholderView];
        [self setupVideoLibraryViews];

        NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelVideoMediaListReset
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelVideoMediaItemDeleted
                                 object:nil];

        NSString * const videoMediaResetLongLoadStartNotification = [VLCLibraryModelVideoMediaListReset stringByAppendingString:VLCLongNotificationNameStartSuffix];
        NSString * const videoMediaResetLongLoadFinishNotification = [VLCLibraryModelVideoMediaListReset stringByAppendingString:VLCLongNotificationNameFinishSuffix];
        NSString * const videoMediaDeletedLongLoadStartNotification = [VLCLibraryModelVideoMediaItemDeleted stringByAppendingString:VLCLongNotificationNameStartSuffix];
        NSString * const videoMediaDeletedLongLoadFinishNotification = [VLCLibraryModelVideoMediaItemDeleted stringByAppendingString:VLCLongNotificationNameFinishSuffix];

        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadStarted:)
                                   name:videoMediaResetLongLoadStartNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadFinished:)
                                   name:videoMediaResetLongLoadFinishNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadStarted:)
                                   name:videoMediaDeletedLongLoadStartNotification
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelLongLoadFinished:)
                                   name:videoMediaDeletedLongLoadFinishNotification
                                 object:nil];
    }

    return self;
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    NSParameterAssert(libraryWindow);
    _videoLibraryView = libraryWindow.videoLibraryView;
    _videoLibraryCollectionViewScrollView = libraryWindow.videoLibraryCollectionViewScrollView;
    _videoLibraryCollectionView = libraryWindow.videoLibraryCollectionView;

    _videoLibraryTableViewScrollView = libraryWindow.videoLibraryGroupSelectionTableViewScrollView;
    libraryWindow.videoLibrarySplitView.hidden = YES;
    libraryWindow.videoLibraryGroupsTableViewScrollView.hidden = YES;

    [_videoLibraryTableViewScrollView removeFromSuperview];
    _videoLibraryTableViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    [_videoLibraryView addSubview:_videoLibraryTableViewScrollView];
    [NSLayoutConstraint activateConstraints:@[
        [_videoLibraryTableViewScrollView.topAnchor constraintEqualToAnchor:_videoLibraryView.topAnchor],
        [_videoLibraryTableViewScrollView.leadingAnchor constraintEqualToAnchor:_videoLibraryView.leadingAnchor],
        [_videoLibraryTableViewScrollView.trailingAnchor constraintEqualToAnchor:_videoLibraryView.trailingAnchor],
        [_videoLibraryTableViewScrollView.bottomAnchor constraintEqualToAnchor:_videoLibraryView.bottomAnchor],
    ]];
}

- (void)setupTableView
{
    NSNib * const tableCellViewNib =
        [[NSNib alloc] initWithNibNamed:NSStringFromClass(VLCLibraryTableCellView.class)
                                 bundle:nil];
    [self.videoLibraryTableView registerNib:tableCellViewNib
                              forIdentifier:@"VLCVideoLibraryTableViewCellIdentifier"];
}

- (void)setupVideoDataSource
{
    _libraryVideoDataSource = [[VLCLibraryVideoDataSource alloc] init];
    self.libraryVideoDataSource.libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    self.libraryVideoDataSource.tableView = self.videoLibraryTableView;
    self.libraryVideoDataSource.collectionView = self.videoLibraryCollectionView;
}

- (void)setupShowsDataSource
{
    _libraryShowsDataSource = [[VLCLibraryShowsDataSource alloc] init];
    self.libraryShowsDataSource.libraryModel =
        VLCMain.sharedInstance.libraryController.libraryModel;
    self.libraryShowsDataSource.collectionView = self.videoLibraryCollectionView;
    self.libraryShowsDataSource.detailTableView = self.videoLibraryTableView;
}

- (void)setupMoviesDataSource
{
    _libraryMoviesDataSource = [[VLCLibraryMoviesDataSource alloc] init];
    self.libraryMoviesDataSource.libraryModel =
        VLCMain.sharedInstance.libraryController.libraryModel;
    self.libraryMoviesDataSource.tableView = self.videoLibraryTableView;
    self.libraryMoviesDataSource.collectionView = self.videoLibraryCollectionView;
}

- (void)setupCollectionView
{
    _collectionViewLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];

    const CGFloat collectionItemSpacing = VLCLibraryUIUnits.collectionViewItemSpacing;
    const NSEdgeInsets collectionViewSectionInset = VLCLibraryUIUnits.collectionViewSectionInsets;
    _collectionViewLayout.headerReferenceSize = VLCLibraryCollectionViewSupplementaryElementView.defaultHeaderSize;
    _collectionViewLayout.minimumLineSpacing = collectionItemSpacing;
    _collectionViewLayout.minimumInteritemSpacing = collectionItemSpacing;
    _collectionViewLayout.sectionInset = collectionViewSectionInset;

    NSCollectionView * const collectionView = self.videoLibraryCollectionView;
    collectionView.collectionViewLayout = _collectionViewLayout;

    _collectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
    _collectionViewDelegate.itemsAspectRatio = VLCLibraryCollectionViewItemAspectRatioVideoItem;
    _collectionViewDelegate.staticItemSize = VLCLibraryCollectionViewItem.defaultVideoItemSize;
    collectionView.delegate = _collectionViewDelegate;

    [collectionView registerClass:VLCLibraryCollectionViewItem.class
            forItemWithIdentifier:VLCLibraryCellIdentifier];

    [collectionView registerClass:VLCLibraryCollectionViewSupplementaryElementView.class
       forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                   withIdentifier:VLCLibrarySupplementaryElementViewIdentifier];

    NSString * const mediaItemSupplementaryDetailViewString =
        NSStringFromClass(VLCLibraryCollectionViewMediaItemSupplementaryDetailView.class);
    NSNib * const mediaItemSupplementaryDetailViewNib =
        [[NSNib alloc] initWithNibNamed:mediaItemSupplementaryDetailViewString bundle:nil];
    
    [collectionView registerNib:mediaItemSupplementaryDetailViewNib
     forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                 withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier];
}

- (void)setupVideoPlaceholderView
{
    _internalPlaceholderImageViewSizeConstraints = @[
        [NSLayoutConstraint constraintWithItem:self.placeholderImageView
                                     attribute:NSLayoutAttributeWidth
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:182.f],
        [NSLayoutConstraint constraintWithItem:self.placeholderImageView
                                     attribute:NSLayoutAttributeHeight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:nil
                                     attribute:NSLayoutAttributeNotAnAttribute
                                    multiplier:0.f
                                      constant:114.f],
    ];
}

- (void)setupVideoLibraryViews
{
    _videoLibraryTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;

    const NSEdgeInsets defaultInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _videoLibraryCollectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _videoLibraryCollectionViewScrollView.contentInsets = defaultInsets;
    _videoLibraryCollectionViewScrollView.scrollerInsets = scrollerInsets;

    _videoLibraryTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _videoLibraryTableViewScrollView.contentInsets = defaultInsets;
    _videoLibraryTableViewScrollView.scrollerInsets = scrollerInsets;
}

#pragma mark - Show the video library view

- (NSArray<NSLayoutConstraint *> *)placeholderImageViewSizeConstraints
{
    return _internalPlaceholderImageViewSizeConstraints;
}

- (id<VLCLibraryDataSource>)currentDataSource
{
    const NSInteger librarySegmentType = self.libraryWindow.librarySegmentType;
    if (librarySegmentType == VLCLibraryVideoSegmentType) {
        return self.libraryVideoDataSource;
    } else if (librarySegmentType == VLCLibraryShowsVideoSubSegmentType) {
        return self.libraryShowsDataSource;
    } else if (librarySegmentType == VLCLibraryMoviesVideoSubSegmentType) {
        return self.libraryMoviesDataSource;
    } else {
        return nil;
    }
}

- (void)updatePresentedVideoLibraryView
{
    _libraryShowsDataSource = nil;
    _libraryMoviesDataSource = nil;
    [self setupVideoDataSource];
    self.videoLibraryCollectionView.dataSource = self.libraryVideoDataSource;

    self.videoLibraryTableView.dataSource = self.libraryVideoDataSource;
    self.videoLibraryTableView.target = self.libraryVideoDataSource;
    self.videoLibraryTableView.delegate = _videoLibraryTableViewDelegate;

    [self.libraryVideoDataSource reloadData];

    const BOOL anyVideoMedia = self.libraryVideoDataSource.libraryModel.numberOfVideoMedia > 0;
    if (anyVideoMedia) {
        const VLCLibraryViewModeSegment viewModeSegment = VLCLibraryWindowPersistentPreferences.sharedInstance.videoLibraryViewMode;
        [self presentVideoLibraryView:viewModeSegment];
    } else if (self.libraryVideoDataSource.libraryModel.filterString.length > 0) {
        [self.libraryWindow displayNoResultsMessage];
    } else {
        [self presentPlaceholderVideoLibraryView];
    }
}

- (void)updatePresentedShowsLibraryView
{
    _libraryVideoDataSource = nil;
    _libraryMoviesDataSource = nil;
    [self setupShowsDataSource];
    self.videoLibraryCollectionView.dataSource = self.libraryShowsDataSource;

    self.videoLibraryTableView.dataSource = self.libraryShowsDataSource;
    self.videoLibraryTableView.target = self.libraryShowsDataSource;
    self.videoLibraryTableView.delegate = _videoLibraryTableViewDelegate;

    [self.libraryShowsDataSource reloadData];

    const BOOL anyShows = self.libraryShowsDataSource.libraryModel.listOfShows.count > 0;
    if (anyShows) {
        const VLCLibraryViewModeSegment viewModeSegment = VLCLibraryWindowPersistentPreferences.sharedInstance.showsLibraryViewMode;
        [self presentVideoLibraryView:viewModeSegment];
    } else if (self.libraryShowsDataSource.libraryModel.filterString.length > 0) {
        [self.libraryWindow displayNoResultsMessage];
    } else {
        [self presentPlaceholderVideoLibraryView];
    }
}

- (void)presentVideoView
{
    [self updatePresentedVideoLibraryView];
}

- (void)updatePresentedMoviesLibraryView
{
    _libraryVideoDataSource = nil;
    _libraryShowsDataSource = nil;
    [self setupMoviesDataSource];
    self.videoLibraryCollectionView.dataSource = self.libraryMoviesDataSource;

    self.videoLibraryTableView.dataSource = self.libraryMoviesDataSource;
    self.videoLibraryTableView.target = self.libraryMoviesDataSource;
    self.videoLibraryTableView.delegate = _videoLibraryTableViewDelegate;

    [self.libraryMoviesDataSource reloadData];

    const BOOL anyMovies = self.libraryMoviesDataSource.libraryModel.numberOfMovies > 0;
    if (anyMovies) {
        const VLCLibraryViewModeSegment viewModeSegment = VLCLibraryWindowPersistentPreferences.sharedInstance.moviesLibraryViewMode;
        [self presentVideoLibraryView:viewModeSegment];
    } else if (self.libraryMoviesDataSource.libraryModel.filterString.length > 0) {
        [self.libraryWindow displayNoResultsMessage];
    } else {
        [self presentPlaceholderVideoLibraryView];
    }
}

- (void)presentMoviesView
{
    [self updatePresentedMoviesLibraryView];
}

- (void)presentShowsView
{
    [self updatePresentedShowsLibraryView];
}

- (void)presentPlaceholderVideoLibraryView
{
    [self.libraryWindow displayLibraryPlaceholderViewWithImage:[NSImage imageNamed:@"placeholder-video"]
                                              usingConstraints:self.placeholderImageViewSizeConstraints
                                             displayingMessage:_NS("Your videos will appear here.\nGo to the Browse section to add videos you love.")];
}

- (void)presentVideoLibraryView:(VLCLibraryViewModeSegment)viewModeSegment
{
    [self.libraryWindow displayLibraryView:self.videoLibraryView];
    if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        self.videoLibraryTableViewScrollView.hidden = YES;
        self.videoLibraryCollectionViewScrollView.hidden = NO;
    } else if (viewModeSegment == VLCLibraryListViewModeSegment) {
        self.videoLibraryTableViewScrollView.hidden = NO;
        self.videoLibraryCollectionViewScrollView.hidden = YES;
    } else {
        NSAssert(false, @"View mode must be grid or list mode");
    }
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    NSParameterAssert(aNotification);
    VLCLibraryModel *model = VLCMain.sharedInstance.libraryController.libraryModel;
    const NSUInteger videoCount = model.numberOfVideoMedia;
    const NSUInteger showsCount = model.numberOfShows;

    if (self.libraryWindow.librarySegmentType == VLCLibraryVideoSegmentType &&
        ((videoCount == 0 && ![self.libraryTargetView.subviews containsObject:self.emptyLibraryView]) ||
         (videoCount > 0 && ![self.libraryTargetView.subviews containsObject:_videoLibraryView])) &&
        !self.libraryWindow.embeddedVideoPlaybackActive) {

        [self updatePresentedVideoLibraryView];
    } else if (self.libraryWindow.librarySegmentType == VLCLibraryShowsVideoSubSegmentType &&
         ((showsCount == 0 && ![self.libraryTargetView.subviews containsObject:self.emptyLibraryView]) ||
          (showsCount > 0 && ![self.libraryTargetView.subviews containsObject:_videoLibraryView])) &&
         !self.libraryWindow.embeddedVideoPlaybackActive) {

         [self updatePresentedShowsLibraryView];
     } else if (self.libraryWindow.librarySegmentType == VLCLibraryMoviesVideoSubSegmentType &&
         ((model.numberOfMovies == 0 && ![self.libraryTargetView.subviews containsObject:self.emptyLibraryView]) ||
          (model.numberOfMovies > 0 && ![self.libraryTargetView.subviews containsObject:_videoLibraryView])) &&
         !self.libraryWindow.embeddedVideoPlaybackActive) {

         [self updatePresentedMoviesLibraryView];
     }
}

- (void)presentLibraryItemWaitForCollectionViewDataSourceFinished:(NSNotification *)notification
{
    [NSNotificationCenter.defaultCenter removeObserver:self
                                                  name:VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification
                                                object:self.libraryVideoDataSource];

    NSIndexPath * const indexPath = [self.libraryVideoDataSource indexPathForLibraryItem:_awaitingPresentingLibraryItem];
    if (indexPath != nil) {
        NSSet * const indexPathSet = [NSSet setWithObject:indexPath];
        [self.videoLibraryCollectionView selectItemsAtIndexPaths:indexPathSet scrollPosition:NSCollectionViewScrollPositionTop];
        [self.videoLibraryCollectionView scrollToItemsAtIndexPaths:indexPathSet scrollPosition:NSCollectionViewScrollPositionTop];
    }

    _awaitingPresentingLibraryItem = nil;
}

- (void)presentLibraryItemWaitForTableViewDataSourceFinished:(NSNotification *)notification
{
    [NSNotificationCenter.defaultCenter removeObserver:self
                                                  name:VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification
                                             object:self.libraryVideoDataSource];

    const NSInteger rowForLibraryItem = [self.libraryVideoDataSource rowForLibraryItem:_awaitingPresentingLibraryItem];
    if (rowForLibraryItem != NSNotFound) {
        NSIndexSet * const indexSet = [NSIndexSet indexSetWithIndex:rowForLibraryItem];
        [self.videoLibraryTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        [self.videoLibraryTableView scrollRowToVisible:rowForLibraryItem];
    }

    _awaitingPresentingLibraryItem = nil;
}

- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return;
    }

    _awaitingPresentingLibraryItem = libraryItem;

     const VLCLibraryViewModeSegment viewModeSegment = VLCLibraryWindowPersistentPreferences.sharedInstance.videoLibraryViewMode;

    if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(presentLibraryItemWaitForCollectionViewDataSourceFinished:)
                                               name:VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification
                                             object:self.libraryVideoDataSource];

    } else if (viewModeSegment == VLCLibraryListViewModeSegment) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(presentLibraryItemWaitForTableViewDataSourceFinished:)
                                               name:VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification
                                             object:self.libraryVideoDataSource];

    } else {
        NSAssert(false, @"View mode must be grid or list mode");
    }
}

- (void)libraryModelLongLoadStarted:(NSNotification *)notification
{
    if (self.connected) {
        [self.libraryVideoDataSource disconnect];
    }
    [self.libraryWindow showLoadingOverlay];
}

- (void)libraryModelLongLoadFinished:(NSNotification *)notification
{
    if (self.connected) {
        [self.libraryVideoDataSource connect];
    }
    [self.libraryWindow hideLoadingOverlay];
}

@end
