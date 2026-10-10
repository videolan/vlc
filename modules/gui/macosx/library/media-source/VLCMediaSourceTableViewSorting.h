/****************************************************************************
 * VLCMediaSourceTableViewSorting.h: Browse table sorting
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

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface VLCMediaSourceTableViewSorting : NSObject

// Descriptors are applied in array order: the first is the primary sort,
// and each subsequent descriptor breaks ties left by the preceding ones.
- (instancetype)initWithSortDescriptors:(NSArray<NSSortDescriptor *> *)sortDescriptors;

// Returns YES if any configured descriptor uses this key, including tie breakers,
// and NO otherwise. This query leaves descriptor order and priority unchanged.
- (BOOL)sortsByKey:(NSString *)sortDescriptorKey;

- (NSArray *)sortedItems:(NSArray *)items
            valueForItem:(id (^)(id item, NSString *key))valueForItem;

@end

NS_ASSUME_NONNULL_END
