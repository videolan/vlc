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

#import "main/VLCMain.h"
#import "library/VLCLibraryDataTypes.h"
#import "extensions/NSString+Helpers.h"

NSString *VLCLibraryModelAudioMediaListUpdated = @"VLCLibraryModelAudioMediaListUpdated";
NSString *VLCLibraryModelArtistListUpdated = @"VLCLibraryModelArtistListUpdated";
NSString *VLCLibraryModelAlbumListUpdated = @"VLCLibraryModelAlbumListUpdated";
NSString *VLCLibraryModelVideoMediaListUpdated = @"VLCLibraryModelVideoMediaListUpdated";
NSString *VLCLibraryModelRecentMediaListUpdated = @"VLCLibraryModelRecentMediaListUpdated";
NSString *VLCLibraryModelMediaItemUpdated = @"VLCLibraryModelMediaItemUpdated";

@interface VLCLibraryModel ()
{
    vlc_medialibrary_t *_p_mediaLibrary;
    vlc_ml_event_callback_t *_p_eventCallback;

    NSArray *_cachedAudioMedia;
    NSArray *_cachedArtists;
    NSArray *_cachedAlbums;
    NSArray *_cachedGenres;
    NSArray *_cachedVideoMedia;
    NSArray *_cachedRecentMedia;
    NSNotificationCenter *_defaultNotificationCenter;

    enum vlc_ml_sorting_criteria_t _sortCriteria;
    bool _sortDescending;
}

- (void)updateCachedListOfAudioMedia;
- (void)updateCachedListOfVideoMedia;
- (void)updateCachedListOfRecentMedia;
- (void)mediaItemWasUpdated:(VLCMediaLibraryMediaItem *)mediaItem;

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
                [libraryModel updateCachedListOfRecentMedia];
                [libraryModel updateCachedListOfAudioMedia];
                [libraryModel updateCachedListOfVideoMedia];
            });
            break;
        case VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED:
            if (p_event->media_thumbnail_generated.b_success) {
                VLCMediaLibraryMediaItem *mediaItem = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:(struct vlc_ml_media_t *)p_event->media_thumbnail_generated.p_media];
                if (mediaItem == nil) {
                    return;
                }
                dispatch_async(dispatch_get_main_queue(), ^{
                    VLCLibraryModel *libraryModel = (__bridge VLCLibraryModel *)p_data;
                    [libraryModel mediaItemWasUpdated:mediaItem];
                });
            }
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
        _sortCriteria = VLC_ML_SORTING_DEFAULT;
        _sortDescending = NO;
        _p_mediaLibrary = library;
        _p_eventCallback = vlc_ml_event_register_callback(_p_mediaLibrary, libraryCallback, (__bridge void *)self);
        _defaultNotificationCenter = [NSNotificationCenter defaultCenter];
        [_defaultNotificationCenter addObserver:self
                                       selector:@selector(applicationWillTerminate:)
                                           name:NSApplicationWillTerminateNotification
                                         object:nil];
    }
    return self;
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    if (_p_eventCallback) {
        vlc_ml_event_unregister_callback(_p_mediaLibrary, _p_eventCallback);
    }
}

- (void)dealloc
{
    [_defaultNotificationCenter removeObserver:self];
}

+ (NSArray *)availableAudioCollections
{
    return @[_NS("Artists"), _NS("Albums"), _NS("Songs"), _NS("Genres")];
}

- (void)mediaItemWasUpdated:(VLCMediaLibraryMediaItem *)mediaItem
{
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelMediaItemUpdated object:mediaItem];
}

- (size_t)numberOfAudioMedia
{
    if (!_cachedAudioMedia) {
        [self updateCachedListOfAudioMedia];
    }
    return _cachedAudioMedia.count;
}

- (void)updateCachedListOfAudioMedia
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_media_list_t *p_media_list = vlc_ml_list_audio_media(self->_p_mediaLibrary, NULL);
        if (!p_media_list) {
            return;
        }

        NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_media_list->i_nb_items];
        for (size_t x = 0; x < p_media_list->i_nb_items; x++) {
            VLCMediaLibraryMediaItem *mediaItem = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:&p_media_list->p_items[x]];
            if (mediaItem != nil) {
                [mutableArray addObject:mediaItem];
            }
        }
        vlc_ml_media_list_release(p_media_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_cachedAudioMedia = [mutableArray copy];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelAudioMediaListUpdated object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfAudioMedia
{
    if (!_cachedAudioMedia) {
        [self updateCachedListOfAudioMedia];
    }
    return _cachedAudioMedia;
}

- (size_t)numberOfArtists
{
    if (!_cachedArtists) {
        [self updateCachedListOfArtists];
    }
    return _cachedArtists.count;
}

- (void)updateCachedListOfArtists
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_artist_list_t *p_artist_list = vlc_ml_list_artists(self->_p_mediaLibrary, NULL, NO);
        NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_artist_list->i_nb_items];
        for (size_t x = 0; x < p_artist_list->i_nb_items; x++) {
            VLCMediaLibraryArtist *artist = [[VLCMediaLibraryArtist alloc] initWithArtist:&p_artist_list->p_items[x]];
            if (artist != nil) {
                [mutableArray addObject:artist];
            }
        }
        vlc_ml_artist_list_release(p_artist_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_cachedArtists = [mutableArray copy];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelArtistListUpdated object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryArtist *> *)listOfArtists
{
    if (!_cachedArtists) {
        [self updateCachedListOfArtists];
    }
    return _cachedArtists;
}

- (size_t)numberOfAlbums
{
    if (!_cachedAlbums) {
        [self updateCachedListOfAlbums];
    }
    return _cachedAlbums.count;
}

- (void)updateCachedListOfAlbums
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_album_list_t *p_album_list = vlc_ml_list_albums(self->_p_mediaLibrary, NULL);
        NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_album_list->i_nb_items];
        for (size_t x = 0; x < p_album_list->i_nb_items; x++) {
            VLCMediaLibraryAlbum *album = [[VLCMediaLibraryAlbum alloc] initWithAlbum:&p_album_list->p_items[x]];
            [mutableArray addObject:album];
        }
        vlc_ml_album_list_release(p_album_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_cachedAlbums = [mutableArray copy];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelArtistListUpdated object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryAlbum *> *)listOfAlbums
{
    if (!_cachedAlbums) {
        [self updateCachedListOfAlbums];
    }
    return _cachedAlbums;
}

- (size_t)numberOfGenres
{
    if (!_cachedGenres) {
        [self updateCachedListOfGenres];
    }
    return _cachedGenres.count;
}

- (void)updateCachedListOfGenres
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_genre_list_t *p_genre_list = vlc_ml_list_genres(self->_p_mediaLibrary, NULL);
        NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_genre_list->i_nb_items];
        for (size_t x = 0; x < p_genre_list->i_nb_items; x++) {
            VLCMediaLibraryGenre *genre = [[VLCMediaLibraryGenre alloc] initWithGenre:&p_genre_list->p_items[x]];
            [mutableArray addObject:genre];
        }
        vlc_ml_genre_list_release(p_genre_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_cachedGenres = [mutableArray copy];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelArtistListUpdated object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfGenres
{
    if (!_cachedGenres) {
        [self updateCachedListOfGenres];
    }
    return _cachedGenres;
}

- (size_t)numberOfVideoMedia
{
    if (!_cachedVideoMedia) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateCachedListOfVideoMedia];
        });
    }
    return _cachedVideoMedia.count;
}

- (void)updateCachedListOfVideoMedia
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_query_params_t queryParameters;
        memset(&queryParameters, 0, sizeof(vlc_ml_query_params_t));
        queryParameters.i_nbResults = 0;
        queryParameters.i_sort = self->_sortCriteria;
        queryParameters.b_desc = self->_sortDescending;
        vlc_ml_media_list_t *p_media_list = vlc_ml_list_video_media(self->_p_mediaLibrary, &queryParameters);
        if (p_media_list == NULL) {
            return;
        }
        NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_media_list->i_nb_items];
        for (size_t x = 0; x < p_media_list->i_nb_items; x++) {
            VLCMediaLibraryMediaItem *mediaItem = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:&p_media_list->p_items[x]];
            if (mediaItem != nil) {
                [mutableArray addObject:mediaItem];
            }
        }
        vlc_ml_media_list_release(p_media_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_cachedVideoMedia = [mutableArray copy];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelVideoMediaListUpdated object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfVideoMedia
{
    if (!_cachedVideoMedia) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateCachedListOfVideoMedia];
        });
    }
    return _cachedVideoMedia;
}

- (void)updateCachedListOfRecentMedia
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_query_params_t queryParameters;
        memset(&queryParameters, 0, sizeof(vlc_ml_query_params_t));
        queryParameters.i_nbResults = 20;
        // we don't set the sorting criteria here as they are not applicable to history
        vlc_ml_media_list_t *p_media_list = vlc_ml_list_history(self->_p_mediaLibrary, &queryParameters);
        if (p_media_list == NULL) {
            return;
        }
        NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_media_list->i_nb_items];
        for (size_t x = 0; x < p_media_list->i_nb_items; x++) {
            VLCMediaLibraryMediaItem *mediaItem = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:&p_media_list->p_items[x]];
            if (mediaItem != nil) {
                [mutableArray addObject:mediaItem];
            }
        }
        vlc_ml_media_list_release(p_media_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self->_cachedRecentMedia = [mutableArray copy];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentMediaListUpdated object:self];
        });
    });
}

- (size_t)numberOfRecentMedia
{
    if (!_cachedRecentMedia) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateCachedListOfRecentMedia];
        });
    }
    return _cachedRecentMedia.count;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentMedia
{
    if (!_cachedRecentMedia) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateCachedListOfRecentMedia];
        });
    }
    return _cachedRecentMedia;
}

- (NSArray<VLCMediaLibraryEntryPoint *> *)listOfMonitoredFolders
{
    vlc_ml_entry_point_list_t *pp_entrypoints;
    int ret = vlc_ml_list_folder(_p_mediaLibrary, &pp_entrypoints);
    if (ret != VLC_SUCCESS) {
        msg_Err(getIntf(), "failed to retrieve list of monitored library folders (%i)", ret);
        return @[];
    }

    NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:pp_entrypoints->i_nb_items];
    for (size_t x = 0; x < pp_entrypoints->i_nb_items; x++) {
        VLCMediaLibraryEntryPoint *entryPoint = [[VLCMediaLibraryEntryPoint alloc] initWithEntryPoint:&pp_entrypoints->p_items[x]];
        if (entryPoint) {
            [mutableArray addObject:entryPoint];
        }
    }

    vlc_ml_entry_point_list_release(pp_entrypoints);
    return [mutableArray copy];
}

- (nullable NSArray <VLCMediaLibraryAlbum *>*)listAlbumsOfParentType:(enum vlc_ml_parent_type)parentType forID:(int64_t)ID;
{
    vlc_ml_album_list_t *p_albumList = vlc_ml_list_albums_of(_p_mediaLibrary, NULL, parentType, ID);
    if (p_albumList == NULL) {
        return nil;
    }
    NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_albumList->i_nb_items];
    for (size_t x = 0; x < p_albumList->i_nb_items; x++) {
        VLCMediaLibraryAlbum *album = [[VLCMediaLibraryAlbum alloc] initWithAlbum:&p_albumList->p_items[x]];
        [mutableArray addObject:album];
    }
    vlc_ml_album_list_release(p_albumList);
    return [mutableArray copy];
}

- (void)sortByCriteria:(enum vlc_ml_sorting_criteria_t)sortCriteria andDescending:(bool)descending
{
    _sortCriteria = sortCriteria;
    _sortDescending = descending;
    [self dropCaches];
}

- (void)dropCaches
{
    _cachedVideoMedia = nil;
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelVideoMediaListUpdated object:nil];
}

@end
