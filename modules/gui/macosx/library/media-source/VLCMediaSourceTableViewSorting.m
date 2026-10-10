/****************************************************************************
 * VLCMediaSourceTableViewSorting.m: Browse table sorting
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCMediaSourceTableViewSorting.h"

@interface VLCMediaSourceTableViewSorting ()
@property (readonly, copy) NSArray<NSSortDescriptor *> *sortDescriptors;
@end

@implementation VLCMediaSourceTableViewSorting

- (instancetype)initWithSortDescriptors:(NSArray<NSSortDescriptor *> *)sortDescriptors
{
    self = [super init];
    if (self) {
        _sortDescriptors = [sortDescriptors copy];
    }
    return self;
}

- (BOOL)sortsByKey:(NSString *)sortDescriptorKey
{
    for (NSSortDescriptor * const descriptor in self.sortDescriptors) {
        if ([descriptor.key isEqualToString:sortDescriptorKey]) {
            return YES;
        }
    }
    return NO;
}

- (NSArray *)sortedItems:(NSArray *)items
            valueForItem:(id (^)(id item, NSString *key))valueForItem
{
    if (self.sortDescriptors.count == 0) {
        return items;
    }
    // Cache by object identity for this call only; subsequent sorts read fresh values.
    NSMapTable<id, NSDictionary<NSString *, id> *> * const valuesByItem =
        [NSMapTable mapTableWithKeyOptions:NSPointerFunctionsStrongMemory | NSPointerFunctionsObjectPointerPersonality
                              valueOptions:NSPointerFunctionsStrongMemory];

    // Read each distinct object's active keys once to avoid metadata reads during comparisons.
    for (id item in items) {
        if ([valuesByItem objectForKey:item] != nil) {
            continue;
        }
        NSMutableDictionary<NSString *, id> * const values = NSMutableDictionary.dictionary;
        for (NSSortDescriptor * const descriptor in self.sortDescriptors) {
            if (values[descriptor.key] == nil) {
                values[descriptor.key] = valueForItem(item, descriptor.key);
            }
        }
        [valuesByItem setObject:values forKey:item];
    }

    // Sort the original objects, preserving their input order when all descriptors compare equal.
    return [items sortedArrayWithOptions:NSSortStable usingComparator:^NSComparisonResult(id left, id right) {
        // Compare cached values in descriptor order, using later descriptors to break ties.
        for (NSSortDescriptor * const descriptor in self.sortDescriptors) {
            const NSComparisonResult result = [descriptor compareObject:[valuesByItem objectForKey:left]
                                                               toObject:[valuesByItem objectForKey:right]];
            if (result != NSOrderedSame) {
                return result;
            }
        }
        return NSOrderedSame;
    }];
}

@end
