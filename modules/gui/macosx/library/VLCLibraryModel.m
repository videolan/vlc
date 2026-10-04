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
#include <sys/qos.h>

#import "VLCMediaLibraryFolderObserver.h"

#import "extensions/NSArray+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "main/VLCMain.h"

NSString * const VLCLibraryModelArtistListReset = @"VLCLibraryModelArtistListReset";
NSString * const VLCLibraryModelAlbumListReset = @"VLCLibraryModelAlbumListReset";
NSString * const VLCLibraryModelGenreListReset = @"VLCLibraryModelGenreListReset";
NSString * const VLCLibraryModelListOfMonitoredFoldersUpdated = @"VLCLibraryModelListOfMonitoredFoldersUpdated";
NSString * const VLCLibraryModelMediaItemThumbnailGenerated = @"VLCLibraryModelMediaItemThumbnailGenerated";

NSString * const VLCLibraryModelAllCachesDropped = @"VLCLibraryModelAllCachesDropped";
NSString * const VLCLibraryModelAudioMediaListReset = @"VLCLibraryModelAudioMediaListReset";
NSString * const VLCLibraryModelVideoMediaListReset = @"VLCLibraryModelVideoMediaListReset";
NSString * const VLCLibraryModelFavoriteAudioMediaListReset = @"VLCLibraryModelFavoriteAudioMediaListReset";
NSString * const VLCLibraryModelFavoriteVideoMediaListReset = @"VLCLibraryModelFavoriteVideoMediaListReset";
NSString * const VLCLibraryModelFavoriteAlbumsListReset = @"VLCLibraryModelFavoriteAlbumsListReset";
NSString * const VLCLibraryModelFavoriteArtistsListReset = @"VLCLibraryModelFavoriteArtistsListReset";
NSString * const VLCLibraryModelFavoriteGenresListReset = @"VLCLibraryModelFavoriteGenresListReset";
NSString * const VLCLibraryModelRecentsMediaListReset = @"VLCLibraryModelRecentsMediaListReset";
NSString * const VLCLibraryModelRecentAudioMediaListReset = @"VLCLibraryModelRecentAudioMediaListReset";
NSString * const VLCLibraryModelListOfShowsReset = @"VLCLibraryModelListOfShowsReset";
NSString * const VLCLibraryModelListOfMoviesReset = @"VLCLibraryModelListOfMoviesReset";
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
NSString * const VLCLibraryModelShowDeleted = @"VLCLibraryModelShowDeleted";

NSString * const VLCLibraryModelAudioMediaItemUpdated = @"VLCLibraryModelAudioMediaItemUpdated";
NSString * const VLCLibraryModelVideoMediaItemUpdated = @"VLCLibraryModelVideoMediaItemUpdated";
NSString * const VLCLibraryModelRecentsMediaItemUpdated = @"VLCLibraryModelRecentsMediaItemUpdated";
NSString * const VLCLibraryModelRecentAudioMediaItemUpdated = @"VLCLibraryModelRecentAudioMediaItemUpdated";
NSString * const VLCLibraryModelAlbumUpdated = @"VLCLibraryModelAlbumUpdated";
NSString * const VLCLibraryModelArtistUpdated = @"VLCLibraryModelArtistUpdated";
NSString * const VLCLibraryModelGenreUpdated = @"VLCLibraryModelGenreUpdated";
NSString * const VLCLibraryModelGroupUpdated = @"VLCLibraryModelGroupUpdated";
NSString * const VLCLibraryModelPlaylistUpdated = @"VLCLibraryModelPlaylistUpdated";
NSString * const VLCLibraryModelShowUpdated = @"VLCLibraryModelShowUpdated";

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
    size_t _initialMovieCount;
    size_t _initialGroupCount;
    size_t _initialRecentsCount;
    size_t _initialRecentAudioCount;

    dispatch_queue_t _mediaItemCacheModificationQueue;
    dispatch_queue_t _albumCacheModificationQueue;
    dispatch_queue_t _artistCacheModificationQueue;
    dispatch_queue_t _genreCacheModificationQueue;
    dispatch_queue_t _groupCacheModificationQueue;
    dispatch_queue_t _mediaTitlesCacheModificationQueue;
    NSCountedSet<NSString *> *_mediaTitleCounts;
    NSMutableDictionary<NSValue *, NSObject *> *_cacheUpdateTokens;
    NSMutableSet<NSValue *> *_pendingCacheUpdates;
}

@property (readwrite) NSArray<VLCMediaLibraryFolderObserver *> *folderObservers;
@property (atomic) NSUInteger cacheGeneration;

@property (readwrite, nonatomic) NSArray *cachedAudioMedia;
@property (readwrite, nonatomic) NSArray *cachedArtists;
@property (readwrite, nonatomic) NSArray *cachedAlbums;
@property (readwrite, nonatomic) NSArray *cachedGenres;
@property (readwrite, nonatomic) NSArray *cachedVideoMedia;
@property (readwrite, nonatomic) NSArray *cachedListOfShows;
@property (readwrite, nonatomic) NSArray *cachedListOfMovies;
@property (readwrite, nonatomic) NSArray *cachedListOfGroups;
@property (readwrite, nonatomic) NSArray *cachedRecentMedia;
@property (readwrite, nonatomic) NSArray *cachedRecentAudioMedia;
@property (readwrite, nonatomic) NSArray *cachedListOfMonitoredFolders;
@property (readwrite, nonatomic) NSArray *cachedMediaTitles;

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
- (void)performAfterCacheWritesOnQueue:(dispatch_queue_t)queue
                                 block:(dispatch_block_t)block;

@end

static void libraryCallback(void *p_data, const vlc_ml_event_t *p_event)
{
    VLCLibraryModel * const libraryModel = (__bridge VLCLibraryModel *)p_data;
    if (libraryModel == nil) {
        return;
    }

    @autoreleasepool {
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
            case VLC_ML_EVENT_FAVORITES_CHANGED:
            {
                dispatch_async(dispatch_get_main_queue(), ^{
                    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelFavoriteAudioMediaListReset object:libraryModel];
                    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelFavoriteVideoMediaListReset object:libraryModel];
                    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelFavoriteAlbumsListReset object:libraryModel];
                    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelFavoriteArtistsListReset object:libraryModel];
                    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelFavoriteGenresListReset object:libraryModel];
                });
                break;
            }
            case VLC_ML_EVENT_DISCOVERY_STARTED:
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
                    [defaultCenter postNotificationName:VLCLibraryModelDiscoveryStarted object:nil];
                });
                break;
            case VLC_ML_EVENT_DISCOVERY_PROGRESS:
            {
                NSString * const entryPoint = toNSStr(p_event->discovery_progress.psz_entry_point);
                dispatch_async(dispatch_get_main_queue(), ^{
                    NSDictionary<NSString *, NSString *> * const info = entryPoint == nil
                        ? nil
                        : @{@"entryPoint": entryPoint};
                    NSNotificationCenter * const defaultCenter = NSNotificationCenter.defaultCenter;
                    [defaultCenter postNotificationName:VLCLibraryModelDiscoveryProgress
                                                 object:nil
                                               userInfo:info];
                });
                break;
            }
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
}

@implementation VLCLibraryModel

@synthesize cachedAudioMedia = _cachedAudioMedia;
@synthesize cachedVideoMedia = _cachedVideoMedia;
@synthesize cachedRecentMedia = _cachedRecentMedia;
@synthesize cachedRecentAudioMedia = _cachedRecentAudioMedia;
@synthesize cachedListOfShows = _cachedListOfShows;
@synthesize cachedListOfMovies = _cachedListOfMovies;
@synthesize cachedListOfMonitoredFolders = _cachedListOfMonitoredFolders;
@synthesize cachedAlbums = _cachedAlbums;
@synthesize cachedArtists = _cachedArtists;
@synthesize cachedGenres = _cachedGenres;
@synthesize cachedListOfGroups = _cachedListOfGroups;
@synthesize cachedMediaTitles = _cachedMediaTitles;

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

        // Concurrent queues allow multiple readers while using barrier flags for exclusive writes,
        // providing better performance than serial queues while maintaining thread safety
        _mediaItemCacheModificationQueue = dispatch_queue_create("mediaItemCacheModificationQueue", DISPATCH_QUEUE_CONCURRENT);
        _albumCacheModificationQueue = dispatch_queue_create("albumCacheModificationQueue", DISPATCH_QUEUE_CONCURRENT);
        _artistCacheModificationQueue = dispatch_queue_create("artistCacheModificationQueue", DISPATCH_QUEUE_CONCURRENT);
        _genreCacheModificationQueue = dispatch_queue_create("genreCacheModificationQueue", DISPATCH_QUEUE_CONCURRENT);
        _groupCacheModificationQueue = dispatch_queue_create("groupCacheModificationQueue", DISPATCH_QUEUE_CONCURRENT);
        _mediaTitlesCacheModificationQueue = dispatch_queue_create("mediaTitlesCacheModificationQueue", DISPATCH_QUEUE_CONCURRENT);
        _mediaTitleCounts = [NSCountedSet new];
        _cacheUpdateTokens = NSMutableDictionary.dictionary;
        _pendingCacheUpdates = NSMutableSet.set;

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
            self->_initialMovieCount = vlc_ml_count_movies(self->_p_mediaLibrary, &queryParameters);
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

#pragma mark - Thread-Safe Cache Getters

- (NSArray *)readCacheOnQueue:(dispatch_queue_t)queue withBlock:(NSArray *(^)(void))block
{
    __block NSArray *cache;
    dispatch_sync(queue, ^{
        cache = block();
    });
    return cache;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)cachedAudioMedia
{
    return [self readCacheOnQueue:_mediaItemCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedAudioMedia;
    }];
}

- (NSArray<VLCMediaLibraryMediaItem *> *)cachedVideoMedia
{
    return [self readCacheOnQueue:_mediaItemCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedVideoMedia;
    }];
}

- (NSArray<VLCMediaLibraryMediaItem *> *)cachedRecentMedia
{
    return [self readCacheOnQueue:_mediaItemCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedRecentMedia;
    }];
}

- (NSArray<VLCMediaLibraryMediaItem *> *)cachedRecentAudioMedia
{
    return [self readCacheOnQueue:_mediaItemCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedRecentAudioMedia;
    }];
}

- (NSArray<VLCMediaLibraryShow *> *)cachedListOfShows
{
    return [self readCacheOnQueue:_mediaItemCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedListOfShows;
    }];
}

- (NSArray<VLCMediaLibraryMovie *> *)cachedListOfMovies
{
    return [self readCacheOnQueue:_mediaItemCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedListOfMovies;
    }];
}

- (NSArray<VLCMediaLibraryEntryPoint *> *)cachedListOfMonitoredFolders
{
    return [self readCacheOnQueue:_mediaItemCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedListOfMonitoredFolders;
    }];
}

- (NSArray<VLCMediaLibraryAlbum *> *)cachedAlbums
{
    return [self readCacheOnQueue:_albumCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedAlbums;
    }];
}

- (NSArray<VLCMediaLibraryArtist *> *)cachedArtists
{
    return [self readCacheOnQueue:_artistCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedArtists;
    }];
}

- (NSArray<VLCMediaLibraryGenre *> *)cachedGenres
{
    return [self readCacheOnQueue:_genreCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedGenres;
    }];
}

- (NSArray<VLCMediaLibraryGroup *> *)cachedListOfGroups
{
    return [self readCacheOnQueue:_groupCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedListOfGroups;
    }];
}

- (NSArray<NSString *> *)cachedMediaTitles
{
    return [self readCacheOnQueue:_mediaTitlesCacheModificationQueue withBlock:^NSArray *{
        return self->_cachedMediaTitles;
    }];
}

#pragma mark - Custom Thread-Safe Cache Setters

- (void)setCachedAudioMedia:(NSArray *)cachedAudioMedia
{
    [self invalidateCacheUpdateForGetter:@selector(cachedAudioMedia)];
    NSArray * const media = [cachedAudioMedia copy];
    dispatch_barrier_async(_mediaItemCacheModificationQueue, ^{
        [self replaceMediaTitlesFromMedia:self->_cachedAudioMedia withMedia:media];
        self->_cachedAudioMedia = media;
    });
}

- (void)setCachedVideoMedia:(NSArray *)cachedVideoMedia
{
    [self invalidateCacheUpdateForGetter:@selector(cachedVideoMedia)];
    NSArray * const media = [cachedVideoMedia copy];
    dispatch_barrier_async(_mediaItemCacheModificationQueue, ^{
        [self replaceMediaTitlesFromMedia:self->_cachedVideoMedia withMedia:media];
        self->_cachedVideoMedia = media;
    });
}

- (void)setCachedRecentMedia:(NSArray *)cachedRecentMedia
{
    [self invalidateCacheUpdateForGetter:@selector(cachedRecentMedia)];
    dispatch_barrier_async(_mediaItemCacheModificationQueue, ^{
        self->_cachedRecentMedia = [cachedRecentMedia copy];
    });
}

- (void)setCachedRecentAudioMedia:(NSArray *)cachedRecentAudioMedia
{
    [self invalidateCacheUpdateForGetter:@selector(cachedRecentAudioMedia)];
    dispatch_barrier_async(_mediaItemCacheModificationQueue, ^{
        self->_cachedRecentAudioMedia = [cachedRecentAudioMedia copy];
    });
}

- (void)setCachedListOfShows:(NSArray *)cachedListOfShows
{
    [self invalidateCacheUpdateForGetter:@selector(cachedListOfShows)];
    dispatch_barrier_async(_mediaItemCacheModificationQueue, ^{
        self->_cachedListOfShows = [cachedListOfShows copy];
    });
}

- (void)setCachedListOfMovies:(NSArray *)cachedListOfMovies
{
    [self invalidateCacheUpdateForGetter:@selector(cachedListOfMovies)];
    dispatch_barrier_async(_mediaItemCacheModificationQueue, ^{
        self->_cachedListOfMovies = [cachedListOfMovies copy];
    });
}

- (void)setCachedListOfMonitoredFolders:(NSArray *)cachedListOfMonitoredFolders
{
    [self invalidateCacheUpdateForGetter:@selector(cachedListOfMonitoredFolders)];
    dispatch_barrier_async(_mediaItemCacheModificationQueue, ^{
        self->_cachedListOfMonitoredFolders = [cachedListOfMonitoredFolders copy];
    });
}

- (void)setCachedAlbums:(NSArray *)cachedAlbums
{
    [self invalidateCacheUpdateForGetter:@selector(cachedAlbums)];
    dispatch_barrier_async(_albumCacheModificationQueue, ^{
        self->_cachedAlbums = [cachedAlbums copy];
    });
}

- (void)setCachedArtists:(NSArray *)cachedArtists
{
    [self invalidateCacheUpdateForGetter:@selector(cachedArtists)];
    dispatch_barrier_async(_artistCacheModificationQueue, ^{
        self->_cachedArtists = [cachedArtists copy];
    });
}

- (void)setCachedGenres:(NSArray *)cachedGenres
{
    [self invalidateCacheUpdateForGetter:@selector(cachedGenres)];
    dispatch_barrier_async(_genreCacheModificationQueue, ^{
        self->_cachedGenres = [cachedGenres copy];
    });
}

- (void)setCachedListOfGroups:(NSArray *)cachedListOfGroups
{
    [self invalidateCacheUpdateForGetter:@selector(cachedListOfGroups)];
    dispatch_barrier_async(_groupCacheModificationQueue, ^{
        self->_cachedListOfGroups = [cachedListOfGroups copy];
    });
}

- (void)setCachedMediaTitles:(NSArray *)cachedMediaTitles
{
    [self invalidateCacheUpdateForGetter:@selector(cachedMediaTitles)];
    dispatch_barrier_async(_mediaTitlesCacheModificationQueue, ^{
        self->_cachedMediaTitles = [cachedMediaTitles copy];
    });
}

- (size_t)numberOfAudioMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const cache = self.cachedAudioMedia;
    if (cache == nil) {
        [self resetCachedListOfAudioMedia];

        // Return initial count here, otherwise it will return 0 on the first time
        return _initialAudioCount;
    }
    return cache.count;
}

- (vlc_ml_query_params_t)queryParams
{
    const vlc_ml_query_params_t queryParams = { .psz_pattern = self->_filterString.length > 0 ? [self->_filterString UTF8String] : NULL, 
                                                .i_sort = self->_sortCriteria, 
                                                .b_desc = self->_sortDescending };
    return queryParams;
}

- (void)performAfterCacheWritesOnQueue:(dispatch_queue_t)queue
                                 block:(dispatch_block_t)block
{
    const NSUInteger generation = self.cacheGeneration;
    // The sentinel runs after all cache writes already queued on this queue.
    dispatch_barrier_async(queue, ^{
        dispatch_async(dispatch_get_main_queue(), ^{
            if (generation == self.cacheGeneration) {
                block();
            }
        });
    });
}

- (NSObject *)beginCacheUpdateForGetter:(SEL)getter
{
    NSObject * const token = NSObject.new;
    @synchronized (_cacheUpdateTokens) {
        _cacheUpdateTokens[[NSValue valueWithPointer:getter]] = token;
        [_pendingCacheUpdates addObject:[NSValue valueWithPointer:getter]];
    }
    return token;
}

- (BOOL)isCurrentCacheUpdate:(NSObject *)token forGetter:(SEL)getter
{
    @synchronized (_cacheUpdateTokens) {
        return _cacheUpdateTokens[[NSValue valueWithPointer:getter]] == token;
    }
}

- (BOOL)invalidateCacheUpdateForGetter:(SEL)getter
{
    @synchronized (_cacheUpdateTokens) {
        NSValue * const key = [NSValue valueWithPointer:getter];
        const BOOL pending = [_pendingCacheUpdates containsObject:key];
        [_cacheUpdateTokens removeObjectForKey:key];
        [_pendingCacheUpdates removeObject:key];
        return pending;
    }
}

- (void)performCacheMutationForGetters:(const SEL *)getters
                                count:(NSUInteger)count
                                block:(dispatch_block_t)block
{
    @synchronized (_cacheUpdateTokens) {
        BOOL pending[count];
        for (NSUInteger index = 0; index < count; index++) {
            pending[index] = [self invalidateCacheUpdateForGetter:getters[index]];
        }
        block();

        // Incremental changes cannot replace a pending membership/order query.
        // Reconcile again unless the mutation already started a newer query.
        for (NSUInteger index = 0; index < count; index++) {
            const SEL getter = getters[index];
            if (!pending[index] || [_pendingCacheUpdates containsObject:[NSValue valueWithPointer:getter]]) {
                continue;
            }
            if (getter == @selector(cachedAudioMedia))
                [self resetCachedListOfAudioMedia];
            else if (getter == @selector(cachedVideoMedia))
                [self resetCachedListOfVideoMedia];
            else if (getter == @selector(cachedRecentAudioMedia))
                [self resetCachedListOfRecentAudioMedia];
            else if (getter == @selector(cachedRecentMedia))
                [self resetCachedListOfRecentMedia];
            else if (getter == @selector(cachedArtists))
                [self resetCachedListOfArtists];
            else if (getter == @selector(cachedAlbums))
                [self resetCachedListOfAlbums];
            else if (getter == @selector(cachedGenres))
                [self resetCachedListOfGenres];
            else if (getter == @selector(cachedListOfGroups))
                [self resetCachedListOfGroups];
            else if (getter == @selector(cachedListOfShows))
                [self resetCachedListOfShows];
        }
    }
}

- (void)performCacheMutationForGetter:(SEL)getter block:(dispatch_block_t)block
{
    SEL const getters[] = { getter };
    [self performCacheMutationForGetters:getters count:1 block:block];
}

- (void)performCacheWriteForGeneration:(NSUInteger)generation
                                 token:(NSObject *)token
                                getter:(SEL)getter
                               onQueue:(dispatch_queue_t)queue
                                 block:(dispatch_block_t)block
{
    dispatch_barrier_async(queue, ^{
        // Validate at the write, not when the background query finishes.
        @synchronized (self->_cacheUpdateTokens) {
            if (generation == self.cacheGeneration && [self isCurrentCacheUpdate:token forGetter:getter]) {
                [self->_pendingCacheUpdates removeObject:[NSValue valueWithPointer:getter]];
                block();
            }
        }
    });
}

- (void)resetCachedListOfAudioMedia
{
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedAudioMedia)];
    NSString * const filterString = [self.filterString copy];
    const vlc_ml_query_params_t capturedParams = [self queryParams];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_query_params_t queryParams = capturedParams;
        queryParams.psz_pattern = filterString.length > 0 ? filterString.UTF8String : NULL;
        vlc_ml_media_list_t * const p_media_list = vlc_ml_list_audio_media(self->_p_mediaLibrary, &queryParams);
        NSArray * const mediaArray = [NSArray arrayFromVlcMediaList:p_media_list];
        if (mediaArray == nil) {
            return;
        }
        vlc_ml_media_list_release(p_media_list);
        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedAudioMedia)
                                     onQueue:self->_mediaItemCacheModificationQueue
                                       block:^{
            [self replaceMediaTitlesFromMedia:self->_cachedAudioMedia withMedia:mediaArray];
            self->_cachedAudioMedia = mediaArray;
            [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
                [self.changeDelegate notifyChange:VLCLibraryModelAudioMediaListReset withObject:self];
            }];
        }];
    });
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfAudioMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const cache = self.cachedAudioMedia;
    if (cache == nil) {
        [self resetCachedListOfAudioMedia];
    }
    return cache;
}

- (size_t)numberOfArtists
{
    NSArray<VLCMediaLibraryArtist *> * const cache = self.cachedArtists;
    if (cache == nil) {
        [self resetCachedListOfArtists];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialArtistCount;
    }
    return cache.count;
}

- (void)resetCachedListOfArtists
{
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedArtists)];
    NSString * const filterString = [self.filterString copy];
    const vlc_ml_query_params_t capturedParams = [self queryParams];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_query_params_t queryParams = capturedParams;
        queryParams.psz_pattern = filterString.length > 0 ? filterString.UTF8String : NULL;
        vlc_ml_artist_list_t * const p_artist_list = vlc_ml_list_artists(self->_p_mediaLibrary, &queryParams, YES);
        const size_t numberOfArtists = p_artist_list->i_nb_items;
        NSMutableArray * const mutableArtistArray = [[NSMutableArray alloc] initWithCapacity:numberOfArtists];

        for (size_t x = 0; x < numberOfArtists; x++) {
            VLCMediaLibraryArtist * const artist = [[VLCMediaLibraryArtist alloc] initWithArtist:&p_artist_list->p_items[x]];

            if (artist != nil) {
                [mutableArtistArray addObject:artist];
            }
        }

        vlc_ml_artist_list_release(p_artist_list);

        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedArtists)
                                     onQueue:self->_artistCacheModificationQueue
                                       block:^{
            self->_cachedArtists = mutableArtistArray.copy;
            [self performAfterCacheWritesOnQueue:self->_artistCacheModificationQueue block:^{
                if (![self isCurrentCacheUpdate:token forGetter:@selector(cachedArtists)]) {
                    return;
                }
                [self.changeDelegate notifyChange:VLCLibraryModelArtistListReset withObject:self];
            }];
        }];
    });
}

- (NSArray<VLCMediaLibraryArtist *> *)listOfArtists
{
    NSArray<VLCMediaLibraryArtist *> * const cache = self.cachedArtists;
    if (cache == nil) {
        [self resetCachedListOfArtists];
    }
    return cache;
}

- (size_t)numberOfAlbums
{
    NSArray<VLCMediaLibraryAlbum *> * const cache = self.cachedAlbums;
    if (cache == nil) {
        [self resetCachedListOfAlbums];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialAlbumCount;
    }
    return cache.count;
}

- (void)resetCachedListOfAlbums
{
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedAlbums)];
    NSString * const filterString = [self.filterString copy];
    const vlc_ml_query_params_t capturedParams = [self queryParams];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_query_params_t queryParams = capturedParams;
        queryParams.psz_pattern = filterString.length > 0 ? filterString.UTF8String : NULL;
        vlc_ml_album_list_t * const p_album_list = vlc_ml_list_albums(self->_p_mediaLibrary, &queryParams);
        const size_t numberOfAlbums = p_album_list->i_nb_items;
        NSMutableArray * const mutableAlbumArray = [[NSMutableArray alloc] initWithCapacity:numberOfAlbums];

        for (size_t x = 0; x < numberOfAlbums; x++) {
            VLCMediaLibraryAlbum * const album = [[VLCMediaLibraryAlbum alloc] initWithAlbum:&p_album_list->p_items[x]];
            [mutableAlbumArray addObject:album];
        }

        vlc_ml_album_list_release(p_album_list);

        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedAlbums)
                                     onQueue:self->_albumCacheModificationQueue
                                       block:^{
            self->_cachedAlbums = mutableAlbumArray.copy;
            [self performAfterCacheWritesOnQueue:self->_albumCacheModificationQueue block:^{
                if (![self isCurrentCacheUpdate:token forGetter:@selector(cachedAlbums)]) {
                    return;
                }
                [self.changeDelegate notifyChange:VLCLibraryModelAlbumListReset withObject:self];
            }];
        }];
    });
}

- (NSArray<VLCMediaLibraryAlbum *> *)listOfAlbums
{
    NSArray<VLCMediaLibraryAlbum *> * const cache = self.cachedAlbums;
    if (cache == nil) {
        [self resetCachedListOfAlbums];
    }
    return cache;
}

- (size_t)numberOfGenres
{
    NSArray<VLCMediaLibraryGenre *> * const cache = self.cachedGenres;
    if (cache == nil) {
        [self resetCachedListOfGenres];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialGenreCount;
    }
    return cache.count;
}

- (void)resetCachedListOfGenres
{
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedGenres)];
    NSString * const filterString = [self.filterString copy];
    const vlc_ml_query_params_t capturedParams = [self queryParams];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_query_params_t queryParams = capturedParams;
        queryParams.psz_pattern = filterString.length > 0 ? filterString.UTF8String : NULL;
        vlc_ml_genre_list_t * const p_genre_list = vlc_ml_list_genres(self->_p_mediaLibrary, &queryParams);
        const size_t numberOfGenres = p_genre_list->i_nb_items;
        NSMutableArray * const mutableGenreArray = [[NSMutableArray alloc] initWithCapacity:numberOfGenres];

        for (size_t x = 0; x < numberOfGenres; x++) {
            VLCMediaLibraryGenre * const genre = [[VLCMediaLibraryGenre alloc] initWithGenre:&p_genre_list->p_items[x]];
            [mutableGenreArray addObject:genre];
        }

        vlc_ml_genre_list_release(p_genre_list);

        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedGenres)
                                     onQueue:self->_genreCacheModificationQueue
                                       block:^{
            self->_cachedGenres = mutableGenreArray.copy;
            [self performAfterCacheWritesOnQueue:self->_genreCacheModificationQueue block:^{
                if (![self isCurrentCacheUpdate:token forGetter:@selector(cachedGenres)]) {
                    return;
                }
                [self.changeDelegate notifyChange:VLCLibraryModelGenreListReset withObject:self];
            }];
        }];
    });
}

- (NSArray<VLCMediaLibraryGenre *> *)listOfGenres
{
    NSArray<VLCMediaLibraryGenre *> * const cache = self.cachedGenres;
    if (cache == nil) {
        [self resetCachedListOfGenres];
    }
    return cache;
}

- (size_t)numberOfVideoMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const cache = self.cachedVideoMedia;
    if (cache == nil) {
        [self resetCachedListOfVideoMedia];

        // Return initial count here, otherwise it will return 0 on the first time
        return _initialVideoCount;
    }
    return cache.count;
}

- (void)resetCachedListOfVideoMedia
{
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedVideoMedia)];
    NSString * const filterString = [self.filterString copy];
    const vlc_ml_query_params_t capturedParams = [self queryParams];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_query_params_t queryParameters = capturedParams;
        queryParameters.psz_pattern = filterString.length > 0 ? filterString.UTF8String : NULL;
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
        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedVideoMedia)
                                     onQueue:self->_mediaItemCacheModificationQueue
                                       block:^{
            NSArray<VLCMediaLibraryMediaItem *> * const media = mutableArray.copy;
            [self replaceMediaTitlesFromMedia:self->_cachedVideoMedia withMedia:media];
            self->_cachedVideoMedia = media;
            [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
                [self.changeDelegate notifyChange:VLCLibraryModelVideoMediaListReset withObject:self];
            }];
        }];
    });
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfVideoMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const cache = self.cachedVideoMedia;
    if (cache == nil) {
        [self resetCachedListOfVideoMedia];
    }
    return cache;
}

- (void)replaceMediaTitlesFromMedia:(NSArray<VLCMediaLibraryMediaItem *> *)oldMedia
                          withMedia:(NSArray<VLCMediaLibraryMediaItem *> *)newMedia
{
    dispatch_barrier_async(_mediaTitlesCacheModificationQueue, ^{
        for (VLCMediaLibraryMediaItem * const item in oldMedia) {
            NSString * const title = item.displayString;
            if (title) {
                [self->_mediaTitleCounts removeObject:title];
            }
        }
        for (VLCMediaLibraryMediaItem * const item in newMedia) {
            NSString * const title = item.displayString;
            if (title) {
                [self->_mediaTitleCounts addObject:title];
            }
        }
        if (newMedia == nil && self->_mediaTitleCounts.count == 0) {
            self->_cachedMediaTitles = nil;
        } else if (newMedia != nil || self->_cachedMediaTitles != nil) {
            self->_cachedMediaTitles = [self->_mediaTitleCounts.allObjects
                sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
        }
    });
}

- (void)replaceMediaTitle:(NSString *)oldTitle withTitle:(NSString *)newTitle
{
    if (oldTitle == newTitle || [oldTitle isEqualToString:newTitle]) {
        return;
    }

    dispatch_barrier_async(_mediaTitlesCacheModificationQueue, ^{
        BOOL removeTitle = NO;
        BOOL insertTitle = NO;
        if (oldTitle && [self->_mediaTitleCounts countForObject:oldTitle] > 0) {
            [self->_mediaTitleCounts removeObject:oldTitle];
            removeTitle = [self->_mediaTitleCounts countForObject:oldTitle] == 0;
        }
        if (newTitle) {
            insertTitle = [self->_mediaTitleCounts countForObject:newTitle] == 0;
            [self->_mediaTitleCounts addObject:newTitle];
        }
        if (self->_cachedMediaTitles == nil || (!removeTitle && !insertTitle)) {
            return;
        }

        NSMutableArray<NSString *> * const titles = self->_cachedMediaTitles.mutableCopy;
        if (removeTitle) {
            [titles removeObject:oldTitle];
        }
        if (insertTitle) {
            const NSUInteger index =
                [titles indexOfObject:newTitle
                        inSortedRange:NSMakeRange(0, titles.count)
                              options:NSBinarySearchingInsertionIndex
                      usingComparator:^NSComparisonResult(NSString *left, NSString *right) {
                return [left localizedCaseInsensitiveCompare:right];
            }];
            [titles insertObject:newTitle atIndex:index];
        }
        self->_cachedMediaTitles = titles.copy;
    });
}

- (NSArray<NSString *> *)listOfMediaTitles
{
    // Source setters maintain the counts; only a title lookup loads missing sources.
    if (self.cachedAudioMedia == nil) {
        [self resetCachedListOfAudioMedia];
    }
    if (self.cachedVideoMedia == nil) {
        [self resetCachedListOfVideoMedia];
    }
    return self.cachedMediaTitles;
}

- (void)getListOfRecentMediaOfType:(vlc_ml_media_type_t)type
                    withCountLimit:(size_t)countLimit
                    withCompletion:(void (^)(NSArray *recentMediaArray))completionHandler
{
    NSString * const filterString = [self.filterString copy];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_query_params_t queryParameters = vlc_ml_query_params_create();
        queryParameters.i_nbResults = countLimit;
        queryParameters.psz_pattern = filterString.length > 0 ? filterString.UTF8String : NULL;
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

- (size_t)countOfRecentVideoMediaWithCountLimit:(size_t)countLimit
{
    vlc_ml_query_params_t queryParameters = vlc_ml_query_params_create();
    queryParameters.i_nbResults = countLimit;
    queryParameters.psz_pattern = self->_filterString.length > 0 ? self->_filterString.UTF8String : NULL;
    return vlc_ml_count_video_history(self->_p_mediaLibrary, &queryParameters);
}

- (void)resetCachedListOfRecentMedia
{
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedRecentMedia)];
    [self getListOfRecentMediaOfType:VLC_ML_MEDIA_TYPE_VIDEO
                      withCountLimit:_recentMediaLimit
                      withCompletion:^(NSArray * const mediaArray) {
        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedRecentMedia)
                                     onQueue:self->_mediaItemCacheModificationQueue
                                       block:^{
            self->_cachedRecentMedia = mediaArray;
            [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
                [self.changeDelegate notifyChange:VLCLibraryModelRecentsMediaListReset withObject:self];
            }];
        }];
    }];
}

- (size_t)numberOfRecentMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const cache = self.cachedRecentMedia;
    if (cache == nil) {
        [self resetCachedListOfRecentMedia];
        // Return the filtered count immediately during search, otherwise keep the startup fast path.
        if (_filterString.length > 0) {
            return [self countOfRecentVideoMediaWithCountLimit:_recentMediaLimit];
        }
        return _initialRecentsCount;
    }
    return cache.count;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const cache = self.cachedRecentMedia;
    if (cache == nil) {
        [self resetCachedListOfRecentMedia];
    }
    return cache;
}

- (void)resetCachedListOfRecentAudioMedia
{
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedRecentAudioMedia)];
    [self getListOfRecentMediaOfType:VLC_ML_MEDIA_TYPE_AUDIO
                      withCountLimit:_recentAudioMediaLimit
                      withCompletion:^(NSArray * const mediaArray) {
        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedRecentAudioMedia)
                                     onQueue:self->_mediaItemCacheModificationQueue
                                       block:^{
            self->_cachedRecentAudioMedia = mediaArray;
            [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
                [self.changeDelegate notifyChange:VLCLibraryModelRecentAudioMediaListReset withObject:self];
            }];
        }];
    }];
}

- (size_t)numberOfRecentAudioMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const cache = self.cachedRecentAudioMedia;
    if (cache == nil) {
        [self resetCachedListOfRecentAudioMedia];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialRecentAudioCount;
    }
    return cache.count;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentAudioMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const cache = self.cachedRecentAudioMedia;
    if (cache == nil) {
        [self resetCachedListOfRecentAudioMedia];
    }
    return cache;
}

- (void)resetCachedListOfShows
{
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedListOfShows)];
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
        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedListOfShows)
                                     onQueue:self->_mediaItemCacheModificationQueue
                                       block:^{
            self->_cachedListOfShows = mutableArray.copy;
            [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
                [self.changeDelegate notifyChange:VLCLibraryModelListOfShowsReset withObject:self];
            }];
        }];
    });
}

- (void)resetCachedListOfMovies
{
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedListOfMovies)];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_media_list_t * const p_movie_list = vlc_ml_list_movies(self->_p_mediaLibrary, NULL);
        if (p_movie_list == NULL) {
            return;
        }
        const size_t itemCount = p_movie_list->i_nb_items;
        NSMutableArray * const mutableArray = [[NSMutableArray alloc] initWithCapacity:itemCount];
        for (size_t x = 0; x < p_movie_list->i_nb_items; x++) {
            vlc_ml_media_t * const p_vlc_media = &p_movie_list->p_items[x];
            // Defensive: only add if subtype is MOVIE
            if (p_vlc_media->i_subtype == VLC_ML_MEDIA_SUBTYPE_MOVIE) {
                VLCMediaLibraryMovie * const movie = [[VLCMediaLibraryMovie alloc] initWithMediaItem:p_vlc_media];
                if (movie) {
                    [mutableArray addObject:movie];
                }
            }
        }
        vlc_ml_media_list_release(p_movie_list);
        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedListOfMovies)
                                     onQueue:self->_mediaItemCacheModificationQueue
                                       block:^{
            self->_cachedListOfMovies = mutableArray.copy;
            [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
                [self.changeDelegate notifyChange:VLCLibraryModelListOfMoviesReset withObject:self];
                [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelListOfMoviesReset object:self];
            }];
        }];
    });
}

- (size_t)numberOfShows
{
    NSArray<VLCMediaLibraryShow *> * const cache = self.cachedListOfShows;
    if (cache == nil) {
        [self resetCachedListOfShows];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialShowCount;
    }
    return cache.count;
}

- (size_t)numberOfMovies
{
    NSArray<VLCMediaLibraryMovie *> * const cache = self.cachedListOfMovies;
    if (cache == nil) {
        [self resetCachedListOfMovies];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialMovieCount;
    }
    return cache.count;
}

- (NSArray<VLCMediaLibraryShow *> *)listOfShows
{
    NSArray<VLCMediaLibraryShow *> * const cache = self.cachedListOfShows;
    if (cache == nil) {
        [self resetCachedListOfShows];
    }
    return cache;
}

- (NSArray<VLCMediaLibraryMovie *> *)listOfMovies
{
    NSArray<VLCMediaLibraryMovie *> * const cache = self.cachedListOfMovies;
    if (cache == nil) {
        [self resetCachedListOfMovies];
    }
    return cache;
}

- (size_t)numberOfGroups
{
    NSArray<VLCMediaLibraryGroup *> * const cache = self.cachedListOfGroups;
    if (cache == nil) {
        [self resetCachedListOfGroups];
        // Return initial count here, otherwise it will return 0 on the first time
        return _initialGroupCount;
    }
    return cache.count;
}

- (NSArray<VLCMediaLibraryGroup *> *)listOfGroups
{
    NSArray<VLCMediaLibraryGroup *> * const cache = self.cachedListOfGroups;
    if (cache == nil) {
        [self resetCachedListOfGroups];
    }
    return cache;
}

- (void)resetCachedListOfGroups
{
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedListOfGroups)];
    NSString * const filterString = [self.filterString copy];
    const vlc_ml_query_params_t capturedParams = [self queryParams];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_query_params_t queryParams = capturedParams;
        queryParams.psz_pattern = filterString.length > 0 ? filterString.UTF8String : NULL;
        vlc_ml_group_list_t * const p_group_list =
            vlc_ml_list_groups(self->_p_mediaLibrary, &queryParams);
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
        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedListOfGroups)
                                     onQueue:self->_groupCacheModificationQueue
                                       block:^{
            self->_cachedListOfGroups = mutableArray.copy;
            [self performAfterCacheWritesOnQueue:self->_groupCacheModificationQueue block:^{
                [self.changeDelegate notifyChange:VLCLibraryModelListOfGroupsReset withObject:self];
            }];
        }];
    });
}

- (void)handleMediaItemAddedEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event);
    const vlc_ml_media_t * const p_media = p_event->creation.p_media;
    NSParameterAssert(p_media);

    if (p_media->i_type == VLC_ML_MEDIA_TYPE_AUDIO || p_media->i_type == VLC_ML_MEDIA_TYPE_UNKNOWN) {
        [self resetCachedListOfAudioMedia];
    }

    if (p_media->i_type == VLC_ML_MEDIA_TYPE_VIDEO || p_media->i_type == VLC_ML_MEDIA_TYPE_UNKNOWN) {
        [self resetCachedListOfVideoMedia];

        if (p_media->i_subtype == VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE) {
            [self resetCachedListOfShows];
        }
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
    const NSUInteger generation = self.cacheGeneration;
    NSObject * const token = [self beginCacheUpdateForGetter:@selector(cachedListOfMonitoredFolders)];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        vlc_ml_folder_list_t *pp_entrypoints = vlc_ml_list_entry_points(self->_p_mediaLibrary, NULL);
        if (pp_entrypoints == NULL) {
            msg_Err(getIntf(), "failed to retrieve list of monitored library folders");
            return;
        }

        NSMutableArray * const mutableArray =
            [[NSMutableArray alloc] initWithCapacity:pp_entrypoints->i_nb_items];

        for (size_t x = 0; x < pp_entrypoints->i_nb_items; x++) {
            VLCMediaLibraryEntryPoint * const entryPoint =
                [[VLCMediaLibraryEntryPoint alloc] initWithEntryPoint:&pp_entrypoints->p_items[x]];
            if (entryPoint) {
                [mutableArray addObject:entryPoint];
            }
        }

        vlc_ml_folder_list_release(pp_entrypoints);

        [self performCacheWriteForGeneration:generation
                                       token:token
                                      getter:@selector(cachedListOfMonitoredFolders)
                                     onQueue:self->_mediaItemCacheModificationQueue
                                       block:^{
            // Stop old observers before creating new ones to avoid duplicate
            // FSEvent streams for the same paths
            self.folderObservers = nil;

            NSMutableArray * const mutableObservers =
                [[NSMutableArray alloc] initWithCapacity:mutableArray.count];
            for (VLCMediaLibraryEntryPoint * const entryPoint in mutableArray) {
                NSURL * const url = [NSURL URLWithString:entryPoint.MRL];
                VLCMediaLibraryFolderObserver * const observer =
                    [[VLCMediaLibraryFolderObserver alloc] initWithURL:url];
                if (observer) {
                    [mutableObservers addObject:observer];
                }
            }

            self->_cachedListOfMonitoredFolders = mutableArray.copy;
            self.folderObservers = mutableObservers.copy;
            [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
                [self.changeDelegate notifyChange:VLCLibraryModelListOfMonitoredFoldersUpdated withObject:self];
            }];
        }];
    });
}

- (NSArray<VLCMediaLibraryEntryPoint *> *)listOfMonitoredFolders
{
    NSArray<VLCMediaLibraryEntryPoint *> * const cache = self.cachedListOfMonitoredFolders;
    if (cache == nil) {
        [self resetCachedListOfMonitoredFolders];
    }
    return cache;
}

- (nullable NSArray <VLCMediaLibraryAlbum *>*)listAlbumsOfParentType:(const enum vlc_ml_parent_type)parentType forID:(int64_t)ID
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
    case VLCMediaLibraryParentGroupTypeAllFavorites:
    {
        const vlc_ml_query_params_t queryParams = [self queryParams];
        vlc_ml_media_list_t * const p_media_list = vlc_ml_list_favorite_media(_p_mediaLibrary, &queryParams);
        NSArray * const mediaArray = [NSArray arrayFromVlcMediaList:p_media_list];
        if (p_media_list) {
            vlc_ml_media_list_release(p_media_list);
        }
        return mediaArray ?: @[];
    }
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
    @synchronized (_cacheUpdateTokens) {
        self.cacheGeneration++;
        [_cacheUpdateTokens removeAllObjects];
        [_pendingCacheUpdates removeAllObjects];
    }
    dispatch_queue_t const cacheQueues[] = {
        _mediaItemCacheModificationQueue,
        _albumCacheModificationQueue,
        _artistCacheModificationQueue,
        _genreCacheModificationQueue,
        _groupCacheModificationQueue,
        _mediaTitlesCacheModificationQueue,
    };
    const NSUInteger cacheQueueCount = sizeof(cacheQueues) / sizeof(cacheQueues[0]);
    dispatch_group_t const cacheDropGroup = dispatch_group_create();

    for (NSUInteger index = 0; index < cacheQueueCount; index++) {
        dispatch_group_enter(cacheDropGroup);
    }

    self.cachedVideoMedia = nil;
    self.cachedAudioMedia = nil;
    self.cachedRecentMedia = nil;
    self.cachedRecentAudioMedia = nil;
    self.cachedListOfShows = nil;
    self.cachedListOfMovies = nil;
    self.cachedListOfMonitoredFolders = nil;
    self.cachedAlbums = nil;
    self.cachedArtists = nil;
    self.cachedGenres = nil;
    self.cachedListOfGroups = nil;

    // Barrier sentinels complete after the setter writes on each queue.
    for (NSUInteger index = 0; index < cacheQueueCount; index++) {
        dispatch_barrier_async(cacheQueues[index], ^{
            dispatch_group_leave(cacheDropGroup);
        });
    }

    dispatch_group_notify(cacheDropGroup, _mediaTitlesCacheModificationQueue, ^{
        // Source setters have now submitted their title updates. Queue the final
        // clear after those updates so none can restore the dropped title cache.
        self.cachedMediaTitles = nil;
        [self performAfterCacheWritesOnQueue:self->_mediaTitlesCacheModificationQueue block:^{
            [self.changeDelegate notifyChange:VLCLibraryModelAllCachesDropped withObject:self];
        }];
    });
}

- (void)performActionOnMediaItemInCache:(const int64_t)libraryId 
                                 action:(void (^)(
                                    NSMutableArray * const,
                                    const NSUInteger,
                                    NSMutableArray * const,
                                    const NSUInteger,
                                    NSMutableArray * const,
                                    const NSUInteger,
                                    const NSUInteger
                                 ))action
{
    const NSUInteger generation = self.cacheGeneration;
    dispatch_barrier_async(_mediaItemCacheModificationQueue, ^{
        if (generation != self.cacheGeneration) {
            return;
        }
        BOOL (^idCheckBlock)(VLCMediaLibraryMediaItem * const, const NSUInteger, BOOL * const) = ^BOOL(VLCMediaLibraryMediaItem * const mediaItem, const NSUInteger __unused idx, BOOL * const __unused stop) {
            NSAssert(mediaItem != nil, @"Cache list should not contain nil media items");
            return mediaItem.libraryID == libraryId;
       };

        NSArray<VLCMediaLibraryMediaItem *> * const cachedRecents = self->_cachedRecentMedia;
        NSArray<VLCMediaLibraryMediaItem *> * const cachedVideos = self->_cachedVideoMedia;
        NSArray<VLCMediaLibraryShow *> * const cachedShows = self->_cachedListOfShows;

        const NSUInteger recentsIndex = cachedRecents ? [cachedRecents indexOfObjectPassingTest:idCheckBlock] : NSNotFound;
        const NSUInteger videoIndex = cachedVideos ? [cachedVideos indexOfObjectPassingTest:idCheckBlock] : NSNotFound;

        if (videoIndex != NSNotFound || recentsIndex != NSNotFound) {
            // Search shows for a matching episode.
            NSInteger showIndex = NSNotFound;
            NSInteger episodeIndex = NSNotFound;
            NSUInteger currentShowIndex = 0;
            if (cachedShows) {
                for (VLCMediaLibraryShow * const show in cachedShows) {
                    episodeIndex = [show.episodes indexOfObjectPassingTest:idCheckBlock];
                    if (episodeIndex != NSNotFound) {
                        showIndex = currentShowIndex;
                        break;
                    }
                    currentShowIndex++;
                }
            }

            // Create mutable copies for modification
            NSMutableArray<VLCMediaLibraryMediaItem *> * const recentsMutable = cachedRecents.mutableCopy;
            NSMutableArray<VLCMediaLibraryMediaItem *> * const videoMutable = cachedVideos.mutableCopy;
            NSMutableArray<VLCMediaLibraryShow *> * const showsMutable =
                showIndex == NSNotFound ? nil : cachedShows.mutableCopy;

            SEL const getters[] = { @selector(cachedVideoMedia), @selector(cachedRecentMedia), @selector(cachedListOfShows) };
            [self performCacheMutationForGetters:getters count:showsMutable != nil ? 3 : 2 block:^{
                action(videoMutable, videoIndex, recentsMutable, recentsIndex, showsMutable, showIndex, episodeIndex);
                // Item handlers adjust title counts individually inside this barrier.
                self->_cachedVideoMedia = videoMutable.copy;
                self.cachedRecentMedia = recentsMutable.copy;
                if (showsMutable)
                    self.cachedListOfShows = showsMutable.copy;
            }];
            return;
        }

        // Not in either video cache, check the audio caches.
        NSArray<VLCMediaLibraryMediaItem *> * const cachedRecentAudios = self->_cachedRecentAudioMedia;
        NSArray<VLCMediaLibraryMediaItem *> * const cachedAudios = self->_cachedAudioMedia;

        const NSUInteger recentAudiosIndex = cachedRecentAudios ? [cachedRecentAudios indexOfObjectPassingTest:idCheckBlock] : NSNotFound;
        const NSUInteger audioIndex = cachedAudios ? [cachedAudios indexOfObjectPassingTest:idCheckBlock] : NSNotFound;

        if (audioIndex != NSNotFound || recentAudiosIndex != NSNotFound) {
            // Found in an audio cache - create mutable copies for modification.
            NSMutableArray<VLCMediaLibraryMediaItem *> * const recentAudiosMutable = cachedRecentAudios.mutableCopy;
            NSMutableArray<VLCMediaLibraryMediaItem *> * const audioMutable = cachedAudios.mutableCopy;

            SEL const getters[] = { @selector(cachedAudioMedia), @selector(cachedRecentAudioMedia) };
            [self performCacheMutationForGetters:getters count:2 block:^{
                action(audioMutable, audioIndex, recentAudiosMutable, recentAudiosIndex, nil, NSNotFound, NSNotFound);
                self->_cachedAudioMedia = audioMutable.copy;
                self.cachedRecentAudioMedia = recentAudiosMutable.copy;
            }];
            return;
        }

        // Not found in any cache
        action(nil, NSNotFound, nil, NSNotFound, nil, NSNotFound, NSNotFound);
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

    [self performActionOnMediaItemInCache:itemId action:^(
        NSMutableArray * const cachedMediaArray,
        const NSUInteger cachedMediaIndex,
        NSMutableArray * const recentMediaArray,
        const NSUInteger recentMediaIndex,
        NSMutableArray * const showsArray,
        const NSUInteger showIndex,
        const NSUInteger __unused showEpisodeIndex
    ) {
        if (cachedMediaIndex == NSNotFound && recentMediaIndex == NSNotFound) {
            NSLog(@"Could not handle update for media library item with id %lld in model", itemId);
            return;
        }

        // Notify what happened
        if (cachedMediaIndex != NSNotFound) {
            [self replaceMediaTitle:((VLCMediaLibraryMediaItem *)cachedMediaArray[cachedMediaIndex]).displayString
                          withTitle:mediaItem.displayString];
            [cachedMediaArray replaceObjectAtIndex:cachedMediaIndex withObject:mediaItem];
        }

        if (recentMediaArray != nil && recentMediaIndex != NSNotFound) {
            [recentMediaArray replaceObjectAtIndex:recentMediaIndex withObject:mediaItem];
            [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
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
            }];
        }

        if (showsArray != nil && showIndex != NSNotFound) {
            // An episode has changed. Refresh the whole show.
            VLCMediaLibraryShow * const staleShow = showsArray[showIndex];
            VLCMediaLibraryShow * const updatedShow = [VLCMediaLibraryShow showWithLibraryId:staleShow.libraryID];
            [showsArray replaceObjectAtIndex:showIndex withObject:updatedShow];
            [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
                [self.changeDelegate notifyChange:VLCLibraryModelShowUpdated withObject:updatedShow];
            }];
        }

        [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
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
        }];
    }];
}

- (void)handleMediaItemDeletionEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    [self performActionOnMediaItemInCache:itemId action:^(
        NSMutableArray * const cachedMediaArray,
        const NSUInteger cachedMediaIndex,
        NSMutableArray * const recentMediaArray,
        const NSUInteger recentMediaIndex,
        NSMutableArray * const showsArray,
        const NSUInteger showIndex,
        const NSUInteger __unused showEpisodeIndex
    ) {

        if (cachedMediaIndex == NSNotFound && recentMediaIndex == NSNotFound) {
            NSLog(@"Could not handle deletion for media library item with id %lld in model", itemId);
            return;
        }

        VLCMediaLibraryMediaItem * const mediaItem = cachedMediaIndex != NSNotFound
            ? cachedMediaArray[cachedMediaIndex] : recentMediaArray[recentMediaIndex];
        // Notify what happened
        if (cachedMediaIndex != NSNotFound) {
            [self replaceMediaTitle:mediaItem.displayString withTitle:nil];
            [cachedMediaArray removeObjectAtIndex:cachedMediaIndex];
        }

        if (recentMediaArray != nil && recentMediaIndex != NSNotFound) {
            [recentMediaArray removeObjectAtIndex:recentMediaIndex];
            [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
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
            }];
        }

        if (showsArray != nil && showIndex != NSNotFound) {
            // An episode has changed. Refresh the whole show.
            VLCMediaLibraryShow * const staleShow = showsArray[showIndex];
            VLCMediaLibraryShow * const updatedShow =
                [VLCMediaLibraryShow showWithLibraryId:staleShow.libraryID];

            if (updatedShow == nil || updatedShow.episodeCount == 0) {
                [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
                    [self.changeDelegate notifyChange:VLCLibraryModelShowDeleted
                                           withObject:@(staleShow.libraryID)];
                }];
            } else {
                [showsArray replaceObjectAtIndex:showIndex withObject:updatedShow];
                [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
                    [self.changeDelegate notifyChange:VLCLibraryModelShowUpdated
                                           withObject:updatedShow];
                }];
            }
        }

        [self performAfterCacheWritesOnQueue:self->_mediaItemCacheModificationQueue block:^{
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
        }];
    }];
}

- (NSInteger)indexForAudioGroupInCache:(NSArray * const)cache withItemId:(const int64_t)itemId
{
    if (cache == nil) {
        return NSNotFound;
    }
    return [cache indexOfObjectPassingTest:^BOOL(id<VLCMediaLibraryAudioGroupProtocol> audioGroupItem, const NSUInteger __unused idx, BOOL * const __unused stop) {
        NSAssert(audioGroupItem != nil, @"Cache list should not contain nil audio group items");
        return audioGroupItem.libraryID == itemId;
    }];
}

- (void)updateAudioGroupItem:(const id<VLCMediaLibraryAudioGroupProtocol>)audioGroupItem
                 usingGetter:(NSArray *(^)(void))cacheGetter
                    forCache:(SEL)getter
                 usingSetter:(const SEL)setterSelector
                  usingQueue:(const dispatch_queue_t)queue
        withNotificationName:(const NSNotificationName)notificationName
{
    NSParameterAssert(cacheGetter != nil);
    NSParameterAssert([self respondsToSelector:setterSelector]);
    const int64_t itemId = audioGroupItem.libraryID;

    dispatch_barrier_async(queue, ^{
        NSArray * const cache = cacheGetter();

        if (cache == nil) {
            return;
        }

        const NSUInteger audioGroupIndex = [self indexForAudioGroupInCache:cache
                                                                withItemId:itemId];
        if (audioGroupIndex == NSNotFound) {
            NSLog(@"Did not find audio group item with id %lli in cache.", itemId);
            return;
        }

        // Create mutable copy for modification
        NSMutableArray * const mutableAudioGroupCache = [cache mutableCopy];
        [mutableAudioGroupCache replaceObjectAtIndex:audioGroupIndex withObject:audioGroupItem];

        [self performCacheMutationForGetter:getter block:^{
            const IMP cacheSetterImp = [self methodForSelector:setterSelector];
            void (*cacheSetterFunction)(id, SEL, NSArray *) = (void *)cacheSetterImp;
            cacheSetterFunction(self, setterSelector, mutableAudioGroupCache.copy);
        }];

        [self performAfterCacheWritesOnQueue:queue block:^{
            [self.changeDelegate notifyChange:notificationName withObject:audioGroupItem];
        }];
    });
}

- (void)deleteAudioGroupItemWithId:(const int64_t)itemId
                       usingGetter:(NSArray *(^)(void))cacheGetter
                          forCache:(SEL)getter
                       usingSetter:(const SEL)setterSelector
                        usingQueue:(const dispatch_queue_t)queue
              withNotificationName:(const NSNotificationName)notificationName
{
    NSParameterAssert(cacheGetter != nil);
    NSParameterAssert([self respondsToSelector:setterSelector]);

    dispatch_barrier_async(queue, ^{
        NSArray * const cache = cacheGetter();

        if (cache == nil) {
            return;
        }

        const NSUInteger audioGroupIndex = [self indexForAudioGroupInCache:cache
                                                                withItemId:itemId];
        if (audioGroupIndex == NSNotFound) {
            NSLog(@"Did not find audio group item with id %lli in cache.", itemId);
            return;
        }

        const id<VLCMediaLibraryAudioGroupProtocol> audioGroupItem = cache[audioGroupIndex];

        // Create mutable copy for modification
        NSMutableArray * const mutableAudioGroupCache = [cache mutableCopy];
        [mutableAudioGroupCache removeObjectAtIndex:audioGroupIndex];

        [self performCacheMutationForGetter:getter block:^{
            const IMP cacheSetterImp = [self methodForSelector:setterSelector];
            void (*cacheSetterFunction)(id, SEL, NSArray *) = (void *)cacheSetterImp;
            cacheSetterFunction(self, setterSelector, mutableAudioGroupCache.copy);
        }];

        [self performAfterCacheWritesOnQueue:queue block:^{
            [self.changeDelegate notifyChange:notificationName withObject:audioGroupItem];
        }];
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
                   usingGetter:^NSArray *{ return self->_cachedAlbums; }
                      forCache:@selector(cachedAlbums)
                   usingSetter:@selector(setCachedAlbums:)
                    usingQueue:_albumCacheModificationQueue
          withNotificationName:VLCLibraryModelAlbumUpdated];
}

- (void)handleAlbumDeletionEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    [self deleteAudioGroupItemWithId:itemId
                         usingGetter:^NSArray *{ return self->_cachedAlbums; }
                            forCache:@selector(cachedAlbums)
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
                   usingGetter:^NSArray *{ return self->_cachedArtists; }
                      forCache:@selector(cachedArtists)
                   usingSetter:@selector(setCachedArtists:)
                    usingQueue:_artistCacheModificationQueue
          withNotificationName:VLCLibraryModelArtistUpdated];
}

- (void)handleArtistDeletionEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    [self deleteAudioGroupItemWithId:itemId
                         usingGetter:^NSArray *{ return self->_cachedArtists; }
                            forCache:@selector(cachedArtists)
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
                   usingGetter:^NSArray *{ return self->_cachedGenres; }
                      forCache:@selector(cachedGenres)
                   usingSetter:@selector(setCachedGenres:)
                    usingQueue:_genreCacheModificationQueue
          withNotificationName:VLCLibraryModelGenreUpdated];
}

- (void)handleGenreDeletionEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    [self deleteAudioGroupItemWithId:itemId
                         usingGetter:^NSArray *{ return self->_cachedGenres; }
                            forCache:@selector(cachedGenres)
                         usingSetter:@selector(setCachedGenres:)
                          usingQueue:_genreCacheModificationQueue
                withNotificationName:VLCLibraryModelGenreDeleted];
}

- (void)handleGroupDeletionEvent:(const vlc_ml_event_t *const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;

    dispatch_barrier_async(_groupCacheModificationQueue, ^{
        NSArray<VLCMediaLibraryGroup *> * const cachedGroups = self->_cachedListOfGroups;
        if (cachedGroups == nil) {
            return;
        }

        const NSUInteger groupIdx =
            [cachedGroups indexOfObjectPassingTest:^BOOL(VLCMediaLibraryGroup * const group,
                                                         const NSUInteger __unused idx,
                                                         BOOL * const __unused stop) {
            return group.libraryID == itemId;
        }];

        if (groupIdx == NSNotFound) {
            NSLog(@"Could not handle deletion of group with id %lld in model", itemId);
            return;
        }

        VLCMediaLibraryGroup * const groupToDelete = cachedGroups[groupIdx];

        NSMutableArray * const mutableGroups = cachedGroups.mutableCopy;
        [mutableGroups removeObjectAtIndex:groupIdx];
        [self performCacheMutationForGetter:@selector(cachedListOfGroups) block:^{
            self.cachedListOfGroups = mutableGroups.copy;
        }];

        [self performAfterCacheWritesOnQueue:self->_groupCacheModificationQueue block:^{
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelGroupDeleted
                                                            object:groupToDelete];
        }];
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

    dispatch_barrier_async(_groupCacheModificationQueue, ^{
        NSArray<VLCMediaLibraryGroup *> * const cachedGroups = self->_cachedListOfGroups;
        if (cachedGroups == nil) {
            return;
        }

        const NSUInteger groupIdx = 
            [cachedGroups indexOfObjectPassingTest:^BOOL(VLCMediaLibraryGroup * const group,
                                                         const NSUInteger __unused idx,
                                                         BOOL * const __unused stop) {
            NSAssert(group != nil, @"Cache list should not contain nil groups");
            return group.libraryID == itemId;
        }];

        if (groupIdx == NSNotFound) {
            NSLog(@"Could not handle update of group with id %lld in model", itemId);
            return;
        }

        NSMutableArray * const mutableGroups = cachedGroups.mutableCopy;
        [mutableGroups replaceObjectAtIndex:groupIdx withObject:group];
        [self performCacheMutationForGetter:@selector(cachedListOfGroups) block:^{
            self.cachedListOfGroups = mutableGroups.copy;
        }];

        [self performAfterCacheWritesOnQueue:self->_groupCacheModificationQueue block:^{
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelGroupUpdated
                                                            object:group];
        }];
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

    dispatch_async(dispatch_get_main_queue(), ^{
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

    dispatch_async(dispatch_get_main_queue(), ^{
        [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelPlaylistUpdated
                                                        object:playlist];
    });
}

- (void)handlePlaylistDeletionEvent:(const vlc_ml_event_t * const)p_event
{
    NSParameterAssert(p_event != NULL);

    const int64_t itemId = p_event->modification.i_entity_id;
    dispatch_async(dispatch_get_main_queue(), ^{
        [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelPlaylistDeleted 
                                                        object:@(itemId)];
    });
}

#pragma mark - Favorites Support

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfFavoriteAudioMedia
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    vlc_ml_media_list_t * const p_media_list = vlc_ml_list_favorite_audios(_p_mediaLibrary, &queryParams);
    NSArray * const mediaArray = [NSArray arrayFromVlcMediaList:p_media_list];
    if (mediaArray == nil) {
        return @[];
    }
    vlc_ml_media_list_release(p_media_list);
    return mediaArray;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfFavoriteVideoMedia
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    vlc_ml_media_list_t * const p_media_list = vlc_ml_list_favorite_videos(_p_mediaLibrary, &queryParams);
    NSArray * const mediaArray = [NSArray arrayFromVlcMediaList:p_media_list];
    if (mediaArray == nil) {
        return @[];
    }
    vlc_ml_media_list_release(p_media_list);
    return mediaArray;
}

- (NSArray<VLCMediaLibraryAlbum *> *)listOfFavoriteAlbums
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    vlc_ml_album_list_t * const p_album_list = vlc_ml_list_favorite_albums(_p_mediaLibrary, &queryParams);
    if (p_album_list == NULL) {
        return @[];
    }
    
    NSMutableArray * const mutableArray = [[NSMutableArray alloc] initWithCapacity:p_album_list->i_nb_items];
    for (size_t x = 0; x < p_album_list->i_nb_items; x++) {
        VLCMediaLibraryAlbum * const album = [[VLCMediaLibraryAlbum alloc] initWithAlbum:&p_album_list->p_items[x]];
        if (album != nil) {
            [mutableArray addObject:album];
        }
    }
    vlc_ml_album_list_release(p_album_list);
    return mutableArray.copy;
}

- (NSArray<VLCMediaLibraryArtist *> *)listOfFavoriteArtists
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    vlc_ml_artist_list_t * const p_artist_list = vlc_ml_list_favorite_artists(_p_mediaLibrary, &queryParams);
    if (p_artist_list == NULL) {
        return @[];
    }
    
    NSMutableArray * const mutableArray = [[NSMutableArray alloc] initWithCapacity:p_artist_list->i_nb_items];
    for (size_t x = 0; x < p_artist_list->i_nb_items; x++) {
        VLCMediaLibraryArtist * const artist = [[VLCMediaLibraryArtist alloc] initWithArtist:&p_artist_list->p_items[x]];
        if (artist != nil) {
            [mutableArray addObject:artist];
        }
    }
    vlc_ml_artist_list_release(p_artist_list);
    return mutableArray.copy;
}

- (NSArray<VLCMediaLibraryGenre *> *)listOfFavoriteGenres
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    vlc_ml_genre_list_t * const p_genre_list = vlc_ml_list_favorite_genres(_p_mediaLibrary, &queryParams);
    if (p_genre_list == NULL) {
        return @[];
    }
    
    NSMutableArray * const mutableArray = [[NSMutableArray alloc] initWithCapacity:p_genre_list->i_nb_items];
    for (size_t x = 0; x < p_genre_list->i_nb_items; x++) {
        VLCMediaLibraryGenre * const genre = [[VLCMediaLibraryGenre alloc] initWithGenre:&p_genre_list->p_items[x]];
        if (genre != nil) {
            [mutableArray addObject:genre];
        }
    }
    vlc_ml_genre_list_release(p_genre_list);
    return mutableArray.copy;
}

- (size_t)numberOfFavoriteAudioMedia
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    return vlc_ml_count_favorite_audios(_p_mediaLibrary, &queryParams);
}

- (size_t)numberOfFavoriteVideoMedia
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    return vlc_ml_count_favorite_videos(_p_mediaLibrary, &queryParams);
}

- (size_t)numberOfFavoriteAlbums
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    return vlc_ml_count_favorite_albums(_p_mediaLibrary, &queryParams);
}

- (size_t)numberOfFavoriteArtists
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    return vlc_ml_count_favorite_artists(_p_mediaLibrary, &queryParams);
}

- (size_t)numberOfFavoriteGenres
{
    const vlc_ml_query_params_t queryParams = [self queryParams];
    return vlc_ml_count_favorite_genres(_p_mediaLibrary, &queryParams);
}

@end
