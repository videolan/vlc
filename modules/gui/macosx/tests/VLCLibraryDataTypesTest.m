/****************************************************************************
 * VLCLibraryDataTypesTest.m: VLC library data type native tests
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

#import "library/VLCLibraryDataTypes.h"
#import "tests/VLCInputItemTestSupport.h"
#import "tests/VLCLibraryDataTypesTestSupport.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"

@interface VLCLibraryDataTypesTest : XCTestCase
@end

@implementation VLCLibraryDataTypesTest

- (void)testFileMapsFieldsAndReadableTypes
{
    struct vlc_ml_file_t file = { 0 };
    file.psz_mrl = (char *)"file:///tmp/VLC%20Test.mp4";
    file.i_last_modification_date = 1234;
    file.i_type = VLC_ML_FILE_TYPE_SUBTITLE;
    file.b_external = true;
    file.b_removable = true;
    file.b_present = true;

    VLCMediaLibraryFile * const libraryFile = [[VLCMediaLibraryFile alloc] initWithFile:&file];

    XCTAssertEqualObjects(libraryFile.MRL, @"file:///tmp/VLC%20Test.mp4");
    XCTAssertEqualObjects(libraryFile.fileURL,
                          [NSURL URLWithString:@"file:///tmp/VLC%20Test.mp4"]);
    XCTAssertEqual(libraryFile.fileType, VLC_ML_FILE_TYPE_SUBTITLE);
    XCTAssertEqualObjects(libraryFile.readableFileType, @"Subtitle");
    XCTAssertTrue(libraryFile.external);
    XCTAssertTrue(libraryFile.removable);
    XCTAssertTrue(libraryFile.present);
    XCTAssertEqual(libraryFile.lastModificationDate, (time_t)1234);
    XCTAssertTrue([libraryFile.description containsString:@"VLCMediaLibraryFile"]);

    const vlc_ml_file_type_t fileTypes[] = {
        VLC_ML_FILE_TYPE_MAIN,
        VLC_ML_FILE_TYPE_PART,
        VLC_ML_FILE_TYPE_PLAYLIST,
        VLC_ML_FILE_TYPE_SOUNDTRACK,
        VLC_ML_FILE_TYPE_SUBTITLE,
        VLC_ML_FILE_TYPE_UNKNOWN,
    };
    NSArray<NSString *> * const readableTypes = @[
        @"Main", @"Part", @"Playlist", @"Soundtrack", @"Subtitle", @"Unknown"
    ];

    for (NSUInteger i = 0; i < sizeof(fileTypes) / sizeof(fileTypes[0]); ++i) {
        file.i_type = fileTypes[i];
        VLCMediaLibraryFile * const typedFile =
            [[VLCMediaLibraryFile alloc] initWithFile:&file];
        XCTAssertEqualObjects(typedFile.readableFileType, readableTypes[i]);
    }

    VLCMediaLibraryFile * const emptyFile = [[VLCMediaLibraryFile alloc] initWithFile:NULL];
    XCTAssertNil(emptyFile.MRL);
    XCTAssertNil(emptyFile.fileURL);
    XCTAssertEqualObjects(emptyFile.readableFileType, @"Unknown");
}

- (void)testTrackAndShowEpisodeNilInitializersHaveSafeDefaults
{
    VLCMediaLibraryTrack * const emptyTrack =
        [[VLCMediaLibraryTrack alloc] initWithTrack:NULL];
    XCTAssertNotNil(emptyTrack);
    XCTAssertNil(emptyTrack.codec);
    XCTAssertNil(emptyTrack.language);
    XCTAssertNil(emptyTrack.trackDescription);
    XCTAssertEqual(emptyTrack.trackType, VLC_ML_TRACK_TYPE_UNKNOWN);
    XCTAssertEqual(emptyTrack.bitrate, (uint32_t)0);
    XCTAssertNil(emptyTrack.resolutionLabel);
    XCTAssertEqualObjects(emptyTrack.readableCodecName, @"");
    XCTAssertEqualObjects(emptyTrack.readableTrackType, @"Unknown");
    XCTAssertTrue([emptyTrack.description containsString:@"VLCMediaLibraryTrack"]);

    VLCMediaLibraryShowEpisode * const emptyEpisode =
        [[VLCMediaLibraryShowEpisode alloc] initWithShowEpisode:NULL];
    XCTAssertNotNil(emptyEpisode);
    XCTAssertNil(emptyEpisode.summary);
    XCTAssertNil(emptyEpisode.tvdbID);
    XCTAssertEqual(emptyEpisode.episodeNumber, (uint32_t)0);
    XCTAssertEqual(emptyEpisode.seasonNumber, (uint32_t)0);
}

- (void)testTrackMapsFieldsAndResolutionLabels
{
    struct vlc_ml_media_track_t track = { 0 };
    track.psz_codec = (char *)"mp4a";
    track.psz_language = (char *)"en";
    track.psz_description = (char *)"Main audio";
    track.i_type = VLC_ML_TRACK_TYPE_AUDIO;
    track.i_bitrate = 192000;
    track.a.i_nbChannels = 2;
    track.a.i_sampleRate = 48000;

    VLCMediaLibraryTrack * const audioTrack =
        [[VLCMediaLibraryTrack alloc] initWithTrack:&track];

    XCTAssertEqualObjects(audioTrack.codec, @"mp4a");
    XCTAssertEqualObjects(audioTrack.language, @"en");
    XCTAssertEqualObjects(audioTrack.trackDescription, @"Main audio");
    XCTAssertEqual(audioTrack.trackType, VLC_ML_TRACK_TYPE_AUDIO);
    XCTAssertEqual(audioTrack.bitrate, (uint32_t)192000);
    XCTAssertEqual(audioTrack.numberOfAudioChannels, (uint32_t)2);
    XCTAssertEqual(audioTrack.audioSampleRate, (uint32_t)48000);
    XCTAssertEqualObjects(audioTrack.readableTrackType, @"Audio");
    XCTAssertNil(audioTrack.resolutionLabel);

    const struct {
        uint32_t width;
        uint32_t height;
        NSString *label;
    } resolutions[] = {
        { 640, 360, nil },
        { 960, 540, @"SD" },
        { 1920, 1080, @"HD" },
        { 3840, 2160, @"4K" },
        { 7680, 4320, @"8K" },
    };

    for (NSUInteger i = 0; i < sizeof(resolutions) / sizeof(resolutions[0]); ++i) {
        track = (struct vlc_ml_media_track_t){ 0 };
        track.i_type = VLC_ML_TRACK_TYPE_VIDEO;
        track.v.i_width = resolutions[i].width;
        track.v.i_height = resolutions[i].height;
        VLCMediaLibraryTrack * const videoTrack =
            [[VLCMediaLibraryTrack alloc] initWithTrack:&track];
        XCTAssertEqualObjects(videoTrack.resolutionLabel, resolutions[i].label);
        XCTAssertEqualObjects(videoTrack.readableTrackType, @"Video");
    }

    track.i_type = VLC_ML_TRACK_TYPE_UNKNOWN;
    VLCMediaLibraryTrack * const unknownTrack = [[VLCMediaLibraryTrack alloc] initWithTrack:&track];
    XCTAssertEqualObjects(unknownTrack.readableTrackType, @"Unknown");
    XCTAssertNotNil(unknownTrack);
    XCTAssertNotNil([[VLCMediaLibraryTrack alloc] initWithTrack:NULL]);
}

- (void)testShowEpisodeMapsFields
{
    struct vlc_ml_show_episode_t episode = { 0 };
    episode.psz_summary = (char *)"Episode summary";
    episode.psz_tvdb_id = (char *)"tvdb-123";
    episode.i_episode_nb = 3;
    episode.i_season_number = 2;

    VLCMediaLibraryShowEpisode * const showEpisode =
        [[VLCMediaLibraryShowEpisode alloc] initWithShowEpisode:&episode];

    XCTAssertEqualObjects(showEpisode.summary, @"Episode summary");
    XCTAssertEqualObjects(showEpisode.tvdbID, @"tvdb-123");
    XCTAssertEqual(showEpisode.episodeNumber, (uint32_t)3);
    XCTAssertEqual(showEpisode.seasonNumber, (uint32_t)2);
    XCTAssertNotNil([[VLCMediaLibraryShowEpisode alloc] initWithShowEpisode:NULL]);
}

- (void)testArtistProperties
{
    struct vlc_ml_artist_t artistData = { 0 };
    artistData.i_id = 7;
    artistData.psz_name = (char *)"Artist";
    artistData.psz_shortbio = (char *)"Biography";
    artistData.psz_mb_id = (char *)"mb-artist";
    artistData.i_nb_album = 2;
    artistData.i_nb_tracks = 3;
    artistData.b_is_favorite = true;
    artistData.thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl =
        (char *)"file:///tmp/artist.jpg";

    VLCMediaLibraryArtist * const artist =
        [[VLCMediaLibraryArtist alloc] initWithArtist:&artistData];
    XCTAssertEqual(artist.libraryID, (int64_t)7);
    XCTAssertEqualObjects(artist.name, @"Artist");
    XCTAssertEqualObjects(artist.displayString, @"Artist");
    XCTAssertEqualObjects(artist.shortBiography, @"Biography");
    XCTAssertEqualObjects(artist.musicBrainzID, @"mb-artist");
    XCTAssertEqual(artist.numberOfAlbums, (unsigned int)2);
    XCTAssertEqualObjects(artist.durationString, @"2 albums, 3 songs");
    XCTAssertTrue(artist.favorited);
    XCTAssertTrue(artist.smallArtworkGenerated);
    XCTAssertEqualObjects(artist.smallArtworkMRL, @"file:///tmp/artist.jpg");
    XCTAssertEqual(artist.matchingParentType, VLCMediaLibraryParentGroupTypeArtist);
    XCTAssertEqualObjects(artist.artists, @[ artist ]);
    XCTAssertEqualObjects(artist.genreString, @"");

    artistData.psz_name = (char *)"";
    VLCMediaLibraryArtist * const unknownArtist =
        [[VLCMediaLibraryArtist alloc] initWithArtist:&artistData];
    XCTAssertEqualObjects(unknownArtist.displayString, @"Unknown Artist");
}

- (void)testArtistAlbumAndGenreFactoriesFailWithoutAMediaLibrary
{
    XCTAssertNil([VLCMediaLibraryArtist artistWithID:7]);
    XCTAssertNil([VLCMediaLibraryAlbum albumWithID:8]);
    XCTAssertNil([VLCMediaLibraryGenre genreWithID:9]);

    struct vlc_ml_artist_t emptyArtistData = { 0 };
    VLCMediaLibraryArtist * const emptyArtist =
        [[VLCMediaLibraryArtist alloc] initWithArtist:&emptyArtistData];
    XCTAssertEqualObjects(emptyArtist.displayString, @"Unknown Artist");
    XCTAssertEqualObjects(emptyArtist.durationString, @"0 albums, 0 songs");
    XCTAssertEqualObjects(emptyArtist.genreString, @"");
    XCTAssertEqualObjects(emptyArtist.artists, @[ emptyArtist ]);
    XCTAssertNil(emptyArtist.albums);
    XCTAssertNil(emptyArtist.mediaItems);
    XCTAssertNil(emptyArtist.secondaryActionableDetailLibraryItem);

    struct vlc_ml_album_t emptyAlbumData = { 0 };
    VLCMediaLibraryAlbum * const emptyAlbum =
        [[VLCMediaLibraryAlbum alloc] initWithAlbum:&emptyAlbumData];
    XCTAssertEqualObjects(emptyAlbum.displayString, @"Unknown Album");
    XCTAssertEqualObjects(emptyAlbum.durationString, @"--:--");
    XCTAssertEqualObjects(emptyAlbum.genreString, @"");
    XCTAssertEqualObjects(emptyAlbum.albums, @[ emptyAlbum ]);
    XCTAssertNil(emptyAlbum.mediaItems);
    XCTAssertNil(emptyAlbum.primaryActionableDetailLibraryItem);
    XCTAssertNil(emptyAlbum.secondaryActionableDetailLibraryItem);

    struct vlc_ml_genre_t emptyGenreData = { 0 };
    VLCMediaLibraryGenre * const emptyGenre =
        [[VLCMediaLibraryGenre alloc] initWithGenre:&emptyGenreData];
    XCTAssertEqualObjects(emptyGenre.displayString, @"Unknown Genre");
    XCTAssertEqualObjects(emptyGenre.durationString, @"0 songs");
    XCTAssertEqualObjects(emptyGenre.genres, @[ emptyGenre ]);
    XCTAssertNil(emptyGenre.albums);
    XCTAssertNil(emptyGenre.artists);
    XCTAssertNil(emptyGenre.mediaItems);
}

- (void)testAlbumProperties
{
    struct vlc_ml_album_t albumData = { 0 };
    albumData.i_id = 8;
    albumData.psz_title = (char *)"Album";
    albumData.psz_summary = (char *)"Album summary";
    albumData.psz_artist = (char *)"Artist";
    albumData.i_artist_id = 7;
    albumData.i_nb_tracks = 10;
    albumData.i_duration = 61000;
    albumData.i_year = 2026;
    albumData.b_is_favorite = true;

    VLCMediaLibraryAlbum * const album =
        [[VLCMediaLibraryAlbum alloc] initWithAlbum:&albumData];
    XCTAssertEqual(album.libraryID, (int64_t)8);
    XCTAssertEqualObjects(album.title, @"Album");
    XCTAssertEqualObjects(album.displayString, @"Album");
    XCTAssertEqualObjects(album.summary, @"Album summary");
    XCTAssertEqualObjects(album.artistName, @"Artist");
    XCTAssertEqual(album.artistID, (int64_t)7);
    XCTAssertEqual(album.numberOfTracks, (unsigned int)10);
    XCTAssertEqual(album.duration, (int64_t)61000);
    XCTAssertEqual(album.year, (unsigned int)2026);
    XCTAssertEqualObjects(album.durationString, @"01:01");
    XCTAssertTrue(album.favorited);
    XCTAssertEqual(album.matchingParentType, VLCMediaLibraryParentGroupTypeAlbum);
    XCTAssertEqualObjects(album.genreString, @"");
}

- (void)testGenreProperties
{
    struct vlc_ml_genre_t genreData = { 0 };
    genreData.i_id = 9;
    genreData.psz_name = (char *)"Genre";
    genreData.i_nb_tracks = 4;
    genreData.b_is_favorite = true;

    VLCMediaLibraryGenre * const genre =
        [[VLCMediaLibraryGenre alloc] initWithGenre:&genreData];
    XCTAssertEqual(genre.libraryID, (int64_t)9);
    XCTAssertEqualObjects(genre.name, @"Genre");
    XCTAssertEqualObjects(genre.displayString, @"Genre");
    XCTAssertEqualObjects(genre.durationString, @"4 songs");
    XCTAssertTrue(genre.favorited);
    XCTAssertEqualObjects(genre.genres, @[ genre ]);
    XCTAssertEqual(genre.matchingParentType, VLCMediaLibraryParentGroupTypeGenre);
}

- (void)testShowProperties
{
    struct vlc_ml_show_t showData = { 0 };
    showData.i_id = 10;
    showData.psz_name = (char *)"Show";
    showData.psz_summary = (char *)"Show summary";
    showData.psz_artwork_mrl = (char *)"file:///tmp/show.jpg";
    showData.psz_tvdb_id = (char *)"tvdb-show";
    showData.i_release_year = 2025;
    showData.i_nb_episodes = 12;
    showData.i_nb_seasons = 3;

    VLCMediaLibraryShow * const show = [[VLCMediaLibraryShow alloc] initWithShow:&showData];
    XCTAssertEqual(show.libraryID, (int64_t)10);
    XCTAssertEqualObjects(show.name, @"Show");
    XCTAssertEqualObjects(show.displayString, @"Show");
    XCTAssertEqualObjects(show.summary, @"Show summary");
    XCTAssertEqualObjects(show.tvdbId, @"tvdb-show");
    XCTAssertEqual(show.releaseYear, (unsigned int)2025);
    XCTAssertEqual(show.episodeCount, (uint32_t)12);
    XCTAssertEqual(show.seasonCount, (uint32_t)3);
    XCTAssertEqualObjects(show.primaryDetailString, @"3 seasons, 12 episodes");
    XCTAssertEqualObjects(show.secondaryDetailString, @"Released in 2025");
    XCTAssertTrue(show.smallArtworkGenerated);
    XCTAssertEqualObjects(show.smallArtworkMRL, @"file:///tmp/show.jpg");
}

- (void)testGroupPlaylistAndEntryPointProperties
{
    struct vlc_ml_group_t groupData = { 0 };
    groupData.i_id = 11;
    groupData.psz_name = (char *)"Group";
    groupData.i_nb_total_media = 4;
    groupData.i_nb_video = 2;
    groupData.i_nb_audio = 1;
    groupData.i_nb_unknown = 1;
    groupData.i_nb_present_media = 3;
    groupData.i_nb_present_video = 2;
    groupData.i_nb_present_audio = 1;
    groupData.i_nb_present_unknown = 0;
    groupData.i_nb_seen = 2;
    groupData.i_nb_present_seen = 1;
    groupData.i_duration = 120000;
    groupData.i_creation_date = 100;
    groupData.i_last_modification_date = 200;

    VLCMediaLibraryGroup * const group =
        [[VLCMediaLibraryGroup alloc] initWithGroup:&groupData];
    XCTAssertEqual(group.libraryID, (int64_t)11);
    XCTAssertEqualObjects(group.name, @"Group");
    XCTAssertEqual(group.numberOfTotalItems, (NSUInteger)4);
    XCTAssertEqual(group.numberOfVideoItems, (NSUInteger)2);
    XCTAssertEqual(group.numberOfAudioItems, (NSUInteger)1);
    XCTAssertEqual(group.numberOfUnknownItems, (NSUInteger)1);
    XCTAssertEqual(group.numberOfPresentTotalItems, (NSUInteger)3);
    XCTAssertEqual(group.numberOfSeenItems, (NSUInteger)2);
    XCTAssertEqual(group.duration, (int64_t)120000);
    XCTAssertEqualObjects(group.durationString, @"02:00");
    XCTAssertEqualObjects(group.creationDate, [NSDate dateWithTimeIntervalSince1970:100]);
    XCTAssertEqualObjects(group.lastModificationDate,
                          [NSDate dateWithTimeIntervalSince1970:200]);
    XCTAssertNil(group.mediaItems);

    struct vlc_ml_playlist_t playlistData = { 0 };
    playlistData.i_id = 12;
    playlistData.psz_name = (char *)"Playlist";
    playlistData.psz_mrl = (char *)"file:///tmp/playlist.m3u";
    playlistData.psz_artwork_mrl = (char *)"file:///tmp/playlist.jpg";
    playlistData.i_nb_media = 2;
    playlistData.i_nb_audio = 1;
    playlistData.i_nb_video = 1;
    playlistData.i_duration = 90000;
    playlistData.i_nb_duration_unknown = 1;
    playlistData.i_creation_date = 300;
    playlistData.b_is_read_only = true;
    playlistData.b_is_favorite = true;

    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    XCTAssertEqual(playlist.libraryID, (int64_t)12);
    XCTAssertEqualObjects(playlist.MRL, @"file:///tmp/playlist.m3u");
    XCTAssertEqualObjects(playlist.displayString, @"Playlist");
    XCTAssertEqualObjects(playlist.primaryDetailString, @"2 items");
    XCTAssertEqualObjects(playlist.durationString, @"01:30");
    XCTAssertEqual(playlist.numberOfMedia, (unsigned int)2);
    XCTAssertEqual(playlist.numberOfAudios, (uint32_t)1);
    XCTAssertEqual(playlist.numberOfVideos, (uint32_t)1);
    XCTAssertEqual(playlist.duration, (int64_t)90000);
    XCTAssertEqual(playlist.numberDurationUnknown, (uint32_t)1);
    XCTAssertTrue(playlist.readOnly);
    XCTAssertTrue(playlist.favorited);

    struct vlc_ml_folder_t folder = { 0 };
    char folderMRL[] = "file:///tmp/VLC%20Folder";
    folder.psz_mrl = folderMRL;
    folder.b_present = true;
    folder.b_banned = true;
    VLCMediaLibraryEntryPoint * const entryPoint =
        [[VLCMediaLibraryEntryPoint alloc] initWithEntryPoint:&folder];
    XCTAssertEqualObjects(entryPoint.MRL, @"file:///tmp/VLC%20Folder");
    XCTAssertEqualObjects(entryPoint.decodedMRL, @"file:///tmp/VLC Folder");
    XCTAssertTrue(entryPoint.isPresent);
    XCTAssertTrue(entryPoint.isBanned);
}

- (void)testGroupEmptyStateAndRemainingCounts
{
    struct vlc_ml_group_t groupData = { 0 };
    groupData.psz_name = (char *)"";
    groupData.i_nb_present_media = 2;
    groupData.i_nb_present_video = 1;
    groupData.i_nb_present_audio = 1;
    groupData.i_nb_present_unknown = 0;
    groupData.i_nb_present_seen = 1;

    VLCMediaLibraryGroup * const group =
        [[VLCMediaLibraryGroup alloc] initWithGroup:&groupData];
    XCTAssertEqualObjects(group.displayString, @"Unknown Group");
    XCTAssertEqual(group.numberOfPresentTotalItems, (NSUInteger)2);
    XCTAssertEqual(group.numberOfPresentVideoItems, (NSUInteger)1);
    XCTAssertEqual(group.numberOfPresentAudioItems, (NSUInteger)1);
    XCTAssertEqual(group.numberOfPresentUnknownItems, (NSUInteger)0);
    XCTAssertEqual(group.numberOfPresentSeenItems, (NSUInteger)1);
    XCTAssertEqualObjects(group.mediaItems, @[]);
    XCTAssertNil(group.firstMediaItem);
}

- (void)testMediaItemMapsFilesTracksAndSubtypes
{
    VLCMediaLibraryMediaItem * const albumTrack =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK);
    XCTAssertNotNil(albumTrack);
    XCTAssertEqual(albumTrack.libraryID, (int64_t)13);
    XCTAssertEqual(albumTrack.mediaType, VLC_ML_MEDIA_TYPE_VIDEO);
    XCTAssertEqualObjects(albumTrack.readableMediaType, @"Video");
    XCTAssertEqual(albumTrack.mediaSubType, VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK);
    XCTAssertEqualObjects(albumTrack.readableMediaSubType, @"Album Track");
    XCTAssertEqualObjects(albumTrack.title, @"Media");
    XCTAssertEqualObjects(albumTrack.displayString, @"Media");
    XCTAssertEqualObjects(albumTrack.durationString, @"02:03");
    XCTAssertEqual(albumTrack.year, 2024);
    XCTAssertEqual(albumTrack.playCount, (uint32_t)2);
    XCTAssertEqual(albumTrack.lastPlayedDate, (time_t)4567);
    XCTAssertEqual(albumTrack.progress, .5);
    XCTAssertTrue(albumTrack.favorited);
    XCTAssertTrue(albumTrack.smallArtworkGenerated);
    XCTAssertEqualObjects(albumTrack.smallArtworkMRL, @"file:///tmp/media.jpg");
    XCTAssertEqual(albumTrack.files.count, (NSUInteger)1);
    XCTAssertEqual(albumTrack.tracks.count, (NSUInteger)2);
    XCTAssertEqual(albumTrack.firstVideoTrack, albumTrack.tracks.firstObject);
    XCTAssertEqual(albumTrack.artistID, (int64_t)7);
    XCTAssertEqual(albumTrack.albumID, (int64_t)8);
    XCTAssertEqual(albumTrack.genreID, (int64_t)9);
    XCTAssertEqual(albumTrack.trackNumber, 3);
    XCTAssertEqual(albumTrack.discNumber, 1);

    VLCMediaLibraryMediaItem * const movie =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    XCTAssertEqualObjects(movie.readableMediaSubType, @"Movie");
    XCTAssertEqualObjects(movie.movie.summary, @"Movie summary");
    XCTAssertEqualObjects(movie.movie.imdbID, @"tt123");

    VLCMediaLibraryMediaItem * const episode =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE);
    XCTAssertEqualObjects(episode.readableMediaSubType, @"Show Episode");
    XCTAssertEqual(episode.showEpisode.seasonNumber, (uint32_t)2);
    XCTAssertEqual(episode.showEpisode.episodeNumber, (uint32_t)4);

    VLCMediaLibraryMediaItem * const unknown =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_UNKNOWN);
    XCTAssertEqualObjects(unknown.readableMediaType, @"Video");
    XCTAssertEqualObjects(unknown.readableMediaSubType, @"Unknown Media Type");
}

- (void)testMovieShowAndEntryPointEmptyDefaults
{
    VLCMediaLibraryMovie * const emptyMovie =
        [[VLCMediaLibraryMovie alloc] initWithMediaItem:NULL];
    XCTAssertNotNil(emptyMovie);
    XCTAssertNil(emptyMovie.summary);
    XCTAssertNil(emptyMovie.imdbID);
    XCTAssertNil(emptyMovie.displayString);
    XCTAssertEqualObjects(emptyMovie.mediaItems, @[]);
    XCTAssertNil(emptyMovie.firstMediaItem);

    struct vlc_ml_show_t emptyShowData = { 0 };
    VLCMediaLibraryShow * const emptyShow =
        [[VLCMediaLibraryShow alloc] initWithShow:&emptyShowData];
    XCTAssertNotNil(emptyShow);
    XCTAssertEqualObjects(emptyShow.name, @"");
    XCTAssertEqualObjects(emptyShow.summary, @"");
    XCTAssertEqualObjects(emptyShow.tvdbId, @"");
    XCTAssertNil(emptyShow.episodes);
    XCTAssertNil(emptyShow.mediaItems);
    XCTAssertEqualObjects(emptyShow.primaryDetailString, @"0 seasons, 0 episodes");
    XCTAssertEqualObjects(emptyShow.secondaryDetailString, @"Released in 0");
    XCTAssertNil([VLCMediaLibraryShow showWithLibraryId:10]);

    VLCMediaLibraryEntryPoint * const emptyEntryPoint =
        [[VLCMediaLibraryEntryPoint alloc] initWithEntryPoint:NULL];
    XCTAssertNotNil(emptyEntryPoint);
    XCTAssertNil(emptyEntryPoint.MRL);
    XCTAssertNil(emptyEntryPoint.decodedMRL);
    XCTAssertFalse(emptyEntryPoint.isPresent);
    XCTAssertFalse(emptyEntryPoint.isBanned);
}

- (void)testDummyItemEnumeratesAndDescribesItsChildren
{
    VLCMediaLibraryDummyItem * const emptyItem =
        [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:@"Empty"
                                           withPrimaryDetailString:nil
                                         withSecondaryDetailString:nil];
    XCTAssertEqualObjects(emptyItem.displayString, @"Empty");
    XCTAssertEqualObjects(emptyItem.primaryDetailString, nil);
    XCTAssertEqualObjects(emptyItem.secondaryDetailString, nil);
    XCTAssertEqualObjects(emptyItem.mediaItems, nil);
    XCTAssertEqual(emptyItem.libraryID, (int64_t)-1);
    XCTAssertFalse(emptyItem.isFileBacked);
    XCTAssertEqual([emptyItem setFavorite:YES], VLC_EGENERIC);

    VLCMediaLibraryMediaItem * const first =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    VLCMediaLibraryMediaItem * const second =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE);
    VLCMediaLibraryDummyItem * const item =
        [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:@"Collection"
                                                 withMediaItems:@[ first, second ]];

    XCTAssertEqualObjects(item.primaryDetailString, @"2 items");
    XCTAssertEqualObjects(item.secondaryDetailString, @"");
    XCTAssertEqualObjects(item.mediaItems, (@[ first, second ]));
    XCTAssertEqual(item.firstMediaItem, first);

    NSMutableArray<VLCMediaLibraryMediaItem *> * enumerated = NSMutableArray.array;
    [item enumerateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem *childItem, BOOL *stop) {
        [enumerated addObject:childItem];
        *stop = childItem == first;
    }];
    XCTAssertEqualObjects(enumerated, @[ first ]);

    enumerated = NSMutableArray.array;
    [item iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem *childItem) {
        [enumerated addObject:childItem];
    }];
    XCTAssertEqualObjects(enumerated, (@[ first, second ]));
}

@end

#pragma clang diagnostic pop
