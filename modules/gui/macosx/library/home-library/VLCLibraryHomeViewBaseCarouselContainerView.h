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

#import "library/VLCLibraryCarouselView.h"
#import "library/VLCLibraryCollectionViewDataSource.h"

NS_ASSUME_NONNULL_BEGIN

@class VLCSubScrollView;
@class VLCLibraryCollectionViewDelegate;
@class VLCLibraryCollectionViewFlowLayout;
@interface VLCLibraryHomeViewBaseCarouselContainerView : NSView<VLCLibraryHomeViewContainerView>

@property (readonly) NSTextField *titleView;
@property (readonly) VLCSubScrollView *scrollView;
@property (readonly) VLCLibraryCarouselView *collectionView;
@property (readonly) VLCLibraryCollectionViewDelegate *collectionViewDelegate;
@property (readonly) VLCLibraryCollectionViewFlowLayout *collectionViewLayout;
@property (readonly) NSButton *leftButton;
@property (readonly) NSButton *rightButton;
@property (readonly) NSObject<VLCLibraryCollectionViewDataSource> *dataSource;
@property (readwrite, nonatomic) CGFloat itemHeight;

- (void)setup;

@end

NS_ASSUME_NONNULL_END
