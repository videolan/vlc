/*****************************************************************************
 * VLCLibraryDataTypes.h: MacOS X interface module
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
#import <vlc_media_library.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString * const VLCMediaLibraryMediaItemPasteboardType;
extern NSString * const VLCMediaLibraryMediaItemUTI;

@class VLCMediaLibraryMediaItem;
@class VLCMediaLibraryAlbum;
@class VLCMediaLibraryArtist;
@class VLCMediaLibraryGenre;
@class VLCInputItem;

extern const CGFloat VLCMediaLibrary4KWidth;
extern const CGFloat VLCMediaLibrary4KHeight;
extern const CGFloat VLCMediaLibrary720pWidth;
extern const CGFloat VLCMediaLibrary720pHeight;
extern const long long int VLCMediaLibraryMediaItemDurationDenominator;

vlc_medialibrary_t * _Nullable getMediaLibrary(void);

typedef NS_ENUM(NSUInteger, VLCMediaLibraryParentGroupType) {
    VLCMediaLibraryParentGroupTypeUnknown = VLC_ML_PARENT_UNKNOWN,
    VLCMediaLibraryParentGroupTypeAlbum = VLC_ML_PARENT_ALBUM,
    VLCMediaLibraryParentGroupTypeArtist = VLC_ML_PARENT_ARTIST,
    VLCMediaLibraryParentGroupTypeShow = VLC_ML_PARENT_SHOW,
    VLCMediaLibraryParentGroupTypeGenre = VLC_ML_PARENT_GENRE,
    VLCMediaLibraryParentGroupTypeGroup = VLC_ML_PARENT_GROUP,
    VLCMediaLibraryParentGroupTypeFolder = VLC_ML_PARENT_FOLDER,
    VLCMediaLibraryParentGroupTypePlaylist = VLC_ML_PARENT_PLAYLIST,
    // Additional types over vlc_ml_parent_type below
    VLCMediaLibraryParentGroupTypeAudioLibrary,
    VLCMediaLibraryParentGroupTypeRecentAudios,
    // Video library-specific entries.
    // Please define these in the order the are expected to be presented
    VLCMediaLibraryParentGroupTypeRecentVideos,
    VLCMediaLibraryParentGroupTypeVideoLibrary, // This should be last
};

@interface VLCMediaLibraryFile : NSObject

- (instancetype)initWithFile:(struct vlc_ml_file_t *)p_file;

@property (readonly) NSString *MRL;
@property (readonly) NSURL *fileURL;
@property (readonly) vlc_ml_file_type_t fileType;
@property (readonly) NSString *readableFileType;
@property (readonly) BOOL external;
@property (readonly) BOOL removable;
@property (readonly) BOOL present;
@property (readonly) time_t lastModificationDate;

@end

@interface VLCMediaLibraryTrack : NSObject

- (instancetype)initWithTrack:(struct vlc_ml_media_track_t *)p_track;

@property (readonly) NSString *codec;
@property (readonly) NSString *readableCodecName;
@property (readonly) NSString *language;
@property (readonly) NSString *trackDescription;
@property (readonly) vlc_ml_track_type_t trackType;
@property (readonly) NSString *readableTrackType;
@property (readonly) uint32_t bitrate;

@property (readonly) uint32_t numberOfAudioChannels;
@property (readonly) uint32_t audioSampleRate;

@property (readonly) uint32_t videoHeight;
@property (readonly) uint32_t videoWidth;
@property (readonly) uint32_t sourceAspectRatio;
@property (readonly) uint32_t sourceAspectRatioDenominator;
@property (readonly) uint32_t frameRate;
@property (readonly) uint32_t frameRateDenominator;

@end

@interface VLCMediaLibraryShowEpisode : NSObject

- (instancetype)initWithShowEpisode:(struct vlc_ml_show_episode_t *)p_showEpisode;

@property (readonly) NSString *summary;
@property (readonly) NSString *tvdbID;
@property (readonly) uint32_t episodeNumber;
@property (readonly) uint32_t seasonNumber;

@end

@protocol VLCLocallyManipulableItemProtocol <NSObject>

- (void)revealInFinder;
- (void)moveToTrash;

@end

// Protocol with common methods and properties expected for media library items
@protocol VLCMediaLibraryItemProtocol <VLCLocallyManipulableItemProtocol>

@property (readonly) int64_t libraryID;
@property (readonly) BOOL smallArtworkGenerated;
@property (readonly) NSString *smallArtworkMRL;
@property (readonly) NSString *displayString;
@property (readonly) BOOL isFileBacked;
@property (readonly) NSString *primaryDetailString;
@property (readonly) NSString *secondaryDetailString;
@property (readonly) NSString *durationString;
@property (readonly) VLCMediaLibraryMediaItem *firstMediaItem;
// Media items should be delivered album-wise. If it is required for derivative
// types to provide media items in a different grouping or order, use methods
// or properties specific to the type (e.g. like in VLCMediaLibraryGenre)
@property (readonly) NSArray<VLCMediaLibraryMediaItem *> *mediaItems;
// If the info in detailString contains a library object that can be used for nav,
// we lazy load the actionable library item to avoid using the property until we
// actually need to -- resort to `actionableDetail` to know if there is one instead
@property (readonly) BOOL primaryActionableDetail;
@property (readonly) id<VLCMediaLibraryItemProtocol> primaryActionableDetailLibraryItem;
@property (readonly) BOOL secondaryActionableDetail;
@property (readonly) id<VLCMediaLibraryItemProtocol> secondaryActionableDetailLibraryItem;
@property (readonly) NSArray<NSString *> *labels;
@property (readonly) BOOL favorited;

- (int)setFavorite:(BOOL)favorite;
- (int)toggleFavorite;

- (void)iterateMediaItemsWithBlock:(void (^)(VLCMediaLibraryMediaItem*))mediaItemBlock;

@end

// Base abstract class with common implementations of properties used by media library items.
// Do not use directly -- subclass to create new media library item types.
@interface VLCAbstractMediaLibraryItem : NSObject<VLCMediaLibraryItemProtocol>

@end

// Extended VLCMediaLibraryItemProtocol that includes additional properties for media library item
// audio groups (i.e. artists, genres, etc.)
@protocol VLCMediaLibraryAudioGroupProtocol <VLCMediaLibraryItemProtocol>

@property (readonly) unsigned int numberOfTracks;
@property (readonly) NSArray<VLCMediaLibraryArtist *> *artists;
@property (readonly) NSArray<VLCMediaLibraryAlbum *> *albums;
@property (readonly) NSArray<VLCMediaLibraryGenre *> *genres;
@property (readonly) VLCMediaLibraryParentGroupType matchingParentType;

- (void)iterateMediaItemsWithBlock:(void (^)(VLCMediaLibraryMediaItem*))mediaItemBlock;

@end

// Like VLCAbstractMediaLibraryItem but with some additional functionality for audio groupings
// such as artists and genres. Do not use directly, subclass instead.
@interface VLCAbstractMediaLibraryAudioGroup : VLCAbstractMediaLibraryItem<VLCMediaLibraryAudioGroupProtocol>

@end

#pragma mark - Media library classes

@interface VLCMediaLibraryArtist : VLCAbstractMediaLibraryAudioGroup<VLCMediaLibraryAudioGroupProtocol>

+ (nullable instancetype)artistWithID:(int64_t)artistID;
- (instancetype)initWithArtist:(struct vlc_ml_artist_t *)p_artist;

@property (readonly) NSString *name;
@property (readonly) NSString *genreString; // Lazy loaded for performance
@property (readonly) NSString *shortBiography;
@property (readonly) NSString *musicBrainzID;
@property (readonly) unsigned int numberOfAlbums;

@end

@interface VLCMediaLibraryAlbum : VLCAbstractMediaLibraryAudioGroup<VLCMediaLibraryAudioGroupProtocol>

+ (nullable instancetype)albumWithID:(int64_t)albumID;
- (instancetype)initWithAlbum:(struct vlc_ml_album_t *)p_album;

@property (readonly) NSString *title;
@property (readonly) NSString *summary;
@property (readonly) NSString *artistName;
@property (readonly) NSString *genreString; // Lazy loaded for performance
@property (readonly) int64_t artistID;
@property (readonly) unsigned int duration;
@property (readonly) unsigned int year;

@end

@interface VLCMediaLibraryGenre : VLCAbstractMediaLibraryAudioGroup<VLCMediaLibraryAudioGroupProtocol>

+ (nullable instancetype)genreWithID:(int64_t)genreID;
- (instancetype)initWithGenre:(struct vlc_ml_genre_t *)p_genre;

@property (readonly) NSString *name;

@end

@interface VLCMediaLibraryShow : VLCAbstractMediaLibraryItem<VLCMediaLibraryItemProtocol>

+ (nullable instancetype)showWithLibraryId:(int64_t)libraryId;
- (instancetype)initWithShow:(struct vlc_ml_show_t *)p_show;

@property (readonly) NSString *name;
@property (readonly) NSString *summary;
@property (readonly) NSString *tvdbId;
@property (readonly) unsigned int releaseYear;
@property (readonly) uint32_t episodeCount;
@property (readonly) uint32_t seasonCount;
@property (readonly) NSArray<VLCMediaLibraryMediaItem *> *episodes;

@end

@interface VLCMediaLibraryMovie : VLCAbstractMediaLibraryItem <VLCMediaLibraryItemProtocol>

- (instancetype)initWithMediaItem:(struct vlc_ml_media_t *)p_media;

@property (readonly) NSString *summary;
@property (readonly) NSString *imdbID;

@end

@interface VLCMediaLibraryGroup : VLCAbstractMediaLibraryItem

@property (readonly) NSString *name;
@property (readonly) NSUInteger numberOfTotalItems;
@property (readonly) NSUInteger numberOfVideoItems;
@property (readonly) NSUInteger numberOfAudioItems;
@property (readonly) NSUInteger numberOfUnknownItems;
@property (readonly) NSUInteger numberOfPresentTotalItems;
@property (readonly) NSUInteger numberOfPresentVideoItems;
@property (readonly) NSUInteger numberOfPresentAudioItems;
@property (readonly) NSUInteger numberOfPresentUnknownItems;
@property (readonly) NSUInteger numberOfSeenItems;
@property (readonly) NSUInteger numberOfPresentSeenItems;
@property (readonly) NSInteger duration; // milliseconds
@property (readonly) NSDate *creationDate;
@property (readonly) NSDate *lastModificationDate;

+ (nullable instancetype)groupWithID:(int64_t)libraryID;
- (instancetype)initWithGroup:(struct vlc_ml_group_t *)p_group;

@end

@interface VLCMediaLibraryPlaylist : VLCAbstractMediaLibraryItem<VLCMediaLibraryItemProtocol>

@property (readonly) NSString *MRL;

@property (readonly) NSArray<VLCMediaLibraryMediaItem *> *mediaItems;

@property (readonly) unsigned int numberOfMedia;
@property (readonly) uint32_t numberOfVideos;
@property (readonly) uint32_t numberOfAudios;
@property (readonly) uint32_t numberOfUnknowns;

@property (readonly) unsigned int numberOfPresentMedia;
@property (readonly) uint32_t numberOfPresentVideos;
@property (readonly) uint32_t numberOfPresentAudios;
@property (readonly) uint32_t numberOfPresentUnknowns;

@property (readonly) NSDate *creationDate;

@property (readonly) int64_t duration;
@property (readonly) uint32_t numberDurationUnknown;

@property (readonly) BOOL readOnly;

+ (instancetype)playlistForLibraryID:(int64_t)libraryID;
- (instancetype)initWithPlaylist:(const struct vlc_ml_playlist_t * const)p_playlist;

@end

@interface VLCMediaLibraryMediaItem : NSObject<VLCMediaLibraryItemProtocol>

+ (nullable instancetype)mediaItemForLibraryID:(int64_t)libraryID;
+ (nullable instancetype)mediaItemForURL:(NSURL *)url;
- (nullable instancetype)initWithMediaItem:(struct vlc_ml_media_t *)mediaItem;
- (nullable instancetype)initWithExternalURL:(NSURL *)url;
- (nullable instancetype)initWithStreamURL:(NSURL *)url;

@property (readonly) vlc_ml_media_type_t mediaType;
@property (readonly) NSString *readableMediaType;
@property (readonly) vlc_ml_media_subtype_t mediaSubType;
@property (readonly) NSString *readableMediaSubType;
@property (readonly) VLCInputItem *inputItem;

@property (readonly) NSArray <VLCMediaLibraryFile *> *files;
@property (readonly) NSArray <VLCMediaLibraryTrack *> *tracks;
@property (readonly, nullable) VLCMediaLibraryTrack *firstVideoTrack;

@property (readonly) int32_t year;
@property (readonly) int64_t duration; /* Duration in milliseconds */
@property (readonly) uint32_t playCount;
@property (readonly) time_t lastPlayedDate;
@property (readonly) float progress;
@property (readonly) NSString *title;

@property (readonly, nullable) VLCMediaLibraryShowEpisode *showEpisode;
@property (readonly, nullable) VLCMediaLibraryMovie *movie;

@property (readwrite) int rating;
@property (readwrite) float lastPlaybackRate;
@property (readwrite) int lastTitle;
@property (readwrite) int lastChapter;
@property (readwrite) int lastProgram;
@property (readwrite) int lastVideoTrack;
@property (readwrite) NSString *lastAspectRatio;
@property (readwrite) NSString *lastZoom;
@property (readwrite) NSString *lastCrop;
@property (readwrite) NSString *lastDeinterlaceFilter;
@property (readwrite) NSString *lastVideoFilters;
@property (readwrite) int lastAudioTrack;
@property (readwrite) float lastGain;
@property (readwrite) int lastAudioDelay;
@property (readwrite) int lastSubtitleTrack;
@property (readwrite) int lastSubtitleDelay;

@property (readonly) int64_t artistID;
@property (readonly) int64_t albumID;
@property (readonly) int64_t genreID;

@property (readonly) int trackNumber;
@property (readonly) int discNumber;

@end

@interface VLCMediaLibraryEntryPoint : NSObject

- (instancetype)initWithEntryPoint:(struct vlc_ml_folder_t *)p_entryPoint;

@property (readonly) NSString *MRL;
@property (readonly) NSString *decodedMRL;
@property (readonly) BOOL isPresent;
@property (readonly) BOOL isBanned;

@end

@interface VLCMediaLibraryDummyItem : NSObject<VLCMediaLibraryItemProtocol>

- (instancetype)initWithDisplayString:(NSString *)displayString
              withPrimaryDetailString:(nullable NSString *)primaryDetailString
            withSecondaryDetailString:(nullable NSString *)secondaryDetailString;

- (instancetype)initWithDisplayString:(NSString *)displayString
                       withMediaItems:(NSArray<VLCMediaLibraryMediaItem *> *)mediaItems;      

@end

NS_ASSUME_NONNULL_END
