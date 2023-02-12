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

extern NSString *VLCMediaLibraryMediaItemPasteboardType;

@class VLCMediaLibraryMediaItem;
@class VLCMediaLibraryAlbum;
@class VLCMediaLibraryArtist;
@class VLCInputItem;

extern const CGFloat VLCMediaLibrary4KWidth;
extern const CGFloat VLCMediaLibrary4KHeight;
extern const CGFloat VLCMediaLibrary720pWidth;
extern const CGFloat VLCMediaLibrary720pHeight;
extern const long long int VLCMediaLibraryMediaItemDurationDenominator;

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

@interface VLCMediaLibraryMovie : NSObject

- (instancetype)initWithMovie:(struct vlc_ml_movie_t *)p_movie;

@property (readonly) NSString *summary;
@property (readonly) NSString *imdbID;

@end

@interface VLCMediaLibraryShowEpisode : NSObject

- (instancetype)initWithShowEpisode:(struct vlc_ml_show_episode_t *)p_showEpisode;

@property (readonly) NSString *summary;
@property (readonly) NSString *tvdbID;
@property (readonly) uint32_t episodeNumber;
@property (readonly) uint32_t seasonNumber;

@end

#pragma mark - Media library classes

// Protocol with common methods and properties expected for media library items
@protocol VLCMediaLibraryItemProtocol <NSObject>

@property (readonly) int64_t libraryID;
@property (readonly) BOOL smallArtworkGenerated;
@property (readonly) NSImage *smallArtworkImage;
@property (readonly) NSString *smallArtworkMRL;
@property (readonly) NSString *displayString;
@property (readonly) NSString *detailString;
@property (readonly) NSString *durationString;
@property (readonly) VLCMediaLibraryMediaItem *firstMediaItem;

- (void)iterateMediaItemsWithBlock:(void (^)(VLCMediaLibraryMediaItem*))mediaItemBlock;

@end

// Extended VLCMediaLibraryItemProtocol that includes additional properties for media library item
// audio groups (i.e. artists, genres, etc.)
@protocol VLCMediaLibraryAudioGroupProtocol <VLCMediaLibraryItemProtocol>

@property (readonly) unsigned int numberOfTracks;
@property (readonly) NSArray <VLCMediaLibraryArtist *> *artists;
@property (readonly) NSArray <VLCMediaLibraryAlbum *> *albums;
@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *tracksAsMediaItems;

@end

@protocol VLCLocallyManipulableItemProtocol <NSObject>

- (void)revealInFinder;
- (void)moveToTrash;

@end

// Base abstract class with common implementations of properties used by media library items.
// Do not use directly -- subclass to create new media library item types.
@interface VLCAbstractMediaLibraryItem : NSObject

@property (readonly) int64_t libraryID;
@property (readonly) BOOL smallArtworkGenerated;
@property (readonly) NSImage *smallArtworkImage;
@property (readonly, strong, atomic) NSString *smallArtworkMRL;

@end

// Like VLCAbstractMediaLibraryItem but with some additional functionality for audio groupings
// such as artists and genres. Do not use directly, subclass instead.
@interface VLCAbstractMediaLibraryAudioGroup : VLCAbstractMediaLibraryItem<VLCLocallyManipulableItemProtocol>

@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *tracksAsMediaItems;
@property (readonly) VLCMediaLibraryMediaItem *firstMediaItem;

- (void)iterateMediaItemsWithBlock:(void (^)(VLCMediaLibraryMediaItem*))mediaItemBlock;

@end

@interface VLCMediaLibraryArtist : VLCAbstractMediaLibraryAudioGroup<VLCMediaLibraryAudioGroupProtocol>

+ (nullable instancetype)artistWithID:(int64_t)artistID;
- (instancetype)initWithArtist:(struct vlc_ml_artist_t *)p_artist;

@property (readonly) NSString *name;
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
@property (readonly) int64_t artistID;
@property (readonly) unsigned int duration;
@property (readonly) unsigned int year;

@end

@interface VLCMediaLibraryGenre : VLCAbstractMediaLibraryAudioGroup<VLCMediaLibraryAudioGroupProtocol>

+ (nullable instancetype)genreWithID:(int64_t)genreID;
- (instancetype)initWithGenre:(struct vlc_ml_genre_t *)p_genre;

@property (readonly) NSString *name;

@end

@interface VLCMediaLibraryMediaItem : VLCAbstractMediaLibraryItem<VLCMediaLibraryItemProtocol, VLCLocallyManipulableItemProtocol>

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

@property (readonly) BOOL favorited;

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

- (instancetype)initWithDisplayString:(NSString*)displayString
                     withDetailString:(NSString*)detailString;

@end

NS_ASSUME_NONNULL_END
