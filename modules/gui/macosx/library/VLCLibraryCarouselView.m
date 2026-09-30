/*****************************************************************************
 * VLCLibraryCarouselView.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Serhii Bykov <esphynox@gmail.com>
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

#import "VLCLibraryCarouselView.h"

#import "VLCLibraryCarouselCollectionViewItem.h"
#import "VLCLibraryCollectionViewFlowLayout.h"

@implementation VLCLibraryCarouselView

#pragma mark - NSCollectionView overrides

- (void)setCollectionViewLayout:(NSCollectionViewLayout *)collectionViewLayout
{
    [super setCollectionViewLayout:collectionViewLayout];

    if ([collectionViewLayout isKindOfClass:VLCLibraryCollectionViewFlowLayout.class]) {
        VLCLibraryCollectionViewFlowLayout * const layout =
            (VLCLibraryCollectionViewFlowLayout *)collectionViewLayout;
        self.leadingItemInset = layout.sectionInset.left;
    }
}

#pragma mark - VLCCarouselView overrides

- (void)activateItemAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCarouselCollectionViewItem * const item =
        (VLCLibraryCarouselCollectionViewItem *)[self itemAtIndexPath:indexPath];
    [item playRepresentedItem];
}

- (BOOL)requestContextMenuForItemAtIndexPath:(NSIndexPath *)indexPath event:(NSEvent *)event
{
    VLCLibraryCarouselCollectionViewItem * const item =
        (VLCLibraryCarouselCollectionViewItem *)[self itemAtIndexPath:indexPath];
    if (item == nil) {
        return NO;
    }

    [item openContextMenu:event];
    return YES;
}

@end
