/*****************************************************************************
 * VLCLibraryModel.m: MacOS X interface module
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

#import "VLCLibraryModel.h"
#import "extensions/NSString+Helpers.h"

NSString *VLCLibraryModelVideoMediaListUpdated = @"VLCLibraryModelVideoMediaListUpdated";

@interface VLCLibraryModel ()
{
    vlc_medialibrary_t *_p_mediaLibrary;
    vlc_ml_event_callback_t *_p_eventCallback;

    NSArray *_cachedVideoMedia;
    NSNotificationCenter *_defaultNotificationCenter;
}

- (void)updateCachedListOfVideoMedia;

@end

static void libraryCallback(void *p_data, const vlc_ml_event_t *p_event)
{
    switch(p_event->i_type)
    {
        case VLC_ML_EVENT_MEDIA_ADDED:
        case VLC_ML_EVENT_MEDIA_UPDATED:
        case VLC_ML_EVENT_MEDIA_DELETED:
            dispatch_async(dispatch_get_main_queue(), ^{
                VLCLibraryModel *libraryModel = (__bridge VLCLibraryModel *)p_data;
                [libraryModel updateCachedListOfVideoMedia];
            });
            break;
        default:
            break;
    }
}

@implementation VLCLibraryModel

- (instancetype)initWithLibrary:(vlc_medialibrary_t *)library
{
    self = [super init];
    if (self) {
        _p_mediaLibrary = library;
        _p_eventCallback = vlc_ml_event_register_callback(_p_mediaLibrary, libraryCallback, (__bridge void *)self);
        _defaultNotificationCenter = [[NSNotificationCenter alloc] init];
    }
    return self;
}

- (void)dealloc
{
    if (_p_eventCallback) {
        vlc_ml_event_unregister_callback(_p_mediaLibrary, _p_eventCallback);
    }
}

- (size_t)numberOfVideoMedia
{
    if (!_cachedVideoMedia) {
        [self updateCachedListOfVideoMedia];
    }

    return _cachedVideoMedia.count;
}

- (void)updateCachedListOfVideoMedia
{
    vlc_ml_media_list_t *p_media_list = vlc_ml_list_video_media(_p_mediaLibrary, NULL);
    NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_media_list->i_nb_items];
    for (size_t x = 0; x < p_media_list->i_nb_items; x++) {
        VLCMediaLibraryMediaItem *mediaItem = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:&p_media_list->p_items[x]];
        [mutableArray addObject:mediaItem];
    }
    _cachedVideoMedia = [mutableArray copy];
    vlc_ml_media_list_release(p_media_list);
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelVideoMediaListUpdated object:self];
}

- (nullable VLCMediaLibraryMediaItem *)mediaItemAtIndexPath:(NSIndexPath *)indexPath
{
    // FIXME: the scope needs be larger than just the video list
    if (!_cachedVideoMedia) {
        return nil;
    }
    return _cachedVideoMedia[indexPath.item];
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfVideoMedia
{
    if (!_cachedVideoMedia) {
        [self updateCachedListOfVideoMedia];
    }

    return _cachedVideoMedia;
}

@end

@implementation VLCMediaLibraryFile

- (instancetype)initWithFile:(struct vlc_ml_file_t *)file
{
    self = [super init];
    if (self && file != NULL) {
        _MRL = toNSStr(file->psz_mrl);
        _fileType = file->i_type;
        _external = file->b_external;
        _removable = file->b_removable;
        _present = file->b_present;
    }
    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ — type: %i, MRL: %@", NSStringFromClass([self class]), _fileType, _MRL];
}

@end

@implementation VLCMediaLibraryTrack

- (instancetype)initWithTrack:(struct vlc_ml_media_track_t *)track
{
    self = [super init];
    if (self && track != NULL) {
        _codec = toNSStr(track->psz_codec);
        _language = toNSStr(track->psz_language);
        _trackDescription = toNSStr(track->psz_description);
        _trackType = track->i_type;
        _bitrate = track->i_bitrate;

        _numberOfAudioChannels = track->a.i_nbChannels;
        _audioSampleRate = track->a.i_sampleRate;

        _videoHeight = track->v.i_height;
        _videoWidth = track->v.i_width;
        _sourceAspectRatio = track->v.i_sarNum;
        _sourceAspectRatioDenominator = track->v.i_sarDen;
        _frameRate = track->v.i_fpsNum;
        _frameRateDenominator = track->v.i_fpsDen;
    }
    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ — type: %i, codec %@", NSStringFromClass([self class]), _trackType, _codec];
}

@end

@implementation VLCMediaLibraryMediaItem

- (instancetype)initWithMediaItem:(struct vlc_ml_media_t *)p_mediaItem
{
    self = [super init];
    if (self) {
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
            }
        }
        _tracks = [mutArray copy];
        _year = p_mediaItem->i_year;
        _duration = p_mediaItem->i_duration;
        _playCount = p_mediaItem->i_playcount;
        _lastPlayedDate = p_mediaItem->i_last_played_date;
        _title = toNSStr(p_mediaItem->psz_title);
        _artworkMRL = toNSStr(p_mediaItem->psz_artwork_mrl);
        _artworkGenerated = p_mediaItem->b_artwork_generated;
        _favorited = p_mediaItem->b_is_favorite;
        _showEpisode = p_mediaItem->show_episode;
        _movie = p_mediaItem->movie;
        _albumTrack = p_mediaItem->album_track;
    }
    return self;
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ — title: %@, ID: %lli, type: %i, artwork: %@",
            NSStringFromClass([self class]), _title, _libraryID, _mediaType, _artworkMRL];
}

@end
