/*****************************************************************************
 * VLCLibrarySearchDataSource.m: MacOS X interface module
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

#import "VLCLibrarySearchDataSource.h"

#import "VLCLibrarySearchProvider.h"

#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryTableCellView.h"

#import "extensions/NSPasteboardItem+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

@interface VLCLibrarySearchDataSource ()
{
    NSArray<VLCLibrarySearchProvider *> *_providers;
    NSArray<VLCLibrarySearchFlattenedRow *> *_flattenedRows;
}
@end

@implementation VLCLibrarySearchDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _providers = [VLCLibrarySearchProvider defaultProviders];
        [self connect];
    }
    return self;
}

- (void)connect
{
    [NSNotificationCenter.defaultCenter addObserver:self
                                           selector:@selector(providerResultsUpdated:)
                                               name:VLCLibrarySearchProviderResultsUpdated
                                             object:nil];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)providerResultsUpdated:(NSNotification *)notification
{
    [self reloadData];
}

#pragma mark - Search

- (void)searchForString:(NSString *)string
{
    for (VLCLibrarySearchProvider * const provider in _providers) {
        [provider searchForString:string];
    }
}

- (void)clearSearch
{
    for (VLCLibrarySearchProvider * const provider in _providers) {
        [provider clearSearch];
    }
    [self reloadData];
}

#pragma mark - Data management

- (void)reloadData
{
    if (self.collectionView.dataSource == self) {
        [self.collectionView reloadData];
    }
}


#pragma mark - NSCollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return _providers.count;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    VLCLibrarySearchProvider * const provider = _providers[section];
    return provider ? provider.results.count : 0;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem * const viewItem =
        [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    const id<VLCMediaLibraryItemProtocol> item =
        [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
    VLCLibraryRepresentedItem * const representedItem =
        [[VLCLibraryRepresentedItem alloc] initWithItem:item
                                             parentType:VLCMediaLibraryParentGroupTypeSearchResults];
    viewItem.representedItem = representedItem;
    return viewItem;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView
viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
               atIndexPath:(NSIndexPath *)indexPath
{
    if ([kind isEqualToString:NSCollectionElementKindSectionHeader]) {
        VLCLibraryCollectionViewSupplementaryElementView * const sectionHeadingView =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:VLCLibrarySupplementaryElementViewIdentifier
                                           forIndexPath:indexPath];
        VLCLibrarySearchProvider * const provider = _providers[indexPath.section];
        sectionHeadingView.stringValue = provider.displayTitle;
        return sectionHeadingView;

    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView * const detailView =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                                           forIndexPath:indexPath];
        const id<VLCMediaLibraryItemProtocol> item =
            [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        VLCLibraryRepresentedItem * const representedItem =
            [[VLCLibraryRepresentedItem alloc] initWithItem:item
                                                 parentType:VLCMediaLibraryParentGroupTypeSearchResults];
        detailView.representedItem = representedItem;
        detailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return detailView;
    }

    return nil;
}

#pragma mark - VLCLibraryCollectionViewDataSource

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    VLCLibrarySearchProvider * const provider = _providers[indexPath.section];
    if (provider && indexPath.item >= 0 && (NSUInteger)indexPath.item < provider.results.count) {
        return provider.results[indexPath.item];
    }
    return nil;
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return nil;
    }

    for (NSUInteger section = 0; section < _providers.count; section++) {
        VLCLibrarySearchProvider * const provider = _providers[section];
        for (NSUInteger i = 0; i < provider.results.count; i++) {
            if (provider.results[i].libraryID == libraryItem.libraryID) {
                return [NSIndexPath indexPathForItem:i inSection:section];
            }
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
        const id<VLCMediaLibraryItemProtocol> libraryItem =
            [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        if (libraryItem) {
            VLCLibraryRepresentedItem * const representedItem =
                [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem
                                                     parentType:VLCMediaLibraryParentGroupTypeSearchResults];
            [representedItems addObject:representedItem];
        }
    }

    return representedItems;
}

@end
