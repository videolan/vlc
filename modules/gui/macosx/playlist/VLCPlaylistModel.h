/*****************************************************************************
 * VLCPlaylistModel.h: MacOS X interface module
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

@class VLCPlaylistController;
@class VLCPlaylistItem;

NS_ASSUME_NONNULL_BEGIN

@interface VLCPlaylistModel : NSObject

@property (readwrite, assign) VLCPlaylistController *playlistController;
@property (readonly) NSUInteger numberOfPlaylistItems;

- (void)dropExistingData;
- (VLCPlaylistItem *)playlistItemAtIndex:(NSInteger)index;
- (void)addItems:(NSArray *)array;
- (void)addItems:(NSArray *)array atIndex:(size_t)index count:(size_t)count;
- (void)moveItemAtIndex:(size_t)index toTarget:(size_t)target;
- (void)removeItemsInRange:(NSRange)range;
- (void)updateItemAtIndex:(size_t)index;

@end

NS_ASSUME_NONNULL_END
