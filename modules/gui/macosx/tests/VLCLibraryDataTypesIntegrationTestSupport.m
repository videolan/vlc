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
#include <stdlib.h>
#include <dispatch/dispatch.h>
#include <signal.h>
#include <fcntl.h>
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
static int64_t sSortingAudioAlphaMediaID;
static int64_t sSortingAudioZuluMediaID;
static int64_t sSortingVideoAlphaMediaID;
static int64_t sSortingVideoZuluMediaID;
static int64_t sAlbumID;
static int64_t sGenreID;
static int64_t sShowID;
static int64_t sGroupID;
static BOOL sFactoryFixturesPrepared;
static NSString *sUserDataPath;

// The media-library plugin has its own SQLite copy. Run fixture SQL in another
// process so SQLite's process-wide file locks keep the two clients coordinated.
static NSDictionary *VLCLibraryDataTypesIntegrationSQLStatement(NSString *sql,
                                                               NSArray *parameters,
                                                               NSUInteger minimumChanges)
{
    return @{ @"sql": sql, @"parameters": parameters, @"minimumChanges": @(minimumChanges) };
}

static BOOL VLCLibraryDataTypesIntegrationExecuteStatements(NSArray<NSDictionary *> *statements)
{
    NSString * const databasePath = [sUserDataPath stringByAppendingPathComponent:@"ml/ml.db"];
    NSString * const helperPath = [[[[NSBundle bundleForClass:VLCInputItem.class] executablePath]
        stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"VLCLibraryFixtureSQL"];
    NSError *error = nil;
    NSData * const request = [NSJSONSerialization dataWithJSONObject:
        @{ @"database": databasePath, @"statements": statements }
        options:0 error:&error];
    if (request == nil) {
        NSLog(@"Could not encode fixture SQL request: %@", error);
        return NO;
    }

    NSTask * const task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:helperPath];
    NSPipe * const input = NSPipe.pipe;
    NSPipe * const output = NSPipe.pipe;
    task.standardInput = input;
    task.standardOutput = output;
    // A child killed at the deadline must not send SIGPIPE to the test runner.
    if (fcntl(input.fileHandleForWriting.fileDescriptor, F_SETNOSIGPIPE, 1) < 0) {
        [input.fileHandleForWriting closeFile];
        [output.fileHandleForReading closeFile];
        NSLog(@"Could not configure fixture SQL input pipe");
        return NO;
    }
    // The child reports one small JSON result; diagnostics inherit stderr.
    dispatch_semaphore_t const finished = dispatch_semaphore_create(0);
    task.terminationHandler = ^(NSTask * __unused terminatedTask) {
        dispatch_semaphore_signal(finished);
    };
    if (![task launchAndReturnError:&error]) {
        [input.fileHandleForWriting closeFile];
        [output.fileHandleForReading closeFile];
        NSLog(@"Could not launch fixture SQL helper: %@", error);
        return NO;
    }

    dispatch_group_t const io = dispatch_group_create();
    dispatch_queue_t const queue = dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0);
    __block NSString *writeError = nil;
    __block NSString *readError = nil;
    __block NSData *responseData = nil;
    dispatch_group_async(io, queue, ^{
        @try {
            [input.fileHandleForWriting writeData:request];
        } @catch (NSException *exception) {
            writeError = exception.reason;
        } @finally {
            [input.fileHandleForWriting closeFile];
        }
    });
    dispatch_group_async(io, queue, ^{
        @try {
            responseData = [output.fileHandleForReading readDataToEndOfFile];
        } @catch (NSException *exception) {
            readError = exception.reason;
        } @finally {
            [output.fileHandleForReading closeFile];
        }
    });
    const BOOL timedOut = dispatch_semaphore_wait(finished,
        dispatch_time(DISPATCH_TIME_NOW, 15 * NSEC_PER_SEC)) != 0;
    if (timedOut && task.running)
        kill(task.processIdentifier, SIGKILL);
    // Reap the child and finish pipe I/O before the fixture database is removed.
    [task waitUntilExit];
    dispatch_group_wait(io, DISPATCH_TIME_FOREVER);
    if (timedOut || writeError != nil || readError != nil) {
        NSLog(@"Fixture SQL transport failed: %@", timedOut
            ? @"helper exceeded its 15-second deadline" : writeError ?: readError);
        return NO;
    }
    id const response = [NSJSONSerialization JSONObjectWithData:responseData options:0 error:&error];
    if (task.terminationStatus != EXIT_SUCCESS ||
        ![response isKindOfClass:NSDictionary.class] || ![response[@"success"] boolValue]) {
        NSLog(@"Fixture SQL helper failed (exit %d): %@", task.terminationStatus,
              [response isKindOfClass:NSDictionary.class] ? response[@"error"] : error);
        return NO;
    }
    return YES;
}

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
    // The VLC bridge exposes movie fields as read-only. Seed the related rows
    // atomically so media-library readers cannot observe a partly converted movie.
    return VLCLibraryDataTypesIntegrationExecuteStatements(@[
        VLCLibraryDataTypesIntegrationSQLStatement(
            @"UPDATE Media SET type = ?1, subtype = ?2 WHERE id_media = ?3",
            @[ @(VLC_ML_MEDIA_TYPE_VIDEO), @(VLC_ML_MEDIA_SUBTYPE_MOVIE), @(mediaID) ], 1),
        VLCLibraryDataTypesIntegrationSQLStatement(
            @"DELETE FROM Movie WHERE media_id = ?1", @[ @(mediaID) ], 0),
        VLCLibraryDataTypesIntegrationSQLStatement(
            @"INSERT INTO Movie (media_id, summary, imdb_id) VALUES (?1, ?2, ?3)",
            @[ @(mediaID), @"Factory movie summary", @"tt1234567" ], 1),
    ]);
}

static BOOL VLCLibraryDataTypesIntegrationSetMediaType(int64_t mediaID,
                                                       int type)
{
    return VLCLibraryDataTypesIntegrationExecuteStatements(@[
        VLCLibraryDataTypesIntegrationSQLStatement(
            @"UPDATE Media SET type = ?2, import_type = 0, is_present = 1 WHERE id_media = ?1",
            @[ @(mediaID), @(type) ], 1),
    ]);
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
            break;
        case VLC_ML_EVENT_BACKGROUND_IDLE_CHANGED:
            if (event->background_idle_changed.b_idle &&
                wait->mediaAddedCount == wait->mediaMRLCount) {
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
    if (mediaMRLCount == 0 || mediaMRLCount > 6) {
        return NO;
    }
    const char *mediaMRLs[6] = { NULL };
    BOOL mediaAdded[6] = { NO };
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
    if (!completed) {
        NSLog(@"Fixture discovery did not finish: %zu/%zu media added, discovery failed: %d",
              wait.mediaAddedCount,
              wait.mediaMRLCount,
              wait.failed);
    }
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
    /* Use the build's plugins.dat without scanning .libs: loading the macOS GUI
     * plugin here would register a second copy of the classes under test. */
    setenv("VLC_PLUGIN_PATH", buildModulesPath.fileSystemRepresentation, 1);
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
        "--ignore-config",
        "--no-plugins-scan",
        "--vout=dummy",
        "--aout=dummy",
        "--text-renderer=dummy",
        "--no-auto-preparse",
        "--media-library",
        "--no-ml-network-discovery",
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
    sSortingAudioAlphaMediaID = 0;
    sSortingAudioZuluMediaID = 0;
    sSortingVideoAlphaMediaID = 0;
    sSortingVideoZuluMediaID = 0;
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

static void VLCLibraryDataTypesIntegrationWriteID3v1Tag(NSMutableData *data,
                                                        const char *title,
                                                        const char *artist,
                                                        const char *album,
                                                        unsigned char genre)
{
    unsigned char tag[128] = { 0 };
    memcpy(tag, "TAG", 3);
    memcpy(tag + 3, title, MIN(strlen(title), (size_t)30));
    memcpy(tag + 33, artist, MIN(strlen(artist), (size_t)30));
    memcpy(tag + 63, album, MIN(strlen(album), (size_t)30));
    memcpy(tag + 93, "2026", 4);
    memcpy(tag + 97, "VLC test fixture", 16);
    tag[127] = genre;
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
    if (audioData.length < 10) {
        return NO;
    }
    const unsigned char * const bytes = audioData.bytes;
    if (memcmp(bytes, "ID3", 3) != 0) {
        return NO;
    }
    /* The sample's ID3v2 title and artist take precedence over ID3v1.
     * Remove that tag so the fixture metadata is what gets indexed. */
    NSUInteger tagLength = 10 + ((NSUInteger)bytes[6] << 21) +
        ((NSUInteger)bytes[7] << 14) + ((NSUInteger)bytes[8] << 7) + bytes[9];
    if ((bytes[5] & 0x10) != 0) {
        tagLength += 10;
    }
    if (tagLength >= audioData.length) {
        return NO;
    }
    [audioData replaceBytesInRange:NSMakeRange(0, tagLength) withBytes:NULL length:0];
    VLCLibraryDataTypesIntegrationWriteID3v1Tag(audioData,
                                                "Factory Track",
                                                "Factory Artist",
                                                "Factory Album",
                                                17);

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
    NSString * const sortingAudioAlphaPath =
        [fixtureDirectory stringByAppendingPathComponent:@"vlc-sort-audio-alpha.mp3"];
    NSString * const sortingAudioZuluPath =
        [fixtureDirectory stringByAppendingPathComponent:@"vlc-sort-audio-zulu.mp3"];
    NSMutableData * const sortingAudioAlphaData = [audioData mutableCopy];
    NSMutableData * const sortingAudioZuluData = [audioData mutableCopy];
    [sortingAudioAlphaData setLength:sortingAudioAlphaData.length - 128];
    [sortingAudioZuluData setLength:sortingAudioZuluData.length - 128];
    VLCLibraryDataTypesIntegrationWriteID3v1Tag(sortingAudioAlphaData,
                                                "vlc-sort-audio-alpha.mp3",
                                                "Sorting Artist",
                                                "Sorting Album",
                                                0);
    VLCLibraryDataTypesIntegrationWriteID3v1Tag(sortingAudioZuluData,
                                                "vlc-sort-audio-zulu.mp3",
                                                "Sorting Artist",
                                                "Sorting Album",
                                                0);
    NSString * const sortingVideoAlphaPath =
        [fixtureDirectory stringByAppendingPathComponent:@"vlc-sort-video-alpha.mp4"];
    NSString * const sortingVideoZuluPath =
        [fixtureDirectory stringByAppendingPathComponent:@"vlc-sort-video-zulu.mp4"];
    if (![sortingAudioAlphaData writeToFile:sortingAudioAlphaPath atomically:YES] ||
        ![sortingAudioZuluData writeToFile:sortingAudioZuluPath atomically:YES] ||
        ![videoData writeToFile:sortingVideoAlphaPath atomically:YES] ||
        ![videoData writeToFile:sortingVideoZuluPath atomically:YES]) {
        return NO;
    }
    NSArray<NSString *> * const mediaMRLs = @[
        audioMRL,
        videoMRL,
        [NSURL fileURLWithPath:sortingAudioAlphaPath].absoluteString,
        [NSURL fileURLWithPath:sortingAudioZuluPath].absoluteString,
        [NSURL fileURLWithPath:sortingVideoAlphaPath].absoluteString,
        [NSURL fileURLWithPath:sortingVideoZuluPath].absoluteString,
    ];
    if (!VLCLibraryDataTypesIntegrationWaitForMediaAdded(
            [NSURL fileURLWithPath:fixtureDirectory].absoluteString,
            mediaMRLs)) {
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
    vlc_ml_media_t * const sortingAudioAlpha =
        vlc_ml_get_media_by_mrl(sMediaLibrary, mediaMRLs[2].UTF8String);
    vlc_ml_media_t * const sortingAudioZulu =
        vlc_ml_get_media_by_mrl(sMediaLibrary, mediaMRLs[3].UTF8String);
    vlc_ml_media_t * const sortingVideoAlpha =
        vlc_ml_get_media_by_mrl(sMediaLibrary, mediaMRLs[4].UTF8String);
    vlc_ml_media_t * const sortingVideoZulu =
        vlc_ml_get_media_by_mrl(sMediaLibrary, mediaMRLs[5].UTF8String);
    if (audio != NULL) {
        sFactoryAudioMediaID = audio->i_id;
        vlc_ml_media_release(audio);
    }
    if (video != NULL) {
        sFactoryVideoMediaID = video->i_id;
        vlc_ml_media_release(video);
    }
    if (sortingAudioAlpha != NULL) {
        sSortingAudioAlphaMediaID = sortingAudioAlpha->i_id;
        vlc_ml_media_release(sortingAudioAlpha);
    }
    if (sortingAudioZulu != NULL) {
        sSortingAudioZuluMediaID = sortingAudioZulu->i_id;
        vlc_ml_media_release(sortingAudioZulu);
    }
    if (sortingVideoAlpha != NULL) {
        sSortingVideoAlphaMediaID = sortingVideoAlpha->i_id;
        vlc_ml_media_release(sortingVideoAlpha);
    }
    if (sortingVideoZulu != NULL) {
        sSortingVideoZuluMediaID = sortingVideoZulu->i_id;
        vlc_ml_media_release(sortingVideoZulu);
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
        sSortingAudioAlphaMediaID == 0 || sSortingAudioZuluMediaID == 0 ||
        sSortingVideoAlphaMediaID == 0 || sSortingVideoZuluMediaID == 0 ||
        sAlbumID == 0 || sGenreID == 0 || sShowID == 0 || sGroupID == 0) {
        return NO;
    }

    /* The tiny fixture is sufficient for metadata extraction, but the
     * preparser does not consistently classify it in every test environment.
     * Seed the persisted category without scheduling another parse task. */
    if (!VLCLibraryDataTypesIntegrationSetMediaType(sFactoryAudioMediaID,
                                                    VLC_ML_MEDIA_TYPE_AUDIO) ||
        !VLCLibraryDataTypesIntegrationSetMediaType(sFactoryVideoMediaID,
                                                    VLC_ML_MEDIA_TYPE_VIDEO) ||
        !VLCLibraryDataTypesIntegrationSetMediaType(sSortingAudioAlphaMediaID,
                                                    VLC_ML_MEDIA_TYPE_AUDIO) ||
        !VLCLibraryDataTypesIntegrationSetMediaType(sSortingAudioZuluMediaID,
                                                    VLC_ML_MEDIA_TYPE_AUDIO) ||
        !VLCLibraryDataTypesIntegrationSetMediaType(sSortingVideoAlphaMediaID,
                                                    VLC_ML_MEDIA_TYPE_VIDEO) ||
        !VLCLibraryDataTypesIntegrationSetMediaType(sSortingVideoZuluMediaID,
                                                    VLC_ML_MEDIA_TYPE_VIDEO)) {
        return NO;
    }
    sFactoryFixturesPrepared = YES;
    return YES;
}

BOOL VLCLibraryDataTypesIntegrationConvertMediaToMovie(int64_t mediaID)
{
    if (!VLCLibraryDataTypesIntegrationSetMovieMetadata(mediaID) ||
        !VLCLibraryDataTypesIntegrationSetMediaType(mediaID, VLC_ML_MEDIA_TYPE_VIDEO))
        return NO;
    vlc_ml_media_t * const media = vlc_ml_get_media(sMediaLibrary, mediaID);
    const BOOL isMovie = media != NULL && media->i_subtype == VLC_ML_MEDIA_SUBTYPE_MOVIE;
    if (media != NULL)
        vlc_ml_media_release(media);
    return isMovie;
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
                return VLCLibraryDataTypesIntegrationSetMovieMetadata(sFactoryMovieMediaID) &&
                       VLCLibraryDataTypesIntegrationSetMediaType(sFactoryMovieMediaID,
                                                                  VLC_ML_MEDIA_TYPE_VIDEO);
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

    if (!VLCLibraryDataTypesIntegrationSetMediaType(sFactoryMovieMediaID,
                                                    VLC_ML_MEDIA_TYPE_VIDEO)) {
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

int64_t VLCLibraryDataTypesIntegrationSortingAudioAlphaMediaID(void)
{
    return sSortingAudioAlphaMediaID;
}

int64_t VLCLibraryDataTypesIntegrationSortingAudioZuluMediaID(void)
{
    return sSortingAudioZuluMediaID;
}

int64_t VLCLibraryDataTypesIntegrationSortingVideoAlphaMediaID(void)
{
    return sSortingVideoAlphaMediaID;
}

int64_t VLCLibraryDataTypesIntegrationSortingVideoZuluMediaID(void)
{
    return sSortingVideoZuluMediaID;
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

int64_t VLCLibraryDataTypesIntegrationCreateScannedMedia(NSString *name, vlc_ml_media_type_t type)
{
    if ((type != VLC_ML_MEDIA_TYPE_AUDIO && type != VLC_ML_MEDIA_TYPE_VIDEO) ||
        !VLCLibraryDataTypesIntegrationPrepareFactoryFixtures()) {
        return 0;
    }

    const BOOL audio = type == VLC_ML_MEDIA_TYPE_AUDIO;
    NSString * const sourceName = audio ? @"factory-track.mp3" : @"Factory Show S01E01.mp4";
    NSString * const source = [[sUserDataPath stringByAppendingPathComponent:@"factory-fixtures"]
        stringByAppendingPathComponent:sourceName];
    NSString * const directory = [sUserDataPath stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    NSString * const path = [directory stringByAppendingPathComponent:
        [name stringByAppendingPathExtension:audio ? @"mp3" : @"mp4"]];
    if (![NSFileManager.defaultManager createDirectoryAtPath:directory
                                 withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:nil] ||
        ![NSFileManager.defaultManager copyItemAtPath:source toPath:path error:nil] ||
        !VLCLibraryDataTypesIntegrationWaitForMediaAdded(
            [NSURL fileURLWithPath:directory].absoluteString,
            @[[NSURL fileURLWithPath:path].absoluteString])) {
        return 0;
    }

    vlc_ml_media_t * const media = vlc_ml_get_media_by_mrl(sMediaLibrary,
                                                           [NSURL fileURLWithPath:path].absoluteString.UTF8String);
    if (media == NULL) {
        return 0;
    }
    const int64_t mediaID = media->i_id;
    vlc_ml_media_release(media);
    return VLCLibraryDataTypesIntegrationSetMediaType(mediaID, type) ? mediaID : 0;
}

int64_t VLCLibraryDataTypesIntegrationCreateScannedShow(NSString *name)
{
    NSString * const directory = [sUserDataPath stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    NSString * const path = [directory stringByAppendingPathComponent:
        [name stringByAppendingString:@" S01E01.mp4"]];
    NSData * const data = [[NSData alloc]
        initWithBase64EncodedString:VLCLibraryDataTypesIntegrationVideoFixtureBase64()
                            options:0];
    if (![NSFileManager.defaultManager createDirectoryAtPath:directory
                                 withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:nil] ||
        ![data writeToFile:path atomically:YES] ||
        !VLCLibraryDataTypesIntegrationWaitForMediaAdded(
            [NSURL fileURLWithPath:directory].absoluteString,
            @[[NSURL fileURLWithPath:path].absoluteString])) {
        return 0;
    }
    vlc_ml_media_t * const media = vlc_ml_get_media_by_mrl(sMediaLibrary,
                                                           [NSURL fileURLWithPath:path].absoluteString.UTF8String);
    if (media == NULL) {
        return 0;
    }
    const BOOL episode = media->i_subtype == VLC_ML_MEDIA_SUBTYPE_SHOW_EPISODE;
    const int64_t mediaID = media->i_id;
    vlc_ml_media_release(media);
    if (!episode || !VLCLibraryDataTypesIntegrationSetMediaType(mediaID, VLC_ML_MEDIA_TYPE_VIDEO))
        return 0;
    vlc_ml_show_list_t * const shows = vlc_ml_list_shows(sMediaLibrary, NULL);
    if (shows == NULL)
        return 0;
    int64_t showID = 0;
    for (size_t index = 0; index < shows->i_nb_items; index++) {
        if (shows->p_items[index].psz_name != NULL &&
            strcmp(shows->p_items[index].psz_name, name.UTF8String) == 0) {
            showID = shows->p_items[index].i_id;
            break;
        }
    }
    vlc_ml_show_list_release(shows);
    return showID;
}

static BOOL VLCLibraryDataTypesIntegrationExecuteEntityStatement(const char *sql,
                                                                 int64_t libraryID,
                                                                 id value)
{
    NSArray * const parameters = value == nil ? @[ @(libraryID) ] : @[ @(libraryID), value ];
    return VLCLibraryDataTypesIntegrationExecuteStatements(@[
        VLCLibraryDataTypesIntegrationSQLStatement([NSString stringWithUTF8String:sql], parameters, 1),
    ]);
}

int64_t VLCLibraryDataTypesIntegrationCreateExternalMediaOfType(const char *mrl, vlc_ml_media_type_t type)
{
    const int64_t mediaID = VLCLibraryDataTypesIntegrationCreateExternalMedia(mrl);
    if (mediaID == 0)
        return 0;
    // Keep the native external import type: remove_stream only accepts genuine
    // external records. Main audio/video listings exclude these; history does not.
    if (!VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Media SET type = ?2, is_present = 1 WHERE id_media = ?1",
        mediaID,
        @(type))) {
        vlc_ml_remove_stream(sMediaLibrary, mediaID);
        return 0;
    }
    return mediaID;
}

BOOL VLCLibraryDataTypesIntegrationSetAlbumDuration(int64_t libraryID, int64_t duration)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Album SET duration = ?2 WHERE id_album = ?1",
        libraryID,
        @(duration));
}

BOOL VLCLibraryDataTypesIntegrationSetAlbumYear(int64_t libraryID, unsigned int year)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Album SET release_year = ?2 WHERE id_album = ?1",
        libraryID,
        @(year));
}

BOOL VLCLibraryDataTypesIntegrationRenameAlbumArtist(int64_t libraryID, NSString *name)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Artist SET name = ?2 WHERE id_artist = (SELECT artist_id FROM Album WHERE id_album = ?1)",
        libraryID,
        name) &&
        VLCLibraryDataTypesIntegrationExecuteEntityStatement(
            "UPDATE ArtistFts SET name = ?2 WHERE rowid = (SELECT artist_id FROM Album WHERE id_album = ?1)",
            libraryID,
            name) &&
        VLCLibraryDataTypesIntegrationExecuteEntityStatement(
            "UPDATE AlbumFts SET artist = ?2 WHERE rowid IN (SELECT id_album FROM Album "
            "WHERE artist_id = (SELECT artist_id FROM Album WHERE id_album = ?1))",
            libraryID,
            name);
}

BOOL VLCLibraryDataTypesIntegrationRenameAlbum(int64_t libraryID, NSString *title)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Album SET title = ?2 WHERE id_album = ?1",
        libraryID,
        title) &&
        VLCLibraryDataTypesIntegrationExecuteEntityStatement(
            "UPDATE AlbumFts SET title = ?2 WHERE rowid = ?1",
            libraryID,
            title);
}

BOOL VLCLibraryDataTypesIntegrationSetMediaInsertionDate(int64_t mediaID, NSDate *date)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Media SET insertion_date = ?2 WHERE id_media = ?1",
        mediaID,
        @((int64_t)date.timeIntervalSince1970));
}

BOOL VLCLibraryDataTypesIntegrationSetMediaPresent(int64_t libraryID, BOOL present)
{
    // Real presence triggers update the album, artist, and group counts.
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Media SET is_present = ?2 WHERE id_media = ?1",
        libraryID,
        @(present));
}

BOOL VLCLibraryDataTypesIntegrationRenameArtist(int64_t libraryID, NSString *name)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Artist SET name = ?2 WHERE id_artist = ?1",
        libraryID,
        name) &&
        VLCLibraryDataTypesIntegrationExecuteEntityStatement(
            "UPDATE ArtistFts SET name = ?2 WHERE rowid = ?1",
            libraryID,
            name) &&
        VLCLibraryDataTypesIntegrationExecuteEntityStatement(
            "UPDATE AlbumFts SET artist = ?2 WHERE rowid IN "
            "(SELECT id_album FROM Album WHERE artist_id = ?1)",
            libraryID,
            name);
}

BOOL VLCLibraryDataTypesIntegrationRenameGenre(int64_t libraryID, NSString *name)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Genre SET name = ?2 WHERE id_genre = ?1",
        libraryID,
        name) &&
        VLCLibraryDataTypesIntegrationExecuteEntityStatement(
            "UPDATE GenreFts SET name = ?2 WHERE rowid = ?1",
            libraryID,
            name);
}

BOOL VLCLibraryDataTypesIntegrationRenameMedia(int64_t libraryID, NSString *title)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Media SET title = ?2 WHERE id_media = ?1",
        libraryID,
        title) &&
        VLCLibraryDataTypesIntegrationExecuteEntityStatement(
            "UPDATE MediaFts SET title = ?2 WHERE rowid = ?1",
            libraryID,
            title);
}

BOOL VLCLibraryDataTypesIntegrationClearEpisodeSubtype(int64_t mediaID)
{
    // Retain the relationship so the model must use its cached episode->show
    // mapping after a preceding media update has cleared the subtype.
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Media SET subtype = ?2 WHERE id_media = ?1",
        mediaID,
        @(VLC_ML_MEDIA_SUBTYPE_UNKNOWN));
}

BOOL VLCLibraryDataTypesIntegrationSetShowSummary(int64_t showID, NSString *summary)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Show SET short_summary = ?2 WHERE id_show = ?1",
        showID,
        summary);
}

BOOL VLCLibraryDataTypesIntegrationSetMediaLastPlayedDate(int64_t mediaID, NSDate *date)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Media SET last_played_date = ?2 WHERE id_media = ?1",
        mediaID,
        @((int64_t)date.timeIntervalSince1970));
}

BOOL VLCLibraryDataTypesIntegrationMoveMediaToAlbum(int64_t mediaID, int64_t albumID)
{
    // The real album-id trigger transfers counts and duration between albums.
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Media SET album_id = ?2 WHERE id_media = ?1",
        mediaID,
        @(albumID));
}

BOOL VLCLibraryDataTypesIntegrationMoveMediaToArtist(int64_t mediaID, int64_t artistID)
{
    // Keep the primary artist and trigger-maintained relation counts consistent.
    return VLCLibraryDataTypesIntegrationExecuteStatements(@[
        VLCLibraryDataTypesIntegrationSQLStatement(
            @"DELETE FROM MediaArtistRelation WHERE media_id = ?1", @[ @(mediaID) ], 1),
        VLCLibraryDataTypesIntegrationSQLStatement(
            @"UPDATE Media SET artist_id = ?2 WHERE id_media = ?1", @[ @(mediaID), @(artistID) ], 1),
        VLCLibraryDataTypesIntegrationSQLStatement(
            @"INSERT INTO MediaArtistRelation (media_id, artist_id) VALUES (?1, ?2)",
            @[ @(mediaID), @(artistID) ], 1),
    ]);
}

BOOL VLCLibraryDataTypesIntegrationSetMovieSummary(int64_t mediaID, NSString *summary)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE Movie SET summary = ?2 WHERE media_id = ?1",
        mediaID,
        summary);
}

BOOL VLCLibraryDataTypesIntegrationSetGroupDuration(int64_t libraryID, int64_t duration)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE MediaGroup SET duration = ?2 WHERE id_group = ?1",
        libraryID,
        @(duration));
}

BOOL VLCLibraryDataTypesIntegrationSetGroupModificationDate(int64_t libraryID, NSDate *date)
{
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE MediaGroup SET last_modification_date = ?2 WHERE id_group = ?1",
        libraryID,
        @((int64_t)date.timeIntervalSince1970));
}

BOOL VLCLibraryDataTypesIntegrationDeleteEntity(enum vlc_ml_event_type eventType, int64_t libraryID)
{
    vlc_ml_media_list_t *media = NULL;
    switch (eventType) {
        case VLC_ML_EVENT_MEDIA_DELETED: {
            // Controlled-event tests delete persisted rows through the schema's
            // cascades, without changing the import type of in-flight native objects.
            // Native callback tests instead delete genuinely external records.
            if (!VLCLibraryDataTypesIntegrationExecuteEntityStatement(
                "DELETE FROM Media WHERE id_media = ?1",
                libraryID,
                nil))
                return NO;
            vlc_ml_media_t * const remaining = vlc_ml_get_media(sMediaLibrary, libraryID);
            const BOOL deleted = remaining == NULL;
            if (remaining != NULL)
                vlc_ml_media_release(remaining);
            return deleted;
        }
        case VLC_ML_EVENT_ARTIST_DELETED:
            media = vlc_ml_list_artist_tracks(sMediaLibrary, NULL, libraryID);
            break;
        case VLC_ML_EVENT_ALBUM_DELETED:
            media = vlc_ml_list_album_tracks(sMediaLibrary, NULL, libraryID);
            break;
        case VLC_ML_EVENT_GENRE_DELETED:
            media = vlc_ml_list_genre_tracks(sMediaLibrary, NULL, libraryID);
            break;
        case VLC_ML_EVENT_GROUP_DELETED: {
            // Ungroup its real members. The database trigger removes a group
            // after its last member has been detached.
            if (!VLCLibraryDataTypesIntegrationExecuteEntityStatement(
                "UPDATE Media SET group_id = NULL WHERE group_id = ?1",
                libraryID,
                nil))
                return NO;
            vlc_ml_group_t * const group = vlc_ml_get_group(sMediaLibrary, libraryID);
            if (group == NULL)
                return YES;
            vlc_ml_group_release(group);
            return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
                "DELETE FROM MediaGroup WHERE id_group = ?1",
                libraryID,
                nil);
        }
        default:
            return NO;
    }
    if (media == NULL || media->i_nb_items == 0) {
        if (media != NULL)
            vlc_ml_media_list_release(media);
        return NO;
    }
    NSMutableArray<NSNumber *> * const IDs = NSMutableArray.array;
    for (size_t index = 0; index < media->i_nb_items; index++)
        [IDs addObject:@(media->p_items[index].i_id)];
    vlc_ml_media_list_release(media);
    for (NSNumber *ID in IDs) {
        if (!VLCLibraryDataTypesIntegrationDeleteEntity(VLC_ML_EVENT_MEDIA_DELETED, ID.longLongValue))
            return NO;
    }
    // The schema's deletion cascades through the real artist/album/genre/group
    // relationships. Confirm the parent is truly gone, not merely uncached.
    switch (eventType) {
        case VLC_ML_EVENT_ARTIST_DELETED: {
            vlc_ml_artist_t * const item = vlc_ml_get_artist(sMediaLibrary, libraryID);
            if (item == NULL)
                return YES;
            vlc_ml_artist_release(item);
            return NO;
        }
        case VLC_ML_EVENT_ALBUM_DELETED: {
            vlc_ml_album_t * const item = vlc_ml_get_album(sMediaLibrary, libraryID);
            if (item == NULL)
                return YES;
            vlc_ml_album_release(item);
            return NO;
        }
        case VLC_ML_EVENT_GENRE_DELETED: {
            vlc_ml_genre_t * const item = vlc_ml_get_genre(sMediaLibrary, libraryID);
            if (item == NULL)
                return YES;
            vlc_ml_genre_release(item);
            return NO;
        }
        default:
            return NO;
    }
}

BOOL VLCLibraryDataTypesIntegrationRenameGroup(int64_t libraryID, NSString *name)
{
    // VLC's ML bridge has no group-rename API. Seed both the stored name and
    // its full-text search entry so real filtered queries see the new name.
    return VLCLibraryDataTypesIntegrationExecuteEntityStatement(
        "UPDATE MediaGroup SET name = ?2 WHERE id_group = ?1",
        libraryID,
        name) &&
        VLCLibraryDataTypesIntegrationExecuteEntityStatement(
            "UPDATE MediaGroupFts SET name = ?2 WHERE rowid = ?1",
            libraryID,
            name);
}

int64_t VLCLibraryDataTypesIntegrationCreateNamedGroup(int64_t mediaID, NSString *name)
{
    // Seed a user-created group rather than renaming an automatically managed
    // scanner group. The real database triggers index it and maintain counts
    // as its real persisted media is reassigned.
    if (!VLCLibraryDataTypesIntegrationExecuteEntityStatement(
            "INSERT INTO MediaGroup(name, creation_date, last_modification_date, "
            "user_interacted, forced_singleton) VALUES(?2, strftime('%s'), strftime('%s'), 1, 0)",
            mediaID,
            name) ||
        !VLCLibraryDataTypesIntegrationExecuteEntityStatement(
            "UPDATE Media SET group_id = (SELECT id_group FROM MediaGroup WHERE name = ?2) "
            "WHERE id_media = ?1",
            mediaID,
            name))
        return 0;
    vlc_ml_group_list_t * const groups = vlc_ml_list_groups(sMediaLibrary, NULL);
    if (groups == NULL)
        return 0;
    int64_t groupID = 0;
    for (size_t index = 0; index < groups->i_nb_items; index++) {
        if (groups->p_items[index].psz_name != NULL &&
            strcmp(groups->p_items[index].psz_name, name.UTF8String) == 0) {
            groupID = groups->p_items[index].i_id;
            break;
        }
    }
    vlc_ml_group_list_release(groups);
    return groupID;
}
