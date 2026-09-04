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

#include <stdint.h>

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

- (VLCMediaLibraryMediaItem *)factoryAudioTrack
{
    XCTAssertTrue(VLCLibraryDataTypesIntegrationPrepareFactoryFixtures());
    VLCMediaLibraryMediaItem * const track =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    XCTAssertNotNil(track);
    return track;
}

- (VLCMediaLibraryMediaItem *)factoryVideoEpisode
{
    XCTAssertTrue(VLCLibraryDataTypesIntegrationPrepareFactoryFixtures());
    VLCMediaLibraryMediaItem * const episode =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryVideoMediaID()];
    XCTAssertNotNil(episode);
    return episode;
}

- (VLCMediaLibraryAlbum *)factoryAlbum
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryAlbum * const album =
        [VLCMediaLibraryAlbum albumWithID:track.albumID];
    XCTAssertNotNil(album);
    return album;
}

- (VLCMediaLibraryGenre *)factoryGenre
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryGenre * const genre =
        [VLCMediaLibraryGenre genreWithID:track.genreID];
    XCTAssertNotNil(genre);
    return genre;
}

- (VLCMediaLibraryArtist *)factoryArtist
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryArtist * const artist = [VLCMediaLibraryArtist artistWithID:track.artistID];
    XCTAssertNotNil(artist);
    return artist;
}

- (VLCMediaLibraryShow *)factoryShow
{
    XCTAssertTrue(VLCLibraryDataTypesIntegrationPrepareFactoryFixtures());
    VLCMediaLibraryShow * const show =
        [VLCMediaLibraryShow showWithLibraryId:VLCLibraryDataTypesIntegrationShowID()];
    XCTAssertNotNil(show);
    return show;
}

- (VLCMediaLibraryGroup *)factoryGroup
{
    XCTAssertTrue(VLCLibraryDataTypesIntegrationPrepareFactoryFixtures());
    VLCMediaLibraryGroup * const group =
        [VLCMediaLibraryGroup groupWithID:VLCLibraryDataTypesIntegrationGroupID()];
    XCTAssertNotNil(group);
    return group;
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

- (void)testMediaItemFactoryRejectsUnknownLibraryID
{
    XCTAssertNil([VLCMediaLibraryMediaItem mediaItemForLibraryID:INT64_MAX]);
}

- (void)testMediaItemFactoryResolvesPersistedExternalMediaByURL
{
    NSURL * const url = [NSURL URLWithString:@"mock://macosx-datatypes-integration"];
    VLCMediaLibraryMediaItem * const item = [VLCMediaLibraryMediaItem mediaItemForURL:url];

    XCTAssertEqual(item.libraryID, VLCLibraryDataTypesIntegrationMediaID());
}

- (void)testMediaItemFactoryRejectsUnknownURL
{
    NSURL * const url = [NSURL URLWithString:@"mock://macosx-datatypes-unknown"];
    XCTAssertNil([VLCMediaLibraryMediaItem mediaItemForURL:url]);
    XCTAssertNil([VLCMediaLibraryMediaItem mediaItemForURL:nil]);
}

- (void)testArtistFactoryRejectsUnknownLibraryID
{
    XCTAssertNil([VLCMediaLibraryArtist artistWithID:INT64_MAX]);
}

- (void)testArtistFactoryResolvesUnknownArtist
{
    VLCMediaLibraryArtist * const artist =
        [VLCMediaLibraryArtist artistWithID:1];

    XCTAssertNotNil(artist);
    XCTAssertEqual(artist.libraryID, (int64_t)1);
    XCTAssertEqualObjects(artist.name, @"Unknown Artist");
}

- (void)testArtistFactoryResolvesVariousArtists
{
    VLCMediaLibraryArtist * const artist =
        [VLCMediaLibraryArtist artistWithID:2];

    XCTAssertNotNil(artist);
    XCTAssertEqual(artist.libraryID, (int64_t)2);
    XCTAssertEqualObjects(artist.name, @"Various Artist");
}

- (void)testAlbumFactoryRejectsUnknownLibraryID
{
    XCTAssertNil([VLCMediaLibraryAlbum albumWithID:INT64_MAX]);
}

- (void)testAlbumFactoryResolvesPersistedAlbum
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryAlbum * const album =
        [VLCMediaLibraryAlbum albumWithID:track.albumID];

    XCTAssertEqual(album.libraryID, track.albumID);
    XCTAssertEqualObjects(album.title, @"Factory Album");
}

- (void)testGenreFactoryRejectsUnknownLibraryID
{
    XCTAssertNil([VLCMediaLibraryGenre genreWithID:INT64_MAX]);
}

- (void)testGenreFactoryResolvesPersistedGenre
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryGenre * const genre =
        [VLCMediaLibraryGenre genreWithID:track.genreID];

    XCTAssertEqual(genre.libraryID, track.genreID);
    XCTAssertEqualObjects(genre.name, @"Rock");
}

- (void)testShowFactoryRejectsUnknownLibraryID
{
    XCTAssertNil([VLCMediaLibraryShow showWithLibraryId:INT64_MAX]);
}

- (void)testShowFactoryResolvesUnknownShow
{
    VLCMediaLibraryShow * const show =
        [VLCMediaLibraryShow showWithLibraryId:1];

    XCTAssertNotNil(show);
    XCTAssertEqual(show.libraryID, (int64_t)1);
    XCTAssertEqualObjects(show.name, @"");
}

- (void)testShowFactoryResolvesPersistedShow
{
    VLCMediaLibraryShow * const show = [self factoryShow];

    XCTAssertEqual(show.libraryID, VLCLibraryDataTypesIntegrationShowID());
    XCTAssertEqualObjects(show.name, @"Factory Show");
}

- (void)testGroupFactoryRejectsUnknownLibraryID
{
    XCTAssertNil([VLCMediaLibraryGroup groupWithID:INT64_MAX]);
}

- (void)testGroupFactoryResolvesPersistedGroup
{
    VLCMediaLibraryGroup * const group = [self factoryGroup];

    XCTAssertEqual(group.libraryID, VLCLibraryDataTypesIntegrationGroupID());
    XCTAssertEqualObjects(group.displayString, @"Factory Show S01E01");
}

- (void)testArtistArtistsRelationshipContainsTheArtist
{
    VLCMediaLibraryArtist * const artist = [self factoryArtist];

    XCTAssertEqual(artist.artists.count, (NSUInteger)1);
    XCTAssertEqual(artist.artists.firstObject.libraryID, artist.libraryID);
}

- (void)testArtistAlbumsRelationshipContainsTheAlbum
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryArtist * const artist = [self factoryArtist];

    NSArray<VLCMediaLibraryAlbum *> * const albums = artist.albums;
    XCTAssertEqual(albums.count, (NSUInteger)1);
    XCTAssertEqual(albums.firstObject.libraryID, track.albumID);
}

- (void)testArtistGenresRelationshipContainsTheGenre
{
    VLCMediaLibraryArtist * const artist = [self factoryArtist];
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];

    NSArray<VLCMediaLibraryGenre *> * const genres = artist.genres;
    XCTAssertEqual(genres.count, (NSUInteger)1);
    XCTAssertEqual(genres.firstObject.libraryID, track.genreID);
}

- (void)testArtistMediaItemsRelationshipContainsTheAlbumTrack
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryArtist * const artist = [self factoryArtist];

    NSArray<VLCMediaLibraryMediaItem *> * const mediaItems = artist.mediaItems;
    XCTAssertEqual(mediaItems.count, (NSUInteger)1);
    XCTAssertEqual(mediaItems.firstObject.libraryID, track.libraryID);
    XCTAssertEqual(mediaItems.firstObject.artistID, artist.libraryID);
}

- (void)testArtistSecondaryActionableDetailResolvesItsGenre
{
    VLCMediaLibraryArtist * const artist = [self factoryArtist];
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];

    XCTAssertEqual(artist.secondaryActionableDetailLibraryItem.libraryID,
                   track.genreID);
}

- (void)testArtistEnumerationTraversesAlbumTracks
{
    VLCMediaLibraryArtist * const artist = [self factoryArtist];
    __block NSUInteger enumeratedCount = 0;
    __block int64_t enumeratedID = 0;

    [artist enumerateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem * const item,
                                           BOOL * const __unused stop) {
        enumeratedCount++;
        enumeratedID = item.libraryID;
    }];

    XCTAssertEqual(enumeratedCount, (NSUInteger)1);
    XCTAssertNotEqual(enumeratedID, (int64_t)0);
}

- (void)testArtistIterationTraversesAlbumTracks
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryArtist * const artist = [self factoryArtist];
    __block NSUInteger iteratedCount = 0;

    [artist iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem * const item) {
        XCTAssertEqual(item.libraryID, track.libraryID);
        iteratedCount++;
    }];

    XCTAssertEqual(iteratedCount, (NSUInteger)1);
}

- (void)testAlbumArtistsRelationshipResolvesThePersistedArtist
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryAlbum * const album = [self factoryAlbum];

    NSArray<VLCMediaLibraryArtist *> * const artists = album.artists;
    XCTAssertEqual(artists.count, (NSUInteger)1);
    XCTAssertEqual(artists.firstObject.libraryID, track.artistID);
}

- (void)testAlbumAlbumsRelationshipContainsTheAlbum
{
    VLCMediaLibraryAlbum * const album = [self factoryAlbum];

    XCTAssertEqual(album.albums.count, (NSUInteger)1);
    XCTAssertEqual(album.albums.firstObject.libraryID, album.libraryID);
}

- (void)testAlbumGenresRelationshipContainsThePersistedGenre
{
    VLCMediaLibraryAlbum * const album = [self factoryAlbum];
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];

    NSArray<VLCMediaLibraryGenre *> * const genres = album.genres;
    XCTAssertEqual(genres.count, (NSUInteger)1);
    XCTAssertEqual(genres.firstObject.libraryID, track.genreID);
}

- (void)testAlbumMediaItemsRelationshipContainsTheAlbumTrack
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryAlbum * const album = [self factoryAlbum];

    NSArray<VLCMediaLibraryMediaItem *> * const mediaItems = album.mediaItems;
    XCTAssertEqual(mediaItems.count, (NSUInteger)1);
    XCTAssertEqual(mediaItems.firstObject.libraryID, track.libraryID);
    XCTAssertEqual(mediaItems.firstObject.albumID, album.libraryID);
}

- (void)testAlbumPrimaryActionableDetailResolvesItsArtist
{
    VLCMediaLibraryAlbum * const album = [self factoryAlbum];

    XCTAssertEqual(album.primaryActionableDetailLibraryItem.libraryID, album.artistID);
}

- (void)testAlbumSecondaryActionableDetailResolvesItsGenre
{
    VLCMediaLibraryAlbum * const album = [self factoryAlbum];
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];

    XCTAssertEqual(album.secondaryActionableDetailLibraryItem.libraryID,
                   track.genreID);
}

- (void)testGenreArtistsRelationshipContainsThePersistedArtist
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryGenre * const genre = [self factoryGenre];

    NSArray<VLCMediaLibraryArtist *> * const artists = genre.artists;
    XCTAssertEqual(artists.count, (NSUInteger)1);
    XCTAssertEqual(artists.firstObject.libraryID, track.artistID);
}

- (void)testGenreAlbumsRelationshipContainsThePersistedAlbum
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryGenre * const genre = [self factoryGenre];

    NSArray<VLCMediaLibraryAlbum *> * const albums = genre.albums;
    XCTAssertEqual(albums.count, (NSUInteger)1);
    XCTAssertEqual(albums.firstObject.libraryID, track.albumID);
}

- (void)testGenreGenresRelationshipContainsTheGenre
{
    VLCMediaLibraryGenre * const genre = [self factoryGenre];

    XCTAssertEqual(genre.genres.count, (NSUInteger)1);
    XCTAssertEqual(genre.genres.firstObject.libraryID, genre.libraryID);
}

- (void)testGenreMediaItemsRelationshipContainsTheAlbumTrack
{
    VLCMediaLibraryMediaItem * const track = [self factoryAudioTrack];
    VLCMediaLibraryGenre * const genre = [self factoryGenre];

    NSArray<VLCMediaLibraryMediaItem *> * const mediaItems = genre.mediaItems;
    XCTAssertEqual(mediaItems.count, (NSUInteger)1);
    XCTAssertEqual(mediaItems.firstObject.libraryID, track.libraryID);
    XCTAssertEqual(mediaItems.firstObject.genreID, genre.libraryID);
}

- (void)testShowEpisodesRelationshipContainsThePersistedEpisode
{
    VLCMediaLibraryMediaItem * const episode = [self factoryVideoEpisode];
    VLCMediaLibraryShow * const show = [self factoryShow];

    NSArray<VLCMediaLibraryMediaItem *> * const episodes = show.episodes;
    XCTAssertEqual(episodes.count, (NSUInteger)1);
    XCTAssertEqual(episodes.firstObject.libraryID, episode.libraryID);
    XCTAssertEqual(episodes.firstObject.showEpisode.seasonNumber,
                   episode.showEpisode.seasonNumber);
    XCTAssertEqual(episodes.firstObject.showEpisode.episodeNumber,
                   episode.showEpisode.episodeNumber);
}

- (void)testShowMediaItemsRelationshipAliasesEpisodes
{
    VLCMediaLibraryShow * const show = [self factoryShow];

    XCTAssertEqual(show.mediaItems.count, show.episodes.count);
    XCTAssertEqual(show.mediaItems.firstObject.libraryID, show.episodes.firstObject.libraryID);
}

- (void)testGroupMediaItemsRelationshipContainsThePersistedEpisode
{
    VLCMediaLibraryMediaItem * const episode = [self factoryVideoEpisode];
    VLCMediaLibraryGroup * const group = [self factoryGroup];

    NSArray<VLCMediaLibraryMediaItem *> * const mediaItems = group.mediaItems;
    XCTAssertEqual(mediaItems.count, (NSUInteger)1);
    XCTAssertEqual(mediaItems.firstObject.libraryID, episode.libraryID);
}

- (void)testGroupFirstMediaItemMatchesItsMediaItemsRelationship
{
    VLCMediaLibraryGroup * const group = [self factoryGroup];

    XCTAssertEqual(group.firstMediaItem.libraryID, group.mediaItems.firstObject.libraryID);
}

- (void)testPlaylistFactoryResolvesPersistedPlaylist
{
    VLCMediaLibraryPlaylist * const playlist = [self integrationPlaylist];
    const int64_t playlistID = playlist.libraryID;

    VLCMediaLibraryPlaylist * const refreshedPlaylist =
        [VLCMediaLibraryPlaylist playlistForLibraryID:playlistID];
    XCTAssertNotNil(refreshedPlaylist);
    XCTAssertEqual(refreshedPlaylist.libraryID, playlistID);

    vlc_ml_playlist_delete(VLCLibraryDataTypesIntegrationMediaLibrary(), playlistID);
}

- (void)testPlaylistFactoryRejectsUnknownLibraryID
{
    XCTAssertNil([VLCMediaLibraryPlaylist playlistForLibraryID:INT64_MAX]);
}

- (void)testMediaItemExposesPersistedInputItem
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    VLCInputItem * const inputItem = item.inputItem;

    XCTAssertNotNil(inputItem);
    XCTAssertEqualObjects(inputItem.MRL, @"mock://macosx-datatypes-integration");
}

- (void)testStreamURLInitializerCreatesPersistedStream
{
    NSURL * const url = [NSURL URLWithString:@"mock://macosx-datatypes-stream"];
    VLCMediaLibraryMediaItem * const item = [[VLCMediaLibraryMediaItem alloc] initWithStreamURL:url];

    VLCMediaLibraryMediaItem * const refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:item.libraryID];
    XCTAssertNotNil(refreshedItem);
    XCTAssertEqualObjects(refreshedItem.inputItem.MRL, url.absoluteString);
    XCTAssertEqual(vlc_ml_remove_stream(VLCLibraryDataTypesIntegrationMediaLibrary(), item.libraryID), VLC_SUCCESS);
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

- (void)testMediaItemTitlePreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastTitle = 3;
    XCTAssertEqual(item.lastTitle, 3);
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastTitle, 3);

    item.lastTitle = 8;
    XCTAssertEqual(item.lastTitle, 8);
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastTitle, 8);
}

- (void)testMediaItemChapterPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastChapter = 7;
    XCTAssertEqual(item.lastChapter, 7);
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastChapter, 7);

    item.lastChapter = 11;
    XCTAssertEqual(item.lastChapter, 11);
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastChapter, 11);
}

- (void)testMediaItemProgramPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastProgram = 2;
    XCTAssertEqual(item.lastProgram, 2);
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastProgram, 2);

    item.lastProgram = 5;
    XCTAssertEqual(item.lastProgram, 5);
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastProgram, 5);
}

- (void)testMediaItemVideoTrackPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastVideoTrack = 4;
    XCTAssertEqual(item.lastVideoTrack, 4);
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastVideoTrack, 4);

    item.lastVideoTrack = 9;
    XCTAssertEqual(item.lastVideoTrack, 9);
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastVideoTrack, 9);
}

- (void)testMediaItemZoomPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastZoom = @"1.5";
    XCTAssertEqualObjects(item.lastZoom, @"1.5");
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualObjects(refreshedItem.lastZoom, @"1.5");

    item.lastZoom = @"2.0";
    XCTAssertEqualObjects(item.lastZoom, @"2.0");
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualObjects(refreshedItem.lastZoom, @"2.0");
}

- (void)testMediaItemCropPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastCrop = @"16:9";
    XCTAssertEqualObjects(item.lastCrop, @"16:9");
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualObjects(refreshedItem.lastCrop, @"16:9");

    item.lastCrop = @"4:3";
    XCTAssertEqualObjects(item.lastCrop, @"4:3");
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualObjects(refreshedItem.lastCrop, @"4:3");
}

- (void)testMediaItemDeinterlacePreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastDeinterlaceFilter = @"yadif";
    XCTAssertEqualObjects(item.lastDeinterlaceFilter, @"yadif");
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualObjects(refreshedItem.lastDeinterlaceFilter, @"yadif");

    item.lastDeinterlaceFilter = @"bwdif";
    XCTAssertEqualObjects(item.lastDeinterlaceFilter, @"bwdif");
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualObjects(refreshedItem.lastDeinterlaceFilter, @"bwdif");
}

- (void)testMediaItemVideoFiltersPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastVideoFilters = @"sepia";
    XCTAssertEqualObjects(item.lastVideoFilters, @"sepia");
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualObjects(refreshedItem.lastVideoFilters, @"sepia");

    item.lastVideoFilters = @"grayscale";
    XCTAssertEqualObjects(item.lastVideoFilters, @"grayscale");
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualObjects(refreshedItem.lastVideoFilters, @"grayscale");
}

- (void)testMediaItemAudioTrackPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastAudioTrack = 5;
    XCTAssertEqual(item.lastAudioTrack, 5);
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastAudioTrack, 5);

    item.lastAudioTrack = 8;
    XCTAssertEqual(item.lastAudioTrack, 8);
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastAudioTrack, 8);
}

- (void)testMediaItemGainPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastGain = 1.25f;
    XCTAssertEqualWithAccuracy(item.lastGain, 1.25f, 0.001f);
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualWithAccuracy(refreshedItem.lastGain, 1.25f, 0.001f);

    item.lastGain = 0.75f;
    XCTAssertEqualWithAccuracy(item.lastGain, 0.75f, 0.001f);
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqualWithAccuracy(refreshedItem.lastGain, 0.75f, 0.001f);
}

- (void)testMediaItemAudioDelayPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastAudioDelay = 120;
    XCTAssertEqual(item.lastAudioDelay, 120);
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastAudioDelay, 120);

    item.lastAudioDelay = -240;
    XCTAssertEqual(item.lastAudioDelay, -240);
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastAudioDelay, -240);
}

- (void)testMediaItemSubtitleTrackPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastSubtitleTrack = 6;
    XCTAssertEqual(item.lastSubtitleTrack, 6);
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastSubtitleTrack, 6);

    item.lastSubtitleTrack = 10;
    XCTAssertEqual(item.lastSubtitleTrack, 10);
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastSubtitleTrack, 10);
}

- (void)testMediaItemSubtitleDelayPreferencePersistsThroughMediaLibrary
{
    VLCMediaLibraryMediaItem * const item = [self integrationMediaItem];

    item.lastSubtitleDelay = -80;
    XCTAssertEqual(item.lastSubtitleDelay, -80);
    VLCMediaLibraryMediaItem *refreshedItem =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastSubtitleDelay, -80);

    item.lastSubtitleDelay = 160;
    XCTAssertEqual(item.lastSubtitleDelay, 160);
    refreshedItem = [VLCMediaLibraryMediaItem mediaItemForLibraryID:VLCLibraryDataTypesIntegrationMediaID()];
    XCTAssertEqual(refreshedItem.lastSubtitleDelay, 160);
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
