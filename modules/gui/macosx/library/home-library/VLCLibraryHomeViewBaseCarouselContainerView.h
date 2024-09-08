/*****************************************************************************
 * VLCLibraryHomeViewBaseCarouselContainerView.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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

#import "VLCLibraryHomeViewContainerView.h"

#import "library/VLCLibraryCollectionViewDataSource.h"
#import "views/iCarousel/iCarousel.h"

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryHomeViewBaseCarouselContainerView : NSView<VLCLibraryHomeViewContainerView, iCarouselDelegate>

@property (readonly) NSTextField *titleView;
@property (readonly) iCarousel *carouselView;
@property (readonly) NSButton *leftButton;
@property (readonly) NSButton *rightButton;
@property (readonly) NSObject<VLCLibraryCollectionViewDataSource, iCarouselDataSource> *dataSource;
// We want the carousel view to be packed tight around the actual items and not have excess space.
// To do this we need to be aware of the carousel view item height so we can resize the view.
// Changing this property DOES NOT affect the actual sie of the carousel view items, though!
// This is decided in the carousel view data sources' method:
// carousel:(iCarousel *)carousel viewForItemAtIndex:(NSInteger)index reusingView:(NSView *)view
@property (readwrite, nonatomic) CGFloat itemHeight;

- (void)setup;

@end

NS_ASSUME_NONNULL_END
