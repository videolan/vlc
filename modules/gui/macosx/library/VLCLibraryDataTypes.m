/*****************************************************************************
 * VLCLibraryDataTypes.m: MacOS X interface module
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

#import "VLCLibraryDataTypes.h"

#import "main/VLCMain.h"
#import "extensions/NSString+Helpers.h"
#import "library/VLCInputItem.h"
#import "library/VLCLibraryImageCache.h"

#import <vlc_url.h>

NSString *VLCMediaLibraryMediaItemPasteboardType = @"VLCMediaLibraryMediaItemPasteboardType";

const CGFloat VLCMediaLibrary4KWidth = 3840.;
const CGFloat VLCMediaLibrary4KHeight = 2160.;
const CGFloat VLCMediaLibrary720pWidth = 1280.;
const CGFloat VLCMediaLibrary720pHeight = 720.;
const long long int VLCMediaLibraryMediaItemDurationDenominator = 1000;

NSString *VLCMediaLibraryMediaItemLibraryID = @"VLCMediaLibraryMediaItemLibraryID";

@implementation VLCMediaLibraryFile

- (instancetype)initWithFile:(struct vlc_ml_file_t *)p_file
{
    self = [super init];
    if (self && p_file != NULL) {
        _MRL = toNSStr(p_file->psz_mrl);
        _fileType = p_file->i_type;
        _external = p_file->b_external;
        _removable = p_file->b_removable;
        _present = p_file->b_present;
    }
    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ — type: %i, MRL: %@", NSStringFromClass([self class]), _fileType, _MRL];
}

- (NSURL *)fileURL
{
    return [NSURL URLWithString:_MRL];
}

- (NSString *)readableFileType
{
    switch (_fileType) {
        case VLC_ML_FILE_TYPE_MAIN:
            return _NS("Main");
            break;

        case VLC_ML_FILE_TYPE_PART:
            return _NS("Part");
            break;

        case VLC_ML_FILE_TYPE_PLAYLIST:
            return _NS("Playlist");
            break;

        case VLC_ML_FILE_TYPE_SUBTITLE:
            return _NS("Subtitle");
            break;

        case VLC_ML_FILE_TYPE_SOUNDTRACK:
            return _NS("Soundtrack");

        default:
            return _NS("Unknown");
            break;
    }
}

@end

@implementation VLCMediaLibraryTrack

- (instancetype)initWithTrack:(struct vlc_ml_media_track_t *)p_track
{
    self = [super init];
    if (self && p_track != NULL) {
        _codec = toNSStr(p_track->psz_codec);
        _language = toNSStr(p_track->psz_language);
        _trackDescription = toNSStr(p_track->psz_description);
        _trackType = p_track->i_type;
        _bitrate = p_track->i_bitrate;

        _numberOfAudioChannels = p_track->a.i_nbChannels;
        _audioSampleRate = p_track->a.i_sampleRate;

        _videoHeight = p_track->v.i_height;
        _videoWidth = p_track->v.i_width;
        _sourceAspectRatio = p_track->v.i_sarNum;
        _sourceAspectRatioDenominator = p_track->v.i_sarDen;
        _frameRate = p_track->v.i_fpsNum;
        _frameRateDenominator = p_track->v.i_fpsDen;
    }
    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ — type: %i, codec %@", NSStringFromClass([self class]), _trackType, _codec];
}

- (NSString *)readableCodecName
{
    vlc_fourcc_t fourcc = vlc_fourcc_GetCodecFromString(UNKNOWN_ES, _codec.UTF8String);
    return toNSStr(vlc_fourcc_GetDescription(UNKNOWN_ES, fourcc));
}

- (NSString *)readableTrackType
{
    switch (_trackType) {
        case VLC_ML_TRACK_TYPE_AUDIO:
            return _NS("Audio");
            break;

        case VLC_ML_TRACK_TYPE_VIDEO:
            return _NS("Video");

        default:
            return _NS("Unknown");
            break;
    }
}

@end

@implementation VLCMediaLibraryMovie

- (instancetype)initWithMovie:(struct vlc_ml_movie_t *)p_movie
{
    self = [super init];
    if (self && p_movie != NULL) {
        _summary = toNSStr(p_movie->psz_summary);
        _imdbID = toNSStr(p_movie->psz_imdb_id);
    }
    return self;
}

@end

@implementation VLCMediaLibraryShowEpisode

- (instancetype)initWithShowEpisode:(struct vlc_ml_show_episode_t *)p_showEpisode
{
    self = [super init];
    if (self && p_showEpisode != NULL) {
        _summary = toNSStr(p_showEpisode->psz_summary);
        _tvdbID = toNSStr(p_showEpisode->psz_tvdb_id);
        _episodeNumber = p_showEpisode->i_episode_nb;
        _seasonNumber = p_showEpisode->i_season_number;
    }
    return self;
}

@end

@implementation VLCMediaLibraryArtist

+ (nullable instancetype)artistWithID:(int64_t)artistID
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf) {
        return nil;
    }
    vlc_medialibrary_t *p_mediaLibrary = vlc_ml_instance_get(p_intf);
    if (!p_mediaLibrary) {
        return nil;
    }
    vlc_ml_artist_t *p_artist = vlc_ml_get_artist(p_mediaLibrary, artistID);
    VLCMediaLibraryArtist *artist = nil;
    if (p_artist) {
        artist = [[VLCMediaLibraryArtist alloc] initWithArtist:p_artist];
    }
    return artist;
}

- (instancetype)initWithArtist:(struct vlc_ml_artist_t *)p_artist;
{
    self = [super init];
    if (self && p_artist != NULL) {
        _artistID = p_artist->i_id;
        _name = toNSStr(p_artist->psz_name);
        _shortBiography = toNSStr(p_artist->psz_shortbio);
        _artworkMRL = toNSStr(p_artist->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl);
        _musicBrainzID = toNSStr(p_artist->psz_mb_id);
        _numberOfAlbums = p_artist->i_nb_album;
        _numberOfTracks = p_artist->i_nb_tracks;
    }
    return self;
}

@end

@implementation VLCMediaLibraryAlbum

- (instancetype)initWithAlbum:(struct vlc_ml_album_t *)p_album
{
    self = [super init];
    if (self && p_album != NULL) {
        _albumID = p_album->i_id;
        _title = toNSStr(p_album->psz_title);
        _summary = toNSStr(p_album->psz_summary);
        _artworkMRL = toNSStr(p_album->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl);
        _artistName = toNSStr(p_album->psz_artist);
        _artistID = p_album->i_artist_id;
        _numberOfTracks = p_album->i_nb_tracks;
        _duration = p_album->i_duration;
        _year = p_album->i_year;
    }
    return self;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)tracksAsMediaItems
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf) {
        return @[];
    }
    vlc_medialibrary_t *p_mediaLibrary = vlc_ml_instance_get(p_intf);
    if (!p_mediaLibrary) {
        return @[];
    }
    vlc_ml_media_list_t *p_mediaList = vlc_ml_list_album_tracks(p_mediaLibrary, NULL, _albumID);
    NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_mediaList->i_nb_items];
    for (size_t x = 0; x < p_mediaList->i_nb_items; x++) {
        VLCMediaLibraryMediaItem *mediaItem = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:&p_mediaList->p_items[x]];
        [mutableArray addObject:mediaItem];
    }
    vlc_ml_media_list_release(p_mediaList);
    return [mutableArray copy];
}

@end

@implementation VLCMediaLibraryAlbumTrack

- (instancetype)initWithAlbumTrack:(struct vlc_ml_album_track_t *)p_albumTrack
{
    self = [super init];
    if (self && p_albumTrack != NULL) {
        _artistID = p_albumTrack->i_artist_id;
        _albumID = p_albumTrack->i_album_id;
        _genreID = p_albumTrack->i_genre_id;

        _trackNumber = p_albumTrack->i_track_nb;
        _discNumber = p_albumTrack->i_disc_nb;
    }
    return self;
}

@end

@implementation VLCMediaLibraryGenre

- (instancetype)initWithGenre:(struct vlc_ml_genre_t *)p_genre
{
    self = [super init];
    if (self && p_genre != NULL) {
        _genreID = p_genre->i_id;
        _name = toNSStr(p_genre->psz_name);
        _numberOfTracks = p_genre->i_nb_tracks;
    }
    return self;
}

@end

@interface VLCMediaLibraryMediaItem ()

@property (readwrite, assign) vlc_medialibrary_t *p_mediaLibrary;

@end

@implementation VLCMediaLibraryMediaItem

#pragma mark - initialization

+ (nullable instancetype)mediaItemForLibraryID:(int64_t)libraryID
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf) {
        return nil;
    }
    vlc_medialibrary_t *p_mediaLibrary = vlc_ml_instance_get(p_intf);
    if (!p_mediaLibrary) {
        return nil;
    }
    vlc_ml_media_t *p_mediaItem = vlc_ml_get_media(p_mediaLibrary, libraryID);
    VLCMediaLibraryMediaItem *returnValue = nil;
    if (p_mediaItem) {
        returnValue = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:p_mediaItem library:p_mediaLibrary];
    }
    return returnValue;
}

+ (nullable instancetype)mediaItemForURL:(NSURL *)url
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf) {
        return nil;
    }
    vlc_medialibrary_t *p_mediaLibrary = vlc_ml_instance_get(p_intf);
    if (!p_mediaLibrary) {
        return nil;
    }
    vlc_ml_media_t *p_mediaItem = vlc_ml_get_media_by_mrl(p_mediaLibrary,
                                                          [[url absoluteString] UTF8String]);
    VLCMediaLibraryMediaItem *returnValue = nil;
    if (p_mediaItem) {
        returnValue = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:p_mediaItem library:p_mediaLibrary];
    }
    return returnValue;
}

- (nullable instancetype)initWithMediaItem:(struct vlc_ml_media_t *)p_mediaItem
{
    intf_thread_t *p_intf = getIntf();
    if (!p_intf) {
        return nil;
    }
    vlc_medialibrary_t *p_mediaLibrary = vlc_ml_instance_get(p_intf);
    if (!p_mediaLibrary) {
        return nil;
    }
    if (p_mediaItem != NULL && p_mediaLibrary != NULL) {
        return [self initWithMediaItem:p_mediaItem library:p_mediaLibrary];
    }
    return nil;
}

- (instancetype)initWithMediaItem:(struct vlc_ml_media_t *)p_mediaItem library:(vlc_medialibrary_t *)p_mediaLibrary
{
    self = [super init];
    if (self) {
        _p_mediaLibrary = p_mediaLibrary;
        _libraryID = p_mediaItem->i_id;
        _mediaType = p_mediaItem->i_type;
        _mediaSubType = p_mediaItem->i_subtype;
        NSMutableArray *mutArray = [[NSMutableArray alloc] initWithCapacity:p_mediaItem->p_files->i_nb_items];
        for (size_t x = 0; x < p_mediaItem->p_files->i_nb_items; x++) {
            VLCMediaLibraryFile *file = [[VLCMediaLibraryFile alloc] initWithFile:&p_mediaItem->p_files->p_items[x]];
            if (file) {
                [mutArray addObject:file];
            }
        }
        _files = [mutArray copy];
        mutArray = [[NSMutableArray alloc] initWithCapacity:p_mediaItem->p_tracks->i_nb_items];
        for (size_t x = 0; x < p_mediaItem->p_tracks->i_nb_items; x++) {
            VLCMediaLibraryTrack *track = [[VLCMediaLibraryTrack alloc] initWithTrack:&p_mediaItem->p_tracks->p_items[x]];
            if (track) {
                [mutArray addObject:track];
                if (track.trackType == VLC_ML_TRACK_TYPE_VIDEO && _firstVideoTrack == nil) {
                    _firstVideoTrack = track;
                }
            }
        }
        _tracks = [mutArray copy];
        _year = p_mediaItem->i_year;
        _duration = p_mediaItem->i_duration;
        _playCount = p_mediaItem->i_playcount;
        _lastPlayedDate = p_mediaItem->i_last_played_date;
        _title = toNSStr(p_mediaItem->psz_title);
        _smallArtworkGenerated = p_mediaItem->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl != NULL;
        if (_smallArtworkGenerated) {
            _smallArtworkMRL = toNSStr(p_mediaItem->thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl);
        } else {
            _smallArtworkMRL = nil;
        }
        _favorited = p_mediaItem->b_is_favorite;

        switch (p_mediaItem->i_subtype) {
            case VLC_ML_MEDIA_SUBTYPE_MOVIE:
                _movie = [[VLCMediaLibraryMovie alloc] initWithMovie:&p_mediaItem->movie];
                break;

            case VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE:
                _showEpisode = [[VLCMediaLibraryShowEpisode alloc] initWithShowEpisode:&p_mediaItem->show_episode];
                break;

            case VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK:
                _albumTrack = [[VLCMediaLibraryAlbumTrack alloc] initWithAlbumTrack:&p_mediaItem->album_track];
                break;

            default:
                break;
        }
    }
    return self;
}

- (nullable instancetype)initWithExternalURL:(NSURL *)url
{
    NSString *urlString = url.absoluteString;
    if (!urlString) {
        return self;
    }

    intf_thread_t *p_intf = getIntf();
    if (!p_intf) {
        return nil;
    }
    vlc_medialibrary_t *p_mediaLibrary = vlc_ml_instance_get(p_intf);
    if (!p_mediaLibrary) {
        return nil;
    }
    vlc_ml_media_t *p_media = vlc_ml_new_external_media(p_mediaLibrary, urlString.UTF8String);
    if (p_media) {
        self = [self initWithMediaItem:p_media library:p_mediaLibrary];
        vlc_ml_media_release(p_media);
    }
    return self;
}

- (nullable instancetype)initWithStreamURL:(NSURL *)url
{
    NSString *urlString = url.absoluteString;
    if (!urlString) {
        return self;
    }

    intf_thread_t *p_intf = getIntf();
    if (!p_intf) {
        return nil;
    }
    vlc_medialibrary_t *p_mediaLibrary = vlc_ml_instance_get(p_intf);
    if (!p_mediaLibrary) {
        return nil;
    }
    vlc_ml_media_t *p_media = vlc_ml_new_stream(p_mediaLibrary, urlString.UTF8String);
    if (p_media) {
        self = [self initWithMediaItem:p_media library:p_mediaLibrary];
        vlc_ml_media_release(p_media);
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)aDecoder
{
    int64_t libraryID = [aDecoder decodeInt64ForKey:VLCMediaLibraryMediaItemLibraryID];
    self = [VLCMediaLibraryMediaItem mediaItemForLibraryID:libraryID];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)aCoder
{
    [aCoder encodeInt64:_libraryID forKey:VLCMediaLibraryMediaItemLibraryID];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ — title: %@, ID: %lli, type: %i, artwork: %@",
            NSStringFromClass([self class]), _title, _libraryID, _mediaType, _smallArtworkMRL];
}

- (NSString *)readableMediaType
{
    switch (_mediaType) {
        case VLC_ML_MEDIA_TYPE_AUDIO:
            return _NS("Audio");
            break;

        case VLC_ML_MEDIA_TYPE_VIDEO:
            return _NS("Video");
            break;

        default:
            return _NS("Unknown");
            break;
    }
}

- (NSString *)readableMediaSubType
{
    switch (_mediaSubType) {
        case VLC_ML_MEDIA_SUBTYPE_MOVIE:
            return _NS("Movie");
            break;

        case VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE:
            return _NS("Show Episode");
            break;

        case VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK:
            return _NS("Album Track");
            break;

        default:
            return _NS("Unknown");
            break;
    }
}

- (NSImage *)smallArtworkImage
{
    return [VLCLibraryImageCache thumbnailForMediaItem:self];
}

- (VLCInputItem *)inputItem
{
    input_item_t *p_inputItem = vlc_ml_get_input_item(_p_mediaLibrary, _libraryID);
    VLCInputItem *inputItem = nil;
    if (p_inputItem) {
        inputItem = [[VLCInputItem alloc] initWithInputItem:p_inputItem];
    }
    input_item_Release(p_inputItem);
    return inputItem;
}

#pragma mark - preference setters / getters

- (int)setIntegerPreference:(int)value forKey:(enum vlc_ml_playback_state)key
{
    return vlc_ml_media_set_playback_state(_p_mediaLibrary, _libraryID, key, [[[NSNumber numberWithInt:value] stringValue] UTF8String]);
}

- (int)integerPreferenceForKey:(enum vlc_ml_playback_state)key
{
    int ret = 0;
    char *psz_value;

    if (vlc_ml_media_get_playback_state(_p_mediaLibrary, _libraryID, key, &psz_value) == VLC_SUCCESS && psz_value != NULL) {
        ret = atoi(psz_value);
        free(psz_value);
    }

    return ret;
}

- (int)setFloatPreference:(float)value forKey:(enum vlc_ml_playback_state)key
{
    return vlc_ml_media_set_playback_state(_p_mediaLibrary, _libraryID, key, [[[NSNumber numberWithFloat:value] stringValue] UTF8String]);
}

- (float)floatPreferenceForKey:(enum vlc_ml_playback_state)key
{
    float ret = .0;
    char *psz_value;

    if (vlc_ml_media_get_playback_state(_p_mediaLibrary, _libraryID, key, &psz_value) == VLC_SUCCESS && psz_value != NULL) {
        ret = atof(psz_value);
        free(psz_value);
    }

    return ret;
}

- (int)setStringPreference:(NSString *)value forKey:(enum vlc_ml_playback_state)key
{
    return vlc_ml_media_set_playback_state(_p_mediaLibrary, _libraryID, key, [value UTF8String]);
}

- (NSString *)stringPreferenceForKey:(enum vlc_ml_playback_state)key
{
    NSString *ret = @"";
    char *psz_value;

    if (vlc_ml_media_get_playback_state(_p_mediaLibrary, _libraryID, key, &psz_value) == VLC_SUCCESS && psz_value != NULL) {
        ret = toNSStr(psz_value);
        free(psz_value);
    }

    return ret;
}

#pragma mark - preference properties

- (int)rating
{
    return [self integerPreferenceForKey:VLC_ML_PLAYBACK_STATE_RATING];
}

- (void)setRating:(int)rating
{
    [self setIntegerPreference:rating forKey:VLC_ML_PLAYBACK_STATE_RATING];
}

- (float)lastPlaybackPosition
{
    return [self floatPreferenceForKey:VLC_ML_PLAYBACK_STATE_PROGRESS];
}

- (void)setLastPlaybackPosition:(float)lastPlaybackPosition
{
    [self setFloatPreference:lastPlaybackPosition forKey:VLC_ML_PLAYBACK_STATE_PROGRESS];
}

- (float)lastPlaybackRate
{
    return [self floatPreferenceForKey:VLC_ML_PLAYBACK_STATE_SPEED];
}

- (void)setLastPlaybackRate:(float)lastPlaybackRate
{
    [self setFloatPreference:lastPlaybackRate forKey:VLC_ML_PLAYBACK_STATE_SPEED];
}

- (int)lastTitle
{
    return [self integerPreferenceForKey:VLC_ML_PLAYBACK_STATE_TITLE];
}

- (void)setLastTitle:(int)lastTitle
{
    [self setIntegerPreference:lastTitle forKey:VLC_ML_PLAYBACK_STATE_TITLE];
}

- (int)lastChapter
{
    return [self integerPreferenceForKey:VLC_ML_PLAYBACK_STATE_CHAPTER];
}

- (void)setLastChapter:(int)lastChapter
{
    [self setIntegerPreference:lastChapter forKey:VLC_ML_PLAYBACK_STATE_CHAPTER];
}

- (int)lastProgram
{
    return [self integerPreferenceForKey:VLC_ML_PLAYBACK_STATE_PROGRAM];
}

- (void)setLastProgram:(int)lastProgram
{
    [self setIntegerPreference:lastProgram forKey:VLC_ML_PLAYBACK_STATE_PROGRAM];
}

- (BOOL)seen
{
    return [self integerPreferenceForKey:VLC_ML_PLAYBACK_STATE_SEEN] > 0 ? YES : NO;
}

- (void)setSeen:(BOOL)seen
{
    [self setIntegerPreference:seen forKey:VLC_ML_PLAYBACK_STATE_SEEN];
}

- (int)lastVideoTrack
{
    return [self integerPreferenceForKey:VLC_ML_PLAYBACK_STATE_VIDEO_TRACK];
}

- (void)setLastVideoTrack:(int)lastVideoTrack
{
    [self setIntegerPreference:lastVideoTrack forKey:VLC_ML_PLAYBACK_STATE_VIDEO_TRACK];
}

- (NSString *)lastAspectRatio
{
    return [self stringPreferenceForKey:VLC_ML_PLAYBACK_STATE_ASPECT_RATIO];
}

- (void)setLastAspectRatio:(NSString *)lastAspectRatio
{
    [self setStringPreference:lastAspectRatio forKey:VLC_ML_PLAYBACK_STATE_ASPECT_RATIO];
}

- (NSString *)lastZoom
{
    return [self stringPreferenceForKey:VLC_ML_PLAYBACK_STATE_ZOOM];
}

- (void)setLastZoom:(NSString *)lastZoom
{
    [self setStringPreference:lastZoom forKey:VLC_ML_PLAYBACK_STATE_ZOOM];
}

- (NSString *)lastCrop
{
    return [self stringPreferenceForKey:VLC_ML_PLAYBACK_STATE_CROP];
}

- (void)setLastCrop:(NSString *)lastCrop
{
    [self setStringPreference:lastCrop forKey:VLC_ML_PLAYBACK_STATE_CROP];
}

- (NSString *)lastDeinterlaceFilter
{
    return [self stringPreferenceForKey:VLC_ML_PLAYBACK_STATE_DEINTERLACE];
}

- (void)setLastDeinterlaceFilter:(NSString *)lastDeinterlaceFilter
{
    [self setStringPreference:lastDeinterlaceFilter forKey:VLC_ML_PLAYBACK_STATE_DEINTERLACE];
}

- (NSString *)lastVideoFilters
{
    return [self stringPreferenceForKey:VLC_ML_PLAYBACK_STATE_VIDEO_FILTER];
}

- (void)setLastVideoFilters:(NSString *)lastVideoFilters
{
    [self setStringPreference:lastVideoFilters forKey:VLC_ML_PLAYBACK_STATE_VIDEO_FILTER];
}

- (int)lastAudioTrack
{
    return [self integerPreferenceForKey:VLC_ML_PLAYBACK_STATE_AUDIO_TRACK];
}

- (void)setLastAudioTrack:(int)lastAudioTrack
{
    [self setIntegerPreference:lastAudioTrack forKey:VLC_ML_PLAYBACK_STATE_AUDIO_TRACK];
}

- (float)lastGain
{
    return [self floatPreferenceForKey:VLC_ML_PLAYBACK_STATE_GAIN];
}

- (void)setLastGain:(float)lastGain
{
    [self setFloatPreference:lastGain forKey:VLC_ML_PLAYBACK_STATE_GAIN];
}

- (int)lastAudioDelay
{
    return [self integerPreferenceForKey:VLC_ML_PLAYBACK_STATE_AUDIO_DELAY];
}

- (void)setLastAudioDelay:(int)lastAudioDelay
{
    [self setIntegerPreference:lastAudioDelay forKey:VLC_ML_PLAYBACK_STATE_AUDIO_DELAY];
}

- (int)lastSubtitleTrack
{
    return [self integerPreferenceForKey:VLC_ML_PLAYBACK_STATE_SUBTITLE_TRACK];
}

- (void)setLastSubtitleTrack:(int)lastSubtitleTrack
{
    [self setIntegerPreference:lastSubtitleTrack forKey:VLC_ML_PLAYBACK_STATE_SUBTITLE_TRACK];
}

- (int)lastSubtitleDelay
{
    return [self integerPreferenceForKey:VLC_ML_PLAYBACK_STATE_SUBTITLE_DELAY];
}

- (void)setLastSubtitleDelay:(int)lastSubtitleDelay
{
    [self setIntegerPreference:lastSubtitleDelay forKey:VLC_ML_PLAYBACK_STATE_SUBTITLE_DELAY];
}

- (int)increasePlayCount
{
    return vlc_ml_media_increase_playcount(_p_mediaLibrary, _libraryID);
}

@end

@implementation VLCMediaLibraryEntryPoint

- (instancetype)initWithEntryPoint:(struct vlc_ml_entry_point_t *)p_entryPoint
{
    self = [super init];
    if (self && p_entryPoint != NULL) {

        _MRL = toNSStr(p_entryPoint->psz_mrl);
        _decodedMRL = toNSStr(vlc_uri_decode(p_entryPoint->psz_mrl));
        _isPresent = p_entryPoint->b_present;
        _isBanned = p_entryPoint->b_banned;
    }
    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ — MRL: %@, present: %i, banned: %i",
            NSStringFromClass([self class]), _MRL, _isPresent, _isBanned];
}

@end
