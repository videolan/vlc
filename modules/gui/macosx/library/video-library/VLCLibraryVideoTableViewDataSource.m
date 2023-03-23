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
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelVideoMediaListUpdated
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelRecentMediaListUpdated
                                 object:nil];
    }
    return self;
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    [self reloadData];
}

- (void)reloadData
{
    if(!_libraryModel) {
        return;
    }
    
    [_collectionViewFlowLayout resetLayout];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_recentsArray = [self->_libraryModel listOfRecentMedia];
        self->_libraryArray = [self->_libraryModel listOfVideoMedia];
        [self->_groupsTableView reloadData];
        [self->_groupSelectionTableView reloadData];
    });
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
