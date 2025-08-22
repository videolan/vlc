/*****************************************************************************
 * VLCPlayQueueModel.h: MacOS X interface module
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
#import <vlc_playlist.h>

@class VLCPlayQueueController;
@class VLCPlayQueueItem;

NS_ASSUME_NONNULL_BEGIN

@interface VLCPlayQueueModel : NSObject

@property (readwrite, assign) VLCPlayQueueController *playQueueController;
@property (readonly) NSUInteger numberOfPlayQueueItems;
@property (readonly) NSArray<VLCPlayQueueItem *> *playQueueItems;

- (void)dropExistingData;
- (VLCPlayQueueItem *)playQueueItemAtIndex:(NSInteger)index;
- (void)addItems:(NSArray<VLCPlayQueueItem *> *)array;
- (void)addItems:(NSArray<VLCPlayQueueItem *> *)array atIndex:(size_t)index count:(size_t)count;
- (void)moveItemAtIndex:(size_t)index toTarget:(size_t)target;
- (void)removeItemsInRange:(NSRange)range;
- (void)updateItemAtIndex:(size_t)index;
- (void)replaceItemAtIndex:(size_t)index withItem:(VLCPlayQueueItem *)newItem;
@end

NS_ASSUME_NONNULL_END
