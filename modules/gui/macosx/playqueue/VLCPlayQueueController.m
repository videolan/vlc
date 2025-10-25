/*****************************************************************************
 * VLCPlayQueueController.m: MacOS X interface module
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

#import "VLCPlayQueueController.h"

#import <vlc_interface.h>
#import <vlc_player.h>
#import <vlc_media_library.h>

#import "extensions/NSString+Helpers.h"
#import "main/VLCMain.h"
#import "playqueue/VLCPlayQueueModel.h"
#import "playqueue/VLCPlayQueueItem.h"
#import "playqueue/VLCPlayQueueDataSource.h"
#import "playqueue/VLCPlayerController.h"
#import "windows/VLCOpenInputMetadata.h"
#import "library/VLCInputItem.h"

NSString * const VLCPlaybackOrderChanged = @"VLCPlaybackOrderChanged";
NSString * const VLCPlaybackRepeatChanged = @"VLCPlaybackRepeatChanged";
NSString * const VLCPlaybackHasPreviousChanged = @"VLCPlaybackHasPreviousChanged";
NSString * const VLCPlaybackHasNextChanged = @"VLCPlaybackHasNextChanged";
NSString * const VLCPlayQueueCurrentItemIndexChanged = @"VLCPlayQueueCurrentItemIndexChanged";
NSString * const VLCPlayQueueItemsAdded = @"VLCPlayQueueItemsAdded";
NSString * const VLCPlayQueueItemsRemoved = @"VLCPlayQueueItemsRemoved";

NSString * const VLCLibraryPlayQueueModeDefaultsKey = @"VLCLibraryPlayQueueMode";

@interface VLCPlayQueueController ()
{
    NSNotificationCenter *_defaultNotificationCenter;

    vlc_playlist_t *_p_playlist;
    vlc_playlist_listener_id *_playlistListenerID;
}

- (void)playQueueResetWithItems:(NSArray *)items;
- (void)playQueueAdded:(NSArray *)items atIndex:(size_t)insertionIndex count:(size_t)numberOfItems;
- (void)playQueueMovedIndex:(size_t)index toTarget:(size_t)target numberOfItems:(size_t)count;
- (void)playQueueRemovedItemsAtIndex:(size_t)index count:(size_t)numberOfItems;
- (void)playQueueUpdatedForIndex:(size_t)firstUpdatedIndex items:(vlc_playlist_item_t *const *)items count:(size_t)numberOfItems;
- (void)playQueuePlaybackRepeatUpdated:(enum vlc_playlist_playback_repeat)currentRepeatMode;
- (void)playQueuePlaybackOrderUpdated:(enum vlc_playlist_playback_order)currentOrder;
- (void)currentPlayQueueItemIndexChanged:(size_t)index;
- (void)playQueueHasPreviousItem:(BOOL)hasPrevious;
- (void)playQueueHasNextItem:(BOOL)hasNext;
- (void)stopActionChanged:(enum vlc_playlist_media_stopped_action)stoppedAction;

@end

#pragma mark -
#pragma mark core callbacks

static void
cb_playlist_items_reset(vlc_playlist_t *const __unused playlist,
                        vlc_playlist_item_t *const items[],
                        size_t numberOfItems,
                        void *p_data)
{
    NSMutableArray * const array = [NSMutableArray arrayWithCapacity:numberOfItems];
    for (size_t i = 0; i < numberOfItems; i++) {
        VLCPlayQueueItem * const item = [[VLCPlayQueueItem alloc] initWithPlaylistItem:items[i]];
        [array addObject:item];
    }
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController playQueueResetWithItems:array];
    });
}

static void
cb_playlist_items_added(vlc_playlist_t *const __unused playlist,
                        size_t insertionIndex,
                        vlc_playlist_item_t *const items[],
                        size_t numberOfAddedItems,
                        void *p_data)
{
    NSMutableArray * const array = [NSMutableArray arrayWithCapacity:numberOfAddedItems];
    for (size_t i = 0; i < numberOfAddedItems; i++) {
        VLCPlayQueueItem * const item = [[VLCPlayQueueItem alloc] initWithPlaylistItem:items[i]];
        [array addObject:item];
    }
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController playQueueAdded:array atIndex:insertionIndex count:numberOfAddedItems];
    });
}

static void
cb_playlist_items_moved(vlc_playlist_t *const __unused playlist,
                        size_t index,
                        size_t numberOfMovedItems,
                        size_t target,
                        void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController playQueueMovedIndex:index toTarget:target numberOfItems:numberOfMovedItems];
    });
}

static void
cb_playlist_items_removed(vlc_playlist_t *const __unused playlist,
                          size_t index,
                          size_t count,
                          void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController playQueueRemovedItemsAtIndex:index count:count];
    });
}

static void
cb_playlist_items_updated(vlc_playlist_t *const __unused playlist,
                          size_t firstUpdatedIndex,
                          vlc_playlist_item_t *const items[],
                          size_t numberOfUpdatedItems,
                          void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController playQueueUpdatedForIndex:firstUpdatedIndex items:items count:numberOfUpdatedItems];
    });
}

static void
cb_playlist_playback_repeat_changed(vlc_playlist_t *const __unused playlist,
                                    enum vlc_playlist_playback_repeat repeat,
                                    void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController playQueuePlaybackRepeatUpdated:repeat];
    });
}

static void
cb_playlist_playback_order_changed(vlc_playlist_t *const __unused playlist,
                                   enum vlc_playlist_playback_order order,
                                   void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController playQueuePlaybackOrderUpdated:order];
    });
}

static void
cb_playlist_current_item_index_changed(vlc_playlist_t *const __unused playlist,
                                 ssize_t index,
                                 void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController currentPlayQueueItemIndexChanged:index];
    });
}

static void
cb_playlist_has_prev_changed(vlc_playlist_t *const __unused playlist,
                             bool has_prev,
                             void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController playQueueHasPreviousItem:has_prev];
    });
}

static void
cb_playlist_has_next_changed(vlc_playlist_t *const __unused playlist,
                             bool has_next,
                             void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController playQueueHasNextItem:has_next];
    });
}

static void
cb_playlist_media_stopped_action_changed(vlc_playlist_t *const __unused p_playlist,
                                         enum vlc_playlist_media_stopped_action newAction,
                                         void *p_data)
{
    VLC_UNUSED(p_playlist);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCPlayQueueController * const playQueueController = (__bridge VLCPlayQueueController *)p_data;
        [playQueueController stopActionChanged:newAction];
    });
}

static const struct vlc_playlist_callbacks playlist_callbacks = {
    cb_playlist_items_reset,
    cb_playlist_items_added,
    cb_playlist_items_moved,
    cb_playlist_items_removed,
    cb_playlist_items_updated,
    cb_playlist_playback_repeat_changed,
    cb_playlist_playback_order_changed,
    cb_playlist_current_item_index_changed,
    cb_playlist_has_prev_changed,
    cb_playlist_has_next_changed,
    cb_playlist_media_stopped_action_changed,
};

#pragma mark -
#pragma mark class initialization

@implementation VLCPlayQueueController

- (instancetype)initWithPlaylist:(vlc_playlist_t *)playlist
{
    self = [super init];
    if (self) {
        _defaultNotificationCenter = NSNotificationCenter.defaultCenter;
        [_defaultNotificationCenter addObserver:self
                                       selector:@selector(applicationWillTerminate:)
                                           name:NSApplicationWillTerminateNotification
                                         object:nil];
        [_defaultNotificationCenter addObserver:self
                                       selector:@selector(applicationDidFinishLaunching:)
                                           name:NSApplicationDidFinishLaunchingNotification
                                         object:nil];
        _p_playlist = playlist;

        /* set initial values, further updates through callbacks */
        vlc_playlist_Lock(_p_playlist);
        _unsorted = YES;
        _libraryPlayQueueMode =
            [NSUserDefaults.standardUserDefaults boolForKey:VLCLibraryPlayQueueModeDefaultsKey];
        _playbackOrder = vlc_playlist_GetPlaybackOrder(_p_playlist);
        _playbackRepeat = vlc_playlist_GetPlaybackRepeat(_p_playlist);
        _playlistListenerID = vlc_playlist_AddListener(_p_playlist,
                                                       &playlist_callbacks,
                                                       (__bridge void *)self,
                                                       YES);
        vlc_playlist_Unlock(_p_playlist);
        _playQueueModel = [[VLCPlayQueueModel alloc] init];
        _playQueueModel.playQueueController = self;
        _playerController = [[VLCPlayerController alloc] initWithPlayer:vlc_playlist_GetPlayer(_p_playlist)];
    }
    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    /* Handle sleep notification */
    [NSWorkspace.sharedWorkspace.notificationCenter addObserver:self
                                                           selector:@selector(computerWillSleep:)
                                                               name:NSWorkspaceWillSleepNotification
                                                             object:nil];

    // respect playlist-autostart
    if (var_GetBool(getIntf(), "playlist-autostart")) {
        if ([self.playQueueModel numberOfPlayQueueItems] > 0) {
            [self startPlayQueue];
        }
    }
}

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    if (_p_playlist) {
        if (_playlistListenerID) {
            vlc_playlist_Lock(_p_playlist);
            vlc_playlist_RemoveListener(_p_playlist, _playlistListenerID);
            vlc_playlist_Unlock(_p_playlist);
        }
    }
}

- (void)computerWillSleep:(NSNotification *)notification
{
    [self pausePlayback];
}

- (void)dealloc
{
    [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:self];
    [_defaultNotificationCenter removeObserver:self];
}

#pragma mark - callback forwarders

- (void)playQueueResetWithItems:(NSArray *)items
{
    // Clear all items (reset)
    [_playQueueModel dropExistingData];

    [_playQueueModel addItems:items];

    [_playQueueDataSource playQueueUpdated];
}

- (void)playQueueAdded:(NSArray *)items atIndex:(size_t)insertionIndex count:(size_t)numberOfItems
{
    [_playQueueModel addItems:items atIndex:insertionIndex count:numberOfItems];

    [_playQueueDataSource playQueueUpdated];
    [_defaultNotificationCenter postNotificationName:VLCPlayQueueItemsAdded object:self];
}

- (void)playQueueMovedIndex:(size_t)index toTarget:(size_t)target numberOfItems:(size_t)count
{
    [_playQueueModel moveItemAtIndex:index toTarget:target];
    [_playQueueDataSource playQueueUpdated];
}

- (void)playQueueRemovedItemsAtIndex:(size_t)index count:(size_t)numberOfItems
{
    NSRange range = NSMakeRange(index, numberOfItems);
    [_playQueueModel removeItemsInRange:range];

    [_playQueueDataSource playQueueUpdated];
    [_defaultNotificationCenter postNotificationName:VLCPlayQueueItemsRemoved object:self];
}

- (void)playQueueUpdatedForIndex:(size_t)firstUpdatedIndex items:(vlc_playlist_item_t *const *)items count:(size_t)numberOfItems
{
    vlc_playlist_Lock(_p_playlist);
    for (size_t i = 0; i < numberOfItems; i++) {
        size_t replaceIndex = firstUpdatedIndex + i;
        if (replaceIndex < vlc_playlist_Count(_p_playlist)) {
            vlc_playlist_item_t *currentItem = vlc_playlist_Get(_p_playlist, replaceIndex);
            VLCPlayQueueItem *newItem = [[VLCPlayQueueItem alloc] initWithPlaylistItem:currentItem];
            [_playQueueModel replaceItemAtIndex:replaceIndex withItem:newItem];
        }
    }
    vlc_playlist_Unlock(_p_playlist);

    [_playQueueDataSource playQueueUpdated];

}

- (void)playQueuePlaybackRepeatUpdated:(enum vlc_playlist_playback_repeat)currentRepeatMode
{
    _playbackRepeat = currentRepeatMode;
    [_defaultNotificationCenter postNotificationName:VLCPlaybackRepeatChanged object:self];
}

- (void)playQueuePlaybackOrderUpdated:(enum vlc_playlist_playback_order)currentOrder
{
    _playbackOrder = currentOrder;
    [_defaultNotificationCenter postNotificationName:VLCPlaybackOrderChanged object:self];
}

- (void)currentPlayQueueItemIndexChanged:(size_t)index
{
    _currentPlayQueueIndex = index;
    [_playQueueDataSource scrollToCurrentPlayQueueItem];
    [_defaultNotificationCenter postNotificationName:VLCPlayQueueCurrentItemIndexChanged object:self];
}

- (void)playQueueHasPreviousItem:(BOOL)hasPrevious
{
    _hasPreviousPlayQueueItem = hasPrevious;
    [_defaultNotificationCenter postNotificationName:VLCPlaybackHasPreviousChanged object:self];
}

- (void)playQueueHasNextItem:(BOOL)hasNext
{
    _hasNextPlayQueueItem = hasNext;
    [_defaultNotificationCenter postNotificationName:VLCPlaybackHasNextChanged object:self];
}

#pragma mark - controller functions for use within the UI

- (void)addPlayQueueItems:(NSArray <VLCOpenInputMetadata *> *)array
{
    BOOL b_autoplay = var_InheritBool(getIntf(), "macosx-autoplay");
    [self addPlayQueueItems:array atPosition:NSUIntegerMax startPlayback:b_autoplay];
}

- (void)addPlayQueueItems:(NSArray <VLCOpenInputMetadata *> *)itemArray
              atPosition:(size_t)insertionIndex
           startPlayback:(BOOL)startPlayback
{
    /* note: we don't add the item as cached data to the model here
     * because this will be done asynchronously through the callback */

    intf_thread_t *p_intf = getIntf();
    NSUInteger numberOfItems = [itemArray count];

    for (NSUInteger i = 0; i < numberOfItems; i++) {
        VLCOpenInputMetadata *itemMetadata = itemArray[i];
        input_item_t *p_input = [self createInputItemBasedOnMetadata:itemMetadata];
        NSString *itemURLString = itemMetadata.MRLString;

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
        if (insertionIndex == NSUIntegerMax) {
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

        if (insertionIndex != NSUIntegerMax) {
            insertionIndex++;
        }
    }
}

- (int)addInputItem:(input_item_t *)p_inputItem atPosition:(size_t)insertionIndex startPlayback:(BOOL)startPlayback
{
    if (p_inputItem == NULL) {
        return VLC_EINVAL;
    }
    int ret = 0;

    vlc_playlist_Lock(_p_playlist);
    if (insertionIndex == NSUIntegerMax) {
        insertionIndex = vlc_playlist_Count(_p_playlist);
    }
    ret = vlc_playlist_InsertOne(_p_playlist, insertionIndex, p_inputItem);
    if (ret != VLC_SUCCESS) {
        vlc_playlist_Unlock(_p_playlist);
        return ret;
    }
    if (startPlayback) {
        ret = vlc_playlist_PlayAt(_p_playlist, insertionIndex);
    }
    vlc_playlist_Unlock(_p_playlist);
    return ret;
}

- (int)moveItemWithID:(int64_t)uniqueID toPosition:(size_t)target
{
    vlc_playlist_item_t **items = calloc(1, sizeof(vlc_playlist_item_t *));
    vlc_playlist_Lock(_p_playlist);
    ssize_t itemIndex = vlc_playlist_IndexOfId(_p_playlist, uniqueID);
    vlc_playlist_item_t *p_item = vlc_playlist_Get(_p_playlist, itemIndex);
    items[0] = p_item;
    int ret = vlc_playlist_RequestMove(_p_playlist, items, 1, target, itemIndex);
    vlc_playlist_Unlock(_p_playlist);
    free(items);
    return ret;
}

- (void)removeItemsAtIndexes:(NSIndexSet *)indexes
{
    if (indexes.count == 0)
        return;

    __block vlc_playlist_item_t **items = calloc(indexes.count, sizeof(vlc_playlist_item_t *));
    __block NSUInteger pos = 0;
    [indexes enumerateIndexesUsingBlock:^(const NSUInteger idx, BOOL * const __unused stop) {
        VLCPlayQueueItem *item = [self->_playQueueModel playQueueItemAtIndex:idx];
        items[pos++] = item.playlistItem;
    }];

    /* note: we don't remove the cached data from the model here
     * because this will be done asynchronously through the callback */

    vlc_playlist_Lock(_p_playlist);
    vlc_playlist_RequestRemove(_p_playlist, items, pos, indexes.firstIndex);
    vlc_playlist_Unlock(_p_playlist);

    free(items);
}

- (void)clearPlayQueue
{
    vlc_playlist_Lock(_p_playlist);
    vlc_playlist_Clear(_p_playlist);
    vlc_playlist_Unlock(_p_playlist);
}

- (int)sortByKey:(enum vlc_playlist_sort_key)sortKey andOrder:(enum vlc_playlist_sort_order)sortOrder
{
    struct vlc_playlist_sort_criterion sortCriterion = { sortKey, sortOrder };
    int returnValue = VLC_SUCCESS;
    vlc_playlist_Lock(_p_playlist);
    returnValue = vlc_playlist_Sort(_p_playlist, &sortCriterion, 1);
    vlc_playlist_Unlock(_p_playlist);
    if (returnValue == VLC_SUCCESS) {
        _lastSortKey = sortKey;
        _lastSortOrder = sortOrder;
        _unsorted = NO;
    }
    return returnValue;
}

- (int)startPlayQueue
{
    const NSInteger selectedIndex = [_playQueueDataSource.tableView selectedRow];
    return [self playItemAtIndex:selectedIndex];
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

- (void)setPlaybackOrder:(enum vlc_playlist_playback_order)playbackOrder
{
    // Set the ivar first and then we will receive whatever has been applied via vlc_playlist_SetPlaybackOrder
    [self playQueuePlaybackOrderUpdated:playbackOrder];

    vlc_playlist_Lock(_p_playlist);
    vlc_playlist_SetPlaybackOrder(_p_playlist, playbackOrder);
    vlc_playlist_Unlock(_p_playlist);
}

- (void)setPlaybackRepeat:(enum vlc_playlist_playback_repeat)playbackRepeat
{
    // Set the ivar first and then we will receive whatever has been applied via vlc_playlist_SetPlaybackRepeat
    [self playQueuePlaybackRepeatUpdated:playbackRepeat];

    vlc_playlist_Lock(_p_playlist);
    vlc_playlist_SetPlaybackRepeat(_p_playlist, playbackRepeat);
    vlc_playlist_Unlock(_p_playlist);
}

- (void)setLibraryPlayQueueMode:(BOOL)libraryPlayQueueMode
{
    _libraryPlayQueueMode = libraryPlayQueueMode;
    [NSUserDefaults.standardUserDefaults setBool:libraryPlayQueueMode
                                          forKey:VLCLibraryPlayQueueModeDefaultsKey];
}

#pragma mark - properties

- (VLCInputItem *)currentlyPlayingInputItem
{
    vlc_player_t *player = vlc_playlist_GetPlayer(_p_playlist);
    VLCInputItem *inputItem;
    vlc_player_Lock(player);
    input_item_t *vlcInputItem = vlc_player_GetCurrentMedia(player);
    if (vlcInputItem) {
        inputItem = [[VLCInputItem alloc] initWithInputItem:vlcInputItem];
    }
    vlc_player_Unlock(player);
    return inputItem;
}

#pragma mark - helper methods

- (input_item_t *)createInputItemBasedOnMetadata:(VLCOpenInputMetadata *)itemMetadata
{
    intf_thread_t *p_intf = getIntf();

    input_item_t *p_input;
    BOOL b_rem = FALSE, b_dir = FALSE, b_writable = FALSE;
    NSString *uri, *name, *path;
    NSURL * url;
    NSArray *optionsArray;

    /* Get the item */
    uri = itemMetadata.MRLString;
    url = [NSURL URLWithString: uri];
    path = [url path];
    name = itemMetadata.itemName;
    optionsArray = itemMetadata.playbackOptions;

    if ([[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&b_dir]
        && b_dir &&
        [NSWorkspace.sharedWorkspace getFileSystemInfoForPath:path
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

- (NSArray<VLCPlaylistExportModuleDescription *> *)availablePlaylistExportModules
{
    VLCPlaylistExportModuleDescription *xspf = [[VLCPlaylistExportModuleDescription alloc] init];
    xspf.humanReadableName = _NS("XSPF playlist");
    xspf.fileExtension = @"xspf";
    xspf.moduleName = @"export-xspf";

    VLCPlaylistExportModuleDescription *m3u = [[VLCPlaylistExportModuleDescription alloc] init];
    m3u.humanReadableName = _NS("M3U playlist");
    m3u.fileExtension = @"m3u";
    m3u.moduleName = @"export-m3u";

    VLCPlaylistExportModuleDescription *m3u8 = [[VLCPlaylistExportModuleDescription alloc] init];
    m3u8.humanReadableName = _NS("M3U8 playlist");
    m3u8.fileExtension = @"m3u8";
    m3u8.moduleName = @"export-m3u8";

    VLCPlaylistExportModuleDescription *html = [[VLCPlaylistExportModuleDescription alloc] init];
    html.humanReadableName = _NS("HTML playlist");
    html.fileExtension = @"html";
    html.moduleName = @"export-html";

    return @[xspf, m3u, m3u8, html];
}

- (int)exportPlaylistToPath:(NSString *)path exportModule:(VLCPlaylistExportModuleDescription *)exportModule
{
    vlc_playlist_Lock(_p_playlist);
    int ret = vlc_playlist_Export(_p_playlist,
                                  path.fileSystemRepresentation,
                                  exportModule.moduleName.UTF8String);
    vlc_playlist_Unlock(_p_playlist);
    return ret;
}

- (void)stopActionChanged:(enum vlc_playlist_media_stopped_action)stoppedAction
{
    _actionAfterStop = stoppedAction;
}

- (void)setActionAfterStop:(enum vlc_playlist_media_stopped_action)actionAfterStop
{
    // Set the ivar first and then we will receive whatever has been enabled via actionAfterStopChanged
    [self stopActionChanged:actionAfterStop];

    vlc_playlist_Lock(_p_playlist);
    vlc_playlist_SetMediaStoppedAction(_p_playlist, actionAfterStop);
    vlc_playlist_Unlock(_p_playlist);
}

@end

@implementation VLCPlaylistExportModuleDescription

@end
