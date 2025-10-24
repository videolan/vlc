/*****************************************************************************
 * VLCLibraryAbstractGroupingDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#import "VLCLibraryAbstractGroupingDataSource.h"

#import "extensions/NSPasteboardItem+VLCAdditions.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryRepresentedItem.h"

@implementation VLCLibraryAbstractGroupingDataSource

- (NSArray<id<VLCMediaLibraryItemProtocol>> *)backingArray
{
    [self doesNotRecognizeSelector:_cmd];
    return nil;
}

- (VLCMediaLibraryParentGroupType)currentParentType
{
    [self doesNotRecognizeSelector:_cmd];
    return VLCMediaLibraryParentGroupTypeUnknown;
}

- (void)reloadData
{
    [(VLCLibraryCollectionViewFlowLayout *)self.collectionView.collectionViewLayout resetLayout];
    [self.masterTableView reloadData];
    [self.detailTableView reloadData];
    [self.collectionView reloadData];
}

- (NSUInteger)indexOfMediaItem:(const int64_t)libraryId inArray:(NSArray const *)array
{
    return [array indexOfObjectPassingTest:^BOOL(const id<VLCMediaLibraryItemProtocol> findItem,
                                                 const NSUInteger __unused idx,
                                                 BOOL * const __unused stop) {
        NSAssert(findItem != nil, @"Collection should not contain nil items");
        return findItem.libraryID == libraryId;
    }];
}

#pragma mark - table view data source and delegation

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (tableView == self.masterTableView) {
        return self.backingArray.count;
    }

    const NSInteger selectedMasterRow = self.masterTableView.selectedRow;
    const NSInteger backingItemsCount = self.backingArray.count;
    const BOOL selectedRowIsValid = selectedMasterRow > -1 && selectedMasterRow < backingItemsCount;

    if (tableView == self.detailTableView && backingItemsCount > 0 && selectedRowIsValid) {
        return self.backingArray[selectedMasterRow].mediaItems.count;
    }

    return 0;
}

- (id<NSPasteboardWriting>)tableView:(NSTableView *)tableView pasteboardWriterForRow:(NSInteger)row
{
    const id<VLCMediaLibraryItemProtocol> libraryItem = [self libraryItemAtRow:row
                                                                  forTableView:tableView];
    return [NSPasteboardItem pasteboardItemWithLibraryItem:libraryItem];
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    if (tableView == self.masterTableView) {
        return self.backingArray[row];
    }

    const NSInteger selectedMasterRow = self.masterTableView.selectedRow;
    if (tableView == self.detailTableView && selectedMasterRow > -1) {
        const id<VLCMediaLibraryItemProtocol> item = self.backingArray[selectedMasterRow];
        return item.mediaItems[row];
    }

    return nil;
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return NSNotFound;
    }
    return [self indexOfMediaItem:libraryItem.libraryID inArray:self.backingArray];
}

# pragma mark - collection view data source and delegation

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return self.backingArray.count;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    return self.backingArray[section].mediaItems.count;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem * const viewItem =
        [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    const id<VLCMediaLibraryItemProtocol> item =
        [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
    VLCLibraryRepresentedItem * const representedItem =
        [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:self.currentParentType];
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
        const id<VLCMediaLibraryItemProtocol> item = self.backingArray[indexPath.section];
        sectionHeadingView.stringValue = item.displayString;
        return sectionHeadingView;

    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {
        NSString * const viewIdentifier =
            VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier;
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView * const mediaItemDetailView =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:viewIdentifier
                                           forIndexPath:indexPath];
        const id<VLCMediaLibraryItemProtocol> item = [self libraryItemAtIndexPath:indexPath
                                                                forCollectionView:collectionView];
        VLCLibraryRepresentedItem * const representedItem =
            [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:self.currentParentType];

        mediaItemDetailView.representedItem = representedItem;
        mediaItemDetailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return mediaItemDetailView;
    }

    return nil;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    const id<VLCMediaLibraryItemProtocol> item = self.backingArray[indexPath.section];
    return item.mediaItems[indexPath.item];
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if ([libraryItem isKindOfClass:self.backingArray.firstObject.class]) {
        const NSInteger itemIndex = [self indexOfMediaItem:libraryItem.libraryID
                                                   inArray:self.backingArray];
        return itemIndex != NSNotFound ? [NSIndexPath indexPathForItem:0 inSection:itemIndex] : nil;
    }

    __block NSInteger itemInternalMediaItemIndex = NSNotFound;
    const NSInteger itemIndex =
        [self.backingArray indexOfObjectPassingTest:^BOOL(const id<VLCMediaLibraryItemProtocol> item,
                                                          const NSUInteger __unused idx,
                                                          BOOL * const __unused stop) {
            itemInternalMediaItemIndex =
                [self indexOfMediaItem:libraryItem.libraryID inArray:item.mediaItems];
            return itemInternalMediaItemIndex != NSNotFound;
        }];

    return itemIndex != NSNotFound
        ? [NSIndexPath indexPathForItem:itemInternalMediaItemIndex inSection:itemIndex]
        : nil;
}

- (NSArray<VLCLibraryRepresentedItem *> *)representedItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
                                                     forCollectionView:(NSCollectionView *)collectionView
{
    NSMutableArray<VLCLibraryRepresentedItem *> * const representedItems =
        [NSMutableArray arrayWithCapacity:indexPaths.count];

    for (NSIndexPath * const indexPath in indexPaths) {
        const id<VLCMediaLibraryItemProtocol> libraryItem =
            [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        VLCLibraryRepresentedItem * const representedItem =
            [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem
                                                 parentType:self.currentParentType];
        [representedItems addObject:representedItem];
    }

    return representedItems;
}

- (NSString *)supplementaryDetailViewKind
{
    return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
}

@end
