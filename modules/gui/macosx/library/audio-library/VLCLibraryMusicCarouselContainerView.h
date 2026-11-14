/*****************************************************************************
 * VLCLibraryMusicCarouselContainerView.h: MacOS X interface module
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

#import "library/VLCLibrarySegment.h"
#import "library/audio-library/VLCLibraryMusicCarouselDataSource.h"
#import "library/home-library/VLCLibraryHomeViewBaseCarouselContainerView.h"

NS_ASSUME_NONNULL_BEGIN

extern NSNotificationName const VLCLibraryMusicCarouselItemsChangedNotification;

typedef NS_ENUM(NSUInteger, VLCLibraryMusicCarouselItemStyle) {
    VLCLibraryMusicCarouselItemStyleArtwork,
    VLCLibraryMusicCarouselItemStyleCompact,
};

@interface VLCLibraryMusicCarouselContainerView : VLCLibraryHomeViewBaseCarouselContainerView

@property (readonly) BOOL hasLoadedItems;

- (instancetype)initWithTitle:(NSString *)title
                   parentType:(VLCMediaLibraryParentGroupType)parentType
               librarySegment:(VLCLibrarySegmentType)librarySegment
                itemsProvider:(VLCLibraryMusicCarouselItemsProvider)itemsProvider
           resetNotifications:(NSArray<NSNotificationName> *)resetNotifications;

- (instancetype)initWithTitle:(NSString *)title
                   parentType:(VLCMediaLibraryParentGroupType)parentType
               librarySegment:(VLCLibrarySegmentType)librarySegment
                itemsProvider:(VLCLibraryMusicCarouselItemsProvider)itemsProvider
           resetNotifications:(NSArray<NSNotificationName> *)resetNotifications
                 numberOfRows:(NSUInteger)numberOfRows;

- (instancetype)initWithTitle:(NSString *)title
                   parentType:(VLCMediaLibraryParentGroupType)parentType
               librarySegment:(VLCLibrarySegmentType)librarySegment
                itemsProvider:(VLCLibraryMusicCarouselItemsProvider)itemsProvider
           resetNotifications:(NSArray<NSNotificationName> *)resetNotifications
                 numberOfRows:(NSUInteger)numberOfRows
                    itemStyle:(VLCLibraryMusicCarouselItemStyle)itemStyle;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(NSRect)frameRect NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END
