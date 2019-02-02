/*****************************************************************************
 * VLCPlaylistModel.m: MacOS X interface module
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

#import "VLCPlaylistModel.h"
#import "VLCPlaylistController.h"
#import "VLCPlaylistItem.h"
#import <vlc_common.h>

@interface VLCPlaylistModel ()
{
    NSMutableArray *_playlistArray;
}
@end

@implementation VLCPlaylistModel

- (instancetype)init
{
    self = [super init];
    if (self) {
        _playlistArray = [[NSMutableArray alloc] init];
    }
    return self;
}

- (size_t)numberOfPlaylistItems
{
    size_t ret = 0;

    vlc_playlist_t *p_playlist = self.playlistController.p_playlist;

    ret = vlc_playlist_Count(p_playlist);

    return ret;
}

- (void)dropExistingData
{
    [_playlistArray removeAllObjects];
}

- (VLCPlaylistItem *)playlistItemAtIndex:(NSInteger)index
{
    return _playlistArray[index];
}

- (void)addItems:(NSArray *)array
{
    [_playlistArray addObjectsFromArray:array];
}

- (void)addItems:(NSArray *)array atIndex:(size_t)index count:(size_t)count
{
    [_playlistArray insertObjects:array atIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(index, count)]];
}

- (void)removeItemsInRange:(NSRange)range
{
    [_playlistArray removeObjectsInRange:range];
}

- (void)updateItemAtIndex:(size_t)index
{
    VLCPlaylistItem *item = _playlistArray[index];
    [item updateRepresentation];
}

@end
