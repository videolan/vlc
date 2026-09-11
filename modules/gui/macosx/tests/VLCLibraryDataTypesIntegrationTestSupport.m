/****************************************************************************
 * VLCLibraryDataTypesIntegrationTestSupport.m: medialibrary test lifecycle
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

#import <Cocoa/Cocoa.h>
#import <vlc_interface.h>
#import <vlc/libvlc.h>

#include <string.h>
#include <dispatch/dispatch.h>
#include <sqlite3.h>
#include <unistd.h>

#import "VLCLibraryDataTypesIntegrationTestSupport.h"

#import "library/VLCInputItem.h"

#import "tests/VLCInputItemTestSupport.h"

#import "lib/libvlc_internal.h"

#define MODULE_NAME test_macosx_medialibrary_integration
#undef VLC_DYNAMIC_PLUGIN
#import <vlc_plugin.h>

static libvlc_instance_t *sLibVLC;
static intf_thread_t *sIntf;
static dispatch_semaphore_t sIntfReady;
static vlc_medialibrary_t *sMediaLibrary;
static int64_t sMediaID;
static int64_t sFactoryAudioMediaID;
static int64_t sFactoryVideoMediaID;
static int64_t sFactoryMovieMediaID;
static int64_t sAlbumID;
static int64_t sGenreID;
static int64_t sShowID;
static int64_t sGroupID;
static BOOL sFactoryFixturesPrepared;
static NSString *sUserDataPath;

typedef struct
{
    dispatch_semaphore_t semaphore;
    const char * const *mediaMRLs;
    BOOL *mediaAdded;
    size_t mediaMRLCount;
    size_t mediaAddedCount;
    BOOL failed;
} VLCLibraryDataTypesDiscoveryWait;

static BOOL VLCLibraryDataTypesIntegrationMediaMatchesMRL(const vlc_ml_media_t *media,
                                                          const char *mrl)
{
    if (media == NULL || media->p_files == NULL || mrl == NULL) {
        return NO;
    }
    for (size_t index = 0; index < media->p_files->i_nb_items; ++index) {
        if (media->p_files->p_items[index].psz_mrl != NULL &&
            strcmp(media->p_files->p_items[index].psz_mrl, mrl) == 0) {
            return YES;
        }
    }
    return NO;
}

static BOOL VLCLibraryDataTypesIntegrationSetMovieMetadata(int64_t mediaID)
{
    /* The VLC medialibrary bridge exposes movie fields as read-only. Seed the
     * persisted media and movie rows so the wrapper conversion is tested with
     * a real video-typed movie and non-empty metadata. */
    NSString * const databasePath =
        [sUserDataPath stringByAppendingPathComponent:@"ml/ml.db"];
    sqlite3 *database = NULL;
    if (sqlite3_open_v2(databasePath.fileSystemRepresentation,
                        &database,
                        SQLITE_OPEN_READWRITE,
                        NULL) != SQLITE_OK) {
        if (database != NULL) {
            sqlite3_close(database);
        }
        return NO;
    }

    sqlite3_busy_timeout(database, 5000);
    sqlite3_stmt *statement = NULL;
    BOOL success = sqlite3_prepare_v2(database,
                                      "UPDATE Media SET type = ?, subtype = ? "
                                      "WHERE id_media = ?",
                                      -1,
                                      &statement,
                                      NULL) == SQLITE_OK;
    if (success) {
        sqlite3_bind_int(statement, 1, VLC_ML_MEDIA_TYPE_VIDEO);
        sqlite3_bind_int(statement, 2, VLC_ML_MEDIA_SUBTYPE_MOVIE);
        sqlite3_bind_int64(statement, 3, mediaID);
        success = sqlite3_step(statement) == SQLITE_DONE;
    }
    sqlite3_finalize(statement);

    if (success) {
        success = sqlite3_prepare_v2(database,
                                      "DELETE FROM Movie WHERE media_id = ?",
                                      -1,
                                      &statement,
                                      NULL) == SQLITE_OK;
    }
    if (success) {
        sqlite3_bind_int64(statement, 1, mediaID);
        success = sqlite3_step(statement) == SQLITE_DONE;
    }
    sqlite3_finalize(statement);

    if (success) {
        success = sqlite3_prepare_v2(database,
                                      "INSERT INTO Movie (media_id, summary, imdb_id) "
                                      "VALUES (?, ?, ?)",
                                      -1,
                                      &statement,
                                      NULL) == SQLITE_OK;
    }
    if (success) {
        sqlite3_bind_int64(statement, 1, mediaID);
        sqlite3_bind_text(statement, 2, "Factory movie summary", -1, SQLITE_STATIC);
        sqlite3_bind_text(statement, 3, "tt1234567", -1, SQLITE_STATIC);
        success = sqlite3_step(statement) == SQLITE_DONE;
    }
    sqlite3_finalize(statement);
    sqlite3_close(database);
    return success;
}

static void VLCLibraryDataTypesIntegrationMediaLibraryEvent(void *data,
                                                             const vlc_ml_event_t *event)
{
    VLCLibraryDataTypesDiscoveryWait * const wait = data;
    switch (event->i_type) {
        case VLC_ML_EVENT_MEDIA_ADDED:
            for (size_t index = 0; index < wait->mediaMRLCount; ++index) {
                if (!wait->mediaAdded[index] &&
                    VLCLibraryDataTypesIntegrationMediaMatchesMRL(
                        event->creation.p_media, wait->mediaMRLs[index])) {
                    wait->mediaAdded[index] = YES;
                    wait->mediaAddedCount++;
                    break;
                }
            }
            if (wait->mediaAddedCount == wait->mediaMRLCount) {
                dispatch_semaphore_signal(wait->semaphore);
            }
            break;
        case VLC_ML_EVENT_DISCOVERY_FAILED:
            wait->failed = YES;
            dispatch_semaphore_signal(wait->semaphore);
            break;
        default:
            break;
    }
}

static BOOL VLCLibraryDataTypesIntegrationWaitForMediaAdded(NSString *folderURL,
    NSArray<NSString *> *mediaMRLStrings)
{
    const NSUInteger mediaMRLCount = mediaMRLStrings.count;
    if (mediaMRLCount == 0 || mediaMRLCount > 2) {
        return NO;
    }
    const char *mediaMRLs[2] = { NULL, NULL };
    BOOL mediaAdded[2] = { NO, NO };
    for (NSUInteger index = 0; index < mediaMRLCount; ++index) {
        mediaMRLs[index] = mediaMRLStrings[index].UTF8String;
    }
    VLCLibraryDataTypesDiscoveryWait wait = {
        .semaphore = dispatch_semaphore_create(0),
        .mediaMRLs = mediaMRLs,
        .mediaAdded = mediaAdded,
        .mediaMRLCount = mediaMRLCount,
        .mediaAddedCount = 0,
        .failed = NO,
    };
    vlc_ml_event_callback_t * const callback =
        vlc_ml_event_register_callback(sMediaLibrary,
                                       VLCLibraryDataTypesIntegrationMediaLibraryEvent,
                                       &wait);
    if (callback == NULL) {
        return NO;
    }

    const BOOL added = vlc_ml_add_folder(sMediaLibrary, folderURL.UTF8String) == VLC_SUCCESS;
    BOOL completed = NO;
    if (added) {
        const dispatch_time_t deadline =
            dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC);
        completed = dispatch_semaphore_wait(wait.semaphore, deadline) == 0 &&
                    wait.mediaAddedCount == wait.mediaMRLCount && !wait.failed;
    }
    vlc_ml_event_unregister_callback(sMediaLibrary, callback);
    return completed;
}

static int OpenIntegrationInterface(vlc_object_t *root)
{
    sIntf = (intf_thread_t *)root;
    if (sIntfReady != NULL)
        dispatch_semaphore_signal(sIntfReady);
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_callback(OpenIntegrationInterface)
    set_capability("interface", 0)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

BOOL VLCLibraryDataTypesIntegrationStart(void)
{
    if (sLibVLC != nil) {
        return YES;
    }

    char userDataTemplate[] = "/private/tmp/vlc-macosx-medialibrary.XXXXXX";
    const char * const userDataPath = mkdtemp(userDataTemplate);
    if (userDataPath == NULL) {
        return NO;
    }
    sUserDataPath = [[NSString alloc] initWithUTF8String:userDataPath];
    setenv("VLC_USERDATA_PATH", sUserDataPath.fileSystemRepresentation, 1);

    NSString * const bundlePath = [[NSBundle bundleForClass:[VLCInputItem class]] bundlePath];
    NSString * const modulesPath = [[bundlePath stringByDeletingLastPathComponent]
                                    stringByAppendingPathComponent:@".libs"];
    NSString * const buildModulesPath = [modulesPath stringByDeletingLastPathComponent];
    NSString * const appLibexecPath = [[[[buildModulesPath stringByDeletingLastPathComponent]
                                         stringByAppendingPathComponent:@"VLC.app"]
                                        stringByAppendingPathComponent:@"Contents"]
                                       stringByAppendingPathComponent:@"MacOS"];
    setenv("VLC_PLUGIN_PATH", modulesPath.fileSystemRepresentation, 1);
    if ([[NSFileManager defaultManager]
            fileExistsAtPath:[appLibexecPath stringByAppendingPathComponent:@"vlc-preparser"]]) {
        /* The packaged helper has the application framework rpaths needed by
         * the dynamically-linked external preparser. */
        setenv("VLC_LIB_PATH", modulesPath.fileSystemRepresentation, 1);
        setenv("VLC_LIBEXEC_PATH", appLibexecPath.fileSystemRepresentation, 1);
    } else {
        /* A build-tree-only checkout uses the static helper selected by the
         * external preparser when VLC_LIB_PATH ends in `/modules`. */
        setenv("VLC_LIB_PATH", buildModulesPath.fileSystemRepresentation, 1);
        unsetenv("VLC_LIBEXEC_PATH");
    }

    const char * const args[] = {
        "--vout=dummy",
        "--aout=dummy",
        "--text-renderer=dummy",
        "--no-auto-preparse",
        "--media-library",
    };
    sIntfReady = dispatch_semaphore_create(0);
    sLibVLC = libvlc_new((int)(sizeof(args) / sizeof(args[0])), args);
    if (sLibVLC == nil) {
        VLCLibraryDataTypesIntegrationStop();
        return NO;
    }

    if (libvlc_InternalAddIntf(sLibVLC->p_libvlc_int, MODULE_STRING) != VLC_SUCCESS) {
        VLCLibraryDataTypesIntegrationStop();
        return NO;
    }
    libvlc_InternalPlay(sLibVLC->p_libvlc_int);

    const dispatch_time_t interfaceDeadline =
        dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC);
    if (dispatch_semaphore_wait(sIntfReady, interfaceDeadline) != 0) {
        VLCLibraryDataTypesIntegrationStop();
        return NO;
    }

    sMediaLibrary = vlc_ml_instance_get(VLC_OBJECT(sIntf));
    if (sMediaLibrary == NULL) {
        VLCLibraryDataTypesIntegrationStop();
        return NO;
    }
    VLCInputItemTestSetInterface(sIntf);

    vlc_ml_media_t * const media =
        vlc_ml_new_external_media(sMediaLibrary, "mock://macosx-datatypes-integration");
    if (media == NULL) {
        VLCLibraryDataTypesIntegrationStop();
        return NO;
    }
    sMediaID = media->i_id;
    vlc_ml_media_release(media);
    return YES;
}

void VLCLibraryDataTypesIntegrationStop(void)
{
    sMediaLibrary = NULL;
    sMediaID = 0;
    sIntfReady = nil;
    sFactoryAudioMediaID = 0;
    sFactoryVideoMediaID = 0;
    sFactoryMovieMediaID = 0;
    sAlbumID = 0;
    sGenreID = 0;
    sShowID = 0;
    sGroupID = 0;
    sFactoryFixturesPrepared = NO;
    VLCInputItemTestSetInterface(NULL);
    sIntf = NULL;
    if (sLibVLC != nil) {
        libvlc_release(sLibVLC);
        sLibVLC = nil;
    }
    if (sUserDataPath != nil) {
        [[NSFileManager defaultManager] removeItemAtPath:sUserDataPath error:nil];
        sUserDataPath = nil;
    }
}

vlc_medialibrary_t *VLCLibraryDataTypesIntegrationMediaLibrary(void)
{
    return sMediaLibrary;
}

int64_t VLCLibraryDataTypesIntegrationMediaID(void)
{
    return sMediaID;
}

int64_t VLCLibraryDataTypesIntegrationCreateExternalMedia(const char *mrl)
{
    if (sMediaLibrary == NULL || mrl == NULL) {
        return 0;
    }

    vlc_ml_media_t * const media = vlc_ml_new_external_media(sMediaLibrary, mrl);
    if (media == NULL) {
        return 0;
    }

    const int64_t mediaID = media->i_id;
    vlc_ml_media_release(media);
    return mediaID;
}

int64_t VLCLibraryDataTypesIntegrationCreatePlaylist(void)
{
    vlc_ml_playlist_t * const playlist =
        vlc_ml_playlist_create(sMediaLibrary, "Integration Playlist");
    if (playlist == NULL) {
        return 0;
    }

    const int64_t playlistID = playlist->i_id;
    vlc_ml_playlist_release(playlist);
    return playlistID;
}

static void VLCLibraryDataTypesIntegrationWriteID3v1Tag(NSMutableData *data)
{
    unsigned char tag[128] = { 0 };
    memcpy(tag, "TAG", 3);
    memcpy(tag + 3, "Factory Track", 13);
    memcpy(tag + 33, "Factory Artist", 14);
    memcpy(tag + 63, "Factory Album", 13);
    memcpy(tag + 93, "2026", 4);
    memcpy(tag + 97, "VLC test fixture", 16);
    tag[127] = 17; // Rock, from the ID3v1 genre table.
    [data appendBytes:tag length:sizeof(tag)];
}

static NSString *VLCLibraryDataTypesIntegrationVideoFixtureBase64(void)
{
    return @"AAAAIGZ0eXBpc29tAAACAGlzb21pc28yYXZjMW1wNDEAAAMVbW9vdgAAAGxtdmhkAAAAAAAAAAAAAAAAAAAD6AAAA+gAAQAAAQAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAgAAAj90cmFrAAAAXHRraGQAAAADAAAAAAAAAAAAAAABAAAAAAAAA+gAAAAAAAAAAAAAAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAABAAAAAABAAAAAQAAAAAAAkZWR0cwAAABxlbHN0AAAAAAAAAAEAAAPoAAAAAAABAAAAAAG3bWRpYQAAACBtZGhkAAAAAAAAAAAAAAAAAABAAAAAQABVxAAAAAAALWhkbHIAAAAAAAAAAHZpZGUAAAAAAAAAAAAAAABWaWRlb0hhbmRsZXIAAAABYm1pbmYAAAAUdm1oZAAAAAEAAAAAAAAAAAAAACRkaW5mAAAAHGRyZWYAAAAAAAAAAQAAAAx1cmwgAAAAAQAAASJzdGJsAAAAvnN0c2QAAAAAAAAAAQAAAK5hdmMxAAAAAAAAAAEAAAAAAAAAAAAAAAAAAAAAABAAEABIAAAASAAAAAAAAAABFUxhdmM2Mi4yOC4xMDEgbGlieDI2NAAAAAAAAAAAAAAAGP//AAAANGF2Y0MBZAAK/+EAF2dkAAqs2V7ARAAAAwAEAAADAAg8SJZYAQAGaOvjyyLA/fj4AAAAABBwYXNwAAAAAQAAAAEAAAAUYnRydAAAAAAAABYoAAAAAAAAABhzdHRzAAAAAAAAAAEAAAABAABAAAAAABxzdHNjAAAAAAAAAAEAAAABAAAAAQAAAAEAAAAUc3RzegAAAAAAAALFAAAAAQAAABRzdGNvAAAAAAAAAAEAAANFAAAAYnVkdGEAAABabWV0YQAAAAAAAAAhaGRscgAAAAAAAAAAbWRpcmFwcGwAAAAAAAAAAAAAAAAtaWxzdAAAACWpdG9vAAAAHWRhdGEAAAABAAAAAExhdmY2Mi4xMi4xMDEAAAAIZnJlZQAAAs1tZGF0AAACrQYF//+p3EXpvebZSLeWLNgg2SPu73gyNjQgLSBjb3JlIDE2NSByMzIyMiBiMzU2MDVhIC0gSC4yNjQvTVBFRy00IEFWQyBjb2RlYyAtIENvcHlsZWZ0IDIwMDMtMjAyNSAtIGh0dHA6Ly93d3cudmlkZW9sYW4ub3JnL3gyNjQuaHRtbCAtIG9wdGlvbnM6IGNhYmFjPTEgcmVmPTMgZGVibG9jaz0xOjA6MCBhbmFseXNlPTB4MzoweDExMyBtZT1oZXggc3VibWU9NyBwc3k9MSBwc3lfcmQ9MS4wMDowLjAwIG1peGVkX3JlZj0xIG1lX3JhbmdlPTE2IGNocm9tYV9tZT0xIHRyZWxsaXM9MSA4eDhkY3Q9MSBjcW09MCBkZWFkem9uZT0yMSwxMSBmYXN0X3Bza2lwPTEgY2hyb21hX3FwX29mZnNldD0tMiB0aHJlYWRzPTEgbG9va2FoZWFkX3RocmVhZHM9MSBzbGljZWRfdGhyZWFkcz0wIG5yPTAgZGVjaW1hdGU9MSBpbnRlcmxhY2VkPTAgYmx1cmF5X2NvbXBhdD0wIGNvbnN0cmFpbmVkX2ludHJhPTAgYmZyYW1lcz0zIGJfcHlyYW1pZD0yIGJfYWRhcHQ9MSBiX2JpYXM9MCBkaXJlY3Q9MSB3ZWlnaHRiPTEgb3Blbl9nb3A9MCB3ZWlnaHRwPTIga2V5aW50PTI1MCBrZXlpbnRfbWluPTEgc2NlbmVjdXQ9NDAgaW50cmFfcmVmcmVzaD0wIHJjX2xvb2thaGVhZD00MCByYz1jcmYgbWJ0cmVlPTEgY3JmPTIzLjAgcWNvbXA9MC42MCBxcG1pbj0wIHFwbWF4PTY5IHFwc3RlcD00IGlwX3JhdGlvPTEuNDAgYXE9MToxLjAwAIAAAAAQZYiEABX//vfJ78Cm69vfgQ==";
}

BOOL VLCLibraryDataTypesIntegrationPrepareFactoryFixtures(void)
{
    if (sFactoryFixturesPrepared) {
        return YES;
    }
    if (sMediaLibrary == NULL || sUserDataPath == nil) {
        return NO;
    }

    NSString * const sourcePath =
        @VLC_MACOSX_TEST_SOURCE_DIR "/test/samples/meta.mp3";
    NSMutableData * const audioData =
        [[NSMutableData alloc] initWithContentsOfFile:sourcePath];
    if (audioData == nil) {
        return NO;
    }
    VLCLibraryDataTypesIntegrationWriteID3v1Tag(audioData);

    NSString * const fixtureDirectory =
        [sUserDataPath stringByAppendingPathComponent:@"factory-fixtures"];
    if (![[NSFileManager defaultManager] createDirectoryAtPath:fixtureDirectory
                                    withIntermediateDirectories:YES
                                                     attributes:nil
                                                          error:nil]) {
        return NO;
    }
    NSString * const fixturePath =
        [fixtureDirectory stringByAppendingPathComponent:@"factory-track.mp3"];
    if (![audioData writeToFile:fixturePath atomically:YES]) {
        return NO;
    }
    NSData * const videoData = [[NSData alloc]
        initWithBase64EncodedString:VLCLibraryDataTypesIntegrationVideoFixtureBase64()
                             options:0];
    NSString * const videoPath =
        [fixtureDirectory stringByAppendingPathComponent:@"Factory Show S01E01.mp4"];
    if (videoData == nil || ![videoData writeToFile:videoPath atomically:YES]) {
        return NO;
    }
    NSString * const audioMRL = [NSURL fileURLWithPath:fixturePath].absoluteString;
    NSString * const videoMRL = [NSURL fileURLWithPath:videoPath].absoluteString;
    if (!VLCLibraryDataTypesIntegrationWaitForMediaAdded(
            [NSURL fileURLWithPath:fixtureDirectory].absoluteString,
            @[audioMRL, videoMRL])) {
        return NO;
    }

    vlc_ml_album_list_t * const albums = vlc_ml_list_albums(sMediaLibrary, NULL);
    vlc_ml_genre_list_t * const genres = vlc_ml_list_genres(sMediaLibrary, NULL);
    vlc_ml_show_list_t * const shows = vlc_ml_list_shows(sMediaLibrary, NULL);
    vlc_ml_group_list_t * const groups = vlc_ml_list_groups(sMediaLibrary, NULL);
    if (albums != NULL) {
        for (size_t index = 0; index < albums->i_nb_items; ++index) {
            if (albums->p_items[index].psz_title != NULL &&
                strcmp(albums->p_items[index].psz_title, "Factory Album") == 0) {
                sAlbumID = albums->p_items[index].i_id;
                break;
            }
        }
    }
    if (genres != NULL) {
        for (size_t index = 0; index < genres->i_nb_items; ++index) {
            if (genres->p_items[index].psz_name != NULL &&
                strcmp(genres->p_items[index].psz_name, "Rock") == 0) {
                sGenreID = genres->p_items[index].i_id;
                break;
            }
        }
    }
    if (shows != NULL) {
        for (size_t index = 0; index < shows->i_nb_items; ++index) {
            if (shows->p_items[index].psz_name != NULL &&
                strcmp(shows->p_items[index].psz_name, "Factory Show") == 0) {
                sShowID = shows->p_items[index].i_id;
                break;
            }
        }
    }
    if (groups != NULL) {
        for (size_t index = 0; index < groups->i_nb_items; ++index) {
            if (groups->p_items[index].psz_name != NULL &&
                strcmp(groups->p_items[index].psz_name, "Factory Show S01E01") == 0) {
                sGroupID = groups->p_items[index].i_id;
                break;
            }
        }
    }
    vlc_ml_media_t * const audio =
        vlc_ml_get_media_by_mrl(sMediaLibrary, audioMRL.UTF8String);
    vlc_ml_media_t * const video =
        vlc_ml_get_media_by_mrl(sMediaLibrary, videoMRL.UTF8String);
    if (audio != NULL) {
        sFactoryAudioMediaID = audio->i_id;
        vlc_ml_media_release(audio);
    }
    if (video != NULL) {
        sFactoryVideoMediaID = video->i_id;
        vlc_ml_media_release(video);
    }
    if (albums != NULL) {
        vlc_ml_album_list_release(albums);
    }
    if (genres != NULL) {
        vlc_ml_genre_list_release(genres);
    }
    if (shows != NULL) {
        vlc_ml_show_list_release(shows);
    }
    if (groups != NULL) {
        vlc_ml_group_list_release(groups);
    }
    if (sFactoryAudioMediaID == 0 || sFactoryVideoMediaID == 0 ||
        sAlbumID == 0 || sGenreID == 0 || sShowID == 0 || sGroupID == 0) {
        return NO;
    }
    sFactoryFixturesPrepared = YES;
    return YES;
}

BOOL VLCLibraryDataTypesIntegrationPrepareMovieFixture(void)
{
    if (sMediaLibrary == NULL || sUserDataPath == nil) {
        return NO;
    }

    NSString * const fixtureDirectory =
        [sUserDataPath stringByAppendingPathComponent:@"movie-fixtures"];
    NSString * const moviePath =
        [fixtureDirectory stringByAppendingPathComponent:@"Factory Movie.mp4"];

    if (sFactoryMovieMediaID != 0) {
        vlc_ml_media_t * const existingMedia =
            vlc_ml_get_media(sMediaLibrary, sFactoryMovieMediaID);
        if (existingMedia != NULL) {
            const BOOL isMovie = existingMedia->i_subtype == VLC_ML_MEDIA_SUBTYPE_MOVIE;
            vlc_ml_media_release(existingMedia);
            if (isMovie && [[NSFileManager defaultManager] fileExistsAtPath:moviePath]) {
                return VLCLibraryDataTypesIntegrationSetMovieMetadata(sFactoryMovieMediaID);
            }
        }
        sFactoryMovieMediaID = 0;
    }
    if (![[NSFileManager defaultManager] createDirectoryAtPath:fixtureDirectory
                                   withIntermediateDirectories:YES
                                                    attributes:nil
                                                         error:nil]) {
        return NO;
    }

    NSData * const movieData = [[NSData alloc]
        initWithBase64EncodedString:VLCLibraryDataTypesIntegrationVideoFixtureBase64()
                            options:0];
    if (movieData == nil || ![movieData writeToFile:moviePath atomically:YES]) {
        return NO;
    }

    NSString * const movieMRL = [NSURL fileURLWithPath:moviePath].absoluteString;
    vlc_ml_media_t * const externalMedia =
        vlc_ml_new_external_media(sMediaLibrary, movieMRL.UTF8String);
    if (externalMedia == NULL) {
        return NO;
    }
    sFactoryMovieMediaID = externalMedia->i_id;
    vlc_ml_media_release(externalMedia);

    if (!VLCLibraryDataTypesIntegrationSetMovieMetadata(sFactoryMovieMediaID)) {
        sFactoryMovieMediaID = 0;
        return NO;
    }

    vlc_ml_media_t * const movie =
        vlc_ml_get_media(sMediaLibrary, sFactoryMovieMediaID);
    const BOOL isMovie = movie != NULL && movie->i_subtype == VLC_ML_MEDIA_SUBTYPE_MOVIE;
    if (movie != NULL) {
        vlc_ml_media_release(movie);
    }
    if (!isMovie) {
        sFactoryMovieMediaID = 0;
    }
    return isMovie;
}

int64_t VLCLibraryDataTypesIntegrationFactoryAudioMediaID(void)
{
    return sFactoryAudioMediaID;
}

int64_t VLCLibraryDataTypesIntegrationFactoryVideoMediaID(void)
{
    return sFactoryVideoMediaID;
}

int64_t VLCLibraryDataTypesIntegrationFactoryMovieMediaID(void)
{
    return sFactoryMovieMediaID;
}

int64_t VLCLibraryDataTypesIntegrationAlbumID(void)
{
    return sAlbumID;
}

int64_t VLCLibraryDataTypesIntegrationGenreID(void)
{
    return sGenreID;
}

int64_t VLCLibraryDataTypesIntegrationShowID(void)
{
    return sShowID;
}

int64_t VLCLibraryDataTypesIntegrationGroupID(void)
{
    return sGroupID;
}
