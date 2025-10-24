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
    NSParameterAssert(inputNodePathControlItem != nil);
    NSParameterAssert(inputNodePathControlItem.image != nil);
    NSParameterAssert(inputNodePathControlItem.image.accessibilityDescription != nil);
    NSParameterAssert(![inputNodePathControlItem.image.accessibilityDescription isEqualToString:@""]);

    if (self.inputNodePathControlItems == nil) {
        _inputNodePathControlItems = [NSMutableDictionary dictionary];
    }

    [self.inputNodePathControlItems setObject:inputNodePathControlItem forKey:inputNodePathControlItem.image.accessibilityDescription];

    NSMutableArray * const pathItems = [NSMutableArray arrayWithArray:self.pathItems];
    [pathItems addObject:inputNodePathControlItem];
    self.pathItems = pathItems;
}

- (void)removeLastInputNodePathControlItem
{
    if (self.pathItems.count == 0) {
        _inputNodePathControlItems = NSMutableDictionary.dictionary;
        return;
    }

    NSMutableArray * const pathItems = [NSMutableArray arrayWithArray:self.pathItems];
    NSPathControlItem * const lastItem = pathItems.lastObject;

    [pathItems removeLastObject];
    self.pathItems = pathItems;
    [self.inputNodePathControlItems removeObjectForKey:lastItem.image.accessibilityDescription];
}

- (void)clearInputNodePathControlItems
{
    _inputNodePathControlItems = NSMutableDictionary.dictionary;
    self.pathItems = @[];
}

- (void)clearPathControlItemsAheadOf:(NSPathControlItem *)item
{
    if ([item.image.accessibilityDescription isEqualToString:@""]) {
        return;
    }

    const NSUInteger indexOfItem = [self.pathItems indexOfObjectPassingTest:^BOOL(NSPathControlItem * const searchItem, const NSUInteger __unused idx, BOOL * const __unused stop) {
        return [searchItem.image.accessibilityDescription isEqualToString:item.image.accessibilityDescription];
    }];

    if (indexOfItem == NSNotFound) {
        return;
    }

    NSMutableArray<NSPathControlItem *> * const pathItems = [NSMutableArray arrayWithArray:self.pathItems];
    NSArray<NSPathControlItem *> * const itemsToRemove = [pathItems subarrayWithRange:NSMakeRange(indexOfItem + 1, pathItems.count - indexOfItem - 1)];
    NSMutableArray<NSString *> * const itemIdsToRemove = [NSMutableArray arrayWithCapacity:itemsToRemove.count];

    for (NSPathControlItem * const searchItem in itemsToRemove) {
        NSString * const searchItemId = searchItem.image.accessibilityDescription;
        [itemIdsToRemove addObject:searchItemId];
    };

    self.pathItems = [pathItems subarrayWithRange:NSMakeRange(0, indexOfItem + 1)];
    [self.inputNodePathControlItems removeObjectsForKeys:itemIdsToRemove];
}

@end
