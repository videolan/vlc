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
#import "tests/VLCLibraryDataTypesTestSupport.h"
#import "tests/VLCInputItemTestSupport.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"

@interface VLCLibraryDataTypesTest : XCTestCase
@end

@implementation VLCLibraryDataTypesTest

- (void)testFileMapsFields
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
    XCTAssertTrue(libraryFile.external);
    XCTAssertTrue(libraryFile.removable);
    XCTAssertTrue(libraryFile.present);
    XCTAssertEqual(libraryFile.lastModificationDate, (time_t)1234);
}

- (void)testFileReadableTypes
{
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
        struct vlc_ml_file_t file = { 0 };
        file.i_type = fileTypes[i];
        VLCMediaLibraryFile * const typedFile =
            [[VLCMediaLibraryFile alloc] initWithFile:&file];
        XCTAssertEqualObjects(typedFile.readableFileType, readableTypes[i]);
    }
}

- (void)testTrackMapsFields
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
}

- (void)testTrackReadableTypes
{
    const vlc_ml_track_type_t trackTypes[] = {
        VLC_ML_TRACK_TYPE_AUDIO,
        VLC_ML_TRACK_TYPE_VIDEO,
        VLC_ML_TRACK_TYPE_UNKNOWN,
    };
    NSArray<NSString *> * const readableTypes = @[
        @"Audio", @"Video", @"Unknown"
    ];

    for (NSUInteger i = 0; i < sizeof(trackTypes) / sizeof(trackTypes[0]); ++i) {
        struct vlc_ml_media_track_t track = { 0 };
        track.i_type = trackTypes[i];
        VLCMediaLibraryTrack * const typedTrack =
            [[VLCMediaLibraryTrack alloc] initWithTrack:&track];
        XCTAssertEqualObjects(typedTrack.readableTrackType, readableTypes[i]);
    }
}

- (void)testTrackReadableCodecNames
{
    const struct {
        const char *codec;
        NSString *readableName;
    } codecs[] = {
        { "avc1", @"H264 - MPEG-4 AVC (part 10)" },
        { "mp4a", @"MPEG AAC Audio" },
    };

    for (NSUInteger i = 0; i < sizeof(codecs) / sizeof(codecs[0]); ++i) {
        struct vlc_ml_media_track_t track = { 0 };
        track.psz_codec = (char *)codecs[i].codec;
        VLCMediaLibraryTrack * const typedTrack =
            [[VLCMediaLibraryTrack alloc] initWithTrack:&track];
        XCTAssertEqualObjects(typedTrack.readableCodecName, codecs[i].readableName);
    }
}

- (void)testAudioTrackHasNoResolutionLabel
{
    struct vlc_ml_media_track_t track = { 0 };
    track.i_type = VLC_ML_TRACK_TYPE_AUDIO;
    track.v.i_width = 1920;
    track.v.i_height = 1080;
    VLCMediaLibraryTrack * const audioTrack =
        [[VLCMediaLibraryTrack alloc] initWithTrack:&track];
    XCTAssertNil(audioTrack.resolutionLabel);
}

- (void)testTrackResolutionLabels
{
    struct vlc_ml_media_track_t track = { 0 };
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
    }
}

- (void)testVideoTrackMapsVideoFields
{
    struct vlc_ml_media_track_t track = { 0 };
    track.i_type = VLC_ML_TRACK_TYPE_VIDEO;
    track.v.i_width = 1920;
    track.v.i_height = 1080;
    track.v.i_sarNum = 16;
    track.v.i_sarDen = 9;
    track.v.i_fpsNum = 24;
    track.v.i_fpsDen = 1;

    VLCMediaLibraryTrack * const videoTrack =
        [[VLCMediaLibraryTrack alloc] initWithTrack:&track];
    XCTAssertEqual(videoTrack.videoWidth, (uint32_t)1920);
    XCTAssertEqual(videoTrack.videoHeight, (uint32_t)1080);
    XCTAssertEqual(videoTrack.sourceAspectRatio, (uint32_t)16);
    XCTAssertEqual(videoTrack.sourceAspectRatioDenominator, (uint32_t)9);
    XCTAssertEqual(videoTrack.frameRate, (uint32_t)24);
    XCTAssertEqual(videoTrack.frameRateDenominator, (uint32_t)1);
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
}

- (void)testArtistDurationStringUsesSingularForms
{
    struct vlc_ml_artist_t artistData = { 0 };
    artistData.i_nb_album = 1;
    artistData.i_nb_tracks = 1;
    VLCMediaLibraryArtist * const artist =
        [[VLCMediaLibraryArtist alloc] initWithArtist:&artistData];
    XCTAssertEqualObjects(artist.durationString, @"1 album, 1 song");
}

- (void)testArtistDisplayStringFallsBackForMissingName
{
    struct vlc_ml_artist_t artistData = { 0 };
    artistData.psz_name = (char *)"";
    VLCMediaLibraryArtist * const unknownArtist =
        [[VLCMediaLibraryArtist alloc] initWithArtist:&artistData];
    XCTAssertEqualObjects(unknownArtist.displayString, @"Unknown Artist");
}

- (void)testEmptyArtistHasSafeDefaults
{
    struct vlc_ml_artist_t emptyArtistData = { 0 };
    VLCMediaLibraryArtist * const emptyArtist =
        [[VLCMediaLibraryArtist alloc] initWithArtist:&emptyArtistData];
    XCTAssertEqualObjects(emptyArtist.displayString, @"Unknown Artist");
    XCTAssertEqualObjects(emptyArtist.durationString, @"0 albums, 0 songs");
    XCTAssertEqualObjects(emptyArtist.genreString, @"");
}

- (void)testEmptyAlbumHasSafeDefaults
{
    struct vlc_ml_album_t emptyAlbumData = { 0 };
    VLCMediaLibraryAlbum * const emptyAlbum =
        [[VLCMediaLibraryAlbum alloc] initWithAlbum:&emptyAlbumData];
    XCTAssertEqualObjects(emptyAlbum.displayString, @"Unknown Album");
    XCTAssertEqualObjects(emptyAlbum.durationString, @"--:--");
    XCTAssertEqualObjects(emptyAlbum.genreString, @"");
}

- (void)testEmptyGenreHasSafeDefaults
{
    struct vlc_ml_genre_t emptyGenreData = { 0 };
    VLCMediaLibraryGenre * const emptyGenre =
        [[VLCMediaLibraryGenre alloc] initWithGenre:&emptyGenreData];
    XCTAssertEqualObjects(emptyGenre.displayString, @"Unknown Genre");
    XCTAssertEqualObjects(emptyGenre.durationString, @"0 songs");
}

- (void)testGenreDurationStringUsesSingularForm
{
    struct vlc_ml_genre_t genreData = { 0 };
    genreData.i_nb_tracks = 1;
    VLCMediaLibraryGenre * const genre =
        [[VLCMediaLibraryGenre alloc] initWithGenre:&genreData];
    XCTAssertEqualObjects(genre.durationString, @"1 song");
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
    albumData.thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl =
        (char *)"file:///tmp/album.jpg";

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
    XCTAssertEqualObjects(album.albums, @[ album ]);
    XCTAssertTrue(album.favorited);
    XCTAssertTrue(album.smallArtworkGenerated);
    XCTAssertEqualObjects(album.smallArtworkMRL, @"file:///tmp/album.jpg");
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
    genreData.thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl =
        (char *)"file:///tmp/genre.jpg";

    VLCMediaLibraryGenre * const genre =
        [[VLCMediaLibraryGenre alloc] initWithGenre:&genreData];
    XCTAssertEqual(genre.libraryID, (int64_t)9);
    XCTAssertEqualObjects(genre.name, @"Genre");
    XCTAssertEqualObjects(genre.displayString, @"Genre");
    XCTAssertEqualObjects(genre.durationString, @"4 songs");
    XCTAssertTrue(genre.favorited);
    XCTAssertEqualObjects(genre.genres, @[ genre ]);
    XCTAssertEqual(genre.matchingParentType, VLCMediaLibraryParentGroupTypeGenre);
    XCTAssertTrue(genre.smallArtworkGenerated);
    XCTAssertEqualObjects(genre.smallArtworkMRL, @"file:///tmp/genre.jpg");
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

- (void)testShowPrimaryDetailStringUsesSingularForms
{
    struct vlc_ml_show_t showData = { 0 };
    showData.i_nb_seasons = 1;
    showData.i_nb_episodes = 1;
    VLCMediaLibraryShow * const show =
        [[VLCMediaLibraryShow alloc] initWithShow:&showData];
    XCTAssertEqualObjects(show.primaryDetailString, @"1 season, 1 episode");
}

- (void)testGroupProperties
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
}

- (void)testGroupPrimaryDetailStringUsesSingularForm
{
    struct vlc_ml_group_t groupData = { 0 };
    groupData.i_nb_total_media = 1;
    VLCMediaLibraryGroup * const group =
        [[VLCMediaLibraryGroup alloc] initWithGroup:&groupData];
    XCTAssertEqualObjects(group.primaryDetailString, @"1 item");
}

- (void)testPlaylistProperties
{
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
    XCTAssertTrue(playlist.smallArtworkGenerated);
    XCTAssertEqualObjects(playlist.smallArtworkMRL, @"file:///tmp/playlist.jpg");
    XCTAssertEqualObjects(playlist.primaryDetailString, @"2 items");
    XCTAssertEqualObjects(playlist.durationString, @"01:30");
    XCTAssertEqual(playlist.numberOfMedia, (unsigned int)2);
    XCTAssertEqual(playlist.numberOfAudios, (uint32_t)1);
    XCTAssertEqual(playlist.numberOfVideos, (uint32_t)1);
    XCTAssertEqual(playlist.duration, (int64_t)90000);
    XCTAssertEqual(playlist.numberDurationUnknown, (uint32_t)1);
    XCTAssertTrue(playlist.readOnly);
    XCTAssertTrue(playlist.favorited);
}

- (void)testPlaylistPrimaryDetailStringUsesSingularForm
{
    struct vlc_ml_playlist_t playlistData = { 0 };
    playlistData.i_nb_media = 1;
    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    XCTAssertEqualObjects(playlist.primaryDetailString, @"1 item");
}

- (void)testPlaylistMapsPresentCounts
{
    struct vlc_ml_playlist_t playlistData = { 0 };
    playlistData.i_nb_present_media = 3;
    playlistData.i_nb_present_audio = 1;
    playlistData.i_nb_present_video = 2;
    playlistData.i_nb_present_unknown = 0;

    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    XCTAssertEqual(playlist.numberOfPresentMedia, (unsigned int)3);
    XCTAssertEqual(playlist.numberOfPresentAudios, (uint32_t)1);
    XCTAssertEqual(playlist.numberOfPresentVideos, (uint32_t)2);
    XCTAssertEqual(playlist.numberOfPresentUnknowns, (uint32_t)0);
}

- (void)testPlaylistMapsArtworkAndCreationDate
{
    struct vlc_ml_playlist_t playlistData = { 0 };
    playlistData.i_creation_date = 300;
    playlistData.psz_artwork_mrl = (char *)"file:///tmp/playlist.jpg";

    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    XCTAssertEqualObjects(playlist.creationDate,
                          [NSDate dateWithTimeIntervalSince1970:300]);
    XCTAssertEqualObjects(playlist.smallArtworkMRL, @"file:///tmp/playlist.jpg");
}

- (void)testEntryPointProperties
{
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

- (void)testGroupPresentCounts
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
    XCTAssertEqual(group.numberOfPresentTotalItems, (NSUInteger)2);
    XCTAssertEqual(group.numberOfPresentVideoItems, (NSUInteger)1);
    XCTAssertEqual(group.numberOfPresentAudioItems, (NSUInteger)1);
    XCTAssertEqual(group.numberOfPresentUnknownItems, (NSUInteger)0);
    XCTAssertEqual(group.numberOfPresentSeenItems, (NSUInteger)1);
}

- (void)testGroupDisplayStringFallsBackForMissingName
{
    struct vlc_ml_group_t groupData = { 0 };
    VLCMediaLibraryGroup * const group =
        [[VLCMediaLibraryGroup alloc] initWithGroup:&groupData];
    XCTAssertEqualObjects(group.displayString, @"Unknown Group");
}

- (void)testEmptyGroupHasNoMediaItems
{
    struct vlc_ml_group_t groupData = { 0 };
    VLCMediaLibraryGroup * const group =
        [[VLCMediaLibraryGroup alloc] initWithGroup:&groupData];
    XCTAssertEqualObjects(group.mediaItems, @[]);
    XCTAssertNil(group.firstMediaItem);
}

- (void)testEmptyPlaylistDefaults
{
    struct vlc_ml_playlist_t playlistData = { 0 };
    VLCMediaLibraryPlaylist * const emptyPlaylist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    XCTAssertEqualObjects(emptyPlaylist.primaryDetailString, @"No item");
    XCTAssertEqualObjects(emptyPlaylist.durationString, @"--:--");
    XCTAssertFalse(emptyPlaylist.readOnly);
    XCTAssertFalse(emptyPlaylist.favorited);
}

- (void)testPlaylistIsFileBackedOnlyForExistingLocalFiles
{
    struct vlc_ml_playlist_t playlistData = { 0 };
    playlistData.psz_mrl = (char *)"https://example.com/playlist.m3u";
    VLCMediaLibraryPlaylist * const remotePlaylist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    XCTAssertFalse(remotePlaylist.isFileBacked);

    NSString * const path = [NSTemporaryDirectory() stringByAppendingPathComponent:
                             @"vlc-datatypes-file-backed-test.m3u"];
    XCTAssertTrue([[NSFileManager defaultManager] createFileAtPath:path
                                                              contents:[NSData data]
                                                            attributes:nil]);
    NSString * const fileMRL = [NSURL fileURLWithPath:path].absoluteString;
    playlistData.psz_mrl = (char *)fileMRL.UTF8String;
    VLCMediaLibraryPlaylist * const filePlaylist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    XCTAssertTrue(filePlaylist.isFileBacked);
    [[NSFileManager defaultManager] removeItemAtPath:path error:nil];

    VLCMediaLibraryPlaylist * const missingFilePlaylist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    XCTAssertFalse(missingFilePlaylist.isFileBacked);
}

- (void)testPlaylistRevealInFinder
{
    VLCInputItemTestResetAppKitState();
    struct vlc_ml_playlist_t playlistData = { 0 };
    NSString * const path = [NSTemporaryDirectory() stringByAppendingPathComponent:
                             @"vlc-datatypes-reveal-test.m3u"];
    XCTAssertTrue([[NSFileManager defaultManager] createFileAtPath:path
                                                              contents:[NSData data]
                                                            attributes:nil]);
    NSString * const fileMRL = [NSURL fileURLWithPath:path].absoluteString;
    playlistData.psz_mrl = (char *)fileMRL.UTF8String;
    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    [playlist revealInFinder];
    XCTAssertTrue(VLCInputItemTestDidReveal());
    [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
}

- (void)testPlaylistRevealInFinderWithoutMRLDoesNothing
{
    VLCInputItemTestResetAppKitState();
    struct vlc_ml_playlist_t playlistData = { 0 };
    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    [playlist revealInFinder];
    XCTAssertFalse(VLCInputItemTestDidReveal());
}

- (void)testReadOnlyPlaylistRejectsAppendingMedia
{
    struct vlc_ml_playlist_t playlistData = { 0 };
    playlistData.b_is_read_only = true;
    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];
    VLCMediaLibraryMediaItem * const item =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);

    XCTAssertFalse([playlist appendMediaItems:@[item]]);
}

- (void)testWritablePlaylistRejectsAppendingNoMedia
{
    struct vlc_ml_playlist_t playlistData = { 0 };
    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];

    XCTAssertFalse([playlist appendMediaItems:@[]]);
}

- (void)testReadOnlyPlaylistRejectsRename
{
    struct vlc_ml_playlist_t playlistData = { 0 };
    playlistData.b_is_read_only = true;
    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];

    XCTAssertFalse([playlist renameTo:@"Renamed playlist"]);
}

- (void)testPlaylistRejectsRenameToEmptyName
{
    struct vlc_ml_playlist_t playlistData = { 0 };
    VLCMediaLibraryPlaylist * const playlist =
        [[VLCMediaLibraryPlaylist alloc] initWithPlaylist:&playlistData];

    XCTAssertFalse([playlist renameTo:@""]);
}

- (void)testMediaItemMapsCommonFields
{
    VLCMediaLibraryMediaItem * const item =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    XCTAssertEqual(item.libraryID, (int64_t)13);
    XCTAssertEqual(item.mediaType, VLC_ML_MEDIA_TYPE_VIDEO);
    XCTAssertEqualObjects(item.readableMediaType, @"Video");
    XCTAssertEqualObjects(item.title, @"Media");
    XCTAssertEqualObjects(item.displayString, @"Media");
    XCTAssertEqual(item.year, 2024);
    XCTAssertEqual(item.duration, (int64_t)123000);
    XCTAssertEqualObjects(item.durationString, @"02:03");
    XCTAssertEqual(item.playCount, (uint32_t)2);
    XCTAssertEqual(item.lastPlayedDate, (time_t)4567);
    XCTAssertEqual(item.progress, .5);
    XCTAssertTrue(item.favorited);
    XCTAssertTrue(item.smallArtworkGenerated);
    XCTAssertEqualObjects(item.smallArtworkMRL, @"file:///tmp/media.jpg");
}

- (void)testMediaItemDisplayStringFallsBackForMissingTitle
{
    VLCMediaLibraryMediaItem * const item =
        VLCLibraryDataTypesTestMediaItemWithEmptyTitle(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    XCTAssertEqualObjects(item.displayString, @"Unknown item");
}

- (void)testMediaItemPrimaryDetailFallsBackToDuration
{
    VLCMediaLibraryMediaItem * const item =
        VLCLibraryDataTypesTestMediaItemWithTypeAndSubtype(VLC_ML_MEDIA_TYPE_VIDEO,
                                                           VLC_ML_MEDIA_SUBTYPE_UNKNOWN);

    XCTAssertEqualObjects(item.primaryDetailString, item.durationString);
}

- (void)testMoviePrimaryDetailUsesDirector
{
    VLCMediaLibraryMediaItem * const movie =
        VLCLibraryDataTypesTestMediaItemWithInputMetadata(VLC_ML_MEDIA_SUBTYPE_MOVIE,
            @{ VLCLibraryDataTypesTestInputItemDirectorKey: @"Director" });

    XCTAssertEqualObjects(movie.primaryDetailString, @"Director");
}

- (void)testMoviePrimaryDetailFallsBackForEmptyDirector
{
    VLCMediaLibraryMediaItem * const movie =
        VLCLibraryDataTypesTestMediaItemWithInputMetadata(VLC_ML_MEDIA_SUBTYPE_MOVIE,
            @{ VLCLibraryDataTypesTestInputItemDirectorKey: @"" });

    XCTAssertEqualObjects(movie.primaryDetailString, movie.durationString);
}

- (void)testShowEpisodePrimaryDetailUsesShowName
{
    VLCMediaLibraryMediaItem * const episode =
        VLCLibraryDataTypesTestMediaItemWithInputMetadata(VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE,
            @{ VLCLibraryDataTypesTestInputItemShowNameKey: @"Show" });

    XCTAssertEqualObjects(episode.primaryDetailString, @"Show");
}

- (void)testShowEpisodePrimaryDetailFallsBackToDurationWithoutShowName
{
    VLCMediaLibraryMediaItem * const episode =
        VLCLibraryDataTypesTestMediaItemWithInputMetadata(VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE,
            @{ VLCLibraryDataTypesTestInputItemShowNameKey: @"" });

    XCTAssertEqualObjects(episode.primaryDetailString, episode.durationString);
}

- (void)testAlbumTrackPrimaryDetailUsesArtistName
{
    VLCInputItemTestSetArtistName(@"Artist");
    VLCMediaLibraryMediaItem * const track =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK);

    XCTAssertEqualObjects(track.primaryDetailString, @"Artist");
    VLCInputItemTestSetArtistName(nil);
}

- (void)testAlbumTrackPrimaryDetailFallsBackForMissingArtistName
{
    VLCInputItemTestSetArtistName(nil);
    VLCMediaLibraryMediaItem * const track =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK);

    XCTAssertEqualObjects(track.primaryDetailString, track.durationString);
}

- (void)testMediaItemSecondaryDetailUsesInputItemDate
{
    VLCMediaLibraryMediaItem * const item =
        VLCLibraryDataTypesTestMediaItemWithInputMetadata(VLC_ML_MEDIA_SUBTYPE_UNKNOWN,
            @{ VLCLibraryDataTypesTestInputItemDateKey: @"2026-09-05" });

    XCTAssertEqualObjects(item.secondaryDetailString, @"2026-09-05");
}

- (void)testAlbumTrackSecondaryDetailFallsBackToInputItemDateWithoutGenre
{
    VLCMediaLibraryMediaItem * const track =
        VLCLibraryDataTypesTestMediaItemWithInputMetadata(VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK,
            @{ VLCLibraryDataTypesTestInputItemDateKey: @"2026-09-05" });

    XCTAssertEqualObjects(track.secondaryDetailString, @"2026-09-05");
}

- (void)testMediaItemMapsFilesAndTracks
{
    VLCMediaLibraryMediaItem * const item =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    XCTAssertEqual(item.files.count, (NSUInteger)1);
    XCTAssertEqual(item.tracks.count, (NSUInteger)2);
    XCTAssertEqual(item.firstVideoTrack, item.tracks.firstObject);
    XCTAssertEqual(item.firstVideoTrack.videoWidth, (uint32_t)1920);
    XCTAssertEqual(item.firstVideoTrack.videoHeight, (uint32_t)1080);
}

- (void)testMediaItemMapsAlbumTrackFields
{
    VLCMediaLibraryMediaItem * const albumTrack =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK);
    XCTAssertEqual(albumTrack.mediaSubType, VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK);
    XCTAssertEqualObjects(albumTrack.readableMediaSubType, @"Album Track");
    XCTAssertEqual(albumTrack.artistID, (int64_t)7);
    XCTAssertEqual(albumTrack.albumID, (int64_t)8);
    XCTAssertEqual(albumTrack.genreID, (int64_t)9);
    XCTAssertEqual(albumTrack.trackNumber, 3);
    XCTAssertEqual(albumTrack.discNumber, 1);
}

- (void)testMediaItemMapsMovieFields
{
    VLCMediaLibraryMediaItem * const movie =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    XCTAssertEqual(movie.mediaSubType, VLC_ML_MEDIA_SUBTYPE_MOVIE);
    XCTAssertEqualObjects(movie.readableMediaSubType, @"Movie");
    XCTAssertEqualObjects(movie.movie.summary, @"Movie summary");
    XCTAssertEqualObjects(movie.movie.imdbID, @"tt123");
}

- (void)testMediaItemMapsShowEpisodeFields
{
    VLCMediaLibraryMediaItem * const episode =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE);
    XCTAssertEqual(episode.mediaSubType, VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE);
    XCTAssertEqualObjects(episode.readableMediaSubType, @"Show Episode");
    XCTAssertEqual(episode.showEpisode.seasonNumber, (uint32_t)2);
    XCTAssertEqual(episode.showEpisode.episodeNumber, (uint32_t)4);
}

- (void)testShowEpisodeSecondaryDetailUsesEpisodeNumbers
{
    VLCMediaLibraryMediaItem * const episode =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE);

    XCTAssertEqualObjects(episode.secondaryDetailString, @"Season 2, Episode 4");
}

- (void)testMediaItemMapsUnknownSubtype
{
    VLCMediaLibraryMediaItem * const unknown =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_UNKNOWN);
    XCTAssertEqualObjects(unknown.readableMediaSubType, @"Unknown Media Type");
}

- (void)testMediaItemReadableMediaType
{
    VLCMediaLibraryMediaItem * const audio =
        VLCLibraryDataTypesTestMediaItemWithTypeAndSubtype(VLC_ML_MEDIA_TYPE_AUDIO,
                                                           VLC_ML_MEDIA_SUBTYPE_UNKNOWN);
    XCTAssertEqualObjects(audio.readableMediaType, @"Audio");

    VLCMediaLibraryMediaItem * const unknown =
        VLCLibraryDataTypesTestMediaItemWithTypeAndSubtype(VLC_ML_MEDIA_TYPE_UNKNOWN,
                                                           VLC_ML_MEDIA_SUBTYPE_UNKNOWN);
    XCTAssertEqualObjects(unknown.readableMediaType, @"Unknown Media Type");
}

- (void)testMediaItemEnumeratesItself
{
    VLCMediaLibraryMediaItem * const item =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    __block NSUInteger enumeratedCount = 0;
    [item enumerateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem *childItem, BOOL *stop) {
        XCTAssertEqual(childItem, item);
        XCTAssertFalse(*stop);
        enumeratedCount++;
    }];
    XCTAssertEqual(enumeratedCount, (NSUInteger)1);
}

- (void)testMediaItemIteratesItself
{
    VLCMediaLibraryMediaItem * const item =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    __block NSUInteger iteratedCount = 0;
    [item iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem *childItem) {
        XCTAssertEqual(childItem, item);
        iteratedCount++;
    }];
    XCTAssertEqual(iteratedCount, (NSUInteger)1);
}

- (void)testMediaItemCollections
{
    VLCMediaLibraryMediaItem * const item =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    XCTAssertEqual(item.firstMediaItem, item);
    XCTAssertEqualObjects(item.mediaItems, @[ item ]);
}

- (void)testMediaItemActionableDetailsDependOnSubtype
{
    VLCMediaLibraryMediaItem * const movie =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    XCTAssertFalse(movie.primaryActionableDetail);
    XCTAssertFalse(movie.secondaryActionableDetail);
    XCTAssertNil(movie.primaryActionableDetailLibraryItem);
    XCTAssertNil(movie.secondaryActionableDetailLibraryItem);

    VLCMediaLibraryMediaItem * const albumTrack =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK);
    XCTAssertTrue(albumTrack.primaryActionableDetail);
    XCTAssertTrue(albumTrack.secondaryActionableDetail);
}

- (void)testMediaItemSecureCoding
{
    VLCMediaLibraryMediaItem * const item =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    XCTAssertTrue([VLCMediaLibraryMediaItem supportsSecureCoding]);
    NSKeyedArchiver * const archiver =
        [[NSKeyedArchiver alloc] initRequiringSecureCoding:NO];
    [item encodeWithCoder:archiver];
    [archiver finishEncoding];
    NSData * const data = archiver.encodedData;

    NSError *error = nil;
    NSKeyedUnarchiver * const unarchiver =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
    XCTAssertNil(error);
    XCTAssertEqual([unarchiver decodeInt64ForKey:@"VLCMediaLibraryMediaItemLibraryID"],
                   (int64_t)13);
    [unarchiver finishDecoding];
    XCTAssertNil([[VLCMediaLibraryMediaItem alloc] initWithCoder:nil]);
}

- (void)testMediaItemsFromPasteboardDataRejectsMalformedData
{
    NSData * const malformedData = [@"not a keyed archive"
        dataUsingEncoding:NSUTF8StringEncoding];

    XCTAssertNil([VLCMediaLibraryMediaItem mediaItemsFromPasteboardData:malformedData]);
}

- (void)testMediaItemsFromPasteboardDataRejectsUnexpectedArchiveRoot
{
    NSError *archiveError = nil;
    NSData * const data = [NSKeyedArchiver archivedDataWithRootObject:@{
        @"unexpected": @"root"
    } requiringSecureCoding:NO error:&archiveError];

    XCTAssertNil(archiveError);
    XCTAssertNotNil(data);
    XCTAssertNil([VLCMediaLibraryMediaItem mediaItemsFromPasteboardData:data]);
}

- (void)testMovieEmptyDefaults
{
    VLCMediaLibraryMovie * const emptyMovie =
        [[VLCMediaLibraryMovie alloc] initWithMediaItem:NULL];
    XCTAssertNotNil(emptyMovie);
    XCTAssertNil(emptyMovie.summary);
    XCTAssertNil(emptyMovie.imdbID);
    XCTAssertNil(emptyMovie.displayString);
}

- (void)testMovieDisplayStringUsesTitle
{
    struct vlc_ml_media_t movieData = { 0 };
    movieData.psz_title = (char *)"Movie title";

    VLCMediaLibraryMovie * const movie =
        [[VLCMediaLibraryMovie alloc] initWithMediaItem:&movieData];

    XCTAssertEqualObjects(movie.displayString, @"Movie title");
}

- (void)testShowEmptyDefaults
{
    struct vlc_ml_show_t emptyShowData = { 0 };
    VLCMediaLibraryShow * const emptyShow =
        [[VLCMediaLibraryShow alloc] initWithShow:&emptyShowData];
    XCTAssertNotNil(emptyShow);
    XCTAssertEqualObjects(emptyShow.name, @"");
    XCTAssertEqualObjects(emptyShow.summary, @"");
    XCTAssertEqualObjects(emptyShow.tvdbId, @"");
    XCTAssertEqualObjects(emptyShow.primaryDetailString, @"0 seasons, 0 episodes");
    XCTAssertEqualObjects(emptyShow.secondaryDetailString, @"Released in 0");
}

- (void)testEntryPointEmptyDefaults
{
    VLCMediaLibraryEntryPoint * const emptyEntryPoint =
        [[VLCMediaLibraryEntryPoint alloc] initWithEntryPoint:NULL];
    XCTAssertNotNil(emptyEntryPoint);
    XCTAssertNil(emptyEntryPoint.MRL);
    XCTAssertNil(emptyEntryPoint.decodedMRL);
    XCTAssertFalse(emptyEntryPoint.isPresent);
    XCTAssertFalse(emptyEntryPoint.isBanned);
}

- (void)testEmptyDummyItemDefaults
{
    VLCMediaLibraryDummyItem * const emptyItem =
        [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:@"Empty"
                                           withPrimaryDetailString:nil
                                         withSecondaryDetailString:nil];
    XCTAssertEqualObjects(emptyItem.displayString, @"Empty");
    XCTAssertNil(emptyItem.primaryDetailString);
    XCTAssertNil(emptyItem.secondaryDetailString);
    XCTAssertNil(emptyItem.mediaItems);
    XCTAssertEqual(emptyItem.libraryID, (int64_t)-1);
    XCTAssertFalse(emptyItem.isFileBacked);
}

- (void)testEmptyDummyItemCannotSetFavorite
{
    VLCMediaLibraryDummyItem * const emptyItem =
        [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:@"Empty"
                                           withPrimaryDetailString:nil
                                         withSecondaryDetailString:nil];
    XCTAssertEqual([emptyItem setFavorite:YES], VLC_EGENERIC);
}

- (void)testDummyItemStoresItsChildren
{
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
}

- (void)testDummyItemEnumeratesItsChildren
{
    VLCMediaLibraryMediaItem * const first =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    VLCMediaLibraryMediaItem * const second =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE);
    VLCMediaLibraryDummyItem * const item =
        [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:@"Collection"
                                                 withMediaItems:@[ first, second ]];
    NSMutableArray<VLCMediaLibraryMediaItem *> * enumerated = NSMutableArray.array;
    [item enumerateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem *childItem, BOOL *stop) {
        [enumerated addObject:childItem];
        *stop = childItem == first;
    }];
    XCTAssertEqualObjects(enumerated, @[ first ]);
}

- (void)testDummyItemIteratesItsChildren
{
    VLCMediaLibraryMediaItem * const first =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_MOVIE);
    VLCMediaLibraryMediaItem * const second =
        VLCLibraryDataTypesTestMediaItemWithSubtype(VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE);
    VLCMediaLibraryDummyItem * const item =
        [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:@"Collection"
                                                 withMediaItems:@[ first, second ]];
    NSMutableArray<VLCMediaLibraryMediaItem *> * const enumerated = NSMutableArray.array;
    [item iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem *childItem) {
        [enumerated addObject:childItem];
    }];
    XCTAssertEqualObjects(enumerated, (@[ first, second ]));
}

@end

#pragma clang diagnostic pop
