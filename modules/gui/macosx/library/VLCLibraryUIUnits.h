/*****************************************************************************
 * VLCLibraryUIUnits.h: MacOS X interface module
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

@class VLCLibraryCollectionViewFlowLayout;

typedef NS_ENUM(NSUInteger, VLCLibraryCollectionViewItemAspectRatio) {
    VLCLibraryCollectionViewItemAspectRatioDefaultItem = 0,
    VLCLibraryCollectionViewItemAspectRatioVideoItem,
};

NS_ASSUME_NONNULL_BEGIN

@interface VLCLibraryUIUnits : NSObject

// Note that these values are not necessarily linked to the layout defined in the .xib files.
// If the spacing in the layout is changed you will want to change these values too.
@property (class, readonly) const CGFloat largeSpacing;
@property (class, readonly) const CGFloat mediumSpacing;
@property (class, readonly) const CGFloat smallSpacing;

@property (class, readonly) const CGFloat scrollBarSmallSideSize;

@property (class, readonly) const CGFloat largeTableViewRowHeight;
@property (class, readonly) const CGFloat mediumTableViewRowHeight;
@property (class, readonly) const CGFloat smallTableViewRowHeight;

@property (class, readonly) const CGFloat mediumDetailSupplementaryViewCollectionViewWidth;
@property (class, readonly) const CGFloat mediumDetailSupplementaryViewCollectionViewHeight;
@property (class, readonly) const CGFloat largeDetailSupplementaryViewCollectionViewWidth;
@property (class, readonly) const CGFloat largeDetailSupplementaryViewCollectionViewHeight;

@property (class, readonly) const CGFloat dynamicCollectionViewItemMinimumWidth;
@property (class, readonly) const CGFloat dynamicCollectionViewItemMaximumWidth;

@property (class, readonly) const CGFloat collectionViewItemSpacing;
@property (class, readonly) const NSEdgeInsets collectionViewSectionInsets;

@property (class, readonly) const CGFloat carouselViewVideoItemViewWidth;
@property (class, readonly) const CGFloat carouselViewItemViewHeight;

@property (class, readonly) const NSEdgeInsets libraryViewScrollViewContentInsets;
@property (class, readonly) const NSEdgeInsets libraryViewScrollViewScrollerInsets;

@property (class, readonly) const CGFloat controlsFadeAnimationDuration;

@property (class, readonly) const CGFloat libraryWindowControlsBarHeight;

@property (class, readonly) const CGFloat librarySplitViewSelectionViewDefaultWidth;
@property (class, readonly) const CGFloat librarySplitViewMainViewMinimumWidth;
@property (class, readonly) const CGFloat libraryWindowNavSidebarMaxWidth;
@property (class, readonly) const CGFloat libraryWindowPlaylistSidebarMaxWidth;

@property (class, readonly) const CGFloat sliderTickThickness;

+ (const NSSize)adjustedCollectionViewItemSizeForCollectionView:(NSCollectionView *)collectionView
                                                     withLayout:(VLCLibraryCollectionViewFlowLayout *)collectionViewLayout
                                           withItemsAspectRatio:(VLCLibraryCollectionViewItemAspectRatio)itemsAspectRatio;

@end

NS_ASSUME_NONNULL_END
