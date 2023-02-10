/*****************************************************************************
 * VLCLibraryUIUnits.m: MacOS X interface module
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

#import "VLCLibraryUIUnits.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"

@implementation VLCLibraryUIUnits

+ (const CGFloat)largeSpacing
{
    return 20;
}

+ (const CGFloat)mediumSpacing
{
    return 10;
}

+ (const CGFloat)smallSpacing
{
    return 5;
}

+ (const CGFloat)scrollBarSmallSideSize
{
    return 16;
}

+ (const CGFloat)largeTableViewRowHeight
{
    return 100;
}

+ (const CGFloat)mediumTableViewRowHeight
{
    return 50;
}

+ (const CGFloat)smallTableViewRowHeight
{
    return 25;
}

+ (const CGFloat)mediumDetailSupplementaryViewCollectionViewHeight
{
    return 300;
}

+ (const CGFloat)largeDetailSupplementaryViewCollectionViewHeight
{
    return 500;
}

+ (const CGFloat)dynamicCollectionViewItemMinimumWidth
{
    return 180;
}

+ (const CGFloat)dynamicCollectionViewItemMaximumWidth
{
    return 280;
}

+ (const NSSize)adjustedCollectionViewItemSizeForCollectionView:(NSCollectionView *)collectionView
                                                     withLayout:(VLCLibraryCollectionViewFlowLayout *)collectionViewLayout
                                           withItemsAspectRatio:(VLCLibraryCollectionViewItemAspectRatio)itemsAspectRatio
{
    static uint numItemsInRow = 5;

    NSSize itemSize = [self itemSizeForCollectionView:collectionView
                                           withLayout:collectionViewLayout
                                 withItemsAspectRatio:itemsAspectRatio
                               withNumberOfItemsInRow:numItemsInRow];

    while (itemSize.width > [VLCLibraryUIUnits dynamicCollectionViewItemMaximumWidth]) {
        ++numItemsInRow;
        itemSize = [self itemSizeForCollectionView:collectionView
                                        withLayout:collectionViewLayout
                              withItemsAspectRatio:itemsAspectRatio
                            withNumberOfItemsInRow:numItemsInRow];
    }
    while (itemSize.width < [VLCLibraryUIUnits dynamicCollectionViewItemMinimumWidth]) {
        --numItemsInRow;
        itemSize = [self itemSizeForCollectionView:collectionView
                                        withLayout:collectionViewLayout
                              withItemsAspectRatio:itemsAspectRatio
                            withNumberOfItemsInRow:numItemsInRow];
    }

    return itemSize;
}

+ (const NSSize)itemSizeForCollectionView:(NSCollectionView *)collectionView
                               withLayout:(VLCLibraryCollectionViewFlowLayout *)collectionViewLayout
                     withItemsAspectRatio:(VLCLibraryCollectionViewItemAspectRatio)itemsAspectRatio
                   withNumberOfItemsInRow:(uint)numItemsInRow
{
    NSParameterAssert(numItemsInRow > 0);
    NSParameterAssert(collectionView);
    NSParameterAssert(collectionViewLayout);

    const NSEdgeInsets sectionInsets = collectionViewLayout.sectionInset;
    const CGFloat interItemSpacing = collectionViewLayout.minimumInteritemSpacing;

    const CGFloat rowOfItemsWidth = collectionView.bounds.size.width -
                                    (sectionInsets.left +
                                     sectionInsets.right +
                                     (interItemSpacing * (numItemsInRow - 1)) +
                                     1);

    const CGFloat itemWidth = rowOfItemsWidth / numItemsInRow;
    const CGFloat itemHeight = itemsAspectRatio == VLCLibraryCollectionViewItemAspectRatioDefaultItem ?
        itemWidth + [VLCLibraryCollectionViewItem bottomTextViewsHeight] :
        (itemWidth * [VLCLibraryCollectionViewItem videoHeightAspectRatioMultiplier]) + [VLCLibraryCollectionViewItem bottomTextViewsHeight];

    return NSMakeSize(itemWidth, itemHeight);
}

+ (const NSEdgeInsets)libraryViewScrollViewContentInsets
{
    return NSEdgeInsetsMake([VLCLibraryUIUnits mediumSpacing],
                            [VLCLibraryUIUnits mediumSpacing],
                            [VLCLibraryUIUnits mediumSpacing],
                            [VLCLibraryUIUnits mediumSpacing]);
}

+ (const NSEdgeInsets)libraryViewScrollViewScrollerInsets
{
    const NSEdgeInsets contentInsets = [self libraryViewScrollViewContentInsets];
    return NSEdgeInsetsMake(-contentInsets.top,
                            -contentInsets.left,
                            -contentInsets.bottom,
                            -contentInsets.right);
}

@end
