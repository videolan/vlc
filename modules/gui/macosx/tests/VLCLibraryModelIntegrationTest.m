/****************************************************************************
 * VLCLibraryModelIntegrationTest.m: medialibrary-backed model tests
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
#import "tests/VLCLibraryDataTypesIntegrationTestSupport.h"

static int64_t sSortingAudioAlphaID;
static int64_t sSortingAudioZuluID;
static int64_t sSortingVideoAlphaID;
static int64_t sSortingVideoZuluID;

@interface VLCLibraryModel ()
- (void)applicationWillTerminate:(NSNotification *)notification;
@end

@interface VLCLibraryModelIntegrationTest : XCTestCase
@property (nonatomic, strong) VLCLibraryModel *model;
@end

@implementation VLCLibraryModelIntegrationTest

+ (void)setUp
{
    [super setUp];
    XCTAssertTrue(VLCLibraryDataTypesIntegrationStart());
    XCTAssertTrue(VLCLibraryDataTypesIntegrationPrepareFactoryFixtures());

    /* External records provide deterministic titles without requiring another
     * filesystem discovery pass. */
    vlc_medialibrary_t * const mediaLibrary =
        VLCLibraryDataTypesIntegrationMediaLibrary();
    sSortingAudioAlphaID = VLCLibraryDataTypesIntegrationCreateExternalMedia(
        "file:///tmp/vlc-sort-audio-alpha.mp3");
    sSortingAudioZuluID = VLCLibraryDataTypesIntegrationCreateExternalMedia(
        "file:///tmp/vlc-sort-audio-zulu.mp3");
    sSortingVideoAlphaID = VLCLibraryDataTypesIntegrationCreateExternalMedia(
        "file:///tmp/vlc-sort-video-alpha.mp4");
    sSortingVideoZuluID = VLCLibraryDataTypesIntegrationCreateExternalMedia(
        "file:///tmp/vlc-sort-video-zulu.mp4");

    XCTAssertNotEqual(sSortingAudioAlphaID, (int64_t)0);
    XCTAssertNotEqual(sSortingAudioZuluID, (int64_t)0);
    XCTAssertNotEqual(sSortingVideoAlphaID, (int64_t)0);
    XCTAssertNotEqual(sSortingVideoZuluID, (int64_t)0);
    XCTAssertEqual(vlc_ml_media_set_type(mediaLibrary, sSortingAudioAlphaID,
                                         VLC_ML_MEDIA_TYPE_AUDIO), VLC_SUCCESS);
    XCTAssertEqual(vlc_ml_media_set_type(mediaLibrary, sSortingAudioZuluID,
                                         VLC_ML_MEDIA_TYPE_AUDIO), VLC_SUCCESS);
    XCTAssertEqual(vlc_ml_media_set_type(mediaLibrary, sSortingVideoAlphaID,
                                         VLC_ML_MEDIA_TYPE_VIDEO), VLC_SUCCESS);
    XCTAssertEqual(vlc_ml_media_set_type(mediaLibrary, sSortingVideoZuluID,
                                         VLC_ML_MEDIA_TYPE_VIDEO), VLC_SUCCESS);
}

+ (void)tearDown
{
    vlc_medialibrary_t * const mediaLibrary =
        VLCLibraryDataTypesIntegrationMediaLibrary();
    if (mediaLibrary != NULL && sSortingAudioAlphaID != 0) {
        vlc_ml_remove_stream(mediaLibrary, sSortingAudioAlphaID);
    }
    if (mediaLibrary != NULL && sSortingAudioZuluID != 0) {
        vlc_ml_remove_stream(mediaLibrary, sSortingAudioZuluID);
    }
    if (mediaLibrary != NULL && sSortingVideoAlphaID != 0) {
        vlc_ml_remove_stream(mediaLibrary, sSortingVideoAlphaID);
    }
    if (mediaLibrary != NULL && sSortingVideoZuluID != 0) {
        vlc_ml_remove_stream(mediaLibrary, sSortingVideoZuluID);
    }
    VLCLibraryDataTypesIntegrationStop();
    [super tearDown];
}

- (void)setUp
{
    [super setUp];

    self.model = [[VLCLibraryModel alloc]
                  initWithLibrary:VLCLibraryDataTypesIntegrationMediaLibrary()];
    self.model.changeDelegate.throttleInterval = 0.;
}

- (void)tearDown
{
    [self.model applicationWillTerminate:nil];
    self.model = nil;
    [super tearDown];
}

- (NSArray *)cachedItemsAfterFilter:(NSString *)filter
                   notificationName:(NSString *)notificationName
                             loader:(NSArray *(^)(void))loader
{
    XCTestExpectation * const expectation =
        [self expectationForNotification:notificationName
                                  object:self.model
                                 handler:nil];

    self.model.filterString = filter;
    loader();
    [self waitForExpectations:@[ expectation ] timeout:5.];
    return loader();
}

- (NSArray<VLCMediaLibraryMediaItem *> *)audioMediaAfterFilter:(NSString *)filter
{
    return [self cachedItemsAfterFilter:filter
                       notificationName:VLCLibraryModelAudioMediaListReset
                                 loader:^NSArray *{
        return self.model.listOfAudioMedia;
    }];
}

- (NSArray<VLCMediaLibraryMediaItem *> *)videoMediaAfterFilter:(NSString *)filter
{
    return [self cachedItemsAfterFilter:filter
                       notificationName:VLCLibraryModelVideoMediaListReset
                                 loader:^NSArray *{
        return self.model.listOfVideoMedia;
    }];
}

- (NSArray<VLCMediaLibraryArtist *> *)artistsAfterFilter:(NSString *)filter
{
    return [self cachedItemsAfterFilter:filter
                       notificationName:VLCLibraryModelArtistListReset
                                 loader:^NSArray *{
        return self.model.listOfArtists;
    }];
}

- (NSArray<VLCMediaLibraryAlbum *> *)albumsAfterFilter:(NSString *)filter
{
    return [self cachedItemsAfterFilter:filter
                       notificationName:VLCLibraryModelAlbumListReset
                                 loader:^NSArray *{
        return self.model.listOfAlbums;
    }];
}

- (NSArray<VLCMediaLibraryGenre *> *)genresAfterFilter:(NSString *)filter
{
    return [self cachedItemsAfterFilter:filter
                       notificationName:VLCLibraryModelGenreListReset
                                 loader:^NSArray *{
        return self.model.listOfGenres;
    }];
}

- (NSArray<VLCMediaLibraryGroup *> *)groupsAfterFilter:(NSString *)filter
{
    return [self cachedItemsAfterFilter:filter
                       notificationName:VLCLibraryModelListOfGroupsReset
                                 loader:^NSArray *{
        return self.model.listOfGroups;
    }];
}

- (void)markMediaIDAsRecentlyPlayed:(int64_t)mediaID
{
    vlc_medialibrary_t * const mediaLibrary =
        VLCLibraryDataTypesIntegrationMediaLibrary();
    XCTAssertEqual(vlc_ml_clear_history(mediaLibrary, VLC_ML_HISTORY_TYPE_GLOBAL),
                   VLC_SUCCESS);
    XCTAssertEqual(vlc_ml_media_set_played(mediaLibrary, mediaID, true),
                   VLC_SUCCESS);
}

- (void)clearRecentMediaHistory
{
    XCTAssertEqual(vlc_ml_clear_history(VLCLibraryDataTypesIntegrationMediaLibrary(),
                                        VLC_ML_HISTORY_TYPE_GLOBAL),
                   VLC_SUCCESS);
}

- (NSArray *)itemsAfterFilter:(NSString *)filter
                       loader:(NSArray *(^)(void))loader
{
    self.model.filterString = filter;
    return loader();
}

- (NSArray<VLCMediaLibraryPlaylist *> *)playlistsForSortingWithPrefix:(NSString *)prefix
{
    const int64_t alphaID = VLCLibraryDataTypesIntegrationCreatePlaylist();
    const int64_t zuluID = VLCLibraryDataTypesIntegrationCreatePlaylist();
    XCTAssertNotEqual(alphaID, (int64_t)0);
    XCTAssertNotEqual(zuluID, (int64_t)0);

    VLCMediaLibraryPlaylist * const alpha =
        [VLCMediaLibraryPlaylist playlistForLibraryID:alphaID];
    VLCMediaLibraryPlaylist * const zulu =
        [VLCMediaLibraryPlaylist playlistForLibraryID:zuluID];
    XCTAssertTrue([alpha renameTo:[prefix stringByAppendingString:@" Alpha"]]);
    XCTAssertTrue([zulu renameTo:[prefix stringByAppendingString:@" Zulu"]]);

    self.model.filterString = prefix;
    return [self.model listOfPlaylistsOfType:VLC_ML_PLAYLIST_TYPE_ALL];
}

- (NSArray<VLCMediaLibraryPlaylist *> *)playlistsForDurationSortingWithPrefix:(NSString *)prefix
{
    const int64_t emptyID = VLCLibraryDataTypesIntegrationCreatePlaylist();
    const int64_t filledID = VLCLibraryDataTypesIntegrationCreatePlaylist();
    XCTAssertNotEqual(emptyID, (int64_t)0);
    XCTAssertNotEqual(filledID, (int64_t)0);

    VLCMediaLibraryPlaylist * const empty =
        [VLCMediaLibraryPlaylist playlistForLibraryID:emptyID];
    VLCMediaLibraryPlaylist * const filled =
        [VLCMediaLibraryPlaylist playlistForLibraryID:filledID];
    XCTAssertTrue([empty renameTo:[prefix stringByAppendingString:@" Empty"]]);
    XCTAssertTrue([filled renameTo:[prefix stringByAppendingString:@" Filled"]]);

    VLCMediaLibraryMediaItem * const fixture =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    XCTAssertNotNil(fixture);
    if (fixture == nil) {
        return @[ ];
    }
    XCTAssertTrue([filled appendMediaItems:@[fixture]]);

    self.model.filterString = prefix;
    return [self.model listOfPlaylistsOfType:VLC_ML_PLAYLIST_TYPE_ALL];
}

- (void)testAudioMediaQueryReturnsMatchingFixtureMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self audioMediaAfterFilter:@"Factory Track"];

    XCTAssertEqual(media.count, (NSUInteger)1);
    XCTAssertEqualObjects(media.firstObject.displayString, @"Factory Track");
}

- (void)testAudioMediaQueryRejectsNonMatchingFilter
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self audioMediaAfterFilter:@"No Matching Fixture Media"];

    XCTAssertEqual(media.count, (NSUInteger)0);
}

- (void)testVideoMediaQueryReturnsMatchingFixtureMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self videoMediaAfterFilter:@"Factory Show"];

    XCTAssertEqual(media.count, (NSUInteger)1);
}

- (void)testVideoMediaQueryRejectsNonMatchingFilter
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self videoMediaAfterFilter:@"No Matching Fixture Media"];

    XCTAssertEqual(media.count, (NSUInteger)0);
}

- (void)testPlaylistQuerySortsMatchingItemsInAscendingOrder
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:NO];

    NSArray<VLCMediaLibraryPlaylist *> * const playlists =
        [self playlistsForSortingWithPrefix:@"Sorting Ascending"];

    XCTAssertEqual(playlists.count, (NSUInteger)2);
    XCTAssertEqualObjects(playlists[0].displayString, @"Sorting Ascending Alpha");
    XCTAssertEqualObjects(playlists[1].displayString, @"Sorting Ascending Zulu");
}

- (void)testPlaylistQuerySortsMatchingItemsInDescendingOrder
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];

    NSArray<VLCMediaLibraryPlaylist *> * const playlists =
        [self playlistsForSortingWithPrefix:@"Sorting Descending"];

    XCTAssertEqual(playlists.count, (NSUInteger)2);
    XCTAssertEqualObjects(playlists[0].displayString, @"Sorting Descending Zulu");
    XCTAssertEqualObjects(playlists[1].displayString, @"Sorting Descending Alpha");
}

- (void)testAudioMediaQuerySortsMatchingItemsInAscendingOrder
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:NO];

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self audioMediaAfterFilter:@"sort"];

    XCTAssertEqual(media.count, (NSUInteger)2);
    XCTAssertEqualObjects(media[0].displayString, @"vlc-sort-audio-alpha.mp3");
    XCTAssertEqualObjects(media[1].displayString, @"vlc-sort-audio-zulu.mp3");
}

- (void)testAudioMediaQuerySortsMatchingItemsInDescendingOrder
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self audioMediaAfterFilter:@"sort"];

    XCTAssertEqual(media.count, (NSUInteger)2);
    XCTAssertEqualObjects(media[0].displayString, @"vlc-sort-audio-zulu.mp3");
    XCTAssertEqualObjects(media[1].displayString, @"vlc-sort-audio-alpha.mp3");
}

- (void)testVideoMediaQuerySortsMatchingItemsInAscendingOrder
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:NO];

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self videoMediaAfterFilter:@"sort"];

    XCTAssertEqual(media.count, (NSUInteger)2);
    XCTAssertEqualObjects(media[0].displayString, @"vlc-sort-video-alpha.mp4");
    XCTAssertEqualObjects(media[1].displayString, @"vlc-sort-video-zulu.mp4");
}

- (void)testVideoMediaQuerySortsMatchingItemsInDescendingOrder
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self videoMediaAfterFilter:@"sort"];

    XCTAssertEqual(media.count, (NSUInteger)2);
    XCTAssertEqualObjects(media[0].displayString, @"vlc-sort-video-zulu.mp4");
    XCTAssertEqualObjects(media[1].displayString, @"vlc-sort-video-alpha.mp4");
}

- (void)testPlaylistQuerySortsMatchingItemsByDurationAscending
{
    [self.model sortByCriteria:VLC_ML_SORTING_DURATION andDescending:NO];

    NSArray<VLCMediaLibraryPlaylist *> * const playlists =
        [self playlistsForDurationSortingWithPrefix:@"Sorting Duration Ascending"];

    XCTAssertEqual(playlists.count, (NSUInteger)2);
    XCTAssertEqualObjects(playlists[0].displayString, @"Sorting Duration Ascending Empty");
    XCTAssertEqualObjects(playlists[1].displayString, @"Sorting Duration Ascending Filled");
}

- (void)testPlaylistQuerySortsMatchingItemsByDurationDescending
{
    [self.model sortByCriteria:VLC_ML_SORTING_DURATION andDescending:YES];

    NSArray<VLCMediaLibraryPlaylist *> * const playlists =
        [self playlistsForDurationSortingWithPrefix:@"Sorting Duration Descending"];

    XCTAssertEqual(playlists.count, (NSUInteger)2);
    XCTAssertEqualObjects(playlists[0].displayString, @"Sorting Duration Descending Filled");
    XCTAssertEqualObjects(playlists[1].displayString, @"Sorting Duration Descending Empty");
}

- (void)testFavoriteAudioQuerySortsMatchingItemsInAscendingOrder
{
    VLCMediaLibraryMediaItem * const alpha =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:sSortingAudioAlphaID];
    VLCMediaLibraryMediaItem * const zulu =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:sSortingAudioZuluID];
    XCTAssertEqual([alpha setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([zulu setFavorite:YES], VLC_SUCCESS);
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:NO];

    NSArray<VLCMediaLibraryMediaItem *> * const favorites =
        [self itemsAfterFilter:@"sort"
                         loader:^NSArray *{
        return self.model.listOfFavoriteAudioMedia;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)2);
    XCTAssertEqualObjects(favorites[0].displayString, @"vlc-sort-audio-alpha.mp3");
    XCTAssertEqualObjects(favorites[1].displayString, @"vlc-sort-audio-zulu.mp3");
    XCTAssertEqual([alpha setFavorite:NO], VLC_SUCCESS);
    XCTAssertEqual([zulu setFavorite:NO], VLC_SUCCESS);
}

- (void)testFavoriteAudioQuerySortsMatchingItemsInDescendingOrder
{
    VLCMediaLibraryMediaItem * const alpha =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:sSortingAudioAlphaID];
    VLCMediaLibraryMediaItem * const zulu =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:sSortingAudioZuluID];
    XCTAssertEqual([alpha setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([zulu setFavorite:YES], VLC_SUCCESS);
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];

    NSArray<VLCMediaLibraryMediaItem *> * const favorites =
        [self itemsAfterFilter:@"sort"
                         loader:^NSArray *{
        return self.model.listOfFavoriteAudioMedia;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)2);
    XCTAssertEqualObjects(favorites[0].displayString, @"vlc-sort-audio-zulu.mp3");
    XCTAssertEqualObjects(favorites[1].displayString, @"vlc-sort-audio-alpha.mp3");
    XCTAssertEqual([alpha setFavorite:NO], VLC_SUCCESS);
    XCTAssertEqual([zulu setFavorite:NO], VLC_SUCCESS);
}

- (void)testRecentVideoQueryReturnsMatchingFixtureMedia
{
    [self markMediaIDAsRecentlyPlayed:
        VLCLibraryDataTypesIntegrationFactoryVideoMediaID()];
    self.model.recentMediaLimit = 10;

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self cachedItemsAfterFilter:@"Factory Show"
                    notificationName:VLCLibraryModelRecentsMediaListReset
                              loader:^NSArray *{
        return self.model.listOfRecentMedia;
    }];

    XCTAssertEqual(media.count, (NSUInteger)1);
    XCTAssertEqual(media.firstObject.libraryID,
                   VLCLibraryDataTypesIntegrationFactoryVideoMediaID());
    XCTAssertEqual(self.model.numberOfRecentMedia, (size_t)1);
    [self clearRecentMediaHistory];
}

- (void)testRecentVideoQueryRejectsNonMatchingFilter
{
    [self markMediaIDAsRecentlyPlayed:
        VLCLibraryDataTypesIntegrationFactoryVideoMediaID()];
    self.model.recentMediaLimit = 10;

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self cachedItemsAfterFilter:@"No Matching Recent Video"
                    notificationName:VLCLibraryModelRecentsMediaListReset
                              loader:^NSArray *{
        return self.model.listOfRecentMedia;
    }];

    XCTAssertEqual(media.count, (NSUInteger)0);
    XCTAssertEqual(self.model.numberOfRecentMedia, (size_t)0);
    [self clearRecentMediaHistory];
}

- (void)testRecentAudioQueryReturnsMatchingFixtureMedia
{
    [self markMediaIDAsRecentlyPlayed:
        VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    self.model.recentAudioMediaLimit = 10;

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self cachedItemsAfterFilter:@"Factory Track"
                    notificationName:VLCLibraryModelRecentAudioMediaListReset
                              loader:^NSArray *{
        return self.model.listOfRecentAudioMedia;
    }];

    XCTAssertEqual(media.count, (NSUInteger)1);
    XCTAssertEqual(media.firstObject.libraryID,
                   VLCLibraryDataTypesIntegrationFactoryAudioMediaID());
    XCTAssertEqual(self.model.numberOfRecentAudioMedia, (size_t)1);
    [self clearRecentMediaHistory];
}

- (void)testRecentAudioQueryRejectsNonMatchingFilter
{
    [self markMediaIDAsRecentlyPlayed:
        VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    self.model.recentAudioMediaLimit = 10;

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self cachedItemsAfterFilter:@"No Matching Recent Audio"
                    notificationName:VLCLibraryModelRecentAudioMediaListReset
                              loader:^NSArray *{
        return self.model.listOfRecentAudioMedia;
    }];

    XCTAssertEqual(media.count, (NSUInteger)0);
    XCTAssertEqual(self.model.numberOfRecentAudioMedia, (size_t)0);
    [self clearRecentMediaHistory];
}

- (void)testArtistQueryReturnsMatchingFixtureArtist
{
    NSArray<VLCMediaLibraryArtist *> * const artists =
        [self artistsAfterFilter:@"Factory Artist"];

    XCTAssertEqual(artists.count, (NSUInteger)1);
    XCTAssertEqualObjects(artists.firstObject.name, @"Factory Artist");
}

- (void)testArtistQueryRejectsNonMatchingFilter
{
    NSArray<VLCMediaLibraryArtist *> * const artists =
        [self artistsAfterFilter:@"No Matching Fixture Artist"];

    XCTAssertEqual(artists.count, (NSUInteger)0);
}

- (void)testAlbumQueryReturnsMatchingFixtureAlbum
{
    NSArray<VLCMediaLibraryAlbum *> * const albums =
        [self albumsAfterFilter:@"Factory Album"];

    XCTAssertEqual(albums.count, (NSUInteger)1);
    XCTAssertEqualObjects(albums.firstObject.title, @"Factory Album");
}

- (void)testAlbumQueryRejectsNonMatchingFilter
{
    NSArray<VLCMediaLibraryAlbum *> * const albums =
        [self albumsAfterFilter:@"No Matching Fixture Album"];

    XCTAssertEqual(albums.count, (NSUInteger)0);
}

- (void)testGenreQueryReturnsMatchingFixtureGenre
{
    NSArray<VLCMediaLibraryGenre *> * const genres =
        [self genresAfterFilter:@"Rock"];

    XCTAssertEqual(genres.count, (NSUInteger)1);
    XCTAssertEqualObjects(genres.firstObject.name, @"Rock");
}

- (void)testGenreQueryRejectsNonMatchingFilter
{
    NSArray<VLCMediaLibraryGenre *> * const genres =
        [self genresAfterFilter:@"No Matching Fixture Genre"];

    XCTAssertEqual(genres.count, (NSUInteger)0);
}

- (void)testGroupQueryReturnsMatchingFixtureGroup
{
    NSArray<VLCMediaLibraryGroup *> * const groups =
        [self groupsAfterFilter:@"Factory Show"];

    XCTAssertEqual(groups.count, (NSUInteger)1);
    XCTAssertEqualObjects(groups.firstObject.displayString, @"Factory Show S01E01");
}

- (void)testGroupQueryRejectsNonMatchingFilter
{
    NSArray<VLCMediaLibraryGroup *> * const groups =
        [self groupsAfterFilter:@"No Matching Fixture Group"];

    XCTAssertEqual(groups.count, (NSUInteger)0);
}

- (void)testPlaylistQueryReturnsMatchingFixturePlaylist
{
    const int64_t playlistID = VLCLibraryDataTypesIntegrationCreatePlaylist();
    XCTAssertNotEqual(playlistID, (int64_t)0);

    NSArray<VLCMediaLibraryPlaylist *> * const playlists =
        [self itemsAfterFilter:@"Integration Playlist"
                        loader:^NSArray *{
        return [self.model listOfPlaylistsOfType:VLC_ML_PLAYLIST_TYPE_ALL];
    }];

    VLCMediaLibraryPlaylist * const playlist =
        [playlists filteredArrayUsingPredicate:
            [NSPredicate predicateWithBlock:^BOOL(VLCMediaLibraryPlaylist *item,
                                                   NSDictionary<NSString *,id> * __unused bindings) {
            return item.libraryID == playlistID;
    }]].firstObject;
    XCTAssertNotNil(playlist);
    XCTAssertEqualObjects(playlist.displayString, @"Integration Playlist");
    XCTAssertEqual([self.model numberOfPlaylistsOfType:VLC_ML_PLAYLIST_TYPE_ALL],
                   playlists.count);
}

- (void)testPlaylistQueryRejectsNonMatchingFilter
{
    const int64_t playlistID = VLCLibraryDataTypesIntegrationCreatePlaylist();
    XCTAssertNotEqual(playlistID, (int64_t)0);

    NSArray<VLCMediaLibraryPlaylist *> * const playlists =
        [self itemsAfterFilter:@"No Matching Fixture Playlist"
                        loader:^NSArray *{
        return [self.model listOfPlaylistsOfType:VLC_ML_PLAYLIST_TYPE_ALL];
    }];

    XCTAssertEqual(playlists.count, (NSUInteger)0);
    XCTAssertEqual([self.model numberOfPlaylistsOfType:VLC_ML_PLAYLIST_TYPE_ALL],
                   (size_t)0);
}

- (void)testAlbumsOfArtistQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const track =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    XCTAssertNotNil(track);

    NSArray<VLCMediaLibraryAlbum *> * const albums =
        [self itemsAfterFilter:@"Factory Album"
                        loader:^NSArray *{
        return [self.model listAlbumsOfParentType:VLC_ML_PARENT_ARTIST
                                            forID:track.artistID];
    }];

    XCTAssertEqual(albums.count, (NSUInteger)1);
    XCTAssertEqualObjects(albums.firstObject.title, @"Factory Album");
}

- (void)testAlbumsOfArtistQueryRejectsNonMatchingFilter
{
    VLCMediaLibraryMediaItem * const track =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    XCTAssertNotNil(track);

    NSArray<VLCMediaLibraryAlbum *> * const albums =
        [self itemsAfterFilter:@"No Matching Fixture Album"
                        loader:^NSArray *{
        return [self.model listAlbumsOfParentType:VLC_ML_PARENT_ARTIST
                                            forID:track.artistID];
    }];

    XCTAssertEqual(albums.count, (NSUInteger)0);
}

- (void)testFavoriteAudioQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    XCTAssertNotNil(item);
    XCTAssertEqual([item setFavorite:YES], VLC_SUCCESS);

    NSArray<VLCMediaLibraryMediaItem *> * const favorites =
        [self itemsAfterFilter:@"Factory Track"
                        loader:^NSArray *{
        return self.model.listOfFavoriteAudioMedia;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(favorites.firstObject.libraryID, item.libraryID);
    XCTAssertEqual(self.model.numberOfFavoriteAudioMedia, (size_t)1);

    XCTAssertEqual([item setFavorite:NO], VLC_SUCCESS);
}

- (void)testFavoriteVideoQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryVideoMediaID()];
    XCTAssertNotNil(item);
    XCTAssertEqual([item setFavorite:YES], VLC_SUCCESS);

    NSArray<VLCMediaLibraryMediaItem *> * const favorites =
        [self itemsAfterFilter:@"Factory Show"
                        loader:^NSArray *{
        return self.model.listOfFavoriteVideoMedia;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(favorites.firstObject.libraryID, item.libraryID);
    XCTAssertEqual(self.model.numberOfFavoriteVideoMedia, (size_t)1);

    XCTAssertEqual([item setFavorite:NO], VLC_SUCCESS);
}

- (void)testFavoriteAlbumQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const track =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    VLCMediaLibraryAlbum * const album = [VLCMediaLibraryAlbum albumWithID:track.albumID];
    XCTAssertNotNil(album);
    XCTAssertEqual([album setFavorite:YES], VLC_SUCCESS);

    NSArray<VLCMediaLibraryAlbum *> * const favorites =
        [self itemsAfterFilter:@"Factory Album"
                        loader:^NSArray *{
        return self.model.listOfFavoriteAlbums;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(favorites.firstObject.libraryID, album.libraryID);
    XCTAssertEqual(self.model.numberOfFavoriteAlbums, (size_t)1);

    XCTAssertEqual([album setFavorite:NO], VLC_SUCCESS);
}

- (void)testFavoriteArtistQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const track =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    VLCMediaLibraryArtist * const artist = [VLCMediaLibraryArtist artistWithID:track.artistID];
    XCTAssertNotNil(artist);
    XCTAssertEqual([artist setFavorite:YES], VLC_SUCCESS);

    NSArray<VLCMediaLibraryArtist *> * const favorites =
        [self itemsAfterFilter:@"Factory Artist"
                        loader:^NSArray *{
        return self.model.listOfFavoriteArtists;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(favorites.firstObject.libraryID, artist.libraryID);
    XCTAssertEqual(self.model.numberOfFavoriteArtists, (size_t)1);

    XCTAssertEqual([artist setFavorite:NO], VLC_SUCCESS);
}

- (void)testFavoriteGenreQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const track =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    VLCMediaLibraryGenre * const genre = [VLCMediaLibraryGenre genreWithID:track.genreID];
    XCTAssertNotNil(genre);
    XCTAssertEqual([genre setFavorite:YES], VLC_SUCCESS);

    NSArray<VLCMediaLibraryGenre *> * const favorites =
        [self itemsAfterFilter:@"Rock"
                        loader:^NSArray *{
        return self.model.listOfFavoriteGenres;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(favorites.firstObject.libraryID, genre.libraryID);
    XCTAssertEqual(self.model.numberOfFavoriteGenres, (size_t)1);

    XCTAssertEqual([genre setFavorite:NO], VLC_SUCCESS);
}

- (void)testAllFavoritesParentQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    XCTAssertNotNil(item);
    XCTAssertEqual([item setFavorite:YES], VLC_SUCCESS);

    NSArray<id<VLCMediaLibraryItemProtocol>> * const favorites =
        [self itemsAfterFilter:@"Factory Track"
                        loader:^NSArray *{
        return [self.model listOfLibraryItemsOfParentType:
            VLCMediaLibraryParentGroupTypeAllFavorites];
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(((VLCMediaLibraryMediaItem *)favorites.firstObject).libraryID,
                   item.libraryID);

    XCTAssertEqual([item setFavorite:NO], VLC_SUCCESS);
}

@end
