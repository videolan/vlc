/*****************************************************************************
 * VLCLibraryAudioTableViewDelegate.m: MacOS X interface module
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

#import "VLCLibraryAudioTableViewDelegate.h"

#import "VLCLibraryAlbumTableCellView.h"
#import "VLCLibraryAudioDataSource.h"
#import "VLCLibrarySongTableCellView.h"
#import "VLCLibrarySongsTableViewSongPlayingTableCellView.h"

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"

@implementation VLCLibraryAudioTableViewDelegate

- (NSView *)tableView:(NSTableView *)tableView
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    if (![tableView.dataSource conformsToProtocol:@protocol(VLCLibraryTableViewDataSource)]) {
        return nil;
    }

    NSObject<VLCLibraryTableViewDataSource> * const vlcDataSource = (NSObject<VLCLibraryTableViewDataSource>*)tableView.dataSource;
    NSAssert(vlcDataSource != nil, @"Should be a valid data source");

    const id<VLCMediaLibraryItemProtocol> libraryItem = [vlcDataSource libraryItemAtRow:row forTableView:tableView];

    // The table view for songs in the list view mode of the audio library is different from the other audio groupings
    // and we use a vanilla NSTableView created in the VLCLibraryWindow XIB for it
    if ([tableView.identifier isEqualToString:@"VLCLibrarySongsTableViewIdentifier"]) {
        const NSString * const columnIdentifier = tableColumn.identifier;

        if (![libraryItem isKindOfClass:[VLCMediaLibraryMediaItem class]]) {
            return nil;
        }

        const VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)libraryItem;

        if (!mediaItem) {
            return nil;
        }

        const VLCMediaLibraryAlbum * const album = [VLCMediaLibraryAlbum albumWithID:mediaItem.albumID];
        const VLCMediaLibraryGenre * const genre = [VLCMediaLibraryGenre genreWithID:mediaItem.genreID];

        NSString *cellText = @"";
        NSString *cellIdentifier = @"";

        if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewSongPlayingColumnIdentifier]) {
            VLCLibrarySongsTableViewSongPlayingTableCellView *cellView = (VLCLibrarySongsTableViewSongPlayingTableCellView*)[tableView makeViewWithIdentifier:@"VLCLibrarySongsTableViewSongPlayingTableCellViewIdentifier" owner:self];
            NSAssert(cellView, @"Unexpectedly received null cellview");
            cellView.representedMediaItem = (VLCMediaLibraryMediaItem *)mediaItem;
            return cellView;
        } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewTitleColumnIdentifier]) {
            cellIdentifier = @"VLCLibrarySongsTableViewTitleTableCellViewIdentifier";
            cellText = mediaItem.title;
        } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewDurationColumnIdentifier]) {
            cellIdentifier = @"VLCLibrarySongsTableViewDurationTableCellViewIdentifier";
            cellText = mediaItem.durationString;
        } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewArtistColumnIdentifier]) {
            cellIdentifier = @"VLCLibrarySongsTableViewArtistTableCellViewIdentifier";
            cellText = album.artistName.length == 0 ? @"" : album.artistName;
        } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewAlbumColumnIdentifier]) {
            cellIdentifier = @"VLCLibrarySongsTableViewAlbumTableCellViewIdentifier";
            cellText = album.title.length == 0 ? @"" : album.title;
        } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewGenreColumnIdentifier]) {
            cellIdentifier = @"VLCLibrarySongsTableViewGenreTableCellViewIdentifier";
            cellText = genre.name.length == 0 ? @"" : genre.name;
        } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewPlayCountColumnIdentifier]) {
            cellIdentifier = @"VLCLibrarySongsTableViewPlayCountTableCellViewIdentifier";
            cellText = [@(mediaItem.playCount) stringValue];
        } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewYearColumnIdentifier]) {
            cellIdentifier = @"VLCLibrarySongsTableViewYearTableCellViewIdentifier";
            cellText = [@(mediaItem.year) stringValue];
        } else {
            NSAssert(true, @"Received unknown column identifier %@", columnIdentifier);
        }

        NSTableCellView *cellView = [tableView makeViewWithIdentifier:cellIdentifier owner:self];
        cellView.textField.stringValue = cellText;
        return cellView;
    }

    VLCLibraryTableCellView *cellView = [tableView makeViewWithIdentifier:VLCAudioLibraryCellIdentifier owner:vlcDataSource];

    if (cellView == nil) {
        cellView = [VLCLibraryTableCellView fromNibWithOwner:vlcDataSource];
        cellView.identifier = VLCAudioLibraryCellIdentifier;
    }

    [cellView setRepresentedItem:libraryItem];
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

    if ([vlcDataSource isKindOfClass:[VLCLibraryAudioDataSource class]]) {
        [(VLCLibraryAudioDataSource*)vlcDataSource tableView:tableView selectRow:selectedRow];
    }
}

@end
