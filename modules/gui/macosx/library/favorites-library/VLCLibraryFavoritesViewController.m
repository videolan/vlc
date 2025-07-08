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
#import "library/VLCLibraryUIUnits.h"
#import "library/VLCLibraryWindow.h"
#import "library/favorites-library/VLCLibraryFavoritesDataSource.h"
#import "main/VLCMain.h"

@interface VLCLibraryFavoritesViewController ()
{
    VLCLibraryCollectionViewDelegate *_collectionViewDelegate;
    VLCLibraryCollectionViewFlowLayout *_collectionViewLayout;
    NSArray<NSLayoutConstraint *> *_internalPlaceholderImageViewSizeConstraints;
}

@property (readwrite, weak) NSView *favoritesLibraryView;
@property (readwrite, weak) NSScrollView *favoritesLibraryCollectionViewScrollView;
@property (readwrite, weak) VLCLibraryCollectionView *favoritesLibraryCollectionView;

@end

@implementation VLCLibraryFavoritesViewController

- (instancetype)initWithLibraryWindow:(VLCLibraryWindow *)libraryWindow
{
    self = [super initWithLibraryWindow:libraryWindow];
    
    if (self) {
        [self setupPropertiesFromLibraryWindow:libraryWindow];
        [self setupCollectionView];
        [self setupFavoritesDataSource];
        [self setupFavoritesPlaceholderView];
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
    _favoritesLibraryCollectionViewScrollView = libraryWindow.videoLibraryCollectionViewScrollView;
    _favoritesLibraryCollectionView = libraryWindow.videoLibraryCollectionView;
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
    [self.libraryFavoritesDataSource reloadData];
    
    if ([self hasFavoriteItems]) {
        [self presentFavoritesCollectionView];
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

#pragma mark - Notification handlers

- (void)libraryModelUpdated:(NSNotification *)notification
{
    NSParameterAssert(notification);
    if (self.libraryWindow.librarySegmentType == VLCLibraryFavoritesSegmentType) {
        [self updatePresentedFavoritesView];
    }
}

@end
