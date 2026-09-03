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

- (VLCMediaLibraryMediaItem *)integrationMediaItem
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertNotNil(item);
    return item;
}

- (VLCMediaLibraryPlaylist *)integrationPlaylist
{
    const int64_t playlistID = VLCLibraryDataTypesIntegrationCreatePlaylist();
    XCTAssertNotEqual(playlistID, (int64_t)0);

    VLCMediaLibraryPlaylist * const playlist =
        [VLCMediaLibraryPlaylist playlistForLibraryID:playlistID];
    XCTAssertNotNil(playlist);
    return playlist;
}

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
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    XCTAssertEqual(item.libraryID, VLCLibraryDataTypesIntegrationMediaID());
}

- (void)testMediaItemFactoryResolvesPersistedExternalMediaByURL
{
    NSURL * const url = [NSURL URLWithString:@"mock://macosx-datatypes-integration"];
    VLCMediaLibraryMediaItem * const item = [VLCMediaLibraryMediaItem mediaItemForURL:url];

    XCTAssertEqual(item.libraryID, VLCLibraryDataTypesIntegrationMediaID());
}

- (void)testMediaItemExposesPersistedInputItem
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    VLCInputItem * const inputItem = item.inputItem;

    XCTAssertNotNil(inputItem);
    XCTAssertEqualObjects(inputItem.MRL, @"mock://macosx-datatypes-integration");
}

- (void)testMediaItemRatingRoundTripsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

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
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    XCTAssertEqual([item setFavorite:YES], VLC_SUCCESS);

    VLCMediaLibraryMediaItem * const refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertNotNil(refreshedItem);
    XCTAssertTrue(refreshedItem.favorited);
}

- (void)testMediaItemPlaybackRateRoundTripsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

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
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

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

- (void)testPlaylistAppendMedia
{
    VLCMediaLibraryPlaylist * const playlist = [self integrationPlaylist];
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];
    XCTAssertTrue([playlist appendMediaItems:@[item]]);
    XCTAssertEqual(playlist.mediaItems.count, (NSUInteger)1);

    vlc_ml_playlist_delete(VLCLibraryDataTypesIntegrationMediaLibrary(), playlist.libraryID);
}

- (void)testPlaylistAppendPersistsMediaCount
{
    VLCMediaLibraryPlaylist * const playlist = [self integrationPlaylist];
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    XCTAssertTrue([playlist appendMediaItems:@[item]]);
    VLCMediaLibraryPlaylist * const refreshedPlaylist =
        [VLCMediaLibraryPlaylist playlistForLibraryID:playlist.libraryID];
    XCTAssertEqual(refreshedPlaylist.numberOfMedia, (unsigned int)1);
    XCTAssertEqual(refreshedPlaylist.mediaItems.firstObject.libraryID, item.libraryID);

    vlc_ml_playlist_delete(VLCLibraryDataTypesIntegrationMediaLibrary(), playlist.libraryID);
}

- (void)testPlaylistAppendPreservesMediaOrder
{
    vlc_ml_media_t * const secondMedia =
        vlc_ml_new_external_media(VLCLibraryDataTypesIntegrationMediaLibrary(),
                                  "mock://macosx-datatypes-second-item");
    VLCMediaLibraryPlaylist * const playlist = [self integrationPlaylist];
    VLCMediaLibraryMediaItem * const firstItem = [self integrationMediaItem];
    VLCMediaLibraryMediaItem * const secondItem = secondMedia == NULL ? nil :
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:secondMedia->i_id];
    if (secondMedia != NULL) {
        vlc_ml_media_release(secondMedia);
    }

    NSArray<VLCMediaLibraryMediaItem *> * const items = @[firstItem, secondItem];
    XCTAssertTrue([playlist appendMediaItems:items]);
    VLCMediaLibraryPlaylist * const refreshedPlaylist =
        [VLCMediaLibraryPlaylist playlistForLibraryID:playlist.libraryID];
    XCTAssertEqual(refreshedPlaylist.mediaItems.count, (NSUInteger)2);
    XCTAssertEqual(refreshedPlaylist.mediaItems[0].libraryID, firstItem.libraryID);
    XCTAssertEqual(refreshedPlaylist.mediaItems[1].libraryID, secondItem.libraryID);

    vlc_ml_playlist_delete(VLCLibraryDataTypesIntegrationMediaLibrary(), playlist.libraryID);
}

- (void)testPlaylistRenamePersistsThroughMediaLibrary
{
    VLCMediaLibraryPlaylist *playlist = [self integrationPlaylist];

    XCTAssertTrue([playlist renameTo:@"Renamed Integration Playlist"]);
    playlist = [VLCMediaLibraryPlaylist playlistForLibraryID:playlist.libraryID];
    XCTAssertEqualObjects(playlist.displayString, @"Renamed Integration Playlist");

    vlc_ml_playlist_delete(VLCLibraryDataTypesIntegrationMediaLibrary(), playlist.libraryID);
}

- (void)testPlaylistRemoveMediaInvalidatesCachedMediaItems
{
    VLCMediaLibraryPlaylist * const playlist = [self integrationPlaylist];
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];
    XCTAssertTrue([playlist appendMediaItems:@[item]]);
    XCTAssertEqual(playlist.mediaItems.count, (NSUInteger)1);

    [playlist removeMediaItemsAtPositions:@[@0]];
    XCTAssertEqual(playlist.mediaItems.count, (NSUInteger)0);

    VLCMediaLibraryPlaylist * const refreshedPlaylist =
        [VLCMediaLibraryPlaylist playlistForLibraryID:playlist.libraryID];
    XCTAssertEqual(refreshedPlaylist.mediaItems.count, (NSUInteger)0);

    vlc_ml_playlist_delete(VLCLibraryDataTypesIntegrationMediaLibrary(), playlist.libraryID);
}

- (void)testDeletedPlaylistIsNotReturnedByFactory
{
    VLCMediaLibraryPlaylist * const playlist = [self integrationPlaylist];
    XCTAssertEqual(vlc_ml_playlist_delete(VLCLibraryDataTypesIntegrationMediaLibrary(), playlist.libraryID), VLC_SUCCESS);
    XCTAssertNil([VLCMediaLibraryPlaylist playlistForLibraryID:playlist.libraryID]);
}

- (void)testPlaylistFavoriteRoundTripsThroughMediaLibrary
{
    VLCMediaLibraryPlaylist *playlist = [self integrationPlaylist];
    XCTAssertTrue([playlist setFavorite:YES] == VLC_SUCCESS);
    playlist = [VLCMediaLibraryPlaylist playlistForLibraryID:playlist.libraryID];
    XCTAssertTrue(playlist.favorited);

    vlc_ml_playlist_delete(VLCLibraryDataTypesIntegrationMediaLibrary(), playlist.libraryID);
}

@end
