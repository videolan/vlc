/*****************************************************************************
>>>>>>> Stashed changes
 * VLCPLItem.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2014 VLC authors and VideoLAN
 * $Id$
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

#import "VLCPLModel.h"

#import "misc.h"    /* VLCByteCountFormatter */

#import "VLCPlaylist.h"
#import "VLCStringUtility.h"
#import "VLCMain.h"
#import "VLCMainWindowControlsBar.h"
#import "VLCMainMenu.h"
#import "VLCPlaylistInfo.h"
#import "VLCMainWindow.h"

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif
#include <assert.h>

#include <vlc_playlist_legacy.h>
#include <vlc_input_item.h>
#include <vlc_input.h>
#include <vlc_url.h>

@interface VLCPLModel ()
{
    playlist_t *p_playlist;
    __weak NSOutlineView *_outlineView;

    NSUInteger _retainedRowSelection;
}

- (void)VLCPLItemAppended:(NSArray *)valueArray;
- (void)VLCPLItemRemoved:(NSNumber *)value;
- (void)VLCPLItemUpdated;

@end

#pragma mark -

static int VLCPLItemUpdated(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        VLCPLModel *model = (__bridge VLCPLModel*)param;
        [model performSelectorOnMainThread:@selector(VLCPLItemUpdated) withObject:nil waitUntilDone:NO];

        return VLC_SUCCESS;
    }
}

static int VLCPLItemAppended(vlc_object_t *p_this, const char *psz_var,
                          vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        playlist_item_t *p_item = new_val.p_address;
        int i_node = p_item->p_parent ? p_item->p_parent->i_id : -1;
        NSArray *o_val = [NSArray arrayWithObjects:[NSNumber numberWithInt:i_node], [NSNumber numberWithInt:p_item->i_id], nil];
        VLCPLModel *model = (__bridge VLCPLModel*)param;
        [model performSelectorOnMainThread:@selector(VLCPLItemAppended:) withObject:o_val waitUntilDone:NO];

        return VLC_SUCCESS;
    }
}

static int VLCPLItemRemoved(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        playlist_item_t *p_item = new_val.p_address;
        NSNumber *o_val = [NSNumber numberWithInt:p_item->i_id];
        VLCPLModel *model = (__bridge VLCPLModel*)param;
        [model performSelectorOnMainThread:@selector(VLCPLItemRemoved:) withObject:o_val waitUntilDone:NO];

        return VLC_SUCCESS;
    }
}

static int PlaybackModeUpdated(vlc_object_t *p_this, const char *psz_var,
                               vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        VLCPLModel *model = (__bridge VLCPLModel*)param;
        [model performSelectorOnMainThread:@selector(playbackModeUpdated) withObject:nil waitUntilDone:NO];

        return VLC_SUCCESS;
    }
}

static int VolumeUpdated(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{
            [[[VLCMain sharedInstance] mainWindow] updateVolumeSlider];
        });

        return VLC_SUCCESS;
    }
}

#pragma mark -

@implementation VLCPLModel

#pragma mark -
#pragma mark Init and Stuff

- (id)initWithOutlineView:(NSOutlineView *)outlineView playlist:(playlist_t *)pl rootItem:(playlist_item_t *)root;
{
    self = [super init];
    if (self) {
        p_playlist = pl;
        _outlineView = outlineView;

        msg_Dbg(getIntf(), "Initializing playlist model");
        var_AddCallback(p_playlist, "item-change", VLCPLItemUpdated, (__bridge void *)self);
        var_AddCallback(p_playlist, "playlist-item-append", VLCPLItemAppended, (__bridge void *)self);
        var_AddCallback(p_playlist, "playlist-item-deleted", VLCPLItemRemoved, (__bridge void *)self);
        var_AddCallback(p_playlist, "random", PlaybackModeUpdated, (__bridge void *)self);
        var_AddCallback(p_playlist, "repeat", PlaybackModeUpdated, (__bridge void *)self);
        var_AddCallback(p_playlist, "loop", PlaybackModeUpdated, (__bridge void *)self);
        var_AddCallback(p_playlist, "volume", VolumeUpdated, (__bridge void *)self);
        var_AddCallback(p_playlist, "mute", VolumeUpdated, (__bridge void *)self);

        PL_LOCK;
        _rootItem = [[VLCPLItem alloc] initWithPlaylistItem:root];
        [self rebuildVLCPLItem:_rootItem];
        PL_UNLOCK;
    }

    return self;
}

- (void)dealloc
{
    msg_Dbg(getIntf(), "Deinitializing playlist model");
    var_DelCallback(p_playlist, "item-change", VLCPLItemUpdated, (__bridge void *)self);
    var_DelCallback(p_playlist, "playlist-item-append", VLCPLItemAppended, (__bridge void *)self);
    var_DelCallback(p_playlist, "playlist-item-deleted", VLCPLItemRemoved, (__bridge void *)self);
    var_DelCallback(p_playlist, "random", PlaybackModeUpdated, (__bridge void *)self);
    var_DelCallback(p_playlist, "repeat", PlaybackModeUpdated, (__bridge void *)self);
    var_DelCallback(p_playlist, "loop", PlaybackModeUpdated, (__bridge void *)self);
    var_DelCallback(p_playlist, "volume", VolumeUpdated, (__bridge void *)self);
    var_DelCallback(p_playlist, "mute", VolumeUpdated, (__bridge void *)self);
}

- (void)changeRootItem:(playlist_item_t *)p_root;
{
    PL_ASSERT_LOCKED;
    _rootItem = [[VLCPLItem alloc] initWithPlaylistItem:p_root];
    [self rebuildVLCPLItem:_rootItem];

    [_outlineView reloadData];
    [_outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
}

- (BOOL)hasChildren
{
    return [[_rootItem children] count] > 0;
}

- (PLRootType)currentRootType
{
    int i_root_id = [_rootItem plItemId];
    if (i_root_id == p_playlist->p_playing->i_id)
        return ROOT_TYPE_PLAYLIST;

    return ROOT_TYPE_OTHER;
}

- (BOOL)editAllowed
{
    return [self currentRootType] == ROOT_TYPE_PLAYLIST;
}

- (void)deleteSelectedItem
{
    // check if deletion is allowed
    if (![self editAllowed])
        return;

    NSIndexSet *selectedIndexes = [_outlineView selectedRowIndexes];
    _retainedRowSelection = [selectedIndexes firstIndex];
    if (_retainedRowSelection == NSNotFound)
        _retainedRowSelection = 0;

    [selectedIndexes enumerateIndexesUsingBlock:^(NSUInteger idx, BOOL *stop) {
        VLCPLItem *item = [self->_outlineView itemAtRow: idx];
        if (!item)
            return;

        // model deletion is done via callback
        playlist_Lock( self->p_playlist );
        playlist_item_t *p_root = playlist_ItemGetById(self->p_playlist, [item plItemId]);
        if( p_root != NULL )
            playlist_NodeDelete(self->p_playlist, p_root);
        playlist_Unlock( self->p_playlist );
    }];
}

- (void)rebuildVLCPLItem:(VLCPLItem *)item
{
    [item clear];
    playlist_item_t *p_item = playlist_ItemGetById(p_playlist, [item plItemId]);
    if (p_item) {
        int currPos = 0;
        for(int i = 0; i < p_item->i_children; ++i) {
            playlist_item_t *p_child = p_item->pp_children[i];

            if (p_child->i_flags & PLAYLIST_DBL_FLAG)
                continue;

            VLCPLItem *child = [[VLCPLItem alloc] initWithPlaylistItem:p_child];
            [item addChild:child atPos:currPos++];

            if (p_child->i_children >= 0) {
                [self rebuildVLCPLItem:child];
            }

        }
    }

}

- (VLCPLItem *)findItemByPlaylistId:(int)i_pl_id
{
    return [self findItemInnerByPlaylistId:i_pl_id node:_rootItem];
}

- (VLCPLItem *)findItemInnerByPlaylistId:(int)i_pl_id node:(VLCPLItem *)node
{
    if ([node plItemId] == i_pl_id) {
        return node;
    }

    for (NSUInteger i = 0; i < [[node children] count]; ++i) {
        VLCPLItem *o_sub_item = [[node children] objectAtIndex:i];
        if ([o_sub_item plItemId] == i_pl_id) {
            return o_sub_item;
        }

        if (![o_sub_item isLeaf]) {
            VLCPLItem *o_returned = [self findItemInnerByPlaylistId:i_pl_id node:o_sub_item];
            if (o_returned)
                return o_returned;
        }
    }

    return nil;
}

#pragma mark -
#pragma mark Core events


- (void)VLCPLItemAppended:(NSArray *)valueArray
{
    int i_node = [[valueArray firstObject] intValue];
    int i_item = [[valueArray objectAtIndex:1] intValue];

    [self addItem:i_item withParentNode:i_node];

    // update badge in sidebar
    [[[VLCMain sharedInstance] mainWindow] updateWindow];

    [[NSNotificationCenter defaultCenter] postNotificationName: VLCMediaKeySupportSettingChangedNotification
                                                        object: nil
                                                      userInfo: nil];
}

- (void)VLCPLItemRemoved:(NSNumber *)value
{
    int i_item = [value intValue];

    [self removeItem:i_item];
    // retain selection before deletion
    [_outlineView selectRowIndexes:[NSIndexSet indexSetWithIndex:_retainedRowSelection] byExtendingSelection:NO];

    // update badge in sidebar
    [[[VLCMain sharedInstance] mainWindow] updateWindow];

    [[NSNotificationCenter defaultCenter] postNotificationName: VLCMediaKeySupportSettingChangedNotification
                                                        object: nil
                                                      userInfo: nil];
}

- (void)VLCPLItemUpdated
{
    VLCMain *instance = [VLCMain sharedInstance];
    [[instance mainWindow] updateName];

    [[instance currentMediaInfoPanel] updateMetadata];
}

- (void)addItem:(int)i_item withParentNode:(int)i_node
{
    VLCPLItem *o_parent = [self findItemByPlaylistId:i_node];
    if (!o_parent) {
        return;
    }

    PL_LOCK;
    playlist_item_t *p_item = playlist_ItemGetById(p_playlist, i_item);
    if (!p_item || p_item->i_flags & PLAYLIST_DBL_FLAG)
    {
        PL_UNLOCK;
        return;
    }

    int pos;
    for(pos = p_item->p_parent->i_children - 1; pos >= 0; pos--)
        if(p_item->p_parent->pp_children[pos] == p_item)
            break;

    VLCPLItem *o_new_item = [[VLCPLItem alloc] initWithPlaylistItem:p_item];
    PL_UNLOCK;
    if (pos < 0)
        return;

    [o_parent addChild:o_new_item atPos:pos];

    if ([o_parent plItemId] == [_rootItem plItemId])
        [_outlineView reloadData];
    else // only reload leafs this way, doing it with nil collapses width of title column
        [_outlineView reloadItem:o_parent reloadChildren:YES];
}

- (void)removeItem:(int)i_item
{
    VLCPLItem *o_item = [self findItemByPlaylistId:i_item];
    if (!o_item) {
        return;
    }

    VLCPLItem *o_parent = [o_item parent];
    [o_parent deleteChild:o_item];

    if ([o_parent plItemId] == [_rootItem plItemId])
        [_outlineView reloadData];
    else
        [_outlineView reloadItem:o_parent reloadChildren:YES];
}

- (void)updateItem:(input_item_t *)p_input_item
{
    PL_LOCK;
    playlist_item_t *pl_item = playlist_ItemGetByInput(p_playlist, p_input_item);
    if (!pl_item) {
        PL_UNLOCK;
        return;
    }
    VLCPLItem *item = [self findItemByPlaylistId:pl_item->i_id];
    PL_UNLOCK;

    if (!item)
        return;

    NSInteger row = [_outlineView rowForItem:item];
    if (row == -1)
        return;

    [_outlineView reloadDataForRowIndexes:[NSIndexSet indexSetWithIndex:row]
                            columnIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, [[_outlineView tableColumns] count])]];

}

- (VLCPLItem *)currentlyPlayingItem
{
    VLCPLItem *item = nil;

    PL_LOCK;
    playlist_item_t *p_current = playlist_CurrentPlayingItem(p_playlist);
    if (p_current)
        item = [self findItemByPlaylistId:p_current->i_id];
    PL_UNLOCK;
    return item;
}

- (void)playbackModeUpdated
{
    bool loop = var_GetBool(p_playlist, "loop");
    bool repeat = var_GetBool(p_playlist, "repeat");

    VLCMainWindowControlsBar *controlsBar = (VLCMainWindowControlsBar *)[[[VLCMain sharedInstance] mainWindow] controlsBar];
    VLCMainMenu *mainMenu = [[VLCMain sharedInstance] mainMenu];
    if (repeat) {
        [controlsBar setRepeatOne];
        [mainMenu setRepeatOne];
    } else if (loop) {
        [controlsBar setRepeatAll];
        [mainMenu setRepeatAll];
    } else {
        [controlsBar setRepeatOff];
        [mainMenu setRepeatOff];
    }

    [controlsBar setShuffle];
    [mainMenu setShuffle];
}

#pragma mark -
#pragma mark Sorting / Searching

- (void)sortForColumn:(NSString *)o_column withMode:(int)i_mode
{
    int i_column = 0;
    if ([o_column isEqualToString:TRACKNUM_COLUMN])
        i_column = SORT_TRACK_NUMBER;
    else if ([o_column isEqualToString:TITLE_COLUMN])
        i_column = SORT_TITLE;
    else if ([o_column isEqualToString:ARTIST_COLUMN])
        i_column = SORT_ARTIST;
    else if ([o_column isEqualToString:GENRE_COLUMN])
        i_column = SORT_GENRE;
    else if ([o_column isEqualToString:DURATION_COLUMN])
        i_column = SORT_DURATION;
    else if ([o_column isEqualToString:ALBUM_COLUMN])
        i_column = SORT_ALBUM;
    else if ([o_column isEqualToString:DESCRIPTION_COLUMN])
        i_column = SORT_DESCRIPTION;
    else if ([o_column isEqualToString:URI_COLUMN])
        i_column = SORT_URI;
    else
        return;

    PL_LOCK;
    playlist_item_t *p_root = playlist_ItemGetById(p_playlist, [_rootItem plItemId]);
    if (!p_root) {
        PL_UNLOCK;
        return;
    }

    playlist_RecursiveNodeSort(p_playlist, p_root, i_column, i_mode);

    [self rebuildVLCPLItem:_rootItem];
    [_outlineView reloadData];
    PL_UNLOCK;
}

- (void)searchUpdate:(NSString *)o_search
{
    PL_LOCK;
    playlist_item_t *p_root = playlist_ItemGetById(p_playlist, [_rootItem plItemId]);
    if (!p_root) {
        PL_UNLOCK;
        return;
    }
    playlist_LiveSearchUpdate(p_playlist, p_root, [o_search UTF8String],
                              true);
    [self rebuildVLCPLItem:_rootItem];
    [_outlineView reloadData];
    PL_UNLOCK;
}

@end

#pragma mark -
#pragma mark Outline view data source

@implementation VLCPLModel(NSOutlineViewDataSource)

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    return !item ? [[_rootItem children] count] : [[item children] count];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    return !item ? YES : [[item children] count] > 0;
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item
{
    id obj = !item ? _rootItem : item;
    return [[obj children] objectAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    id o_value = nil;
    char *psz_value;

    input_item_t *p_input = [item input];

    NSString * o_identifier = [tableColumn identifier];

    if ([o_identifier isEqualToString:TRACKNUM_COLUMN]) {
        psz_value = input_item_GetTrackNumber(p_input);
        o_value = toNSStr(psz_value);
        free(psz_value);

    } else if ([o_identifier isEqualToString:TITLE_COLUMN]) {
        psz_value = input_item_GetTitleFbName(p_input);
        o_value = toNSStr(psz_value);
        free(psz_value);

    } else if ([o_identifier isEqualToString:ARTIST_COLUMN]) {
        psz_value = input_item_GetArtist(p_input);
        o_value = toNSStr(psz_value);
        free(psz_value);

    } else if ([o_identifier isEqualToString:DURATION_COLUMN]) {
        char psz_duration[MSTRTIME_MAX_SIZE];
        vlc_tick_t dur = input_item_GetDuration(p_input);
        if (dur != -1) {
            secstotimestr(psz_duration, (int32_t)SEC_FROM_VLC_TICK(dur));
            o_value = toNSStr(psz_duration);
        }
        else
            o_value = @"--:--";

    } else if ([o_identifier isEqualToString:GENRE_COLUMN]) {
        psz_value = input_item_GetGenre(p_input);
        o_value = toNSStr(psz_value);
        free(psz_value);

    } else if ([o_identifier isEqualToString:ALBUM_COLUMN]) {
        psz_value = input_item_GetAlbum(p_input);
        o_value = toNSStr(psz_value);
        free(psz_value);

    } else if ([o_identifier isEqualToString:DESCRIPTION_COLUMN]) {
        psz_value = input_item_GetDescription(p_input);
        o_value = toNSStr(psz_value);
        free(psz_value);

    } else if ([o_identifier isEqualToString:DATE_COLUMN]) {
        psz_value = input_item_GetDate(p_input);
        o_value = toNSStr(psz_value);
        free(psz_value);

    } else if ([o_identifier isEqualToString:LANGUAGE_COLUMN]) {
        psz_value = input_item_GetLanguage(p_input);
        o_value = toNSStr(psz_value);
        free(psz_value);

    } else if ([o_identifier isEqualToString:URI_COLUMN]) {
        psz_value = vlc_uri_decode(input_item_GetURI(p_input));
        o_value = toNSStr(psz_value);
        free(psz_value);

    } else if ([o_identifier isEqualToString:FILESIZE_COLUMN]) {
        psz_value = input_item_GetURI(p_input);
        if (!psz_value)
            return @"";
        NSURL *url = [NSURL URLWithString:toNSStr(psz_value)];
        free(psz_value);
        if (![url isFileURL])
            return @"";

        NSFileManager *fileManager = [NSFileManager defaultManager];
        BOOL b_isDir;
        if (![fileManager fileExistsAtPath:[url path] isDirectory:&b_isDir] || b_isDir)
            return @"";

        NSDictionary *attributes = [fileManager attributesOfItemAtPath:[url path] error:nil];
        if (!attributes)
            return @"";

        o_value = [VLCByteCountFormatter stringFromByteCount:[attributes fileSize] countStyle:NSByteCountFormatterCountStyleDecimal];

    } else if ([o_identifier isEqualToString:STATUS_COLUMN]) {
        if (input_item_HasErrorWhenReading(p_input)) {
            o_value = [[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeForHFSTypeCode(kAlertCautionIcon)];
            [o_value setSize: NSMakeSize(16,16)];
        }
    }

    return o_value;
}

#pragma mark -
#pragma mark Drag and Drop support

- (BOOL)isItem: (VLCPLItem *)p_item inNode: (VLCPLItem *)p_node
{
    while(p_item) {
        if ([p_item plItemId] == [p_node plItemId]) {
            return YES;
        }

        p_item = [p_item parent];
    }

    return NO;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView writeItems:(NSArray *)items toPasteboard:(NSPasteboard *)pboard
{
    _draggedItems = [[NSMutableArray alloc] initWithArray:items];

    /* Add the data to the pasteboard object. */
    [pboard declareTypes: [NSArray arrayWithObject:VLCPLItemPasteboadType] owner: self];
    [pboard setData:[NSData data] forType:VLCPLItemPasteboadType];

    return YES;
}

- (NSDragOperation)outlineView:(NSOutlineView *)outlineView validateDrop:(id <NSDraggingInfo>)info proposedItem:(id)item proposedChildIndex:(NSInteger)index
{
    NSPasteboard *o_pasteboard = [info draggingPasteboard];

    /* Dropping ON items is not allowed if item is not a node */
    if (item) {
        if (index == NSOutlineViewDropOnItemIndex && [item isLeaf]) {
            return NSDragOperationNone;
        }
    }

    if (![self editAllowed])
        return NSDragOperationNone;

    /* Drop from the Playlist */
    if ([[o_pasteboard types] containsObject:VLCPLItemPasteboadType]) {
        NSUInteger count = [_draggedItems count];
        for (NSUInteger i = 0 ; i < count ; i++) {
            /* We refuse to Drop in a child of an item we are moving */
            if ([self isItem: item inNode: [_draggedItems objectAtIndex:i]]) {
                return NSDragOperationNone;
            }
        }
        return NSDragOperationMove;
    }
    /* Drop from the Finder */
    else if ([[o_pasteboard types] containsObject: NSFilenamesPboardType]) {
        return NSDragOperationGeneric;
    }
    return NSDragOperationNone;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView acceptDrop:(id <NSDraggingInfo>)info item:(id)targetItem childIndex:(NSInteger)index
{
    NSPasteboard *o_pasteboard = [info draggingPasteboard];

    if (targetItem == nil) {
        targetItem = _rootItem;
    }

    /* Drag & Drop inside the playlist */
    if ([[o_pasteboard types] containsObject:VLCPLItemPasteboadType]) {

        NSMutableArray *o_filteredItems = [NSMutableArray arrayWithArray:_draggedItems];
        const NSUInteger draggedItemsCount = [_draggedItems count];
        for (NSInteger i = 0; i < [o_filteredItems count]; i++) {
            for (NSUInteger j = 0; j < draggedItemsCount; j++) {
                VLCPLItem *itemToCheck = [o_filteredItems objectAtIndex:i];
                VLCPLItem *nodeToTest = [_draggedItems objectAtIndex:j];
                if ([itemToCheck plItemId] == [nodeToTest plItemId])
                    continue;

                if ([self isItem:itemToCheck inNode:nodeToTest]) {
                    [o_filteredItems removeObjectAtIndex:i];
                    --i;
                    break;
                }
            }
        }

        NSUInteger count = [o_filteredItems count];
        if (count == 0)
            return NO;

        playlist_item_t **pp_items = (playlist_item_t **)calloc(count, sizeof(playlist_item_t*));
        if (!pp_items)
            return NO;

        PL_LOCK;
        playlist_item_t *p_new_parent = playlist_ItemGetById(p_playlist, [targetItem plItemId]);
        if (!p_new_parent) {
            PL_UNLOCK;
            free(pp_items);
            return NO;
        }

        int j = 0;
        for (int i = 0; i < count; i++) {
            playlist_item_t *p_item = playlist_ItemGetById(p_playlist, [[o_filteredItems objectAtIndex:i] plItemId]);
            if (p_item)
                pp_items[j++] = p_item;
        }

        // drop on a node itself will append entries at the end
        if (index == NSOutlineViewDropOnItemIndex)
            index = p_new_parent->i_children;

        if (playlist_TreeMoveMany(p_playlist, j, pp_items, p_new_parent, (int)index) != VLC_SUCCESS) {
            PL_UNLOCK;
            free(pp_items);
            return NO;
        }

        PL_UNLOCK;
        free(pp_items);

        // FIXME: Fix below code to avoid rebuilding the whole model
        // rebuild our model
//        NSUInteger filteredItemsCount = [o_filteredItems count];
//        for(int i = 0; i < filteredItemsCount; ++i) {
//            VLCPLItem *o_item = [o_filteredItems objectAtIndex:i];
//            NSLog(@"delete child from parent %p", [o_item parent]);
//            [[o_item parent] deleteChild:o_item];
//            [targetItem addChild:o_item atPos:(int)index + i];
//        }

        PL_LOCK;
        [self rebuildVLCPLItem:_rootItem];
        PL_UNLOCK;

        [_outlineView reloadData];

        NSMutableIndexSet *selectedIndexes = [[NSMutableIndexSet alloc] init];
        for(NSUInteger i = 0; i < draggedItemsCount; ++i) {
            NSInteger row = [_outlineView rowForItem:[_draggedItems objectAtIndex:i]];
            if (row < 0)
                continue;

            [selectedIndexes addIndex:row];
        }

        if ([selectedIndexes count] == 0)
            [selectedIndexes addIndex:[_outlineView rowForItem:targetItem]];

        [_outlineView selectRowIndexes:selectedIndexes byExtendingSelection:NO];

        return YES;
    }

    // try file drop

    // drop on a node itself will append entries at the end
    static_assert(NSOutlineViewDropOnItemIndex == -1, "Expect NSOutlineViewDropOnItemIndex to be -1");

    NSArray *items = [[[VLCMain sharedInstance] playlist] createItemsFromExternalPasteboard:o_pasteboard];
    if (items.count == 0)
        return NO;

    [[[VLCMain sharedInstance] playlist] addPlaylistItems:items
                                         withParentItemId:[targetItem plItemId]
                                                    atPos:(int)index
                                            startPlayback:NO];
    return YES;
}

@end
