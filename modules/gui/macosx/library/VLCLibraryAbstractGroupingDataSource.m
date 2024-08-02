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

#import "library/VLCLibraryDataTypes.h"

@implementation VLCLibraryAbstractGroupingDataSource

- (NSArray<id<VLCMediaLibraryItemProtocol>> *)backingArray
{
    [self doesNotRecognizeSelector:_cmd];
    return nil;
}

- (void)reloadData
{
    [(VLCLibraryCollectionViewFlowLayout *)self.collectionView.collectionViewLayout resetLayout];
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
        return self.backingArray.count;
    }

    const NSInteger selectedMasterRow = self.masterTableView.selectedRow;
    if (tableView == self.detailTableView && selectedMasterRow > -1) {
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

@end
