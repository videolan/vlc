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

#import "VLCMediaLibraryFolderObserver.h"

#import "extensions/NSArray+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "main/VLCMain.h"

NSString * const VLCLibraryModelArtistListReset = @"VLCLibraryModelArtistListReset";
NSString * const VLCLibraryModelAlbumListReset = @"VLCLibraryModelAlbumListReset";
NSString * const VLCLibraryModelGenreListReset = @"VLCLibraryModelGenreListReset";
NSString * const VLCLibraryModelListOfMonitoredFoldersUpdated = @"VLCLibraryModelListOfMonitoredFoldersUpdated";
NSString * const VLCLibraryModelMediaItemThumbnailGenerated = @"VLCLibraryModelMediaItemThumbnailGenerated";

NSString * const VLCLibraryModelAudioMediaListReset = @"VLCLibraryModelAudioMediaListReset";
NSString * const VLCLibraryModelVideoMediaListReset = @"VLCLibraryModelVideoMediaListReset";
NSString * const VLCLibraryModelRecentsMediaListReset = @"VLCLibraryModelRecentsMediaListReset";
NSString * const VLCLibraryModelRecentAudioMediaListReset = @"VLCLibraryModelRecentAudioMediaListReset";
NSString * const VLCLibraryModelListOfShowsReset = @"VLCLibraryModelListOfShowsReset";
NSString * const VLCLibraryModelListOfGroupsReset = @"VLCLibraryModelListOfGroupsReset";

NSString * const VLCLibraryModelPlaylistAdded = @"VLCLibraryModelPlaylistAdded";

NSString * const VLCLibraryModelAudioMediaItemDeleted = @"VLCLibraryModelAudioMediaItemDeleted";
NSString * const VLCLibraryModelVideoMediaItemDeleted = @"VLCLibraryModelVideoMediaItemDeleted";
NSString * const VLCLibraryModelRecentsMediaItemDeleted = @"VLCLibraryModelRecentsMediaItemDeleted";
NSString * const VLCLibraryModelRecentAudioMediaItemDeleted = @"VLCLibraryModelRecentAudioMediaItemDeleted";
NSString * const VLCLibraryModelAlbumDeleted = @"VLCLibraryModelAlbumDeleted";
NSString * const VLCLibraryModelArtistDeleted = @"VLCLibraryModelArtistDeleted";
NSString * const VLCLibraryModelGenreDeleted = @"VLCLibraryModelGenreDeleted";
NSString * const VLCLibraryModelGroupDeleted = @"VLCLibraryModelGroupDeleted";
NSString * const VLCLibraryModelPlaylistDeleted = @"VLCLibraryModelPlaylistDeleted";

NSString * const VLCLibraryModelAudioMediaItemUpdated = @"VLCLibraryModelAudioMediaItemUpdated";
NSString * const VLCLibraryModelVideoMediaItemUpdated = @"VLCLibraryModelVideoMediaItemUpdated";
NSString * const VLCLibraryModelRecentsMediaItemUpdated = @"VLCLibraryModelRecentsMediaItemUpdated";
NSString * const VLCLibraryModelRecentAudioMediaItemUpdated = @"VLCLibraryModelRecentAudioMediaItemUpdated";
NSString * const VLCLibraryModelAlbumUpdated = @"VLCLibraryModelAlbumUpdated";
NSString * const VLCLibraryModelArtistUpdated = @"VLCLibraryModelArtistUpdated";
NSString * const VLCLibraryModelGenreUpdated = @"VLCLibraryModelGenreUpdated";
NSString * const VLCLibraryModelGroupUpdated = @"VLCLibraryModelGroupUpdated";
NSString * const VLCLibraryModelPlaylistUpdated = @"VLCLibraryModelPlaylistUpdated";

NSString * const VLCLibraryModelDiscoveryStarted = @"VLCLibraryModelDiscoveryStarted";
NSString * const VLCLibraryModelDiscoveryProgress = @"VLCLibraryModelDiscoveryProgress";
NSString * const VLCLibraryModelDiscoveryCompleted = @"VLCLibraryModelDiscoveryCompleted";
NSString * const VLCLibraryModelDiscoveryFailed = @"VLCLibraryModelDiscoveryFailed";

@interface VLCLibraryModel ()
{
    vlc_medialibrary_t *_p_mediaLibrary;
    vlc_ml_event_callback_t *_p_eventCallback;

    NSNotificationCenter *_defaultNotificationCenter;

    enum vlc_ml_sorting_criteria_t _sortCriteria;
    bool _sortDescending;

    size_t _initialVideoCount;
    size_t _initialAudioCount;
    size_t _initialAlbumCount;
    size_t _initialArtistCount;
    size_t _initialGenreCount;
    size_t _initialShowCount;
    size_t _initialGroupCount;
    size_t _initialRecentsCount;
    size_t _initialRecentAudioCount;

    dispatch_queue_t _mediaItemCacheModificationQueue;
    dispatch_queue_t _albumCacheModificationQueue;
    dispatch_queue_t _artistCacheModificationQueue;
    dispatch_queue_t _genreCacheModificationQueue;
    dispatch_queue_t _groupCacheModificationQueue;
}

@property (readwrite) NSArray<VLCMediaLibraryFolderObserver *> *folderObservers;

@property (readwrite, atomic) NSArray *cachedAudioMedia;
@property (readwrite, atomic) NSArray *cachedArtists;
@property (readwrite, atomic) NSArray *cachedAlbums;
@property (readwrite, atomic) NSArray *cachedGenres;
@property (readwrite, atomic) NSArray *cachedVideoMedia;
@property (readwrite, atomic) NSArray *cachedListOfShows;
@property (readwrite, atomic) NSArray *cachedListOfGroups;
@property (readwrite, atomic) NSArray *cachedRecentMedia;
@property (readwrite, atomic) NSArray *cachedRecentAudioMedia;
@property (readwrite, atomic) NSArray *cachedListOfMonitoredFolders;

- (void)resetCachedListOfRecentMedia;
- (void)resetCachedListOfRecentAudioMedia;
- (void)resetCachedListOfArtists;
- (void)resetCachedListOfAlbums;
- (void)resetCachedListOfGenres;
- (void)resetCachedListOfShows;
- (void)resetCachedListOfGroups;
- (void)resetCachedListOfMonitoredFolders;
- (void)mediaItemThumbnailGenerated:(VLCMediaLibraryMediaItem *)mediaItem;
- (void)handleMediaItemAddedEvent:(const vlc_ml_event_t * const)p_event;
- (void)handlePlaylistAddedEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleMediaItemDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleAlbumDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleArtistDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleGenreDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleGroupDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handlePlaylistDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleMediaItemUpdateEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleAlbumUpdateEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleArtistUpdateEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleGenreUpdateEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleGroupUpdateEvent:(const vlc_ml_event_t * const)p_event;
- (void)handlePlaylistUpdateEvent:(const vlc_ml_event_t * const)p_event;

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
            [libraryModel handleMediaItemAddedEvent:p_event];
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
        case VLC_ML_EVENT_GROUP_ADDED:
            [libraryModel resetCachedListOfGroups];
            break;
        case VLC_ML_EVENT_GROUP_UPDATED:
            [libraryModel handleGroupUpdateEvent:p_event];
            break;
        case VLC_ML_EVENT_GROUP_DELETED:
            [libraryModel handleGroupDeletionEvent:p_event];
            break;
        case VLC_ML_EVENT_PLAYLIST_ADDED:
            [libraryModel handlePlaylistAddedEvent:p_event];
            break;
        case VLC_ML_EVENT_PLAYLIST_UPDATED:
            [libraryModel handlePlaylistUpdateEvent:p_event];
            break;
        case VLC_ML_EVENT_PLAYLIST_DELETED:
            [libraryModel handlePlaylistDeletionEvent:p_event];
            break;
        case VLC_ML_EVENT_FOLDER_ADDED:
        case VLC_ML_EVENT_FOLDER_UPDATED:
        case VLC_ML_EVENT_FOLDER_DELETED:
            [libraryModel resetCachedListOfMonitoredFolders];
            break;
        case VLC_ML_EVENT_HISTORY_CHANGED:
            [libraryModel resetCachedListOfRecentMedia];
            [libraryModel resetCachedListOfRecentAudioMedia];
            break;
        case VLC_ML_EVENT_DISCOVERY_STARTED:
            dispatch_async(dispatch_get_main_queue(), ^{
                NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
                [defaultCenter postNotificationName:VLCLibraryModelDiscoveryStarted object:nil];
            });
            break;
        case VLC_ML_EVENT_DISCOVERY_PROGRESS:
            dispatch_async(dispatch_get_main_queue(), ^{
                NSString * const entryPoint = toNSStr(p_event->discovery_progress.psz_entry_point);
                NSDictionary<NSString *, NSString *> * const info = entryPoint == nil
                    ? nil
                    : @{@"entryPoint": entryPoint};
                NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
                [defaultCenter postNotificationName:VLCLibraryModelDiscoveryProgress
                                             object:nil
                                           userInfo:info];
            });
            break;
        case VLC_ML_EVENT_DISCOVERY_COMPLETED:
            dispatch_async(dispatch_get_main_queue(), ^{
                NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
                [defaultCenter postNotificationName:VLCLibraryModelDiscoveryCompleted object:nil];
            });
            break;
        case VLC_ML_EVENT_DISCOVERY_FAILED:
            dispatch_async(dispatch_get_main_queue(), ^{
                NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
                [defaultCenter postNotificationName:VLCLibraryModelDiscoveryFailed object:nil];
            });
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
        _changeDelegate = [[VLCLibraryModelChangeDelegate alloc] initWithLibraryModel:self];
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
        _groupCacheModificationQueue = dispatch_queue_create("groupCacheModificationQueue", 0);

        _defaultNotificationCenter = NSNotificationCenter.defaultCenter;
        [_defaultNotificationCenter addObserver:self
                                       selector:@selector(applicationWillTerminate:)
                                           name:NSApplicationWillTerminateNotification
                                         object:nil];

        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            vlc_ml_query_params_t queryParameters = vlc_ml_query_params_create();

            // Preload video and audio count for gui
            self->_initialVideoCount = vlc_ml_count_video_media(self->_p_mediaLibrary, &queryParameters);
            self->_initialAudioCount = vlc_ml_count_audio_media(self->_p_mediaLibrary, &queryParameters);
            self->_initialAlbumCount = vlc_ml_count_albums(self->_p_mediaLibrary, &queryParameters);
            self->_initialArtistCount = vlc_ml_count_artists(self->_p_mediaLibrary, &queryParameters, true);
            self->_initialGenreCount = vlc_ml_count_genres(self->_p_mediaLibrary, &queryParameters);
            self->_initialShowCount = vlc_ml_count_shows(self->_p_mediaLibrary, &queryParameters);
            self->_initialGroupCount = vlc_ml_count_groups(self->_p_mediaLibrary, &queryParameters);

            queryParameters.i_nbResults = self->_recentMediaLimit;
            self->_initialRecentsCount = vlc_ml_count_video_history(self->_p_mediaLibrary, &queryParameters);
            self->_initialRecentAudioCount = vlc_ml_count_audio_history(self->_p_mediaLibrary, &queryParameters);
        });

        [self resetCachedListOfMonitoredFolders];
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
    [self.changeDelegate notifyChange:VLCLibraryModelMediaItemThumbnailGenerated withObject:mediaItem];
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
            [self.changeDelegate notifyChange:VLCLibraryModelAudioMediaListReset withObject:self];
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
            [self.changeDelegate notifyChange:VLCLibraryModelArtistListReset withObject:self];
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
            [self.changeDelegate notifyChange:VLCLibraryModelAlbumListReset withObject:self];
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
            [self.changeDelegate notifyChange:VLCLibraryModelGenreListReset withObject:self];
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
            [self.changeDelegate notifyChange:VLCLibraryModelVideoMediaListReset withObject:self];
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
        vlc_ml_media_list_t *p_media_list = NULL;
        if (type == VLC_ML_MEDIA_TYPE_VIDEO)
            p_media_list = vlc_ml_list_video_history(self->_p_mediaLibrary, &queryParameters);
        else if (type == VLC_ML_MEDIA_TYPE_AUDIO)
            p_media_list = vlc_ml_list_audio_history(self->_p_mediaLibrary, &queryParameters);

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
        [self.changeDelegate notifyChange:VLCLibraryModelRecentsMediaListReset withObject:self];
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
        [self.changeDelegate notifyChange:VLCLibraryModelRecentAudioMediaListReset withObject:self];
    }];
}

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

- (void)resetCachedListOfShows
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_show_list_t * const p_show_list = vlc_ml_list_shows(self->_p_mediaLibrary, NULL);
        if (p_show_list == NULL) {
            return;
        }
        const size_t itemCount = p_show_list->i_nb_items;
        NSMutableArray * const mutableArray = [[NSMutableArray alloc] initWithCapacity:itemCount];
        for (size_t x = 0; x < p_show_list->i_nb_items; x++) {
            vlc_ml_show_t * const p_vlc_show = &p_show_list->p_items[x];
            VLCMediaLibraryShow * const show = [[VLCMediaLibraryShow alloc] initWithShow:p_vlc_show];
            if (show) {
                [mutableArray addObject:show];
            }
        }
        vlc_ml_show_list_release(p_show_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedListOfShows = mutableArray.copy;
            [self.changeDelegate notifyChange:VLCLibraryModelListOfShowsReset withObject:self];
        });
    });
}

- (size_t)numberOfShows
{
    if (!_cachedListOfShows) {
        [self resetCachedListOfShows];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialShowCount;
    }
    return _cachedListOfShows.count;
}

- (NSArray<VLCMediaLibraryShow *> *)listOfShows
{
    if (!_cachedListOfShows) {
        [self resetCachedListOfShows];
    }
    return _cachedListOfShows;
}

- (size_t)numberOfGroups
{
    if (!_cachedListOfGroups) {
        [self resetCachedListOfGroups];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialGroupCount;
    }
    return _cachedListOfGroups.count;
}

- (NSArray<VLCMediaLibraryGroup *> *)listOfGroups
{
    if (!_cachedListOfGroups) {
        [self resetCachedListOfGroups];
    }
    return _cachedListOfGroups;
}

- (void)resetCachedListOfGroups
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_group_list_t * const p_group_list = vlc_ml_list_groups(self->_p_mediaLibrary, NULL);
        if (p_group_list == NULL) {
            return;
        }
        const size_t itemCount = p_group_list->i_nb_items;
        NSMutableArray * const mutableArray = [[NSMutableArray alloc] initWithCapacity:itemCount];
        for (size_t x = 0; x < p_group_list->i_nb_items; x++) {
            vlc_ml_group_t * const p_vlc_group = &p_group_list->p_items[x];
            VLCMediaLibraryGroup * const group = [[VLCMediaLibraryGroup alloc] initWithGroup:p_vlc_group];
            if (group) {
                [mutableArray addObject:group];
            }
        }
        vlc_ml_group_list_release(p_group_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedListOfGroups = mutableArray.copy;
            [self.changeDelegate notifyChange:VLCLibraryModelListOfGroupsReset withObject:self];
        });
    });
}

- (void)handleMediaItemAddedEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event);
    const vlc_ml_media_t * const p_media = p_event->creation.p_media;
    NSParameterAssert(p_media);

    if (p_media->i_type == VLC_ML_MEDIA_TYPE_AUDIO || p_media->i_type == VLC_ML_MEDIA_TYPE_UNKNOWN) {
        [self resetCachedListOfAudioMedia];

        if (p_media->i_subtype == VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE) {
            [self resetCachedListOfShows];
        }
    }

    if (p_media->i_type == VLC_ML_MEDIA_TYPE_VIDEO || p_media->i_type == VLC_ML_MEDIA_TYPE_UNKNOWN) {
        [self resetCachedListOfVideoMedia];
    }
}

- (size_t)numberOfPlaylistsOfType:(const enum vlc_ml_playlist_type_t)playlistType
{
    const vlc_ml_query_params_t queryParams = self.queryParams;
    return vlc_ml_count_playlists(_p_mediaLibrary, &queryParams, playlistType);
}

- (nullable NSArray<VLCMediaLibraryPlaylist *> *)listOfPlaylistsOfType:(const enum vlc_ml_playlist_type_t)playlistType
{
    const vlc_ml_query_params_t queryParams = self.queryParams;
    vlc_ml_playlist_list_t * const p_playlistList =
        vlc_ml_list_playlists(_p_mediaLibrary, &queryParams, playlistType);
    if (p_playlistList == NULL) {
        return nil;
    }

    NSMutableArray * const mutableArray =
        [[NSMutableArray alloc] initWithCapacity:p_playlistList->i_nb_items];
    for (size_t x = 0; x < p_playlistList->i_nb_items; x++) {
        VLCMediaLibraryPlaylist * const playlist =
            [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&p_playlistList->p_items[x]];
        if (playlist) {
            [mutableArray addObject:playlist];
        }
    }

    vlc_ml_playlist_list_release(p_playlistList);
    return mutableArray.copy;
}

- (void)resetCachedListOfMonitoredFolders
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_folder_list_t *pp_entrypoints = vlc_ml_list_entry_points(self->_p_mediaLibrary, NULL);
        if (pp_entrypoints == NULL) {
            msg_Err(getIntf(), "failed to retrieve list of monitored library folders");
            return;
        }

        NSMutableArray * const mutableArray =
            [[NSMutableArray alloc] initWithCapacity:pp_entrypoints->i_nb_items];
        NSMutableArray * const mutableObservers =
            [[NSMutableArray alloc] initWithCapacity:pp_entrypoints->i_nb_items];

        for (size_t x = 0; x < pp_entrypoints->i_nb_items; x++) {
            VLCMediaLibraryEntryPoint * const entryPoint =
                [[VLCMediaLibraryEntryPoint alloc] initWithEntryPoint:&pp_entrypoints->p_items[x]];
            if (entryPoint) {
                [mutableArray addObject:entryPoint];

                NSURL * const url = [NSURL URLWithString:entryPoint.MRL];
                VLCMediaLibraryFolderObserver * const observer =
                    [[VLCMediaLibraryFolderObserver alloc] initWithURL:url];
                [mutableObservers addObject:observer];
            }
        }

        vlc_ml_folder_list_release(pp_entrypoints);

        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedListOfMonitoredFolders = mutableArray.copy;
            self.folderObservers = mutableObservers.copy;
            [self.changeDelegate notifyChange:VLCLibraryModelListOfMonitoredFoldersUpdated withObject:self];
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

- (void)setFilterString:(NSString *)filterString
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

    [self.changeDelegate notifyChange:VLCLibraryModelVideoMediaListReset withObject:self];
    [self.changeDelegate notifyChange:VLCLibraryModelAudioMediaListReset withObject:self];
    [self.changeDelegate notifyChange:VLCLibraryModelAlbumListReset withObject:self];
    [self.changeDelegate notifyChange:VLCLibraryModelArtistListReset withObject:self];
    [self.changeDelegate notifyChange:VLCLibraryModelGenreListReset withObject:self];
    [self.changeDelegate notifyChange:VLCLibraryModelRecentsMediaListReset withObject:self];
    [self.changeDelegate notifyChange:VLCLibraryModelRecentAudioMediaListReset withObject:self];
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
                    [self.changeDelegate notifyChange:VLCLibraryModelRecentsMediaItemUpdated 
                                           withObject:mediaItem];
                    break;
                case VLC_ML_MEDIA_TYPE_AUDIO:
                    [self.changeDelegate notifyChange:VLCLibraryModelRecentAudioMediaItemUpdated 
                                           withObject:mediaItem];
                    break;
                case VLC_ML_MEDIA_TYPE_UNKNOWN:
                    NSLog(@"Unknown type of media type encountered, don't know what to do in deletion");
                    break;
            }
        }

        switch (mediaItem.mediaType) {
            case VLC_ML_MEDIA_TYPE_VIDEO:
                [self.changeDelegate notifyChange:VLCLibraryModelVideoMediaItemUpdated 
                                       withObject:mediaItem];
                break;
            case VLC_ML_MEDIA_TYPE_AUDIO:
                [self.changeDelegate notifyChange:VLCLibraryModelAudioMediaItemUpdated 
                                       withObject:mediaItem];
                break;
            case VLC_ML_MEDIA_TYPE_UNKNOWN:
                NSLog(@"Unknown type of media type encountered, don't know what to do in update");
                break;
        }

        if (mediaItem.mediaSubType == VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE) {
            [self resetCachedListOfShows];
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
                    [self.changeDelegate notifyChange:VLCLibraryModelRecentsMediaItemDeleted
                                           withObject:mediaItem];
                    break;
                case VLC_ML_MEDIA_TYPE_AUDIO:
                    [self.changeDelegate notifyChange:VLCLibraryModelRecentAudioMediaItemDeleted 
                                           withObject:mediaItem];
                    break;
                case VLC_ML_MEDIA_TYPE_UNKNOWN:
                    NSLog(@"Unknown type of media type encountered, don't know what to do in deletion");
                    break;
            }
        }

        switch (mediaItem.mediaType) {
            case VLC_ML_MEDIA_TYPE_VIDEO:
                [self.changeDelegate notifyChange:VLCLibraryModelVideoMediaItemDeleted
                                       withObject:mediaItem];
                break;
            case VLC_ML_MEDIA_TYPE_AUDIO:
                [self.changeDelegate notifyChange:VLCLibraryModelAudioMediaItemDeleted
                                       withObject:mediaItem];
                break;
            case VLC_ML_MEDIA_TYPE_UNKNOWN:
                NSLog(@"Unknown type of media type encountered, don't know what to do in deletion");
                break;
        }

        if (mediaItem.mediaSubType == VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE) {
            [self resetCachedListOfShows];
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

            [self.changeDelegate notifyChange:notificationName withObject:audioGroupItem];
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

            [self.changeDelegate notifyChange:notificationName withObject:audioGroupItem];
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

- (void)handleGroupDeletionEvent:(const vlc_ml_event_t *const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    dispatch_async(_groupCacheModificationQueue, ^{
        NSMutableArray * const mutableGroups = self.cachedListOfGroups.mutableCopy;
        const NSUInteger groupIdx =
            [mutableGroups indexOfObjectPassingTest:^BOOL(VLCMediaLibraryGroup * const group,
                                                          const NSUInteger idx,
                                                          BOOL * const stop) {
            return group.libraryID == itemId;
        }];

        if (groupIdx == NSNotFound) {
            NSLog(@"Could not handle deletion of groupI with id %lld in model", itemId);
            return;
        }

        dispatch_sync(dispatch_get_main_queue(), ^{
            VLCMediaLibraryGroup * const group = mutableGroups[groupIdx];
            [mutableGroups removeObjectAtIndex:groupIdx];
            self.cachedListOfGroups = mutableGroups.copy;
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelGroupDeleted
                                                            object:group];
        });
    });
}

- (void)handleGroupUpdateEvent:(const vlc_ml_event_t *const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;
    VLCMediaLibraryGroup * const group = [VLCMediaLibraryGroup groupWithID:itemId];
    if (group == nil) {
        NSLog(@"Could not find a library group with this ID. Can't handle update.");
        return;
    }

    dispatch_async(_groupCacheModificationQueue, ^{
        const NSUInteger groupIdx = 
            [self.cachedListOfGroups indexOfObjectPassingTest:^BOOL(VLCMediaLibraryGroup * const group,
                                                                    const NSUInteger idx,
                                                                    BOOL * const stop) {
            NSAssert(group != nil, @"Cache list should not contain nil groups");
            return group.libraryID == itemId;
        }];

        if (groupIdx == NSNotFound) {
            NSLog(@"Could not handle deletion of group with id %lld in model", itemId);
            return;
        }

        dispatch_sync(dispatch_get_main_queue(), ^{
            NSMutableArray * const mutableGroups = self.cachedListOfGroups.mutableCopy;
            [mutableGroups replaceObjectAtIndex:groupIdx withObject:group];
            self.cachedListOfGroups = mutableGroups.copy;
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelGroupUpdated object:group];
        });
    });
}

- (void)handlePlaylistAddedEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const vlc_ml_playlist_t * const p_playlist = p_event->creation.p_playlist;
    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:p_playlist];
    if (playlist == nil) {
        NSLog(@"Could not find a library playlist with this ID. Can't handle update.");
        return;
    }

    dispatch_sync(dispatch_get_main_queue(), ^{
        [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelPlaylistAdded
                                                        object:playlist];
    });
}

- (void)handlePlaylistUpdateEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;
    VLCMediaLibraryPlaylist * const playlist = [VLCMediaLibraryPlaylist playlistForLibraryID:itemId];
    if (playlist == nil) {
        NSLog(@"Could not find a library playlist with this ID. Can't handle update.");
        return;
    }

    dispatch_sync(dispatch_get_main_queue(), ^{
        [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelPlaylistUpdated
                                                        object:playlist];
    });
}

- (void)handlePlaylistDeletionEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;
    dispatch_sync(dispatch_get_main_queue(), ^{
        [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelPlaylistDeleted 
                                                        object:@(itemId)];
    });
}

@end
