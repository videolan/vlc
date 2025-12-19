/*****************************************************************************
 * VLCLibraryNameCache.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryNameCache : NSObject

+ (instancetype)sharedInstance;

- (nullable NSString *)albumTitleForID:(int64_t)albumID;
- (nullable NSString *)albumArtistForID:(int64_t)albumID;
- (nullable NSString *)genreNameForID:(int64_t)genreID;

- (void)invalidateAlbumWithID:(int64_t)albumID;
- (void)invalidateGenreWithID:(int64_t)genreID;
- (void)invalidateAll;

@end

NS_ASSUME_NONNULL_END
