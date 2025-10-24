/*****************************************************************************
 * VLCLibraryFavoritesDataSource.m: MacOS X interface module
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

#import "VLCLibraryFavoritesDataSource.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/audio-library/VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.h"

#import "views/VLCImageView.h"

#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"

#import "extensions/NSIndexSet+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSPasteboardItem+VLCAdditions.h"

NSString * const VLCLibraryFavoritesDataSourceDisplayedCollectionChangedNotification = @"VLCLibraryFavoritesDataSourceDisplayedCollectionChangedNotification";

@interface VLCLibraryFavoritesDataSource ()
{
    NSArray<VLCMediaLibraryMediaItem *> *_favoriteVideoMediaArray;
    NSArray<VLCMediaLibraryMediaItem *> *_favoriteAudioMediaArray;
    NSArray<VLCMediaLibraryAlbum *> *_favoriteAlbumsArray;
    NSArray<VLCMediaLibraryArtist *> *_favoriteArtistsArray;
    NSArray<VLCMediaLibraryGenre *> *_favoriteGenresArray;
    VLCLibraryCollectionViewFlowLayout *_collectionViewFlowLayout;
    NSArray<NSNumber *> *_visibleSectionMapping; // Maps visible sections to VLCLibraryFavoritesSection values
    NSMutableDictionary<NSNumber *, NSArray<id<VLCMediaLibraryItemProtocol>> *> *_flattenedRowMappings; // Maps row indices to items for flattened artist/genre views
}

@end

@implementation VLCLibraryFavoritesDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _flattenedRowMappings = [NSMutableDictionary dictionary];
        [self connect];
    }
    return self;
}

- (NSArray<id<VLCMediaLibraryItemProtocol>> *)arrayForSection:(VLCLibraryFavoritesSection)section
{
    switch (section) {
        case VLCLibraryFavoritesSectionVideoMedia:
            return _favoriteVideoMediaArray;
        case VLCLibraryFavoritesSectionAudioMedia:
            return _favoriteAudioMediaArray;
        case VLCLibraryFavoritesSectionAlbums:
            return _favoriteAlbumsArray;
        case VLCLibraryFavoritesSectionArtists:
            return _favoriteArtistsArray;
        case VLCLibraryFavoritesSectionGenres:
            return _favoriteGenresArray;
        default:
            return @[];
    }
}

- (NSString *)titleForSection:(VLCLibraryFavoritesSection)section
{
    switch (section) {
        case VLCLibraryFavoritesSectionVideoMedia:
            return _NS("Favorite Videos");
        case VLCLibraryFavoritesSectionAudioMedia:
            return _NS("Favorite Music");
        case VLCLibraryFavoritesSectionAlbums:
            return _NS("Favorite Albums");
        case VLCLibraryFavoritesSectionArtists:
            return _NS("Favorite Artists");
        case VLCLibraryFavoritesSectionGenres:
            return _NS("Favorite Genres");
        default:
            return @"";
    }
}

- (VLCMediaLibraryParentGroupType)parentTypeForSection:(VLCLibraryFavoritesSection)section
{
    switch (section) {
        case VLCLibraryFavoritesSectionVideoMedia:
            return VLCMediaLibraryParentGroupTypeVideoLibrary;
        case VLCLibraryFavoritesSectionAudioMedia:
            return VLCMediaLibraryParentGroupTypeAudioLibrary;
        case VLCLibraryFavoritesSectionAlbums:
            return VLCMediaLibraryParentGroupTypeAlbum;
        case VLCLibraryFavoritesSectionArtists:
            return VLCMediaLibraryParentGroupTypeArtist;
        case VLCLibraryFavoritesSectionGenres:
            return VLCMediaLibraryParentGroupTypeGenre;
        default:
            return VLCMediaLibraryParentGroupTypeUnknown;
    }
}

- (VLCLibraryFavoritesSection)sectionForVisibleIndex:(NSInteger)visibleIndex
{
    if (visibleIndex < 0 || (NSUInteger)visibleIndex >= _visibleSectionMapping.count) {
        return VLCLibraryFavoritesSectionCount; // Invalid
    }
    return (VLCLibraryFavoritesSection)[_visibleSectionMapping[visibleIndex] integerValue];
}

- (BOOL)isAudioGroupSection:(VLCLibraryFavoritesSection)section
{
    // Only artists and genres show the audio group view (list of albums)
    return section == VLCLibraryFavoritesSectionArtists ||
           section == VLCLibraryFavoritesSectionGenres;
}

- (BOOL)isMediaListSection:(VLCLibraryFavoritesSection)section
{
    // Albums show the media list view (list of tracks)
    return section == VLCLibraryFavoritesSectionAlbums;
}

- (NSInteger)visibleIndexForSection:(VLCLibraryFavoritesSection)section
{
    NSNumber * const sectionNumber = @(section);
    NSUInteger index = [_visibleSectionMapping indexOfObject:sectionNumber];
    return index == NSNotFound ? -1 : (NSInteger)index;
}

- (void)updateVisibleSectionMapping
{
    NSMutableArray<NSNumber *> * const visibleSections = [NSMutableArray array];
    
    for (NSUInteger i = 0; i < VLCLibraryFavoritesSectionCount; i++) {
        NSArray * const sectionArray = [self arrayForSection:i];
        if (sectionArray.count > 0) {
            [visibleSections addObject:@(i)];
        }
    }
    
    _visibleSectionMapping = [visibleSections copy];
}

- (NSUInteger)indexOfMediaItem:(const int64_t)libraryId inArray:(NSArray const *)array
{
    return [array indexOfObjectPassingTest:^BOOL(id<VLCMediaLibraryItemProtocol> const findMediaItem, const NSUInteger __unused idx, BOOL * const __unused stop) {
        NSAssert(findMediaItem != nil, @"Collection should not contain nil media items");
        return findMediaItem.libraryID == libraryId;
    }];
}

- (id<VLCMediaLibraryItemProtocol>)createGroupDescriptorForSection:(VLCLibraryFavoritesSection)section
{
    NSArray * const sectionArray = [self arrayForSection:section];
    
    // For video and audio media sections, the array already contains VLCMediaLibraryMediaItem objects
    if (section == VLCLibraryFavoritesSectionVideoMedia || 
        section == VLCLibraryFavoritesSectionAudioMedia) {
        return [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:[self titleForSection:section]
                                                        withMediaItems:sectionArray];
    }
    
    // For albums, artists, and genres, we need to extract media items from each item
    NSMutableArray<VLCMediaLibraryMediaItem *> * const mediaItems = [NSMutableArray array];
    for (id<VLCMediaLibraryItemProtocol> item in sectionArray) {
        NSArray<VLCMediaLibraryMediaItem *> * const itemMediaItems = item.mediaItems;
        if (itemMediaItems) {
            [mediaItems addObjectsFromArray:itemMediaItems];
        }
    }
    
    return [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:[self titleForSection:section]
                                                    withMediaItems:[mediaItems copy]];
}

- (NSArray *)buildFlattenedArrayForAudioGroupSection:(VLCLibraryFavoritesSection)section
{
    NSParameterAssert([self isAudioGroupSection:section]);
    NSArray<id<VLCMediaLibraryAudioGroupProtocol>> * const sectionArray = 
        (NSArray<id<VLCMediaLibraryAudioGroupProtocol>> *)[self arrayForSection:section];
    NSMutableArray * const flattenedArray = [NSMutableArray array];
    
    for (id<VLCMediaLibraryAudioGroupProtocol> audioGroup in sectionArray) {
        [flattenedArray addObject:audioGroup];
        [flattenedArray addObjectsFromArray:audioGroup.albums];
    }
    
    return [flattenedArray copy];
}

#pragma mark - Notification handlers

- (void)libraryModelFavoriteVideoMediaListReset:(NSNotification * const)notification
{
    [self reloadData];
}

- (void)libraryModelFavoriteAudioMediaListReset:(NSNotification * const)notification
{
    [self reloadData];
}

- (void)libraryModelFavoriteAlbumsListReset:(NSNotification * const)notification
{
    [self reloadData];
}

- (void)libraryModelFavoriteArtistsListReset:(NSNotification * const)notification
{
    [self reloadData];
}

- (void)libraryModelFavoriteGenresListReset:(NSNotification * const)notification
{
    [self reloadData];
}

#pragma mark - VLCLibraryDataSource

- (void)connect
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelFavoriteVideoMediaListReset:)
                               name:VLCLibraryModelFavoriteVideoMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelFavoriteAudioMediaListReset:)
                               name:VLCLibraryModelFavoriteAudioMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelFavoriteAlbumsListReset:)
                               name:VLCLibraryModelFavoriteAlbumsListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelFavoriteArtistsListReset:)
                               name:VLCLibraryModelFavoriteArtistsListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelFavoriteGenresListReset:)
                               name:VLCLibraryModelFavoriteGenresListReset
                             object:nil];

    [self reloadData];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)reloadData
{
    if (!_libraryModel) {
        return;
    }

    [_collectionViewFlowLayout resetLayout];

    _favoriteVideoMediaArray = [self.libraryModel listOfFavoriteVideoMedia];
    _favoriteAudioMediaArray = [self.libraryModel listOfFavoriteAudioMedia];
    _favoriteAlbumsArray = [self.libraryModel listOfFavoriteAlbums];
    _favoriteArtistsArray = [self.libraryModel listOfFavoriteArtists];
    _favoriteGenresArray = [self.libraryModel listOfFavoriteGenres];

    [self updateVisibleSectionMapping];
    
    [_flattenedRowMappings removeAllObjects];

    if (self.masterTableView.dataSource == self) {
        [self.masterTableView reloadData];
    }
    if (self.detailTableView.dataSource == self) {
        [self.detailTableView reloadData];
    }
    if (self.collectionView.dataSource == self) {
        [self.collectionView reloadData];
    }
    
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryFavoritesDataSourceDisplayedCollectionChangedNotification
                                                      object:self
                                                    userInfo:nil];
}

#pragma mark - NSTableViewDataSource (Master-Detail View)

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (tableView == self.masterTableView) {
        return _visibleSectionMapping.count;
    } else if (tableView == self.detailTableView && self.masterTableView.selectedRow > -1) {
        const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:self.masterTableView.selectedRow];
        
        // For artist and genre sections, return the flattened count
        if ([self isAudioGroupSection:section]) {
            NSArray * const flattenedArray = _flattenedRowMappings[@(section)];
            if (flattenedArray) {
                return flattenedArray.count;
            }
            NSArray * const newFlattenedArray = [self buildFlattenedArrayForAudioGroupSection:section];
            _flattenedRowMappings[@(section)] = newFlattenedArray;
            return newFlattenedArray.count;
        }
        
        return [self arrayForSection:section].count;
    }
    
    return 0;
}

- (id<NSPasteboardWriting>)tableView:(NSTableView *)tableView pasteboardWriterForRow:(NSInteger)row
{
    const id<VLCMediaLibraryItemProtocol> libraryItem = [self libraryItemAtRow:row forTableView:tableView];
    return [NSPasteboardItem pasteboardItemWithLibraryItem:libraryItem];
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    if (tableView == self.masterTableView) {
        // For master table, return a group descriptor object
        NSParameterAssert(row >= 0 && (NSUInteger)row < _visibleSectionMapping.count);
        return [self createGroupDescriptorForSection:[self sectionForVisibleIndex:row]];
    } else if (tableView == self.detailTableView && self.masterTableView.selectedRow > -1) {
        const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:self.masterTableView.selectedRow];
        
        // For artist and genre sections, use the flattened array
        if ([self isAudioGroupSection:section]) {
            NSArray<id<VLCMediaLibraryItemProtocol>> * const flattenedArray = _flattenedRowMappings[@(section)];
            NSParameterAssert(flattenedArray && (NSUInteger)row < flattenedArray.count);
            return flattenedArray[row];
        } else {
            // For other sections, use the regular array
            NSArray<id<VLCMediaLibraryItemProtocol>> * const sectionArray = [self arrayForSection:section];
            NSParameterAssert(sectionArray && row >= 0 && (NSUInteger)row < sectionArray.count);
            return sectionArray[row];
        }
    }
    
    return nil;
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return NSNotFound;
    }
    
    // Search through all visible sections
    for (NSNumber * const sectionNumber in _visibleSectionMapping) {
        const VLCLibraryFavoritesSection section = (VLCLibraryFavoritesSection)[sectionNumber integerValue];
        NSArray * const sectionArray = [self arrayForSection:section];
        const NSInteger index = [self indexOfMediaItem:libraryItem.libraryID inArray:sectionArray];
        if (index != NSNotFound) {
            return index;
        }
    }
    
    return NSNotFound;
}

- (VLCMediaLibraryParentGroupType)currentParentType
{
    if (self.masterTableView.selectedRow > -1) {
        const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:self.masterTableView.selectedRow];
        return [self parentTypeForSection:section];
    }
    return VLCMediaLibraryParentGroupTypeVideoLibrary; // Default fallback
}

#pragma mark - NSCollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return _visibleSectionMapping.count;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    const VLCLibraryFavoritesSection favoritesSection = [self sectionForVisibleIndex:section];
    return [self arrayForSection:favoritesSection].count;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem * const viewItem =
        [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    
    const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:indexPath.section];
    const VLCMediaLibraryParentGroupType parentType = [self parentTypeForSection:section];
    const id<VLCMediaLibraryItemProtocol> item =
        [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
    
    VLCLibraryRepresentedItem * const representedItem =
        [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:parentType];
    viewItem.representedItem = representedItem;
    return viewItem;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView
viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
               atIndexPath:(NSIndexPath *)indexPath
{
    if([kind isEqualToString:NSCollectionElementKindSectionHeader]) {
        VLCLibraryCollectionViewSupplementaryElementView * const sectionHeadingView =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:VLCLibrarySupplementaryElementViewIdentifier
                                           forIndexPath:indexPath];
        
        const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:indexPath.section];
        sectionHeadingView.stringValue = [self titleForSection:section];
        return sectionHeadingView;

    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {
        const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:indexPath.section];
        if ([self isAudioGroupSection:section]) {
            // Redirect to audio group supplementary view
            return [self collectionView:collectionView 
                viewForSupplementaryElementOfKind:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind 
                               atIndexPath:indexPath];
        } else if ([self isMediaListSection:section]) {
            // Redirect to media list supplementary view
            return [self collectionView:collectionView 
                viewForSupplementaryElementOfKind:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind 
                               atIndexPath:indexPath];
        }
        
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView * const mediaItemSupplementaryDetailView = 
            [collectionView makeSupplementaryViewOfKind:kind 
                                         withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind 
                                           forIndexPath:indexPath];
        
        const id<VLCMediaLibraryItemProtocol> item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:self.currentParentType];

        mediaItemSupplementaryDetailView.representedItem = representedItem;
        mediaItemSupplementaryDetailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return mediaItemSupplementaryDetailView;
    } else if ([kind isEqualToString:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewAudioGroupSupplementaryDetailView * const audioGroupSupplementaryDetailView = 
            [collectionView makeSupplementaryViewOfKind:kind 
                                         withIdentifier:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewIdentifier 
                                           forIndexPath:indexPath];
        
        const id<VLCMediaLibraryItemProtocol> item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:indexPath.section];
        const VLCMediaLibraryParentGroupType parentType = [self parentTypeForSection:section];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:parentType];

        audioGroupSupplementaryDetailView.representedItem = representedItem;
        audioGroupSupplementaryDetailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return audioGroupSupplementaryDetailView;
    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewMediaItemListSupplementaryDetailView * const mediaListSupplementaryDetailView = 
            [collectionView makeSupplementaryViewOfKind:kind 
                                         withIdentifier:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewIdentifier 
                                           forIndexPath:indexPath];
        
        const id<VLCMediaLibraryItemProtocol> item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:indexPath.section];
        const VLCMediaLibraryParentGroupType parentType = [self parentTypeForSection:section];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:parentType];

        mediaListSupplementaryDetailView.representedItem = representedItem;
        mediaListSupplementaryDetailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return mediaListSupplementaryDetailView;
    }

    return nil;
}

#pragma mark - VLCLibraryCollectionViewDataSource

- (VLCLibraryCollectionViewItemAspectRatio)collectionView:(NSCollectionView *)collectionView
                                    aspectRatioForSection:(NSInteger)section
{    
    if ([self sectionForVisibleIndex:section] == VLCLibraryFavoritesSectionVideoMedia) {
        return VLCLibraryCollectionViewItemAspectRatioVideoItem;
    }
    return VLCLibraryCollectionViewItemAspectRatioDefaultItem;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:indexPath.section];
    NSArray * const sectionArray = [self arrayForSection:section];
    
    if (indexPath.item >= 0 && (NSUInteger)indexPath.item < sectionArray.count) {
        return sectionArray[indexPath.item];
    }
    
    return nil;
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return nil;
    }
    
    // Search through all visible sections
    for (NSUInteger visibleIndex = 0; visibleIndex < _visibleSectionMapping.count; visibleIndex++) {
        const VLCLibraryFavoritesSection section = (VLCLibraryFavoritesSection)[_visibleSectionMapping[visibleIndex] integerValue];
        NSArray * const sectionArray = [self arrayForSection:section];
        const NSInteger itemIndex = [self indexOfMediaItem:libraryItem.libraryID inArray:sectionArray];
        if (itemIndex != NSNotFound) {
            return [NSIndexPath indexPathForItem:itemIndex inSection:visibleIndex];
        }
    }
    
    return nil;
}

- (NSArray<VLCLibraryRepresentedItem *> *)representedItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
                                                     forCollectionView:(NSCollectionView *)collectionView
{
    NSMutableArray<VLCLibraryRepresentedItem *> * const representedItems =
        [NSMutableArray arrayWithCapacity:indexPaths.count];

    for (NSIndexPath * const indexPath in indexPaths) {
        const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:indexPath.section];
        const VLCMediaLibraryParentGroupType parentType = [self parentTypeForSection:section];
        const id<VLCMediaLibraryItemProtocol> libraryItem =
            [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        
        if (libraryItem) {
            VLCLibraryRepresentedItem * const representedItem =
                [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem parentType:parentType];
            [representedItems addObject:representedItem];
        }
    }

    return representedItems;
}

- (NSString *)supplementaryDetailViewKind
{
    if (self.collectionView.selectionIndexPaths.count > 0) {
        NSIndexPath * const firstIndexPath = self.collectionView.selectionIndexPaths.anyObject;
        const VLCLibraryFavoritesSection section = [self sectionForVisibleIndex:firstIndexPath.section];
        
        if ([self isAudioGroupSection:section]) {
            return VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind;
        } else if ([self isMediaListSection:section]) {
            return VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind;
        }
    }
    
    return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
}

@end
