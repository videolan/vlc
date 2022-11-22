/*****************************************************************************
 * VLCLibraryVideoCollectionViewsDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#import "VLCLibraryVideoCollectionViewsDataSource.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryModel.h"

#import "library/video-library/VLCLibraryVideoCollectionViewTableViewCell.h"
#import "library/video-library/VLCLibraryVideoCollectionViewTableViewCellDataSource.h"
#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "views/VLCSubScrollView.h"

@interface VLCLibraryVideoCollectionViewsDataSource()
{
    NSArray *_collectionViewCells;
}
@end

@implementation VLCLibraryVideoCollectionViewsDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(cellRowHeightChanged:)
                               name:NSViewFrameDidChangeNotification
                             object:nil];
    [self generateCollectionViewCells];
}

- (void)generateCollectionViewCells
{
    NSMutableArray *collectionViewCells = [[NSMutableArray alloc] init];
    NSUInteger firstRow = VLCLibraryVideoRecentsGroup;
    NSUInteger lastRow = VLCLibraryVideoLibraryGroup;

    for (NSUInteger i = firstRow; i <= lastRow; ++i) {
        VLCLibraryVideoCollectionViewTableViewCell *cellView = [[VLCLibraryVideoCollectionViewTableViewCell alloc] init];
        cellView.identifier = @"VLCLibraryVideoCollectionViewTableViewCellIdentifier";
        cellView.scrollView.parentScrollView = _collectionsTableViewScrollView;

        [collectionViewCells addObject:cellView];
    }

    _collectionViewCells = collectionViewCells;
}

- (void)reloadData
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self generateCollectionViewCells];

        if (self->_collectionsTableView) {
            [self->_collectionsTableView reloadData];

            // HACK: On app init the vertical collection views will not get their heights updated properly.
            // So let's schedule a check when the table view is set to correct this issue...
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
                NSMutableIndexSet *indexSet = [[NSMutableIndexSet alloc] init];

                for (NSUInteger i = 0; i < self->_collectionViewCells.count; ++i) {
                    [indexSet addIndex:i];
                }

                [self->_collectionsTableView noteHeightOfRowsWithIndexesChanged:indexSet];
            });
        }
    });
}

- (void)cellRowHeightChanged:(NSNotification *)notification
{
    if (!_collectionsTableView || [notification.object class] != [VLCLibraryVideoCollectionViewTableViewCell class]) {
        return;
    }

    VLCLibraryVideoCollectionViewTableViewCell *cellView = (VLCLibraryVideoCollectionViewTableViewCell *)notification.object;
    if (!cellView) {
        return;
    }

    NSUInteger cellIndex = [_collectionViewCells indexOfObjectPassingTest:^BOOL(VLCLibraryVideoCollectionViewTableViewCell* existingCell, NSUInteger idx, BOOL *stop) {
        return existingCell.groupDescriptor.group == cellView.groupDescriptor.group;
    }];
    if (cellIndex == NSNotFound) {
        // Let's try a backup
        cellIndex = cellView.groupDescriptor.group - 1;
    }

    [self scheduleRowHeightNoteChangeForRow:cellView.groupDescriptor.group - 1];
}

- (void)scheduleRowHeightNoteChangeForRow:(NSUInteger)row
{
    NSAssert(row >= 0 && row < _collectionViewCells.count, @"Invalid row index passed to scheduleRowHeightNoteChangeForRow");
    dispatch_async(dispatch_get_main_queue(), ^{
        NSIndexSet *indexSetToNote = [NSIndexSet indexSetWithIndex:row];
        [self->_collectionsTableView noteHeightOfRowsWithIndexesChanged:indexSetToNote];
    });
}

- (void)setCollectionsTableView:(NSTableView *)collectionsTableView
{
    _collectionsTableView = collectionsTableView;
    _collectionsTableView.dataSource = self;
    _collectionsTableView.delegate = self;

    _collectionsTableView.intercellSpacing = NSMakeSize(20., 20.);
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _collectionViewCells.count;
}

- (NSView *)tableView:(NSTableView *)tableView
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    VLCLibraryVideoCollectionViewTableViewCell* cellView = _collectionViewCells[row];
    cellView.videoGroup = row + 1;
    return cellView;
}

- (CGFloat)tableView:(NSTableView *)tableView
         heightOfRow:(NSInteger)row
{
    CGFloat fallback = _collectionViewItemSize.height +
                       _collectionViewSectionInset.top +
                       _collectionViewSectionInset.bottom;
    
    VLCLibraryVideoCollectionViewTableViewCell* cellView = _collectionViewCells[row];
    if (cellView && cellView.collectionView.collectionViewLayout.collectionViewContentSize.height > fallback) {
        return cellView.collectionView.collectionViewLayout.collectionViewContentSize.height;
    }

    if (fallback <= 0) {
        NSLog(@"Unable to provide reasonable fallback or accurate rowheight -- providing rough rowheight");
        return 300;
    }

    return fallback;
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)rowIndex
{
    return NO;
}

@end
