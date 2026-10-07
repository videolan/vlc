/****************************************************************************
 * VLCLibraryModelConcurrencyTest.m: cache ordering and publication regressions
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

#import <XCTest/XCTest.h>
#import "VLCLibraryModelTestSupport.h"
#import "VLCLibraryDataTypesIntegrationTestSupport.h"

typedef NS_ENUM(NSUInteger, VLCLibraryModelSupersedingAction) {
    VLCLibraryModelSupersedingReset,
    VLCLibraryModelSupersedingFilter,
    VLCLibraryModelSupersedingSort,
    VLCLibraryModelSupersedingDrop,
};

@interface VLCLibraryModelFixtureTestCase : XCTestCase
@property (strong) VLCLibraryModelTestOverlapFixture *fixture;
@end

@implementation VLCLibraryModelFixtureTestCase

- (void)setUp
{
    [super setUp];
    self.continueAfterFailure = NO;
    // Mutating tests need isolated records, including after deletions.
    XCTestExpectation * const ready = [self expectationWithDescription:@"Temporary library ready"];
    __block BOOL prepared = NO;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^{
        prepared = VLCLibraryDataTypesIntegrationStart() &&
                   VLCLibraryDataTypesIntegrationPrepareFactoryFixtures();
        [ready fulfill];
    });
    [self waitForExpectations:@[ ready ] timeout:20.];
    XCTAssertTrue(prepared);
}

- (void)tearDown
{
    // XCTest runs the registered gate/observer teardown blocks first.
    if (self.fixture != nil) {
        XCTestExpectation * const finished = [self expectationWithDescription:@"Native reads finished before library shutdown"];
        [self.fixture.model finishPendingQueriesWithCompletion:^{
            [finished fulfill];
        }];
        [self waitForExpectations:@[ finished ] timeout:10.];
    }
    self.fixture = nil;
    VLCLibraryDataTypesIntegrationStop();
    [super tearDown];
}

- (NSArray<NSNumber *> *)orderedIDs:(NSArray<id<VLCMediaLibraryItemProtocol>> *)items
{
    NSMutableArray<NSNumber *> * const result = NSMutableArray.array;
    for (id<VLCMediaLibraryItemProtocol> item in items)
        [result addObject:@(item.libraryID)];
    return result.copy;
}

- (id<VLCMediaLibraryItemProtocol>)itemWithID:(int64_t)libraryID
                                      inItems:(NSArray<id<VLCMediaLibraryItemProtocol>> *)items
{
    for (id<VLCMediaLibraryItemProtocol> item in items) {
        if (item.libraryID == libraryID)
            return item;
    }
    return nil;
}

- (void)observeNotification:(NSNotificationName)name
                     object:(id)object
                      queue:(NSOperationQueue *)queue
                 usingBlock:(void (^)(NSNotification *notification))block
{
    id const observer = [NSNotificationCenter.defaultCenter addObserverForName:name
                                                                        object:object
                                                                         queue:queue
                                                                    usingBlock:block];
    [self addTeardownBlock:^{
        [NSNotificationCenter.defaultCenter removeObserver:observer];
    }];
}

@end

@interface VLCLibraryModelConcurrencyTest : VLCLibraryModelFixtureTestCase
@end

@implementation VLCLibraryModelConcurrencyTest

- (void)prepareCacheKind:(VLCLibraryModelTestCacheKind)kind
{
    XCTestExpectation * const ready = [self expectationWithDescription:@"Cache fixture prepared"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^{
        self.fixture = [[VLCLibraryModelTestOverlapFixture alloc] initWithCacheKind:kind];
        [ready fulfill];
    });
    [self waitForExpectations:@[ ready ] timeout:15.];
    XCTAssertNotNil(self.fixture);
    XCTAssertTrue([self.fixture preparePendingChange]);
    if (kind <= VLCLibraryModelTestGroupCache) {
        XCTestExpectation * const dropped = [self expectationForNotification:VLCLibraryModelAllCachesDropped
                                                                      object:self.fixture.model
                                                                     handler:nil];
        [self.fixture.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:NO];
        [self waitForExpectations:@[ dropped ] timeout:5.];
    }
    if (kind == VLCLibraryModelTestShowCache) {
        XCTestExpectation * const seeded = [self expectationWithDescription:@"Show membership mapped"];
        XCTestExpectation * const written = [self expectationWithDescription:@"Initial show cache written"];
        VLCLibraryModelTestQueryGate * const model = self.fixture.model;
        [model holdNextQueryResultForGetter:self.fixture.cacheGetter
                                    handler:^{
            [seeded fulfill];
        }
                          completionHandler:^{
            [written fulfill];
        }];
        [self addTeardownBlock:^{
            [model releaseQuery];
        }];
        [self.fixture seedCacheWithItems:self.fixture.libraryItems];
        [self waitForExpectations:@[ seeded ] timeout:5.];
        [self.fixture.model releaseQuery];
        [self waitForExpectations:@[ written ] timeout:5.];
    } else {
        // Start with a settled real snapshot, without carrying a prior reset's
        // throttle timer into the notification-ordering assertions below.
        [self.fixture seedCacheWithItems:self.fixture.libraryItems];
    }
    XCTAssertNotNil(self.fixture.cachedItems);
}

- (void)assertSupersededResultForCacheKind:(VLCLibraryModelTestCacheKind)kind
                                    action:(VLCLibraryModelSupersedingAction)action
                               queuedWrite:(BOOL)queuedWrite
{
    [self prepareCacheKind:kind];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    VLCLibraryModelTestQueryGate * const model = fixture.model;
    NSArray<NSNumber *> * const oldIDs = [self orderedIDs:fixture.cachedItems];
    XCTestExpectation * const captured = [self expectationWithDescription:@"Older result held"];
    XCTestExpectation * const processed = [self expectationWithDescription:@"Older write processed"];
    if (queuedWrite) {
        [model holdNextCacheWriteForGetter:fixture.cacheGetter
                                   handler:^{
            [captured fulfill];
        }
                         completionHandler:^{
            [processed fulfill];
        }];
        [self addTeardownBlock:^{
            [model releaseCacheWrite];
        }];
    } else {
        [model holdNextQueryResultForGetter:fixture.cacheGetter
                                    handler:^{
            [captured fulfill];
        }
                          completionHandler:^{
            [processed fulfill];
        }];
        [self addTeardownBlock:^{
            [model releaseQuery];
        }];
    }
    __block NSUInteger resetCount = 0;
    XCTestExpectation * const extraReset = [self expectationWithDescription:@"No reset from rejected work or duplicate publication"];
    extraReset.inverted = YES;
    [self observeNotification:fixture.changeNotifications.firstObject
                       object:model
                        queue:nil
                   usingBlock:^(NSNotification *notification) {
        XCTAssertTrue(NSThread.isMainThread);
        XCTAssertEqual(notification.object, model);
        resetCount++;
        if (resetCount > (action == VLCLibraryModelSupersedingDrop ? 0 : 1))
            [extraReset fulfill];
    }];
    [fixture refreshCache];
    [self waitForExpectations:@[ captured ] timeout:5.];
    XCTestExpectation *dropped = nil;
    if (action != VLCLibraryModelSupersedingReset) {
        dropped = [self expectationForNotification:VLCLibraryModelAllCachesDropped object:model handler:nil];
    }
    switch (action) {
        case VLCLibraryModelSupersedingReset:
            XCTAssertTrue([fixture deleteChangedItem]);
            break;
        case VLCLibraryModelSupersedingFilter:
            model.filterString = fixture.changedItem.displayString;
            break;
        case VLCLibraryModelSupersedingSort:
            [model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];
            break;
        case VLCLibraryModelSupersedingDrop:
            [model dropCaches];
            break;
    }
    if (dropped != nil && !queuedWrite)
        [self waitForExpectations:@[ dropped ] timeout:5.];

    XCTestExpectation *replacement = nil;
    if (action != VLCLibraryModelSupersedingDrop) {
        replacement = [self expectationForNotification:fixture.changeNotifications.firstObject object:model handler:nil];
        [fixture refreshCache];
        if (!queuedWrite)
            [self waitForExpectations:@[ replacement ] timeout:5.];
    }
    if (queuedWrite)
        [model releaseCacheWrite];
    else
        [model releaseQuery];
    [self waitForExpectations:@[ processed ] timeout:5.];
    if (queuedWrite && dropped != nil)
        [self waitForExpectations:@[ dropped ] timeout:5.];
    if (queuedWrite && replacement != nil)
        [self waitForExpectations:@[ replacement ] timeout:5.];
    // Model-wide notifications use a run-loop timer even at zero throttle.
    // Observe that publication turn as well as the immediate write callback.
    [self waitForExpectations:@[ extraReset ] timeout:.05];
    if (queuedWrite)
        XCTAssertFalse(model.didExecuteHeldCacheWrite, @"Reject obsolete work even after its barrier was submitted");

    if (action == VLCLibraryModelSupersedingDrop) {
        XCTAssertNil(fixture.cachedItems);
        XCTAssertEqual(resetCount, (NSUInteger)0, @"Rejected results must not emit reset notifications");
    } else {
        NSArray<NSNumber *> * const expected = [self orderedIDs:fixture.libraryItems];
        XCTAssertNotEqualObjects(expected, oldIDs, @"The replacement must really change membership or order");
        XCTAssertEqualObjects([self orderedIDs:fixture.cachedItems], expected);
        XCTAssertEqual(resetCount, (NSUInteger)1, @"Only the accepted replacement may publish a reset");
    }
}

- (void)assertPendingRenameForCacheKind:(VLCLibraryModelTestCacheKind)kind
                       followedByDelete:(BOOL)deleteItem
{
    [self prepareCacheKind:kind];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    VLCLibraryModelTestQueryGate * const model = fixture.model;
    NSArray<NSNumber *> * const originalOrder = [self orderedIDs:fixture.cachedItems];
    const int64_t renamedID = fixture.changedItem.libraryID;
    const int64_t otherID = fixture.pendingItem.libraryID;
    NSString * const newName = [fixture.changedItem.displayString compare:fixture.pendingItem.displayString] == NSOrderedAscending
        ? @"ZZZ Pending Rename"
        : @"AAA Pending Rename";
    XCTAssertTrue([fixture renameItemWithID:renamedID to:newName]);
    XCTAssertNotEqualObjects([self orderedIDs:fixture.libraryItems], originalOrder);

    XCTestExpectation * const captured = [self expectationWithDescription:@"Rename's reordered result held"];
    XCTestExpectation * const processed = [self expectationWithDescription:@"Superseded rename result discarded"];
    [model holdNextCacheRefreshForGetter:fixture.cacheGetter
                                 handler:^{
        [captured fulfill];
    }
                       completionHandler:^{
        [processed fulfill];
    }];
    [self addTeardownBlock:^{
        [model releaseQuery];
    }];
    [fixture deliverUpdateEvent];
    [self waitForExpectations:@[ captured ] timeout:5.];
    XCTestExpectation * const reconciled = [self expectationForNotification:fixture.changeNotifications.firstObject
                                                                     object:model
                                                                    handler:nil];
    if (deleteItem) {
        XCTAssertTrue([fixture deletePendingItem]);
        [fixture deliverPendingDeletionEvent];
    } else {
        XCTAssertEqual([fixture.pendingItem setFavorite:YES], VLC_SUCCESS);
        [fixture deliverUpdateEventForID:otherID];
    }
    [self waitForExpectations:@[ reconciled ] timeout:5.];
    [model releaseQuery];
    [self waitForExpectations:@[ processed ] timeout:5.];

    NSArray<id<VLCMediaLibraryItemProtocol>> * const actual = fixture.cachedItems;
    NSArray<id<VLCMediaLibraryItemProtocol>> * const persisted = fixture.libraryItems;
    XCTAssertEqualObjects([self orderedIDs:actual], [self orderedIDs:persisted]);
    XCTAssertEqualObjects([self itemWithID:renamedID inItems:actual].displayString, newName);
    if (deleteItem) {
        XCTAssertNil([self itemWithID:otherID inItems:actual]);
        XCTAssertNil([self itemWithID:otherID inItems:persisted]);
    } else {
        XCTAssertTrue([self itemWithID:otherID inItems:persisted].favorited);
        XCTAssertTrue([self itemWithID:otherID inItems:actual].favorited);
    }
}

- (void)assertBackToBackMutationsForCacheKind:(VLCLibraryModelTestCacheKind)kind
                            deletingOtherItem:(BOOL)deleteItem
{
    [self prepareCacheKind:kind];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    VLCLibraryModelTestQueryGate * const model = fixture.model;
    const int64_t changedID = fixture.changedItem.libraryID;
    const int64_t otherID = fixture.pendingItem.libraryID;
    NSArray<NSNumber *> * const originalOrder = [self orderedIDs:fixture.cachedItems];
    XCTestExpectation * const held = [self expectationWithDescription:@"Owning cache queue held"];
    [model holdWritesFollowingNextCacheReadWithHandler:^{
        [held fulfill];
    }];
    [self addTeardownBlock:^{
        [model releaseCacheReads];
    }];
    // This synchronized read installs the barrier gate before either event.
    XCTAssertNotNil(fixture.cachedItems);
    [self waitForExpectations:@[ held ] timeout:5.];
    XCTAssertTrue([fixture setChangedItemFavorite:YES]);
    NSDate * const firstDate = [NSDate dateWithTimeIntervalSince1970:123456];
    NSDate * const secondDate = [NSDate dateWithTimeIntervalSince1970:234567];
    if (kind == VLCLibraryModelTestGroupCache)
        XCTAssertTrue(VLCLibraryDataTypesIntegrationSetGroupModificationDate(changedID, firstDate));
    if (deleteItem)
        XCTAssertTrue([fixture deletePendingItem]);
    else
        XCTAssertEqual([fixture.pendingItem setFavorite:YES], VLC_SUCCESS);
    if (kind == VLCLibraryModelTestGroupCache && !deleteItem)
        XCTAssertTrue(VLCLibraryDataTypesIntegrationSetGroupModificationDate(otherID, secondDate));
    XCTestExpectation * const first = [self expectationForNotification:fixture.changeNotifications[1]
                                                                object:nil
                                                               handler:^BOOL(NSNotification *notification) {
        return [notification.object libraryID] == changedID;
    }];
    XCTestExpectation * const second = [self expectationForNotification:fixture.changeNotifications[deleteItem ? 2 : 1]
                                                                 object:nil
                                                                handler:^BOOL(NSNotification *notification) {
        return [notification.object libraryID] == otherID;
    }];
    [fixture deliverUpdateEvent];
    if (deleteItem)
        [fixture deliverPendingDeletionEvent];
    else
        [fixture deliverUpdateEventForID:otherID];
    // Both handlers are submitted before either can enqueue an old-snapshot setter.
    [model releaseCacheReads];
    [self waitForExpectations:@[ first, second ] timeout:5.];

    NSArray<id<VLCMediaLibraryItemProtocol>> * const actual = fixture.cachedItems;
    NSArray<id<VLCMediaLibraryItemProtocol>> * const persisted = fixture.libraryItems;
    XCTAssertEqualObjects([self orderedIDs:actual], [self orderedIDs:persisted]);
    XCTAssertTrue([self itemWithID:changedID inItems:actual].favorited);
    XCTAssertTrue([self itemWithID:changedID inItems:persisted].favorited);
    if (kind == VLCLibraryModelTestGroupCache) {
        XCTAssertEqualObjects(((VLCMediaLibraryGroup *)[self itemWithID:changedID inItems:actual]).lastModificationDate,
                              firstDate);
        XCTAssertEqualObjects(((VLCMediaLibraryGroup *)[self itemWithID:changedID inItems:persisted]).lastModificationDate,
                              firstDate);
        if (!deleteItem) {
            XCTAssertEqualObjects(((VLCMediaLibraryGroup *)[self itemWithID:otherID inItems:actual]).lastModificationDate,
                                  secondDate);
            XCTAssertEqualObjects(((VLCMediaLibraryGroup *)[self itemWithID:otherID inItems:persisted]).lastModificationDate,
                                  secondDate);
        }
    }
    if (deleteItem) {
        XCTAssertNil([self itemWithID:otherID inItems:actual]);
        XCTAssertNil([self itemWithID:otherID inItems:persisted]);
    } else {
        XCTAssertEqualObjects([self orderedIDs:actual], originalOrder);
        XCTAssertTrue([self itemWithID:otherID inItems:actual].favorited);
        XCTAssertTrue([self itemWithID:otherID inItems:persisted].favorited);
    }
}

- (void)assertResetNotificationWaitsForWriteForCacheKind:(VLCLibraryModelTestCacheKind)kind
{
    [self prepareCacheKind:kind];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    VLCLibraryModelTestQueryGate * const model = fixture.model;
    XCTestExpectation * const captured = [self expectationWithDescription:@"Real write submitted behind held barrier"];
    XCTestExpectation * const processed = [self expectationWithDescription:@"Real write executed"];
    [model holdNextCacheWriteForGetter:fixture.cacheGetter
                               handler:^{
        [captured fulfill];
    }
                     completionHandler:^{
        [processed fulfill];
    }];
    [self addTeardownBlock:^{
        [model releaseCacheWrite];
    }];
    XCTestExpectation * const published = [self expectationWithDescription:@"Reset delivered after its write"];
    XCTestExpectation * const premature = [self expectationWithDescription:@"No reset before its write"];
    premature.inverted = YES;
    XCTestExpectation * const duplicate = [self expectationWithDescription:@"No duplicate reset for a single accepted write"];
    duplicate.inverted = YES;
    __block NSUInteger count = 0;
    [self observeNotification:fixture.changeNotifications.firstObject
                       object:model
                        queue:nil
                   usingBlock:^(NSNotification *notification) {
        count++;
        if (count > 1)
            [duplicate fulfill];
        if (!model.didExecuteHeldCacheWrite)
            [premature fulfill];
        XCTAssertTrue(model.didExecuteHeldCacheWrite);
        XCTAssertTrue(NSThread.isMainThread);
        XCTAssertEqual(notification.object, model);
        if (count == 1)
            [published fulfill];
    }];
    [fixture refreshCache];
    [self waitForExpectations:@[ captured ] timeout:5.];
    XCTAssertFalse(model.didExecuteHeldCacheWrite);
    [self waitForExpectations:@[ premature ] timeout:.05];
    [model releaseCacheWrite];
    [self waitForExpectations:@[ processed, published ] timeout:5.];
    [self waitForExpectations:@[ duplicate ] timeout:.05];
    XCTAssertEqual(count, (NSUInteger)1);
    XCTAssertEqualObjects([self orderedIDs:fixture.cachedItems], [self orderedIDs:fixture.libraryItems]);
}

- (void)testAudioOlderResultIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAudioCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:NO];
}

- (void)testAudioOlderResultIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAudioCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:NO];
}

- (void)testAudioAlreadyQueuedWriteIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAudioCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:YES];
}

- (void)testAudioAlreadyQueuedWriteIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAudioCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:YES];
}

- (void)testAudioOlderResultIsRejectedAfterFilterChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAudioCache
                                      action:VLCLibraryModelSupersedingFilter
                                 queuedWrite:NO];
}

- (void)testAudioOlderResultIsRejectedAfterSortChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAudioCache
                                      action:VLCLibraryModelSupersedingSort
                                 queuedWrite:NO];
}

- (void)testAudioPendingRenameSurvivesLaterFavoriteChange
{
    [self assertPendingRenameForCacheKind:VLCLibraryModelTestAudioCache followedByDelete:NO];
}

- (void)testAudioPendingRenameSurvivesLaterDeletion
{
    [self assertPendingRenameForCacheKind:VLCLibraryModelTestAudioCache followedByDelete:YES];
}

- (void)testAudioBackToBackUpdatesAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestAudioCache deletingOtherItem:NO];
}

- (void)testAudioBackToBackUpdateAndDeletionAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestAudioCache deletingOtherItem:YES];
}

- (void)testAudioResetNotificationWaitsForItsWrite
{
    [self assertResetNotificationWaitsForWriteForCacheKind:VLCLibraryModelTestAudioCache];
}

- (void)testVideoOlderResultIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestVideoCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:NO];
}

- (void)testVideoOlderResultIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestVideoCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:NO];
}

- (void)testVideoAlreadyQueuedWriteIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestVideoCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:YES];
}

- (void)testVideoAlreadyQueuedWriteIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestVideoCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:YES];
}

- (void)testVideoOlderResultIsRejectedAfterFilterChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestVideoCache
                                      action:VLCLibraryModelSupersedingFilter
                                 queuedWrite:NO];
}

- (void)testVideoOlderResultIsRejectedAfterSortChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestVideoCache
                                      action:VLCLibraryModelSupersedingSort
                                 queuedWrite:NO];
}

- (void)testVideoPendingRenameSurvivesLaterFavoriteChange
{
    [self assertPendingRenameForCacheKind:VLCLibraryModelTestVideoCache followedByDelete:NO];
}

- (void)testVideoPendingRenameSurvivesLaterDeletion
{
    [self assertPendingRenameForCacheKind:VLCLibraryModelTestVideoCache followedByDelete:YES];
}

- (void)testVideoBackToBackUpdatesAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestVideoCache deletingOtherItem:NO];
}

- (void)testVideoBackToBackUpdateAndDeletionAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestVideoCache deletingOtherItem:YES];
}

- (void)testVideoResetNotificationWaitsForItsWrite
{
    [self assertResetNotificationWaitsForWriteForCacheKind:VLCLibraryModelTestVideoCache];
}

- (void)testArtistOlderResultIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestArtistCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:NO];
}

- (void)testArtistOlderResultIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestArtistCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:NO];
}

- (void)testArtistAlreadyQueuedWriteIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestArtistCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:YES];
}

- (void)testArtistAlreadyQueuedWriteIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestArtistCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:YES];
}

- (void)testArtistOlderResultIsRejectedAfterFilterChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestArtistCache
                                      action:VLCLibraryModelSupersedingFilter
                                 queuedWrite:NO];
}

- (void)testArtistOlderResultIsRejectedAfterSortChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestArtistCache
                                      action:VLCLibraryModelSupersedingSort
                                 queuedWrite:NO];
}

- (void)testArtistPendingRenameSurvivesLaterFavoriteChange
{
    [self assertPendingRenameForCacheKind:VLCLibraryModelTestArtistCache followedByDelete:NO];
}

- (void)testArtistPendingRenameSurvivesLaterDeletion
{
    [self assertPendingRenameForCacheKind:VLCLibraryModelTestArtistCache followedByDelete:YES];
}

- (void)testArtistBackToBackUpdatesAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestArtistCache deletingOtherItem:NO];
}

- (void)testArtistBackToBackUpdateAndDeletionAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestArtistCache deletingOtherItem:YES];
}

- (void)testArtistResetNotificationWaitsForItsWrite
{
    [self assertResetNotificationWaitsForWriteForCacheKind:VLCLibraryModelTestArtistCache];
}

- (void)testAlbumOlderResultIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAlbumCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:NO];
}

- (void)testAlbumOlderResultIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAlbumCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:NO];
}

- (void)testAlbumAlreadyQueuedWriteIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAlbumCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:YES];
}

- (void)testAlbumAlreadyQueuedWriteIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAlbumCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:YES];
}

- (void)testAlbumOlderResultIsRejectedAfterFilterChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAlbumCache
                                      action:VLCLibraryModelSupersedingFilter
                                 queuedWrite:NO];
}

- (void)testAlbumOlderResultIsRejectedAfterSortChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestAlbumCache
                                      action:VLCLibraryModelSupersedingSort
                                 queuedWrite:NO];
}

- (void)testAlbumPendingRenameSurvivesLaterFavoriteChange
{
    [self assertPendingRenameForCacheKind:VLCLibraryModelTestAlbumCache followedByDelete:NO];
}

- (void)testAlbumPendingRenameSurvivesLaterDeletion
{
    [self assertPendingRenameForCacheKind:VLCLibraryModelTestAlbumCache followedByDelete:YES];
}

- (void)testAlbumBackToBackUpdatesAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestAlbumCache deletingOtherItem:NO];
}

- (void)testAlbumBackToBackUpdateAndDeletionAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestAlbumCache deletingOtherItem:YES];
}

- (void)testAlbumResetNotificationWaitsForItsWrite
{
    [self assertResetNotificationWaitsForWriteForCacheKind:VLCLibraryModelTestAlbumCache];
}

- (void)testGenreOlderResultIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGenreCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:NO];
}

- (void)testGenreOlderResultIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGenreCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:NO];
}

- (void)testGenreAlreadyQueuedWriteIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGenreCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:YES];
}

- (void)testGenreAlreadyQueuedWriteIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGenreCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:YES];
}

- (void)testGenreOlderResultIsRejectedAfterFilterChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGenreCache
                                      action:VLCLibraryModelSupersedingFilter
                                 queuedWrite:NO];
}

- (void)testGenrePendingRenameSurvivesLaterFavoriteChange
{
    [self assertPendingRenameForCacheKind:VLCLibraryModelTestGenreCache followedByDelete:NO];
}

- (void)testGenrePendingRenameSurvivesLaterDeletion
{
    [self assertPendingRenameForCacheKind:VLCLibraryModelTestGenreCache followedByDelete:YES];
}

- (void)testGenreBackToBackUpdatesAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestGenreCache deletingOtherItem:NO];
}

- (void)testGenreBackToBackUpdateAndDeletionAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestGenreCache deletingOtherItem:YES];
}

- (void)testGenreResetNotificationWaitsForItsWrite
{
    [self assertResetNotificationWaitsForWriteForCacheKind:VLCLibraryModelTestGenreCache];
}

- (void)testGroupOlderResultIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGroupCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:NO];
}

- (void)testGroupOlderResultIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGroupCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:NO];
}

- (void)testGroupAlreadyQueuedWriteIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGroupCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:YES];
}

- (void)testGroupAlreadyQueuedWriteIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGroupCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:YES];
}

- (void)testGroupOlderResultIsRejectedAfterFilterChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGroupCache
                                      action:VLCLibraryModelSupersedingFilter
                                 queuedWrite:NO];
}

- (void)testGroupOlderResultIsRejectedAfterSortChange
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestGroupCache
                                      action:VLCLibraryModelSupersedingSort
                                 queuedWrite:NO];
}

- (void)testGroupBackToBackUpdatesAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestGroupCache deletingOtherItem:NO];
}

- (void)testGroupBackToBackUpdateAndDeletionAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestGroupCache deletingOtherItem:YES];
}

- (void)testGroupResetNotificationWaitsForItsWrite
{
    [self assertResetNotificationWaitsForWriteForCacheKind:VLCLibraryModelTestGroupCache];
}

- (void)testShowOlderResultIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestShowCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:NO];
}

- (void)testShowOlderResultIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestShowCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:NO];
}

- (void)testShowAlreadyQueuedWriteIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestShowCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:YES];
}

- (void)testShowAlreadyQueuedWriteIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestShowCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:YES];
}

- (void)testShowResetNotificationWaitsForItsWrite
{
    [self assertResetNotificationWaitsForWriteForCacheKind:VLCLibraryModelTestShowCache];
}

- (void)testMovieOlderResultIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestMovieCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:NO];
}

- (void)testMovieOlderResultIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestMovieCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:NO];
}

- (void)testMovieAlreadyQueuedWriteIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestMovieCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:YES];
}

- (void)testMovieAlreadyQueuedWriteIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestMovieCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:YES];
}

- (void)testMovieResetNotificationWaitsForItsWrite
{
    [self assertResetNotificationWaitsForWriteForCacheKind:VLCLibraryModelTestMovieCache];
}

- (void)testRecentAudioOlderResultIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestRecentAudioCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:NO];
}

- (void)testRecentAudioOlderResultIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestRecentAudioCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:NO];
}

- (void)testRecentAudioAlreadyQueuedWriteIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestRecentAudioCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:YES];
}

- (void)testRecentAudioAlreadyQueuedWriteIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestRecentAudioCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:YES];
}

- (void)testRecentAudioBackToBackUpdatesAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestRecentAudioCache deletingOtherItem:NO];
}

- (void)testRecentAudioBackToBackUpdateAndDeletionAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestRecentAudioCache deletingOtherItem:YES];
}

- (void)testRecentAudioResetNotificationWaitsForItsWrite
{
    [self assertResetNotificationWaitsForWriteForCacheKind:VLCLibraryModelTestRecentAudioCache];
}

- (void)testRecentVideoOlderResultIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestRecentVideoCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:NO];
}

- (void)testRecentVideoOlderResultIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestRecentVideoCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:NO];
}

- (void)testRecentVideoAlreadyQueuedWriteIsRejectedAfterDrop
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestRecentVideoCache
                                      action:VLCLibraryModelSupersedingDrop
                                 queuedWrite:YES];
}

- (void)testRecentVideoAlreadyQueuedWriteIsRejectedAfterNewerReset
{
    [self assertSupersededResultForCacheKind:VLCLibraryModelTestRecentVideoCache
                                      action:VLCLibraryModelSupersedingReset
                                 queuedWrite:YES];
}

- (void)testRecentVideoBackToBackUpdatesAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestRecentVideoCache deletingOtherItem:NO];
}

- (void)testRecentVideoBackToBackUpdateAndDeletionAreBothPreserved
{
    [self assertBackToBackMutationsForCacheKind:VLCLibraryModelTestRecentVideoCache deletingOtherItem:YES];
}

- (void)testRecentVideoResetNotificationWaitsForItsWrite
{
    [self assertResetNotificationWaitsForWriteForCacheKind:VLCLibraryModelTestRecentVideoCache];
}

@end

typedef NS_ENUM(NSUInteger, VLCLibraryModelOverlapChange) {
    VLCLibraryModelOverlapNoChange,
    VLCLibraryModelOverlapUpdate,
    VLCLibraryModelOverlapDeletion,
    VLCLibraryModelOverlapPendingItemDeletion,
};

@interface VLCLibraryModelOverlapTest : VLCLibraryModelFixtureTestCase
@end

@implementation VLCLibraryModelOverlapTest

- (NSSet<NSNumber *> *)libraryIDsForItems:(NSArray<id<VLCMediaLibraryItemProtocol>> *)items
{
    NSMutableSet<NSNumber *> * const IDs = NSMutableSet.set;
    for (id<VLCMediaLibraryItemProtocol> item in items)
        [IDs addObject:@(item.libraryID)];
    return IDs.copy;
}

- (void)assertRefreshForCacheKind:(VLCLibraryModelTestCacheKind)kind
                overlappingChange:(VLCLibraryModelOverlapChange)change
{
    XCTestExpectation * const preparedFixture = [self expectationWithDescription:@"Overlap fixtures prepared"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^{
        self.fixture = [[VLCLibraryModelTestOverlapFixture alloc] initWithCacheKind:kind];
        [preparedFixture fulfill];
    });
    [self waitForExpectations:@[ preparedFixture ] timeout:15.];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    XCTAssertNotNil(fixture);
    VLCLibraryModelTestQueryGate * const model = fixture.model;
    const int64_t pendingID = fixture.pendingItem.libraryID;
    const int64_t changedID = fixture.changedItem.libraryID;
    NSArray<id<VLCMediaLibraryItemProtocol>> * const persistedItems = fixture.libraryItems;
    XCTAssertNotNil(persistedItems);
    NSMutableArray * const previousItems = persistedItems.mutableCopy;
    // The cached list predates delivery of the newly persisted item's event.
    [previousItems filterUsingPredicate:[NSPredicate predicateWithBlock:
        ^BOOL(id<VLCMediaLibraryItemProtocol> item, NSDictionary * __unused bindings) {
        return item.libraryID != pendingID;
    }]];
    XCTAssertNotNil([self itemWithID:changedID inItems:previousItems]);
    if (kind == VLCLibraryModelTestShowCache) {
        XCTestExpectation * const prepared = [self expectationWithDescription:@"Initial show membership prepared"];
        XCTestExpectation * const written = [self expectationWithDescription:@"Initial show cache written"];
        [model holdNextQueryResultForGetter:fixture.cacheGetter
                                    handler:^{
            [prepared fulfill];
        }
                          completionHandler:^{
            [written fulfill];
        }];
        [self addTeardownBlock:^{
            [model releaseQuery];
        }];
        [fixture seedCacheWithItems:previousItems];
        [self waitForExpectations:@[ prepared ] timeout:5.];
        [model releaseQuery];
        [self waitForExpectations:@[ written ] timeout:5.];
    } else {
        [fixture seedCacheWithItems:previousItems];
    }
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.cachedItems],
                          [self libraryIDsForItems:previousItems]);
    XCTAssertFalse([[self libraryIDsForItems:fixture.cachedItems] containsObject:@(pendingID)]);
    XCTAssertTrue([fixture preparePendingChange]);
    XCTAssertNotNil([self itemWithID:pendingID inItems:fixture.libraryItems]);

    XCTestExpectation * const captured = [self expectationWithDescription:@"Full refresh result captured"];
    XCTestExpectation * const processed = [self expectationWithDescription:@"Held refresh write processed"];
    [model holdNextQueryResultForGetter:fixture.cacheGetter
                                handler:^{
        [captured fulfill];
    }
                      completionHandler:^{
        [processed fulfill];
    }];
    [self addTeardownBlock:^{
        [model releaseQuery];
    }];
    XCTestExpectation * const reconciled = [self expectationWithDescription:@"Full cache reconciliation committed"];
    __block BOOL didReconcile = NO;
    // Each fixture lists its full-list reset first, followed by item events.
    [self observeNotification:fixture.changeNotifications.firstObject
                       object:model
                        queue:NSOperationQueue.mainQueue
                   usingBlock:^(NSNotification * __unused notification) {
        if (!didReconcile) {
            didReconcile = YES;
            [reconciled fulfill];
        }
    }];
    [fixture deliverRefreshEvent];
    [self waitForExpectations:@[ captured ] timeout:5.];
    if (change == VLCLibraryModelOverlapPendingItemDeletion) {
        XCTAssertTrue([fixture deletePendingItem]);
        XCTAssertNil([self itemWithID:pendingID inItems:fixture.libraryItems]);
        [fixture deliverPendingDeletionEvent];
        // A deletion absent from the old cache may emit no item notification.
        // The synchronized getter waits for its owning-queue handler instead.
        NSArray * const cacheAfterDeletion = fixture.cachedItems;
        XCTAssertNotNil(cacheAfterDeletion);
        XCTAssertNil([self itemWithID:pendingID inItems:cacheAfterDeletion]);
    } else if (change != VLCLibraryModelOverlapNoChange) {
        XCTestExpectation * const changed = [self expectationWithDescription:@"Later cache change committed"];
        __block BOOL fulfilled = NO;
        for (NSNotificationName name in fixture.changeNotifications) {
            [self observeNotification:name
                               object:nil
                                queue:NSOperationQueue.mainQueue
                           usingBlock:^(NSNotification *notification) {
                id const object = notification.object;
                const BOOL matches = object == model ||
                    ([object isKindOfClass:NSNumber.class] && [object longLongValue] == changedID) ||
                    ([object conformsToProtocol:@protocol(VLCMediaLibraryItemProtocol)] &&
                     [object libraryID] == changedID);
                if (matches && !fulfilled) {
                    fulfilled = YES;
                    [changed fulfill];
                }
            }];
        }
        if (change == VLCLibraryModelOverlapUpdate) {
            XCTAssertTrue([fixture updateChangedItem]);
            [fixture deliverUpdateEvent];
        } else {
            XCTAssertTrue([fixture deleteChangedItem]);
            // Verify actual database removal, not merely a model event.
            XCTAssertNil([self itemWithID:changedID inItems:fixture.libraryItems]);
            [fixture deliverDeletionEvent];
        }
        [self waitForExpectations:@[ changed ] timeout:5.];
    }
    [model releaseQuery];
    [self waitForExpectations:@[ processed ] timeout:5.];
    // Processing the rejected snapshot does not imply its replacement query
    // has finished. Wait for the actual full-list write before inspecting it.
    [self waitForExpectations:@[ reconciled ] timeout:5.];
    NSArray<id<VLCMediaLibraryItemProtocol>> * const expected = fixture.libraryItems;
    NSArray<id<VLCMediaLibraryItemProtocol>> * const actual = fixture.cachedItems;
    XCTAssertNotNil(expected);
    XCTAssertNotNil(actual);
    if (change == VLCLibraryModelOverlapPendingItemDeletion) {
        XCTAssertNil([self itemWithID:pendingID inItems:expected]);
        XCTAssertNil([self itemWithID:pendingID inItems:actual],
                     @"The held query must not restore the deleted, previously uncached item");
        XCTAssertNotNil([self itemWithID:changedID inItems:actual]);
    } else {
        XCTAssertNotNil([self itemWithID:pendingID inItems:expected]);
    }
    if (change == VLCLibraryModelOverlapUpdate) {
        id<VLCMediaLibraryItemProtocol> const updated = [self itemWithID:changedID inItems:actual];
        id<VLCMediaLibraryItemProtocol> const persisted = [self itemWithID:changedID inItems:expected];
        XCTAssertNotNil(updated);
        if (kind == VLCLibraryModelTestGroupCache) {
            XCTAssertEqualObjects(persisted.displayString, @"Overlap Updated Group");
            XCTAssertEqualObjects(updated.displayString, persisted.displayString);
        } else if (kind == VLCLibraryModelTestShowCache) {
            XCTAssertEqualObjects(((VLCMediaLibraryShow *)persisted).summary, @"Overlap Updated Show");
            XCTAssertEqualObjects(((VLCMediaLibraryShow *)updated).summary,
                                  ((VLCMediaLibraryShow *)persisted).summary);
        } else {
            XCTAssertTrue(persisted.favorited);
            XCTAssertTrue(updated.favorited);
        }
    } else if (change == VLCLibraryModelOverlapDeletion) {
        XCTAssertNil([self itemWithID:changedID inItems:actual]);
    }
    // Both the later mutation and the original membership change must
    // survive, not merely the rejection of the held query's token.
    XCTAssertEqualObjects([self libraryIDsForItems:actual],
                          [self libraryIDsForItems:expected],
                          @"The final cache must match persisted membership after the later event");
}

- (NSArray<NSString *> *)groupFixtureNamesInItems:(NSArray<id<VLCMediaLibraryItemProtocol>> *)items
{
    NSMutableArray<NSString *> * const names = NSMutableArray.array;
    for (id<VLCMediaLibraryItemProtocol> item in items) {
        if (item.libraryID == self.fixture.changedItem.libraryID ||
            item.libraryID == self.fixture.pendingItem.libraryID)
            [names addObject:item.displayString];
    }
    return names.copy;
}

- (void)assertGroupRenameFrom:(NSString *)oldName
                       toName:(NSString *)newName
                       filter:(NSString *)filter
                   descending:(BOOL)descending
             expectedOldNames:(NSArray<NSString *> *)expectedOldNames
             expectedNewNames:(NSArray<NSString *> *)expectedNewNames
             overlappingFetch:(BOOL)overlappingFetch
{
    XCTestExpectation * const prepared = [self expectationWithDescription:@"Persisted groups prepared"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^{
        self.fixture = [[VLCLibraryModelTestOverlapFixture alloc] initWithGroupName:@"Group Cache Middle"
                                                                     otherGroupName:oldName];
        [prepared fulfill];
    });
    [self waitForExpectations:@[ prepared ] timeout:15.];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    XCTAssertNotNil(fixture);
    VLCLibraryModelTestQueryGate * const model = fixture.model;
    [model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:descending];
    model.filterString = filter;
    XCTAssertEqualObjects([self groupFixtureNamesInItems:fixture.libraryItems], expectedOldNames);
    XCTestExpectation * const loaded = [self expectationForNotification:VLCLibraryModelListOfGroupsReset
                                                                 object:model
                                                                handler:nil];
    [fixture deliverRefreshEvent];
    [self waitForExpectations:@[ loaded ] timeout:5.];
    XCTAssertEqualObjects([self groupFixtureNamesInItems:fixture.cachedItems], expectedOldNames);
    XCTAssertEqualObjects([self orderedIDs:fixture.cachedItems],
                          [self orderedIDs:fixture.libraryItems]);

    XCTestExpectation *captured = nil;
    XCTestExpectation *processed = nil;
    if (overlappingFetch) {
        captured = [self expectationWithDescription:@"Pre-rename group list fetched"];
        processed = [self expectationWithDescription:@"Pre-rename group list processed"];
        [model holdNextQueryResultForGetter:fixture.cacheGetter
                                    handler:^{
            [captured fulfill];
        }
                          completionHandler:^{
            [processed fulfill];
        }];
        [self addTeardownBlock:^{
            [model releaseQuery];
        }];
    }
    if (overlappingFetch) {
        [fixture deliverRefreshEvent];
        [self waitForExpectations:@[ captured ] timeout:5.];
    }
    XCTAssertTrue(VLCLibraryDataTypesIntegrationRenameGroup(fixture.changedItem.libraryID, newName));
    // Prove the real ML query sees the new membership/order before the
    // model event is delivered, independently of its cached objects.
    XCTAssertEqualObjects([self groupFixtureNamesInItems:fixture.libraryItems], expectedNewNames);
    XCTAssertEqualObjects([VLCMediaLibraryGroup groupWithID:fixture.changedItem.libraryID].name, newName);

    XCTestExpectation * const updated = [self expectationWithDescription:@"Group cache update published"];
    __block BOOL fulfilled = NO;
    for (NSNotificationName name in @[ VLCLibraryModelListOfGroupsReset, VLCLibraryModelGroupUpdated ]) {
        [self observeNotification:name
                           object:nil
                            queue:NSOperationQueue.mainQueue
                       usingBlock:^(NSNotification *notification) {
            if (!fulfilled && (notification.object == model ||
                ([notification.object isKindOfClass:VLCMediaLibraryGroup.class] &&
                 [notification.object libraryID] == fixture.changedItem.libraryID))) {
                fulfilled = YES;
                [updated fulfill];
            }
        }];
    }
    [fixture deliverUpdateEvent];
    // Drain the event handler before allowing its captured old result
    // to write. This also exercises the uncached-group early return.
    (void)fixture.cachedItems;
    if (overlappingFetch) {
        [model releaseQuery];
        [self waitForExpectations:@[ processed ] timeout:5.];
    }
    [self waitForExpectations:@[ updated ] timeout:5.];
    XCTAssertEqualObjects([self groupFixtureNamesInItems:fixture.cachedItems], expectedNewNames);
    XCTAssertEqualObjects([self orderedIDs:fixture.cachedItems],
                          [self orderedIDs:fixture.libraryItems]);
    VLCMediaLibraryGroup * const cached = (id)[self itemWithID:fixture.changedItem.libraryID
                                                       inItems:fixture.cachedItems];
    if (cached != nil)
        XCTAssertEqualObjects(cached.name, newName);
}

- (void)assertFavoriteUpdateStaysInCache:(VLCLibraryModelTestCacheKind)kind
                                  filter:(BOOL)filtered
                                 sorting:(enum vlc_ml_sorting_criteria_t)sorting
{
    XCTestExpectation * const prepared = [self expectationWithDescription:@"Update fixtures prepared"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^{
        self.fixture = kind == VLCLibraryModelTestGroupCache
            ? [[VLCLibraryModelTestOverlapFixture alloc] initWithGroupName:@"Selective Middle"
                                                            otherGroupName:@"Selective First"]
            : [[VLCLibraryModelTestOverlapFixture alloc] initWithCacheKind:kind];
        [prepared fulfill];
    });
    [self waitForExpectations:@[ prepared ] timeout:15.];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    XCTAssertNotNil(fixture);
    VLCLibraryModelTestQueryGate * const model = fixture.model;
    [model sortByCriteria:sorting andDescending:NO];
    if (filtered)
        model.filterString = fixture.changedItem.displayString;
    XCTestExpectation * const loaded = [self expectationForNotification:fixture.changeNotifications.firstObject
                                                                 object:model
                                                                handler:nil];
    [fixture deliverRefreshEvent];
    [self waitForExpectations:@[ loaded ] timeout:5.];
    NSArray * const before = fixture.cachedItems;
    id<VLCMediaLibraryItemProtocol> const previous = [self itemWithID:fixture.changedItem.libraryID inItems:before];
    XCTAssertNotNil(previous);
    XCTAssertFalse(previous.favorited);
    XCTAssertEqualObjects([self orderedIDs:before],
                          [self orderedIDs:fixture.libraryItems]);
    const NSUInteger queryCount = [model queryCountForGetter:fixture.cacheGetter];
    XCTestExpectation * const updated = [self expectationWithDescription:@"Favorite change committed"];
    __block BOOL fulfilled = NO;
    for (NSNotificationName name in [fixture.changeNotifications subarrayWithRange:NSMakeRange(0, 2)]) {
        [self observeNotification:name
                           object:nil
                            queue:NSOperationQueue.mainQueue
                       usingBlock:^(NSNotification *notification) {
            if (!fulfilled && (notification.object == model ||
                ([notification.object conformsToProtocol:@protocol(VLCMediaLibraryItemProtocol)] &&
                 [notification.object libraryID] == fixture.changedItem.libraryID))) {
                fulfilled = YES;
                [updated fulfill];
            }
        }];
    }
    XCTAssertTrue([fixture setChangedItemFavorite:YES]);
    XCTAssertTrue([self itemWithID:fixture.changedItem.libraryID inItems:fixture.libraryItems].favorited);
    [fixture deliverUpdateEvent];
    [self waitForExpectations:@[ updated ] timeout:5.];
    NSArray * const after = fixture.cachedItems;
    XCTAssertTrue([self itemWithID:fixture.changedItem.libraryID inItems:after].favorited);
    XCTAssertEqualObjects([self orderedIDs:after], [self orderedIDs:before]);
    XCTAssertEqualObjects([self orderedIDs:after],
                          [self orderedIDs:fixture.libraryItems]);
    XCTAssertEqual([model queryCountForGetter:fixture.cacheGetter],
                   queryCount,
                   @"Unchanged search and sort fields should permit in-place replacement");
}

- (void)assertUpdateRefreshesCache:(VLCLibraryModelTestCacheKind)kind
                            filter:(NSString *)filter
                           sorting:(enum vlc_ml_sorting_criteria_t)sorting
                           prepare:(BOOL (^)(VLCLibraryModelTestOverlapFixture *fixture))prepare
                            change:(BOOL (^)(VLCLibraryModelTestOverlapFixture *fixture))change
{
    XCTestExpectation * const prepared = [self expectationWithDescription:@"Reload fixtures prepared"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^{
        self.fixture = kind == VLCLibraryModelTestGroupCache
            ? [[VLCLibraryModelTestOverlapFixture alloc] initWithGroupName:@"Selective Middle"
                                                            otherGroupName:@"Selective First"]
            : [[VLCLibraryModelTestOverlapFixture alloc] initWithCacheKind:kind];
        [prepared fulfill];
    });
    [self waitForExpectations:@[ prepared ] timeout:15.];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    XCTAssertNotNil(fixture);
    VLCLibraryModelTestQueryGate * const model = fixture.model;
    if (prepare != nil)
        XCTAssertTrue(prepare(fixture));
    [model sortByCriteria:sorting andDescending:NO];
    model.filterString = filter;
    XCTestExpectation * const loaded = [self expectationForNotification:fixture.changeNotifications.firstObject
                                                                 object:model
                                                                handler:nil];
    [fixture deliverRefreshEvent];
    [self waitForExpectations:@[ loaded ] timeout:5.];
    NSArray * const before = fixture.cachedItems;
    XCTAssertNotNil([self itemWithID:fixture.changedItem.libraryID inItems:before]);
    XCTAssertEqualObjects([self orderedIDs:before],
                          [self orderedIDs:fixture.libraryItems]);
    const NSUInteger queryCount = [model queryCountForGetter:fixture.cacheGetter];
    XCTAssertTrue(change(fixture));
    id<VLCMediaLibraryItemProtocol> const freshItem = [fixture refetchChangedItem];
    XCTAssertNotNil(freshItem);
    XCTAssertEqualObjects(freshItem.displayString,
                          [self itemWithID:fixture.changedItem.libraryID inItems:before].displayString,
                          @"Membership and ordering changes must be detected even when the displayed name is unchanged");
    NSArray * const expected = fixture.libraryItems;
    XCTAssertGreaterThan(expected.count, (NSUInteger)0, @"Unaffected records must survive the update");
    XCTAssertNotEqualObjects([self orderedIDs:expected],
                             [self orderedIDs:before],
                             @"The persisted change must really affect query membership or order");
    XCTestExpectation * const reloaded = [self expectationForNotification:fixture.changeNotifications.firstObject
                                                                   object:model
                                                                  handler:nil];
    [fixture deliverUpdateEvent];
    [self waitForExpectations:@[ reloaded ] timeout:5.];
    XCTAssertEqualObjects([self orderedIDs:fixture.cachedItems],
                          [self orderedIDs:expected]);
    XCTAssertEqual([model queryCountForGetter:fixture.cacheGetter], queryCount + 1);
}

- (void)testAlbumDurationChangeReordersDurationSortedCache
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestAlbumCache
                              filter:nil
                             sorting:VLC_ML_SORTING_DURATION
                             prepare:nil
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return VLCLibraryDataTypesIntegrationSetAlbumDuration(fixture.changedItem.libraryID, 1);
    }];
}

- (void)testAlbumYearChangeReordersReleaseDateSortedCache
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestAlbumCache
                              filter:nil
                             sorting:VLC_ML_SORTING_RELEASEDATE
                             prepare:nil
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return VLCLibraryDataTypesIntegrationSetAlbumYear(fixture.changedItem.libraryID, 1900);
    }];
}

- (void)testAlbumArtistChangeRemovesAlbumFromArtistNameFilter
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestAlbumCache
                              filter:@"Artist"
                             sorting:VLC_ML_SORTING_ALPHA
                             prepare:nil
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return VLCLibraryDataTypesIntegrationRenameAlbumArtist(fixture.changedItem.libraryID, @"Different Performer");
    }];
}

- (void)testAlbumArtistChangeReordersArtistSortedCache
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestAlbumCache
                              filter:nil
                             sorting:VLC_ML_SORTING_ARTIST
                             prepare:nil
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return VLCLibraryDataTypesIntegrationRenameAlbumArtist(fixture.changedItem.libraryID, @"AAA Performer");
    }];
}

- (void)testAlbumArtistChangeReordersAlphabeticalAlbumsWithSameTitle
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestAlbumCache
                              filter:nil
                             sorting:VLC_ML_SORTING_ALPHA
                             prepare:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return VLCLibraryDataTypesIntegrationRenameAlbum(fixture.pendingItem.libraryID,
                                                        fixture.changedItem.displayString);
    }
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return VLCLibraryDataTypesIntegrationRenameAlbumArtist(fixture.changedItem.libraryID, @"AAA Performer");
    }];
}

- (void)testAlbumBecomingUnavailableRemovesItFromCache
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestAlbumCache
                              filter:nil
                             sorting:VLC_ML_SORTING_ALPHA
                             prepare:nil
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return [fixture setChangedItemPresent:NO];
    }];
}

- (void)testArtistBecomingUnavailableRemovesItFromCache
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestArtistCache
                              filter:nil
                             sorting:VLC_ML_SORTING_ALPHA
                             prepare:nil
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return [fixture setChangedItemPresent:NO];
    }];
}

- (void)testGroupBecomingUnavailableRemovesItFromCache
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestGroupCache
                              filter:nil
                             sorting:VLC_ML_SORTING_ALPHA
                             prepare:nil
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return [fixture setChangedItemPresent:NO];
    }];
}

- (void)testGroupDurationChangeReordersDurationSortedCache
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestGroupCache
                              filter:nil
                             sorting:VLC_ML_SORTING_DURATION
                             prepare:nil
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return VLCLibraryDataTypesIntegrationSetGroupDuration(fixture.changedItem.libraryID, 1);
    }];
}

- (void)testGroupModificationDateChangeReordersDateSortedCache
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestGroupCache
                              filter:nil
                             sorting:VLC_ML_SORTING_LASTMODIFICATIONDATE
                             prepare:nil
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return VLCLibraryDataTypesIntegrationSetGroupModificationDate(fixture.changedItem.libraryID,
                                                                     [NSDate dateWithTimeIntervalSince1970:1]);
    }];
}

- (void)testMediaLabelRemovalRemovesItemFromLabelFilterWithoutChangingTitle
{
    [self assertUpdateRefreshesCache:VLCLibraryModelTestAudioCache
                              filter:@"Transient"
                             sorting:VLC_ML_SORTING_ALPHA
                             prepare:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        vlc_medialibrary_t * const library = VLCLibraryDataTypesIntegrationMediaLibrary();
        return vlc_ml_media_add_label(library, fixture.changedItem.libraryID, "Transient Changed") == VLC_SUCCESS &&
               vlc_ml_media_add_label(library, fixture.pendingItem.libraryID, "Transient Pending") == VLC_SUCCESS;
    }
                              change:^BOOL(VLCLibraryModelTestOverlapFixture *fixture) {
        return vlc_ml_media_remove_label(VLCLibraryDataTypesIntegrationMediaLibrary(),
                                        fixture.changedItem.libraryID,
                                        "Transient Changed") == VLC_SUCCESS;
    }];
}

- (void)testFilteredArtistFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestArtistCache filter:YES sorting:VLC_ML_SORTING_ALPHA];
}

- (void)testFilteredAlbumFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestAlbumCache filter:YES sorting:VLC_ML_SORTING_ALPHA];
}

- (void)testFilteredGenreFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestGenreCache filter:YES sorting:VLC_ML_SORTING_ALPHA];
}

- (void)testFilteredGroupFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestGroupCache filter:YES sorting:VLC_ML_SORTING_ALPHA];
}

- (void)testDurationSortedAlbumFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestAlbumCache filter:NO sorting:VLC_ML_SORTING_DURATION];
}

- (void)testReleaseDateSortedAlbumFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestAlbumCache filter:NO sorting:VLC_ML_SORTING_RELEASEDATE];
}

- (void)testArtistSortedAlbumFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestAlbumCache filter:NO sorting:VLC_ML_SORTING_ARTIST];
}

- (void)testTrackCountSortedAlbumFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestAlbumCache filter:NO sorting:VLC_ML_SORTING_TRACKNUMBER];
}

- (void)testTrackCountSortedArtistFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestArtistCache filter:NO sorting:VLC_ML_SORTING_TRACKNUMBER];
}

- (void)testDurationSortedGroupFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestGroupCache filter:NO sorting:VLC_ML_SORTING_DURATION];
}

- (void)testCreationDateSortedGroupFavoriteUpdateDoesNotReloadCache
{
    [self assertFavoriteUpdateStaysInCache:VLCLibraryModelTestGroupCache filter:NO sorting:VLC_ML_SORTING_INSERTIONDATE];
}

- (void)testAudioAdditionRefreshWithoutOverlap
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestAudioCache overlappingChange:VLCLibraryModelOverlapNoChange];
}

- (void)testMovieAdditionRefreshWithoutOverlap
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestMovieCache overlappingChange:VLCLibraryModelOverlapNoChange];
}

- (void)testMovieAdditionRefreshDoesNotRestoreDeletedUncachedMovie
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestMovieCache overlappingChange:VLCLibraryModelOverlapPendingItemDeletion];
}

- (void)testMovieAdditionRefreshSurvivesExistingMovieDeletion
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestMovieCache overlappingChange:VLCLibraryModelOverlapDeletion];
}

- (void)testUnrelatedMediaDeletionDoesNotReloadSettledMovieCache
{
    XCTestExpectation * const prepared = [self expectationWithDescription:@"Movie fixtures prepared"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^{
        self.fixture = [[VLCLibraryModelTestOverlapFixture alloc] initWithCacheKind:VLCLibraryModelTestMovieCache];
        [prepared fulfill];
    });
    [self waitForExpectations:@[ prepared ] timeout:15.];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    XCTAssertNotNil(fixture);
    NSArray * const movies = fixture.libraryItems;
    XCTAssertEqual(movies.count, (NSUInteger)2);
    [fixture seedCacheWithItems:movies];
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.cachedItems], [self libraryIDsForItems:movies]);
    const NSUInteger queryCount = [fixture.model queryCountForGetter:fixture.cacheGetter];
    const int64_t audioID = VLCLibraryDataTypesIntegrationFactoryAudioMediaID();
    XCTAssertTrue(VLCLibraryDataTypesIntegrationDeleteEntity(VLC_ML_EVENT_MEDIA_DELETED, audioID));
    XCTAssertNil([VLCMediaLibraryMediaItem mediaItemForLibraryID:audioID]);
    [fixture deliverDeletionEventForID:audioID];
    // The synchronized movie read drains the deletion's owning-queue handler.
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.cachedItems], [self libraryIDsForItems:movies]);
    XCTAssertEqual([fixture.model queryCountForGetter:fixture.cacheGetter], queryCount);
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.libraryItems], [self libraryIDsForItems:movies]);
}

- (void)testAudioAdditionRefreshDoesNotRestoreDeletedUncachedItem
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestAudioCache overlappingChange:VLCLibraryModelOverlapPendingItemDeletion];
}

- (void)testVideoAdditionRefreshDoesNotRestoreDeletedUncachedItem
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestVideoCache overlappingChange:VLCLibraryModelOverlapPendingItemDeletion];
}

- (void)testArtistAdditionRefreshDoesNotRestoreDeletedUncachedItem
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestArtistCache overlappingChange:VLCLibraryModelOverlapPendingItemDeletion];
}

- (void)testAlbumAdditionRefreshDoesNotRestoreDeletedUncachedItem
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestAlbumCache overlappingChange:VLCLibraryModelOverlapPendingItemDeletion];
}

- (void)testGenreAdditionRefreshDoesNotRestoreDeletedUncachedItem
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestGenreCache overlappingChange:VLCLibraryModelOverlapPendingItemDeletion];
}

- (void)testGroupAdditionRefreshDoesNotRestoreDeletedUncachedItem
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestGroupCache overlappingChange:VLCLibraryModelOverlapPendingItemDeletion];
}

- (void)testShowAdditionRefreshDoesNotRestoreDeletedUncachedShow
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestShowCache overlappingChange:VLCLibraryModelOverlapPendingItemDeletion];
}

- (void)testRecentAudioHistoryRefreshDoesNotRestoreDeletedUncachedItem
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestRecentAudioCache overlappingChange:VLCLibraryModelOverlapPendingItemDeletion];
}

- (void)testRecentVideoHistoryRefreshDoesNotRestoreDeletedUncachedItem
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestRecentVideoCache overlappingChange:VLCLibraryModelOverlapPendingItemDeletion];
}

- (void)testAudioAdditionRefreshSurvivesUnrelatedUpdate
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestAudioCache overlappingChange:VLCLibraryModelOverlapUpdate];
}

- (void)testAudioAdditionRefreshSurvivesUnrelatedDeletion
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestAudioCache overlappingChange:VLCLibraryModelOverlapDeletion];
}

- (void)testVideoAdditionRefreshWithoutOverlap
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestVideoCache overlappingChange:VLCLibraryModelOverlapNoChange];
}

- (void)testVideoAdditionRefreshSurvivesUnrelatedUpdate
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestVideoCache overlappingChange:VLCLibraryModelOverlapUpdate];
}

- (void)testVideoAdditionRefreshSurvivesUnrelatedDeletion
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestVideoCache overlappingChange:VLCLibraryModelOverlapDeletion];
}

- (void)testArtistAdditionRefreshWithoutOverlap
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestArtistCache overlappingChange:VLCLibraryModelOverlapNoChange];
}

- (void)testArtistAdditionRefreshSurvivesUnrelatedUpdate
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestArtistCache overlappingChange:VLCLibraryModelOverlapUpdate];
}

- (void)testArtistAdditionRefreshSurvivesUnrelatedDeletion
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestArtistCache overlappingChange:VLCLibraryModelOverlapDeletion];
}

- (void)testAlbumAdditionRefreshWithoutOverlap
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestAlbumCache overlappingChange:VLCLibraryModelOverlapNoChange];
}

- (void)testAlbumAdditionRefreshSurvivesUnrelatedUpdate
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestAlbumCache overlappingChange:VLCLibraryModelOverlapUpdate];
}

- (void)testAlbumAdditionRefreshSurvivesUnrelatedDeletion
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestAlbumCache overlappingChange:VLCLibraryModelOverlapDeletion];
}

- (void)testGenreAdditionRefreshWithoutOverlap
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestGenreCache overlappingChange:VLCLibraryModelOverlapNoChange];
}

- (void)testGenreAdditionRefreshSurvivesUnrelatedUpdate
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestGenreCache overlappingChange:VLCLibraryModelOverlapUpdate];
}

- (void)testGenreAdditionRefreshSurvivesUnrelatedDeletion
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestGenreCache overlappingChange:VLCLibraryModelOverlapDeletion];
}

- (void)testGroupAdditionRefreshWithoutOverlap
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestGroupCache overlappingChange:VLCLibraryModelOverlapNoChange];
}

- (void)testGroupRenameReordersAscendingCache
{
    [self assertGroupRenameFrom:@"Group Cache Alpha"
                         toName:@"Group Cache Zulu"
                         filter:@""
                     descending:NO
               expectedOldNames:@[ @"Group Cache Alpha", @"Group Cache Middle" ]
               expectedNewNames:@[ @"Group Cache Middle", @"Group Cache Zulu" ]
               overlappingFetch:NO];
}

- (void)testGroupRenameReordersDescendingCache
{
    [self assertGroupRenameFrom:@"Group Cache Zulu"
                         toName:@"Group Cache Alpha"
                         filter:@""
                     descending:YES
               expectedOldNames:@[ @"Group Cache Zulu", @"Group Cache Middle" ]
               expectedNewNames:@[ @"Group Cache Middle", @"Group Cache Alpha" ]
               overlappingFetch:NO];
}

- (void)testGroupRenameRemovesGroupNoLongerMatchingFilter
{
    [self assertGroupRenameFrom:@"Group Cache Alpha"
                         toName:@"Outside Filter"
                         filter:@"Group Cache"
                     descending:NO
               expectedOldNames:@[ @"Group Cache Alpha", @"Group Cache Middle" ]
               expectedNewNames:@[ @"Group Cache Middle" ]
               overlappingFetch:NO];
}

- (void)testGroupRenameAddsGroupNewlyMatchingFilter
{
    [self assertGroupRenameFrom:@"Outside Filter"
                         toName:@"Group Cache Alpha"
                         filter:@"Group Cache"
                     descending:NO
               expectedOldNames:@[ @"Group Cache Middle" ]
               expectedNewNames:@[ @"Group Cache Alpha", @"Group Cache Middle" ]
               overlappingFetch:NO];
}

- (void)testPendingGroupFetchPreservesRenameNewlyMatchingFilter
{
    [self assertGroupRenameFrom:@"Outside Filter"
                         toName:@"Group Cache Alpha"
                         filter:@"Group Cache"
                     descending:NO
               expectedOldNames:@[ @"Group Cache Middle" ]
               expectedNewNames:@[ @"Group Cache Alpha", @"Group Cache Middle" ]
               overlappingFetch:YES];
}

- (void)testGroupAdditionRefreshSurvivesUnrelatedUpdate
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestGroupCache overlappingChange:VLCLibraryModelOverlapUpdate];
}

- (void)testGroupAdditionRefreshSurvivesUnrelatedDeletion
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestGroupCache overlappingChange:VLCLibraryModelOverlapDeletion];
}

- (void)testShowAdditionRefreshWithoutOverlap
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestShowCache overlappingChange:VLCLibraryModelOverlapNoChange];
}

- (void)testShowAdditionRefreshSurvivesExistingEpisodeUpdate
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestShowCache overlappingChange:VLCLibraryModelOverlapUpdate];
}

- (void)testShowAdditionRefreshSurvivesExistingEpisodeDeletion
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestShowCache overlappingChange:VLCLibraryModelOverlapDeletion];
}

- (void)testRecentAudioHistoryRefreshWithoutOverlap
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestRecentAudioCache overlappingChange:VLCLibraryModelOverlapNoChange];
}

- (void)testRecentAudioHistoryRefreshSurvivesUnrelatedUpdate
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestRecentAudioCache overlappingChange:VLCLibraryModelOverlapUpdate];
}

- (void)testRecentAudioHistoryRefreshSurvivesUnrelatedDeletion
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestRecentAudioCache overlappingChange:VLCLibraryModelOverlapDeletion];
}

- (void)testRecentVideoHistoryRefreshWithoutOverlap
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestRecentVideoCache overlappingChange:VLCLibraryModelOverlapNoChange];
}

- (void)testRecentVideoHistoryRefreshSurvivesUnrelatedUpdate
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestRecentVideoCache overlappingChange:VLCLibraryModelOverlapUpdate];
}

- (void)testRecentVideoHistoryRefreshSurvivesUnrelatedDeletion
{
    [self assertRefreshForCacheKind:VLCLibraryModelTestRecentVideoCache overlappingChange:VLCLibraryModelOverlapDeletion];
}

@end

@interface VLCLibraryModelMediaCachesTest : VLCLibraryModelFixtureTestCase
@end

@implementation VLCLibraryModelMediaCachesTest

- (VLCMediaLibraryMediaItem *)freshMediaItemWithID:(int64_t)libraryID
{
    VLCMediaLibraryMediaItem * const item = [VLCMediaLibraryMediaItem mediaItemForLibraryID:libraryID];
    XCTAssertNotNil(item);
    return item;
}

- (void)prepareCachesOfKind:(VLCLibraryModelTestCacheKind)kind
{
    self.fixture = [[VLCLibraryModelTestOverlapFixture alloc] initWithCacheKind:kind];
    XCTAssertNotNil(self.fixture);
    vlc_medialibrary_t * const library = VLCLibraryDataTypesIntegrationMediaLibrary();
    const int64_t changedID = self.fixture.changedItem.libraryID;
    const int64_t pendingID = self.fixture.pendingItem.libraryID;
    XCTAssertEqual(vlc_ml_media_set_favorite(library, changedID, false), VLC_SUCCESS);
    XCTAssertEqual(vlc_ml_media_set_favorite(library, pendingID, false), VLC_SUCCESS);
    XCTAssertEqual(vlc_ml_media_set_played(library, changedID, true), VLC_SUCCESS);
    XCTAssertEqual(vlc_ml_media_set_played(library, pendingID, true), VLC_SUCCESS);
    VLCMediaLibraryMediaItem * const changed = [self freshMediaItemWithID:changedID];
    VLCMediaLibraryMediaItem * const pending = [self freshMediaItemWithID:pendingID];
    XCTAssertFalse(changed.favorited);
    XCTAssertFalse(pending.favorited);
    [self.fixture seedCacheWithItems:@[ changed, pending ]];
    if (kind == VLCLibraryModelTestAudioCache)
        self.fixture.model.cachedRecentAudioMedia = @[ pending, changed ];
    else
        self.fixture.model.cachedRecentMedia = @[ pending, changed ];
    XCTAssertEqual(self.fixture.cachedItems.count, (NSUInteger)2);
    XCTAssertEqual([self recentItemsForKind:kind].count, (NSUInteger)2);
}

- (NSArray<VLCMediaLibraryMediaItem *> *)recentItemsForKind:(VLCLibraryModelTestCacheKind)kind
{
    return kind == VLCLibraryModelTestAudioCache
        ? self.fixture.model.cachedRecentAudioMedia
        : self.fixture.model.cachedRecentMedia;
}

- (XCTestExpectation *)expectChangesNamed:(NSNotificationName)name
                                      IDs:(NSArray<NSNumber *> *)IDs
{
    NSMutableSet<NSNumber *> * const notifiedIDs = NSMutableSet.set;
    XCTestExpectation * const changed = [self expectationForNotification:name
                                                                  object:nil
                                                                 handler:^BOOL(NSNotification *notification) {
        VLCMediaLibraryMediaItem * const item = notification.object;
        NSNumber * const libraryID = @(item.libraryID);
        if (![IDs containsObject:libraryID])
            return NO;
        XCTAssertTrue(NSThread.isMainThread);
        if ([notifiedIDs containsObject:libraryID]) {
            XCTFail(@"Duplicate %@ notification for media %@", name, libraryID);
            return NO;
        }
        [notifiedIDs addObject:libraryID];
        return YES;
    }];
    changed.expectedFulfillmentCount = IDs.count;
    return changed;
}

- (void)assertChangesToSharedCachesOfKind:(VLCLibraryModelTestCacheKind)kind
                       deletingSecondItem:(BOOL)deletingSecondItem
{
    [self prepareCachesOfKind:kind];
    VLCLibraryModelTestQueryGate * const model = self.fixture.model;
    const int64_t changedID = self.fixture.changedItem.libraryID;
    const int64_t pendingID = self.fixture.pendingItem.libraryID;
    const BOOL audio = kind == VLCLibraryModelTestAudioCache;
    NSNotificationName const mainUpdate = audio ? VLCLibraryModelAudioMediaItemUpdated : VLCLibraryModelVideoMediaItemUpdated;
    NSNotificationName const recentUpdate = audio ? VLCLibraryModelRecentAudioMediaItemUpdated : VLCLibraryModelRecentsMediaItemUpdated;
    NSMutableArray<XCTestExpectation *> * const changes = NSMutableArray.array;
    NSArray<NSNumber *> * const updatedIDs = deletingSecondItem ? @[ @(changedID) ] : @[ @(changedID), @(pendingID) ];
    [changes addObject:[self expectChangesNamed:mainUpdate IDs:updatedIDs]];
    [changes addObject:[self expectChangesNamed:recentUpdate IDs:updatedIDs]];
    if (deletingSecondItem) {
        NSNotificationName const mainDeletion = audio ? VLCLibraryModelAudioMediaItemDeleted : VLCLibraryModelVideoMediaItemDeleted;
        NSNotificationName const recentDeletion = audio ? VLCLibraryModelRecentAudioMediaItemDeleted : VLCLibraryModelRecentsMediaItemDeleted;
        [changes addObject:[self expectChangesNamed:mainDeletion IDs:@[ @(pendingID) ]]];
        [changes addObject:[self expectChangesNamed:recentDeletion IDs:@[ @(pendingID) ]]];
    }
    XCTestExpectation * const queueHeld = [self expectationWithDescription:@"Shared media-cache queue held"];
    [model holdWritesFollowingNextCacheReadWithHandler:^{
        [queueHeld fulfill];
    }];
    [self addTeardownBlock:^{
        [model releaseCacheReads];
    }];
    (void)self.fixture.cachedItems;

    [self waitForExpectations:@[ queueHeld ] timeout:5.];
    XCTAssertTrue(self.fixture.updateChangedItem);
    [self.fixture deliverUpdateEvent];
    if (deletingSecondItem) {
        XCTAssertTrue(self.fixture.deletePendingItem);
        [self.fixture deliverPendingDeletionEvent];
    } else {
        XCTAssertEqual([self.fixture.pendingItem setFavorite:YES], VLC_SUCCESS);
        [self.fixture deliverUpdateEventForID:pendingID];
    }
    // Both handlers are queued before either can replace a cached list.
    // A second property-setter write would allow these mutations to race.
    [model releaseCacheReads];
    [self waitForExpectations:changes timeout:5.];

    NSArray<VLCMediaLibraryMediaItem *> * const main = (id)self.fixture.cachedItems;
    NSArray<VLCMediaLibraryMediaItem *> * const recent = [self recentItemsForKind:kind];
    NSArray<NSNumber *> * const expectedMain = deletingSecondItem ? @[ @(changedID) ] : @[ @(changedID), @(pendingID) ];
    NSArray<NSNumber *> * const expectedRecent = deletingSecondItem ? @[ @(changedID) ] : @[ @(pendingID), @(changedID) ];
    XCTAssertEqualObjects([self orderedIDs:main], expectedMain);
    XCTAssertEqualObjects([self orderedIDs:recent], expectedRecent);
    for (NSArray<VLCMediaLibraryMediaItem *> * const cache in @[ main, recent ]) {
        for (VLCMediaLibraryMediaItem * const item in cache)
            XCTAssertTrue(item.favorited);
    }
    // Verify the changed snapshots against independent persisted records too.
    XCTAssertTrue([self freshMediaItemWithID:changedID].favorited);
    if (deletingSecondItem)
        XCTAssertNil([VLCMediaLibraryMediaItem mediaItemForLibraryID:pendingID]);
    else
        XCTAssertTrue([self freshMediaItemWithID:pendingID].favorited);
}

- (void)testBackToBackAudioUpdatesAreRetainedInMainAndRecentCaches
{
    [self assertChangesToSharedCachesOfKind:VLCLibraryModelTestAudioCache deletingSecondItem:NO];
}

- (void)testBackToBackVideoUpdatesAreRetainedInMainAndRecentCaches
{
    [self assertChangesToSharedCachesOfKind:VLCLibraryModelTestVideoCache deletingSecondItem:NO];
}

- (void)testAudioUpdateFollowedByDeletionIsRetainedInMainAndRecentCaches
{
    [self assertChangesToSharedCachesOfKind:VLCLibraryModelTestAudioCache deletingSecondItem:YES];
}

- (void)testVideoUpdateFollowedByDeletionIsRetainedInMainAndRecentCaches
{
    [self assertChangesToSharedCachesOfKind:VLCLibraryModelTestVideoCache deletingSecondItem:YES];
}

- (void)testCacheDropNotificationWaitsUntilOwningQueueWritesComplete
{
    [self prepareCachesOfKind:VLCLibraryModelTestAudioCache];
    VLCLibraryModelTestQueryGate * const model = self.fixture.model;
    VLCMediaLibraryMediaItem * const audio = (id)self.fixture.changedItem;
    VLCMediaLibraryMediaItem * const video = [self freshMediaItemWithID:
        VLCLibraryDataTypesIntegrationFactoryVideoMediaID()];
    model.cachedVideoMedia = @[ video ];
    model.cachedRecentMedia = @[ video ];
    model.cachedArtists = @[ [VLCMediaLibraryArtist artistWithID:audio.artistID] ];
    model.cachedAlbums = @[ [VLCMediaLibraryAlbum albumWithID:audio.albumID] ];
    model.cachedGenres = @[ [VLCMediaLibraryGenre genreWithID:audio.genreID] ];
    model.cachedListOfGroups = @[ [VLCMediaLibraryGroup groupWithID:VLCLibraryDataTypesIntegrationGroupID()] ];
    XCTestExpectation * const mapped = [self expectationWithDescription:@"Initial show relationship mapped"];
    XCTestExpectation * const written = [self expectationWithDescription:@"Initial show snapshot written"];
    [model holdNextQueryResultForGetter:@selector(cachedListOfShows)
                                handler:^{
        [mapped fulfill];
    }
                      completionHandler:^{
        [written fulfill];
    }];
    [self addTeardownBlock:^{
        [model releaseQuery];
    }];
    model.cachedListOfShows = @[ [VLCMediaLibraryShow showWithLibraryId:VLCLibraryDataTypesIntegrationShowID()] ];
    [self waitForExpectations:@[ mapped ] timeout:5.];
    [model releaseQuery];
    [self waitForExpectations:@[ written ] timeout:5.];
    XCTAssertEqual(model.cachedListOfShows.count, (NSUInteger)1);
    __block BOOL writesReleased = NO;
    XCTestExpectation * const ready = [self expectationForNotification:VLCLibraryModelAllCachesDropped
                                                                object:model
                                                               handler:^BOOL(NSNotification * __unused notification) {
        XCTAssertTrue(writesReleased);
        XCTAssertTrue(NSThread.isMainThread);
        // Never let a premature notification hide itself by blocking these
        // synchronized reads until the test releases the held owning queue.
        if (!writesReleased)
            return YES;
        XCTAssertNil(model.cachedAudioMedia);
        XCTAssertNil(model.cachedVideoMedia);
        XCTAssertNil(model.cachedRecentAudioMedia);
        XCTAssertNil(model.cachedRecentMedia);
        XCTAssertNil(model.cachedAlbums);
        XCTAssertNil(model.cachedArtists);
        XCTAssertNil(model.cachedGenres);
        XCTAssertNil(model.cachedListOfGroups);
        XCTAssertNil(model.cachedListOfShows);
        XCTAssertNil(model.cachedListOfMovies);
        XCTAssertNil(model.cachedListOfMonitoredFolders);
        XCTAssertNil(model.cachedMediaTitles);
        return YES;
    }];
    XCTestExpectation * const premature = [self expectationForNotification:VLCLibraryModelAllCachesDropped
                                                                    object:model
                                                                   handler:^BOOL(NSNotification * __unused notification) {
        return !writesReleased;
    }];
    premature.inverted = YES;
    XCTestExpectation * const queueHeld = [self expectationWithDescription:@"Media queue delays source clears"];
    [model holdWritesFollowingNextCacheReadWithHandler:^{
        [queueHeld fulfill];
    }];
    [self addTeardownBlock:^{
        writesReleased = YES;
        [model releaseCacheReads];
    }];
    (void)model.cachedAudioMedia;

    [self waitForExpectations:@[ queueHeld ] timeout:5.];
    [model dropCaches];
    [self waitForExpectations:@[ premature ] timeout:0.1];
    writesReleased = YES;
    [model releaseCacheReads];
    [self waitForExpectations:@[ ready ] timeout:5.];
}

@end
