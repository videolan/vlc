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
+ (const CGFloat)largeSpacing;
+ (const CGFloat)mediumSpacing;
+ (const CGFloat)smallSpacing;

+ (const CGFloat)scrollBarSmallSideSize;

+ (const CGFloat)largeTableViewRowHeight;
+ (const CGFloat)mediumTableViewRowHeight;
+ (const CGFloat)smallTableViewRowHeight;

+ (const CGFloat)mediumDetailSupplementaryViewCollectionViewHeight;
+ (const CGFloat)largeDetailSupplementaryViewCollectionViewHeight;

+ (const CGFloat)dynamicCollectionViewItemMinimumWidth;
+ (const CGFloat)dynamicCollectionViewItemMaximumWidth;

+ (const CGFloat)collectionViewItemSpacing;
+ (const NSEdgeInsets)collectionViewSectionInsets;

+ (const NSSize)adjustedCollectionViewItemSizeForCollectionView:(NSCollectionView *)collectionView
                                                     withLayout:(VLCLibraryCollectionViewFlowLayout *)collectionViewLayout
                                           withItemsAspectRatio:(VLCLibraryCollectionViewItemAspectRatio)itemsAspectRatio;

+ (const NSEdgeInsets)libraryViewScrollViewContentInsets;
+ (const NSEdgeInsets)libraryViewScrollViewScrollerInsets;

+ (const CGFloat)controlsFadeAnimationDuration;

+ (const CGFloat)librarySplitViewMainViewMinimumWidth;

@end

NS_ASSUME_NONNULL_END
