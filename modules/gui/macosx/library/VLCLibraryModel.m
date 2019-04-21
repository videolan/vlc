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

#import "library/VLCLibraryDataTypes.h"

NSString *VLCLibraryModelAudioMediaListUpdated = @"VLCLibraryModelAudioMediaListUpdated";
NSString *VLCLibraryModelVideoMediaListUpdated = @"VLCLibraryModelVideoMediaListUpdated";

@interface VLCLibraryModel ()
{
    vlc_medialibrary_t *_p_mediaLibrary;
    vlc_ml_event_callback_t *_p_eventCallback;

    NSArray *_cachedAudioMedia;
    NSArray *_cachedVideoMedia;
    NSNotificationCenter *_defaultNotificationCenter;
}

- (void)updateCachedListOfAudioMedia;
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
                switch (libraryModel.libraryMode) {
                    case VLCLibraryModeAudio:
                        [libraryModel updateCachedListOfAudioMedia];
                        break;

                    case VLCLibraryModeVideo:
                        [libraryModel updateCachedListOfVideoMedia];
                        break;

                    default:
                        break;
                }

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

- (size_t)numberOfAudioMedia
{
    if (_cachedAudioMedia) {
        [self updateCachedListOfAudioMedia];
    }

    return _cachedAudioMedia.count;
}

- (void)updateCachedListOfAudioMedia
{
    vlc_ml_media_list_t *p_media_list = vlc_ml_list_audio_media(_p_mediaLibrary, NULL);
    NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_media_list->i_nb_items];
    for (size_t x = 0; x < p_media_list->i_nb_items; x++) {
        VLCMediaLibraryMediaItem *mediaItem = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:&p_media_list->p_items[x]];
        [mutableArray addObject:mediaItem];
    }
    _cachedAudioMedia = [mutableArray copy];
    vlc_ml_media_list_release(p_media_list);
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelAudioMediaListUpdated object:self];
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfAudioMedia
{
    if (!_cachedAudioMedia) {
        [self updateCachedListOfAudioMedia];
    }

    return _cachedAudioMedia;
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
