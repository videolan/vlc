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

#import "main/VLCMain.h"
#import "extensions/NSString+Helpers.h"

NSString *VLCInputItemParsingSucceeded = @"VLCInputItemParsingSucceeded";
NSString *VLCInputItemParsingFailed = @"VLCInputItemParsingFailed";
NSString *VLCInputItemSubtreeAdded = @"VLCInputItemSubtreeAdded";

@interface VLCInputItem()
{
    input_item_parser_id_t *_p_parserID;
}

- (void)parsingEnded:(int)status;
- (void)subTreeAdded:(input_item_node_t *)p_node;

@end

static void cb_parsing_ended(input_item_t *p_item, int status, void *p_data)
{
    VLC_UNUSED(p_item);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCInputItem *inputItem = (__bridge VLCInputItem *)p_data;
        [inputItem parsingEnded:status];
    });
}

static void cb_subtree_added(input_item_t *p_item, input_item_node_t *p_node, void *p_data)
{
    VLC_UNUSED(p_item);
    dispatch_async(dispatch_get_main_queue(), ^{
        VLCInputItem *inputItem = (__bridge VLCInputItem *)p_data;
        [inputItem subTreeAdded:p_node];
    });
}

static const struct input_item_parser_cbs_t parserCallbacks =
{
    cb_parsing_ended,
    cb_subtree_added,
};

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
    if (_p_parserID) {
        input_item_parser_id_Release(_p_parserID);
    }
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

- (void)parseInputItem
{
    _p_parserID = input_item_Parse(_vlcInputItem,
                                   (vlc_object_t *)getIntf(),
                                   &parserCallbacks,
                                   (__bridge void *) self);
}

- (void)cancelParsing
{
    if (_p_parserID) {
        input_item_parser_id_Interrupt(_p_parserID);
    }
}

- (void)parsingEnded:(int)status
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    if (status) {
        [notificationCenter postNotificationName:VLCInputItemParsingSucceeded object:self];
    } else {
        [notificationCenter postNotificationName:VLCInputItemParsingFailed object:self];
    }
    input_item_parser_id_Release(_p_parserID);
    _p_parserID = NULL;
}

- (void)subTreeAdded:(input_item_node_t *)p_node
{
    _subTree = p_node;
    [[NSNotificationCenter defaultCenter] postNotificationName:VLCInputItemSubtreeAdded object:self];
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

- (NSString *)description
{
    NSString *inputItemName;
    if (_p_inputNode->p_item)
        inputItemName = toNSStr(_p_inputNode->p_item->psz_name);
    else
        inputItemName = @"p_item == nil";
    return [NSString stringWithFormat:@"%@: node: %p input name: %@, number of children: %i", NSStringFromClass([self class]), _p_inputNode, inputItemName, self.numberOfChildren];
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
