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

#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModelChangeDelegate.h"

NS_ASSUME_NONNULL_BEGIN

extern NSString * const VLCLibraryModelArtistListReset;
extern NSString * const VLCLibraryModelAlbumListReset;
extern NSString * const VLCLibraryModelGenreListReset;
extern NSString * const VLCLibraryModelPlaylistListReset;
extern NSString * const VLCLibraryModelListOfMonitoredFoldersUpdated;
extern NSString * const VLCLibraryModelMediaItemThumbnailGenerated;

extern NSString * const VLCLibraryModelAudioMediaListReset;
extern NSString * const VLCLibraryModelVideoMediaListReset;
extern NSString * const VLCLibraryModelRecentsMediaListReset;
extern NSString * const VLCLibraryModelRecentAudioMediaListReset;
extern NSString * const VLCLibraryModelListOfShowsReset;
extern NSString * const VLCLibraryModelListOfGroupsReset;

extern NSString * const VLCLibraryModelAudioMediaItemDeleted;
extern NSString * const VLCLibraryModelVideoMediaItemDeleted;
extern NSString * const VLCLibraryModelRecentsMediaItemDeleted;
extern NSString * const VLCLibraryModelRecentAudioMediaItemDeleted;
extern NSString * const VLCLibraryModelAlbumDeleted;
extern NSString * const VLCLibraryModelArtistDeleted;
extern NSString * const VLCLibraryModelGenreDeleted;
extern NSString * const VLCLibraryModelGroupDeleted;
extern NSString * const VLCLibraryModelPlaylistDeleted;

extern NSString * const VLCLibraryModelAudioMediaItemUpdated;
extern NSString * const VLCLibraryModelVideoMediaItemUpdated;
extern NSString * const VLCLibraryModelRecentsMediaItemUpdated;
extern NSString * const VLCLibraryModelRecentAudioMediaItemUpdated;
extern NSString * const VLCLibraryModelAlbumUpdated;
extern NSString * const VLCLibraryModelArtistUpdated;
extern NSString * const VLCLibraryModelGenreUpdated;
extern NSString * const VLCLibraryModelGroupUpdated;
extern NSString * const VLCLibraryModelPlaylistUpdated;

@interface VLCLibraryModel : NSObject

+ (NSUInteger)modelIndexFromModelItemNotification:(NSNotification * const)aNotification;

- (instancetype)initWithLibrary:(vlc_medialibrary_t *)library;

@property (readonly) VLCLibraryModelChangeDelegate *changeDelegate;

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

@property (readonly) size_t numberOfPlaylists;
@property (readonly) NSArray <VLCMediaLibraryPlaylist *> *listOfPlaylists;

@property (readwrite) uint32_t recentMediaLimit;
@property (readonly) size_t numberOfRecentMedia;
@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *listOfRecentMedia;

@property (readwrite) uint32_t recentAudioMediaLimit;
@property (readonly) size_t numberOfRecentAudioMedia;
@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *listOfRecentAudioMedia;

@property (readonly) size_t numberOfShows;
@property (readonly) NSArray <VLCMediaLibraryShow *> *listOfShows;

@property (readonly) size_t numberOfGroups;
@property (readonly) NSArray <VLCMediaLibraryGroup *> *listOfGroups;

@property (readonly) NSArray <VLCMediaLibraryEntryPoint *> *listOfMonitoredFolders;

@property (readonly) NSDictionary<NSNumber *, NSString *> *albumDict;
@property (readonly) NSDictionary<NSNumber *, NSString *> *artistDict;
@property (readonly) NSDictionary<NSNumber *, NSString *> *genreDict;

@property (readwrite, nonatomic) NSString *filterString;

- (nullable NSArray<VLCMediaLibraryAlbum *> *)listAlbumsOfParentType:(const enum vlc_ml_parent_type)parentType forID:(int64_t)ID;
- (NSArray<id<VLCMediaLibraryItemProtocol>> *)listOfLibraryItemsOfParentType:(const VLCMediaLibraryParentGroupType)parentType;
- (NSArray<VLCMediaLibraryMediaItem *> *)listOfMediaItemsForParentType:(const VLCMediaLibraryParentGroupType)parentType;

- (void)sortByCriteria:(enum vlc_ml_sorting_criteria_t)sortCriteria
         andDescending:(bool)descending;

@end

NS_ASSUME_NONNULL_END
