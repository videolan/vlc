/****************************************************************************
 * VLCLibraryModelTest.m: library model tests
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
#import "library/VLCLibraryModelChangeDelegate.h"

@interface VLCLibraryModelTest : XCTestCase
@property (nonatomic, strong) VLCLibraryModel *model;
@property (nonatomic, strong) VLCLibraryModelChangeDelegate *changeDelegate;
@end

@implementation VLCLibraryModelTest

- (void)setUp
{
    [super setUp];

    self.model = [[VLCLibraryModel alloc] initWithoutMediaLibrary];
    self.changeDelegate = self.model.changeDelegate;
    self.changeDelegate.throttleInterval = 0.;
}

- (void)tearDown
{
    self.model = nil;
    self.changeDelegate = nil;
    [super tearDown];
}

- (void)testModelIndexReadsIndexFromNotification
{
    NSNotification * const notification =
        [NSNotification notificationWithName:@"test"
                                      object:nil
                                    userInfo:@{ @"index": @17 }];

    XCTAssertEqual([VLCLibraryModel modelIndexFromModelItemNotification:notification], (NSUInteger)17);
}

- (void)testChangingFilterStoresNewValue
{
    self.model.filterString = @"rock";

    XCTAssertEqualObjects(self.model.filterString, @"rock");
}

- (void)testChangingFilterDropsAllCaches
{
    self.model.cachedAudioMedia = @[ [VLCMediaLibraryMediaItem new] ];
    self.model.cachedVideoMedia = @[ [VLCMediaLibraryMediaItem new] ];
    self.model.cachedAlbums = @[ [VLCMediaLibraryAlbum new] ];
    self.model.cachedArtists = @[ [VLCMediaLibraryArtist new] ];
    self.model.cachedGenres = @[ [VLCMediaLibraryGenre new] ];
    self.model.cachedListOfShows = @[ [VLCMediaLibraryShow new] ];
    self.model.cachedListOfMovies = @[ [VLCMediaLibraryMovie new] ];
    self.model.cachedListOfGroups = @[ [VLCMediaLibraryGroup new] ];
    self.model.cachedRecentMedia = @[ [VLCMediaLibraryMediaItem new] ];
    self.model.cachedRecentAudioMedia = @[ [VLCMediaLibraryMediaItem new] ];
    self.model.cachedListOfMonitoredFolders = @[ [VLCMediaLibraryEntryPoint new] ];
    self.model.cachedMediaTitles = @[ @"title" ];

    XCTestExpectation * const notificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    self.model.filterString = @"rock";
    [self waitForExpectations:@[ notificationExpectation ] timeout:1.];

    XCTAssertEqualObjects(self.model.filterString, @"rock");
    XCTAssertNil(self.model.cachedAudioMedia);
    XCTAssertNil(self.model.cachedVideoMedia);
    XCTAssertNil(self.model.cachedAlbums);
    XCTAssertNil(self.model.cachedArtists);
    XCTAssertNil(self.model.cachedGenres);
    XCTAssertNil(self.model.cachedListOfShows);
    XCTAssertNil(self.model.cachedListOfMovies);
    XCTAssertNil(self.model.cachedListOfGroups);
    XCTAssertNil(self.model.cachedRecentMedia);
    XCTAssertNil(self.model.cachedRecentAudioMedia);
    XCTAssertNil(self.model.cachedListOfMonitoredFolders);
    XCTAssertNil(self.model.cachedMediaTitles);
}

- (void)testClearingFilterPostsCacheDropNotification
{
    XCTestExpectation * const initialNotificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    self.model.filterString = @"rock";
    [self waitForExpectations:@[ initialNotificationExpectation ] timeout:1.];

    XCTestExpectation * const notificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    self.model.filterString = @"";
    [self waitForExpectations:@[ notificationExpectation ] timeout:1.];

    XCTAssertEqualObjects(self.model.filterString, @"");
}

- (void)testUnchangedFilterDoesNotPostCacheDropNotification
{
    XCTestExpectation * const notificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];
    notificationExpectation.inverted = YES;

    self.model.filterString = @"";

    [self waitForExpectations:@[ notificationExpectation ] timeout:0.1];
}

- (void)testRepeatingNonEmptyFilterDoesNotPostCacheDropNotification
{
    XCTestExpectation * const initialNotificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    self.model.filterString = @"rock";
    [self waitForExpectations:@[ initialNotificationExpectation ] timeout:1.];

    XCTestExpectation * const notificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];
    notificationExpectation.inverted = YES;

    self.model.filterString = @"rock";

    [self waitForExpectations:@[ notificationExpectation ] timeout:0.1];
}

- (void)testChangingSortCriteriaDropsCachedLists
{
    self.model.cachedVideoMedia = @[ [VLCMediaLibraryMediaItem new] ];

    XCTestExpectation * const notificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];
    [self waitForExpectations:@[ notificationExpectation ] timeout:1.];

    XCTAssertNil(self.model.cachedVideoMedia);
}

- (void)testChangingSortDirectionDropsCachedLists
{
    XCTestExpectation * const initialNotificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:NO];
    [self waitForExpectations:@[ initialNotificationExpectation ] timeout:1.];

    XCTestExpectation * const notificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];
    [self waitForExpectations:@[ notificationExpectation ] timeout:1.];
}

- (void)testRepeatingSortDoesNotPostCacheDropNotification
{
    XCTestExpectation * const initialNotificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];
    [self waitForExpectations:@[ initialNotificationExpectation ] timeout:1.];

    XCTestExpectation * const notificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];
    notificationExpectation.inverted = YES;

    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];

    [self waitForExpectations:@[ notificationExpectation ] timeout:0.1];
}

- (void)testQueryParamsOmitsEmptyFilterPattern
{
    const vlc_ml_query_params_t queryParams = [self.model queryParams];

    XCTAssertEqual(queryParams.psz_pattern, (const char *)NULL);
    XCTAssertEqual(queryParams.i_sort, VLC_ML_SORTING_DEFAULT);
    XCTAssertFalse(queryParams.b_desc);
}

- (void)testQueryParamsUsesFilterPattern
{
    self.model.filterString = @"jazz";

    const vlc_ml_query_params_t queryParams = [self.model queryParams];

    XCTAssertNotEqual(queryParams.psz_pattern, (const char *)NULL);
    XCTAssertEqual(strcmp(queryParams.psz_pattern, "jazz"), 0);
}

- (void)testQueryParamsPreservesFilterAndSortTogether
{
    self.model.filterString = @"jazz";
    [self.model sortByCriteria:VLC_ML_SORTING_DURATION andDescending:YES];

    const vlc_ml_query_params_t queryParams = [self.model queryParams];

    XCTAssertNotEqual(queryParams.psz_pattern, (const char *)NULL);
    XCTAssertEqual(strcmp(queryParams.psz_pattern, "jazz"), 0);
    XCTAssertEqual(queryParams.i_sort, VLC_ML_SORTING_DURATION);
    XCTAssertTrue(queryParams.b_desc);
}

- (void)testQueryParamsPreservesUTF8FilterPattern
{
    NSString * const filter = @"Beyoncé";
    self.model.filterString = filter;

    const vlc_ml_query_params_t queryParams = [self.model queryParams];

    XCTAssertNotEqual(queryParams.psz_pattern, (const char *)NULL);
    XCTAssertEqual(strcmp(queryParams.psz_pattern, filter.UTF8String), 0);
}

- (void)testQueryParamsUsesSortState
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];

    const vlc_ml_query_params_t queryParams = [self.model queryParams];

    XCTAssertEqual(queryParams.i_sort, VLC_ML_SORTING_ALPHA);
    XCTAssertTrue(queryParams.b_desc);
}

- (void)testQueryParamsUsesAscendingSortState
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:NO];

    const vlc_ml_query_params_t queryParams = [self.model queryParams];

    XCTAssertEqual(queryParams.i_sort, VLC_ML_SORTING_ALPHA);
    XCTAssertFalse(queryParams.b_desc);
}

@end

#pragma mark - Media title reference counts

@interface VLCLibraryModelTitlesTest : XCTestCase
@property (strong) VLCLibraryModelTestTitleReadGate *model;
@end

@implementation VLCLibraryModelTitlesTest

+ (void)setUp
{
    [super setUp];
    // Media wrappers use the real temporary library while these tests isolate
    // the model's source setters from asynchronous library queries/events.
    XCTAssertTrue(VLCLibraryDataTypesIntegrationStart());
}

+ (void)tearDown
{
    VLCLibraryDataTypesIntegrationStop();
    [super tearDown];
}

- (void)setUp
{
    [super setUp];
    self.continueAfterFailure = NO;
    self.model = [[VLCLibraryModelTestTitleReadGate alloc] initWithoutMediaLibrary];
    self.model.changeDelegate.throttleInterval = 0.;
}

- (void)tearDown
{
    [self.model releaseTitleRead];
    self.model = nil;
    [super tearDown];
}

- (void)seedAudioMedia:(NSArray<VLCMediaLibraryMediaItem *> *)audio
            videoMedia:(NSArray<VLCMediaLibraryMediaItem *> *)video
{
    self.model.cachedAudioMedia = audio;
    self.model.cachedVideoMedia = video;
    // Source getters wait for the writes to submit their title-count changes.
    XCTAssertEqualObjects(self.model.cachedAudioMedia, audio);
    XCTAssertEqualObjects(self.model.cachedVideoMedia, video);
}

- (void)holdTitleUpdatesAfterReadingTitles:(NSArray<NSString *> *)titles
{
    XCTestExpectation * const held = [self expectationWithDescription:@"Title queue held"];
    [self.model holdNextTitleReadWithHandler:^{
        [held fulfill];
    }];
    XCTAssertEqualObjects(self.model.listOfMediaTitles, titles);
    [self waitForExpectations:@[ held ] timeout:5.];
}

- (void)testTitleLookupReturnsSortedAudioAndVideoUnion
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Zulu"),
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 2, @"alpha"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 3, @"Bravo"),
    ]];

    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"alpha", @"Bravo", @"Zulu" ]));
}

- (void)testRepeatedAudioTitleIsReturnedOnlyOnce
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Shared"),
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 2, @"Shared"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 3, @"Video"),
    ]];

    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Shared", @"Video" ]));
}

- (void)testTitleSharedAcrossAudioAndVideoIsReturnedOnlyOnce
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Shared"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 2, @"Shared"),
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 3, @"Video"),
    ]];

    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Shared", @"Video" ]));
}

- (void)testReplacingAudioPreservesTitlesStillReferencedByVideo
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Audio only"),
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 2, @"Shared"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 3, @"Shared"),
    ]];

    self.model.cachedAudioMedia = @[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 4, @"Replacement"),
    ];

    XCTAssertEqual(self.model.cachedAudioMedia.count, (NSUInteger)1);
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Replacement", @"Shared" ]));
}

- (void)testReplacingVideoPreservesTitlesStillReferencedByAudio
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Shared"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 2, @"Shared"),
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 3, @"Video only"),
    ]];

    self.model.cachedVideoMedia = @[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 4, @"Replacement"),
    ];

    XCTAssertEqual(self.model.cachedVideoMedia.count, (NSUInteger)1);
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Replacement", @"Shared" ]));
}

- (void)testReplacingAudioRemovesTitleAfterItsLastSourceReferenceDisappears
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Duplicate"),
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 2, @"Duplicate"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 3, @"Video"),
    ]];

    self.model.cachedAudioMedia = @[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 4, @"Replacement"),
    ];

    XCTAssertEqual(self.model.cachedAudioMedia.count, (NSUInteger)1);
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Replacement", @"Video" ]));
}

- (void)testRepeatedReplacementDoesNotAccumulateTitleReferences
{
    VLCMediaLibraryMediaItem * const audio =
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Audio");
    [self seedAudioMedia:@[ audio ] videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 2, @"Video"),
    ]];

    self.model.cachedAudioMedia = @[ audio ];
    self.model.cachedAudioMedia = @[ audio ];
    self.model.cachedAudioMedia = @[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 3, @"Replacement"),
    ];

    XCTAssertEqual(self.model.cachedAudioMedia.count, (NSUInteger)1);
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Replacement", @"Video" ]));
}

- (void)testClearingAudioPreservesOnlyVideoTitleReferences
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Audio only"),
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 2, @"Shared"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 3, @"Shared"),
    ]];

    self.model.cachedAudioMedia = nil;

    XCTAssertNil(self.model.cachedAudioMedia);
    // Avoid the public getter here: it intentionally queries missing sources.
    XCTAssertEqualObjects(self.model.cachedMediaTitles, (@[ @"Shared" ]));
}

- (void)testClearingVideoPreservesOnlyAudioTitleReferences
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Shared"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 2, @"Shared"),
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 3, @"Video only"),
    ]];

    self.model.cachedVideoMedia = nil;

    XCTAssertNil(self.model.cachedVideoMedia);
    XCTAssertEqualObjects(self.model.cachedMediaTitles, (@[ @"Shared" ]));
}

- (void)testRecentAudioDoesNotAddTitleReferences
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Audio"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 2, @"Video"),
    ]];

    self.model.cachedRecentAudioMedia = @[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 3, @"History only"),
    ];

    XCTAssertEqual(self.model.cachedRecentAudioMedia.count, (NSUInteger)1);
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Audio", @"Video" ]));
}

- (void)testDelayedTitleUpdatesDoNotBlockAudioCacheReplacement
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Original"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 2, @"Video"),
    ]];
    [self holdTitleUpdatesAfterReadingTitles:@[ @"Original", @"Video" ]];

    NSArray * const replacement = @[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 3, @"Replacement"),
    ];
    self.model.cachedAudioMedia = replacement;
    XCTestExpectation * const audioWritten = [self expectationWithDescription:@"Audio write completes while titles wait"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^{
        XCTAssertEqualObjects(self.model.cachedAudioMedia, replacement);
        [audioWritten fulfill];
    });
    [self waitForExpectations:@[ audioWritten ] timeout:5.];
    [self.model releaseTitleRead];

    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Replacement", @"Video" ]));
}

- (void)testDroppingAndRebuildingSourcesDoesNotRetainOldTitleReferences
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Shared"),
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 2, @"Old audio"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 3, @"Shared"),
    ]];
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Old audio", @"Shared" ]));
    XCTestExpectation * const dropped =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    [self.model dropCaches];
    [self waitForExpectations:@[ dropped ] timeout:5.];
    XCTAssertNil(self.model.cachedMediaTitles);
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 4, @"New audio"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 5, @"New video"),
    ]];

    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"New audio", @"New video" ]));
}

- (void)testDroppingCachesDoesNotEraseTitlesRebuiltByNewSourceWrites
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Old audio"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 2, @"Old video"),
    ]];
    [self holdTitleUpdatesAfterReadingTitles:@[ @"Old audio", @"Old video" ]];
    XCTestExpectation * const dropped =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    [self.model dropCaches];
    // A new query can publish its source after a drop starts but before the
    // delayed final title clear. Reproduce those source writes directly.
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 3, @"New audio"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 4, @"New video"),
    ]];
    [self.model releaseTitleRead];
    [self waitForExpectations:@[ dropped ] timeout:5.];

    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"New audio", @"New video" ]));
}

- (void)testOverlappingDropAndRebuildPreservesOnlyTheRebuiltTitleReferences
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Shared"),
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 2, @"Old audio"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 3, @"Shared"),
    ]];
    VLCMediaLibraryMediaItem * const firstAudio =
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 4, @"Shared");
    VLCMediaLibraryMediaItem * const secondAudio =
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 5, @"Shared");
    VLCMediaLibraryMediaItem * const sharedVideo =
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 6, @"Shared");
    VLCMediaLibraryMediaItem * const otherVideo =
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 7, @"Video only");
    [self holdTitleUpdatesAfterReadingTitles:@[ @"Old audio", @"Shared" ]];
    XCTestExpectation * const dropped =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];
    [self.model dropCaches];
    [self seedAudioMedia:@[ firstAudio, secondAudio ] videoMedia:@[ sharedVideo, otherVideo ]];
    [self.model releaseTitleRead];
    [self waitForExpectations:@[ dropped ] timeout:5.];
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Shared", @"Video only" ]));

    self.model.cachedAudioMedia = @[ secondAudio ];
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Shared", @"Video only" ]));
    self.model.cachedAudioMedia = @[];
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Shared", @"Video only" ]));
    self.model.cachedVideoMedia = @[ otherVideo ];
    XCTAssertEqualObjects(self.model.listOfMediaTitles,
                          (@[ @"Video only" ]),
                          @"Dropped references must not keep a title alive after its rebuilt references are gone");
}

- (void)testConsecutiveOverlappingDropsKeepOnlyTheFinalSourceTitles
{
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 1, @"Old audio"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 2, @"Old video"),
    ]];
    [self holdTitleUpdatesAfterReadingTitles:@[ @"Old audio", @"Old video" ]];
    XCTestExpectation * const dropped =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];
    [self.model dropCaches];
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 3, @"Intermediate audio"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 4, @"Intermediate video"),
    ]];
    [self.model dropCaches];
    [self seedAudioMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_AUDIO, 5, @"Final audio"),
    ]
              videoMedia:@[
        VLCLibraryModelTestMediaItem(VLC_ML_MEDIA_TYPE_VIDEO, 6, @"Final video"),
    ]];
    [self.model releaseTitleRead];
    // Cache-drop notifications can be throttled together. Their publication
    // follows the final queued title writes, without clearing the rebuilt list.
    [self waitForExpectations:@[ dropped ] timeout:5.];
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Final audio", @"Final video" ]));
    self.model.cachedAudioMedia = @[];
    XCTAssertEqualObjects(self.model.listOfMediaTitles, (@[ @"Final video" ]));
    self.model.cachedVideoMedia = @[];
    XCTAssertEqualObjects(self.model.listOfMediaTitles,
                          @[],
                          @"Neither the old nor intermediate source titles may retain references");
}

@end
