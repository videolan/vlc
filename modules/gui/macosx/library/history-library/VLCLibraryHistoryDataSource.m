/*****************************************************************************
 * VLCLibraryHistoryDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCLibraryHistoryDataSource.h"

#import "extensions/NSPasteboardItem+VLCAdditions.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

@interface VLCLibraryHistoryDataSource ()
{
    BOOL _connected;
    NSCalendar *_calendar;
    NSDate *_now;
    // A header has item == NSNotFound; media rows use their collection index path.
    NSArray<NSIndexPath *> *_tableRows;
}
@end

@implementation VLCLibraryHistoryDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _sections = @[];
        _tableRows = @[];
    }
    return self;
}

- (void)dealloc
{
    [self disconnect];
}

- (void)setLibraryModel:(VLCLibraryModel *)libraryModel
{
    const BOOL reconnect = _connected;
    [self disconnect];
    _libraryModel = libraryModel;
    if (reconnect) {
        [self connect];
    }
}

- (void)connect
{
    if (_connected) {
        return;
    }
    _connected = YES;
    NSNotificationCenter * const center = NSNotificationCenter.defaultCenter;
    [center addObserver:self
               selector:@selector(libraryModelChanged:)
                   name:VLCLibraryModelHistoryMediaListReset
                 object:self.libraryModel];
    [center addObserver:self
               selector:@selector(libraryModelChanged:)
                   name:VLCLibraryModelAllCachesDropped
                 object:self.libraryModel];
    [self reloadData];
}

- (void)disconnect
{
    _connected = NO;
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)libraryModelChanged:(NSNotification *)notification
{
    [self reloadData];
}

- (NSSet<NSNumber *> *)selectedMediaIDs
{
    NSMutableSet<NSNumber *> * const selectedIDs = NSMutableSet.set;
    if (self.tableViewActive) {
        [self.tableView.selectedRowIndexes enumerateIndexesUsingBlock:^(NSUInteger row, BOOL * const __unused stop) {
            id<VLCMediaLibraryItemProtocol> const item = [self libraryItemAtRow:row forTableView:self.tableView];
            if (item != nil) {
                [selectedIDs addObject:@(item.libraryID)];
            }
        }];
    } else {
        for (NSIndexPath * const indexPath in self.collectionView.selectionIndexPaths) {
            id<VLCMediaLibraryItemProtocol> const item =
                [self libraryItemAtIndexPath:indexPath forCollectionView:self.collectionView];
            if (item != nil) {
                [selectedIDs addObject:@(item.libraryID)];
            }
        }
    }
    return selectedIDs.copy;
}

- (void)restoreSelectionWithMediaIDs:(NSSet<NSNumber *> *)selectedIDs
{
    NSMutableSet<NSIndexPath *> * const selectedPaths = NSMutableSet.set;
    NSMutableIndexSet * const selectedRows = NSMutableIndexSet.indexSet;
    [_tableRows enumerateObjectsUsingBlock:^(NSIndexPath *indexPath, NSUInteger row, BOOL * const __unused stop) {
        if (indexPath.item == NSNotFound) {
            return;
        }
        VLCMediaLibraryMediaItem * const item = self.sections[indexPath.section].mediaItems[indexPath.item];
        if ([selectedIDs containsObject:@(item.libraryID)]) {
            [selectedPaths addObject:indexPath];
            [selectedRows addIndex:row];
        }
    }];
    NSCollectionView * const collectionView = self.collectionView;
    if (![collectionView.selectionIndexPaths isEqualToSet:selectedPaths]) {
        [collectionView deselectItemsAtIndexPaths:collectionView.selectionIndexPaths];
        [collectionView selectItemsAtIndexPaths:selectedPaths scrollPosition:NSCollectionViewScrollPositionNone];
    }
    if (![self.tableView.selectedRowIndexes isEqualToIndexSet:selectedRows]) {
        [self.tableView selectRowIndexes:selectedRows byExtendingSelection:NO];
    }
}

- (void)setTableViewActive:(BOOL)tableViewActive
{
    if (_tableViewActive == tableViewActive) {
        return;
    }
    NSSet<NSNumber *> * const selectedIDs = [self selectedMediaIDs];
    _tableViewActive = tableViewActive;
    [self restoreSelectionWithMediaIDs:selectedIDs];
}

- (void)reloadData
{
    NSAssert(NSThread.isMainThread, @"History view updates must run on the main thread");
    NSCollectionView * const collectionView = self.collectionView;
    NSAssert(collectionView == nil || collectionView.dataSource == self,
             @"Cannot reload a collection view with a different data source");
    NSTableView * const tableView = self.tableView;
    NSAssert(tableView == nil || tableView.dataSource == self,
             @"Cannot reload a table view with a different data source");

    NSSet<NSNumber *> * const selectedIDs = [self selectedMediaIDs];
    [collectionView deselectItemsAtIndexPaths:collectionView.selectionIndexPaths];
    if ([collectionView.collectionViewLayout isKindOfClass:VLCLibraryCollectionViewFlowLayout.class]) {
        [(VLCLibraryCollectionViewFlowLayout *)collectionView.collectionViewLayout resetLayout];
    }

    _calendar = NSCalendar.currentCalendar.copy;
    _now = NSDate.date;
    _sections = [VLCLibraryHistorySection sectionsForMediaItems:self.libraryModel.listOfHistoryMedia ?: @[]
                                                       calendar:_calendar];
    NSMutableArray<NSIndexPath *> * const tableRows = NSMutableArray.array;
    [_sections enumerateObjectsUsingBlock:^(VLCLibraryHistorySection *section,
                                            NSUInteger sectionIndex,
                                            BOOL * const __unused stop) {
        [tableRows addObject:[NSIndexPath indexPathForItem:NSNotFound inSection:sectionIndex]];
        for (NSUInteger itemIndex = 0; itemIndex < section.mediaItems.count; ++itemIndex) {
            [tableRows addObject:[NSIndexPath indexPathForItem:itemIndex inSection:sectionIndex]];
        }
    }];
    _tableRows = tableRows.copy;
    [collectionView reloadData];
    [tableView reloadData];
    [self restoreSelectionWithMediaIDs:selectedIDs];
    if (self.dataChangedHandler != nil) {
        self.dataChangedHandler();
    }
}

#pragma mark - Table view data source

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _tableRows.count;
}

- (BOOL)isHeaderRow:(NSInteger)row
{
    return row >= 0 && (NSUInteger)row < _tableRows.count && _tableRows[row].item == NSNotFound;
}

- (NSString *)titleForRow:(NSInteger)row
{
    if (![self isHeaderRow:row]) {
        return @"";
    }
    return [self.sections[_tableRows[row].section] titleWithCalendar:_calendar now:_now];
}

- (VLCLibraryRepresentedItem *)representedItemForHeaderRow:(NSInteger)row
{
    if (![self isHeaderRow:row]) {
        return nil;
    }

    VLCLibraryHistorySection * const section = self.sections[_tableRows[row].section];
    VLCMediaLibraryDummyItem * const groupItem =
        [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:[self titleForRow:row]
                                                 withMediaItems:section.mediaItems];
    return [[VLCLibraryRepresentedItem alloc] initWithItem:groupItem
                                                parentType:VLCMediaLibraryParentGroupTypeUnknown];
}

- (VLCMediaLibraryParentGroupType)currentParentType
{
    return VLCMediaLibraryParentGroupTypeUnknown;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row forTableView:(NSTableView *)tableView
{
    if (row < 0 || (NSUInteger)row >= _tableRows.count || [self isHeaderRow:row]) {
        return nil;
    }
    return [self libraryItemAtIndexPath:_tableRows[row] forCollectionView:self.collectionView];
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return NSNotFound;
    }
    NSIndexPath * const indexPath = [self indexPathForLibraryItem:libraryItem];
    return indexPath == nil ? NSNotFound : [_tableRows indexOfObject:indexPath];
}

- (id<NSPasteboardWriting>)tableView:(NSTableView *)tableView pasteboardWriterForRow:(NSInteger)row
{
    id<VLCMediaLibraryItemProtocol> const item = [self libraryItemAtRow:row forTableView:tableView];
    return item == nil ? nil : [NSPasteboardItem pasteboardItemWithLibraryItem:item];
}

- (void)doubleClickAction:(id)sender
{
    if (sender != self.tableView) {
        return;
    }
    id<VLCMediaLibraryItemProtocol> const item =
        [self libraryItemAtRow:self.tableView.clickedRow forTableView:self.tableView];
    if (item != nil) {
        VLCLibraryRepresentedItem * const representedItem =
            [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:self.currentParentType];
        [representedItem play];
    }
}

#pragma mark - Collection view data source

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return self.sections.count;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
    if (section < 0 || (NSUInteger)section >= self.sections.count) {
        return 0;
    }
    return self.sections[section].mediaItems.count;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    if (indexPath.section < 0 || (NSUInteger)indexPath.section >= self.sections.count) {
        return nil;
    }
    NSArray<VLCMediaLibraryMediaItem *> * const items = self.sections[indexPath.section].mediaItems;
    return indexPath.item >= 0 && (NSUInteger)indexPath.item < items.count ? items[indexPath.item] : nil;
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    for (NSUInteger section = 0; section < self.sections.count; ++section) {
        NSArray<VLCMediaLibraryMediaItem *> * const items = self.sections[section].mediaItems;
        const NSUInteger index = [items indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem *item,
                                                                       NSUInteger __unused index,
                                                                       BOOL * const __unused stop) {
            return item.libraryID == libraryItem.libraryID;
        }];
        if (index != NSNotFound) {
            return [NSIndexPath indexPathForItem:index inSection:section];
        }
    }
    return nil;
}

- (NSArray<VLCLibraryRepresentedItem *> *)representedItemsAtIndexPaths:(NSSet<NSIndexPath *> *const)indexPaths
                                                     forCollectionView:(NSCollectionView *)collectionView
{
    NSMutableArray<VLCLibraryRepresentedItem *> * const items = NSMutableArray.array;
    for (NSIndexPath * const indexPath in [indexPaths.allObjects sortedArrayUsingSelector:@selector(compare:)]) {
        id<VLCMediaLibraryItemProtocol> const item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        if (item != nil) {
            [items addObject:[[VLCLibraryRepresentedItem alloc] initWithItem:item
                                                                  parentType:VLCMediaLibraryParentGroupTypeUnknown]];
        }
    }
    return items.copy;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem * const viewItem =
        [collectionView makeItemWithIdentifier:VLCLibraryCollectionViewItemIdentifier forIndexPath:indexPath];
    id<VLCMediaLibraryItemProtocol> const item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
    viewItem.representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:item
                                                                    parentType:VLCMediaLibraryParentGroupTypeUnknown];
    viewItem.showsMarkUnplayedButton = YES;
    return viewItem;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind atIndexPath:(NSIndexPath *)indexPath
{
    if ([kind isEqualToString:NSCollectionElementKindSectionHeader]) {
        VLCLibraryCollectionViewSupplementaryElementView * const header =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:VLCLibrarySupplementaryElementViewIdentifier
                                           forIndexPath:indexPath];
        header.stringValue = [self.sections[indexPath.section] titleWithCalendar:_calendar now:_now];
        return header;
    }
    if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView * const detail =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier
                                           forIndexPath:indexPath];
        id<VLCMediaLibraryItemProtocol> const item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        detail.representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:item
                                                                      parentType:VLCMediaLibraryParentGroupTypeUnknown];
        detail.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return detail;
    }
    return nil;
}

- (NSString *)supplementaryDetailViewKind
{
    return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
}

@end
