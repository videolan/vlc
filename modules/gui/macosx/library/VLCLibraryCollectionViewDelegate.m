/*****************************************************************************
 * VLCLibraryCollectionViewDelegate.m: MacOS X interface module
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

#import "VLCLibraryCollectionViewDelegate.h"

#import "VLCLibraryCollectionViewDataSource.h"
#import "VLCLibraryCollectionViewFlowLayout.h"
#import "VLCLibraryDataTypes.h"

@implementation VLCLibraryCollectionViewDelegate

- (instancetype)init
{
    self = [super init];
    if (self) {
        _dynamicItemSizing = YES;
    }
    return self;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath *indexPath = indexPaths.anyObject;
    if (!indexPath) {
        return;
    }

    VLCLibraryCollectionViewFlowLayout *collectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout*)collectionView.collectionViewLayout;
    if(collectionViewFlowLayout) {
        [collectionViewFlowLayout expandDetailSectionAtIndex:indexPath];
    }
}

- (void)collectionView:(NSCollectionView *)collectionView didDeselectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath *indexPath = indexPaths.anyObject;
    if (!indexPath) {
        return;
    }

    VLCLibraryCollectionViewFlowLayout *collectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout*)collectionView.collectionViewLayout;
    if (collectionViewFlowLayout) {
        [collectionViewFlowLayout collapseDetailSectionAtIndex:indexPath];
    }
}

- (NSSize)collectionView:(NSCollectionView *)collectionView
                  layout:(NSCollectionViewLayout *)collectionViewLayout
  sizeForItemAtIndexPath:(NSIndexPath *)indexPath
{
    if (!_dynamicItemSizing) {
        return CGSizeMake(214., 260.);
    }
    
    VLCLibraryCollectionViewFlowLayout *collectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout*)collectionViewLayout;
    if (collectionViewLayout) {
        VLCLibraryCollectionViewFlowLayout *collectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout*)collectionViewLayout;
        return [self adjustedItemSizeForCollectionView:collectionView
                                            withLayout:collectionViewFlowLayout];
    }

    return NSZeroSize;
}

- (NSSize)adjustedItemSizeForCollectionView:(NSCollectionView *)collectionView
                                 withLayout:(VLCLibraryCollectionViewFlowLayout *)collectionViewLayout
{
    static const CGFloat maxItemWidth = 280;
    static const CGFloat minItemWidth = 180;

    static uint numItemsInRow = 5;

    NSSize itemSize = [self itemSizeForCollectionView:collectionView
                                           withLayout:collectionViewLayout
                               withNumberOfItemsInRow:numItemsInRow];

    while (itemSize.width > maxItemWidth) {
        ++numItemsInRow;
        itemSize = [self itemSizeForCollectionView:collectionView
                                        withLayout:collectionViewLayout
                            withNumberOfItemsInRow:numItemsInRow];
    }
    while (itemSize.width < minItemWidth) {
        --numItemsInRow;
        itemSize = [self itemSizeForCollectionView:collectionView
                                        withLayout:collectionViewLayout
                            withNumberOfItemsInRow:numItemsInRow];
    }

    return itemSize;
}

- (NSSize)itemSizeForCollectionView:(NSCollectionView *)collectionView
                        withLayout:(VLCLibraryCollectionViewFlowLayout *)collectionViewLayout
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
    return NSMakeSize(itemWidth, itemWidth + 46); // Text fields height needed
}

- (BOOL)collectionView:(NSCollectionView *)collectionView
canDragItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
             withEvent:(NSEvent *)event
{
    return YES;
}

- (BOOL)collectionView:(NSCollectionView *)collectionView
writeItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
          toPasteboard:(NSPasteboard *)pasteboard
{
    if (![collectionView.dataSource conformsToProtocol:@protocol(VLCLibraryCollectionViewDataSource)]) {
        return NO;
    }

    NSObject<VLCLibraryCollectionViewDataSource> *vlcDataSource = (NSObject<VLCLibraryCollectionViewDataSource>*)collectionView.dataSource;

    NSUInteger numberOfIndexPaths = indexPaths.count;
    NSMutableArray *encodedLibraryItemsArray = [NSMutableArray arrayWithCapacity:numberOfIndexPaths];
    NSMutableArray *filePathsArray = [NSMutableArray arrayWithCapacity:numberOfIndexPaths];

    for (NSIndexPath *indexPath in indexPaths) {

        id<VLCMediaLibraryItemProtocol> libraryItem = [vlcDataSource libraryItemAtIndexPath:indexPath
                                                                          forCollectionView:collectionView];

        VLCMediaLibraryMediaItem *mediaItem = libraryItem.firstMediaItem;
        [encodedLibraryItemsArray addObject:mediaItem];

        VLCMediaLibraryFile *file = mediaItem.files.firstObject;
        if (file) {
            NSURL *url = [NSURL URLWithString:file.MRL];
            [filePathsArray addObject:url.path];
        }
    }

    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:encodedLibraryItemsArray];
    [pasteboard declareTypes:@[VLCMediaLibraryMediaItemPasteboardType, NSFilenamesPboardType] owner:self];
    [pasteboard setPropertyList:filePathsArray forType:NSFilenamesPboardType];
    [pasteboard setData:data forType:VLCMediaLibraryMediaItemPasteboardType];

    return YES;
}

@end
