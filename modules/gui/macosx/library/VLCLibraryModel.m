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
#import "extensions/NSArray+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

NSString * const VLCLibraryModelArtistListUpdated = @"VLCLibraryModelArtistListUpdated";
NSString * const VLCLibraryModelAlbumListUpdated = @"VLCLibraryModelAlbumListUpdated";
NSString * const VLCLibraryModelGenreListUpdated = @"VLCLibraryModelGenreListUpdated";
NSString * const VLCLibraryModelListOfMonitoredFoldersUpdated = @"VLCLibraryModelListOfMonitoredFoldersUpdated";
NSString * const VLCLibraryModelMediaItemThumbnailGenerated = @"VLCLibraryModelMediaItemThumbnailGenerated";

NSString * const VLCLibraryModelAudioMediaListReset = @"VLCLibraryModelAudioMediaListReset";
NSString * const VLCLibraryModelVideoMediaListReset = @"VLCLibraryModelVideoMediaListReset";
NSString * const VLCLibraryModelRecentsMediaListReset = @"VLCLibraryModelRecentsMediaListReset";
NSString * const VLCLibraryModelRecentAudioMediaListReset = @"VLCLibraryModelRecentAudioMediaListReset";

NSString * const VLCLibraryModelAudioMediaItemDeleted = @"VLCLibraryModelAudioMediaItemDeleted";
NSString * const VLCLibraryModelVideoMediaItemDeleted = @"VLCLibraryModelVideoMediaItemDeleted";
NSString * const VLCLibraryModelRecentsMediaItemDeleted = @"VLCLibraryModelRecentsMediaItemDeleted";
NSString * const VLCLibraryModelRecentAudioMediaItemDeleted = @"VLCLibraryModelRecentAudioMediaItemDeleted";
NSString * const VLCLibraryModelAlbumDeleted = @"VLCLibraryModelAlbumDeleted";
NSString * const VLCLibraryModelArtistDeleted = @"VLCLibraryModelArtistDeleted";
NSString * const VLCLibraryModelGenreDeleted = @"VLCLibraryModelGenreDeleted";

NSString * const VLCLibraryModelAudioMediaItemUpdated = @"VLCLibraryModelAudioMediaItemUpdated";
NSString * const VLCLibraryModelVideoMediaItemUpdated = @"VLCLibraryModelVideoMediaItemUpdated";
NSString * const VLCLibraryModelRecentsMediaItemUpdated = @"VLCLibraryModelRecentsMediaItemUpdated";
NSString * const VLCLibraryModelRecentAudioMediaItemUpdated = @"VLCLibraryModelRecentAudioMediaItemUpdated";
NSString * const VLCLibraryModelAlbumUpdated = @"VLCLibraryModelAlbumUpdated";
NSString * const VLCLibraryModelArtistUpdated = @"VLCLibraryModelArtistUpdated";
NSString * const VLCLibraryModelGenreUpdated = @"VLCLibraryModelGenreUpdated";

@interface VLCLibraryModel ()
{
    vlc_medialibrary_t *_p_mediaLibrary;
    vlc_ml_event_callback_t *_p_eventCallback;

    NSNotificationCenter *_defaultNotificationCenter;

    enum vlc_ml_sorting_criteria_t _sortCriteria;
    bool _sortDescending;
    NSString *_filterString;

    size_t _initialVideoCount;
    size_t _initialAudioCount;
    size_t _initialAlbumCount;
    size_t _initialArtistCount;
    size_t _initialGenreCount;
    size_t _initialRecentsCount;
    size_t _initialRecentAudioCount;

    dispatch_queue_t _mediaItemCacheModificationQueue;
    dispatch_queue_t _albumCacheModificationQueue;
    dispatch_queue_t _artistCacheModificationQueue;
    dispatch_queue_t _genreCacheModificationQueue;
}

@property (readwrite, atomic) NSArray *cachedAudioMedia;
@property (readwrite, atomic) NSArray *cachedArtists;
@property (readwrite, atomic) NSArray *cachedAlbums;
@property (readwrite, atomic) NSArray *cachedGenres;
@property (readwrite, atomic) NSArray *cachedVideoMedia;
@property (readwrite, atomic) NSArray *cachedRecentMedia;
@property (readwrite, atomic) NSArray *cachedRecentAudioMedia;
@property (readwrite, atomic) NSArray *cachedListOfMonitoredFolders;

- (void)resetCachedMediaItemLists;
- (void)resetCachedListOfArtists;
- (void)resetCachedListOfAlbums;
- (void)resetCachedListOfGenres;
- (void)resetCachedListOfMonitoredFolders;
- (void)mediaItemThumbnailGenerated:(VLCMediaLibraryMediaItem *)mediaItem;
- (void)handleMediaItemDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleAlbumDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleArtistDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleGenreDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleMediaItemUpdateEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleAlbumUpdateEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleArtistUpdateEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleGenreUpdateEvent:(const vlc_ml_event_t * const)p_event;

@end

static void libraryCallback(void *p_data, const vlc_ml_event_t *p_event)
{
    VLCLibraryModel * const libraryModel = (__bridge VLCLibraryModel *)p_data;
    if (libraryModel == nil) {
        return;
    }

    switch(p_event->i_type)
    {
        case VLC_ML_EVENT_MEDIA_ADDED:
            [libraryModel resetCachedMediaItemLists];
            break;
        case VLC_ML_EVENT_MEDIA_UPDATED:
            [libraryModel handleMediaItemUpdateEvent:p_event];
            break;
        case VLC_ML_EVENT_MEDIA_DELETED:
            [libraryModel handleMediaItemDeletionEvent:p_event];
            break;
        case VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED:
            if (p_event->media_thumbnail_generated.b_success) {
                VLCMediaLibraryMediaItem *mediaItem = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:(struct vlc_ml_media_t *)p_event->media_thumbnail_generated.p_media];
                if (mediaItem == nil) {
                    return;
                }
                dispatch_async(dispatch_get_main_queue(), ^{
                    [libraryModel mediaItemThumbnailGenerated:mediaItem];
                });
            }
            break;
        case VLC_ML_EVENT_ARTIST_ADDED:
            [libraryModel resetCachedListOfArtists];
            break;
        case VLC_ML_EVENT_ARTIST_UPDATED:
            [libraryModel handleArtistUpdateEvent:p_event];
            break;
        case VLC_ML_EVENT_ARTIST_DELETED:
            [libraryModel handleArtistDeletionEvent:p_event];
            break;
        case VLC_ML_EVENT_ALBUM_ADDED:
            [libraryModel resetCachedListOfAlbums];
            break;
        case VLC_ML_EVENT_ALBUM_UPDATED:
            [libraryModel handleAlbumUpdateEvent:p_event];
            break;
        case VLC_ML_EVENT_ALBUM_DELETED:
            [libraryModel handleAlbumDeletionEvent:p_event];
            break;
        case VLC_ML_EVENT_GENRE_ADDED:
            [libraryModel resetCachedListOfGenres];
            break;
        case VLC_ML_EVENT_GENRE_UPDATED:
            [libraryModel handleGenreUpdateEvent:p_event];
            break;
        case VLC_ML_EVENT_GENRE_DELETED:
            [libraryModel handleGenreDeletionEvent:p_event];
            break;
        case VLC_ML_EVENT_FOLDER_ADDED:
        case VLC_ML_EVENT_FOLDER_UPDATED:
        case VLC_ML_EVENT_FOLDER_DELETED:
            [libraryModel resetCachedListOfMonitoredFolders];
            break;
        default:
            break;
    }
}

@implementation VLCLibraryModel

+ (NSUInteger)modelIndexFromModelItemNotification:(NSNotification * const)aNotification
{
    NSParameterAssert(aNotification);
    NSDictionary * const notificationUserInfo = aNotification.userInfo;
    NSAssert(notificationUserInfo != nil, @"Video item-related notification should carry valid user info");

    NSNumber * const modelIndexNumber = (NSNumber * const)[notificationUserInfo objectForKey:@"index"];
    NSAssert(modelIndexNumber != nil, @"Video item notification user info should carry index for updated item");

    return modelIndexNumber.longLongValue;
}

- (instancetype)initWithLibrary:(vlc_medialibrary_t *)library
{
    self = [super init];
    if (self) {
        _sortCriteria = VLC_ML_SORTING_DEFAULT;
        _sortDescending = NO;
        _filterString = @"";
        _recentMediaLimit = 20;
        _p_mediaLibrary = library;
        _p_eventCallback = vlc_ml_event_register_callback(_p_mediaLibrary, libraryCallback, (__bridge void *)self);

        // Serial queues make avoiding concurrent modification of the caches easy, while avoiding
        // locking other queues
        _mediaItemCacheModificationQueue = dispatch_queue_create("mediaItemCacheModificationQueue", 0);
        _albumCacheModificationQueue = dispatch_queue_create("albumCacheModificationQueue", 0);
        _artistCacheModificationQueue = dispatch_queue_create("artistCacheModificationQueue", 0);
        _genreCacheModificationQueue = dispatch_queue_create("genreCacheModificationQueue", 0);

        _defaultNotificationCenter = NSNotificationCenter.defaultCenter;
        [_defaultNotificationCenter addObserver:self
                                       selector:@selector(applicationWillTerminate:)
                                           name:NSApplicationWillTerminateNotification
                                         object:nil];

        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            const vlc_ml_query_params_t queryParameters = {};

            // Preload video and audio count for gui
            self->_initialVideoCount = vlc_ml_count_video_media(self->_p_mediaLibrary, &queryParameters);
            self->_initialAudioCount = vlc_ml_count_audio_media(self->_p_mediaLibrary, &queryParameters);
            self->_initialAlbumCount = vlc_ml_count_albums(self->_p_mediaLibrary, &queryParameters);
            self->_initialArtistCount = vlc_ml_count_artists(self->_p_mediaLibrary, &queryParameters, true);
            self->_initialGenreCount = vlc_ml_count_genres(self->_p_mediaLibrary, &queryParameters);
            self->_initialRecentsCount = vlc_ml_count_history_by_type(self->_p_mediaLibrary, &((vlc_ml_query_params_t){ .i_nbResults = self->_recentMediaLimit }), VLC_ML_MEDIA_TYPE_VIDEO);
            self->_initialRecentAudioCount = vlc_ml_count_history_by_type(self->_p_mediaLibrary, &((vlc_ml_query_params_t){ .i_nbResults = self->_recentMediaLimit }), VLC_ML_MEDIA_TYPE_AUDIO);
        });
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

- (void)mediaItemThumbnailGenerated:(VLCMediaLibraryMediaItem *)mediaItem
{
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelMediaItemThumbnailGenerated object:mediaItem];
}

- (size_t)numberOfAudioMedia
{
    if (!_cachedAudioMedia) {
        [self resetCachedListOfAudioMedia];

        // Return initial count here, otherwise it will return 0 on the first time
        return _initialAudioCount;
    }
    return _cachedAudioMedia.count;
}

- (vlc_ml_query_params_t)queryParams
{
    const vlc_ml_query_params_t queryParams = { .psz_pattern = self->_filterString.length > 0 ? [self->_filterString UTF8String] : NULL, 
                                                .i_sort = self->_sortCriteria, 
                                                .b_desc = self->_sortDescending };
    return queryParams;
}

- (void)resetCachedListOfAudioMedia
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t queryParams = [self queryParams];
        vlc_ml_media_list_t * const p_media_list = vlc_ml_list_audio_media(self->_p_mediaLibrary, &queryParams);
        NSArray * const mediaArray = [NSArray arrayFromVlcMediaList:p_media_list];
        if (mediaArray == nil) {
            return;
        }
        vlc_ml_media_list_release(p_media_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedAudioMedia = mediaArray;
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelAudioMediaListReset object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfAudioMedia
{
    if (!_cachedAudioMedia) {
        [self resetCachedListOfAudioMedia];
    }
    return _cachedAudioMedia;
}

- (size_t)numberOfArtists
{
    if (!_cachedArtists) {
        [self resetCachedListOfArtists];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialArtistCount;
    }
    return _cachedArtists.count;
}

- (void)resetCachedListOfArtists
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t queryParams = [self queryParams];
        vlc_ml_artist_list_t * const p_artist_list = vlc_ml_list_artists(self->_p_mediaLibrary, &queryParams, NO);
        const size_t numberOfArtists = p_artist_list->i_nb_items;
        NSMutableArray * const mutableArtistArray = [[NSMutableArray alloc] initWithCapacity:numberOfArtists];
        NSMutableDictionary * const mutableArtistDict = [NSMutableDictionary dictionaryWithCapacity:numberOfArtists];

        for (size_t x = 0; x < numberOfArtists; x++) {
            VLCMediaLibraryArtist * const artist = [[VLCMediaLibraryArtist alloc] initWithArtist:&p_artist_list->p_items[x]];

            if (artist != nil) {
                [mutableArtistArray addObject:artist];
                [mutableArtistDict setObject:artist.name forKey:@(artist.libraryID)];
            }
        }

        vlc_ml_artist_list_release(p_artist_list);

        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedArtists = mutableArtistArray.copy;
            self->_artistDict = mutableArtistDict.copy;
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelArtistListUpdated object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryArtist *> *)listOfArtists
{
    if (!_cachedArtists) {
        [self resetCachedListOfArtists];
    }
    return _cachedArtists;
}

- (size_t)numberOfAlbums
{
    if (!_cachedAlbums) {
        [self resetCachedListOfAlbums];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialAlbumCount;
    }
    return _cachedAlbums.count;
}

- (void)resetCachedListOfAlbums
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t queryParams = [self queryParams];
        vlc_ml_album_list_t * const p_album_list = vlc_ml_list_albums(self->_p_mediaLibrary, &queryParams);
        const size_t numberOfAlbums = p_album_list->i_nb_items;
        NSMutableArray * const mutableAlbumArray = [[NSMutableArray alloc] initWithCapacity:numberOfAlbums];
        NSMutableDictionary * const mutableAlbumDict = [NSMutableDictionary dictionaryWithCapacity:numberOfAlbums];

        for (size_t x = 0; x < numberOfAlbums; x++) {
            VLCMediaLibraryAlbum * const album = [[VLCMediaLibraryAlbum alloc] initWithAlbum:&p_album_list->p_items[x]];
            [mutableAlbumArray addObject:album];
            [mutableAlbumDict setObject:album.title forKey:@(album.libraryID)];
        }

        vlc_ml_album_list_release(p_album_list);

        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedAlbums = mutableAlbumArray.copy;
            self->_albumDict = mutableAlbumDict.copy;
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelAlbumListUpdated object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryAlbum *> *)listOfAlbums
{
    if (!_cachedAlbums) {
        [self resetCachedListOfAlbums];
    }
    return _cachedAlbums;
}

- (size_t)numberOfGenres
{
    if (!_cachedGenres) {
        [self resetCachedListOfGenres];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialGenreCount;
    }
    return _cachedGenres.count;
}

- (void)resetCachedListOfGenres
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t queryParams = [self queryParams];
        vlc_ml_genre_list_t * const p_genre_list = vlc_ml_list_genres(self->_p_mediaLibrary, &queryParams);
        const size_t numberOfGenres = p_genre_list->i_nb_items;
        NSMutableArray * const mutableGenreArray = [[NSMutableArray alloc] initWithCapacity:numberOfGenres];
        NSMutableDictionary * const mutableGenreDict = [NSMutableDictionary dictionaryWithCapacity:numberOfGenres];

        for (size_t x = 0; x < numberOfGenres; x++) {
            VLCMediaLibraryGenre * const genre = [[VLCMediaLibraryGenre alloc] initWithGenre:&p_genre_list->p_items[x]];
            [mutableGenreArray addObject:genre];
            [mutableGenreDict setObject:genre.name forKey:@(genre.libraryID)];
        }

        vlc_ml_genre_list_release(p_genre_list);

        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedGenres = mutableGenreArray.copy;
            self->_genreDict = mutableGenreDict.copy;
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelGenreListUpdated object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfGenres
{
    if (!_cachedGenres) {
        [self resetCachedListOfGenres];
    }
    return _cachedGenres;
}

- (size_t)numberOfVideoMedia
{
    if (!_cachedVideoMedia) {
        [self resetCachedListOfVideoMedia];

        // Return initial count here, otherwise it will return 0 on the first time
        return _initialVideoCount;
    }
    return _cachedVideoMedia.count;
}

- (void)resetCachedListOfVideoMedia
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t queryParameters = [self queryParams];
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
            self.cachedVideoMedia = [mutableArray copy];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelVideoMediaListReset object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfVideoMedia
{
    if (!_cachedVideoMedia) {
        [self resetCachedListOfVideoMedia];
    }
    return _cachedVideoMedia;
}

- (void)getListOfRecentMediaOfType:(vlc_ml_media_type_t)type
                    withCountLimit:(size_t)countLimit
                    withCompletion:(void (^)(NSArray *recentMediaArray))completionHandler
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t queryParameters = { .i_nbResults = countLimit };
        // we don't set the sorting criteria here as they are not applicable to history
        vlc_ml_media_list_t *p_media_list = vlc_ml_list_history_by_type(self->_p_mediaLibrary, &queryParameters, type);
        NSArray * const mediaArray = [NSArray arrayFromVlcMediaList:p_media_list];
        if (mediaArray == nil) {
            return;
        }
        vlc_ml_media_list_release(p_media_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            completionHandler(mediaArray);
        });
    });
}

- (void)resetCachedListOfRecentMedia
{
    [self getListOfRecentMediaOfType:VLC_ML_MEDIA_TYPE_VIDEO
                      withCountLimit:_recentMediaLimit
                      withCompletion:^(NSArray * const mediaArray) {
        self.cachedRecentMedia = mediaArray;
        [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentsMediaListReset object:self];
    }];
}

- (size_t)numberOfRecentMedia
{
    if (!_cachedRecentMedia) {
        [self resetCachedListOfRecentMedia];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialRecentsCount;
    }
    return _cachedRecentMedia.count;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentMedia
{
    if (!_cachedRecentMedia) {
        [self resetCachedListOfRecentMedia];
    }
    return _cachedRecentMedia;
}

- (void)resetCachedListOfRecentAudioMedia
{
    [self getListOfRecentMediaOfType:VLC_ML_MEDIA_TYPE_AUDIO
                      withCountLimit:_recentAudioMediaLimit
                      withCompletion:^(NSArray * const mediaArray) {
        self.cachedRecentAudioMedia = mediaArray;
        [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentAudioMediaListReset object:self];
    }];
}

// TODO: See above
- (size_t)numberOfRecentAudioMedia
{
    if (!_cachedRecentAudioMedia) {
        [self resetCachedListOfRecentAudioMedia];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialRecentAudioCount;
    }
    return _cachedRecentAudioMedia.count;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentAudioMedia
{
    if (!_cachedRecentAudioMedia) {
        [self resetCachedListOfRecentAudioMedia];
    }
    return _cachedRecentAudioMedia;
}

- (void)resetCachedMediaItemLists
{
    [self resetCachedListOfRecentMedia];
    [self resetCachedListOfRecentAudioMedia];
    [self resetCachedListOfAudioMedia];
    [self resetCachedListOfVideoMedia];
}

- (void)resetCachedListOfMonitoredFolders
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_folder_list_t *pp_entrypoints = vlc_ml_list_entry_points(self->_p_mediaLibrary, NULL);
        if (pp_entrypoints == NULL) {
            msg_Err(getIntf(), "failed to retrieve list of monitored library folders");
            return;
        }

        NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:pp_entrypoints->i_nb_items];
        for (size_t x = 0; x < pp_entrypoints->i_nb_items; x++) {
            VLCMediaLibraryEntryPoint *entryPoint = [[VLCMediaLibraryEntryPoint alloc] initWithEntryPoint:&pp_entrypoints->p_items[x]];
            if (entryPoint) {
                [mutableArray addObject:entryPoint];
            }
        }

        vlc_ml_folder_list_release(pp_entrypoints);

        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedListOfMonitoredFolders = [mutableArray copy];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelListOfMonitoredFoldersUpdated object:self];
        });
    });
}

- (NSArray<VLCMediaLibraryEntryPoint *> *)listOfMonitoredFolders
{
    if(!_cachedListOfMonitoredFolders) {
        [self resetCachedListOfMonitoredFolders];
    }

    return _cachedListOfMonitoredFolders;
}

- (nullable NSArray <VLCMediaLibraryAlbum *>*)listAlbumsOfParentType:(const enum vlc_ml_parent_type)parentType forID:(int64_t)ID;
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    vlc_ml_album_list_t *p_albumList = vlc_ml_list_albums_of(_p_mediaLibrary, &queryParams, parentType, ID);
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

- (NSArray<id<VLCMediaLibraryItemProtocol>> *)listOfLibraryItemsOfParentType:(const VLCMediaLibraryParentGroupType)parentType
{
    switch(parentType) {
    case VLCMediaLibraryParentGroupTypeArtist:
        return self.listOfArtists;
    case VLCMediaLibraryParentGroupTypeAlbum:
        return self.listOfAlbums;
    case VLCMediaLibraryParentGroupTypeGenre:
        return self.listOfGenres;
    case VLCMediaLibraryParentGroupTypeAudioLibrary:
        return self.listOfAudioMedia;
    case VLCMediaLibraryParentGroupTypeVideoLibrary:
        return self.listOfVideoMedia;
    case VLCMediaLibraryParentGroupTypeRecentVideos:
        return self.listOfRecentMedia;
    case VLCMediaLibraryParentGroupTypeUnknown:
    default:
        return nil;
    }
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfMediaItemsForParentType:(const VLCMediaLibraryParentGroupType)parentType
{
    NSArray<id<VLCMediaLibraryItemProtocol>> * const libraryItems = [self listOfLibraryItemsOfParentType:parentType];
    NSMutableArray<VLCMediaLibraryMediaItem *> * const mediaItems = [[NSMutableArray alloc] initWithCapacity:self.numberOfAudioMedia];

    for (id<VLCMediaLibraryItemProtocol> libraryItem in libraryItems) {
        [mediaItems addObjectsFromArray:libraryItem.mediaItems];
    }

    return mediaItems.copy;
}

- (void)sortByCriteria:(enum vlc_ml_sorting_criteria_t)sortCriteria andDescending:(bool)descending
{
    if(sortCriteria == _sortCriteria && descending == _sortDescending) {
        return;
    }

    _sortCriteria = sortCriteria;
    _sortDescending = descending;
    [self dropCaches];
}

- (void)filterByString:(NSString*)filterString
{
    if([filterString isEqualToString:_filterString]) {
        return;
    }

    _filterString = filterString;
    [self dropCaches];
}

- (void)dropCaches
{
    _cachedVideoMedia = nil;
    _cachedAlbums = nil;
    _cachedGenres = nil;
    _cachedArtists = nil;
    _cachedAudioMedia = nil;
    _cachedRecentMedia = nil;
    _cachedRecentAudioMedia = nil;

    [_defaultNotificationCenter postNotificationName:VLCLibraryModelVideoMediaListReset object:self];
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelAlbumListUpdated object:self];
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelGenreListUpdated object:self];
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelArtistListUpdated object:self];
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelAudioMediaListReset object:self];
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentsMediaListReset object:self];
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentAudioMediaListReset object:self];
}

- (void)performActionOnMediaItemInCache:(const int64_t)libraryId action:(void (^)(const NSMutableArray*, const NSUInteger, const NSMutableArray*, const NSUInteger))action
{
    dispatch_async(_mediaItemCacheModificationQueue, ^{
        BOOL (^idCheckBlock)(VLCMediaLibraryMediaItem * const, const NSUInteger, BOOL * const) = ^BOOL(VLCMediaLibraryMediaItem * const mediaItem, const NSUInteger idx, BOOL * const stop) {
            NSAssert(mediaItem != nil, @"Cache list should not contain nil media items");
            return mediaItem.libraryID == libraryId;
        };

        // Recents can contain media items the other two do
        NSMutableArray * const recentsMutable = self.cachedRecentMedia.mutableCopy;
        const NSUInteger recentsIndex = [recentsMutable indexOfObjectPassingTest:idCheckBlock];
        const BOOL isInRecents = recentsIndex != NSNotFound;

        NSMutableArray * const videoMutable = self.cachedVideoMedia.mutableCopy;
        const NSUInteger videoIndex = [videoMutable indexOfObjectPassingTest:idCheckBlock];
        if (videoIndex != NSNotFound) {
            dispatch_sync(dispatch_get_main_queue(), ^{
                action(videoMutable, videoIndex, recentsMutable, recentsIndex);
                self.cachedVideoMedia = videoMutable.copy;
                self.cachedRecentMedia = recentsMutable.copy;
            });
            return;
        }

        NSMutableArray * const recentAudiosMutable = self.cachedRecentAudioMedia.mutableCopy;
        const NSUInteger recentAudiosIndex = [recentAudiosMutable indexOfObjectPassingTest:idCheckBlock];
        const BOOL isInRecentAudios = recentAudiosIndex != NSNotFound;

        NSMutableArray * const audioMutable = self.cachedAudioMedia.mutableCopy;
        const NSUInteger audioIndex = [self.cachedAudioMedia indexOfObjectPassingTest:idCheckBlock];
        if (audioIndex != NSNotFound) {
            dispatch_sync(dispatch_get_main_queue(), ^{
                action(audioMutable, audioIndex, recentAudiosMutable, recentAudiosIndex);
                self.cachedAudioMedia = audioMutable.copy;
                self.cachedRecentAudioMedia = recentsMutable.copy;
            });
            return;
        }

        action(nil, NSNotFound, nil, NSNotFound);
    });
}

- (void)handleMediaItemUpdateEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    VLCMediaLibraryMediaItem * const mediaItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:itemId];
    if (mediaItem == nil) {
        NSLog(@"Could not find a library media item with this ID. Can't handle update.");
        return;
    }

    [self performActionOnMediaItemInCache:itemId action:^(NSMutableArray * const cachedMediaArray, const NSUInteger cachedMediaIndex, NSMutableArray * const recentMediaArray, const NSUInteger recentMediaIndex) {

        if (cachedMediaArray == nil || cachedMediaIndex == NSNotFound) {
            NSLog(@"Could not handle update for media library item with id %lld in model", itemId);
            return;
        }

        // Notify what happened
        [cachedMediaArray replaceObjectAtIndex:cachedMediaIndex withObject:mediaItem];

        if (recentMediaArray != nil && recentMediaIndex != NSNotFound) {
            [cachedMediaArray replaceObjectAtIndex:recentMediaIndex withObject:mediaItem];
            switch (mediaItem.mediaType) {
                case VLC_ML_MEDIA_TYPE_VIDEO:
                    [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentsMediaItemUpdated object:mediaItem];
                    break;
                case VLC_ML_MEDIA_TYPE_AUDIO:
                    [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentAudioMediaItemUpdated object:mediaItem];
                    break;
                case VLC_ML_MEDIA_TYPE_UNKNOWN:
                    NSLog(@"Unknown type of media type encountered, don't know what to do in deletion");
                    break;
            }
        }

        switch (mediaItem.mediaType) {
            case VLC_ML_MEDIA_TYPE_VIDEO:
                [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelVideoMediaItemUpdated object:mediaItem];
                break;
            case VLC_ML_MEDIA_TYPE_AUDIO:
                [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelAudioMediaItemUpdated object:mediaItem];
                break;
            case VLC_ML_MEDIA_TYPE_UNKNOWN:
                NSLog(@"Unknown type of media type encountered, don't know what to do in update");
                break;
        }
    }];
}

- (void)handleMediaItemDeletionEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    [self performActionOnMediaItemInCache:itemId action:^(NSMutableArray * const cachedMediaArray, const NSUInteger cachedMediaIndex, NSMutableArray * const recentMediaArray, const NSUInteger recentMediaIndex) {

        if (cachedMediaArray == nil || cachedMediaIndex == NSNotFound) {
            NSLog(@"Could not handle deletion for media library item with id %lld in model", itemId);
            return;
        }

        VLCMediaLibraryMediaItem * const mediaItem = cachedMediaArray[cachedMediaIndex];
        // Notify what happened
        [cachedMediaArray removeObjectAtIndex:cachedMediaIndex];

        if (recentMediaArray != nil && recentMediaIndex != NSNotFound) {
            [recentMediaArray removeObjectAtIndex:recentMediaIndex];
            switch (mediaItem.mediaType) {
                case VLC_ML_MEDIA_TYPE_VIDEO:
                    [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentsMediaItemDeleted object:mediaItem];
                    break;
                case VLC_ML_MEDIA_TYPE_AUDIO:
                    [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentAudioMediaItemDeleted object:mediaItem];
                    break;
                case VLC_ML_MEDIA_TYPE_UNKNOWN:
                    NSLog(@"Unknown type of media type encountered, don't know what to do in deletion");
                    break;
            }
        }

        switch (mediaItem.mediaType) {
            case VLC_ML_MEDIA_TYPE_VIDEO:
                [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelVideoMediaItemDeleted object:mediaItem];
                break;
            case VLC_ML_MEDIA_TYPE_AUDIO:
                [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelAudioMediaItemDeleted object:mediaItem];
                break;
            case VLC_ML_MEDIA_TYPE_UNKNOWN:
                NSLog(@"Unknown type of media type encountered, don't know what to do in deletion");
                break;
        }
    }];
}

- (NSInteger)indexForAudioGroupInCache:(NSArray * const)cache withItemId:(const int64_t)itemId
{
    return [cache indexOfObjectPassingTest:^BOOL(id<VLCMediaLibraryAudioGroupProtocol> audioGroupItem, const NSUInteger idx, BOOL * const stop) {
        NSAssert(audioGroupItem != nil, @"Cache list should not contain nil audio group items");
        return audioGroupItem.libraryID == itemId;
    }];
}

- (void)updateAudioGroupItem:(const id<VLCMediaLibraryAudioGroupProtocol>)audioGroupItem
                     inCache:(NSArray * const)cache
                 usingSetter:(const SEL)setterSelector
                  usingQueue:(const dispatch_queue_t)queue
        withNotificationName:(const NSNotificationName)notificationName
{
    NSParameterAssert([self respondsToSelector:setterSelector]);
    const int64_t itemId = audioGroupItem.libraryID;

    dispatch_async(queue, ^{
        const NSUInteger audioGroupIndex = [self indexForAudioGroupInCache:cache
                                                                withItemId:itemId];
        if (audioGroupIndex == NSNotFound) {
            NSLog(@"Did not find audio group item with id %lli in cache.", itemId);
            return;
        }

        // Block calling queue while we modify the cache, preventing dangerous concurrent modification
        dispatch_sync(dispatch_get_main_queue(), ^{
            NSMutableArray * const mutableAudioGroupCache = [cache mutableCopy];
            [mutableAudioGroupCache replaceObjectAtIndex:audioGroupIndex withObject:audioGroupItem];
            NSArray * const immutableCopy = [mutableAudioGroupCache copy];

            const IMP cacheSetterImp = [self methodForSelector:setterSelector];
            void (*cacheSetterFunction)(id, SEL, NSArray *) = (void *)cacheSetterImp;
            cacheSetterFunction(self, setterSelector, immutableCopy);

            [self->_defaultNotificationCenter postNotificationName:notificationName
                                                            object:audioGroupItem];
        });
    });
}

- (void)deleteAudioGroupItemWithId:(const int64_t)itemId
                           inCache:(NSArray * const)cache
                       usingSetter:(const SEL)setterSelector
                        usingQueue:(const dispatch_queue_t)queue
              withNotificationName:(const NSNotificationName)notificationName
{
    NSParameterAssert([self respondsToSelector:setterSelector]);

    dispatch_async(queue, ^{
        const NSUInteger audioGroupIndex = [self indexForAudioGroupInCache:cache
                                                                withItemId:itemId];
        if (audioGroupIndex == NSNotFound) {
            NSLog(@"Did not find audio group item with id %lli in cache.", itemId);
            return;
        }

        const id<VLCMediaLibraryAudioGroupProtocol> audioGroupItem = cache[audioGroupIndex];

        // Block calling queue while we modify the cache, preventing dangerous concurrent modification
        dispatch_sync(dispatch_get_main_queue(), ^{
            NSMutableArray * const mutableAudioGroupCache = [cache mutableCopy];
            [mutableAudioGroupCache removeObjectAtIndex:audioGroupIndex];
            NSArray * const immutableCopy = [mutableAudioGroupCache copy];

            const IMP cacheSetterImp = [self methodForSelector:setterSelector];
            void (*cacheSetterFunction)(id, SEL, NSArray *) = (void *)cacheSetterImp;
            cacheSetterFunction(self, setterSelector, immutableCopy);

            [self->_defaultNotificationCenter postNotificationName:notificationName
                                                            object:audioGroupItem];
        });
    });
}

- (void)handleAlbumUpdateEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    VLCMediaLibraryAlbum * const album = [VLCMediaLibraryAlbum albumWithID:itemId];
    if (album == nil) {
        NSLog(@"Could not find a library album with this ID. Can't handle update.");
        return;
    }

    [self updateAudioGroupItem:album
                       inCache:_cachedAlbums
                   usingSetter:@selector(setCachedAlbums:)
                    usingQueue:_albumCacheModificationQueue
          withNotificationName:VLCLibraryModelAlbumUpdated];
}

- (void)handleAlbumDeletionEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    [self deleteAudioGroupItemWithId:itemId
                             inCache:_cachedAlbums
                         usingSetter:@selector(setCachedAlbums:)
                          usingQueue:_albumCacheModificationQueue
                withNotificationName:VLCLibraryModelAlbumDeleted];
}

- (void)handleArtistUpdateEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    VLCMediaLibraryArtist * const artist = [VLCMediaLibraryArtist artistWithID:itemId];
    if (artist == nil) {
        NSLog(@"Could not find a library artist with this ID. Can't handle update.");
        return;
    }

    [self updateAudioGroupItem:artist
                       inCache:_cachedArtists
                   usingSetter:@selector(setCachedArtists:)
                    usingQueue:_artistCacheModificationQueue
          withNotificationName:VLCLibraryModelArtistUpdated];
}

- (void)handleArtistDeletionEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    [self deleteAudioGroupItemWithId:itemId
                             inCache:_cachedArtists
                         usingSetter:@selector(setCachedArtists:)
                          usingQueue:_artistCacheModificationQueue
                withNotificationName:VLCLibraryModelArtistDeleted];
}

- (void)handleGenreUpdateEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    VLCMediaLibraryGenre * const genre = [VLCMediaLibraryGenre genreWithID:itemId];
    if (genre == nil) {
        NSLog(@"Could not find a library genre with this ID. Can't handle update.");
        return;
    }

    [self updateAudioGroupItem:genre
                       inCache:_cachedGenres
                   usingSetter:@selector(setCachedGenres:)
                    usingQueue:_genreCacheModificationQueue
          withNotificationName:VLCLibraryModelGenreUpdated];
}

- (void)handleGenreDeletionEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    [self deleteAudioGroupItemWithId:itemId
                             inCache:_cachedGenres
                         usingSetter:@selector(setCachedGenres:)
                          usingQueue:_genreCacheModificationQueue
                withNotificationName:VLCLibraryModelGenreDeleted];
}

@end
