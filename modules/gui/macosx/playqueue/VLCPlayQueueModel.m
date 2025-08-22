/*****************************************************************************
 * VLCPlayQueueModel.m: MacOS X interface module
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

#import "VLCPlayQueueModel.h"

#import <vlc_common.h>

#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayQueueItem.h"

@interface VLCPlayQueueModel ()
{
    NSMutableArray<VLCPlayQueueItem *> *_playQueueArray;
}
@end

@implementation VLCPlayQueueModel

- (instancetype)init
{
    self = [super init];
    if (self) {
        _playQueueArray = [[NSMutableArray alloc] init];
    }
    return self;
}

- (NSUInteger)numberOfPlayQueueItems
{
    return _playQueueArray.count;
}

- (NSArray<VLCPlayQueueItem *> *)playQueueItems
{
    return [_playQueueArray copy];
}

- (void)dropExistingData
{
    [_playQueueArray removeAllObjects];
}

- (VLCPlayQueueItem *)playQueueItemAtIndex:(NSInteger)index
{
    if (index < 0 || index > _playQueueArray.count) {
        return nil;
    }
    
    return _playQueueArray[index];
}

- (void)addItems:(NSArray<VLCPlayQueueItem *> *)array
{
    [_playQueueArray addObjectsFromArray:array];
}

- (void)addItems:(NSArray<VLCPlayQueueItem *> *)array atIndex:(size_t)index count:(size_t)count
{
    [_playQueueArray insertObjects:array atIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(index, count)]];
}

- (void)moveItemAtIndex:(size_t)index toTarget:(size_t)target
{
    VLCPlayQueueItem * const item = [_playQueueArray objectAtIndex:index];
    [_playQueueArray removeObjectAtIndex:index];
    [_playQueueArray insertObject:item atIndex:target];
}

- (void)removeItemsInRange:(NSRange)range
{
    [_playQueueArray removeObjectsInRange:range];
}

- (void)updateItemAtIndex:(size_t)index
{
    VLCPlayQueueItem * const item = _playQueueArray[index];
    [item updateRepresentation];
}
- (void)replaceItemAtIndex:(size_t)index withItem:(VLCPlayQueueItem *)newItem
{
    NSParameterAssert(index >= 0 && index < _playQueueArray.count);
    NSParameterAssert(newItem != nil);
    [_playQueueArray replaceObjectAtIndex:index withObject:newItem];
}
@end
