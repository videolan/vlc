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

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryWindow.h"

#import "library/audio-library/VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.h"

#import "extensions/NSPasteboardItem+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

NSString * const VLCLibrarySearchDataSourceDidReloadNotification = @"VLCLibrarySearchDataSourceDidReloadNotification";

#pragma mark - Flattened row model

/**
 * Represents one row in the flattened table view model.
 * A row is either a section header or a media item within a provider's results.
 */
@interface VLCLibrarySearchFlattenedRow : NSObject
@property (readonly) BOOL isHeader;
@property (readonly) NSUInteger providerIndex;
@property (readonly) NSInteger itemIndex; // -1 for header rows
+ (instancetype)headerForProvider:(NSUInteger)providerIndex;
+ (instancetype)itemAtIndex:(NSInteger)index forProvider:(NSUInteger)providerIndex;
@end

@implementation VLCLibrarySearchFlattenedRow

+ (instancetype)headerForProvider:(NSUInteger)providerIndex
{
    VLCLibrarySearchFlattenedRow * const row = [VLCLibrarySearchFlattenedRow new];
    row->_isHeader = YES;
    row->_providerIndex = providerIndex;
    row->_itemIndex = -1;
    return row;
}

+ (instancetype)itemAtIndex:(NSInteger)index forProvider:(NSUInteger)providerIndex
{
    VLCLibrarySearchFlattenedRow * const row = [VLCLibrarySearchFlattenedRow new];
    row->_isHeader = NO;
    row->_providerIndex = providerIndex;
    row->_itemIndex = index;
    return row;
}

@end

#pragma mark - Data source

@interface VLCLibrarySearchDataSource ()
{
    NSArray<VLCLibrarySearchProvider *> *_providers;
    NSArray<NSNumber *> *_visibleProviderIndices;
    NSArray<VLCLibrarySearchFlattenedRow *> *_flattenedRows;
    NSMutableDictionary<NSString *, VLCMediaLibraryDummyItem *> *_cachedProviderParentItems;
    NSUInteger _pendingProviderCount;
    dispatch_queue_t _rebuildQueue;
}
@end

@implementation VLCLibrarySearchDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _providers = [VLCLibrarySearchProvider defaultProviders];
        _visibleProviderIndices = @[];
        _flattenedRows = @[];
        _cachedProviderParentItems = [NSMutableDictionary dictionary];
        _rebuildQueue = dispatch_queue_create("searchDataSourceQueue", DISPATCH_QUEUE_SERIAL);
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
    if (_pendingProviderCount > 0) {
        _pendingProviderCount--;
    }
    if (_pendingProviderCount == 0) {
        _searching = NO;
    }
    if (self.viewMode == VLCLibraryGridViewModeSegment) {
        [self scheduleRebuildVisibleIndices];
    } else {
        [self scheduleRebuildFlattenedRows];
    }
}

- (void)scheduleRebuildVisibleIndices
{
    __weak typeof(self) weakSelf = self;
    dispatch_async(self->_rebuildQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }
        NSArray<NSNumber *> * const newVisibleIndices = [strongSelf visibleProviderIndices];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (strongSelf == nil) {
                return;
            }
            strongSelf->_visibleProviderIndices = newVisibleIndices;
            [strongSelf.collectionView reloadData];
            [strongSelf postDidReloadNotification];
        });
    });
}

- (void)scheduleRebuildFlattenedRows
{
    __weak typeof(self) weakSelf = self;
    dispatch_async(self->_rebuildQueue, ^{
        __strong typeof(weakSelf) strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }
        NSArray<NSNumber *> * const newVisibleIndices = [strongSelf visibleProviderIndices];
        NSArray<VLCLibrarySearchFlattenedRow *> * const newFlattenedRows =
            [strongSelf flattenedRowsForVisibleIndices:newVisibleIndices];

        dispatch_async(dispatch_get_main_queue(), ^{
            if (strongSelf == nil) {
                return;
            }
            strongSelf->_visibleProviderIndices = newVisibleIndices;
            strongSelf->_flattenedRows = newFlattenedRows;
            [strongSelf->_cachedProviderParentItems removeAllObjects];
            [strongSelf.tableView reloadData];
            [strongSelf postDidReloadNotification];
        });
    });
}

- (void)postDidReloadNotification
{
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibrarySearchDataSourceDidReloadNotification
                                                      object:self];
}

- (NSArray<NSNumber *> *)visibleProviderIndices
{
    NSMutableArray<NSNumber *> * const visible = [NSMutableArray array];
    for (NSUInteger i = 0; i < _providers.count; i++) {
        if (_providers[i].results.count > 0) {
            [visible addObject:@(i)];
        }
    }
    return [visible copy];
}

- (NSArray<VLCLibrarySearchFlattenedRow *> *)flattenedRowsForVisibleIndices:(NSArray<NSNumber *> *)visibleIndices
{
    NSMutableArray<VLCLibrarySearchFlattenedRow *> * const rows = [NSMutableArray array];
    for (NSNumber * const indexNumber in visibleIndices) {
        const NSUInteger providerIndex = indexNumber.unsignedIntegerValue;
        VLCLibrarySearchProvider * const provider = _providers[providerIndex];
        const NSUInteger resultCount = provider.results.count;
        [rows addObject:[VLCLibrarySearchFlattenedRow headerForProvider:providerIndex]];
        for (NSUInteger i = 0; i < resultCount; i++) {
            [rows addObject:[VLCLibrarySearchFlattenedRow itemAtIndex:i forProvider:providerIndex]];
        }
    }
    return [rows copy];
}

- (void)reloadData
{
    [_cachedProviderParentItems removeAllObjects];
    if (self.viewMode != VLCLibraryGridViewModeSegment) {
        [self scheduleRebuildFlattenedRows];
    } else {
        [self scheduleRebuildVisibleIndices];
    }
}

#pragma mark - Search

- (void)searchForString:(NSString *)string
{
    _pendingProviderCount = _providers.count;
    _searching = YES;
    for (VLCLibrarySearchProvider * const provider in _providers) {
        [provider searchForString:string];
    }
}

- (void)clearSearch
{
    _pendingProviderCount = 0;
    _searching = NO;
    for (VLCLibrarySearchProvider * const provider in _providers) {
        [provider clearSearch];
    }
    [self reloadData];
}

- (void)setViewMode:(VLCLibraryViewModeSegment)viewMode
{
    if (_viewMode == viewMode) {
        return;
    }
    _viewMode = viewMode;
    if (viewMode == VLCLibraryGridViewModeSegment) {
        [self scheduleRebuildVisibleIndices];
    } else {
        [self scheduleRebuildFlattenedRows];
    }
}

#pragma mark - Data management

- (VLCLibrarySearchProvider *)providerForVisibleSection:(NSInteger)visibleSection
{
    NSParameterAssert(visibleSection >= 0 && (NSUInteger)visibleSection < _visibleProviderIndices.count);
    const NSUInteger providerIndex = _visibleProviderIndices[visibleSection].unsignedIntegerValue;
    return _providers[providerIndex];
}

- (VLCMediaLibraryDummyItem *)dummyParentItemForProvider:(VLCLibrarySearchProvider *)provider
{
    VLCMediaLibraryDummyItem *cached = _cachedProviderParentItems[provider.displayTitle];
    if (cached != nil) {
        return cached;
    }

    NSMutableArray<VLCMediaLibraryMediaItem *> * const mediaItems = [NSMutableArray array];
    for (id<VLCMediaLibraryItemProtocol> item in provider.results) {
        [item iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem *mediaItem) {
            [mediaItems addObject:mediaItem];
        }];
    }
    cached = [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:provider.displayTitle
                                                      withMediaItems:mediaItems];
    _cachedProviderParentItems[provider.displayTitle] = cached;
    return cached;
}

- (NSInteger)flattenedMediaItemOffsetForResultIndex:(NSInteger)resultIndex
                                         inProvider:(VLCLibrarySearchProvider *)provider
{
    NSInteger offset = 0;
    NSArray<id<VLCMediaLibraryItemProtocol>> * const results = provider.results;
    for (NSInteger i = 0; i < resultIndex && (NSUInteger)i < results.count; i++) {
        offset += results[i].mediaItems.count;
    }
    return offset;
}

- (VLCLibraryRepresentedItem *)representedItemForItem:(id<VLCMediaLibraryItemProtocol>)item
                                           atPosition:(NSInteger)position
                                         fromProvider:(VLCLibrarySearchProvider *)provider
{
    VLCMediaLibraryDummyItem * const parentItem = [self dummyParentItemForProvider:provider];
    const NSInteger flattenedPosition =
        [self flattenedMediaItemOffsetForResultIndex:position inProvider:provider];
    return [[VLCLibraryRepresentedItem alloc] initWithItem:item
                                               parentType:VLCMediaLibraryParentGroupTypeUnknown
                                               parentItem:parentItem
                                         positionInParent:flattenedPosition];
}

#pragma mark - VLCLibraryTableViewDataSource

- (VLCMediaLibraryParentGroupType)currentParentType
{
    return VLCMediaLibraryParentGroupTypeUnknown;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    NSParameterAssert(row >= 0 && (NSUInteger)row < _flattenedRows.count);

    VLCLibrarySearchFlattenedRow * const flatRow = _flattenedRows[row];
    NSParameterAssert(!flatRow.isHeader || (flatRow.providerIndex < _providers.count));

    VLCLibrarySearchProvider * const provider = _providers[flatRow.providerIndex];
    return provider.results[flatRow.itemIndex];
}

- (VLCLibraryRepresentedItem *)representedItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    if ([self isHeaderRow:row]) {
        return [self representedItemForHeaderRow:row];
    }

    VLCLibrarySearchFlattenedRow * const flatRow = _flattenedRows[row];
    VLCLibrarySearchProvider * const provider = _providers[flatRow.providerIndex];
    id<VLCMediaLibraryItemProtocol> const item = provider.results[flatRow.itemIndex];

    return [self representedItemForItem:item atPosition:flatRow.itemIndex fromProvider:provider];
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    NSParameterAssert(libraryItem != nil);

    const NSUInteger rowCount = _flattenedRows.count;
    for (NSUInteger i = 0; i < rowCount; i++) {
        VLCLibrarySearchFlattenedRow * const flatRow = _flattenedRows[i];
        if (flatRow.isHeader) {
            continue;
        }

        VLCLibrarySearchProvider * const provider = _providers[flatRow.providerIndex];
        if (flatRow.itemIndex >= 0 && (NSUInteger)flatRow.itemIndex < provider.results.count) {
            const id<VLCMediaLibraryItemProtocol> item = provider.results[flatRow.itemIndex];
            if (item.libraryID == libraryItem.libraryID) {
                return i;
            }
        }
    }

    return NSNotFound;
}

#pragma mark - VLCLibrarySectionedTableViewDataSource

- (BOOL)isHeaderRow:(NSInteger)row
{
    NSParameterAssert(row >= 0 && (NSUInteger)row < _flattenedRows.count);
    return _flattenedRows[row].isHeader;
}

- (NSString *)titleForRow:(NSInteger)row
{
    NSParameterAssert(row >= 0 && (NSUInteger)row < _flattenedRows.count);
    VLCLibrarySearchFlattenedRow * const flatRow = _flattenedRows[row];
    return _providers[flatRow.providerIndex].displayTitle;
}

- (VLCLibraryRepresentedItem *)representedItemForHeaderRow:(NSInteger)row
{
    NSParameterAssert([self isHeaderRow:row]);

    VLCLibrarySearchFlattenedRow * const flatRow = _flattenedRows[row];
    VLCLibrarySearchProvider * const provider = _providers[flatRow.providerIndex];
    NSParameterAssert(provider.results.count > 0);

    VLCMediaLibraryDummyItem * const groupItem = [self dummyParentItemForProvider:provider];
    return [[VLCLibraryRepresentedItem alloc] initWithItem:groupItem
                                                parentType:VLCMediaLibraryParentGroupTypeUnknown];
}

#pragma mark - NSTableViewDataSource

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _flattenedRows.count;
}

- (id<NSPasteboardWriting>)tableView:(NSTableView *)tableView pasteboardWriterForRow:(NSInteger)row
{
    if ([self isHeaderRow:row]) {
        return nil;
    }
    const id<VLCMediaLibraryItemProtocol> libraryItem = [self libraryItemAtRow:row forTableView:tableView];
    return [NSPasteboardItem pasteboardItemWithLibraryItem:libraryItem];
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
        [collectionView makeItemWithIdentifier:VLCLibraryCollectionViewItemIdentifier forIndexPath:indexPath];
    const id<VLCMediaLibraryItemProtocol> item =
        [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
    VLCLibrarySearchProvider * const provider = [self providerForVisibleSection:indexPath.section];
    VLCLibraryRepresentedItem * const representedItem =
        [self representedItemForItem:item atPosition:indexPath.item fromProvider:provider];
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

    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind] ||
               [kind isEqualToString:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind] ||
               [kind isEqualToString:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewSupplementaryDetailView * const detailView =
            (VLCLibraryCollectionViewSupplementaryDetailView *)
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:kind
                                           forIndexPath:indexPath];
        const id<VLCMediaLibraryItemProtocol> item =
            [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        VLCLibrarySearchProvider * const provider = [self providerForVisibleSection:indexPath.section];
        VLCLibraryRepresentedItem * const representedItem =
            [self representedItemForItem:item atPosition:indexPath.item fromProvider:provider];
        detailView.representedItem = representedItem;
        detailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return detailView;
    }

    return nil;
}

#pragma mark - VLCLibraryCollectionViewDataSource

- (NSString *)supplementaryDetailViewKind
{
    if (self.collectionView == nil) {
        return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
    }

    NSIndexPath * const selectedIndexPath = self.collectionView.selectionIndexPaths.anyObject;
    if (selectedIndexPath == nil) {
        return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
    }

    const id<VLCMediaLibraryItemProtocol> item =
        [self libraryItemAtIndexPath:selectedIndexPath forCollectionView:self.collectionView];
    if ([item isKindOfClass:VLCMediaLibraryAlbum.class]) {
        return VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind;
    } else if ([item conformsToProtocol:@protocol(VLCMediaLibraryAudioGroupProtocol)]) {
        return VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind;
    }
    return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
}

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
            VLCLibrarySearchProvider * const provider = [self providerForVisibleSection:indexPath.section];
            VLCLibraryRepresentedItem * const representedItem =
                [self representedItemForItem:libraryItem atPosition:indexPath.item fromProvider:provider];
            [representedItems addObject:representedItem];
        }
    }

    return representedItems;
}

@end
