/*****************************************************************************
 * VLCMediaSource.m: MacOS X interface module
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

#import "VLCMediaSource.h"

#import "library/VLCInputItem.h"

#import "extensions/NSString+Helpers.h"

@interface VLCMediaSource ()
{
    libvlc_int_t *_p_libvlcInstance;
    vlc_media_source_t *_p_mediaSource;
    vlc_media_tree_listener_id *_p_treeListenerID;
}
@end

NSString *VLCMediaSourceChildrenReset = @"VLCMediaSourceChildrenReset";
NSString *VLCMediaSourceChildrenAdded = @"VLCMediaSourceChildrenAdded";
NSString *VLCMediaSourceChildrenRemoved = @"VLCMediaSourceChildrenRemoved";
NSString *VLCMediaSourcePreparsingEnded = @"VLCMediaSourcePreparsingEnded";

static void cb_children_reset(vlc_media_tree_t *p_tree,
                              input_item_node_t *p_node,
                              void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCMediaSource *mediaSource = (__bridge VLCMediaSource *)p_data;
        [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourceChildrenReset
                                                            object:mediaSource];
    });
}

static void cb_children_added(vlc_media_tree_t *p_tree,
                              input_item_node_t *p_node,
                              input_item_node_t *const p_children[],
                              size_t count,
                              void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCMediaSource *mediaSource = (__bridge VLCMediaSource *)p_data;
        [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourceChildrenAdded
                                                            object:mediaSource];
    });
}

static void cb_children_removed(vlc_media_tree_t *p_tree,
                                input_item_node_t *p_node,
                                input_item_node_t *const p_children[],
                                size_t count,
                                void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCMediaSource *mediaSource = (__bridge VLCMediaSource *)p_data;
        [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourceChildrenRemoved
                                                            object:mediaSource];
    });
}

static void cb_preparse_ended(vlc_media_tree_t *p_tree,
                              input_item_node_t *p_node,
                              enum input_item_preparse_status status,
                              void *p_data)
{
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCMediaSource *mediaSource = (__bridge VLCMediaSource *)p_data;
        [[NSNotificationCenter defaultCenter] postNotificationName:VLCMediaSourcePreparsingEnded
                                                            object:mediaSource];
    });
}

static const struct vlc_media_tree_callbacks treeCallbacks = {
    cb_children_reset,
    cb_children_added,
    cb_children_removed,
    cb_preparse_ended,
};

@implementation VLCMediaSource

- (instancetype)initWithMediaSource:(vlc_media_source_t *)p_mediaSource andLibVLCInstance:(libvlc_int_t *)p_libvlcInstance
{
    self = [super init];
    if (self && p_mediaSource != NULL) {
        _p_libvlcInstance = p_libvlcInstance;
        _p_mediaSource = p_mediaSource;
        vlc_media_source_Hold(_p_mediaSource);
        _p_treeListenerID = vlc_media_tree_AddListener(_p_mediaSource->tree,
                                                       &treeCallbacks,
                                                       (__bridge void *)self,
                                                       NO);
    }
    return self;
}

- (void)dealloc
{
    if (_p_mediaSource != NULL) {
        if (_p_treeListenerID) {
            vlc_media_tree_RemoveListener(_p_mediaSource->tree,
                                          _p_treeListenerID);
        }
        vlc_media_source_Release(_p_mediaSource);
    }
}

- (void)preparseInputItemWithinTree:(VLCInputItem *)inputItem
{
    if (inputItem == nil) {
        return;
    }
    vlc_media_tree_Preparse(_p_mediaSource->tree, _p_libvlcInstance, inputItem.vlcInputItem, NULL);
}

- (NSString *)mediaSourceDescription
{
    if (_p_mediaSource != NULL) {
        return toNSStr(_p_mediaSource->description);
    }
    return @"";
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@ — %@", NSStringFromClass([self class]), self.mediaSourceDescription];
}

- (VLCInputNode *)rootNode
{
    vlc_media_tree_Lock(_p_mediaSource->tree);
    VLCInputNode *inputNode = [[VLCInputNode alloc] initWithInputNode:&_p_mediaSource->tree->root];
    vlc_media_tree_Unlock(_p_mediaSource->tree);
    return inputNode;
}

@end
