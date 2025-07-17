/*****************************************************************************
 * VLCLibraryFavoritesViewController.m MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import "VLCLibraryFavoritesViewController.h"

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
#import "library/VLCLibraryMasterDetailViewTableViewDelegate.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTwoPaneSplitViewDelegate.h"
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"
#import "library/favorites-library/VLCLibraryFavoritesDataSource.h"
#import "main/VLCMain.h"

@interface VLCLibraryFavoritesViewController ()
{
    VLCLibraryMasterDetailViewTableViewDelegate *_favoritesLibraryTableViewDelegate;
    VLCLibraryTwoPaneSplitViewDelegate *_splitViewDelegate;
    VLCLibraryCollectionViewDelegate *_collectionViewDelegate;
    VLCLibraryCollectionViewFlowLayout *_collectionViewLayout;
    NSArray<NSLayoutConstraint *> *_internalPlaceholderImageViewSizeConstraints;
    
    id<VLCMediaLibraryItemProtocol> _awaitingPresentingLibraryItem;
}

@end

@implementation VLCLibraryFavoritesViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];
    
    if (self) {
        _favoritesLibraryTableViewDelegate = [[VLCLibraryMasterDetailViewTableViewDelegate alloc] init];
        _splitViewDelegate = [[VLCLibraryTwoPaneSplitViewDelegate alloc] init];
        
        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupTableViews];
        [self setupCollectionView];
        [self setupFavoritesDataSource];
        [self setupFavoritesPlaceholderView];
        [self setupFavoritesLibraryViews];
        [self setupNotifications];
    }
    
    return self;
}

- (void)presentFavoritesView
{
    [self updatePresentedFavoritesView];
}

- (void)setupPropertiesFromLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    NSParameterAssert(libraryWindow);
    _favoritesLibraryView = libraryWindow.videoLibraryView;
    _favoritesLibrarySplitView = libraryWindow.videoLibrarySplitView;
    _favoritesLibraryCollectionViewScrollView = libraryWindow.videoLibraryCollectionViewScrollView;
    _favoritesLibraryCollectionView = libraryWindow.videoLibraryCollectionView;
    _favoritesLibraryGroupSelectionTableViewScrollView = libraryWindow.videoLibraryGroupSelectionTableViewScrollView;
    _favoritesLibraryGroupSelectionTableView = libraryWindow.videoLibraryGroupSelectionTableView;
    _favoritesLibraryGroupsTableViewScrollView = libraryWindow.videoLibraryGroupsTableViewScrollView;
    _favoritesLibraryGroupsTableView = libraryWindow.videoLibraryGroupsTableView;
}

- (void)setupTableViews
{
    self.favoritesLibrarySplitView.delegate = _splitViewDelegate;
    [_splitViewDelegate resetDefaultSplitForSplitView:self.favoritesLibrarySplitView];

    NSNib * const tableCellViewNib =
        [[NSNib alloc] initWithNibNamed:NSStringFromClass(VLCLibraryTableCellView.class)
                                 bundle:nil];
    [self.favoritesLibraryGroupsTableView registerNib:tableCellViewNib
                                        forIdentifier:@"VLCLibraryTableViewCellIdentifier"];
    [self.favoritesLibraryGroupSelectionTableView registerNib:tableCellViewNib 
                                                forIdentifier:@"VLCLibraryTableViewCellIdentifier"];
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
    
    NSCollectionView * const collectionView = self.favoritesLibraryCollectionView;
    collectionView.collectionViewLayout = _collectionViewLayout;
    
    _collectionViewDelegate = [[VLCLibraryCollectionViewDelegate alloc] init];
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

- (void)setupFavoritesDataSource
{
    _libraryFavoritesDataSource = [[VLCLibraryFavoritesDataSource alloc] init];
    self.libraryFavoritesDataSource.libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    self.libraryFavoritesDataSource.collectionView = self.favoritesLibraryCollectionView;
    self.libraryFavoritesDataSource.masterTableView = self.favoritesLibraryGroupsTableView;
    self.libraryFavoritesDataSource.detailTableView = self.favoritesLibraryGroupSelectionTableView;
}

- (void)setupFavoritesLibraryViews
{
    _favoritesLibraryGroupsTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
    _favoritesLibraryGroupSelectionTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;

    const NSEdgeInsets defaultInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    _favoritesLibraryCollectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    _favoritesLibraryCollectionViewScrollView.contentInsets = defaultInsets;
    _favoritesLibraryCollectionViewScrollView.scrollerInsets = scrollerInsets;

    _favoritesLibraryGroupsTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _favoritesLibraryGroupsTableViewScrollView.contentInsets = defaultInsets;
    _favoritesLibraryGroupsTableViewScrollView.scrollerInsets = scrollerInsets;
    _favoritesLibraryGroupSelectionTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    _favoritesLibraryGroupSelectionTableViewScrollView.contentInsets = defaultInsets;
    _favoritesLibraryGroupSelectionTableViewScrollView.scrollerInsets = scrollerInsets;
}

- (void)setupFavoritesPlaceholderView
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

- (void)setupNotifications
{
    NSNotificationCenter *notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelUpdated:)
                               name:VLCLibraryModelFavoriteVideoMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelUpdated:)
                               name:VLCLibraryModelFavoriteAudioMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelUpdated:)
                               name:VLCLibraryModelFavoriteAlbumsListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelUpdated:)
                               name:VLCLibraryModelFavoriteArtistsListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelUpdated:)
                               name:VLCLibraryModelFavoriteGenresListReset
                             object:nil];
}

#pragma mark - VLCLibraryAbstractMediaLibrarySegmentViewController

- (NSArray<NSLayoutConstraint *> *)placeholderImageViewSizeConstraints
{
    return _internalPlaceholderImageViewSizeConstraints;
}

- (id<VLCLibraryDataSource>)currentDataSource
{
    return self.libraryFavoritesDataSource;
}

#pragma mark - Public methods

- (void)updatePresentedFavoritesView
{
    self.favoritesLibraryCollectionView.dataSource = self.libraryFavoritesDataSource;
    
    self.favoritesLibraryGroupsTableView.dataSource = self.libraryFavoritesDataSource;
    self.favoritesLibraryGroupsTableView.target = self.libraryFavoritesDataSource;
    self.favoritesLibraryGroupsTableView.delegate = _favoritesLibraryTableViewDelegate;

    self.favoritesLibraryGroupSelectionTableView.dataSource = self.libraryFavoritesDataSource;
    self.favoritesLibraryGroupSelectionTableView.target = self.libraryFavoritesDataSource;
    self.favoritesLibraryGroupSelectionTableView.delegate = _favoritesLibraryTableViewDelegate;
    
    [self.libraryFavoritesDataSource reloadData];
    
    if ([self hasFavoriteItems]) {
        const VLCLibraryViewModeSegment viewModeSegment = VLCLibraryWindowPersistentPreferences.sharedInstance.favoritesLibraryViewMode;
        [self presentFavoritesLibraryView:viewModeSegment];
    } else if (self.libraryFavoritesDataSource.libraryModel.filterString.length > 0) {
        [self.libraryWindow displayNoResultsMessage];
    } else {
        [self presentPlaceholderFavoritesView];
    }
}

- (BOOL)hasFavoriteItems
{
    VLCLibraryModel * const libraryModel = self.libraryFavoritesDataSource.libraryModel;
    return libraryModel.numberOfFavoriteVideoMedia > 0 ||
           libraryModel.numberOfFavoriteAudioMedia > 0 ||
           libraryModel.numberOfFavoriteAlbums > 0 ||
           libraryModel.numberOfFavoriteArtists > 0 ||
           libraryModel.numberOfFavoriteGenres > 0;
}

- (void)presentFavoritesCollectionView
{
    [self.libraryWindow displayLibraryView:self.favoritesLibraryView];
    self.favoritesLibraryCollectionViewScrollView.hidden = NO;
}

- (void)presentPlaceholderFavoritesView
{
    [self.libraryWindow displayLibraryPlaceholderViewWithImage:[NSImage imageNamed:@"placeholder-favorites"]
                                              usingConstraints:self.placeholderImageViewSizeConstraints
                                             displayingMessage:_NS("Your favorite items will appear here.\nMark items as favorites to see them in this view.")];
}

- (void)presentFavoritesLibraryView:(VLCLibraryViewModeSegment)viewModeSegment
{
    [self.libraryWindow displayLibraryView:self.favoritesLibraryView];
    if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        self.favoritesLibrarySplitView.hidden = YES;
        self.favoritesLibraryCollectionViewScrollView.hidden = NO;
    } else if (viewModeSegment == VLCLibraryListViewModeSegment) {
        self.favoritesLibrarySplitView.hidden = NO;
        self.favoritesLibraryCollectionViewScrollView.hidden = YES;
    } else {
        NSAssert(false, @"View mode must be grid or list mode");
    }
}

- (void)presentLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return;
    }

    _awaitingPresentingLibraryItem = libraryItem;

    const VLCLibraryViewModeSegment viewModeSegment = VLCLibraryWindowPersistentPreferences.sharedInstance.favoritesLibraryViewMode;

    if (viewModeSegment == VLCLibraryGridViewModeSegment) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(presentLibraryItemWaitForCollectionViewDataSourceFinished:)
                                                   name:VLCLibraryFavoritesDataSourceDisplayedCollectionChangedNotification
                                                 object:self.libraryFavoritesDataSource];
    } else if (viewModeSegment == VLCLibraryListViewModeSegment) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(presentLibraryItemWaitForTableViewDataSourceFinished:)
                                                   name:VLCLibraryFavoritesDataSourceDisplayedCollectionChangedNotification
                                                 object:self.libraryFavoritesDataSource];
    } else {
        NSAssert(false, @"View mode must be grid or list mode");
    }
}

- (void)presentLibraryItemWaitForCollectionViewDataSourceFinished:(NSNotification *)notification
{
    [NSNotificationCenter.defaultCenter removeObserver:self
                                                  name:VLCLibraryFavoritesDataSourceDisplayedCollectionChangedNotification
                                                object:self.libraryFavoritesDataSource];

    _awaitingPresentingLibraryItem = nil;
}

- (void)presentLibraryItemWaitForTableViewDataSourceFinished:(NSNotification *)notification
{
    [NSNotificationCenter.defaultCenter removeObserver:self
                                                  name:VLCLibraryFavoritesDataSourceDisplayedCollectionChangedNotification
                                                object:self.libraryFavoritesDataSource];

    const NSInteger rowForLibraryItem = [self.libraryFavoritesDataSource rowForLibraryItem:_awaitingPresentingLibraryItem];
    if (rowForLibraryItem != NSNotFound) {
        NSIndexSet * const indexSet = [NSIndexSet indexSetWithIndex:rowForLibraryItem];
        [self.favoritesLibraryGroupsTableView selectRowIndexes:indexSet byExtendingSelection:NO];
        [self.favoritesLibraryGroupsTableView scrollRowToVisible:rowForLibraryItem];
    }

    _awaitingPresentingLibraryItem = nil;
}

#pragma mark - Notification handlers

- (void)libraryModelUpdated:(NSNotification *)notification
{
    NSParameterAssert(notification);
    if (self.libraryWindow.librarySegmentType == VLCLibraryFavoritesSegmentType) {
        [self updatePresentedFavoritesView];
    }
}

@end
