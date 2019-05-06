/*****************************************************************************
 * VLCLibraryController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCLibraryController.h"

#import "main/VLCMain.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"

#import <vlc_media_library.h>

@interface VLCLibraryController()
{
    vlc_medialibrary_t *_p_libraryInstance;
}
@end

@implementation VLCLibraryController

- (instancetype)init
{
    self = [super init];
    if (self) {
        _p_libraryInstance = vlc_ml_instance_get(getIntf());
        _libraryModel = [[VLCLibraryModel alloc] initWithLibrary:_p_libraryInstance];

        NSNotificationCenter *defaultNotificationCenter = [NSNotificationCenter defaultCenter];
        [defaultNotificationCenter addObserver:self
                                      selector:@selector(applicationWillEnterBackground:)
                                          name:NSApplicationWillResignActiveNotification
                                        object:nil];
        [defaultNotificationCenter addObserver:self
                                      selector:@selector(applicationWillBecomeActive:)
                                          name:NSApplicationWillBecomeActiveNotification
                                        object:nil];
        [defaultNotificationCenter addObserver:self
                                      selector:@selector(playbackStateChanged:)
                                          name:VLCPlayerStateChanged
                                        object:nil];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self lazyLoad];
        });
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    _p_libraryInstance = NULL;
}

- (void)lazyLoad
{
    [self applicationWillEnterBackground:nil];
}

- (void)applicationWillEnterBackground:(NSNotification *)aNotification
{
    vlc_ml_resume_background(_p_libraryInstance);
}

- (void)applicationWillBecomeActive:(NSNotification *)aNotification
{
    vlc_ml_pause_background(_p_libraryInstance);
}

- (void)playbackStateChanged:(NSNotification *)aNotification
{
    VLCPlayerController *playerController = aNotification.object;
    if (playerController.playerState == VLC_PLAYER_STATE_PLAYING) {
        vlc_ml_pause_background(_p_libraryInstance);
    } else {
        vlc_ml_resume_background(_p_libraryInstance);
    }
}

- (int)appendItemToPlaylist:(VLCMediaLibraryMediaItem *)mediaItem playImmediately:(BOOL)playImmediately
{
    input_item_t *p_inputItem = vlc_ml_get_input_item(_p_libraryInstance, mediaItem.libraryID);
    int ret = [[[VLCMain sharedInstance] playlistController] addInputItem:p_inputItem atPosition:-1 startPlayback:playImmediately];
    input_item_Release(p_inputItem);
    if (ret == VLC_SUCCESS) {
        [mediaItem increasePlayCount];
    }
    return ret;
}

- (void)showItemInFinder:(VLCMediaLibraryMediaItem *)mediaItem;
{
    if (mediaItem == nil) {
        return;
    }
    VLCMediaLibraryFile *firstFile = mediaItem.files.firstObject;

    if (firstFile) {
        NSURL *URL = [NSURL URLWithString:firstFile.MRL];
        if (URL) {
            [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[URL]];
        }
    }
}

- (int)attemptToGenerateThumbnailForMediaItem:(VLCMediaLibraryMediaItem *)mediaItem
{
    return vlc_ml_media_generate_thumbnail(_p_libraryInstance, mediaItem.libraryID);
}

#pragma mark - folder management

- (int)addFolderWithFileURL:(NSURL *)fileURL
{
    return vlc_ml_add_folder(_p_libraryInstance, [[fileURL absoluteString] UTF8String]);
}

- (int)banFolderWithFileURL:(NSURL *)fileURL
{
    return vlc_ml_ban_folder(_p_libraryInstance, [[fileURL absoluteString] UTF8String]);
}

- (int)unbanFolderWithFileURL:(NSURL *)fileURL
{
    return vlc_ml_unban_folder(_p_libraryInstance, [[fileURL absoluteString] UTF8String]);
}

- (int)removeFolderWithFileURL:(NSURL *)fileURL
{
    return vlc_ml_remove_folder(_p_libraryInstance, [[fileURL absoluteString] UTF8String]);
}

- (int)clearHistory
{
    return vlc_ml_clear_history(_p_libraryInstance);
}

@end
