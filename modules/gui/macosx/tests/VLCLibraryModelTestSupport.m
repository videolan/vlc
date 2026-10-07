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
    if (!_loadsInitialCounts)
        return YES;
    // Read the real startup counter through its public getter, without allowing
    // a lazy cache query to satisfy the condition instead of the startup worker.
    @synchronized (self) {
        _initialCountReadThread = NSThread.currentThread;
        const size_t count = [super numberOfRecentAudioMedia];
        _initialCountReadThread = nil;
        return count > 0;
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
