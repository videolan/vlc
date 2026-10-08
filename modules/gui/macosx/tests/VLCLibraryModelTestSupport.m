/****************************************************************************
 * VLCLibraryModelTestSupport.m: controlled library model queries
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

#import "VLCLibraryModelTestSupport.h"
#import <XCTest/XCTest.h>
#import "VLCLibraryDataTypesIntegrationTestSupport.h"
#import "extensions/NSArray+VLCAdditions.h"

VLCMediaLibraryMediaItem *VLCLibraryModelTestMediaItem(vlc_ml_media_type_t type,
                                                       int64_t libraryID,
                                                       NSString *title)
{
    vlc_ml_file_list_t files = {0};
    vlc_ml_media_track_list_t tracks = {0};
    vlc_ml_media_t media = {
        .i_id = libraryID,
        .i_type = type,
        .p_files = &files,
        .p_tracks = &tracks,
        .psz_title = (char *)title.UTF8String
    };
    return [[VLCMediaLibraryMediaItem alloc] initWithMediaItem:&media];
}

@implementation VLCLibraryModelTestShowQueries {
    NSUInteger _showListQueryCount;
    NSMutableDictionary<NSNumber *, NSNumber *> *_showQueryCounts;
    NSMutableSet<NSNumber *> *_expectedDeletionIDs;
    dispatch_block_t _deletionsSubmitted;
    dispatch_semaphore_t _showQueryRelease;
    dispatch_block_t _showQueryCaptured;
    dispatch_block_t _showQueryProcessed;
    dispatch_group_t _targetedShowQueries;
}

- (NSUInteger)showListQueryCount
{
    @synchronized (self) {
        return _showListQueryCount;
    }
}

- (NSDictionary<NSNumber *, NSNumber *> *)showQueryCounts
{
    @synchronized (self) {
        return _showQueryCounts.copy ?: @{};
    }
}

- (void)resetCachedListOfShows
{
    @synchronized (self) {
        _showListQueryCount++;
    }
    [super resetCachedListOfShows];
}

- (void)refreshCachedShowForLibraryID:(int64_t)libraryID
{
    @synchronized (self) {
        if (_targetedShowQueries == nil)
            _targetedShowQueries = dispatch_group_create();
        dispatch_group_enter(_targetedShowQueries);
        if (_showQueryCounts == nil) {
            _showQueryCounts = NSMutableDictionary.dictionary;
        }
        NSNumber * const key = @(libraryID);
        _showQueryCounts[key] = @(_showQueryCounts[key].unsignedIntegerValue + 1);
    }
    [super refreshCachedShowForLibraryID:libraryID];
}

- (void)observeDeletionsForMediaIDs:(NSArray<NSNumber *> *)mediaIDs
                          handler:(dispatch_block_t)handler
{
    @synchronized (self) {
        _expectedDeletionIDs = [NSMutableSet setWithArray:mediaIDs];
        _deletionsSubmitted = [handler copy];
    }
}

- (void)holdNextShowQueryResultWithHandler:(dispatch_block_t)handler
                         completionHandler:(dispatch_block_t)completionHandler
{
    @synchronized (self) {
        _showQueryRelease = dispatch_semaphore_create(0);
        _showQueryCaptured = [handler copy];
        _showQueryProcessed = [completionHandler copy];
    }
}

- (void)releaseShowQuery
{
    dispatch_semaphore_signal(_showQueryRelease);
}

- (void)updateCachedShow:(VLCMediaLibraryShow *)show
               libraryID:(int64_t)libraryID
              generation:(NSUInteger)generation
                   token:(NSObject *)token
{
    dispatch_block_t captured = nil;
    dispatch_block_t processed = nil;
    dispatch_semaphore_t release = nil;
    @synchronized (self) {
        captured = _showQueryCaptured;
        processed = _showQueryProcessed;
        release = _showQueryRelease;
        _showQueryCaptured = nil;
        _showQueryProcessed = nil;
    }
    if (captured != nil) {
        dispatch_async(dispatch_get_main_queue(), captured);
        dispatch_semaphore_wait(release, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
    }
    [super updateCachedShow:show
                  libraryID:libraryID
                 generation:generation
                      token:token];
    // Drain the submitted write without holding the model lock. A superseded
    // result can start another targeted lookup from inside that cache barrier;
    // its group entry must precede this lookup's departure.
    (void)self.cachedListOfShows;
    dispatch_group_t queries;
    @synchronized (self) {
        queries = _targetedShowQueries;
    }
    dispatch_group_leave(queries);
    if (processed != nil) {
        // The real write has now executed or been rejected by the model.
        dispatch_async(dispatch_get_main_queue(), processed);
    }
}

- (void)finishPendingQueriesWithCompletion:(dispatch_block_t)completion
{
    [super finishPendingQueriesWithCompletion:^{
        dispatch_group_t queries;
        @synchronized (self) {
            queries = self->_targetedShowQueries;
        }
        if (queries == nil)
            dispatch_async(dispatch_get_main_queue(), completion);
        else
            dispatch_group_notify(queries, dispatch_get_main_queue(), completion);
    }];
}

- (void)handleMediaItemDeletionEvent:(const vlc_ml_event_t *)event
{
    [super handleMediaItemDeletionEvent:event];
    @synchronized (self) {
        [_expectedDeletionIDs removeObject:@(event->deletion.i_entity_id)];
        if (_expectedDeletionIDs != nil && _expectedDeletionIDs.count == 0) {
            dispatch_async(dispatch_get_main_queue(), _deletionsSubmitted);
            _expectedDeletionIDs = nil;
            _deletionsSubmitted = nil;
        }
    }
}

@end

@implementation VLCLibraryModelTestCacheReadGate {
    dispatch_semaphore_t _cacheReadRelease;
    dispatch_block_t _cacheReadCaptured;
}

- (void)holdWritesFollowingNextCacheReadWithHandler:(dispatch_block_t)handler
{
    @synchronized (self) {
        _cacheReadRelease = dispatch_semaphore_create(0);
        _cacheReadCaptured = [handler copy];
    }
}

- (void)releaseCacheReads
{
    dispatch_semaphore_t release;
    @synchronized (self) {
        release = _cacheReadRelease;
        _cacheReadRelease = nil;
        _cacheReadCaptured = nil;
    }
    if (release != nil)
        dispatch_semaphore_signal(release);
}

- (NSArray *)readCacheOnQueue:(dispatch_queue_t)queue
                  withBlock:(NSArray *(^)(void))block
{
    NSArray * const cache = [super readCacheOnQueue:queue withBlock:block];
    dispatch_block_t captured = nil;
    dispatch_semaphore_t release = nil;
    @synchronized (self) {
        captured = _cacheReadCaptured;
        release = _cacheReadRelease;
        _cacheReadCaptured = nil;
    }
    if (captured != nil) {
        dispatch_barrier_async(queue,
                               ^{
            dispatch_async(dispatch_get_main_queue(), captured);
            dispatch_semaphore_wait(release, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
        });
    }
    return cache;
}

@end

@implementation VLCLibraryModelTestTitleReadGate {
    dispatch_block_t _titleReadCaptured;
}

- (void)holdNextTitleReadWithHandler:(dispatch_block_t)handler
{
    @synchronized (self) {
        _titleReadCaptured = [handler copy];
    }
}

- (void)releaseTitleRead
{
    @synchronized (self) {
        _titleReadCaptured = nil;
    }
    [self releaseCacheReads];
}

- (NSArray<NSString *> *)cachedMediaTitles
{
    @synchronized (self) {
        if (_titleReadCaptured != nil) {
            [self holdWritesFollowingNextCacheReadWithHandler:_titleReadCaptured];
            _titleReadCaptured = nil;
        }
        // Keep the gate tied to this getter even if another cache is read concurrently.
        return [super cachedMediaTitles];
    }
}

@end

@implementation VLCLibraryModelTestQueryGate {
    dispatch_semaphore_t _queryRelease;
    SEL _getter;
    NSObject *_heldToken;
    dispatch_block_t _resultCaptured;
    dispatch_block_t _queryProcessed;
    dispatch_semaphore_t _writeRelease;
    SEL _writeGetter;
    NSObject *_writeToken;
    dispatch_block_t _writeCaptured;
    dispatch_block_t _writeProcessed;
    BOOL _didExecuteHeldCacheWrite;
    BOOL _captureBackgroundQuery;
    NSMutableDictionary<NSValue *, NSNumber *> *_queryCounts;
    NSMutableSet<NSObject *> *_pendingQueries;
    dispatch_block_t _queriesFinished;
    BOOL _loadsInitialCounts;
    BOOL _initialCountsLoaded;
    NSThread *_initialCountReadThread;
}

- (instancetype)initWithLibrary:(vlc_medialibrary_t *)library
{
    return [self initWithLibrary:library observeEvents:YES];
}

- (instancetype)initWithLibrary:(vlc_medialibrary_t *)library
                 observeEvents:(BOOL)observeEvents
{
    // The last startup count is audio history. Ensure it is nonzero so its
    // publication can be observed without guessing how long startup takes.
    // Do not add a pending history item if this fixture already has one.
    if (vlc_ml_count_audio_history(library, NULL) == 0) {
        const int result = vlc_ml_media_set_played(library,
                                                   VLCLibraryDataTypesIntegrationFactoryAudioMediaID(),
                                                   true);
        XCTAssertEqual(result, VLC_SUCCESS);
    }
    self = [super initWithLibrary:library];
    if (self != nil) {
        _loadsInitialCounts = YES;
        if (!observeEvents)
            [self applicationWillTerminate:nil];
        [self waitForInitialCounts];
    }
    return self;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)cachedRecentAudioMedia
{
    @synchronized (self) {
        if (_initialCountReadThread == NSThread.currentThread)
            return nil;
    }
    return [super cachedRecentAudioMedia];
}

- (void)resetCachedListOfRecentAudioMedia
{
    @synchronized (self) {
        if (_initialCountReadThread == NSThread.currentThread)
            return;
    }
    [super resetCachedListOfRecentAudioMedia];
}

- (BOOL)initialCountsLoaded
{
    // Read the real startup counter through its public getter, without allowing
    // a lazy cache query to satisfy the condition instead of the startup worker.
    @synchronized (self) {
        if (!_loadsInitialCounts || _initialCountsLoaded)
            return YES;
        _initialCountReadThread = NSThread.currentThread;
        const size_t count = [super numberOfRecentAudioMedia];
        _initialCountReadThread = nil;
        // Startup completes once; a later filter must not change this result.
        _initialCountsLoaded = count > 0;
        return _initialCountsLoaded;
    }
}

- (void)waitForInitialCounts
{
    NSPredicate * const predicate = [NSPredicate predicateWithBlock:
        ^BOOL(VLCLibraryModelTestQueryGate *model,
              NSDictionary * __unused bindings) {
        return model.initialCountsLoaded;
    }];
    XCTNSPredicateExpectation * const counts = [[XCTNSPredicateExpectation alloc] initWithPredicate:predicate
                                                                                         object:self];
    XCTAssertEqual([XCTWaiter waitForExpectations:@[ counts ] timeout:10.], XCTWaiterResultCompleted);
}

- (void)holdNextQueryResultForGetter:(SEL)getter
                             handler:(dispatch_block_t)handler
                   completionHandler:(dispatch_block_t)completionHandler
{
    @synchronized (self) {
        _queryRelease = dispatch_semaphore_create(0);
        _getter = getter;
        _heldToken = nil;
        _resultCaptured = [handler copy];
        _queryProcessed = [completionHandler copy];
    }
}

- (NSObject *)beginCacheUpdateForGetter:(SEL)getter
{
    NSObject * const token = [super beginCacheUpdateForGetter:getter];
    @synchronized (self) {
        if (_pendingQueries == nil)
            _pendingQueries = NSMutableSet.set;
        [_pendingQueries addObject:token];
        if (_queryCounts == nil)
            _queryCounts = NSMutableDictionary.dictionary;
        NSValue * const key = [NSValue valueWithPointer:getter];
        _queryCounts[key] = @(_queryCounts[key].unsignedIntegerValue + 1);
        // The test requests its refresh on main. Lazy background loads cannot
        // consume the gate, even when they query the same cache.
        if (_getter == getter && _heldToken == nil && (NSThread.isMainThread || _captureBackgroundQuery)) {
            _heldToken = token;
            _captureBackgroundQuery = NO;
        }
        if (_writeGetter == getter && _writeToken == nil && NSThread.isMainThread) {
            _writeToken = token;
        }
    }
    return token;
}

- (NSUInteger)queryCountForGetter:(SEL)getter
{
    @synchronized (self) {
        return _queryCounts[[NSValue valueWithPointer:getter]].unsignedIntegerValue;
    }
}

- (void)finishPendingQueriesWithCompletion:(dispatch_block_t)completion
{
    // Startup counts are not cache-refresh tokens. Their final native read must
    // also finish before the fixture destroys the medialibrary.
    [self waitForInitialCounts];
    // Drain submitted event handlers first: they can start background refreshes.
    (void)self.cachedAudioMedia;
    (void)self.cachedAlbums;
    (void)self.cachedArtists;
    (void)self.cachedGenres;
    (void)self.cachedListOfGroups;
    @synchronized (self) {
        if (_pendingQueries.count == 0)
            dispatch_async(dispatch_get_main_queue(), completion);
        else
            _queriesFinished = [completion copy];
    }
}

- (void)refreshCacheForGetter:(SEL)getter
{
    if (getter == @selector(cachedAudioMedia))
        [self resetCachedListOfAudioMedia];
    else if (getter == @selector(cachedVideoMedia))
        [self resetCachedListOfVideoMedia];
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
    else if (getter == @selector(cachedListOfMovies))
        [self resetCachedListOfMovies];
    else if (getter == @selector(cachedRecentAudioMedia))
        [self resetCachedListOfRecentAudioMedia];
    else if (getter == @selector(cachedRecentMedia))
        [self resetCachedListOfRecentMedia];
    else
        XCTFail(@"Unsupported cache getter");
}

- (void)holdNextCacheRefreshForGetter:(SEL)getter
                              handler:(dispatch_block_t)handler
                    completionHandler:(dispatch_block_t)completionHandler
{
    [self holdNextQueryResultForGetter:getter
                               handler:handler
                     completionHandler:completionHandler];
    @synchronized (self) {
        _captureBackgroundQuery = YES;
    }
}

- (void)holdNextCacheWriteForGetter:(SEL)getter
                            handler:(dispatch_block_t)handler
                  completionHandler:(dispatch_block_t)completionHandler
{
    @synchronized (self) {
        _writeGetter = getter;
        _writeToken = nil;
        _writeRelease = dispatch_semaphore_create(0);
        _didExecuteHeldCacheWrite = NO;
        _writeCaptured = [handler copy];
        _writeProcessed = [completionHandler copy];
    }
}

- (void)performCacheWriteForGeneration:(NSUInteger)generation
                                 token:(NSObject *)token
                                getter:(SEL)getter
                               onQueue:(dispatch_queue_t)queue
                                 block:(dispatch_block_t)block
{
    dispatch_block_t resultCaptured = nil;
    dispatch_block_t writeCaptured = nil;
    dispatch_block_t processed = nil;
    dispatch_semaphore_t queryRelease = nil;
    dispatch_semaphore_t writeRelease = nil;
    @synchronized (self) {
        // Native reads and mapping are finished when the model submits its write.
        [_pendingQueries removeObject:token];
        if (_pendingQueries.count == 0 && _queriesFinished != nil) {
            dispatch_async(dispatch_get_main_queue(), _queriesFinished);
            _queriesFinished = nil;
        }
        if (token == _heldToken) {
            resultCaptured = _resultCaptured;
            processed = _queryProcessed;
            queryRelease = _queryRelease;
            _resultCaptured = nil;
            _queryProcessed = nil;
        }
        if (token == _writeToken && _writeCaptured != nil) {
            writeCaptured = _writeCaptured;
            processed = _writeProcessed;
            writeRelease = _writeRelease;
            _writeCaptured = nil;
            _writeProcessed = nil;
        }
    }
    if (resultCaptured != nil) {
        dispatch_async(dispatch_get_main_queue(), resultCaptured);
        if (NSThread.isMainThread) {
            // History-query completions arrive on main. Never block the test's
            // run loop while waiting for it to release the captured result.
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                           ^{
                dispatch_semaphore_wait(queryRelease,
                                        dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
                [super performCacheWriteForGeneration:generation
                                                token:token
                                               getter:getter
                                              onQueue:queue
                                                block:block];
                if (processed != nil) {
                    dispatch_barrier_async(queue,
                                           ^{
                        dispatch_async(dispatch_get_main_queue(), processed);
                    });
                }
            });
            return;
        }
        // Hold the fetched snapshot without blocking the cache queue.
        dispatch_semaphore_wait(queryRelease,
                                dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
    }
    if (writeCaptured != nil) {
        dispatch_barrier_async(queue,
                               ^{
            dispatch_semaphore_wait(writeRelease,
                                    dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
        });
    }
    dispatch_block_t write = block;
    if (writeCaptured != nil) {
        write = ^{
            @synchronized (self) {
                self->_didExecuteHeldCacheWrite = YES;
            }
            block();
        };
    }
    [super performCacheWriteForGeneration:generation
                                    token:token
                                   getter:getter
                                  onQueue:queue
                                    block:write];
    if (writeCaptured != nil) {
        // The real write has been submitted behind the held barrier.
        dispatch_async(dispatch_get_main_queue(), writeCaptured);
    }
    if (processed != nil) {
        dispatch_barrier_async(queue,
                               ^{
            // Runs after the real model accepts or rejects the query write.
            dispatch_async(dispatch_get_main_queue(), processed);
        });
    }
}

- (void)releaseCacheWrite
{
    dispatch_semaphore_signal(_writeRelease);
}

- (BOOL)didExecuteHeldCacheWrite
{
    @synchronized (self) {
        return _didExecuteHeldCacheWrite;
    }
}

- (void)releaseQuery
{
    dispatch_semaphore_signal(_queryRelease);
}

@end

@implementation VLCLibraryModelTestOverlapFixture {
    VLCLibraryModelTestCacheKind _kind;
    int64_t _changedMediaID;
    int64_t _pendingMediaID;
}

- (instancetype)initWithCacheKind:(VLCLibraryModelTestCacheKind)kind
{
    return [self initWithCacheKind:kind groupName:nil otherGroupName:nil];
}

- (instancetype)initWithGroupName:(NSString *)groupName
                  otherGroupName:(NSString *)otherGroupName
{
    return [self initWithCacheKind:VLCLibraryModelTestGroupCache groupName:groupName otherGroupName:otherGroupName];
}

- (instancetype)initWithCacheKind:(VLCLibraryModelTestCacheKind)kind
                        groupName:(NSString *)groupName
                   otherGroupName:(NSString *)otherGroupName
{
    self = [super init];
    if (self == nil)
        return nil;
    _kind = kind;
    VLCMediaLibraryMediaItem * const audio = [VLCMediaLibraryMediaItem mediaItemForLibraryID:
        VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    VLCMediaLibraryMediaItem * const otherAudio = [VLCMediaLibraryMediaItem mediaItemForLibraryID:
        VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID()];
    VLCMediaLibraryMediaItem * const video = [VLCMediaLibraryMediaItem mediaItemForLibraryID:
        VLCLibraryDataTypesIntegrationFactoryVideoMediaID()];
    VLCMediaLibraryMediaItem * const otherVideo = [VLCMediaLibraryMediaItem mediaItemForLibraryID:
        VLCLibraryDataTypesIntegrationSortingVideoAlphaMediaID()];
    switch (kind) {
        case VLCLibraryModelTestAudioCache:
        case VLCLibraryModelTestRecentAudioCache:
            _pendingItem = audio;
            _changedItem = otherAudio;
            break;
        case VLCLibraryModelTestVideoCache:
        case VLCLibraryModelTestRecentVideoCache:
            _pendingItem = video;
            _changedItem = otherVideo;
            break;
        case VLCLibraryModelTestArtistCache:
            _pendingItem = [VLCMediaLibraryArtist artistWithID:audio.artistID];
            _changedItem = [VLCMediaLibraryArtist artistWithID:otherAudio.artistID];
            break;
        case VLCLibraryModelTestAlbumCache:
            _pendingItem = [VLCMediaLibraryAlbum albumWithID:audio.albumID];
            _changedItem = [VLCMediaLibraryAlbum albumWithID:otherAudio.albumID];
            break;
        case VLCLibraryModelTestGenreCache:
            _pendingItem = [VLCMediaLibraryGenre genreWithID:audio.genreID];
            _changedItem = [VLCMediaLibraryGenre genreWithID:otherAudio.genreID];
            break;
        case VLCLibraryModelTestGroupCache: {
            if (groupName != nil && otherGroupName != nil) {
                _pendingItem = [VLCMediaLibraryGroup groupWithID:
                    VLCLibraryDataTypesIntegrationCreateNamedGroup(video.libraryID, groupName)];
                _changedItem = [VLCMediaLibraryGroup groupWithID:
                    VLCLibraryDataTypesIntegrationCreateNamedGroup(otherVideo.libraryID, otherGroupName)];
                break;
            }
            _pendingItem = [VLCMediaLibraryGroup groupWithID:VLCLibraryDataTypesIntegrationGroupID()];
            for (id<VLCMediaLibraryItemProtocol> item in self.libraryItems) {
                if (item.libraryID != _pendingItem.libraryID) {
                    _changedItem = item;
                    break;
                }
            }
            break;
        }
        case VLCLibraryModelTestShowCache: {
            const int64_t showID = VLCLibraryDataTypesIntegrationCreateScannedShow(@"Overlap New Show");
            _pendingItem = [VLCMediaLibraryShow showWithLibraryId:showID];
            _pendingMediaID = _pendingItem.firstMediaItem.libraryID;
            _changedItem = [VLCMediaLibraryShow showWithLibraryId:VLCLibraryDataTypesIntegrationShowID()];
            _changedMediaID = video.libraryID;
            break;
        }
        case VLCLibraryModelTestMovieCache: {
            if (!VLCLibraryDataTypesIntegrationPrepareMovieFixture() ||
                !VLCLibraryDataTypesIntegrationConvertMediaToMovie(otherVideo.libraryID))
                return nil;
            for (id<VLCMediaLibraryItemProtocol> movie in self.libraryItems) {
                if (movie.libraryID == VLCLibraryDataTypesIntegrationFactoryMovieMediaID())
                    _pendingItem = movie;
                else if (movie.libraryID == otherVideo.libraryID)
                    _changedItem = movie;
            }
            break;
        }
    }
    if (_pendingItem == nil || _changedItem == nil || _pendingItem.libraryID == _changedItem.libraryID)
        return nil;
    if (kind == VLCLibraryModelTestRecentAudioCache || kind == VLCLibraryModelTestRecentVideoCache) {
        vlc_medialibrary_t * const library = VLCLibraryDataTypesIntegrationMediaLibrary();
        if (vlc_ml_clear_history(library, VLC_ML_HISTORY_TYPE_GLOBAL) != VLC_SUCCESS ||
            vlc_ml_media_set_played(library, _changedItem.libraryID, true) != VLC_SUCCESS)
            return nil;
    }
    _model = [[VLCLibraryModelTestQueryGate alloc] initWithLibrary:VLCLibraryDataTypesIntegrationMediaLibrary()
                                                  observeEvents:NO];
    _model.changeDelegate.throttleInterval = 0.;
    // Replay events in the test's chosen order. Queries still go to the real ML.
    return self;
}

- (SEL)cacheGetter
{
    switch (_kind) {
        case VLCLibraryModelTestAudioCache:
            return @selector(cachedAudioMedia);
        case VLCLibraryModelTestVideoCache:
            return @selector(cachedVideoMedia);
        case VLCLibraryModelTestArtistCache:
            return @selector(cachedArtists);
        case VLCLibraryModelTestAlbumCache:
            return @selector(cachedAlbums);
        case VLCLibraryModelTestGenreCache:
            return @selector(cachedGenres);
        case VLCLibraryModelTestGroupCache:
            return @selector(cachedListOfGroups);
        case VLCLibraryModelTestShowCache:
            return @selector(cachedListOfShows);
        case VLCLibraryModelTestMovieCache:
            return @selector(cachedListOfMovies);
        case VLCLibraryModelTestRecentAudioCache:
            return @selector(cachedRecentAudioMedia);
        case VLCLibraryModelTestRecentVideoCache:
            return @selector(cachedRecentMedia);
    }
}

- (NSArray<NSNotificationName> *)changeNotifications
{
    switch (_kind) {
        case VLCLibraryModelTestAudioCache:
            return @[ VLCLibraryModelAudioMediaListReset, VLCLibraryModelAudioMediaItemUpdated, VLCLibraryModelAudioMediaItemDeleted ];
        case VLCLibraryModelTestVideoCache:
            return @[ VLCLibraryModelVideoMediaListReset, VLCLibraryModelVideoMediaItemUpdated, VLCLibraryModelVideoMediaItemDeleted ];
        case VLCLibraryModelTestArtistCache:
            return @[ VLCLibraryModelArtistListReset, VLCLibraryModelArtistUpdated, VLCLibraryModelArtistDeleted ];
        case VLCLibraryModelTestAlbumCache:
            return @[ VLCLibraryModelAlbumListReset, VLCLibraryModelAlbumUpdated, VLCLibraryModelAlbumDeleted ];
        case VLCLibraryModelTestGenreCache:
            return @[ VLCLibraryModelGenreListReset, VLCLibraryModelGenreUpdated, VLCLibraryModelGenreDeleted ];
        case VLCLibraryModelTestGroupCache:
            return @[ VLCLibraryModelListOfGroupsReset, VLCLibraryModelGroupUpdated, VLCLibraryModelGroupDeleted ];
        case VLCLibraryModelTestShowCache:
            return @[ VLCLibraryModelListOfShowsReset, VLCLibraryModelShowUpdated, VLCLibraryModelShowDeleted ];
        case VLCLibraryModelTestMovieCache:
            return @[ VLCLibraryModelListOfMoviesReset ];
        case VLCLibraryModelTestRecentAudioCache:
            return @[ VLCLibraryModelRecentAudioMediaListReset, VLCLibraryModelRecentAudioMediaItemUpdated, VLCLibraryModelRecentAudioMediaItemDeleted ];
        case VLCLibraryModelTestRecentVideoCache:
            return @[ VLCLibraryModelRecentsMediaListReset, VLCLibraryModelRecentsMediaItemUpdated, VLCLibraryModelRecentsMediaItemDeleted ];
    }
}

- (NSArray<id<VLCMediaLibraryItemProtocol>> *)cachedItems
{
    switch (_kind) {
        case VLCLibraryModelTestAudioCache:
            return _model.cachedAudioMedia;
        case VLCLibraryModelTestVideoCache:
            return _model.cachedVideoMedia;
        case VLCLibraryModelTestArtistCache:
            return _model.cachedArtists;
        case VLCLibraryModelTestAlbumCache:
            return _model.cachedAlbums;
        case VLCLibraryModelTestGenreCache:
            return _model.cachedGenres;
        case VLCLibraryModelTestGroupCache:
            return _model.cachedListOfGroups;
        case VLCLibraryModelTestShowCache:
            return _model.cachedListOfShows;
        case VLCLibraryModelTestMovieCache:
            return _model.cachedListOfMovies;
        case VLCLibraryModelTestRecentAudioCache:
            return _model.cachedRecentAudioMedia;
        case VLCLibraryModelTestRecentVideoCache:
            return _model.cachedRecentMedia;
    }
}

- (void)seedCacheWithItems:(NSArray<id<VLCMediaLibraryItemProtocol>> *)items
{
    switch (_kind) {
        case VLCLibraryModelTestAudioCache:
            _model.cachedAudioMedia = (id)items;
            break;
        case VLCLibraryModelTestVideoCache:
            _model.cachedVideoMedia = (id)items;
            break;
        case VLCLibraryModelTestArtistCache:
            _model.cachedArtists = (id)items;
            break;
        case VLCLibraryModelTestAlbumCache:
            _model.cachedAlbums = (id)items;
            break;
        case VLCLibraryModelTestGenreCache:
            _model.cachedGenres = (id)items;
            break;
        case VLCLibraryModelTestGroupCache:
            _model.cachedListOfGroups = (id)items;
            break;
        case VLCLibraryModelTestShowCache:
            _model.cachedListOfShows = (id)items;
            break;
        case VLCLibraryModelTestMovieCache:
            _model.cachedListOfMovies = (id)items;
            break;
        case VLCLibraryModelTestRecentAudioCache:
            _model.cachedRecentAudioMedia = (id)items;
            break;
        case VLCLibraryModelTestRecentVideoCache:
            _model.cachedRecentMedia = (id)items;
            break;
    }
}

- (NSArray<id<VLCMediaLibraryItemProtocol>> *)libraryItems
{
    vlc_medialibrary_t * const library = VLCLibraryDataTypesIntegrationMediaLibrary();
    vlc_ml_query_params_t params = _model == nil
        ? (vlc_ml_query_params_t){0}
        : [_model queryParams];
    if (_kind == VLCLibraryModelTestRecentAudioCache || _kind == VLCLibraryModelTestRecentVideoCache) {
        params.i_sort = VLC_ML_SORTING_DEFAULT;
        params.b_desc = true;
        params.i_nbResults = _kind == VLCLibraryModelTestRecentAudioCache
            ? _model.recentAudioMediaLimit
            : _model.recentMediaLimit;
    }
    NSMutableArray * const items = NSMutableArray.array;
    switch (_kind) {
        case VLCLibraryModelTestAudioCache:
        case VLCLibraryModelTestVideoCache:
        case VLCLibraryModelTestRecentAudioCache:
        case VLCLibraryModelTestRecentVideoCache: {
            vlc_ml_media_list_t *list;
            if (_kind == VLCLibraryModelTestAudioCache)
                list = vlc_ml_list_audio_media(library, &params);
            else if (_kind == VLCLibraryModelTestVideoCache)
                list = vlc_ml_list_video_media(library, &params);
            else if (_kind == VLCLibraryModelTestRecentAudioCache)
                list = vlc_ml_list_audio_history(library, &params);
            else
                list = vlc_ml_list_video_history(library, &params);
            if (list == NULL)
                return nil;
            NSArray * const media = [NSArray arrayFromVlcMediaList:list];
            vlc_ml_media_list_release(list);
            return media;
        }
        case VLCLibraryModelTestArtistCache: {
            vlc_ml_artist_list_t * const list = vlc_ml_list_artists(library, &params, YES);
            if (list == NULL)
                return nil;
            for (size_t i = 0; i < list->i_nb_items; i++)
                [items addObject:[[VLCMediaLibraryArtist alloc] initWithArtist:&list->p_items[i]]];
            vlc_ml_artist_list_release(list);
            break;
        }
        case VLCLibraryModelTestAlbumCache: {
            vlc_ml_album_list_t * const list = vlc_ml_list_albums(library, &params);
            if (list == NULL)
                return nil;
            for (size_t i = 0; i < list->i_nb_items; i++)
                [items addObject:[[VLCMediaLibraryAlbum alloc] initWithAlbum:&list->p_items[i]]];
            vlc_ml_album_list_release(list);
            break;
        }
        case VLCLibraryModelTestGenreCache: {
            vlc_ml_genre_list_t * const list = vlc_ml_list_genres(library, &params);
            if (list == NULL)
                return nil;
            for (size_t i = 0; i < list->i_nb_items; i++)
                [items addObject:[[VLCMediaLibraryGenre alloc] initWithGenre:&list->p_items[i]]];
            vlc_ml_genre_list_release(list);
            break;
        }
        case VLCLibraryModelTestGroupCache: {
            vlc_ml_group_list_t * const list = vlc_ml_list_groups(library, &params);
            if (list == NULL)
                return nil;
            for (size_t i = 0; i < list->i_nb_items; i++)
                [items addObject:[[VLCMediaLibraryGroup alloc] initWithGroup:&list->p_items[i]]];
            vlc_ml_group_list_release(list);
            break;
        }
        case VLCLibraryModelTestShowCache: {
            vlc_ml_show_list_t * const list = vlc_ml_list_shows(library, NULL);
            if (list == NULL)
                return nil;
            for (size_t i = 0; i < list->i_nb_items; i++)
                [items addObject:[[VLCMediaLibraryShow alloc] initWithShow:&list->p_items[i]]];
            vlc_ml_show_list_release(list);
            break;
        }
        case VLCLibraryModelTestMovieCache: {
            vlc_ml_media_list_t * const list = vlc_ml_list_movies(library, NULL);
            if (list == NULL)
                return nil;
            for (size_t i = 0; i < list->i_nb_items; i++)
                [items addObject:[[VLCMediaLibraryMovie alloc] initWithMediaItem:&list->p_items[i]]];
            vlc_ml_media_list_release(list);
            break;
        }
    }
    return items.copy;
}

- (BOOL)preparePendingChange
{
    if (_kind == VLCLibraryModelTestRecentAudioCache || _kind == VLCLibraryModelTestRecentVideoCache)
        return vlc_ml_media_set_played(VLCLibraryDataTypesIntegrationMediaLibrary(),
                                       _pendingItem.libraryID,
                                       true) == VLC_SUCCESS;
    // Other caches hold the real pre-add snapshot. The added record is already
    // persisted; only delivery of its refresh event remains pending.
    return YES;
}

- (void)refreshCache
{
    // Exercise one refresh in isolation; native callback fan-out is covered by
    // the event suite rather than leaving unrelated queries running at teardown.
    [_model refreshCacheForGetter:self.cacheGetter];
}

- (void)deliverRefreshEvent
{
    switch (_kind) {
        case VLCLibraryModelTestAudioCache:
        case VLCLibraryModelTestVideoCache:
        case VLCLibraryModelTestMovieCache:
        case VLCLibraryModelTestShowCache: {
            const int64_t mediaID = _kind == VLCLibraryModelTestShowCache
                ? _pendingItem.firstMediaItem.libraryID
                : _pendingItem.libraryID;
            vlc_ml_media_t * const media = vlc_ml_get_media(VLCLibraryDataTypesIntegrationMediaLibrary(),
                                                            mediaID);
            vlc_ml_event_t event = { .i_type = VLC_ML_EVENT_MEDIA_ADDED, .creation.p_media = media };
            [_model handleMediaItemAddedEvent:&event];
            vlc_ml_media_release(media);
            break;
        }
        // These are the same refresh calls made by libraryCallback for additions
        // and HISTORY_CHANGED; only their delivery order is controlled here.
        case VLCLibraryModelTestArtistCache:
            [_model resetCachedListOfArtists];
            break;
        case VLCLibraryModelTestAlbumCache:
            [_model resetCachedListOfAlbums];
            break;
        case VLCLibraryModelTestGenreCache:
            [_model resetCachedListOfGenres];
            break;
        case VLCLibraryModelTestGroupCache:
            [_model resetCachedListOfGroups];
            break;
        case VLCLibraryModelTestRecentAudioCache:
            [_model resetCachedListOfRecentAudioMedia];
            break;
        case VLCLibraryModelTestRecentVideoCache:
            [_model resetCachedListOfRecentMedia];
            break;
    }
}

- (BOOL)updateChangedItem
{
    if (_kind == VLCLibraryModelTestGroupCache)
        return VLCLibraryDataTypesIntegrationRenameGroup(_changedItem.libraryID, @"Overlap Updated Group");
    if (_kind == VLCLibraryModelTestShowCache)
        return VLCLibraryDataTypesIntegrationSetShowSummary(_changedItem.libraryID, @"Overlap Updated Show") &&
            vlc_ml_media_set_favorite(VLCLibraryDataTypesIntegrationMediaLibrary(),
                                      _changedMediaID,
                                      true) == VLC_SUCCESS;
    return [_changedItem setFavorite:YES] == VLC_SUCCESS;
}

- (BOOL)setChangedItemFavorite:(BOOL)favorite
{
    return [_changedItem setFavorite:favorite] == VLC_SUCCESS;
}

- (BOOL)setChangedItemPresent:(BOOL)present
{
    NSArray<VLCMediaLibraryMediaItem *> * const media = _changedItem.mediaItems;
    if (media.count == 0)
        return NO;
    for (VLCMediaLibraryMediaItem *item in media) {
        if (!VLCLibraryDataTypesIntegrationSetMediaPresent(item.libraryID, present))
            return NO;
    }
    return YES;
}

- (id<VLCMediaLibraryItemProtocol>)refetchChangedItem
{
    const int64_t libraryID = _changedItem.libraryID;
    switch (_kind) {
        case VLCLibraryModelTestArtistCache:
            return [VLCMediaLibraryArtist artistWithID:libraryID];
        case VLCLibraryModelTestAlbumCache:
            return [VLCMediaLibraryAlbum albumWithID:libraryID];
        case VLCLibraryModelTestGenreCache:
            return [VLCMediaLibraryGenre genreWithID:libraryID];
        case VLCLibraryModelTestGroupCache:
            return [VLCMediaLibraryGroup groupWithID:libraryID];
        case VLCLibraryModelTestShowCache:
            return [VLCMediaLibraryShow showWithLibraryId:libraryID];
        case VLCLibraryModelTestMovieCache:
            return [VLCMediaLibraryMediaItem mediaItemForLibraryID:libraryID].movie;
        default:
            return [VLCMediaLibraryMediaItem mediaItemForLibraryID:libraryID];
    }
}

- (enum vlc_ml_event_type)deletionEventType
{
    switch (_kind) {
        case VLCLibraryModelTestArtistCache:
            return VLC_ML_EVENT_ARTIST_DELETED;
        case VLCLibraryModelTestAlbumCache:
            return VLC_ML_EVENT_ALBUM_DELETED;
        case VLCLibraryModelTestGenreCache:
            return VLC_ML_EVENT_GENRE_DELETED;
        case VLCLibraryModelTestGroupCache:
            return VLC_ML_EVENT_GROUP_DELETED;
        default:
            return VLC_ML_EVENT_MEDIA_DELETED;
    }
}

- (BOOL)deleteChangedItem
{
    return VLCLibraryDataTypesIntegrationDeleteEntity(self.deletionEventType,
                                                      _kind == VLCLibraryModelTestShowCache
                                                          ? _changedMediaID
                                                          : _changedItem.libraryID);
}

- (BOOL)deletePendingItem
{
    return VLCLibraryDataTypesIntegrationDeleteEntity(self.deletionEventType,
                                                      _kind == VLCLibraryModelTestShowCache
                                                          ? _pendingMediaID
                                                          : _pendingItem.libraryID);
}

- (void)deliverUpdateEvent
{
    [self deliverUpdateEventForID:_kind == VLCLibraryModelTestShowCache
        ? _changedMediaID
        : _changedItem.libraryID];
}

- (void)deliverUpdateEventForID:(int64_t)libraryID
{
    vlc_ml_event_t event = { .modification.i_entity_id = libraryID };
    switch (_kind) {
        case VLCLibraryModelTestArtistCache:
            event.i_type = VLC_ML_EVENT_ARTIST_UPDATED;
            [_model handleArtistUpdateEvent:&event];
            break;
        case VLCLibraryModelTestAlbumCache:
            event.i_type = VLC_ML_EVENT_ALBUM_UPDATED;
            [_model handleAlbumUpdateEvent:&event];
            break;
        case VLCLibraryModelTestGenreCache:
            event.i_type = VLC_ML_EVENT_GENRE_UPDATED;
            [_model handleGenreUpdateEvent:&event];
            break;
        case VLCLibraryModelTestGroupCache:
            event.i_type = VLC_ML_EVENT_GROUP_UPDATED;
            [_model handleGroupUpdateEvent:&event];
            break;
        default:
            event.i_type = VLC_ML_EVENT_MEDIA_UPDATED;
            [_model handleMediaItemUpdateEvent:&event];
            break;
    }
}

- (BOOL)renameItemWithID:(int64_t)libraryID
                    to:(NSString *)name
{
    switch (_kind) {
        case VLCLibraryModelTestArtistCache:
            return VLCLibraryDataTypesIntegrationRenameArtist(libraryID, name);
        case VLCLibraryModelTestAlbumCache:
            return VLCLibraryDataTypesIntegrationRenameAlbum(libraryID, name);
        case VLCLibraryModelTestGenreCache:
            return VLCLibraryDataTypesIntegrationRenameGenre(libraryID, name);
        case VLCLibraryModelTestGroupCache:
            return VLCLibraryDataTypesIntegrationRenameGroup(libraryID, name);
        default:
            return VLCLibraryDataTypesIntegrationRenameMedia(libraryID, name);
    }
}

- (void)deliverDeletionEventForID:(int64_t)libraryID
{
    vlc_ml_event_t event = {
        .i_type = self.deletionEventType,
        .deletion.i_entity_id = libraryID
    };
    switch (_kind) {
        case VLCLibraryModelTestArtistCache:
            [_model handleArtistDeletionEvent:&event];
            break;
        case VLCLibraryModelTestAlbumCache:
            [_model handleAlbumDeletionEvent:&event];
            break;
        case VLCLibraryModelTestGenreCache:
            [_model handleGenreDeletionEvent:&event];
            break;
        case VLCLibraryModelTestGroupCache:
            [_model handleGroupDeletionEvent:&event];
            break;
        default:
            [_model handleMediaItemDeletionEvent:&event];
            break;
    }
}

- (void)deliverDeletionEvent
{
    [self deliverDeletionEventForID:
        _kind == VLCLibraryModelTestShowCache
            ? _changedMediaID
            : _changedItem.libraryID];
}

- (void)deliverPendingDeletionEvent
{
    [self deliverDeletionEventForID:
        _kind == VLCLibraryModelTestShowCache
            ? _pendingMediaID
            : _pendingItem.libraryID];
}

@end
