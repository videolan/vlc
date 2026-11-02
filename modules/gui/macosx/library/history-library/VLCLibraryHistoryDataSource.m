/*****************************************************************************
 * VLCLibraryHistoryDataSource.m: MacOS X interface module
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

#import "VLCLibraryHistoryDataSource.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

@interface VLCLibraryHistoryDataSource ()
{
    BOOL _connected;
    NSCalendar *_calendar;
    NSDate *_now;
}
@end

@implementation VLCLibraryHistoryDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        _sections = @[];
    }
    return self;
}

- (void)dealloc
{
    [self disconnect];
}

- (void)setLibraryModel:(VLCLibraryModel *)libraryModel
{
    const BOOL reconnect = _connected;
    [self disconnect];
    _libraryModel = libraryModel;
    if (reconnect) {
        [self connect];
    }
}

- (void)connect
{
    if (_connected) {
        return;
    }
    _connected = YES;
    NSNotificationCenter * const center = NSNotificationCenter.defaultCenter;
    [center addObserver:self
               selector:@selector(libraryModelChanged:)
                   name:VLCLibraryModelHistoryMediaListReset
                 object:self.libraryModel];
    [center addObserver:self
               selector:@selector(libraryModelChanged:)
                   name:VLCLibraryModelAllCachesDropped
                 object:self.libraryModel];
    [self reloadData];
}

- (void)disconnect
{
    _connected = NO;
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)libraryModelChanged:(NSNotification *)notification
{
    [self reloadData];
}

- (void)reloadData
{
    NSAssert(NSThread.isMainThread, @"History collection updates must run on the main thread");
    NSCollectionView * const collectionView = self.collectionView;
    NSAssert(collectionView == nil || collectionView.dataSource == self,
             @"Cannot reload a collection view with a different data source");

    NSMutableSet<NSNumber *> * const selectedIDs = NSMutableSet.set;
    for (NSIndexPath * const indexPath in collectionView.selectionIndexPaths) {
        VLCMediaLibraryMediaItem * const item =
            (VLCMediaLibraryMediaItem *)[self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        if (item != nil) {
            [selectedIDs addObject:@(item.libraryID)];
        }
    }
    [collectionView deselectItemsAtIndexPaths:collectionView.selectionIndexPaths];
    if ([collectionView.collectionViewLayout isKindOfClass:VLCLibraryCollectionViewFlowLayout.class]) {
        [(VLCLibraryCollectionViewFlowLayout *)collectionView.collectionViewLayout resetLayout];
    }

    _calendar = NSCalendar.currentCalendar.copy;
    _now = NSDate.date;
    _sections = [VLCLibraryHistorySection sectionsForMediaItems:self.libraryModel.listOfHistoryMedia ?: @[]
                                                       calendar:_calendar];
    [collectionView reloadData];

    NSMutableSet<NSIndexPath *> * const selectedPaths = NSMutableSet.set;
    [_sections enumerateObjectsUsingBlock:^(VLCLibraryHistorySection *section,
                                            NSUInteger sectionIndex,
                                            BOOL * const __unused stop) {
        [section.mediaItems enumerateObjectsUsingBlock:^(VLCMediaLibraryMediaItem *item,
                                                         NSUInteger itemIndex,
                                                         BOOL * const __unused stop) {
            if ([selectedIDs containsObject:@(item.libraryID)]) {
                [selectedPaths addObject:[NSIndexPath indexPathForItem:itemIndex inSection:sectionIndex]];
            }
        }];
    }];
    [collectionView selectItemsAtIndexPaths:selectedPaths scrollPosition:NSCollectionViewScrollPositionNone];
    if (self.dataChangedHandler != nil) {
        self.dataChangedHandler();
    }
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return self.sections.count;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
    if (section < 0 || (NSUInteger)section >= self.sections.count) {
        return 0;
    }
    return self.sections[section].mediaItems.count;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    if (indexPath.section < 0 || (NSUInteger)indexPath.section >= self.sections.count) {
        return nil;
    }
    NSArray<VLCMediaLibraryMediaItem *> * const items = self.sections[indexPath.section].mediaItems;
    return indexPath.item >= 0 && (NSUInteger)indexPath.item < items.count ? items[indexPath.item] : nil;
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    for (NSUInteger section = 0; section < self.sections.count; ++section) {
        NSArray<VLCMediaLibraryMediaItem *> * const items = self.sections[section].mediaItems;
        const NSUInteger index = [items indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem *item,
                                                                       NSUInteger __unused index,
                                                                       BOOL * const __unused stop) {
            return item.libraryID == libraryItem.libraryID;
        }];
        if (index != NSNotFound) {
            return [NSIndexPath indexPathForItem:index inSection:section];
        }
    }
    return nil;
}

- (NSArray<VLCLibraryRepresentedItem *> *)representedItemsAtIndexPaths:(NSSet<NSIndexPath *> *const)indexPaths
                                                     forCollectionView:(NSCollectionView *)collectionView
{
    NSMutableArray<VLCLibraryRepresentedItem *> * const items = NSMutableArray.array;
    for (NSIndexPath * const indexPath in [indexPaths.allObjects sortedArrayUsingSelector:@selector(compare:)]) {
        id<VLCMediaLibraryItemProtocol> const item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        if (item != nil) {
            [items addObject:[[VLCLibraryRepresentedItem alloc] initWithItem:item
                                                                  parentType:VLCMediaLibraryParentGroupTypeUnknown]];
        }
    }
    return items.copy;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem * const viewItem =
        [collectionView makeItemWithIdentifier:VLCLibraryCollectionViewItemIdentifier forIndexPath:indexPath];
    id<VLCMediaLibraryItemProtocol> const item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
    viewItem.representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:item
                                                                    parentType:VLCMediaLibraryParentGroupTypeUnknown];
    return viewItem;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind atIndexPath:(NSIndexPath *)indexPath
{
    if ([kind isEqualToString:NSCollectionElementKindSectionHeader]) {
        VLCLibraryCollectionViewSupplementaryElementView * const header =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:VLCLibrarySupplementaryElementViewIdentifier
                                           forIndexPath:indexPath];
        header.stringValue = [self.sections[indexPath.section] titleWithCalendar:_calendar now:_now];
        return header;
    }
    if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView * const detail =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier
                                           forIndexPath:indexPath];
        id<VLCMediaLibraryItemProtocol> const item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        detail.representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:item
                                                                      parentType:VLCMediaLibraryParentGroupTypeUnknown];
        detail.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return detail;
    }
    return nil;
}

- (NSString *)supplementaryDetailViewKind
{
    return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
}

@end
