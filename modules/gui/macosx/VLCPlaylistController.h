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

/**
 * The vlc core playlist controlled by the instance of this class
 * @note You DO SOMETHING WRONG if you need to access this!
 */
@property (readonly) vlc_playlist_t *p_playlist;

/**
 * The playlist model caching the contents of the playlist controlled by
 * the instance of this class.
 */
@property (readonly) VLCPlaylistModel *playlistModel;

/**
 * The datasource instance used to actually display the playlist.
 */
@property (readwrite, assign) VLCPlaylistDataSource *playlistDataSource;

/**
 * Index of the current playlist item
 */
@property (readonly) size_t currentPlaylistIndex;

/**
 * indicates whether there is a previous item in the list the user could go back to
 */
@property (readonly) BOOL hasPreviousPlaylistItem;

/**
 * indicates whether there is a next item in the list the user could move on to
 */
@property (readonly) BOOL hasNextPlaylistItem;

/**
 * Simplified version to add new items to the end of the current playlist
 * @param array array of items. Each item is a Dictionary with meta info.
 */
- (void)addPlaylistItems:(NSArray*)array;

/**
 * Add new items to the playlist, at specified index.
 * @param itemArray array of items. Each item is a Dictionary with meta info.
 * @param insertionIndex index for new items, -1 for appending at end
 * @param startPlayback starts playback of first item if true
 */
- (void)addPlaylistItems:(NSArray*)itemArray
              atPosition:(size_t)insertionIndex
           startPlayback:(BOOL)startPlayback;

/**
 * Remove the item at the given index (if any)
 * @param index the index to remove
 */
- (void)removeItemAtIndex:(size_t)index;

/**
 * Clear the entire playlist
 */
- (void)clearPlaylist;

/**
 * Start the playlist
 * @return Returns VLC_SUCCESS on success.
 */
- (int)startPlaylist;

/**
 * Play the previous item in the list (if any)
 * @return Returns VLC_SUCCESS on success.
 */
- (int)playPreviousItem;

/**
 * Play the item at the given index (if any)
 * @param index the index to play
 * @return Returns VLC_SUCCESS on success.
 */
- (int)playItemAtIndex:(size_t)index;

/**
 * Play the previous item in the list (if any)
 * @return Returns VLC_SUCCESS on success.
 */
- (int)playNextItem;

/**
 * Stop playback of the playlist and destroy all facilities
 */
- (void)stopPlayback;

/**
 * Pause playback of the playlist while keeping all facilities
 */
- (void)pausePlayback;

/**
 * Resume playback of the playlist if it was paused
 */
- (void)resumePlayback;

@end

NS_ASSUME_NONNULL_END
