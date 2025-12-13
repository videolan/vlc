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

#import "extensions/NSView+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryCollectionView.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.h"
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
#import "library/VLCLibraryTableView.h"
#import "library/favorites-library/VLCLibraryFavoritesDataSource.h"
#import "library/favorites-library/VLCLibraryFavoritesTableViewDelegate.h"
#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.h"
#import "main/VLCMain.h"

@interface VLCLibraryFavoritesViewController ()
{
    VLCLibraryFavoritesTableViewDelegate *_favoritesLibraryTableViewDelegate;
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
        _favoritesLibraryTableViewDelegate = [[VLCLibraryFavoritesTableViewDelegate alloc] init];
        _splitViewDelegate = [[VLCLibraryTwoPaneSplitViewDelegate alloc] init];
        
        [self setupProperties];
        [self setupTableViews];
        [self setupCollectionView];
        [self setupFavoritesDataSource];
        [self setupFavoritesPlaceholderView];
        [self setupFavoritesLibraryViews];
        [self setupFavoritesLibraryContainerView];
        [self setupNotifications];
    }
    
    return self;
}

- (void)presentFavoritesView
{
    [self updatePresentedFavoritesView];
}

- (void)setupProperties
{
    _favoritesLibraryView = [[NSView alloc] init];
    _favoritesLibrarySplitView = [[NSSplitView alloc] init];
    _favoritesLibraryCollectionViewScrollView = [[NSScrollView alloc] init];
    _favoritesLibraryCollectionView = [[VLCLibraryCollectionView alloc] init];
    _favoritesLibraryGroupSelectionTableViewScrollView = [[NSScrollView alloc] init];
    _favoritesLibraryGroupSelectionTableView = [[VLCLibraryTableView alloc] init];
    _favoritesLibraryGroupsTableViewScrollView = [[NSScrollView alloc] init];
    _favoritesLibraryGroupsTableView = [[VLCLibraryTableView alloc] init];
}

- (void)setupTableViews
{
    self.favoritesLibrarySplitView.delegate = _splitViewDelegate;

    NSTableColumn * const groupsColumn = [[NSTableColumn alloc] initWithIdentifier:@"groups"];
    NSTableColumn * const selectedGroupColumn = [[NSTableColumn alloc] initWithIdentifier:@"selectedGroup"];
    
    [self.favoritesLibraryGroupsTableView addTableColumn:groupsColumn];
    [self.favoritesLibraryGroupSelectionTableView addTableColumn:selectedGroupColumn];

    NSNib * const tableCellViewNib =
        [[NSNib alloc] initWithNibNamed:NSStringFromClass(VLCLibraryTableCellView.class)
                                 bundle:nil];
    [self.favoritesLibraryGroupsTableView registerNib:tableCellViewNib
                                        forIdentifier:VLCLibraryTableCellViewIdentifier];
    [self.favoritesLibraryGroupSelectionTableView registerNib:tableCellViewNib 
                                                forIdentifier:VLCLibraryTableCellViewIdentifier];
    
    // Register album cell view for artist/genre sections
    NSNib * const albumCellViewNib =
        [[NSNib alloc] initWithNibNamed:NSStringFromClass(VLCLibraryAlbumTableCellView.class)
                                 bundle:nil];
    [self.favoritesLibraryGroupSelectionTableView registerNib:albumCellViewNib
                                                forIdentifier:VLCAudioLibraryCellIdentifier];
    
    self.favoritesLibraryGroupsTableView.headerView = nil;
    self.favoritesLibraryGroupSelectionTableView.headerView = nil;
    
    self.favoritesLibraryGroupsTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
    self.favoritesLibraryGroupSelectionTableView.rowHeight = VLCLibraryUIUnits.mediumTableViewRowHeight;
                                                
}

- (void)setupCollectionView
{
    self.favoritesLibraryCollectionViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.favoritesLibraryCollectionView.translatesAutoresizingMaskIntoConstraints = NO;

    self.favoritesLibraryCollectionViewScrollView.hasHorizontalScroller = NO;
    self.favoritesLibraryCollectionViewScrollView.borderType = NSNoBorder;
    self.favoritesLibraryCollectionViewScrollView.automaticallyAdjustsContentInsets = NO;
    self.favoritesLibraryCollectionViewScrollView.contentInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    self.favoritesLibraryCollectionViewScrollView.scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;
    self.favoritesLibraryCollectionViewScrollView.documentView = self.favoritesLibraryCollectionView;

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
    
    collectionView.selectable = YES;
    collectionView.allowsEmptySelection = YES;
    collectionView.allowsMultipleSelection = YES;
    
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
    
    NSString * const audioGroupSupplementaryDetailViewString =
        NSStringFromClass(VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.class);
    NSNib * const audioGroupSupplementaryDetailViewNib =
        [[NSNib alloc] initWithNibNamed:audioGroupSupplementaryDetailViewString bundle:nil];
    
    [collectionView registerNib:audioGroupSupplementaryDetailViewNib
     forSupplementaryViewOfKind:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind
                 withIdentifier:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewIdentifier];
    
    NSString * const mediaListSupplementaryDetailViewString =
        NSStringFromClass(VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.class);
    NSNib * const mediaListSupplementaryDetailViewNib =
        [[NSNib alloc] initWithNibNamed:mediaListSupplementaryDetailViewString bundle:nil];
    
    [collectionView registerNib:mediaListSupplementaryDetailViewNib
     forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind
                 withIdentifier:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewIdentifier];
}

- (void)setupFavoritesDataSource
{
    _libraryFavoritesDataSource = [[VLCLibraryFavoritesDataSource alloc] init];
    self.libraryFavoritesDataSource.libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    self.libraryFavoritesDataSource.collectionView = self.favoritesLibraryCollectionView;
    self.libraryFavoritesDataSource.masterTableView = self.favoritesLibraryGroupsTableView;
    self.libraryFavoritesDataSource.detailTableView = self.favoritesLibraryGroupSelectionTableView;
    
    self.favoritesLibraryCollectionView.dataSource = self.libraryFavoritesDataSource;
    
    self.favoritesLibraryGroupsTableView.dataSource = self.libraryFavoritesDataSource;
    self.favoritesLibraryGroupsTableView.target = self.libraryFavoritesDataSource;
    self.favoritesLibraryGroupsTableView.delegate = _favoritesLibraryTableViewDelegate;

    self.favoritesLibraryGroupSelectionTableView.dataSource = self.libraryFavoritesDataSource;
    self.favoritesLibraryGroupSelectionTableView.target = self.libraryFavoritesDataSource;
    self.favoritesLibraryGroupSelectionTableView.delegate = _favoritesLibraryTableViewDelegate;
}

- (void)setupFavoritesLibraryViews
{
    self.favoritesLibraryView.translatesAutoresizingMaskIntoConstraints = NO;
    self.favoritesLibraryGroupsTableViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.favoritesLibraryGroupSelectionTableViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.favoritesLibrarySplitView.translatesAutoresizingMaskIntoConstraints = NO;

    const NSEdgeInsets defaultInsets = VLCLibraryUIUnits.libraryViewScrollViewContentInsets;
    const NSEdgeInsets scrollerInsets = VLCLibraryUIUnits.libraryViewScrollViewScrollerInsets;

    self.favoritesLibraryGroupsTableViewScrollView.hasHorizontalScroller = NO;
    self.favoritesLibraryGroupsTableViewScrollView.borderType = NSNoBorder;
    self.favoritesLibraryGroupsTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    self.favoritesLibraryGroupsTableViewScrollView.contentInsets = defaultInsets;
    self.favoritesLibraryGroupsTableViewScrollView.scrollerInsets = scrollerInsets;

    self.favoritesLibraryGroupSelectionTableViewScrollView.hasHorizontalScroller = NO;
    self.favoritesLibraryGroupSelectionTableViewScrollView.borderType = NSNoBorder;
    self.favoritesLibraryGroupSelectionTableViewScrollView.automaticallyAdjustsContentInsets = NO;
    self.favoritesLibraryGroupSelectionTableViewScrollView.contentInsets = defaultInsets;
    self.favoritesLibraryGroupSelectionTableViewScrollView.scrollerInsets = scrollerInsets;
    self.favoritesLibraryGroupSelectionTableViewScrollView.hasHorizontalScroller = NO;

    self.favoritesLibraryGroupsTableViewScrollView.documentView = self.favoritesLibraryGroupsTableView;
    self.favoritesLibraryGroupSelectionTableViewScrollView.documentView = self.favoritesLibraryGroupSelectionTableView;

    self.favoritesLibrarySplitView.vertical = YES;
    self.favoritesLibrarySplitView.dividerStyle = NSSplitViewDividerStyleThin;
    self.favoritesLibrarySplitView.delegate = _splitViewDelegate;
    [self.favoritesLibrarySplitView addArrangedSubview:self.favoritesLibraryGroupsTableViewScrollView];
    [self.favoritesLibrarySplitView addArrangedSubview:self.favoritesLibraryGroupSelectionTableViewScrollView];
}

- (void)setupFavoritesLibraryContainerView
{
    self.favoritesLibraryCollectionViewScrollView.translatesAutoresizingMaskIntoConstraints = NO;
    self.favoritesLibrarySplitView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.favoritesLibraryView addSubview:self.favoritesLibraryCollectionViewScrollView];
    [self.favoritesLibraryView addSubview:self.favoritesLibrarySplitView];

    [self.favoritesLibraryCollectionViewScrollView applyConstraintsToFillSuperview];
    [self.favoritesLibrarySplitView applyConstraintsToFillSuperview];
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
    [self.libraryWindow displayLibraryView:self.favoritesLibraryCollectionViewScrollView];
}

- (void)presentPlaceholderFavoritesView
{
    [self.libraryWindow displayLibraryPlaceholderViewWithImage:[NSImage imageNamed:@"placeholder-video"]
                                              usingConstraints:self.placeholderImageViewSizeConstraints
                                             displayingMessage:_NS("Your favorite media will appear here.\nMark media items as favorites to see them in this view.")];
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
        [_splitViewDelegate resetDefaultSplitForSplitView:self.favoritesLibrarySplitView];
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
