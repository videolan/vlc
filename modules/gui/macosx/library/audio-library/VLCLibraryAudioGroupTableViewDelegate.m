/*****************************************************************************
 * VLCLibraryAudioGroupTableViewDelegate.m: MacOS X interface module
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

#import "VLCLibraryAudioGroupTableViewDelegate.h"

#import "extensions/NSView+VLCAdditions.h"

#import "VLCLibraryAlbumTableCellView.h"
#import "VLCLibraryAudioDataSource.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"

@implementation VLCLibraryAudioGroupTableViewDelegate

- (instancetype)init
{
    self = [super init];
    if (self) {
        self.cellViewIdentifier = VLCAudioLibraryCellIdentifier;
    }
    return self;
}

- (NSView<VLCLibraryTableCellViewProtocol> *)makeCellView
{
    return [VLCLibraryAlbumTableCellView fromNibWithOwner:self];
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)rowIndex
{
    // We use this with nested table views, since the table view cell is the VLCLibraryAlbumTableCellView.
    // We don't want to select the outer cell, only the inner cells in the album view's table.
    return NO;
}

- (CGFloat)tableView:(NSTableView *)tableView heightOfRow:(NSInteger)row
{
    id dataSource = tableView.dataSource;
    id libraryItem = [dataSource libraryItemAtRow:row forTableView:tableView];

    if ([libraryItem isKindOfClass:[VLCMediaLibraryAlbum class]]) {
        return [VLCLibraryAlbumTableCellView heightForAlbum:libraryItem];
    }
    return VLCLibraryAlbumTableCellView.defaultHeight;
}

@end
