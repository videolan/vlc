/*****************************************************************************
 * VLCLibraryFavoritesTableViewDelegate.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import "VLCLibraryFavoritesTableViewDelegate.h"
#include <Foundation/Foundation.h>

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableCellViewProtocol.h"
#import "library/VLCLibraryTableViewDataSource.h"
#import "library/audio-library/VLCLibraryAlbumTableCellView.h"

@implementation VLCLibraryFavoritesTableViewDelegate

- (NSView *)tableView:(NSTableView *)tableView
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    if (![tableView.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)]) {
        return [super tableView:tableView viewForTableColumn:tableColumn row:row];
    }

    NSObject<VLCLibraryTableViewDataSource> * const vlcDataSource = 
        (NSObject<VLCLibraryTableViewDataSource>*)tableView.dataSource;
    NSAssert(vlcDataSource != nil, @"Should be a valid data source");

    id<VLCMediaLibraryItemProtocol> const libraryItem =
        [vlcDataSource libraryItemAtRow:row forTableView:tableView];
    if (libraryItem == nil)
        return nil;

    VLCLibraryRepresentedItem * const representedItem = 
        [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem
                                             parentType:vlcDataSource.currentParentType];

    const BOOL isAlbum = [libraryItem isKindOfClass:VLCMediaLibraryAlbum.class];

    NSString * const cellIdentifier =
        isAlbum ? VLCAudioLibraryCellIdentifier : VLCLibraryTableCellViewIdentifier;
    NSView<VLCLibraryTableCellViewProtocol> * const cellView =
        (NSView<VLCLibraryTableCellViewProtocol> *)[tableView makeViewWithIdentifier:cellIdentifier 
                                                                               owner:self];
    if (cellView == nil)
        return [super tableView:tableView viewForTableColumn:tableColumn row:row];

    [cellView setRepresentedItem:representedItem];
    return cellView;
}

- (CGFloat)tableView:(NSTableView *)tableView heightOfRow:(NSInteger)row
{
    if (![tableView.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)]) {
        return tableView.rowHeight;
    }

    NSObject<VLCLibraryTableViewDataSource> * const vlcDataSource = 
        (NSObject<VLCLibraryTableViewDataSource>*)tableView.dataSource;
    id<VLCMediaLibraryItemProtocol> const libraryItem = [vlcDataSource libraryItemAtRow:row 
                                                                           forTableView:tableView];

    if ([libraryItem isKindOfClass:VLCMediaLibraryAlbum.class]) {
        return VLCLibraryAlbumTableCellView.defaultHeight;
    }
    return tableView.rowHeight;
}

@end