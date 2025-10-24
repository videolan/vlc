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

@interface VLCLibraryVideoDataSource ()
{
    NSArray *_recentsArray;
    NSArray *_libraryArray;
    VLCLibraryCollectionViewFlowLayout *_collectionViewFlowLayout;
    NSUInteger _priorNumVideoSections;
}

@end

@implementation VLCLibraryVideoDataSource

- (instancetype)init
{
    self = [super init];
    if(self) {
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
    [self checkRecentsSection];
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
    [self checkRecentsSection];

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

- (void)reloadData
{
    if(!_libraryModel) {
        return;
    }

    [_collectionViewFlowLayout resetLayout];

    self->_recentsArray = [self.libraryModel listOfRecentMedia];
    self->_libraryArray = [self.libraryModel listOfVideoMedia];

    if (self.masterTableView.dataSource == self) {
        [self.masterTableView reloadData];
    }
    if (self.detailTableView.dataSource == self) {
        [self.detailTableView reloadData];
    }
    if (self.collectionView.dataSource == self) {
        [self.collectionView reloadData];
    }
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryVideoDataSourceDisplayedCollectionChangedNotification
                                                      object:self
                                                    userInfo:nil];
}

- (void)changeDataForSpecificMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
                          inVideoGroup:(const VLCMediaLibraryParentGroupType)group
                        arrayOperation:(void(^)(const NSMutableArray*, const NSUInteger))arrayOperation
                     completionHandler:(void(^)(const NSIndexSet*))completionHandler
{
    NSMutableArray *groupMutableCopyArray;
    switch(group) {
        case VLCMediaLibraryParentGroupTypeVideoLibrary:
            groupMutableCopyArray = [_libraryArray mutableCopy];
            break;
        case VLCMediaLibraryParentGroupTypeRecentVideos:
            groupMutableCopyArray = [_recentsArray mutableCopy];
            break;
        default:
            return;
    }

    NSUInteger mediaItemIndex = [self indexOfMediaItem:mediaItem.libraryID inArray:groupMutableCopyArray];
    if (mediaItemIndex == NSNotFound) {
        return;
    }

    arrayOperation(groupMutableCopyArray, mediaItemIndex);
    switch(group) {
        case VLCMediaLibraryParentGroupTypeVideoLibrary:
            _libraryArray = [groupMutableCopyArray copy];
            break;
        case VLCMediaLibraryParentGroupTypeRecentVideos:
            _recentsArray = [groupMutableCopyArray copy];
            break;
        default:
            return;
    }

    NSIndexSet * const rowIndexSet = [NSIndexSet indexSetWithIndex:mediaItemIndex];
    completionHandler(rowIndexSet);
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

        if (self.detailTableView.dataSource == self &&
            [self rowToVideoGroup:self.masterTableView.selectedRow] == group) {
            // Don't regenerate the groups by index as these do not change according to the
            // notification, stick to the selection table view
            const NSRange columnRange = NSMakeRange(0, self.masterTableView.numberOfColumns);
            NSIndexSet * const columnIndexSet =
                [NSIndexSet indexSetWithIndexesInRange:columnRange];
            [self.detailTableView reloadDataForRowIndexes:rowIndexSet columnIndexes:columnIndexSet];
        }

        // Don't bother with the groups table view as we always show "recents" and "videos" there
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

        if (self.detailTableView.dataSource == self &&
            [self rowToVideoGroup:self.masterTableView.selectedRow] == group) {
            // Don't regenerate the groups by index as these do not change according to the
            // notification, stick to the selection table view
            [self.detailTableView removeRowsAtIndexes:rowIndexSet
                                        withAnimation:NSTableViewAnimationSlideUp];
        }
    }];
}

#pragma mark - table view data source and delegation

- (BOOL)recentItemsPresent
{
    return self.libraryModel.numberOfRecentMedia > 0;
}

- (BOOL)recentsSectionPresent
{
    // We display Recents and/or Library. This will need to change if we add more sections.
    return _priorNumVideoSections == 2;
}

- (NSUInteger)rowToVideoGroupAdjustment
{
    // We need to adjust the selected row value to match the backing enum.
    // Additionally, we hide recents when there are no recent media items.
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

- (void)checkRecentsSection
{
    const BOOL recentsPresent = [self recentItemsPresent];
    const BOOL recentsVisible = [self recentsSectionPresent];

    if (recentsPresent == recentsVisible) {
        return;
    }

    [self.masterTableView reloadData];
    [self reloadData];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (tableView == self.masterTableView) {
        _priorNumVideoSections = [self recentItemsPresent] ? 2 : 1;
        return _priorNumVideoSections;
    } else if (tableView == self.detailTableView && self.masterTableView.selectedRow > -1) {
        switch([self rowToVideoGroup:self.masterTableView.selectedRow]) {
            case VLCMediaLibraryParentGroupTypeRecentVideos:
                return _recentsArray.count;
            case VLCMediaLibraryParentGroupTypeVideoLibrary:
                return _libraryArray.count;
            default:
                NSAssert(NO, @"Reached unreachable case for video library section");
                break;
        }
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
    if (tableView == self.detailTableView && self.masterTableView.selectedRow > -1) {
        switch([self rowToVideoGroup:self.masterTableView.selectedRow]) {
            case VLCMediaLibraryParentGroupTypeRecentVideos:
                return _recentsArray[row];
            case VLCMediaLibraryParentGroupTypeVideoLibrary:
                return _libraryArray[row];
            default:
                NSAssert(NO, @"Reached unreachable case for video library section");
                break;
        }
    }

    return nil;
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return NSNotFound;
    }
    return [self indexOfMediaItem:libraryItem.libraryID inArray:_libraryArray];
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
