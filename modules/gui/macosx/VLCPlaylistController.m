/*****************************************************************************
 * VLCPlaylistController.m: MacOS X interface module
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

#import "VLCPlaylistController.h"
#import "VLCPlaylistModel.h"
#import "VLCPlaylistDataSource.h"
#import "VLCMain.h"
#import <vlc_interface.h>

@interface VLCPlaylistController ()
{
    vlc_playlist_t *_p_playlist;
    vlc_playlist_listener_id *_playlistListenerID;
}

- (void)playlistResetWithItems:(vlc_playlist_item_t *const *)items count:(size_t)numberOfItems;
- (void)playlistAdded:(vlc_playlist_item_t *const *)items atIndex:(size_t)insertionIndex count:(size_t)numberOfItems;
- (void)playlistRemovedItemsAtIndex:(size_t)index count:(size_t)numberOfItems;
- (void)currentPlaylistItemChanged:(ssize_t)index;
- (void)playlistUpdatedForIndex:(size_t)firstUpdatedIndex items:(vlc_playlist_item_t *const *)items count:(size_t)numberOfItems;

@end

#pragma mark -
#pragma mark core callbacks

static void
cb_playlist_items_reset(vlc_playlist_t *playlist,
                        vlc_playlist_item_t *const items[],
                        size_t numberOfItems,
                        void *p_data)
{
    VLCPlaylistController *playlistController = (__bridge VLCPlaylistController *)p_data;
    [playlistController playlistResetWithItems:items count:numberOfItems];
}

static void
cb_playlist_items_added(vlc_playlist_t *playlist,
                        size_t insertionIndex,
                        vlc_playlist_item_t *const items[],
                        size_t numberOfAddedItems,
                        void *p_data)
{
    VLCPlaylistController *playlistController = (__bridge VLCPlaylistController *)p_data;
    [playlistController playlistAdded:items atIndex:insertionIndex count:numberOfAddedItems];
}

static void
cb_playlist_items_removed(vlc_playlist_t *playlist,
                          size_t index,
                          size_t count,
                          void *p_data)
{
    VLCPlaylistController *playlistController = (__bridge VLCPlaylistController *)p_data;
    [playlistController playlistRemovedItemsAtIndex:index count:count];
}

static void
cb_playlist_items_updated(vlc_playlist_t *playlist,
                          size_t firstUpdatedIndex,
                          vlc_playlist_item_t *const items[],
                          size_t numberOfUpdatedItems,
                          void *p_data)
{
    VLCPlaylistController *playlistController = (__bridge VLCPlaylistController *)p_data;
    [playlistController playlistUpdatedForIndex:firstUpdatedIndex items:items count:numberOfUpdatedItems];
}

static void
cb_playlist_playback_repeat_changed(vlc_playlist_t *playlist,
                                    enum vlc_playlist_playback_repeat repeat,
                                    void *userdata)
{
    NSLog(@"%s: repeat mode: %u", __func__, repeat);
}

static void
cb_playlist_playback_order_changed(vlc_playlist_t *playlist,
                                   enum vlc_playlist_playback_order order,
                                   void *userdata)
{
    NSLog(@"%s: playback order: %u", __func__, order);
}

static void
cb_playlist_current_item_changed(vlc_playlist_t *playlist,
                                 ssize_t index,
                                 void *p_data)
{
    VLCPlaylistController *playlistController = (__bridge VLCPlaylistController *)p_data;
    [playlistController currentPlaylistItemChanged:index];
}

static const struct vlc_playlist_callbacks playlist_callbacks = {
    cb_playlist_items_reset,
    cb_playlist_items_added,
    NULL,
    cb_playlist_items_removed,
    cb_playlist_items_updated,
    cb_playlist_playback_repeat_changed,
    cb_playlist_playback_order_changed,
    cb_playlist_current_item_changed,
    NULL,
    NULL,
};

#pragma mark -
#pragma mark class initialization

@implementation VLCPlaylistController

- (instancetype)init
{
    self = [super init];
    if (self) {
        intf_thread_t *p_intf = getIntf();
        _p_playlist = vlc_intf_GetMainPlaylist( p_intf );
        _playlistListenerID = vlc_playlist_AddListener(_p_playlist,
                                                       &playlist_callbacks,
                                                       (__bridge void *)self,
                                                       YES);
        _playlistModel = [[VLCPlaylistModel alloc] init];
        _playlistModel.playlistController = self;
    }
    return self;
}

- (void)dealloc
{
    if (_p_playlist) {
        if (_playlistListenerID) {
            vlc_playlist_RemoveListener(_p_playlist, _playlistListenerID);
        }
        vlc_playlist_Delete(_p_playlist);
    }
}

#pragma mark - callback forwarders

- (void)playlistResetWithItems:(vlc_playlist_item_t *const *)items count:(size_t)numberOfItems
{
    for (size_t i = 0; i < numberOfItems; i++) {
        [_playlistModel addItem:items[i]];
    }

    [_playlistDataSource performSelectorOnMainThread:@selector(playlistUpdated) withObject:nil waitUntilDone:NO];
}

- (void)playlistAdded:(vlc_playlist_item_t *const *)items atIndex:(size_t)insertionIndex count:(size_t)numberOfItems
{
    for (size_t i = 0; i < numberOfItems; i++) {
        [_playlistModel addItem:items[i] atIndex:insertionIndex];
        insertionIndex++;
    }

    [_playlistDataSource performSelectorOnMainThread:@selector(playlistUpdated) withObject:nil waitUntilDone:NO];
}

- (void)playlistRemovedItemsAtIndex:(size_t)index count:(size_t)numberOfItems
{
    NSRange range = NSMakeRange(index, numberOfItems);
    [_playlistModel removeItemsInRange:range];

    [_playlistDataSource performSelectorOnMainThread:@selector(playlistUpdated) withObject:nil waitUntilDone:NO];
}

- (void)currentPlaylistItemChanged:(ssize_t)index
{
    _currentPlaylistIndex = index;
    [_playlistDataSource performSelectorOnMainThread:@selector(playlistUpdated) withObject:nil waitUntilDone:NO];
}

- (void)playlistUpdatedForIndex:(size_t)firstUpdatedIndex items:(vlc_playlist_item_t *const *)items count:(size_t)numberOfItems
{
    VLC_UNUSED(items);
    for (size_t i = firstUpdatedIndex; i < firstUpdatedIndex + numberOfItems; i++) {
        [_playlistModel updateItemAtIndex:i];
    }
    [_playlistDataSource performSelectorOnMainThread:@selector(playlistUpdated) withObject:nil waitUntilDone:NO];
}

#pragma mark - controller functions for use within the UI

- (void)addPlaylistItems:(NSArray*)array
{
    BOOL b_autoplay = var_InheritBool(getIntf(), "macosx-autoplay");
    [self addPlaylistItems:array atPosition:-1 startPlayback:b_autoplay];
}

- (void)addPlaylistItems:(NSArray*)itemArray
              atPosition:(size_t)insertionIndex
           startPlayback:(BOOL)startPlayback;
{
    /* note: we don't add the item as cached data to the model here
     * because this will be done asynchronously through the callback */

    intf_thread_t *p_intf = getIntf();
    NSUInteger numberOfItems = [itemArray count];

    for (NSUInteger i = 0; i < numberOfItems; i++) {
        NSDictionary *itemMetadata = itemArray[i];
        input_item_t *p_input = [self createInputItemBasedOnMetadata:itemMetadata];
        NSString *itemURLString = itemMetadata[@"ITEM_URL"];

        if (!p_input) {
            if (itemURLString) {
                msg_Warn(p_intf, "failed to create input for %s", [itemURLString UTF8String]);
            } else {
                msg_Warn(p_intf, "failed to create input because no URL was provided");
            }
            continue;
        }

        vlc_playlist_Lock(_p_playlist);

        int ret = 0;
        size_t actualInsertionIndex = insertionIndex;
        if (insertionIndex == -1) {
            actualInsertionIndex = vlc_playlist_Count(_p_playlist);
        }
        ret = vlc_playlist_Insert(_p_playlist,
                                      actualInsertionIndex,
                                      &p_input,
                                      1);

        if (ret != VLC_SUCCESS) {
            msg_Err(p_intf, "failed to insert input item at insertion index: %zu", insertionIndex);
        } else {
            msg_Dbg(p_intf, "Added item %s at insertion index: %zu", [itemURLString UTF8String], insertionIndex);
        }

        if (i == 0 && startPlayback) {
            vlc_playlist_PlayAt(_p_playlist, actualInsertionIndex);
        }

        vlc_playlist_Unlock(_p_playlist);
        input_item_Release(p_input);

        if (insertionIndex != -1) {
            insertionIndex++;
        }
    }
}

- (void)removeItemAtIndex:(size_t)index
{
    /* note: we don't remove the cached data from the model here
     * because this will be done asynchronously through the callback */

    vlc_playlist_Lock(_p_playlist);
    vlc_playlist_Remove(_p_playlist, index, 1);
    vlc_playlist_Unlock(_p_playlist);
}

- (void)clearPlaylist
{
    vlc_playlist_Lock(_p_playlist);
    vlc_playlist_Clear(_p_playlist);
    vlc_playlist_Unlock(_p_playlist);
}

- (int)startPlaylist
{
    vlc_playlist_Lock(_p_playlist);
    int ret = vlc_playlist_Start(_p_playlist);
    vlc_playlist_Unlock(_p_playlist);
    return ret;
}

- (int)playPreviousItem
{
    vlc_playlist_Lock(_p_playlist);
    int ret = vlc_playlist_Prev(_p_playlist);
    vlc_playlist_Unlock(_p_playlist);
    return ret;
}

- (int)playItemAtIndex:(size_t)index
{
    vlc_playlist_Lock(_p_playlist);
    size_t playlistLength = vlc_playlist_Count(_p_playlist);
    int ret = 0;
    if (index >= playlistLength) {
        ret = VLC_EGENERIC;
    } else {
        ret = vlc_playlist_PlayAt(_p_playlist, index);
    }
    vlc_playlist_Unlock(_p_playlist);
    return ret;
}

- (int)playNextItem
{
    vlc_playlist_Lock(_p_playlist);
    int ret = vlc_playlist_Next(_p_playlist);
    vlc_playlist_Unlock(_p_playlist);
    return ret;
}

- (void)stopPlayback
{
    vlc_playlist_Lock(_p_playlist);
    vlc_playlist_Stop(_p_playlist);
    vlc_playlist_Unlock(_p_playlist);
}

- (void)pausePlayback
{
    vlc_playlist_Lock(_p_playlist);
    vlc_playlist_Pause(_p_playlist);
    vlc_playlist_Unlock(_p_playlist);
}

- (void)resumePlayback
{
    vlc_playlist_Lock(_p_playlist);
    vlc_playlist_Resume(_p_playlist);
    vlc_playlist_Unlock(_p_playlist);
}

#pragma mark - helper methods

- (input_item_t *)createInputItemBasedOnMetadata:(NSDictionary *)itemToCreateDict
{
    intf_thread_t *p_intf = getIntf();

    input_item_t *p_input;
    BOOL b_rem = FALSE, b_dir = FALSE, b_writable = FALSE;
    NSString *uri, *name, *path;
    NSURL * url;
    NSArray *optionsArray;

    /* Get the item */
    uri = (NSString *)[itemToCreateDict objectForKey: @"ITEM_URL"];
    url = [NSURL URLWithString: uri];
    path = [url path];
    name = (NSString *)[itemToCreateDict objectForKey: @"ITEM_NAME"];
    optionsArray = (NSArray *)[itemToCreateDict objectForKey: @"ITEM_OPTIONS"];

    if ([[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&b_dir]
        && b_dir &&
        [[NSWorkspace sharedWorkspace] getFileSystemInfoForPath:path
                                                    isRemovable:&b_rem
                                                     isWritable:&b_writable
                                                  isUnmountable:NULL
                                                    description:NULL
                                                           type:NULL]
        && b_rem && !b_writable && [url isFileURL]) {

        NSString *diskType = getVolumeTypeFromMountPath(path);
        msg_Dbg(p_intf, "detected optical media of type %s in the file input", [diskType UTF8String]);

        if ([diskType isEqualToString: kVLCMediaDVD])
            uri = [NSString stringWithFormat: @"dvdnav://%@", getBSDNodeFromMountPath(path)];
        else if ([diskType isEqualToString: kVLCMediaVideoTSFolder])
            uri = [NSString stringWithFormat: @"dvdnav://%@", path];
        else if ([diskType isEqualToString: kVLCMediaAudioCD])
            uri = [NSString stringWithFormat: @"cdda://%@", getBSDNodeFromMountPath(path)];
        else if ([diskType isEqualToString: kVLCMediaVCD])
            uri = [NSString stringWithFormat: @"vcd://%@#0:0", getBSDNodeFromMountPath(path)];
        else if ([diskType isEqualToString: kVLCMediaSVCD])
            uri = [NSString stringWithFormat: @"vcd://%@@0:0", getBSDNodeFromMountPath(path)];
        else if ([diskType isEqualToString: kVLCMediaBD] || [diskType isEqualToString: kVLCMediaBDMVFolder])
            uri = [NSString stringWithFormat: @"bluray://%@", path];
        else
            msg_Warn(getIntf(), "unknown disk type, treating %s as regular input", [path UTF8String]);

        p_input = input_item_New([uri UTF8String], [[[NSFileManager defaultManager] displayNameAtPath:path] UTF8String]);
    } else {
        p_input = input_item_New([uri fileSystemRepresentation], name ? [name UTF8String] : NULL);
    }

    if (!p_input)
        return NULL;

    if (optionsArray) {
        NSUInteger count = [optionsArray count];
        for (NSUInteger i = 0; i < count; i++)
            input_item_AddOption(p_input, [[optionsArray objectAtIndex:i] UTF8String], VLC_INPUT_OPTION_TRUSTED);
    }

    /* Recent documents menu */
    if (url != nil && var_InheritBool(getIntf(), "macosx-recentitems"))
        [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];

    return p_input;
}

@end
