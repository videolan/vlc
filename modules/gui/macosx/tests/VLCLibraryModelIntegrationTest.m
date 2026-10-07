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
#import "library/VLCMediaLibraryFolderObserver.h"
#import "extensions/NSArray+VLCAdditions.h"
#import "tests/VLCLibraryDataTypesIntegrationTestSupport.h"
#import "VLCLibraryModelTestSupport.h"

@interface VLCLibraryModelPersistedTestCase : XCTestCase
- (NSArray<NSNumber *> *)libraryIDsForItems:(NSArray<id<VLCMediaLibraryItemProtocol>> *)items;
- (id<VLCMediaLibraryItemProtocol>)itemWithID:(int64_t)libraryID
                                      inItems:(NSArray<id<VLCMediaLibraryItemProtocol>> *)items;
@end

@implementation VLCLibraryModelPersistedTestCase

- (void)setUp
{
    [super setUp];
    self.continueAfterFailure = NO;
    XCTestExpectation * const initialized = [self expectationWithDescription:@"Persisted library fixtures discovered"];
    __block BOOL prepared = NO;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^{
        prepared = VLCLibraryDataTypesIntegrationStart() &&
            VLCLibraryDataTypesIntegrationPrepareFactoryFixtures();
        [initialized fulfill];
    });
    [self waitForExpectations:@[ initialized ] timeout:20.];
    XCTAssertTrue(prepared);
}

- (void)tearDown
{
    VLCLibraryDataTypesIntegrationStop();
    [super tearDown];
}

- (NSArray<NSNumber *> *)libraryIDsForItems:(NSArray<id<VLCMediaLibraryItemProtocol>> *)items
{
    NSMutableArray<NSNumber *> * const IDs = NSMutableArray.array;
    for (id<VLCMediaLibraryItemProtocol> item in items)
        [IDs addObject:@(item.libraryID)];
    return IDs.copy;
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
    XCTAssertTrue(VLCLibraryDataTypesIntegrationPrepareMovieFixture());
}

+ (void)tearDown
{
    VLCLibraryDataTypesIntegrationStop();
    [super tearDown];
}

- (VLCLibraryModel *)model
{
    if (_model == nil) {
        _model = [[VLCLibraryModel alloc]
                  initWithLibrary:VLCLibraryDataTypesIntegrationMediaLibrary()];
        _model.changeDelegate.throttleInterval = 0.;
    }
    return _model;
}

- (void)tearDown
{
    [_model applicationWillTerminate:nil];
    _model = nil;
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

- (NSArray<VLCMediaLibraryShow *> *)showsAfterQuery
{
    return [self cachedItemsAfterFilter:@"Factory"
                       notificationName:VLCLibraryModelListOfShowsReset
                                 loader:^NSArray *{
        return self.model.listOfShows;
    }];
}

- (NSArray<VLCMediaLibraryMovie *> *)moviesAfterQuery
{
    return [self cachedItemsAfterFilter:@"Factory"
                       notificationName:VLCLibraryModelListOfMoviesReset
                                 loader:^NSArray *{
        return self.model.listOfMovies;
    }];
}

- (NSArray<VLCMediaLibraryEntryPoint *> *)monitoredFoldersAfterQuery
{
    return [self cachedItemsAfterFilter:@"Factory"
                       notificationName:VLCLibraryModelListOfMonitoredFoldersUpdated
                                 loader:^NSArray *{
        return self.model.listOfMonitoredFolders;
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

    VLCMediaLibraryMediaItem * const fixture =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    XCTAssertNotNil(fixture);
    if (fixture == nil) {
        return @[ ];
    }
    XCTAssertTrue([alpha appendMediaItems:@[ fixture ]]);
    XCTAssertTrue([zulu appendMediaItems:@[ fixture ]]);

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
    XCTAssertTrue([empty renameTo:[prefix stringByAppendingString:@" Short"]]);
    XCTAssertTrue([filled renameTo:[prefix stringByAppendingString:@" Long"]]);

    VLCMediaLibraryMediaItem * const fixture =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    XCTAssertNotNil(fixture);
    if (fixture == nil) {
        return @[ ];
    }
    XCTAssertTrue([filled appendMediaItems:@[ fixture ]]);
    XCTAssertTrue([empty appendMediaItems:@[ fixture ]]);
    XCTAssertTrue([filled appendMediaItems:@[ fixture ]]);

    self.model.filterString = prefix;
    return [self.model listOfPlaylistsOfType:VLC_ML_PLAYLIST_TYPE_ALL];
}

- (void)testAudioMediaQueryReturnsMatchingFixtureMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self audioMediaAfterFilter:@"Factory Track"];

    XCTAssertEqual(media.count, (NSUInteger)1);
    XCTAssertEqualObjects(media.firstObject.displayString, @"Factory Track");
    XCTAssertEqual(self.model.numberOfAudioMedia, media.count);
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
    XCTAssertEqual(self.model.numberOfVideoMedia, media.count);
}

- (void)testVideoMediaQueryRejectsNonMatchingFilter
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self videoMediaAfterFilter:@"No Matching Fixture Media"];

    XCTAssertEqual(media.count, (NSUInteger)0);
}

- (void)testShowQueryReturnsFactoryShowAndMatchingCount
{
    NSArray<VLCMediaLibraryShow *> * const shows = [self showsAfterQuery];

    XCTAssertEqual(shows.count, (NSUInteger)1);
    XCTAssertEqual(shows.firstObject.libraryID,
                   VLCLibraryDataTypesIntegrationShowID());
    XCTAssertEqualObjects(shows.firstObject.name, @"Factory Show");
    XCTAssertEqual(self.model.numberOfShows, shows.count);
}

- (void)testMovieQueryReturnsFactoryMovieAndMatchingCount
{
    NSArray<VLCMediaLibraryMovie *> * const movies = [self moviesAfterQuery];

    XCTAssertEqual(movies.count, (NSUInteger)1);
    XCTAssertEqual(movies.firstObject.libraryID,
                   VLCLibraryDataTypesIntegrationFactoryMovieMediaID());
    XCTAssertEqual(self.model.numberOfMovies, movies.count);
}

- (void)testMonitoredFolderQueryReturnsFixtureFolder
{
    NSArray<VLCMediaLibraryEntryPoint *> * const folders =
        [self monitoredFoldersAfterQuery];

    XCTAssertGreaterThan(folders.count, (NSUInteger)0);
    for (VLCMediaLibraryEntryPoint * const folder in folders) {
        XCTAssertGreaterThan(folder.MRL.length, (NSUInteger)0);
    }
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
    XCTAssertEqual(media[0].libraryID,
                   VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID());
    XCTAssertEqual(media[1].libraryID,
                   VLCLibraryDataTypesIntegrationSortingAudioZuluMediaID());
}

- (void)testAudioMediaQuerySortsMatchingItemsInDescendingOrder
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self audioMediaAfterFilter:@"sort"];

    XCTAssertEqual(media.count, (NSUInteger)2);
    XCTAssertEqual(media[0].libraryID,
                   VLCLibraryDataTypesIntegrationSortingAudioZuluMediaID());
    XCTAssertEqual(media[1].libraryID,
                   VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID());
}

- (void)testVideoMediaQuerySortsMatchingItemsInAscendingOrder
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:NO];

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self videoMediaAfterFilter:@"sort"];

    XCTAssertEqual(media.count, (NSUInteger)2);
    XCTAssertEqual(media[0].libraryID,
                   VLCLibraryDataTypesIntegrationSortingVideoAlphaMediaID());
    XCTAssertEqual(media[1].libraryID,
                   VLCLibraryDataTypesIntegrationSortingVideoZuluMediaID());
}

- (void)testVideoMediaQuerySortsMatchingItemsInDescendingOrder
{
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];

    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self videoMediaAfterFilter:@"sort"];

    XCTAssertEqual(media.count, (NSUInteger)2);
    XCTAssertEqual(media[0].libraryID,
                   VLCLibraryDataTypesIntegrationSortingVideoZuluMediaID());
    XCTAssertEqual(media[1].libraryID,
                   VLCLibraryDataTypesIntegrationSortingVideoAlphaMediaID());
}

- (void)testPlaylistQueryFallsBackToAlphaForUnsupportedDurationAscending
{
    [self.model sortByCriteria:VLC_ML_SORTING_DURATION andDescending:NO];

    NSArray<VLCMediaLibraryPlaylist *> * const playlists =
        [self playlistsForDurationSortingWithPrefix:@"Sorting Duration Ascending"];

    XCTAssertEqual(playlists.count, (NSUInteger)2);
    XCTAssertEqualObjects(playlists[0].displayString, @"Sorting Duration Ascending Long");
    XCTAssertEqualObjects(playlists[1].displayString, @"Sorting Duration Ascending Short");
}

- (void)testPlaylistQueryFallsBackToAlphaForUnsupportedDurationDescending
{
    [self.model sortByCriteria:VLC_ML_SORTING_DURATION andDescending:YES];

    NSArray<VLCMediaLibraryPlaylist *> * const playlists =
        [self playlistsForDurationSortingWithPrefix:@"Sorting Duration Descending"];

    XCTAssertEqual(playlists.count, (NSUInteger)2);
    XCTAssertEqualObjects(playlists[0].displayString, @"Sorting Duration Descending Short");
    XCTAssertEqualObjects(playlists[1].displayString, @"Sorting Duration Descending Long");
}

- (void)testFavoriteAudioQuerySortsMatchingItemsInAscendingOrder
{
    VLCMediaLibraryMediaItem * const alpha =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID()];
    VLCMediaLibraryMediaItem * const zulu =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationSortingAudioZuluMediaID()];
    XCTAssertEqual([alpha setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([zulu setFavorite:YES], VLC_SUCCESS);
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:NO];

    NSArray<VLCMediaLibraryMediaItem *> * const favorites =
        [self itemsAfterFilter:@"sort"
                        loader:^NSArray *{
        return self.model.listOfFavoriteAudioMedia;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)2);
    XCTAssertEqual(favorites[0].libraryID,
                   VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID());
    XCTAssertEqual(favorites[1].libraryID,
                   VLCLibraryDataTypesIntegrationSortingAudioZuluMediaID());
    XCTAssertEqual([alpha setFavorite:NO], VLC_SUCCESS);
    XCTAssertEqual([zulu setFavorite:NO], VLC_SUCCESS);
}

- (void)testFavoriteAudioQuerySortsMatchingItemsInDescendingOrder
{
    VLCMediaLibraryMediaItem * const alpha =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID()];
    VLCMediaLibraryMediaItem * const zulu =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationSortingAudioZuluMediaID()];
    XCTAssertEqual([alpha setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([zulu setFavorite:YES], VLC_SUCCESS);
    [self.model sortByCriteria:VLC_ML_SORTING_ALPHA andDescending:YES];

    NSArray<VLCMediaLibraryMediaItem *> * const favorites =
        [self itemsAfterFilter:@"sort"
                        loader:^NSArray *{
        return self.model.listOfFavoriteAudioMedia;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)2);
    XCTAssertEqual(favorites[0].libraryID,
                   VLCLibraryDataTypesIntegrationSortingAudioZuluMediaID());
    XCTAssertEqual(favorites[1].libraryID,
                   VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID());
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
    XCTAssertEqual(self.model.numberOfArtists, artists.count);
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
    XCTAssertEqual(self.model.numberOfAlbums, albums.count);
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
    XCTAssertEqual(self.model.numberOfGenres, genres.count);
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
    XCTAssertEqual(self.model.numberOfGroups, groups.count);
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

    VLCMediaLibraryPlaylist * const createdPlaylist =
        [VLCMediaLibraryPlaylist playlistForLibraryID:playlistID];
    VLCMediaLibraryMediaItem * const fixture =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    XCTAssertNotNil(fixture);
    XCTAssertTrue([createdPlaylist appendMediaItems:@[ fixture ]]);

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
    VLCMediaLibraryMediaItem * const excluded =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID()];
    XCTAssertNotNil(item);
    XCTAssertNotNil(excluded);
    XCTAssertEqual([item setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual(self.model.listOfFavoriteAudioMedia.count, (NSUInteger)2);
    NSArray<VLCMediaLibraryMediaItem *> * const favorites =
        [self itemsAfterFilter:@"Factory Track"
                        loader:^NSArray *{
        return self.model.listOfFavoriteAudioMedia;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(favorites.firstObject.libraryID, item.libraryID);
    XCTAssertEqual(self.model.numberOfFavoriteAudioMedia, (size_t)1);

    XCTAssertEqual([item setFavorite:NO], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:NO], VLC_SUCCESS);
}

- (void)testFavoriteVideoQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryVideoMediaID()];
    VLCMediaLibraryMediaItem * const excluded =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationSortingVideoAlphaMediaID()];
    XCTAssertNotNil(item);
    XCTAssertNotNil(excluded);
    XCTAssertEqual([item setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual(self.model.listOfFavoriteVideoMedia.count, (NSUInteger)2);
    NSArray<VLCMediaLibraryMediaItem *> * const favorites =
        [self itemsAfterFilter:@"Factory Show"
                        loader:^NSArray *{
        return self.model.listOfFavoriteVideoMedia;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(favorites.firstObject.libraryID, item.libraryID);
    XCTAssertEqual(self.model.numberOfFavoriteVideoMedia, (size_t)1);

    XCTAssertEqual([item setFavorite:NO], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:NO], VLC_SUCCESS);
}

- (void)testFavoriteAlbumQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const track =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    VLCMediaLibraryMediaItem * const excludedTrack =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID()];
    VLCMediaLibraryAlbum * const album = [VLCMediaLibraryAlbum albumWithID:track.albumID];
    VLCMediaLibraryAlbum * const excluded =
        [VLCMediaLibraryAlbum albumWithID:excludedTrack.albumID];
    XCTAssertNotNil(album);
    XCTAssertNotNil(excluded);
    XCTAssertNotEqual(album.libraryID, excluded.libraryID);
    XCTAssertEqual([album setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual(self.model.listOfFavoriteAlbums.count, (NSUInteger)2);
    NSArray<VLCMediaLibraryAlbum *> * const favorites =
        [self itemsAfterFilter:@"Factory Album"
                        loader:^NSArray *{
        return self.model.listOfFavoriteAlbums;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(favorites.firstObject.libraryID, album.libraryID);
    XCTAssertEqual(self.model.numberOfFavoriteAlbums, (size_t)1);

    XCTAssertEqual([album setFavorite:NO], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:NO], VLC_SUCCESS);
}

- (void)testFavoriteArtistQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const track =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    VLCMediaLibraryMediaItem * const excludedTrack =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID()];
    VLCMediaLibraryArtist * const artist = [VLCMediaLibraryArtist artistWithID:track.artistID];
    VLCMediaLibraryArtist * const excluded =
        [VLCMediaLibraryArtist artistWithID:excludedTrack.artistID];
    XCTAssertNotNil(artist);
    XCTAssertNotNil(excluded);
    XCTAssertNotEqual(artist.libraryID, excluded.libraryID);
    XCTAssertEqual([artist setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual(self.model.listOfFavoriteArtists.count, (NSUInteger)2);
    NSArray<VLCMediaLibraryArtist *> * const favorites =
        [self itemsAfterFilter:@"Factory Artist"
                        loader:^NSArray *{
        return self.model.listOfFavoriteArtists;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(favorites.firstObject.libraryID, artist.libraryID);
    XCTAssertEqual(self.model.numberOfFavoriteArtists, (size_t)1);

    XCTAssertEqual([artist setFavorite:NO], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:NO], VLC_SUCCESS);
}

- (void)testFavoriteGenreQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const track =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    VLCMediaLibraryMediaItem * const excludedTrack =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID()];
    VLCMediaLibraryGenre * const genre = [VLCMediaLibraryGenre genreWithID:track.genreID];
    VLCMediaLibraryGenre * const excluded =
        [VLCMediaLibraryGenre genreWithID:excludedTrack.genreID];
    XCTAssertNotNil(genre);
    XCTAssertNotNil(excluded);
    XCTAssertNotEqual(genre.libraryID, excluded.libraryID);
    XCTAssertEqual([genre setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual(self.model.listOfFavoriteGenres.count, (NSUInteger)2);
    NSArray<VLCMediaLibraryGenre *> * const favorites =
        [self itemsAfterFilter:@"Rock"
                        loader:^NSArray *{
        return self.model.listOfFavoriteGenres;
    }];

    XCTAssertEqual(favorites.count, (NSUInteger)1);
    XCTAssertEqual(favorites.firstObject.libraryID, genre.libraryID);
    XCTAssertEqual(self.model.numberOfFavoriteGenres, (size_t)1);

    XCTAssertEqual([genre setFavorite:NO], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:NO], VLC_SUCCESS);
}

- (void)testAllFavoritesParentQueryUsesFilter
{
    VLCMediaLibraryMediaItem * const item =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationFactoryAudioMediaID()];
    VLCMediaLibraryMediaItem * const excluded =
        [VLCMediaLibraryMediaItem mediaItemForLibraryID:
            VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID()];
    XCTAssertNotNil(item);
    XCTAssertNotNil(excluded);
    XCTAssertEqual([item setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([excluded setFavorite:YES], VLC_SUCCESS);
    XCTAssertEqual([self.model listOfLibraryItemsOfParentType:
        VLCMediaLibraryParentGroupTypeAllFavorites].count,
                   (NSUInteger)2);
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
    XCTAssertEqual([excluded setFavorite:NO], VLC_SUCCESS);
}

- (void)testAudioLibraryParentQueryReturnsAudioMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self audioMediaAfterFilter:@"Factory Track"];
    NSArray<id<VLCMediaLibraryItemProtocol>> * const parentItems =
        [self.model listOfLibraryItemsOfParentType:VLCMediaLibraryParentGroupTypeAudioLibrary];

    XCTAssertEqual(parentItems.count, media.count);
    XCTAssertEqual(((VLCMediaLibraryMediaItem *)parentItems.firstObject).libraryID,
                   VLCLibraryDataTypesIntegrationFactoryAudioMediaID());
}

- (void)testVideoLibraryParentQueryReturnsVideoMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self videoMediaAfterFilter:@"Factory Show"];
    NSArray<id<VLCMediaLibraryItemProtocol>> * const parentItems =
        [self.model listOfLibraryItemsOfParentType:VLCMediaLibraryParentGroupTypeVideoLibrary];

    XCTAssertEqual(parentItems.count, media.count);
    XCTAssertEqual(((VLCMediaLibraryMediaItem *)parentItems.firstObject).libraryID,
                   VLCLibraryDataTypesIntegrationFactoryVideoMediaID());
}

- (void)testArtistParentQueryReturnsArtists
{
    NSArray<VLCMediaLibraryArtist *> * const artists =
        [self artistsAfterFilter:@"Factory Artist"];
    NSArray<id<VLCMediaLibraryItemProtocol>> * const parentItems =
        [self.model listOfLibraryItemsOfParentType:VLCMediaLibraryParentGroupTypeArtist];

    XCTAssertEqual(parentItems.count, artists.count);
    XCTAssertEqual(((VLCMediaLibraryArtist *)parentItems.firstObject).libraryID,
                   artists.firstObject.libraryID);
}

- (void)testAlbumParentQueryReturnsAlbums
{
    NSArray<VLCMediaLibraryAlbum *> * const albums =
        [self albumsAfterFilter:@"Factory Album"];
    NSArray<id<VLCMediaLibraryItemProtocol>> * const parentItems =
        [self.model listOfLibraryItemsOfParentType:VLCMediaLibraryParentGroupTypeAlbum];

    XCTAssertEqual(parentItems.count, albums.count);
    XCTAssertEqual(((VLCMediaLibraryAlbum *)parentItems.firstObject).libraryID,
                   albums.firstObject.libraryID);
}

- (void)testGenreParentQueryReturnsGenres
{
    NSArray<VLCMediaLibraryGenre *> * const genres = [self genresAfterFilter:@"Rock"];
    NSArray<id<VLCMediaLibraryItemProtocol>> * const parentItems =
        [self.model listOfLibraryItemsOfParentType:VLCMediaLibraryParentGroupTypeGenre];

    XCTAssertEqual(parentItems.count, genres.count);
    XCTAssertEqual(((VLCMediaLibraryGenre *)parentItems.firstObject).libraryID,
                   genres.firstObject.libraryID);
}

- (void)testRecentVideosParentQueryReturnsRecentMedia
{
    [self markMediaIDAsRecentlyPlayed:
        VLCLibraryDataTypesIntegrationFactoryVideoMediaID()];
    NSArray<VLCMediaLibraryMediaItem *> * const recent =
        [self cachedItemsAfterFilter:@"Factory Show"
                    notificationName:VLCLibraryModelRecentsMediaListReset
                              loader:^NSArray *{
        return self.model.listOfRecentMedia;
    }];
    NSArray<id<VLCMediaLibraryItemProtocol>> * const parentItems =
        [self.model listOfLibraryItemsOfParentType:VLCMediaLibraryParentGroupTypeRecentVideos];

    XCTAssertEqual(parentItems.count, recent.count);
    XCTAssertEqual(((VLCMediaLibraryMediaItem *)parentItems.firstObject).libraryID,
                   VLCLibraryDataTypesIntegrationFactoryVideoMediaID());
    [self clearRecentMediaHistory];
}

- (void)testUnknownParentQueryReturnsNil
{
    XCTAssertNil([self.model listOfLibraryItemsOfParentType:
        VLCMediaLibraryParentGroupTypeUnknown]);
}

- (void)testAudioLibraryMediaItemsQueryReturnsFixtureMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const audio =
        [self audioMediaAfterFilter:@"Factory Track"];
    NSArray<VLCMediaLibraryMediaItem *> * const mediaItems =
        [self.model listOfMediaItemsForParentType:VLCMediaLibraryParentGroupTypeAudioLibrary];

    XCTAssertEqual(mediaItems.count, audio.count);
    XCTAssertEqual(mediaItems.firstObject.libraryID,
                   VLCLibraryDataTypesIntegrationFactoryAudioMediaID());
}

- (void)testArtistMediaItemsQueryReturnsFixtureMedia
{
    NSArray<VLCMediaLibraryArtist *> * const artists =
        [self artistsAfterFilter:@"Factory Artist"];
    NSArray<VLCMediaLibraryMediaItem *> * const mediaItems =
        [self.model listOfMediaItemsForParentType:VLCMediaLibraryParentGroupTypeArtist];

    XCTAssertEqual(artists.count, (NSUInteger)1);
    XCTAssertEqual(mediaItems.count, (NSUInteger)1);
    XCTAssertEqual(mediaItems.firstObject.libraryID,
                   VLCLibraryDataTypesIntegrationFactoryAudioMediaID());
}

@end

@interface VLCLibraryModelSelectionTest : VLCLibraryModelPersistedTestCase
@property (strong) VLCLibraryModelTestOverlapFixture *fixture;
@end

@implementation VLCLibraryModelSelectionTest

- (void)tearDown
{
    if (self.fixture != nil) {
        XCTestExpectation * const finished = [self expectationWithDescription:@"Selective refresh queries finished"];
        [self.fixture.model finishPendingQueriesWithCompletion:^{
            [finished fulfill];
        }];
        [self waitForExpectations:@[ finished ] timeout:10.];
    }
    self.fixture = nil;
    [super tearDown];
}

- (void)prepareFixtureOfKind:(VLCLibraryModelTestCacheKind)kind
                     sorting:(enum vlc_ml_sorting_criteria_t)sorting
                  descending:(BOOL)descending
{
    XCTestExpectation * const prepared = [self expectationWithDescription:@"Selective cache entities prepared"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
                   ^{
        self.fixture = [[VLCLibraryModelTestOverlapFixture alloc] initWithCacheKind:kind];
        [prepared fulfill];
    });
    [self waitForExpectations:@[ prepared ] timeout:15.];
    XCTAssertNotNil(self.fixture);
    XCTestExpectation * const dropped = [self expectationForNotification:VLCLibraryModelAllCachesDropped
                                                                  object:self.fixture.model
                                                                 handler:nil];
    [self.fixture.model sortByCriteria:sorting andDescending:descending];
    [self waitForExpectations:@[ dropped ] timeout:5.];
}

- (void)loadFixtureCache
{
    if (self.fixture.cacheGetter == @selector(cachedListOfMovies)) {
        // Do not let a delayed notification from the initial snapshot satisfy
        // the later movie-update expectation.
        [self.fixture seedCacheWithItems:self.fixture.libraryItems];
        XCTAssertNotNil(self.fixture.cachedItems);
        return;
    }
    XCTestExpectation * const loaded = [self expectationForNotification:self.fixture.changeNotifications.firstObject
                                                                 object:self.fixture.model
                                                                handler:nil];
    [self.fixture refreshCache];
    [self waitForExpectations:@[ loaded ] timeout:5.];
    XCTAssertNotNil(self.fixture.cachedItems);
}

- (uint32_t)presentTracksForItem:(id<VLCMediaLibraryItemProtocol>)item
{
    if ([item isKindOfClass:VLCMediaLibraryArtist.class])
        return ((VLCMediaLibraryArtist *)item).numberOfPresentTracks;
    XCTAssertTrue([item isKindOfClass:VLCMediaLibraryAlbum.class]);
    return ((VLCMediaLibraryAlbum *)item).numberOfPresentTracks;
}

- (void)setMediaIDs:(NSArray<NSNumber *> *)mediaIDs present:(BOOL)present
{
    for (NSNumber *ID in mediaIDs)
        XCTAssertTrue(VLCLibraryDataTypesIntegrationSetMediaPresent(ID.longLongValue, present));
}

- (void)assertAvailabilityRestoresItemForCacheKind:(VLCLibraryModelTestCacheKind)kind
{
    [self prepareFixtureOfKind:kind sorting:VLC_ML_SORTING_ALPHA descending:NO];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    NSArray<NSNumber *> * const mediaIDs = [self libraryIDsForItems:fixture.changedItem.mediaItems];
    XCTAssertGreaterThan(mediaIDs.count, (NSUInteger)0);
    [self setMediaIDs:mediaIDs present:NO];
    XCTAssertEqual([self presentTracksForItem:[fixture refetchChangedItem]], (uint32_t)0);
    XCTAssertNil([self itemWithID:fixture.changedItem.libraryID inItems:fixture.libraryItems]);
    [self loadFixtureCache];
    XCTAssertNil([self itemWithID:fixture.changedItem.libraryID inItems:fixture.cachedItems]);
    const NSUInteger queries = [fixture.model queryCountForGetter:fixture.cacheGetter];

    [self setMediaIDs:mediaIDs present:YES];
    XCTAssertGreaterThan([self presentTracksForItem:[fixture refetchChangedItem]], (uint32_t)0);
    XCTAssertNotNil([self itemWithID:fixture.changedItem.libraryID inItems:fixture.libraryItems]);
    XCTestExpectation * const restored = [self expectationForNotification:fixture.changeNotifications.firstObject
                                                                   object:fixture.model
                                                                  handler:nil];
    [fixture deliverUpdateEvent];
    [self waitForExpectations:@[ restored ] timeout:5.];
    id<VLCMediaLibraryItemProtocol> const cached = [self itemWithID:fixture.changedItem.libraryID
                                                            inItems:fixture.cachedItems];
    XCTAssertNotNil(cached);
    XCTAssertEqual([self presentTracksForItem:cached], (uint32_t)mediaIDs.count);
    XCTAssertNotNil([self itemWithID:fixture.pendingItem.libraryID inItems:fixture.cachedItems]);
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.cachedItems],
                          [self libraryIDsForItems:fixture.libraryItems]);
    XCTAssertEqual([fixture.model queryCountForGetter:fixture.cacheGetter], queries + 1);
}

- (void)assertPartialUnavailabilityPreservesTrackCountOrderForCacheKind:(VLCLibraryModelTestCacheKind)kind
{
    [self prepareFixtureOfKind:kind sorting:VLC_ML_SORTING_TRACKNUMBER descending:NO];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    [self loadFixtureCache];
    NSArray<NSNumber *> * const beforeIDs = [self libraryIDsForItems:fixture.cachedItems];
    id<VLCMediaLibraryAudioGroupProtocol> const previous = (id)[self itemWithID:fixture.changedItem.libraryID
                                                                        inItems:fixture.cachedItems];
    XCTAssertNotNil(previous);
    XCTAssertEqual(previous.numberOfTracks, (unsigned int)2);
    XCTAssertEqual([self presentTracksForItem:previous], (uint32_t)2);
    const int64_t mediaID = previous.mediaItems.firstObject.libraryID;
    const NSUInteger queries = [fixture.model queryCountForGetter:fixture.cacheGetter];
    XCTAssertTrue(VLCLibraryDataTypesIntegrationSetMediaPresent(mediaID, NO));
    id<VLCMediaLibraryAudioGroupProtocol> const persisted = (id)[fixture refetchChangedItem];
    XCTAssertEqual(persisted.numberOfTracks, (unsigned int)2);
    XCTAssertEqual([self presentTracksForItem:persisted], (uint32_t)1);
    XCTestExpectation * const updated = [self expectationForNotification:fixture.changeNotifications[1]
                                                                  object:nil
                                                                 handler:^BOOL(NSNotification *notification) {
        return [notification.object conformsToProtocol:@protocol(VLCMediaLibraryAudioGroupProtocol)] &&
            [notification.object libraryID] == fixture.changedItem.libraryID;
    }];
    [fixture deliverUpdateEvent];
    [self waitForExpectations:@[ updated ] timeout:5.];
    id<VLCMediaLibraryAudioGroupProtocol> const cached = (id)[self itemWithID:fixture.changedItem.libraryID
                                                                      inItems:fixture.cachedItems];
    XCTAssertNotEqual(cached, previous);
    XCTAssertEqual(cached.numberOfTracks, (unsigned int)2);
    XCTAssertEqual([self presentTracksForItem:cached], (uint32_t)1);
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.cachedItems], beforeIDs);
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.libraryItems], beforeIDs);
    XCTAssertEqual([fixture.model queryCountForGetter:fixture.cacheGetter], queries);
}

- (void)assertTrackTransferReordersCacheKind:(VLCLibraryModelTestCacheKind)kind
                                  descending:(BOOL)descending
{
    [self prepareFixtureOfKind:kind sorting:VLC_ML_SORTING_TRACKNUMBER descending:descending];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    id<VLCMediaLibraryAudioGroupProtocol> const source = (id)fixture.changedItem;
    id<VLCMediaLibraryAudioGroupProtocol> const destination = (id)fixture.pendingItem;
    XCTAssertEqual(source.numberOfTracks, (unsigned int)2);
    XCTAssertEqual(destination.numberOfTracks, (unsigned int)1);
    NSNumber * const sourceID = @(source.libraryID);
    NSNumber * const destinationID = @(destination.libraryID);
    // Artist track-count order increases by default; album order decreases.
    const BOOL sourceFirst = kind == VLCLibraryModelTestArtistCache
        ? descending
        : !descending;
    NSArray<NSNumber *> * const beforeIDs = sourceFirst
        ? @[ sourceID, destinationID ]
        : @[ destinationID, sourceID ];
    NSArray<NSNumber *> * const afterIDs = sourceFirst
        ? @[ destinationID, sourceID ]
        : @[ sourceID, destinationID ];
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.libraryItems], beforeIDs);
    [self loadFixtureCache];
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.cachedItems], beforeIDs);
    const NSUInteger queries = [fixture.model queryCountForGetter:fixture.cacheGetter];
    const int64_t mediaID = source.mediaItems.firstObject.libraryID;
    XCTAssertGreaterThan(mediaID, (int64_t)0);
    XCTAssertTrue(kind == VLCLibraryModelTestArtistCache
        ? VLCLibraryDataTypesIntegrationMoveMediaToArtist(mediaID, destination.libraryID)
        : VLCLibraryDataTypesIntegrationMoveMediaToAlbum(mediaID, destination.libraryID));
    // The independent persisted query must really reverse order before the
    // model is told about the changed source entity.
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.libraryItems], afterIDs);
    XCTestExpectation * const reordered = [self expectationForNotification:fixture.changeNotifications.firstObject
                                                                    object:fixture.model
                                                                   handler:nil];
    [fixture deliverUpdateEvent];
    [self waitForExpectations:@[ reordered ] timeout:5.];
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.cachedItems], afterIDs);
    id<VLCMediaLibraryAudioGroupProtocol> const cachedSource = (id)[self itemWithID:source.libraryID inItems:fixture.cachedItems];
    id<VLCMediaLibraryAudioGroupProtocol> const cachedDestination = (id)[self itemWithID:destination.libraryID inItems:fixture.cachedItems];
    XCTAssertEqual(cachedSource.numberOfTracks, (unsigned int)1);
    XCTAssertEqual(cachedDestination.numberOfTracks, (unsigned int)2);
    XCTAssertEqual([fixture.model queryCountForGetter:fixture.cacheGetter], queries + 1);
}

- (void)testArtistBecomingAvailableRejoinsArtistCache
{
    [self assertAvailabilityRestoresItemForCacheKind:VLCLibraryModelTestArtistCache];
}

- (void)testAlbumBecomingAvailableRejoinsAlbumCache
{
    [self assertAvailabilityRestoresItemForCacheKind:VLCLibraryModelTestAlbumCache];
}

- (void)testGenreAvailabilityRestoresChildrenWithoutReloadingGenreList
{
    [self prepareFixtureOfKind:VLCLibraryModelTestGenreCache sorting:VLC_ML_SORTING_ALPHA descending:NO];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    NSArray<NSNumber *> * const mediaIDs = [self libraryIDsForItems:fixture.changedItem.mediaItems];
    XCTAssertEqual(mediaIDs.count, (NSUInteger)2);
    [self setMediaIDs:mediaIDs present:NO];
    // Native genre lists intentionally retain genres with unavailable children.
    XCTAssertNotNil([self itemWithID:fixture.changedItem.libraryID inItems:fixture.libraryItems]);
    [self loadFixtureCache];
    VLCMediaLibraryGenre * const previous = (id)[self itemWithID:fixture.changedItem.libraryID inItems:fixture.cachedItems];
    XCTAssertNotNil(previous);
    XCTAssertEqual(previous.mediaItems.count, (NSUInteger)0);
    NSArray<NSNumber *> * const beforeIDs = [self libraryIDsForItems:fixture.cachedItems];
    const NSUInteger queries = [fixture.model queryCountForGetter:fixture.cacheGetter];
    [self setMediaIDs:mediaIDs present:YES];
    XCTestExpectation * const updated = [self expectationForNotification:VLCLibraryModelGenreUpdated
                                                                  object:nil
                                                                 handler:^BOOL(NSNotification *notification) {
        return [notification.object isKindOfClass:VLCMediaLibraryGenre.class] &&
            [notification.object libraryID] == fixture.changedItem.libraryID;
    }];
    [fixture deliverUpdateEvent];
    [self waitForExpectations:@[ updated ] timeout:5.];
    VLCMediaLibraryGenre * const cached = (id)[self itemWithID:fixture.changedItem.libraryID inItems:fixture.cachedItems];
    XCTAssertNotEqual(cached, previous);
    XCTAssertEqualObjects([NSSet setWithArray:[self libraryIDsForItems:cached.mediaItems]],
                          [NSSet setWithArray:mediaIDs]);
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.cachedItems], beforeIDs);
    XCTAssertEqual([fixture.model queryCountForGetter:fixture.cacheGetter], queries);
}

- (void)testPartialArtistUnavailabilityPreservesTotalTrackCountSorting
{
    [self assertPartialUnavailabilityPreservesTrackCountOrderForCacheKind:VLCLibraryModelTestArtistCache];
}

- (void)testPartialAlbumUnavailabilityPreservesTotalTrackCountSorting
{
    [self assertPartialUnavailabilityPreservesTrackCountOrderForCacheKind:VLCLibraryModelTestAlbumCache];
}

- (void)testArtistTrackTransferReordersAscendingCountCache
{
    [self assertTrackTransferReordersCacheKind:VLCLibraryModelTestArtistCache descending:NO];
}

- (void)testArtistTrackTransferReordersDescendingCountCache
{
    [self assertTrackTransferReordersCacheKind:VLCLibraryModelTestArtistCache descending:YES];
}

- (void)testAlbumTrackTransferReordersDefaultCountCache
{
    [self assertTrackTransferReordersCacheKind:VLCLibraryModelTestAlbumCache descending:NO];
}

- (void)testAlbumTrackTransferReordersReversedCountCache
{
    [self assertTrackTransferReordersCacheKind:VLCLibraryModelTestAlbumCache descending:YES];
}

- (void)testMovieMetadataUpdateReplacesCachedMovieSnapshot
{
    [self prepareFixtureOfKind:VLCLibraryModelTestMovieCache sorting:VLC_ML_SORTING_ALPHA descending:NO];
    VLCLibraryModelTestOverlapFixture * const fixture = self.fixture;
    [self loadFixtureCache];
    VLCMediaLibraryMovie * const previous = (id)[self itemWithID:fixture.changedItem.libraryID inItems:fixture.cachedItems];
    XCTAssertNotNil(previous);
    NSString * const summary = @"Updated persisted movie summary";
    XCTAssertNotEqualObjects(previous.summary, summary);
    NSArray<NSNumber *> * const beforeIDs = [self libraryIDsForItems:fixture.cachedItems];
    const NSUInteger queries = [fixture.model queryCountForGetter:fixture.cacheGetter];
    XCTAssertTrue(VLCLibraryDataTypesIntegrationSetMovieSummary(previous.libraryID, summary));
    XCTAssertEqualObjects([VLCMediaLibraryMediaItem mediaItemForLibraryID:previous.libraryID].movie.summary, summary);
    XCTestExpectation * const reloaded = [self expectationForNotification:VLCLibraryModelListOfMoviesReset
                                                                   object:fixture.model
                                                                  handler:nil];
    [fixture deliverUpdateEvent];
    [self waitForExpectations:@[ reloaded ] timeout:5.];
    VLCMediaLibraryMovie * const cached = (id)[self itemWithID:previous.libraryID inItems:fixture.cachedItems];
    XCTAssertNotEqual(cached, previous);
    XCTAssertEqualObjects(cached.summary, summary);
    XCTAssertEqualObjects([self libraryIDsForItems:fixture.cachedItems], beforeIDs);
    XCTAssertEqual([fixture.model queryCountForGetter:fixture.cacheGetter], queries + 1);
}

@end
