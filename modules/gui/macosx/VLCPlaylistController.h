/*****************************************************************************
 * VLCPlaylistController.h: MacOS X interface module
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

#import <Foundation/Foundation.h>
#import <vlc_playlist.h>

NS_ASSUME_NONNULL_BEGIN

@class VLCPlaylistModel;
@class VLCPlaylistDataSource;

@interface VLCPlaylistController : NSObject

@property (readonly) vlc_playlist_t *p_playlist;
@property (readonly) VLCPlaylistModel *playlistModel;
@property (readwrite, assign) VLCPlaylistDataSource *playlistDataSource;

/**
 * Simplified version to add new items at the end of the current playlist
 * @param array array of items. Each item is a Dictionary with meta info.
 */
- (void)addPlaylistItems:(NSArray*)array;

/**
 * Adds new items to the playlist, at specified parent node and index.
 * @param o_array array of items. Each item is a Dictionary with meta info.
 * @param i_plItemId parent playlist node id, -1 for default playlist
 * @param i_position index for new items, -1 for appending at end
 * @param b_start starts playback of first item if true
 */
- (void)addPlaylistItems:(NSArray*)itemArray
              atPosition:(size_t)insertionIndex
           startPlayback:(BOOL)b_start;

- (void)playItemAtIndex:(size_t)index;

- (void)removeItemAtIndex:(size_t)index;

@end

NS_ASSUME_NONNULL_END
