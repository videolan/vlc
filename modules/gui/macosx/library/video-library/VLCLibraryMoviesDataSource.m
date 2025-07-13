/*****************************************************************************
 * VLCLibraryMoviesDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#import "VLCLibraryMoviesDataSource.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewItem.h"

NSString * const VLCLibraryMoviesDataSourceDisplayedCollectionChangedNotification = @"VLCLibraryMoviesDataSourceDisplayedCollectionChangedNotification";

@implementation VLCLibraryMoviesDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self connect];
    }
    return self;
}

- (void)connect
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelMoviesListReset:)
                               name:VLCLibraryModelListOfMoviesReset
                             object:nil];
    [self reloadData];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)libraryModelMoviesListReset:(NSNotification *)notification
{
    [self reloadData];
}

#pragma mark - NSCollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return 1;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
    return self.libraryModel.numberOfMovies;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem * const item = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    const id<VLCMediaLibraryItemProtocol> movie = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:movie parentType:self.currentParentType];
    item.representedItem = representedItem;
    return item;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView viewForSupplementaryElementOfKind:(NSString *)kind atIndexPath:(NSIndexPath *)indexPath
{
    if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView * const detailView =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                                           forIndexPath:indexPath];
        const id<VLCMediaLibraryItemProtocol> movie = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:movie parentType:self.currentParentType];
        detailView.representedItem = representedItem;
        detailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return detailView;
    }
    return nil;
}

#pragma mark - VLCLibraryCollectionViewDataSource

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath forCollectionView:(NSCollectionView *)collectionView
{
    return self.libraryModel.listOfMovies[indexPath.item];
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    const NSUInteger idx = [self.libraryModel.listOfMovies indexOfObject:libraryItem];
    if (idx == NSNotFound) return nil;
    return [NSIndexPath indexPathForItem:idx inSection:0];
}

- (NSArray<VLCLibraryRepresentedItem *> *)representedItemsAtIndexPaths:(NSSet<NSIndexPath *> *const)indexPaths forCollectionView:(NSCollectionView *)collectionView
{
    NSMutableArray * const items = [NSMutableArray arrayWithCapacity:indexPaths.count];
    for (NSIndexPath * const indexPath in indexPaths) {
        const id<VLCMediaLibraryItemProtocol> item = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        if (item) {
            [items addObject:[[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:self.currentParentType]];
        }
    }
    return items;
}

- (NSString *)supplementaryDetailViewKind
{
    return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
}

- (void)reloadData
{
    // Notify all attached views to reload their data
    if (self.collectionView) {
        [self.collectionView reloadData];
    }
    if (self.tableView) {
        [self.tableView reloadData];
    }
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryMoviesDataSourceDisplayedCollectionChangedNotification
                                                      object:self
                                                    userInfo:nil];
}

#pragma mark - VLCLibraryTableViewDataSource

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return self.libraryModel.numberOfMovies;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row forTableView:(nullable NSTableView *)tableView
{
    return self.libraryModel.listOfMovies[row];
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    return [self.libraryModel.listOfMovies indexOfObject:libraryItem];
}

- (VLCMediaLibraryParentGroupType)currentParentType
{
    return VLCMediaLibraryParentGroupTypeVideoLibrary;
}

@end
