/*****************************************************************************
 * VLCUIUnits.m: MacOS X interface module
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

#import "VLCUIUnits.h"

#import "extensions/NSWindow+VLCAdditions.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryWindow.h"

#import "main/VLCMain.h"

#import "views/VLCBottomBarView.h"

#import "windows/controlsbar/VLCControlsBarCommon.h"

NSString * const VLCLibraryCollectionViewItemAdjustmentKey = @"VLCLibraryCollectionViewItemAdjustmentKey";

@implementation VLCCollectionViewItemSizing
@end

@implementation VLCUIUnits

+ (const CGFloat)extraLargeSpacing
{
    return self.largeSpacing + self.mediumSpacing;
}

+ (const CGFloat)largeSpacing
{
    return 16;
}

+ (const CGFloat)mediumSpacing
{
    return 8;
}

+ (const CGFloat)smallSpacing
{
    return 4;
}

+ (const CGFloat)extraSmallSpacing
{
    return self.smallSpacing / 2.;
}

+ (const CGFloat)compactControlPadding
{
    return self.smallSpacing + self.extraSmallSpacing;
}

+ (const CGFloat)cornerRadius
{
    return 4.;
}

+ (const CGFloat)mediumCornerRadius
{
    return 10.;
}

+ (const CGFloat)largeCornerRadius
{
    return 12.;
}

+ (const CGFloat)borderThickness
{
    return 1.;
}

+ (const CGFloat)dashedBorderDashLength
{
    return self.smallSpacing + self.extraSmallSpacing;
}

+ (const CGFloat)dashedBorderDashSpacing
{
    return self.smallSpacing + self.borderThickness;
}

+ (const CGFloat)disabledOpacity
{
    return .5;
}

+ (const CGFloat)smallIconSize
{
    return self.largeSpacing;
}

+ (const CGFloat)mediumIconSize
{
    return self.largeSpacing + self.smallSpacing;
}

+ (const CGFloat)largeIconSize
{
    return 26.;
}

+ (const CGFloat)largeIconContainerSize
{
    return 40.;
}

+ (const CGFloat)largeIllustrationSize
{
    return self.largeSpacing * 4.;
}

+ (const CGFloat)largeActionButtonHeight
{
    return 56.;
}

+ (const CGFloat)actionButtonCompactWidthThreshold
{
    return 130.;
}

+ (const CGFloat)actionButtonNarrowWidthThreshold
{
    return 160.;
}

+ (const NSUInteger)actionButtonMaximumTitleLines
{
    return 2;
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

+ (const CGFloat)videoLibraryTableViewRowHeight
{
    return 42;
}

+ (const CGFloat)mediumDetailSupplementaryViewCollectionViewWidth
{
    return 600;
}

+ (const CGFloat)mediumDetailSupplementaryViewCollectionViewHeight
{
    return 300;
}

+ (const CGFloat)largeDetailSupplementaryViewCollectionViewWidth
{
    return 800;
}

+ (const CGFloat)largeDetailSupplementaryViewCollectionViewHeight
{
    return 500;
}

+ (const CGFloat)mediaItemCollectionViewItemFooterTextHeight
{
    return VLCUIUnits.smallSpacing +
           16 +
           VLCUIUnits.smallSpacing +
           16 +
           VLCUIUnits.smallSpacing;
}

+ (const CGFloat)defaultMediaItemCollectionViewItemWidth
{
    return 214;
}

+ (const NSSize)defaultMediaItemCollectionViewItemSize
{
    const CGFloat width = VLCUIUnits.defaultMediaItemCollectionViewItemWidth;
    return CGSizeMake(width, width + VLCUIUnits.mediaItemCollectionViewItemFooterTextHeight);
}

+ (const NSSize)defaultVideoItemCollectionViewItemSize
{
    const CGFloat width = VLCUIUnits.defaultMediaItemCollectionViewItemWidth;
    const CGFloat imageViewHeight = width * VLCUIUnits.videoItemCollectionViewImageViewAspectRatioMultiplier;
    return CGSizeMake(width, imageViewHeight + VLCUIUnits.mediaItemCollectionViewItemFooterTextHeight);
}

+ (const NSSize)libraryCompactCardSize
{
    return NSMakeSize(200., 72.);
}

+ (const CGFloat)videoItemCollectionViewImageViewAspectRatioMultiplier
{
    return 10. / 16.;
}


+ (const CGFloat)dynamicCollectionViewItemMinimumWidth
{
    return 150;
}

+ (const CGFloat)dynamicCollectionViewItemMaximumWidth
{
    return 200;
}

+ (const CGFloat)collectionViewItemMinimumWidth
{
    return VLCUIUnits.dynamicCollectionViewItemMinimumWidth;
}

+ (const unsigned short)collectionViewMinItemsInRow
{
    return 2;
}

+ (const CGFloat)collectionViewItemSpacing
{
    return [self largeSpacing];
}

+ (const NSEdgeInsets)collectionViewSectionInsets
{
    const CGFloat inset = [self largeSpacing];
    return NSEdgeInsetsMake(inset, inset, inset, inset);
}

+ (const CGFloat)libraryHomeActionsMinimumHeight
{
    return self.largeActionButtonHeight * 2. + self.mediumSpacing + self.largeSpacing * 2.;
}

+ (const CGFloat)libraryHomeActionsNarrowWidthThreshold
{
    return 600.;
}

+ (const CGFloat)libraryHomeActionsGridWidthMultiplier
{
    return 0.52;
}

+ (const CGFloat)libraryHomeActionsLogoWidthMultiplier
{
    return 0.22;
}

+ (const NSSize)adjustedCollectionViewItemSizeForCollectionView:(NSCollectionView *)collectionView
                                                     withLayout:(VLCLibraryCollectionViewFlowLayout *)collectionViewLayout
                                           withItemsAspectRatio:(VLCLibraryCollectionViewItemAspectRatio)itemsAspectRatio
{
    return [self adjustedItemSizingForCollectionView:collectionView
                                          withLayout:collectionViewLayout
                                withItemsAspectRatio:itemsAspectRatio].itemSize;
}

+ (VLCCollectionViewItemSizing *)adjustedItemSizingForCollectionView:(NSCollectionView *)collectionView
                                                          withLayout:(VLCLibraryCollectionViewFlowLayout *)collectionViewLayout
                                                withItemsAspectRatio:(VLCLibraryCollectionViewItemAspectRatio)itemsAspectRatio
{
    const NSEdgeInsets sectionInsets = collectionViewLayout.sectionInset;
    const CGFloat interItemSpacing = collectionViewLayout.minimumInteritemSpacing;
    const CGFloat availableWidth =
        collectionView.bounds.size.width - (sectionInsets.left + sectionInsets.right);
    const float maxPossibleNumItemsInRow =
        floor(availableWidth / (VLCUIUnits.dynamicCollectionViewItemMinimumWidth + interItemSpacing));
    const float minPossibleNumItemsInRow =
        MIN(ceil(availableWidth / (VLCUIUnits.dynamicCollectionViewItemMaximumWidth + interItemSpacing)), maxPossibleNumItemsInRow);

    const NSInteger adjustment =
        [NSUserDefaults.standardUserDefaults integerForKey:VLCLibraryCollectionViewItemAdjustmentKey];
    const uint midPossibleNumItemsInRow =
        round((minPossibleNumItemsInRow + maxPossibleNumItemsInRow) / 2.0);

    const int unclampedNumItemsInRow = midPossibleNumItemsInRow + adjustment;
    const uint numItemsInRow = MAX(unclampedNumItemsInRow, VLCUIUnits.collectionViewMinItemsInRow);
    const NSSize itemSize = [self itemSizeForCollectionView:collectionView
                                                 withLayout:collectionViewLayout
                                       withItemsAspectRatio:itemsAspectRatio
                                     withNumberOfItemsInRow:numItemsInRow];

    VLCCollectionViewItemSizing * const itemSizing = [[VLCCollectionViewItemSizing alloc] init];
    itemSizing.itemSize = itemSize;
    itemSizing.rowItemCount = numItemsInRow;
    itemSizing.unclampedRowItemCount = unclampedNumItemsInRow;
    return itemSizing;
}

+ (const NSSize)itemSizeForCollectionView:(NSCollectionView *)collectionView
                               withLayout:(VLCLibraryCollectionViewFlowLayout *)collectionViewLayout
                     withItemsAspectRatio:(VLCLibraryCollectionViewItemAspectRatio)itemsAspectRatio
                   withNumberOfItemsInRow:(uint)numItemsInRow
{
    NSParameterAssert(numItemsInRow > 0 && collectionView && collectionViewLayout);

    const NSEdgeInsets sectionInsets = collectionViewLayout.sectionInset;
    const CGFloat interItemSpacing = collectionViewLayout.minimumInteritemSpacing;

    const CGFloat rowOfItemsWidth = collectionView.bounds.size.width -
                                    (sectionInsets.left +
                                     sectionInsets.right +
                                     (interItemSpacing * (numItemsInRow - 1)) +
                                     1);

    const CGFloat itemWidth = MAX(rowOfItemsWidth / numItemsInRow, 1);
    const CGFloat itemHeight = itemsAspectRatio == VLCLibraryCollectionViewItemAspectRatioDefaultItem 
        ? itemWidth + VLCUIUnits.mediaItemCollectionViewItemFooterTextHeight
        : (itemWidth * VLCUIUnits.videoItemCollectionViewImageViewAspectRatioMultiplier) 
            + VLCUIUnits.mediaItemCollectionViewItemFooterTextHeight;

    return NSMakeSize(itemWidth, itemHeight);
}

+ (const CGFloat)carouselViewVideoItemViewWidth
{
    return self.carouselViewItemViewHeight / 9 * 16;
}

+ (const CGFloat)carouselViewItemViewHeight
{
    return 180;
}

+ (const CGFloat)libraryWindowContentSafeTopInset
{
    VLCLibraryWindow * const libraryWindow = VLCMain.sharedInstance.libraryWindow;
    if (@available(macOS 11.0, *)) {
        return libraryWindow.contentView.safeAreaInsets.top;
    }

    if (libraryWindow.styleMask & NSWindowStyleMaskFullSizeContentView) {
        return libraryWindow.contentView.frame.size.height - libraryWindow.contentLayoutRect.size.height;
    }

    return 0;
}

+ (const NSEdgeInsets)libraryViewScrollViewContentInsets
{
    const CGFloat toolbarHeight = VLCUIUnits.libraryWindowContentSafeTopInset;
    const CGFloat controlsBarHeight = VLCUIUnits.libraryWindowControlsBarHeight;
    const CGFloat controlsBarPadding = VLCUIUnits.largeSpacing * 2; // Additional padding for floating controls bar

    return NSEdgeInsetsMake(VLCUIUnits.mediumSpacing + toolbarHeight,
                            VLCUIUnits.mediumSpacing,
                            controlsBarHeight + controlsBarPadding,
                            VLCUIUnits.mediumSpacing);
}

+ (const NSEdgeInsets)libraryViewScrollViewDetailListContentInsets
{
    static NSEdgeInsets contentInsets;
    static dispatch_once_t dispatchOnceToken;
    dispatch_once(&dispatchOnceToken, ^{
        contentInsets = [self libraryViewScrollViewContentInsets];
        contentInsets.top -= VLCUIUnits.largeSpacing;
    });
    return contentInsets;
}

+ (const NSEdgeInsets)libraryViewScrollViewDetailGridContentInsets
{
    static NSEdgeInsets contentInsets;
    static dispatch_once_t dispatchOnceToken;
    dispatch_once(&dispatchOnceToken, ^{
        contentInsets = [self libraryViewScrollViewContentInsets];
        contentInsets.top += VLCUIUnits.mediumSpacing;
    });
    return contentInsets;
}

+ (const NSEdgeInsets)libraryViewScrollViewScrollerInsets
{
    const CGFloat toolbarHeight = VLCUIUnits.libraryWindowContentSafeTopInset;

    const NSEdgeInsets contentInsets = [self libraryViewScrollViewContentInsets];
    return NSEdgeInsetsMake(-contentInsets.top + toolbarHeight,
                            -contentInsets.left,
                            -contentInsets.bottom,
                            -contentInsets.right);
}

+ (const CGFloat)controlsFadeAnimationDuration
{
    return 0.4f;
}

+ (const CGFloat)librarySplitViewSelectionViewMinimumWidth
{
    return 220.;
}

+ (const CGFloat)librarySplitViewSelectionViewDefaultWidth
{
    return 280.;
}

+ (const CGFloat)librarySplitViewMainViewMinimumWidth
{
    return 400.;
}

+ (const CGFloat)libraryWindowControlsBarHeight
{
    return 48.;
}

+ (const CGFloat)libraryWindowControlsBarNarrowWidthThreshold
{
    return 768.;
}

+ (const CGFloat)libraryWindowNavSidebarMinWidth
{
    return 180.;
}

+ (const CGFloat)libraryWindowNavSidebarMaxWidth
{
    return 300.;
}

+ (const CGFloat)libraryWindowPlayQueueSidebarMaxWidth
{
    return 400.;
}

+ (const CGFloat)largePlaybackControlButtonSize
{
    return 64.;
}

+ (const CGFloat)mediumPlaybackControlButtonSize
{
    return 48.;
}

+ (const CGFloat)smallPlaybackControlButtonSize
{
    return 32.;
}

+ (const CGFloat)sliderTickThickness
{
    return 1.;
}

@end
