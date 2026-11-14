/*****************************************************************************
 * VLCLibraryMusicCarouselDataSource.h: MacOS X interface module
 *****************************************************************************
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

#import "library/VLCLibraryCollectionViewDataSource.h"
#import "library/VLCLibraryDataTypes.h"

NS_ASSUME_NONNULL_BEGIN

@class VLCLibraryModel;

extern NSNotificationName const VLCLibraryMusicCarouselDataSourceReloaded;

// The data source calls this block on a background queue to fetch the items for
// a Music carousel. Return nil to keep the current items visible while a model
// cache loads. Return an empty array when there are no items to show.
typedef NSArray<id<VLCMediaLibraryItemProtocol>> * _Nullable (^VLCLibraryMusicCarouselItemsProvider)(VLCLibraryModel *libraryModel);

@interface VLCLibraryMusicCarouselDataSource : NSObject <VLCLibraryCollectionViewDataSource>

@property (readwrite, weak, nonatomic) NSCollectionView *collectionView;

- (instancetype)initWithParentType:(VLCMediaLibraryParentGroupType)parentType
                     itemsProvider:(VLCLibraryMusicCarouselItemsProvider)itemsProvider
                resetNotifications:(NSArray<NSNotificationName> *)resetNotifications
                    itemIdentifier:(NSString *)itemIdentifier;

@end

NS_ASSUME_NONNULL_END
