/*****************************************************************************
 * VLCInputItem.h: MacOS X interface module
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

#import <Foundation/Foundation.h>

#import <vlc_common.h>
#import <vlc_input_item.h>
#import <vlc_tick.h>

NS_ASSUME_NONNULL_BEGIN

@interface VLCInputItem : NSObject

- (instancetype)initWithInputItem:(struct input_item_t *)p_inputItem;

@property (readonly) NSString *name;
@property (readonly) NSString *MRL;
@property (readonly) vlc_tick_t duration;
@property (readonly) enum input_item_type_e inputType;

@end

@interface VLCInputNode : NSObject

- (instancetype)initWithInputNode:(struct input_item_node_t *)p_inputNode;

@property (readonly, nullable) VLCInputItem *inputItem;
@property (readonly) int numberOfChildren;
@property (readonly) NSArray <VLCInputNode *> *children;

@end

NS_ASSUME_NONNULL_END
