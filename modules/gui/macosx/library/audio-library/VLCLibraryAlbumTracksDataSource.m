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

#import "extensions/NSPasteboardItem+VLCAdditions.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/audio-library/VLCLibrarySongTableCellView.h"

const CGFloat VLCLibraryTracksRowHeight = 40.;

@interface VLCLibraryAlbumTracksDataSource ()

@property (readwrite, atomic) NSArray<VLCMediaLibraryMediaItem*> *tracks;

@end

@implementation VLCLibraryAlbumTracksDataSource

- (void)setRepresentedAlbum:(VLCMediaLibraryAlbum *)representedAlbum
{
    _representedAlbum = representedAlbum;
    self.tracks = [_representedAlbum tracksAsMediaItems];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_representedAlbum != nil) {
        return _representedAlbum.numberOfTracks;
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
    return self.tracks[row];
}

@end
