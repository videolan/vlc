/****************************************************************************
 * VLCInputNode.m: MacOS X interface module
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
 *          Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *****************************************************************************/

#import "VLCInputNode.h"

#import "VLCInputItem.h"

#import "extensions/NSString+Helpers.h"

@interface VLCInputNode ()
{
    NSArray<VLCInputNode *> *_cachedChildren;
}
@end

@implementation VLCInputNode

- (instancetype)initWithInputNode:(struct input_item_node_t *)p_inputNode
{
    self = [super init];
    if (self && p_inputNode != NULL) {
        _vlcInputItemNode = p_inputNode;

        if (_vlcInputItemNode->p_item) {
            _inputItem = [[VLCInputItem alloc] initWithInputItem:_vlcInputItemNode->p_item];
        }
    }
    return self;
}

- (NSString *)description
{
    NSString *inputItemName;
    if (_vlcInputItemNode && _vlcInputItemNode->p_item)
        inputItemName = toNSStr(_vlcInputItemNode->p_item->psz_name);
    else
        inputItemName = @"p_item == nil";
    return [NSString stringWithFormat:@"%@: node: %p input name: %@, number of children: %i", NSStringFromClass([self class]),_vlcInputItemNode, inputItemName, self.numberOfChildren];
}

- (void)clearChildrenCache
{
    _cachedChildren = nil;
}

- (int)numberOfChildren
{
    if (_cachedChildren) {
        return (int)_cachedChildren.count;
    }
    return _vlcInputItemNode ? _vlcInputItemNode->i_children : 0;
}

- (nullable NSArray<VLCInputNode *> *)children
{
    if (_cachedChildren) {
        return _cachedChildren;
    }

    if (_vlcInputItemNode == NULL) {
        return nil;
    }
    NSMutableArray *mutableArray = [[NSMutableArray alloc] initWithCapacity:_vlcInputItemNode->i_children];
    for (int i = 0; i < _vlcInputItemNode->i_children; i++) {
        if (_vlcInputItemNode->pp_children == NULL) {
            break;
        }
        VLCInputNode *inputNode = [[VLCInputNode alloc] initWithInputNode:_vlcInputItemNode->pp_children[i]];
        if (inputNode) {
            [mutableArray addObject:inputNode];
        }
    }
    _cachedChildren = [mutableArray copy];
    return _cachedChildren;
}

@end
