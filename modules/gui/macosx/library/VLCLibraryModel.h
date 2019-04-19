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

#import <Foundation/Foundation.h>

#import <vlc_media_library.h>

NS_ASSUME_NONNULL_BEGIN

@class VLCMediaLibraryMediaItem;

extern NSString *VLCLibraryModelVideoMediaListUpdated;

@interface VLCLibraryModel : NSObject

- (instancetype)initWithLibrary:(vlc_medialibrary_t *)library;

@property (readonly) size_t numberOfVideoMedia;
@property (readonly) NSArray <VLCMediaLibraryMediaItem *> *listOfVideoMedia;

- (nullable VLCMediaLibraryMediaItem *)mediaItemAtIndexPath:(NSIndexPath *)index;

@end

@interface VLCMediaLibraryFile : NSObject

- (instancetype)initWithFile:(struct vlc_ml_file_t *)file;

@property (readonly) NSString *MRL;
@property (readonly) vlc_ml_file_type_t fileType;
@property (readonly) BOOL external;
@property (readonly) BOOL removable;
@property (readonly) BOOL present;

@end

@interface VLCMediaLibraryTrack : NSObject

- (instancetype)initWithTrack:(struct vlc_ml_media_track_t *)track;

@property (readonly) NSString *codec;
@property (readonly) NSString *language;
@property (readonly) NSString *trackDescription;
@property (readonly) vlc_ml_track_type_t trackType;
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

@interface VLCMediaLibraryMediaItem : NSObject

- (instancetype)initWithMediaItem:(struct vlc_ml_media_t *)mediaItem;

@property (readonly) int64_t libraryID;
@property (readonly) vlc_ml_media_type_t mediaType;
@property (readonly) vlc_ml_media_subtype_t mediaSubType;

@property (readonly) NSArray <VLCMediaLibraryFile *> *files;
@property (readonly) NSArray <VLCMediaLibraryTrack *> *tracks;

@property (readonly) int32_t year;
@property (readonly) int64_t duration; /* Duration in milliseconds */
@property (readonly) uint32_t playCount;
@property (readonly) time_t lastPlayedDate;
@property (readonly) NSString *title;

@property (readonly) NSString *artworkMRL;

@property (readonly) BOOL artworkGenerated;
@property (readonly) BOOL favorited;

@property (readonly) vlc_ml_show_episode_t showEpisode;
@property (readonly) vlc_ml_movie_t movie;
@property (readonly) vlc_ml_album_track_t albumTrack;

@end

NS_ASSUME_NONNULL_END
