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
    NSArray<NSNumber *> *_visibleProviderIndices;
    NSArray<VLCLibrarySearchFlattenedRow *> *_flattenedRows;
}
@end

@implementation VLCLibrarySearchDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _providers = [VLCLibrarySearchProvider defaultProviders];
        _visibleProviderIndices = @[];
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

- (void)updateVisibleProviderIndices
{
    NSMutableArray<NSNumber *> * const visible = [NSMutableArray array];
    for (NSUInteger i = 0; i < _providers.count; i++) {
        if (_providers[i].results.count > 0) {
            [visible addObject:@(i)];
        }
    }
    _visibleProviderIndices = [visible copy];
}

- (void)reloadData
{
    [self updateVisibleProviderIndices];

    if (self.collectionView.dataSource == self) {
        [self.collectionView reloadData];
    }
}

- (VLCLibrarySearchProvider *)providerForVisibleSection:(NSInteger)visibleSection
{
    NSParameterAssert(visibleSection >= 0 && (NSUInteger)visibleSection < _visibleProviderIndices.count);
    const NSUInteger providerIndex = _visibleProviderIndices[visibleSection].unsignedIntegerValue;
    return _providers[providerIndex];
}


#pragma mark - NSCollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return _visibleProviderIndices.count;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    VLCLibrarySearchProvider * const provider = [self providerForVisibleSection:section];
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
        VLCLibrarySearchProvider * const provider = [self providerForVisibleSection:indexPath.section];
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
    VLCLibrarySearchProvider * const provider = [self providerForVisibleSection:indexPath.section];
    NSParameterAssert(provider != nil && indexPath.item >= 0 && (NSUInteger)indexPath.item < provider.results.count);
    return provider.results[indexPath.item];
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    NSParameterAssert(libraryItem != nil);

    for (NSUInteger visibleIndex = 0; visibleIndex < _visibleProviderIndices.count; visibleIndex++) {
        const NSUInteger providerIndex = _visibleProviderIndices[visibleIndex].unsignedIntegerValue;
        VLCLibrarySearchProvider * const provider = _providers[providerIndex];
        for (NSUInteger i = 0; i < provider.results.count; i++) {
            if (provider.results[i].libraryID == libraryItem.libraryID) {
                return [NSIndexPath indexPathForItem:i inSection:visibleIndex];
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
