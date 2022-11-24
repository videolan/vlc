/*****************************************************************************
 * VLCLibraryAlbumTracksDataSource.m: MacOS X interface module
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

#import "VLCLibraryAlbumTracksDataSource.h"

#import "extensions/NSFont+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "views/VLCImageView.h"
#import "views/VLCTrackingView.h"
#import "main/VLCMain.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/audio-library/VLCLibraryAlbumTracksDataSource.h"
#import "library/audio-library/VLCLibrarySongTableCellView.h"

const CGFloat VLCLibraryTracksRowHeight = 40.;

@interface VLCLibraryAlbumTracksDataSource ()
{
    NSArray *_tracks;
}
@end

@implementation VLCLibraryAlbumTracksDataSource

- (void)setRepresentedAlbum:(VLCMediaLibraryAlbum *)representedAlbum
{
    _representedAlbum = representedAlbum;
    _tracks = [_representedAlbum tracksAsMediaItems];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_representedAlbum != nil) {
        return _representedAlbum.numberOfTracks;
    }

    return 0;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibrarySongTableCellView *cellView = [tableView makeViewWithIdentifier:VLCAudioLibrarySongCellIdentifier owner:self];

    if (cellView == nil) {
        cellView = [VLCLibrarySongTableCellView fromNibWithOwner:self];
        cellView.identifier = VLCAudioLibrarySongCellIdentifier;
    }

    cellView.representedMediaItem = (VLCMediaLibraryMediaItem *)[self libraryItemAtRow:row];
    return cellView;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
{
    return _tracks[row];
}

@end
