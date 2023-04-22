/*****************************************************************************
 * VLCLibraryModel.h: MacOS X interface module
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

#import <Cocoa/Cocoa.h>

#import <vlc_media_library.h>

NS_ASSUME_NONNULL_BEGIN

@class VLCMediaLibraryMediaItem;
@class VLCMediaLibraryArtist;
@class VLCMediaLibraryAlbum;
@class VLCMediaLibraryGenre;
@class VLCMediaLibraryEntryPoint;

extern NSString * const VLCLibraryModelArtistListUpdated;
extern NSString * const VLCLibraryModelAlbumListUpdated;
extern NSString * const VLCLibraryModelGenreListUpdated;
extern NSString * const VLCLibraryModelListOfMonitoredFoldersUpdated;
extern NSString * const VLCLibraryModelMediaItemThumbnailGenerated;

extern NSString * const VLCLibraryModelAudioMediaListReset;
extern NSString * const VLCLibraryModelVideoMediaListReset;
extern NSString * const VLCLibraryModelRecentsMediaListReset;

extern NSString * const VLCLibraryModelAudioMediaItemDeleted;
extern NSString * const VLCLibraryModelVideoMediaItemDeleted;
extern NSString * const VLCLibraryModelRecentsMediaItemDeleted;

extern NSString * const VLCLibraryModelAudioMediaItemUpdated;
extern NSString * const VLCLibraryModelVideoMediaItemUpdated;
extern NSString * const VLCLibraryModelRecentsMediaItemUpdated;
extern NSString * const VLCLibraryModelAlbumUpdated;

@interface VLCLibraryModel : NSObject

+ (NSUInteger)modelIndexFromModelItemNotification:(NSNotification * const)aNotification;

- (instancetype)initWithLibrary:(vlc_medialibrary_t *)library;

@property (readonly) size_t numberOfAudioMedia;
@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *listOfAudioMedia;

@property (readonly) size_t numberOfArtists;
@property (readonly) NSArray <VLCMediaLibraryArtist *> *listOfArtists;

@property (readonly) size_t numberOfAlbums;
@property (readonly) NSArray <VLCMediaLibraryAlbum *> *listOfAlbums;

@property (readonly) size_t numberOfGenres;
@property (readonly) NSArray <VLCMediaLibraryGenre *> *listOfGenres;

@property (readonly) size_t numberOfVideoMedia;
@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *listOfVideoMedia;

@property (readonly) size_t numberOfRecentMedia;
@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *listOfRecentMedia;

@property (readonly) NSArray <VLCMediaLibraryEntryPoint *> *listOfMonitoredFolders;

- (nullable NSArray <VLCMediaLibraryAlbum *>*)listAlbumsOfParentType:(enum vlc_ml_parent_type)parentType forID:(int64_t)ID;

- (void)sortByCriteria:(enum vlc_ml_sorting_criteria_t)sortCriteria andDescending:(bool)descending;
- (void)filterByString:(NSString*)filterString;

@end

NS_ASSUME_NONNULL_END
