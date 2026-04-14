/*****************************************************************************
 * VLCLibraryVideoDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCLibraryVideoDataSource.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryTableCellView.h"

#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"

#import "extensions/NSIndexSet+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSPasteboardItem+VLCAdditions.h"

NSString * const VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification = @"VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification";

/**
 * Represents one row in the flattened table view model.
 * A row is either a section header or a media item within a section.
 */
@interface VLCLibraryVideoFlattenedRow : NSObject
@property (readonly) BOOL isHeader;
@property (readonly) VLCMediaLibraryParentGroupType parentType;
@property (readonly) NSInteger itemIndex; // -1 for header rows
+ (instancetype)headerForGroup:(VLCMediaLibraryParentGroupType)group;
+ (instancetype)itemAtIndex:(NSInteger)index inGroup:(VLCMediaLibraryParentGroupType)group;
@end

@implementation VLCLibraryVideoFlattenedRow

+ (instancetype)headerForGroup:(VLCMediaLibraryParentGroupType)group
{
    VLCLibraryVideoFlattenedRow * const row = [VLCLibraryVideoFlattenedRow new];
    row->_isHeader = YES;
    row->_parentType = group;
    row->_itemIndex = -1;
    return row;
}

+ (instancetype)itemAtIndex:(NSInteger)index inGroup:(VLCMediaLibraryParentGroupType)group
{
    VLCLibraryVideoFlattenedRow * const row = [VLCLibraryVideoFlattenedRow new];
    row->_isHeader = NO;
    row->_parentType = group;
    row->_itemIndex = index;
    return row;
}

@end

@interface VLCLibraryVideoDataSource ()
{
    NSMutableArray *_recentsArray;
    NSMutableArray *_libraryArray;
    VLCLibraryCollectionViewFlowLayout *_collectionViewFlowLayout;
    NSUInteger _priorNumVideoSections;
    NSArray<VLCLibraryVideoFlattenedRow *> *_flattenedRows;
}

@end

@implementation VLCLibraryVideoDataSource

- (instancetype)init
{
    self = [super init];
    if(self) {
        _flattenedRows = @[];
        [self connect];
    }
    return self;
}

- (NSUInteger)indexOfMediaItem:(int64_t)libraryId inArray:(NSArray const *)array
{
    return [array indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem * const findMediaItem, const NSUInteger __unused idx, BOOL * const __unused stop) {
        NSAssert(findMediaItem != nil, @"Collection should not contain nil media items");
        return findMediaItem.libraryID == libraryId;
    }];
}

- (void)libraryModelVideoListReset:(NSNotification * const)aNotification
{
    [self reloadData];
}

- (void)libraryModelVideoItemUpdated:(NSNotification * const)aNotification
{
    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem * const notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item updated notification should carry valid media item");

    [self reloadDataForMediaItem:notificationMediaItem
                    inVideoGroup:VLCMediaLibraryParentGroupTypeVideoLibrary];
}

- (void)libraryModelVideoItemDeleted:(NSNotification * const)aNotification
{
    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem * const notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item deleted notification should carry valid media item");

    [self deleteDataForMediaItem:notificationMediaItem
                    inVideoGroup:VLCMediaLibraryParentGroupTypeVideoLibrary];
}

- (void)libraryModelRecentsListReset:(NSNotification * const)aNotification
{
    [self reloadData];
}

- (void)libraryModelRecentsItemUpdated:(NSNotification * const)aNotification
{
    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem * const notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item updated notification should carry valid media item");

    [self reloadDataForMediaItem:notificationMediaItem
                    inVideoGroup:VLCMediaLibraryParentGroupTypeRecentVideos];
}

- (void)libraryModelRecentsItemDeleted:(NSNotification * const)aNotification
{
    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem * const notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item deleted notification should carry valid media item");

    [self deleteDataForMediaItem:notificationMediaItem
                    inVideoGroup:VLCMediaLibraryParentGroupTypeRecentVideos];
}

- (void)connect
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelVideoListReset:)
                               name:VLCLibraryModelVideoMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelVideoListReset:)
                               name:VLCLibraryModelAllCachesDropped
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelVideoItemUpdated:)
                               name:VLCLibraryModelVideoMediaItemUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelVideoItemDeleted:)
                               name:VLCLibraryModelVideoMediaItemDeleted
                             object:nil];

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelRecentsListReset:)
                               name:VLCLibraryModelRecentsMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelRecentsItemUpdated:)
                               name:VLCLibraryModelRecentsMediaItemUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelRecentsItemDeleted:)
                               name:VLCLibraryModelRecentsMediaItemDeleted
                             object:nil];

    [self reloadData];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

#pragma mark - Flattened row model

- (NSArray *)arrayForGroup:(VLCMediaLibraryParentGroupType)group
{
    switch (group) {
        case VLCMediaLibraryParentGroupTypeRecentVideos:
            return _recentsArray;
        case VLCMediaLibraryParentGroupTypeVideoLibrary:
            return _libraryArray;
        default:
            return @[];
    }
}

- (void)rebuildFlattenedRows
{
    NSMutableArray<VLCLibraryVideoFlattenedRow *> * const rows = [NSMutableArray array];

    const NSUInteger recentsCount = _recentsArray.count;
    if (recentsCount > 0) {
        [rows addObject:[VLCLibraryVideoFlattenedRow headerForGroup:VLCMediaLibraryParentGroupTypeRecentVideos]];
        for (NSUInteger i = 0; i < recentsCount; i++) {
            [rows addObject:[VLCLibraryVideoFlattenedRow itemAtIndex:i
                                                             inGroup:VLCMediaLibraryParentGroupTypeRecentVideos]];
        }
    }

    const NSUInteger libraryCount = _libraryArray.count;
    if (libraryCount > 0) {
        [rows addObject:[VLCLibraryVideoFlattenedRow headerForGroup:VLCMediaLibraryParentGroupTypeVideoLibrary]];
        for (NSUInteger i = 0; i < libraryCount; i++) {
            [rows addObject:[VLCLibraryVideoFlattenedRow itemAtIndex:i
                                                             inGroup:VLCMediaLibraryParentGroupTypeVideoLibrary]];
        }
    }

    _flattenedRows = [rows copy];
}

- (void)reloadData
{
    if(!_libraryModel) {
        return;
    }

    [_collectionViewFlowLayout resetLayout];

    self->_recentsArray = [[self.libraryModel listOfRecentMedia] mutableCopy];
    self->_libraryArray = [[self.libraryModel listOfVideoMedia] mutableCopy];

    [self rebuildFlattenedRows];

    if (self.tableView.dataSource == self) {
        [self.tableView reloadData];
    }
    if (self.collectionView.dataSource == self) {
        [self.collectionView reloadData];
    }
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification
                                                      object:self
                                                    userInfo:nil];
}

- (NSInteger)flattenedRowIndexForItemIndex:(NSUInteger)itemIndex
                                   inGroup:(VLCMediaLibraryParentGroupType)group
{
    const NSUInteger rowCount = _flattenedRows.count;
    for (NSUInteger i = 0; i < rowCount; i++) {
        VLCLibraryVideoFlattenedRow * const flatRow = _flattenedRows[i];
        if (!flatRow.isHeader &&
            flatRow.parentType == group &&
            flatRow.itemIndex == (NSInteger)itemIndex) {
            return i;
        }
    }
    return NSNotFound;
}

- (void)changeDataForSpecificMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
                          inVideoGroup:(const VLCMediaLibraryParentGroupType)group
                        arrayOperation:(void(^)(const NSMutableArray*, const NSUInteger))arrayOperation
                     completionHandler:(void(^)(const NSIndexSet*))completionHandler
{
    NSMutableArray *groupArray = (NSMutableArray *)[self arrayForGroup:group];
    if (groupArray == nil) {
        return;
    }

    const NSUInteger mediaItemIndex = [self indexOfMediaItem:mediaItem.libraryID inArray:groupArray];
    if (mediaItemIndex == NSNotFound) {
        return;
    }

    // Find the flattened row index BEFORE mutating the arrays
    const NSInteger flatRowIndex = [self flattenedRowIndexForItemIndex:mediaItemIndex
                                                              inGroup:group];

    arrayOperation(groupArray, mediaItemIndex);
    [self rebuildFlattenedRows];

    NSIndexSet * const rowIndexSet = [NSIndexSet indexSetWithIndex:mediaItemIndex];
    completionHandler(rowIndexSet);

    // Targeted table view update using flattened row index
    if (flatRowIndex != NSNotFound && self.tableView.dataSource == self) {
        [self.tableView reloadDataForRowIndexes:[NSIndexSet indexSetWithIndex:flatRowIndex]
                                  columnIndexes:[NSIndexSet indexSetWithIndex:0]];
    }

    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification
                                                      object:self
                                                    userInfo:nil];
}

- (void)reloadDataForMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
                  inVideoGroup:(const VLCMediaLibraryParentGroupType)group
{
    [self changeDataForSpecificMediaItem:mediaItem
                            inVideoGroup:group
                          arrayOperation:^(NSMutableArray * const mediaArray, const NSUInteger mediaItemIndex) {

        [mediaArray replaceObjectAtIndex:mediaItemIndex withObject:mediaItem];

    } completionHandler:^(NSIndexSet * const rowIndexSet) {

        if (self.collectionView.dataSource == self) {
            const NSInteger section = [self videoGroupToRow:group];
            NSSet<NSIndexPath *> * const indexPathSet =
                [rowIndexSet indexPathSetWithSection:section];
            [self.collectionView reloadItemsAtIndexPaths:indexPathSet];
        }
    }];
}

- (void)deleteDataForMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
                  inVideoGroup:(const VLCMediaLibraryParentGroupType)group
{
    [self changeDataForSpecificMediaItem:mediaItem
                            inVideoGroup:group
                          arrayOperation:^(NSMutableArray * const mediaArray, const NSUInteger mediaItemIndex) {

        [mediaArray removeObjectAtIndex:mediaItemIndex];

    } completionHandler:^(NSIndexSet * const rowIndexSet) {

        if (self.collectionView.dataSource == self) {
            const NSInteger section = [self videoGroupToRow:group];
            NSSet<NSIndexPath *> * const indexPathSet =
                [rowIndexSet indexPathSetWithSection:section];
            [self.collectionView deleteItemsAtIndexPaths:indexPathSet];
        }
    }];
}

#pragma mark - Public query methods

- (BOOL)isHeaderRow:(NSInteger)row
{
    if (row < 0 || (NSUInteger)row >= _flattenedRows.count) {
        return NO;
    }
    return _flattenedRows[row].isHeader;
}

- (VLCMediaLibraryParentGroupType)parentTypeForRow:(NSInteger)row
{
    if (row < 0 || (NSUInteger)row >= _flattenedRows.count) {
        return VLCMediaLibraryParentGroupTypeUnknown;
    }
    return _flattenedRows[row].parentType;
}

- (NSString *)titleForRow:(NSInteger)row
{
    return [self titleForVideoGroup:[self parentTypeForRow:row]];
}

- (VLCLibraryRepresentedItem *)representedItemForHeaderRow:(NSInteger)row
{
    if (![self isHeaderRow:row]) {
        return nil;
    }

    const VLCMediaLibraryParentGroupType parentType = [self parentTypeForRow:row];
    NSArray * const groupArray = [self arrayForGroup:parentType];
    if (groupArray.count == 0) {
        return nil;
    }

    NSString * const title = [self titleForVideoGroup:parentType];
    VLCMediaLibraryDummyItem * const groupItem =
        [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:title
                                                 withMediaItems:groupArray];
    return [[VLCLibraryRepresentedItem alloc] initWithItem:groupItem parentType:parentType];
}

#pragma mark - Table view data source (sectioned flat table)

- (BOOL)recentItemsPresent
{
    return self.libraryModel.numberOfRecentMedia > 0;
}

- (NSUInteger)rowToVideoGroupAdjustment
{
    static const VLCMediaLibraryParentGroupType firstEntry = VLCMediaLibraryParentGroupTypeRecentVideos;
    const BOOL anyRecents = [self recentItemsPresent];
    return anyRecents ? firstEntry : firstEntry + 1;
}

- (VLCMediaLibraryParentGroupType)rowToVideoGroup:(NSInteger)row
{
    return row + [self rowToVideoGroupAdjustment];
}

- (NSUInteger)videoGroupToRow:(NSInteger)videoGroup
{
    return videoGroup - [self rowToVideoGroupAdjustment];
}

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

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    if (row < 0 || (NSUInteger)row >= _flattenedRows.count) {
        return nil;
    }

    VLCLibraryVideoFlattenedRow * const flatRow = _flattenedRows[row];

    if (flatRow.isHeader) {
        return nil;
    }

    NSArray * const groupArray = [self arrayForGroup:flatRow.parentType];
    if (flatRow.itemIndex >= 0 && (NSUInteger)flatRow.itemIndex < groupArray.count) {
        return groupArray[flatRow.itemIndex];
    }

    return nil;
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return NSNotFound;
    }

    const NSUInteger rowCount = _flattenedRows.count;
    for (NSUInteger i = 0; i < rowCount; i++) {
        VLCLibraryVideoFlattenedRow * const flatRow = _flattenedRows[i];
        if (flatRow.isHeader) {
            continue;
        }

        NSArray * const groupArray = [self arrayForGroup:flatRow.parentType];
        if (flatRow.itemIndex >= 0 && (NSUInteger)flatRow.itemIndex < groupArray.count) {
            id<VLCMediaLibraryItemProtocol> const item = groupArray[flatRow.itemIndex];
            if (item.libraryID == libraryItem.libraryID) {
                return i;
            }
        }
    }

    return NSNotFound;
}

- (VLCMediaLibraryParentGroupType)currentParentType
{
    return VLCMediaLibraryParentGroupTypeVideoLibrary;
}

# pragma mark - collection view data source and delegation

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    switch ([self rowToVideoGroup:indexPath.section]) {
        case VLCMediaLibraryParentGroupTypeRecentVideos:
            return _recentsArray[indexPath.item];
        case VLCMediaLibraryParentGroupTypeVideoLibrary:
            return _libraryArray[indexPath.item];
        default:
            NSAssert(NO, @"Unknown video group received");
            return nil;
    }
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    const NSInteger row = [self rowForLibraryItem:libraryItem];
    if (row == NSNotFound) {
        return nil;
    }
    const NSInteger section = [self videoGroupToRow:VLCMediaLibraryParentGroupTypeVideoLibrary];
    return [NSIndexPath indexPathForItem:row inSection:section];
}

- (NSArray<VLCLibraryRepresentedItem *> *)representedItemsAtIndexPaths:(NSSet<NSIndexPath *> *const)indexPaths
                                                     forCollectionView:(NSCollectionView *)collectionView
{
    NSMutableArray<VLCLibraryRepresentedItem *> * const representedItems =
        [NSMutableArray arrayWithCapacity:indexPaths.count];

    for (NSIndexPath * const indexPath in indexPaths) {
        const VLCMediaLibraryParentGroupType parentType = [self rowToVideoGroup:indexPath.section];
        const id<VLCMediaLibraryItemProtocol> libraryItem =
            [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        VLCLibraryRepresentedItem * const representedItem =
            [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem parentType:parentType];
        [representedItems addObject:representedItem];
    }

    return representedItems;
}

- (NSString *)titleForVideoGroup:(VLCMediaLibraryParentGroupType)videoGroup
{
    switch (videoGroup) {
        case VLCMediaLibraryParentGroupTypeRecentVideos:
            return _NS("Recents");
        case VLCMediaLibraryParentGroupTypeVideoLibrary:
            return _NS("Library");
        default:
            NSAssert(NO, @"Received unknown video group");
            return @"";
    }
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    _priorNumVideoSections = [self recentItemsPresent] ? 2 : 1;
    return _priorNumVideoSections;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    switch ([self rowToVideoGroup:section]) {
        case VLCMediaLibraryParentGroupTypeRecentVideos:
            return _recentsArray.count;
        case VLCMediaLibraryParentGroupTypeVideoLibrary:
            return _libraryArray.count;
        default:
            NSAssert(NO, @"Unknown video group received.");
            return NSNotFound;
    }
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem * const viewItem =
        [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    const VLCMediaLibraryParentGroupType parentType = [self rowToVideoGroup:indexPath.section];
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
        const VLCMediaLibraryParentGroupType videoGroup = [self rowToVideoGroup:indexPath.section];
        sectionHeadingView.stringValue = [self titleForVideoGroup:videoGroup];
        return sectionHeadingView;

    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView * const mediaItemSupplementaryDetailView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind forIndexPath:indexPath];
        const id<VLCMediaLibraryItemProtocol> item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:self.currentParentType];

        mediaItemSupplementaryDetailView.representedItem = representedItem;
        mediaItemSupplementaryDetailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return mediaItemSupplementaryDetailView;
    }

    return nil;
}

- (NSString *)supplementaryDetailViewKind
{
    return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
}

@end
