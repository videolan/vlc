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

NSString *VLCLibraryModelArtistListUpdated = @"VLCLibraryModelArtistListUpdated";
NSString *VLCLibraryModelAlbumListUpdated = @"VLCLibraryModelAlbumListUpdated";
NSString *VLCLibraryModelGenreListUpdated = @"VLCLibraryModelGenreListUpdated";
NSString *VLCLibraryModelListOfMonitoredFoldersUpdated = @"VLCLibraryModelListOfMonitoredFoldersUpdated";
NSString *VLCLibraryModelMediaItemThumbnailGenerated = @"VLCLibraryModelMediaItemThumbnailGenerated";

NSString *VLCLibraryModelAudioMediaListReset = @"VLCLibraryModelAudioMediaListReset";
NSString *VLCLibraryModelVideoMediaListReset = @"VLCLibraryModelVideoMediaListReset";
NSString *VLCLibraryModelRecentsMediaListReset = @"VLCLibraryModelRecentsMediaListReset";

NSString *VLCLibraryModelAudioMediaItemDeleted = @"VLCLibraryModelAudioMediaItemDeleted";
NSString *VLCLibraryModelVideoMediaItemDeleted = @"VLCLibraryModelVideoMediaItemDeleted";
NSString *VLCLibraryModelRecentsMediaItemDeleted = @"VLCLibraryModelRecentsMediaItemDeleted";

NSString *VLCLibraryModelAudioMediaItemUpdated = @"VLCLibraryModelAudioMediaItemUpdated";
NSString *VLCLibraryModelVideoMediaItemUpdated = @"VLCLibraryModelVideoMediaItemUpdated";
NSString *VLCLibraryModelRecentsMediaItemUpdated = @"VLCLibraryModelRecentsMediaItemUpdated";

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
    NSArray *_cachedListOfMonitoredFolders;
    NSNotificationCenter *_defaultNotificationCenter;

    enum vlc_ml_sorting_criteria_t _sortCriteria;
    bool _sortDescending;
    NSString *_filterString;
    
    size_t _initialVideoCount;
    size_t _initialAudioCount;
}

@property (readwrite, atomic) NSArray *cachedAudioMedia;
@property (readwrite, atomic) NSArray *cachedArtists;
@property (readwrite, atomic) NSArray *cachedAlbums;
@property (readwrite, atomic) NSArray *cachedGenres;
@property (readwrite, atomic) NSArray *cachedVideoMedia;
@property (readwrite, atomic) NSArray *cachedRecentMedia;
@property (readwrite, atomic) NSArray *cachedListOfMonitoredFolders;

- (void)resetCachedMediaItemLists;
- (void)resetCachedListOfArtists;
- (void)resetCachedListOfAlbums;
- (void)resetCachedListOfGenres;
- (void)resetCachedListOfMonitoredFolders;
- (void)mediaItemThumbnailGenerated:(VLCMediaLibraryMediaItem *)mediaItem;
- (void)handleMediaItemDeletionEvent:(const vlc_ml_event_t * const)p_event;
- (void)handleMediaItemUpdateEvent:(const vlc_ml_event_t * const)p_event;

@end

static void libraryCallback(void *p_data, const vlc_ml_event_t *p_event)
{
    switch(p_event->i_type)
    {
        case VLC_ML_EVENT_MEDIA_ADDED:
        {
            VLCLibraryModel *libraryModel = (__bridge VLCLibraryModel *)p_data;
            [libraryModel resetCachedMediaItemLists];
            break;
        }
        case VLC_ML_EVENT_MEDIA_UPDATED:
        {
            VLCLibraryModel *libraryModel = (__bridge VLCLibraryModel *)p_data;
            [libraryModel handleMediaItemUpdateEvent:p_event];
            break;
        }
        case VLC_ML_EVENT_MEDIA_DELETED:
        {
            VLCLibraryModel *libraryModel = (__bridge VLCLibraryModel *)p_data;
            [libraryModel handleMediaItemDeletionEvent:p_event];
            break;
        }
        case VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED:
            if (p_event->media_thumbnail_generated.b_success) {
                VLCMediaLibraryMediaItem *mediaItem = [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:(struct vlc_ml_media_t *)p_event->media_thumbnail_generated.p_media];
                if (mediaItem == nil) {
                    return;
                }
                dispatch_async(dispatch_get_main_queue(), ^{
                    VLCLibraryModel *libraryModel = (__bridge VLCLibraryModel *)p_data;
                    [libraryModel mediaItemThumbnailGenerated:mediaItem];
                });
            }
            break;
        case VLC_ML_EVENT_ARTIST_ADDED:
        case VLC_ML_EVENT_ARTIST_UPDATED:
        case VLC_ML_EVENT_ARTIST_DELETED:
        {
            VLCLibraryModel *libraryModel = (__bridge VLCLibraryModel *)p_data;
            [libraryModel resetCachedListOfArtists];
            break;
        }
        case VLC_ML_EVENT_ALBUM_ADDED:
        case VLC_ML_EVENT_ALBUM_UPDATED:
        case VLC_ML_EVENT_ALBUM_DELETED:
        {
            VLCLibraryModel *libraryModel = (__bridge VLCLibraryModel *)p_data;
            [libraryModel resetCachedListOfAlbums];
            break;
        }
        case VLC_ML_EVENT_GENRE_ADDED:
        case VLC_ML_EVENT_GENRE_UPDATED:
        case VLC_ML_EVENT_GENRE_DELETED:
        {
            VLCLibraryModel *libraryModel = (__bridge VLCLibraryModel *)p_data;
            [libraryModel resetCachedListOfGenres];
            break;
        }
        case VLC_ML_EVENT_FOLDER_ADDED:
        case VLC_ML_EVENT_FOLDER_UPDATED:
        case VLC_ML_EVENT_FOLDER_DELETED:
        {
            VLCLibraryModel *libraryModel = (__bridge VLCLibraryModel *)p_data;
            [libraryModel resetCachedListOfMonitoredFolders];
            break;
        }
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
        _p_mediaLibrary = library;
        _p_eventCallback = vlc_ml_event_register_callback(_p_mediaLibrary, libraryCallback, (__bridge void *)self);
        _defaultNotificationCenter = [NSNotificationCenter defaultCenter];
        [_defaultNotificationCenter addObserver:self
                                       selector:@selector(applicationWillTerminate:)
                                           name:NSApplicationWillTerminateNotification
                                         object:nil];

        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            const vlc_ml_query_params_t queryParameters = {};

            // Preload video and audio count for gui
            self->_initialVideoCount = vlc_ml_count_video_media(self->_p_mediaLibrary, &queryParameters);
            self->_initialAudioCount = vlc_ml_count_audio_media(self->_p_mediaLibrary, &queryParameters);
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
        vlc_ml_media_list_t *p_media_list = vlc_ml_list_audio_media(self->_p_mediaLibrary, &queryParams);
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
            self.cachedAudioMedia = [mutableArray copy];
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
    }
    return _cachedArtists.count;
}

- (void)resetCachedListOfArtists
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t queryParams = [self queryParams];
        vlc_ml_artist_list_t *p_artist_list = vlc_ml_list_artists(self->_p_mediaLibrary, &queryParams, NO);
        NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_artist_list->i_nb_items];
        for (size_t x = 0; x < p_artist_list->i_nb_items; x++) {
            VLCMediaLibraryArtist *artist = [[VLCMediaLibraryArtist alloc] initWithArtist:&p_artist_list->p_items[x]];
            if (artist != nil) {
                [mutableArray addObject:artist];
            }
        }
        vlc_ml_artist_list_release(p_artist_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedArtists = [mutableArray copy];
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
    }
    return _cachedAlbums.count;
}

- (void)resetCachedListOfAlbums
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t queryParams = [self queryParams];
        vlc_ml_album_list_t *p_album_list = vlc_ml_list_albums(self->_p_mediaLibrary, &queryParams);
        NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_album_list->i_nb_items];
        for (size_t x = 0; x < p_album_list->i_nb_items; x++) {
            VLCMediaLibraryAlbum *album = [[VLCMediaLibraryAlbum alloc] initWithAlbum:&p_album_list->p_items[x]];
            [mutableArray addObject:album];
        }
        vlc_ml_album_list_release(p_album_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedAlbums = [mutableArray copy];
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
    }
    return _cachedGenres.count;
}

- (void)resetCachedListOfGenres
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t queryParams = [self queryParams];
        vlc_ml_genre_list_t *p_genre_list = vlc_ml_list_genres(self->_p_mediaLibrary, &queryParams);
        NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:p_genre_list->i_nb_items];
        for (size_t x = 0; x < p_genre_list->i_nb_items; x++) {
            VLCMediaLibraryGenre *genre = [[VLCMediaLibraryGenre alloc] initWithGenre:&p_genre_list->p_items[x]];
            [mutableArray addObject:genre];
        }
        vlc_ml_genre_list_release(p_genre_list);
        dispatch_async(dispatch_get_main_queue(), ^{
            self.cachedGenres = [mutableArray copy];
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

- (void)resetCachedListOfRecentMedia
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const vlc_ml_query_params_t queryParameters = { .i_nbResults = 20 };
        // we don't set the sorting criteria here as they are not applicable to history
        // we only show videos for recents
        vlc_ml_media_list_t *p_media_list = vlc_ml_list_history_by_type(self->_p_mediaLibrary, &queryParameters, VLC_ML_MEDIA_TYPE_VIDEO);
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
            self.cachedRecentMedia = [mutableArray copy];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentsMediaListReset object:self];
        });
    });
}

- (size_t)numberOfRecentMedia
{
    if (!_cachedRecentMedia) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self resetCachedListOfRecentMedia];
        });
    }
    return _cachedRecentMedia.count;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentMedia
{
    if (!_cachedRecentMedia) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self resetCachedListOfRecentMedia];
        });
    }
    return _cachedRecentMedia;
}

- (void)resetCachedMediaItemLists
{
    [self resetCachedListOfRecentMedia];
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

- (nullable NSArray <VLCMediaLibraryAlbum *>*)listAlbumsOfParentType:(enum vlc_ml_parent_type)parentType forID:(int64_t)ID;
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

    [_defaultNotificationCenter postNotificationName:VLCLibraryModelVideoMediaListReset object:self];
    [_defaultNotificationCenter postNotificationName:VLCLibraryModelAudioMediaListReset object:self];
}

- (void)performActionOnMediaItemInCache:(const int64_t)libraryId action:(void (^)(const NSMutableArray*, const NSUInteger, const NSMutableArray*, const NSUInteger))action
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        BOOL (^idCheckBlock)(VLCMediaLibraryMediaItem * const, const NSUInteger, BOOL * const) = ^BOOL(VLCMediaLibraryMediaItem * const mediaItem, const NSUInteger idx, BOOL * const stop) {
            NSAssert(mediaItem != nil, @"Cache list should not contain nil media items");
            return mediaItem.libraryID == libraryId;
        };

        // Recents can contain media items the other two do
        const NSUInteger recentsIndex = [self.cachedRecentMedia indexOfObjectPassingTest:idCheckBlock];
        BOOL isInRecents = recentsIndex != NSNotFound;
        NSMutableArray *recentsMutable;
        if (isInRecents) {
            recentsMutable = [self.cachedRecentMedia mutableCopy];
        }

        const NSUInteger videoIndex = [self.cachedVideoMedia indexOfObjectPassingTest:idCheckBlock];
        if (videoIndex != NSNotFound) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSMutableArray *videoMutable = [self.cachedVideoMedia mutableCopy];
                action(videoMutable, videoIndex, recentsMutable, recentsIndex);
                self.cachedVideoMedia = [videoMutable copy];
                self.cachedRecentMedia = [recentsMutable copy];
            });
            return;
        }

        const NSUInteger audioIndex = [self.cachedAudioMedia indexOfObjectPassingTest:idCheckBlock];
        if (audioIndex != NSNotFound) {
            dispatch_async(dispatch_get_main_queue(), ^{
                NSMutableArray *audioMutable = [self.cachedAudioMedia mutableCopy];
                action(audioMutable, audioIndex, recentsMutable, recentsIndex);
                self.cachedAudioMedia = [audioMutable copy];
                self.cachedRecentMedia = [recentsMutable copy];
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

    NSLog(@"Updating %lli", itemId);

    VLCMediaLibraryMediaItem * const mediaItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:itemId];
    if (mediaItem == nil) {
        NSLog(@"Could not find a library media item with this ID. Can't handle update.");
        return;
    }

    [self performActionOnMediaItemInCache:itemId action:^(NSMutableArray * const cachedMediaArray, const NSUInteger cachedMediaIndex, NSMutableArray * const recentMediaArray, const NSUInteger recentMediaIndex) {

        // Notify what happened
        [cachedMediaArray replaceObjectAtIndex:cachedMediaIndex withObject:mediaItem];

        if (recentMediaArray != nil && recentMediaIndex != NSNotFound) {
            [cachedMediaArray replaceObjectAtIndex:recentMediaIndex withObject:mediaItem];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentsMediaItemUpdated object:mediaItem];
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
    NSLog(@"Deleting %lli", itemId);

    [self performActionOnMediaItemInCache:itemId action:^(NSMutableArray * const cachedMediaArray, const NSUInteger cachedMediaIndex, NSMutableArray * const recentMediaArray, const NSUInteger recentMediaIndex) {

        if (cachedMediaArray == nil) {
            NSLog(@"Could not handle deletion for media library item with id %lld in model", itemId);
            return;
        }

        VLCMediaLibraryMediaItem * mediaItem = cachedMediaArray[cachedMediaIndex];
        // Notify what happened
        NSLog(@"Deleting %@", mediaItem.displayString);
        [cachedMediaArray removeObjectAtIndex:cachedMediaIndex];

        if (recentMediaArray != nil) {
            [recentMediaArray removeObjectAtIndex:recentMediaIndex];
            [self->_defaultNotificationCenter postNotificationName:VLCLibraryModelRecentsMediaItemDeleted object:mediaItem];
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

@end
