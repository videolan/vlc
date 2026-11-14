/*****************************************************************************
 * VLCLibraryMusicCarouselDataSource.m: MacOS X interface module
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

#import "VLCLibraryMusicCarouselDataSource.h"

#import "library/VLCLibraryCarouselCollectionViewItem.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "main/VLCMain.h"

NSNotificationName const VLCLibraryMusicCarouselDataSourceReloaded = @"VLCLibraryMusicCarouselDataSourceReloaded";

@implementation VLCLibraryMusicCarouselDataSource
{
    VLCMediaLibraryParentGroupType _parentType;
    VLCLibraryMusicCarouselItemsProvider _itemsProvider;
    NSArray<NSNotificationName> *_resetNotifications;
    NSString *_itemIdentifier;
    NSArray<id<VLCMediaLibraryItemProtocol>> *_items;
    NSUInteger _reloadGeneration;
    BOOL _connected;
    BOOL _loading;
}

- (instancetype)initWithParentType:(VLCMediaLibraryParentGroupType)parentType
                     itemsProvider:(VLCLibraryMusicCarouselItemsProvider)itemsProvider
                resetNotifications:(NSArray<NSNotificationName> *)resetNotifications
                    itemIdentifier:(NSString *)itemIdentifier
{
    self = [super init];
    if (self) {
        _parentType = parentType;
        _itemsProvider = [itemsProvider copy];
        _resetNotifications = [resetNotifications arrayByAddingObject:VLCLibraryModelAllCachesDropped];
        _itemIdentifier = itemIdentifier;
        _items = @[];
    }
    return self;
}

- (void)connect
{
    if (_connected) {
        return;
    }
    _connected = YES;

    for (NSNotificationName const name in _resetNotifications) {
        [NSNotificationCenter.defaultCenter addObserver:self
                                               selector:@selector(modelChanged:)
                                                   name:name
                                                 object:nil];
    }
    [self reloadData];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    _connected = NO;
    ++_reloadGeneration;
}

- (void)modelChanged:(NSNotification *)notification
{
    [self reloadData];
}

- (void)reloadData
{
    if (!NSThread.isMainThread) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self reloadData];
        });
        return;
    }

    if (!_connected) {
        return;
    }

    const NSUInteger generation = ++_reloadGeneration;
    if (_loading) {
        return;
    }
    _loading = YES;

    VLCLibraryModel * const model = VLCMain.sharedInstance.libraryController.libraryModel;
    VLCLibraryMusicCarouselItemsProvider const provider = _itemsProvider;
    __weak typeof(self) weakSelf = self;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSArray<id<VLCMediaLibraryItemProtocol>> * const items = model != nil ? [provider(model) copy] : @[];
        dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf applyItems:items generation:generation];
        });
    });
}

- (void)applyItems:(NSArray<id<VLCMediaLibraryItemProtocol>> *)items generation:(NSUInteger)generation
{
    _loading = NO;
    if (generation != _reloadGeneration) {
        // Coalesce notifications received during the previous query into one reload.
        [self reloadData];
        return;
    }
    if (items == nil) {
        return;
    }

    _items = items;
    [self.collectionView reloadData];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryMusicCarouselDataSourceReloaded
                                                      object:self];
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    return _items.count;
}

- (VLCLibraryRepresentedItem *)representedItemAtIndexPath:(NSIndexPath *)indexPath
{
    id<VLCMediaLibraryItemProtocol> const item = [self libraryItemAtIndexPath:indexPath
                                                            forCollectionView:self.collectionView];
    if (item == nil) {
        return nil;
    }
    return [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:_parentType];
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCarouselCollectionViewItem * const viewItem =
        (VLCLibraryCarouselCollectionViewItem *)[collectionView
            makeItemWithIdentifier:_itemIdentifier
                      forIndexPath:indexPath];
    viewItem.representedItem = [self representedItemAtIndexPath:indexPath];
    return viewItem;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    if (indexPath == nil || indexPath.section != 0 || (NSUInteger)indexPath.item >= _items.count) {
        return nil;
    }
    return _items[indexPath.item];
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    const NSUInteger index = [_items indexOfObjectPassingTest:^BOOL(
        id<VLCMediaLibraryItemProtocol> item,
        NSUInteger __unused itemIndex,
        BOOL * __unused stop
    ) {
        return item.class == libraryItem.class && item.libraryID == libraryItem.libraryID;
    }];
    return index == NSNotFound ? nil : [NSIndexPath indexPathForItem:index inSection:0];
}

- (NSArray<VLCLibraryRepresentedItem *> *)representedItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
                                                     forCollectionView:(NSCollectionView *)collectionView
{
    NSMutableArray<VLCLibraryRepresentedItem *> * const representedItems =
        [NSMutableArray arrayWithCapacity:indexPaths.count];
    for (NSIndexPath * const indexPath in indexPaths) {
        VLCLibraryRepresentedItem * const representedItem = [self representedItemAtIndexPath:indexPath];
        if (representedItem != nil) {
            [representedItems addObject:representedItem];
        }
    }
    return representedItems;
}

- (NSString *)supplementaryDetailViewKind
{
    return @"";
}

@end
