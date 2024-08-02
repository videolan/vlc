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

#import "VLCLibraryVideoDataSource.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"

#import "library/groups-library/VLCLibraryGroupsDataSource.h"

@implementation VLCLibraryVideoTableViewDelegate

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.cellViewIdentifier = @"VLCVideoLibraryTableViewCellIdentifier";
        self.cellViewClass = [VLCLibraryTableCellView class];
    }
    return self;
}

- (NSView *)tableView:(NSTableView *)tableView 
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    VLCLibraryTableCellView * const cellView =
        (VLCLibraryTableCellView *)[super tableView:tableView
                                 viewForTableColumn:tableColumn
                                                row:row];
    NSParameterAssert(cellView != nil);

    if ([tableView.dataSource isKindOfClass:[VLCLibraryVideoDataSource class]]) {
        VLCLibraryVideoDataSource * const videoTableViewDataSource =
            (VLCLibraryVideoDataSource *)tableView.dataSource;
        NSParameterAssert(videoTableViewDataSource != nil);
        if (tableView == videoTableViewDataSource.masterTableView) {
            cellView.representedVideoLibrarySection = row;
        }
    }

    return cellView;
}

@end
