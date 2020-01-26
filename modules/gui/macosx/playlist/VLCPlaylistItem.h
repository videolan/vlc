/*****************************************************************************
 * VLCPlaylistItem.h: MacOS X interface module
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

@class VLCInputItem;
@class VLCMediaLibraryMediaItem;

extern NSString *VLCPlaylistItemPasteboardType;

@interface VLCPlaylistItem : NSObject

@property (readonly) vlc_playlist_item_t *playlistItem;
@property (readonly) uint64_t uniqueID;
@property (readwrite, retain) NSString *title;
@property (readonly, copy, nullable) NSURL *url;
@property (readonly, copy, nullable) NSString *path;
@property (readwrite, assign) vlc_tick_t duration;
@property (readonly, nullable) VLCInputItem *inputItem;
@property (readonly, nullable) VLCMediaLibraryMediaItem *mediaLibraryItem;

@property (readwrite, retain, nullable) NSString *artistName;
@property (readwrite, retain, nullable) NSString *albumName;
@property (readonly, copy) NSImage *artworkImage;

- (instancetype)initWithPlaylistItem:(vlc_playlist_item_t *)p_item;
- (void)updateRepresentation;

@end

NS_ASSUME_NONNULL_END
