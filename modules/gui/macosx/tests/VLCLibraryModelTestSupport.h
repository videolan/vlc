/****************************************************************************
 * VLCLibraryModelTestSupport.h: controlled library model queries
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "library/VLCLibraryModel.h"

@class VLCMediaLibraryFolderObserver;

// Private model declarations shared by the test suites and their fixtures.
@interface VLCLibraryModel ()
- (instancetype)initWithoutMediaLibrary;

- (vlc_ml_query_params_t)queryParams;
- (void)applicationWillTerminate:(NSNotification *)notification;
- (void)dropCaches;
- (void)resetCachedListOfAudioMedia;
- (void)resetCachedListOfVideoMedia;
- (void)resetCachedListOfArtists;
- (void)resetCachedListOfAlbums;
- (void)resetCachedListOfGenres;
- (void)resetCachedListOfGroups;
- (void)resetCachedListOfShows;
- (void)resetCachedListOfMovies;
- (void)resetCachedListOfRecentMedia;
- (void)resetCachedListOfRecentAudioMedia;
- (void)resetCachedListOfMonitoredFolders;
- (void)handleMediaItemAddedEvent:(const vlc_ml_event_t *)event;
- (void)handleMediaItemUpdateEvent:(const vlc_ml_event_t *)event;
- (void)handleMediaItemDeletionEvent:(const vlc_ml_event_t *)event;
- (void)handleArtistUpdateEvent:(const vlc_ml_event_t *)event;
- (void)handleArtistDeletionEvent:(const vlc_ml_event_t *)event;
- (void)handleAlbumUpdateEvent:(const vlc_ml_event_t *)event;
- (void)handleAlbumDeletionEvent:(const vlc_ml_event_t *)event;
- (void)handleGenreUpdateEvent:(const vlc_ml_event_t *)event;
- (void)handleGenreDeletionEvent:(const vlc_ml_event_t *)event;
- (void)handleGroupUpdateEvent:(const vlc_ml_event_t *)event;
- (void)handleGroupDeletionEvent:(const vlc_ml_event_t *)event;
- (void)refreshCachedShowForLibraryID:(int64_t)libraryID;
- (void)updateCachedShow:(VLCMediaLibraryShow *)show
               libraryID:(int64_t)libraryID
              generation:(NSUInteger)generation
                   token:(NSObject *)token;
- (NSArray *)readCacheOnQueue:(dispatch_queue_t)queue
                  withBlock:(NSArray *(^)(void))block;
- (NSObject *)beginCacheUpdateForGetter:(SEL)getter;
- (void)performCacheWriteForGeneration:(NSUInteger)generation
                                 token:(NSObject *)token
                                getter:(SEL)getter
                               onQueue:(dispatch_queue_t)queue
                                 block:(dispatch_block_t)block;

@property (readwrite, nonatomic) NSArray<VLCMediaLibraryMediaItem *> *cachedAudioMedia;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryMediaItem *> *cachedVideoMedia;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryArtist *> *cachedArtists;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryAlbum *> *cachedAlbums;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryGenre *> *cachedGenres;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryGroup *> *cachedListOfGroups;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryShow *> *cachedListOfShows;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryMovie *> *cachedListOfMovies;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryMediaItem *> *cachedRecentMedia;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryMediaItem *> *cachedRecentAudioMedia;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryEntryPoint *> *cachedListOfMonitoredFolders;
@property (readwrite, nonatomic) NSArray<NSString *> *cachedMediaTitles;
@property (readonly, nonatomic) NSArray<VLCMediaLibraryFolderObserver *> *folderObservers;
@end


// Installs a blocking barrier after a read, so the test can enqueue a pending
// write. Subsequent reads and writes on that queue wait until the gate is released.
@interface VLCLibraryModelTestCacheReadGate : VLCLibraryModel
- (void)holdWritesFollowingNextCacheReadWithHandler:(dispatch_block_t)handler;
- (void)releaseCacheReads;
@end

// Holds the requested cache's next result before the real model queues its write.
// Other queries and invalidations can proceed without depending on timing.
@interface VLCLibraryModelTestQueryGate : VLCLibraryModelTestCacheReadGate
- (instancetype)initWithLibrary:(vlc_medialibrary_t *)library
                 observeEvents:(BOOL)observeEvents;

- (void)holdNextQueryResultForGetter:(SEL)getter
                             handler:(dispatch_block_t)handler
                   completionHandler:(dispatch_block_t)completionHandler;
- (void)releaseQuery;
- (void)holdNextCacheRefreshForGetter:(SEL)getter
                              handler:(dispatch_block_t)handler
                    completionHandler:(dispatch_block_t)completionHandler;
// Holds the owning queue so a submitted query write cannot execute yet.
- (void)holdNextCacheWriteForGetter:(SEL)getter
                            handler:(dispatch_block_t)handler
                  completionHandler:(dispatch_block_t)completionHandler;
- (void)releaseCacheWrite;
@property (readonly) BOOL didExecuteHeldCacheWrite;
- (NSUInteger)queryCountForGetter:(SEL)getter;
- (void)refreshCacheForGetter:(SEL)getter;
- (void)finishPendingQueriesWithCompletion:(dispatch_block_t)completion;
@end

// Counts full/targeted show queries and observes removal submissions, allowing
// tests to distinguish a targeted refresh from a complete show-list rebuild.
@interface VLCLibraryModelTestShowQueries : VLCLibraryModelTestQueryGate
@property (readonly) NSUInteger showListQueryCount;
@property (readonly) NSDictionary<NSNumber *, NSNumber *> *showQueryCounts;
- (void)observeDeletionsForMediaIDs:(NSArray<NSNumber *> *)mediaIDs
                          handler:(dispatch_block_t)handler;
- (void)holdNextShowQueryResultWithHandler:(dispatch_block_t)handler
                         completionHandler:(dispatch_block_t)completionHandler;
- (void)releaseShowQuery;
@end

// Holds the title queue after a read, delaying title writes but not media writes.

typedef NS_ENUM(NSUInteger, VLCLibraryModelTestCacheKind) {
    VLCLibraryModelTestAudioCache,
    VLCLibraryModelTestVideoCache,
    VLCLibraryModelTestArtistCache,
    VLCLibraryModelTestAlbumCache,
    VLCLibraryModelTestGenreCache,
    VLCLibraryModelTestGroupCache,
    VLCLibraryModelTestShowCache,
    VLCLibraryModelTestMovieCache,
    VLCLibraryModelTestRecentAudioCache,
    VLCLibraryModelTestRecentVideoCache,
};

// Real persisted records and queries, with model events delivered explicitly so
// the tests control their order. No database query results are mocked.
@interface VLCLibraryModelTestOverlapFixture : NSObject
- (instancetype)initWithCacheKind:(VLCLibraryModelTestCacheKind)kind;
- (instancetype)initWithGroupName:(NSString *)groupName
                  otherGroupName:(NSString *)otherGroupName;
@property (readonly) VLCLibraryModelTestQueryGate *model;
@property (readonly) SEL cacheGetter;
@property (readonly) NSArray<NSNotificationName> *changeNotifications;
@property (readonly) id<VLCMediaLibraryItemProtocol> pendingItem;
@property (readonly) id<VLCMediaLibraryItemProtocol> changedItem;
- (NSArray<id<VLCMediaLibraryItemProtocol>> *)libraryItems;
- (NSArray<id<VLCMediaLibraryItemProtocol>> *)cachedItems;
- (void)seedCacheWithItems:(NSArray<id<VLCMediaLibraryItemProtocol>> *)items;
- (BOOL)preparePendingChange;
- (void)deliverRefreshEvent;
- (void)refreshCache;
- (BOOL)updateChangedItem;
- (BOOL)setChangedItemFavorite:(BOOL)favorite;
- (BOOL)setChangedItemPresent:(BOOL)present;
- (id<VLCMediaLibraryItemProtocol>)refetchChangedItem;
- (BOOL)deleteChangedItem;
- (BOOL)deletePendingItem;
- (void)deliverUpdateEvent;
- (void)deliverUpdateEventForID:(int64_t)libraryID;
- (BOOL)renameItemWithID:(int64_t)libraryID
                    to:(NSString *)name;
- (void)deliverDeletionEvent;
- (void)deliverPendingDeletionEvent;
- (void)deliverDeletionEventForID:(int64_t)libraryID;
@end
