/*****************************************************************************
 * VLCLibraryVideoTableViewDataSource.m: MacOS X interface module
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

#import "VLCLibraryVideoTableViewDataSource.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"

#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "main/CompatibilityFixes.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSPasteboardItem+VLCAdditions.h"

@interface VLCLibraryVideoTableViewDataSource ()
{
    NSArray *_recentsArray;
    NSArray *_libraryArray;
    VLCLibraryCollectionViewFlowLayout *_collectionViewFlowLayout;
}

@end

@implementation VLCLibraryVideoTableViewDataSource

- (instancetype)init
{
    self = [super init];
    if(self) {
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
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
    }
    return self;
}

- (NSUInteger)indexOfMediaItem:(const NSUInteger)libraryId inArray:(NSArray const *)array
{
    return [array indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem * const findMediaItem, const NSUInteger idx, BOOL * const stop) {
        NSAssert(findMediaItem != nil, @"Collection should not contain nil media items");
        return findMediaItem.libraryID == libraryId;
    }];
}

- (void)libraryModelVideoListReset:(NSNotification * const)aNotification
{
    if (_groupsTableView.selectedRow == -1 ||
        _groupsTableView.selectedRow != VLCLibraryVideoLibraryGroup - 1) { // Row 0 == second value in enum, so compensate

        return;
    }

    [self reloadData];
}

- (void)libraryModelVideoItemUpdated:(NSNotification * const)aNotification
{
    if (_groupsTableView.selectedRow == -1 ||
        _groupsTableView.selectedRow != VLCLibraryVideoLibraryGroup - 1) { // Row 0 == second value in enum, so compensate

        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item updated notification should carry valid media item");

    [self reloadDataForMediaItem:notificationMediaItem
                    inVideoGroup:VLCLibraryVideoLibraryGroup];
}

- (void)libraryModelVideoItemDeleted:(NSNotification * const)aNotification
{
    if (_groupsTableView.selectedRow == -1 ||
        _groupsTableView.selectedRow != VLCLibraryVideoLibraryGroup - 1) { // Row 0 == second value in enum, so compensate

        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item deleted notification should carry valid media item");

    [self deleteDataForMediaItem:notificationMediaItem
                    inVideoGroup:VLCLibraryVideoLibraryGroup];
}

- (void)libraryModelRecentsListReset:(NSNotification * const)aNotification
{
    if (_groupsTableView.selectedRow == -1 ||
        _groupsTableView.selectedRow != VLCLibraryVideoRecentsGroup - 1) { // Row 0 == second value in enum, so compensate

        return;
    }

    [self reloadData];
}

- (void)libraryModelRecentsItemUpdated:(NSNotification * const)aNotification
{
    if (_groupsTableView.selectedRow == -1 ||
        _groupsTableView.selectedRow != VLCLibraryVideoRecentsGroup - 1) { // Row 0 == second value in enum, so compensate

        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item updated notification should carry valid media item");

    [self reloadDataForMediaItem:notificationMediaItem
                    inVideoGroup:VLCLibraryVideoRecentsGroup];
}

- (void)libraryModelRecentsItemDeleted:(NSNotification * const)aNotification
{
    if (_groupsTableView.selectedRow == -1 ||
        _groupsTableView.selectedRow != VLCLibraryVideoRecentsGroup - 1) { // Row 0 == second value in enum, so compensate

        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item deleted notification should carry valid media item");

    [self deleteDataForMediaItem:notificationMediaItem
                    inVideoGroup:VLCLibraryVideoRecentsGroup];
}

- (void)reloadData
{
    if(!_libraryModel) {
        return;
    }
    
    [_collectionViewFlowLayout resetLayout];
    
    self->_recentsArray = [self.libraryModel listOfRecentMedia];
    self->_libraryArray = [self.libraryModel listOfVideoMedia];
    [self->_groupSelectionTableView reloadData];
}

- (void)changeDataForSpecificMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
                          inVideoGroup:(VLCLibraryVideoGroup)group
                        arrayOperation:(void(^)(const NSMutableArray*, const NSUInteger))arrayOperation
                     completionHandler:(void(^)(const NSIndexSet*))completionHandler
{
    NSMutableArray *groupMutableCopyArray;
    switch(group) {
        case VLCLibraryVideoLibraryGroup:
            groupMutableCopyArray = [_libraryArray mutableCopy];
            break;
        case VLCLibraryVideoRecentsGroup:
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
        case VLCLibraryVideoLibraryGroup:
            _libraryArray = [groupMutableCopyArray copy];
            break;
        case VLCLibraryVideoRecentsGroup:
            _recentsArray = [groupMutableCopyArray copy];
            break;
        default:
            return;
    }

    NSIndexSet * const rowIndexSet = [NSIndexSet indexSetWithIndex:mediaItemIndex];
    completionHandler(rowIndexSet);
}

- (void)reloadDataForMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
                  inVideoGroup:(VLCLibraryVideoGroup)group
{
    [self changeDataForSpecificMediaItem:mediaItem
                            inVideoGroup:group
                          arrayOperation:^(NSMutableArray * const mediaArray, const NSUInteger mediaItemIndex) {

        [mediaArray replaceObjectAtIndex:mediaItemIndex withObject:mediaItem];

    } completionHandler:^(NSIndexSet * const rowIndexSet) {

        // Don't regenerate the groups by index as these do not change according to the notification
        // Stick to the selection table view
        NSRange columnRange = NSMakeRange(0, self->_groupsTableView.numberOfColumns);
        NSIndexSet * const columnIndexSet = [NSIndexSet indexSetWithIndexesInRange:columnRange];
        [self->_groupSelectionTableView reloadDataForRowIndexes:rowIndexSet columnIndexes:columnIndexSet];

    }];
}

- (void)deleteDataForMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
                  inVideoGroup:(VLCLibraryVideoGroup)group
{
    [self changeDataForSpecificMediaItem:mediaItem
                            inVideoGroup:group
                          arrayOperation:^(NSMutableArray * const mediaArray, const NSUInteger mediaItemIndex) {

        [mediaArray removeObjectAtIndex:mediaItemIndex];

    } completionHandler:^(NSIndexSet * const rowIndexSet){

        // Don't regenerate the groups by index as these do not change according to the notification
        // Stick to the selection table view
        [self->_groupSelectionTableView removeRowsAtIndexes:rowIndexSet withAnimation:NSTableViewAnimationSlideUp];

    }];
}

#pragma mark - table view data source and delegation

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (tableView == _groupsTableView) {
        return 2;
    } else if (tableView == _groupSelectionTableView && _groupsTableView.selectedRow > -1) {
        switch(_groupsTableView.selectedRow + 1) { // Group 0 is invalid so add one
            case VLCLibraryVideoRecentsGroup:
                return _recentsArray.count;
            case VLCLibraryVideoLibraryGroup:
                return _libraryArray.count;
            default:
                NSAssert(1, @"Reached unreachable case for video library section");
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
    if (tableView == _groupSelectionTableView && _groupsTableView.selectedRow > -1) {
        switch(_groupsTableView.selectedRow + 1) { // Group 0 is invalid so add one
            case VLCLibraryVideoRecentsGroup:
                return _recentsArray[row];
            case VLCLibraryVideoLibraryGroup:
                return _libraryArray[row];
            default:
                NSAssert(1, @"Reached unreachable case for video library section");
                break;
        }
    }

    return nil;
}

@end
