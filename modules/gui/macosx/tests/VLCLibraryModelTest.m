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

#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryModelChangeDelegate.h"

@interface VLCLibraryModel ()
- (instancetype)initWithoutMediaLibrary;
- (vlc_ml_query_params_t)queryParams;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryMediaItem *> *cachedAudioMedia;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryMediaItem *> *cachedVideoMedia;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryAlbum *> *cachedAlbums;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryArtist *> *cachedArtists;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryGenre *> *cachedGenres;
@property (readwrite, nonatomic) NSArray<VLCMediaLibraryGroup *> *cachedListOfGroups;
@property (readwrite, nonatomic) NSArray<NSString *> *cachedMediaTitles;
@end

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

- (void)testFilterChangeDropsAllCachedLists
{
    self.model.cachedAudioMedia = @[ [VLCMediaLibraryMediaItem new] ];
    self.model.cachedVideoMedia = @[ [VLCMediaLibraryMediaItem new] ];
    self.model.cachedAlbums = @[ [VLCMediaLibraryAlbum new] ];
    self.model.cachedArtists = @[ [VLCMediaLibraryArtist new] ];
    self.model.cachedGenres = @[ [VLCMediaLibraryGenre new] ];
    self.model.cachedListOfGroups = @[ [VLCMediaLibraryGroup new] ];
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
    XCTAssertNil(self.model.cachedListOfGroups);
    XCTAssertNil(self.model.cachedMediaTitles);
}

- (void)testUnchangedFilterDoesNotPostCacheDropNotification
{
    XCTestExpectation * const notificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];
    notificationExpectation.inverted = YES;

    self.model.filterString = @"";

    [self waitForExpectations:@[ notificationExpectation ] timeout:0.1];
}

- (void)testSortChangeDropsCachedLists
{
    self.model.cachedVideoMedia = @[ [VLCMediaLibraryMediaItem new] ];

    XCTestExpectation * const notificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];

    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];
    [self waitForExpectations:@[ notificationExpectation ] timeout:1.];

    XCTAssertNil(self.model.cachedVideoMedia);
}

- (void)testUnchangedSortDoesNotPostCacheDropNotification
{
    XCTestExpectation * const notificationExpectation =
        [self expectationForNotification:VLCLibraryModelAllCachesDropped object:self.model handler:nil];
    notificationExpectation.inverted = YES;

    [self.model sortByCriteria:VLC_ML_SORTING_DEFAULT andDescending:NO];

    [self waitForExpectations:@[ notificationExpectation ] timeout:0.1];
}

- (void)testQueryParamsUsesFilterAndSortState
{
    self.model.filterString = @"jazz";
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];

    const vlc_ml_query_params_t queryParams = [self.model queryParams];

    XCTAssertEqual(queryParams.i_sort, VLC_ML_SORTING_ALPHA);
    XCTAssertTrue(queryParams.b_desc);
    XCTAssertNotEqual(queryParams.psz_pattern, (const char *)NULL);
    XCTAssertEqual(strcmp(queryParams.psz_pattern, "jazz"), 0);
}

@end
