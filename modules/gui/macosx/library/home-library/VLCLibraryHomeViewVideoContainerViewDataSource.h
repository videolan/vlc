/*****************************************************************************
 * VLCLibraryVideoCollectionViewTableViewCellDataSource.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2022 VLC authors and VideoLAN
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

#import <Cocoa/Cocoa.h>

#import "library/VLCLibraryCollectionViewDataSource.h"
#import "views/iCarousel/iCarousel.h"

@class VLCLibraryHomeViewVideoGridContainerView;
@class VLCLibraryVideoCollectionViewGroupDescriptor;

NS_ASSUME_NONNULL_BEGIN

extern NSString * const VLCLibraryVideoCollectionViewDataSourceDisplayedCollectionChangedNotification;

@interface VLCLibraryHomeViewVideoContainerViewDataSource : NSObject <VLCLibraryCollectionViewDataSource, iCarouselDataSource>

@property (readwrite, weak) NSCollectionView *collectionView;
@property (readwrite, weak) iCarousel *carouselView;
@property (readwrite, assign, nonatomic) VLCLibraryVideoCollectionViewGroupDescriptor *groupDescriptor;
@property (readwrite, assign) VLCLibraryHomeViewVideoGridContainerView *parentCell;

- (void)setup;
- (void)reloadData;

@end

NS_ASSUME_NONNULL_END
