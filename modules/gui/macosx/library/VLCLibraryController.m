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
        if (!_p_libraryInstance) {
            msg_Info(getIntf(), "VLC runs without media library support");
            return self;
        }
        _libraryModel = [[VLCLibraryModel alloc] initWithLibrary:_p_libraryInstance];
        _unsorted = YES;

        NSNotificationCenter *defaultNotificationCenter = [NSNotificationCenter defaultCenter];
        [defaultNotificationCenter addObserver:self
                                      selector:@selector(playbackStateChanged:)
                                          name:VLCPlayerStateChanged
                                        object:nil];
    }
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    _p_libraryInstance = NULL;
}

- (void)playbackStateChanged:(NSNotification *)aNotification
{
    if (!_p_libraryInstance) {
        return;
    }
    VLCPlayerController *playerController = aNotification.object;
    if (playerController.playerState == VLC_PLAYER_STATE_PLAYING) {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            vlc_ml_pause_background(self->_p_libraryInstance);
        });
    } else {
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
            vlc_ml_resume_background(self->_p_libraryInstance);
        });
    }
}

- (int)appendItemToPlaylist:(VLCMediaLibraryMediaItem *)mediaItem playImmediately:(BOOL)playImmediately
{
    if (!_p_libraryInstance) {
        return VLC_ENOOBJ;
    }
    input_item_t *p_inputItem = vlc_ml_get_input_item(_p_libraryInstance, mediaItem.libraryID);
    if (!p_inputItem) {
        msg_Err(getIntf(), "No input item found for media id %lli", mediaItem.libraryID);
        return VLC_ENOITEM;
    }
    int ret = [[[VLCMain sharedInstance] playlistController] addInputItem:p_inputItem atPosition:-1 startPlayback:playImmediately];
    input_item_Release(p_inputItem);
    if (ret == VLC_SUCCESS) {
        [mediaItem increasePlayCount];
    }
    return ret;
}

- (int)appendItemsToPlaylist:(NSArray <VLCMediaLibraryMediaItem *> *)mediaItemArray playFirstItemImmediately:(BOOL)playFirstItemImmediately
{
    if (!_p_libraryInstance) {
        return VLC_ENOOBJ;
    }

    NSUInteger itemCount = [mediaItemArray count];
    int ret = VLC_SUCCESS;
    for (NSUInteger x = 0; x < itemCount; x++) {
        if (unlikely(x == 0 && playFirstItemImmediately)) {
            ret = [self appendItemToPlaylist:mediaItemArray[x] playImmediately:YES];
        } else {
            ret = [self appendItemToPlaylist:mediaItemArray[x] playImmediately:NO];
        }
        if (unlikely(ret != VLC_SUCCESS)) {
            break;
        }
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

#pragma mark - folder management

- (int)addFolderWithFileURL:(NSURL *)fileURL
{
    if (!_p_libraryInstance) {
        return VLC_ENOOBJ;
    }
    if (!fileURL) {
        return VLC_EBADVAR;
    }
    return vlc_ml_add_folder(_p_libraryInstance, [[fileURL absoluteString] UTF8String]);
}

- (int)banFolderWithFileURL:(NSURL *)fileURL
{
    if (!_p_libraryInstance) {
        return VLC_ENOOBJ;
    }
    if (!fileURL) {
        return VLC_EBADVAR;
    }
    return vlc_ml_ban_folder(_p_libraryInstance, [[fileURL absoluteString] UTF8String]);
}

- (int)unbanFolderWithFileURL:(NSURL *)fileURL
{
    if (!_p_libraryInstance) {
        return VLC_ENOOBJ;
    }
    if (!fileURL) {
        return VLC_EBADVAR;
    }
    return vlc_ml_unban_folder(_p_libraryInstance, [[fileURL absoluteString] UTF8String]);
}

- (int)removeFolderWithFileURL:(NSURL *)fileURL
{
    if (!_p_libraryInstance) {
        return VLC_ENOOBJ;
    }
    if (!fileURL) {
        return VLC_EBADVAR;
    }
    return vlc_ml_remove_folder(_p_libraryInstance, [[fileURL absoluteString] UTF8String]);
}

- (int)clearHistory
{
    if (!_p_libraryInstance) {
        return VLC_ENOOBJ;
    }
    return vlc_ml_clear_history(_p_libraryInstance);
}

- (void)sortByCriteria:(enum vlc_ml_sorting_criteria_t)sortCriteria andDescending:(bool)descending
{
    _unsorted = NO;
    _lastSortingCriteria = sortCriteria;
    _descendingLibrarySorting = descending;
    [_libraryModel sortByCriteria:sortCriteria andDescending:descending];
}

@end
