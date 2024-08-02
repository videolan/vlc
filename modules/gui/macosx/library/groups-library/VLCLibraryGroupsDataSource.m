/*****************************************************************************
 * VLCLibraryGroupsDataSource.m: MacOS X interface module
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

#import "VLCLibraryGroupsDataSource.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

@interface VLCLibraryGroupsDataSource ()

@property (readwrite, atomic) NSArray<VLCMediaLibraryGroup *> *groupsArray;

@end

@implementation VLCLibraryGroupsDataSource

- (instancetype)init
{
    self = [super init];
    if(self) {
        [self connect];
    }
    return self;
}

- (void)connect
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelGroupsListReset:)
                               name:VLCLibraryModelListOfGroupsReset
                             object:nil];

    [self reloadData];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)libraryModelGroupsListReset:(NSNotification *)notification
{
    [self reloadData];
}

- (void)reloadData
{
    [(VLCLibraryCollectionViewFlowLayout *)self.collectionView.collectionViewLayout resetLayout];

    self.groupsArray = self.libraryModel.listOfGroups;

    [self.masterTableView reloadData];
    [self.detailTableView reloadData];
    [self.collectionView reloadData];
}

- (NSUInteger)indexOfMediaItem:(const NSUInteger)libraryId inArray:(NSArray const *)array
{
    return [array indexOfObjectPassingTest:^BOOL(const id<VLCMediaLibraryItemProtocol> findItem,
                                                 const NSUInteger idx,
                                                 BOOL * const stop) {
        NSAssert(findItem != nil, @"Collection should not contain nil items");
        return findItem.libraryID == libraryId;
    }];
}

#pragma mark - table view data source and delegation

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (tableView == self.masterTableView) {
        return self.groupsArray.count;
    }

    const NSInteger selectedGroupRow = self.masterTableView.selectedRow;
    if (tableView == self.detailTableView && selectedGroupRow > -1) {
        return self.groupsArray[selectedGroupRow].numberOfTotalItems;
    }

    return 0;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    if (tableView == self.masterTableView) {
        return self.groupsArray[row];
    }

    const NSInteger selectedGroupRow = self.masterTableView.selectedRow;
    if (tableView == self.detailTableView && selectedGroupRow > -1) {
        VLCMediaLibraryGroup * const group = self.groupsArray[selectedGroupRow];
        return group.mediaItems[row];
    }

    return nil;
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return NSNotFound;
    }
    return [self indexOfMediaItem:libraryItem.libraryID inArray:self.groupsArray];
}

- (VLCMediaLibraryParentGroupType)currentParentType
{
    return VLCMediaLibraryParentGroupTypeGroup;
}

# pragma mark - collection view data source and delegation

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    VLCMediaLibraryGroup * const group = self.groupsArray[indexPath.section];
    return group.mediaItems[indexPath.item];
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    __block NSInteger groupMediaItemIndex = NSNotFound;
    const NSInteger groupIndex =
        [self.groupsArray indexOfObjectPassingTest:^BOOL(VLCMediaLibraryGroup * const group,
                                                         const NSUInteger idx,
                                                         BOOL * const stop) {
            groupMediaItemIndex =
                [group.mediaItems indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem * const item,
                                                                 const NSUInteger idx,
                                                                 BOOL * const stop) {
                    return item.libraryID == libraryItem.libraryID;
                }];
            return groupMediaItemIndex != NSNotFound;
        }];
    return groupIndex != NSNotFound
        ? [NSIndexPath indexPathForItem:groupMediaItemIndex inSection:groupIndex]
        : nil;
}

- (NSArray<VLCLibraryRepresentedItem *> *)representedItemsAtIndexPaths:(NSSet<NSIndexPath *> *const)indexPaths
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

@end
