/****************************************************************************
 * VLCLibraryDataTypesIntegrationTest.m: real medialibrary wrapper tests
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *****************************************************************************/

#import <XCTest/XCTest.h>

#import "library/VLCInputItem.h"
#import "library/VLCLibraryDataTypes.h"
#import "tests/VLCLibraryDataTypesIntegrationTestSupport.h"

@interface VLCLibraryDataTypesIntegrationTest : XCTestCase
@end

@implementation VLCLibraryDataTypesIntegrationTest

+ (void)setUp
{
    [super setUp];
    XCTAssertTrue(VLCLibraryDataTypesIntegrationStart());
}

+ (void)tearDown
{
    VLCLibraryDataTypesIntegrationStop();
    [super tearDown];
}

- (void)testMediaItemFactoryResolvesPersistedExternalMedia
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];

    XCTAssertNotNil(item);
    XCTAssertEqual(item.libraryID, VLCLibraryDataTypesIntegrationMediaID());
}

- (void)testMediaItemFactoryResolvesPersistedExternalMediaByURL
{
    NSURL * const url = [NSURL URLWithString:@"mock://macosx-datatypes-integration"];
    VLCMediaLibraryMediaItem * const item = [VLCMediaLibraryMediaItem mediaItemForURL:url];

    XCTAssertNotNil(item);
    XCTAssertEqual(item.libraryID, VLCLibraryDataTypesIntegrationMediaID());
}

- (void)testMediaItemExposesPersistedInputItem
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    VLCInputItem * const inputItem = item.inputItem;

    XCTAssertNotNil(inputItem);
    XCTAssertEqualObjects(inputItem.MRL, @"mock://macosx-datatypes-integration");
}

- (void)testMediaItemRatingRoundTripsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertNotNil(item);
    if (item == nil) {
        return;
    }

    item.rating = 1;
    XCTAssertEqual(item.rating, 1);

    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertNotNil(refreshedItem);
    XCTAssertEqual(refreshedItem.rating, 1);

    item.rating = 4;
    XCTAssertEqual(item.rating, 4);

    refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.rating, 4);
}

- (void)testMediaItemFavoritePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertNotNil(item);
    if (item == nil) {
        return;
    }

    XCTAssertEqual([item setFavorite:YES], VLC_SUCCESS);

    VLCMediaLibraryMediaItem * const refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertNotNil(refreshedItem);
    XCTAssertTrue(refreshedItem.favorited);
}

- (void)testMediaItemPlaybackRateRoundTripsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertNotNil(item);
    if (item == nil) {
        return;
    }

    item.lastPlaybackRate = 0.75f;
    XCTAssertEqualWithAccuracy(item.lastPlaybackRate, 0.75f, 0.001f);

    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertNotNil(refreshedItem);
    XCTAssertEqualWithAccuracy(refreshedItem.lastPlaybackRate, 0.75f, 0.001f);

    item.lastPlaybackRate = 1.25f;
    XCTAssertEqualWithAccuracy(item.lastPlaybackRate, 1.25f, 0.001f);

    refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualWithAccuracy(refreshedItem.lastPlaybackRate, 1.25f, 0.001f);
}

- (void)testMediaItemStringPlaybackPreferenceRoundTripsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertNotNil(item);
    if (item == nil) {
        return;
    }

    item.lastAspectRatio = @"4:3";
    XCTAssertEqualObjects(item.lastAspectRatio, @"4:3");

    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertNotNil(refreshedItem);
    XCTAssertEqualObjects(refreshedItem.lastAspectRatio, @"4:3");

    item.lastAspectRatio = @"16:9";
    XCTAssertEqualObjects(item.lastAspectRatio, @"16:9");

    refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualObjects(refreshedItem.lastAspectRatio, @"16:9");
}

@end
