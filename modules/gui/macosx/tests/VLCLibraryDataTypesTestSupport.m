/****************************************************************************
 * VLCLibraryDataTypesTestSupport.m: VLC library data type test fixtures
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 ****************************************************************************/

#import "VLCLibraryDataTypesTestSupport.h"

#import "library/VLCInputItem.h"

NSString * const VLCLibraryDataTypesTestInputItemNameKey = @"name";
NSString * const VLCLibraryDataTypesTestInputItemTitleKey = @"title";
NSString * const VLCLibraryDataTypesTestInputItemArtistKey = @"artist";
NSString * const VLCLibraryDataTypesTestInputItemAlbumKey = @"album";
NSString * const VLCLibraryDataTypesTestInputItemTrackNumberKey = @"trackNumber";
NSString * const VLCLibraryDataTypesTestInputItemGenreKey = @"genre";
NSString * const VLCLibraryDataTypesTestInputItemCopyrightKey = @"copyright";
NSString * const VLCLibraryDataTypesTestInputItemPublisherKey = @"publisher";
NSString * const VLCLibraryDataTypesTestInputItemLanguageKey = @"language";
NSString * const VLCLibraryDataTypesTestInputItemDateKey = @"date";
NSString * const VLCLibraryDataTypesTestInputItemDescriptionKey = @"contentDescription";
NSString * const VLCLibraryDataTypesTestInputItemDirectorKey = @"director";
NSString * const VLCLibraryDataTypesTestInputItemShowNameKey = @"showName";
NSString * const VLCLibraryDataTypesTestInputItemActorsKey = @"actors";
NSString * const VLCLibraryDataTypesTestInputItemArtworkURLKey = @"artworkURL";

@interface VLCMediaLibraryMediaItem (VLCLibraryDataTypesTestPrivate)
- (instancetype)initWithMediaItem:(struct vlc_ml_media_t *)mediaItem
                          library:(vlc_medialibrary_t *)mediaLibrary;
@end

static VLCMediaLibraryMediaItem *VLCLibraryDataTypesTestMediaItemWithTypeAndSubtypeAndTitle(
    vlc_ml_media_type_t type,
    vlc_ml_media_subtype_t subtype,
    const char *title,
    VLCInputItem * _Nullable inputItem);

@interface VLCLibraryDataTypesTestMediaItem : VLCMediaLibraryMediaItem
@property (nonatomic, strong) VLCInputItem *testInputItem;
@end

@implementation VLCLibraryDataTypesTestMediaItem

- (VLCInputItem *)inputItem
{
    return self.testInputItem;
}

@end

VLCMediaLibraryMediaItem *VLCLibraryDataTypesTestMediaItemWithSubtype(vlc_ml_media_subtype_t subtype)
{
    return VLCLibraryDataTypesTestMediaItemWithTypeAndSubtype(VLC_ML_MEDIA_TYPE_VIDEO, subtype);
}

VLCMediaLibraryMediaItem *VLCLibraryDataTypesTestMediaItemWithEmptyTitle(vlc_ml_media_subtype_t subtype)
{
    return VLCLibraryDataTypesTestMediaItemWithTypeAndSubtypeAndTitle(VLC_ML_MEDIA_TYPE_VIDEO,
                                                                       subtype,
                                                                       "",
                                                                       nil);
}

VLCMediaLibraryMediaItem *VLCLibraryDataTypesTestMediaItemWithTypeAndSubtype(vlc_ml_media_type_t type,
                                                                             vlc_ml_media_subtype_t subtype)
{
    return VLCLibraryDataTypesTestMediaItemWithTypeAndSubtypeAndTitle(type,
                                                                       subtype,
                                                                       "Media",
                                                                       nil);
}

static VLCMediaLibraryMediaItem *VLCLibraryDataTypesTestMediaItemWithTypeAndSubtypeAndTitle(
    vlc_ml_media_type_t type,
    vlc_ml_media_subtype_t subtype,
    const char *title,
    VLCInputItem * _Nullable inputItem)
{
    struct TestFileList {
        size_t i_nb_items;
        vlc_ml_file_t p_items[1];
    } files = { 0 };
    files.i_nb_items = 1;
    files.p_items[0].psz_mrl = (char *)"file:///tmp/media.mp4";
    files.p_items[0].i_type = VLC_ML_FILE_TYPE_MAIN;

    struct TestTrackList {
        size_t i_nb_items;
        vlc_ml_media_track_t p_items[2];
    } tracks = { 0 };
    tracks.i_nb_items = 2;
    tracks.p_items[0].psz_codec = (char *)"avc1";
    tracks.p_items[0].i_type = VLC_ML_TRACK_TYPE_VIDEO;
    tracks.p_items[0].v.i_width = 1920;
    tracks.p_items[0].v.i_height = 1080;
    tracks.p_items[0].v.i_sarNum = 16;
    tracks.p_items[0].v.i_sarDen = 9;
    tracks.p_items[0].v.i_fpsNum = 24;
    tracks.p_items[0].v.i_fpsDen = 1;
    tracks.p_items[1].psz_codec = (char *)"mp4a";
    tracks.p_items[1].i_type = VLC_ML_TRACK_TYPE_AUDIO;

    struct vlc_ml_media_t media = { 0 };
    media.i_id = 13;
    media.i_type = type;
    media.i_subtype = subtype;
    media.p_files = (vlc_ml_file_list_t *)&files;
    media.p_tracks = (vlc_ml_media_track_list_t *)&tracks;
    media.i_year = 2024;
    media.i_duration = 123000;
    media.i_playcount = 2;
    media.f_progress = .5;
    media.i_last_played_date = 4567;
    media.psz_title = (char *)title;
    media.thumbnails[VLC_ML_THUMBNAIL_SMALL].psz_mrl = (char *)"file:///tmp/media.jpg";
    media.b_is_favorite = true;

    if (subtype == VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE) {
        media.show_episode.i_episode_nb = 4;
        media.show_episode.i_season_number = 2;
    } else if (subtype == VLC_ML_MEDIA_SUBTYPE_MOVIE) {
        media.movie.psz_summary = (char *)"Movie summary";
        media.movie.psz_imdb_id = (char *)"tt123";
    } else if (subtype == VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK) {
        media.album_track.i_artist_id = 7;
        media.album_track.i_album_id = 8;
        media.album_track.i_genre_id = 9;
        media.album_track.i_track_nb = 3;
        media.album_track.i_disc_nb = 1;
    }

    if (inputItem == nil) {
        return [[VLCMediaLibraryMediaItem alloc]
            initWithMediaItem:&media library:(vlc_medialibrary_t *)0x1];
    }

    VLCLibraryDataTypesTestMediaItem * const item =
        [[VLCLibraryDataTypesTestMediaItem alloc]
            initWithMediaItem:&media
                      library:(vlc_medialibrary_t *)0x1];
    item.testInputItem = inputItem;
    return item;
}

VLCMediaLibraryMediaItem *VLCLibraryDataTypesTestMediaItemWithInputMetadata(
    vlc_ml_media_subtype_t subtype,
    NSDictionary<NSString *, id> * _Nullable metadata)
{
    NSDictionary<NSString *, id> * const defaultMetadata = @{
        VLCLibraryDataTypesTestInputItemNameKey: @"Detail test item",
        VLCLibraryDataTypesTestInputItemTitleKey: @"Detail test item",
        VLCLibraryDataTypesTestInputItemArtistKey: @"Detail Artist",
        VLCLibraryDataTypesTestInputItemAlbumKey: @"Detail Album",
        VLCLibraryDataTypesTestInputItemTrackNumberKey: @"1",
        VLCLibraryDataTypesTestInputItemGenreKey: @"Detail Genre",
        VLCLibraryDataTypesTestInputItemCopyrightKey: @"Detail Copyright",
        VLCLibraryDataTypesTestInputItemPublisherKey: @"Detail Publisher",
        VLCLibraryDataTypesTestInputItemLanguageKey: @"en",
        VLCLibraryDataTypesTestInputItemDescriptionKey: @"Detail Description",
        VLCLibraryDataTypesTestInputItemActorsKey: @"Detail Actor",
        VLCLibraryDataTypesTestInputItemArtworkURLKey: [NSURL fileURLWithPath:@"/tmp/detail-test.jpg"],
    };
    NSMutableDictionary<NSString *, id> * const resolvedMetadata =
        [defaultMetadata mutableCopy];
    [resolvedMetadata addEntriesFromDictionary:metadata ?: @{}];

    VLCInputItem * const inputItem =
        [VLCInputItem inputItemFromURL:[NSURL URLWithString:@"file:///tmp/detail-test.mp4"]];
    inputItem.name = resolvedMetadata[VLCLibraryDataTypesTestInputItemNameKey];
    inputItem.title = resolvedMetadata[VLCLibraryDataTypesTestInputItemTitleKey];
    inputItem.artist = resolvedMetadata[VLCLibraryDataTypesTestInputItemArtistKey];
    inputItem.album = resolvedMetadata[VLCLibraryDataTypesTestInputItemAlbumKey];
    inputItem.trackNumber = resolvedMetadata[VLCLibraryDataTypesTestInputItemTrackNumberKey];
    inputItem.genre = resolvedMetadata[VLCLibraryDataTypesTestInputItemGenreKey];
    inputItem.copyright = resolvedMetadata[VLCLibraryDataTypesTestInputItemCopyrightKey];
    inputItem.publisher = resolvedMetadata[VLCLibraryDataTypesTestInputItemPublisherKey];
    inputItem.language = resolvedMetadata[VLCLibraryDataTypesTestInputItemLanguageKey];
    inputItem.date = resolvedMetadata[VLCLibraryDataTypesTestInputItemDateKey];
    inputItem.contentDescription = resolvedMetadata[VLCLibraryDataTypesTestInputItemDescriptionKey];
    inputItem.director = resolvedMetadata[VLCLibraryDataTypesTestInputItemDirectorKey];
    inputItem.showName = resolvedMetadata[VLCLibraryDataTypesTestInputItemShowNameKey];
    inputItem.actors = resolvedMetadata[VLCLibraryDataTypesTestInputItemActorsKey];
    inputItem.artworkURL = resolvedMetadata[VLCLibraryDataTypesTestInputItemArtworkURLKey];

    const vlc_ml_media_type_t type = subtype == VLC_ML_MEDIA_SUBTYPE_ALBUMTRACK
        ? VLC_ML_MEDIA_TYPE_AUDIO
        : VLC_ML_MEDIA_TYPE_VIDEO;
    return VLCLibraryDataTypesTestMediaItemWithTypeAndSubtypeAndTitle(type,
                                                                       subtype,
                                                                       "Detail test item",
                                                                       inputItem);
}
