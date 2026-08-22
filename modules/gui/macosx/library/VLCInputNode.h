/****************************************************************************
 * VLCInputNode.h: MacOS X interface module
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

#import <Foundation/Foundation.h>

#import <vlc_common.h>
#import <vlc_input_item.h>

NS_ASSUME_NONNULL_BEGIN

@class VLCInputItem;

@interface VLCInputNode : NSObject

- (instancetype)initWithInputNode:(struct input_item_node_t *)p_inputNode;

@property (readonly) struct input_item_node_t *vlcInputItemNode;
@property (readonly, nullable) VLCInputItem *inputItem;
@property (readonly) int numberOfChildren;
@property (readonly, nullable) NSArray <VLCInputNode *> *children;

- (void)clearChildrenCache;

@end

NS_ASSUME_NONNULL_END
