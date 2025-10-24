/*****************************************************************************
 * VLCLibraryShowsDataSource.m: MacOS X interface module
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

#import "VLCLibraryShowsDataSource.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"

@implementation VLCLibraryShowsDataSource

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
                           selector:@selector(libraryModelShowsListReset:)
                               name:VLCLibraryModelListOfShowsReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelShowDeleted:)
                               name:VLCLibraryModelShowDeleted
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelShowUpdated:)
                               name:VLCLibraryModelShowUpdated
                             object:nil];

    [self reloadData];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)libraryModelShowsListReset:(NSNotification *)notification
{
    [self reloadData];
}

- (NSArray<id<VLCMediaLibraryItemProtocol>> *)backingArray
{
    return self.libraryModel.listOfShows;
}

- (VLCMediaLibraryParentGroupType)currentParentType
{
    return VLCMediaLibraryParentGroupTypeShow;
}

- (void)libraryModelShowUpdated:(NSNotification *)notification
{
    VLCMediaLibraryShow * const show = notification.object;
    NSIndexPath * const indexPath = [self indexPathForLibraryItem:show];

    if (indexPath != nil) {
        [self.collectionView reloadItemsAtIndexPaths:[NSSet setWithObject:indexPath]];
    }

    const NSInteger rowIndex = [self rowForLibraryItem:show];
    if (rowIndex == NSNotFound) {
        return;
    }

    const NSInteger selectedMasterRow = self.masterTableView.selectedRow;
    [self.masterTableView reloadDataForRowIndexes:[NSIndexSet indexSetWithIndex:rowIndex]
                                    columnIndexes:[NSIndexSet indexSetWithIndex:0]];

    if (rowIndex == selectedMasterRow && self.masterTableView.selectedRow != selectedMasterRow) {
        [self.masterTableView selectRowIndexes:[NSIndexSet indexSetWithIndex:selectedMasterRow]
                          byExtendingSelection:NO];
    } else {
        [self.detailTableView reloadData];
    }
}

- (void)libraryModelShowDeleted:(NSNotification *)notification
{
    NSNumber * const showIdNumber = notification.object;
    const NSInteger showLibraryId = showIdNumber.integerValue;
    const NSInteger rowIndex = [self.backingArray indexOfObjectPassingTest:^BOOL(VLCMediaLibraryShow * const show, const NSUInteger __unused idx, BOOL * const __unused stop) {
        return show.libraryID == showLibraryId;
    }];
    if (rowIndex == NSNotFound) {
        return;
    }

    NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:0 inSection:rowIndex];
    if (indexPath != nil) {
        [self.collectionView deleteItemsAtIndexPaths:[NSSet setWithObject:indexPath]];
    }

    [self.masterTableView removeRowsAtIndexes:[NSIndexSet indexSetWithIndex:rowIndex]
                                withAnimation:NSTableViewAnimationEffectFade];
}

@end
