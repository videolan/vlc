/*****************************************************************************
 * VLCLibraryRepresentedItem.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
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

#import "VLCLibraryRepresentedItem.h"

#import "library/VLCLibraryDataTypes.h"

@interface VLCLibraryRepresentedItem ()
{
    NSInteger _itemIndexInParent;
}
@end

@implementation VLCLibraryRepresentedItem

- (instancetype)initWithItem:(const id<VLCMediaLibraryItemProtocol>)item
{
    self = [self init];
    if (self) {
        _item = item;
    }
    return self;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    _itemIndexInParent = NSNotFound;
}

- (NSInteger)itemIndexInParent
{
    if (_itemIndexInParent != NSNotFound) {
        return _itemIndexInParent;
    }

    __block NSInteger index = 0;
    const NSInteger itemId = self.item.libraryID;

    [self.item iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem * const mediaItem) {
        if (mediaItem.libraryID == itemId) {
            return;
        }

        index++;
    }];

    _itemIndexInParent = index;
    return _itemIndexInParent;
}

@end
