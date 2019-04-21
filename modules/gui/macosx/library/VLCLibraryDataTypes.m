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

#import "extensions/NSString+Helpers.h"

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
