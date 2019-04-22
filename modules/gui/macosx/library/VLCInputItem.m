/*****************************************************************************
 * VLCInputItem.m: MacOS X interface module
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

#import "VLCInputItem.h"

#import "extensions/NSString+Helpers.h"

@implementation VLCInputItem

- (instancetype)initWithInputItem:(struct input_item_t *)p_inputItem
{
    self = [super init];
    if (self && p_inputItem != NULL) {
        _vlcInputItem = p_inputItem;
        input_item_Hold(_vlcInputItem);
    }
    return self;
}

- (void)dealloc
{
    input_item_Release(_vlcInputItem);
}

- (NSString *)name
{
    if (_vlcInputItem) {
        return toNSStr(_vlcInputItem->psz_name);
    }
    return @"";
}

- (NSString *)MRL
{
    if (_vlcInputItem) {
        return toNSStr(_vlcInputItem->psz_uri);
    }
    return @"";
}

- (vlc_tick_t)duration
{
    if (_vlcInputItem) {
        return _vlcInputItem->i_duration;
    }
    return -1;
}

- (enum input_item_type_e)inputType
{
    if (_vlcInputItem) {
        return _vlcInputItem->i_type;
    }
    return ITEM_TYPE_UNKNOWN;
}

@end

@interface VLCInputNode()
{
    struct input_item_node_t *_p_inputNode;
}
@end

@implementation VLCInputNode

- (instancetype)initWithInputNode:(struct input_item_node_t *)p_inputNode
{
    self = [super init];
    if (self && p_inputNode != NULL) {
        _p_inputNode = p_inputNode;
    }
    return self;
}

- (VLCInputItem *)inputItem
{
    if (_p_inputNode->p_item) {
        return [[VLCInputItem alloc] initWithInputItem:_p_inputNode->p_item];
    }
    return nil;
}

- (int)numberOfChildren
{
    return _p_inputNode->i_children;
}

- (NSArray<VLCInputNode *> *)children
{
    NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:_p_inputNode->i_children];
    for (int i = 0; i < _p_inputNode->i_children; i++) {
        VLCInputNode *inputNode = [[VLCInputNode alloc] initWithInputNode:_p_inputNode->pp_children[i]];
        if (inputNode) {
            [mutableArray addObject:inputNode];
        }
    }
    return [mutableArray copy];
}

@end
