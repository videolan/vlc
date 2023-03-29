/*****************************************************************************
 * VLCBookmarksTableViewDataSource.m: MacOS X interface module bookmarking functionality
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

#import "VLCBookmarksTableViewDelegate.h"

#import "VLCBookmark.h"
#import "VLCBookmarksTableViewDataSource.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSView+VLCAdditions.h"

NSString * const VLCBookmarksTableViewCellIdentifier = @"VLCBookmarksTableViewCellIdentifier";

@implementation VLCBookmarksTableViewDelegate

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCBookmarksTableViewDataSource * const vlcDataSource = (VLCBookmarksTableViewDataSource *)tableView.dataSource;
    NSAssert(vlcDataSource != nil, @"Should be a valid data source");

    VLCBookmark * const bookmark = [vlcDataSource bookmarkForRow:row];
    NSString * const identifier = [tableColumn identifier];

    if ([identifier isEqualToString:@"name"]) {
        return bookmark.bookmarkName;
    } else if ([identifier isEqualToString:@"description"]) {
        return bookmark.bookmarkDescription;
    } else if ([identifier isEqualToString:@"time_offset"]) {
        return [NSString stringWithTime:bookmark.bookmarkTime / 1000];
    }

    return @"";
}

@end
