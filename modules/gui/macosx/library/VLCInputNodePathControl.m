/*****************************************************************************
 * VLCInputNodePathControl.m: MacOS X interface module
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

#import "VLCInputNodePathControl.h"

#import "VLCInputNodePathControlItem.h"

@implementation VLCInputNodePathControl

- (void)appendInputNodePathControlItem:(VLCInputNodePathControlItem *)inputNodePathControlItem
{
    if (_inputNodePathControlItems == nil) {
        _inputNodePathControlItems = [NSMutableDictionary dictionary];
    }

    [_inputNodePathControlItems setObject:inputNodePathControlItem forKey:inputNodePathControlItem.image.name];

    NSMutableArray *pathItems = [NSMutableArray arrayWithArray:self.pathItems];
    [pathItems addObject:inputNodePathControlItem];
    self.pathItems = pathItems;
}

- (void)removeLastInputNodePathControlItem
{
    if (self.pathItems.count == 0) {
        _inputNodePathControlItems = [NSMutableDictionary dictionary];
        return;
    }

    NSMutableArray *pathItems = [NSMutableArray arrayWithArray:self.pathItems];
    NSPathControlItem *lastItem = pathItems.lastObject;

    [pathItems removeLastObject];
    self.pathItems = pathItems;
    [_inputNodePathControlItems removeObjectForKey:lastItem.image.name];
}

- (void)clearInputNodePathControlItems
{
    _inputNodePathControlItems = [NSMutableDictionary dictionary];
    self.pathItems = @[];
}

@end
