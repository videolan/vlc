/*****************************************************************************
 * VLCLibraryVideoTableViewDelegate.m MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#import "VLCLibraryVideoTableViewDelegate.h"

#import "VLCLibraryVideoTableViewDataSource.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"

@implementation VLCLibraryVideoTableViewDelegate

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    if (![tableView.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)]) {
        return nil;
    }

    NSObject<VLCLibraryTableViewDataSource> * const vlcDataSource = (NSObject<VLCLibraryTableViewDataSource>*)tableView.dataSource;
    NSAssert(vlcDataSource != nil, @"Should be a valid data source");

    VLCLibraryTableCellView *cellView = [tableView makeViewWithIdentifier:@"VLCVideoLibraryTableViewCellIdentifier" owner:self];

    if (!cellView) {
        cellView = [VLCLibraryTableCellView fromNibWithOwner:self];
        cellView.identifier = @"VLCVideoLibraryTableViewCellIdentifier";
    }

    const id<VLCMediaLibraryItemProtocol> mediaItem = [vlcDataSource libraryItemAtRow:row forTableView:tableView];
    if (mediaItem != nil) {
        cellView.representedItem = mediaItem;
    } else if ([vlcDataSource isKindOfClass:[VLCLibraryVideoTableViewDataSource class]]) {
        VLCLibraryVideoTableViewDataSource * const videoTableViewDataSource = (VLCLibraryVideoTableViewDataSource *)vlcDataSource;
        NSTableView * const groupsTableView = videoTableViewDataSource.groupsTableView;

        if (tableView == groupsTableView) {
            cellView.representedVideoLibrarySection = row;
        }
    }

    return cellView;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSParameterAssert(notification);
    NSTableView *tableView = (NSTableView *)notification.object;
    NSAssert(tableView, @"Must be a valid table view");
    NSInteger selectedRow = tableView.selectedRow;

    if (![tableView.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)]) {
        return;
    }

    NSObject<VLCLibraryTableViewDataSource> * const vlcDataSource = (NSObject<VLCLibraryTableViewDataSource>*)tableView.dataSource;
    NSAssert(vlcDataSource != nil, @"Should be a valid data source");

    if ([vlcDataSource isKindOfClass:[VLCLibraryVideoTableViewDataSource class]]) {
        VLCLibraryVideoTableViewDataSource * const videoTableViewDataSource = (VLCLibraryVideoTableViewDataSource *)vlcDataSource;
        NSTableView * const groupsTableView = videoTableViewDataSource.groupsTableView;

        if (tableView == videoTableViewDataSource.groupsTableView) {
            [videoTableViewDataSource.groupSelectionTableView reloadData];
        }
    }
}

@end
