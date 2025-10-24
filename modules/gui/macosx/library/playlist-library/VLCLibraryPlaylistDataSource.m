/*****************************************************************************
 * VLCLibraryPlaylistDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
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

#import "VLCLibraryPlaylistDataSource.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "main/VLCMain.h"

typedef NS_ENUM(NSInteger, VLCLibraryDataSourceCacheAction) {
    VLCLibraryDataSourceCacheUpdateAction,
    VLCLibraryDataSourceCacheDeleteAction,
};

@interface VLCLibraryPlaylistDataSource ()

@property (readwrite, atomic) NSArray<VLCMediaLibraryPlaylist *> *playlists;

@end

@implementation VLCLibraryPlaylistDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    _libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    [self connect];
    [self reloadData];
}

- (void)connect
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;
    [notificationCenter addObserver:self
                           selector:@selector(playlistsReset:)
                               name:VLCLibraryModelPlaylistAdded
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playlistUpdated:)
                               name:VLCLibraryModelPlaylistUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playlistDeleted:)
                               name:VLCLibraryModelPlaylistDeleted
                             object:nil];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)playlistsReset:(NSNotification *)notification
{
    NSParameterAssert(notification);
    [self reloadData];
}

- (void)playlistUpdated:(NSNotification *)notification
{
    NSParameterAssert(notification);
    VLCMediaLibraryPlaylist * const playlist = (VLCMediaLibraryPlaylist *)notification.object;
    [self cacheAction:VLCLibraryDataSourceCacheUpdateAction onPlaylist:playlist];
}

- (void)playlistDeleted:(NSNotification *)notification
{
    NSParameterAssert(notification);
    NSParameterAssert((NSNumber *)notification.object != nil);

    const int64_t playlistId = [(NSNumber *)notification.object longLongValue];
    const NSInteger playlistIdx =
        [self.playlists indexOfObjectPassingTest:^BOOL(const VLCMediaLibraryPlaylist * const playlist,
                                                       const NSUInteger __unused idx,
                                                       BOOL * const __unused stop) {
            return playlist.libraryID == playlistId;
        }];
    VLCMediaLibraryPlaylist * const playlist = self.playlists[playlistIdx];

    if (playlist != nil) {
        [self cacheAction:VLCLibraryDataSourceCacheDeleteAction onPlaylist:playlist];
    }
}

- (void)reloadData
{
    self.playlists = [self.libraryModel listOfPlaylistsOfType:self.playlistType];
    [self reloadViews];
}

- (void)reloadViews
{
    [self.masterTableView reloadData];
    [self.detailTableView reloadData];
    
    for (NSCollectionView * const collectionView in self.collectionViews) {
        [(VLCLibraryCollectionViewFlowLayout *)collectionView.collectionViewLayout resetLayout];
        [collectionView reloadData];
    }
}

- (void)reloadViewsAtIndex:(NSUInteger)index
          dueToCacheAction:(VLCLibraryDataSourceCacheAction)action
{
    NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:index inSection:0];
    NSSet<NSIndexPath *> * const indexPathSet = [NSSet setWithObject:indexPath];

    for (NSCollectionView * const collectionView in self.collectionViews) {
        switch (action) {
            case VLCLibraryDataSourceCacheUpdateAction:
                [collectionView reloadItemsAtIndexPaths:indexPathSet];
                break;
            case VLCLibraryDataSourceCacheDeleteAction:
                [collectionView deleteItemsAtIndexPaths:indexPathSet];
                break;
            default:
                NSAssert(false, @"Invalid playlist cache action");
        }
    }
}

- (NSUInteger)indexForPlaylistWithId:(const int64_t)itemId
{
    return [self.playlists indexOfObjectPassingTest:^BOOL(const VLCMediaLibraryPlaylist *playlist, const NSUInteger __unused idx, BOOL * const __unused stop) {
        NSAssert(playlist != nil, @"Cache list should not contain nil playlists");
        return playlist.libraryID == itemId;
    }];
}

- (void)cacheAction:(VLCLibraryDataSourceCacheAction)action
         onPlaylist:(VLCMediaLibraryPlaylist * const)playlist
{
    NSParameterAssert(playlist != nil);

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        const NSUInteger idx = [self indexForPlaylistWithId:playlist.libraryID];
        if (idx == NSNotFound) {
            return;
        }

        NSMutableArray * const mutablePlaylists = self.playlists.mutableCopy;

        switch (action) {
            case VLCLibraryDataSourceCacheUpdateAction:
                [mutablePlaylists replaceObjectAtIndex:idx withObject:playlist];
                break;
            case VLCLibraryDataSourceCacheDeleteAction:
                [mutablePlaylists removeObjectAtIndex:idx];
                break;
            default:
                NSAssert(false, @"Invalid playlist cache action");
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            self.playlists = mutablePlaylists.copy;
            [self reloadViewsAtIndex:idx dueToCacheAction:action];
        });
    });
}

#pragma mark - table view data source

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (tableView == self.masterTableView) {
        return self.playlists.count;
    }

    const NSInteger selectedMasterRow = self.masterTableView.selectedRow;
    if (selectedMasterRow > -1) {
        const id<VLCMediaLibraryItemProtocol> item = self.playlists[selectedMasterRow];
        return item.mediaItems.count;
    }

    return 0;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    if (tableView == self.masterTableView) {
        return self.playlists[row];
    }

    const NSInteger selectedMasterRow = self.masterTableView.selectedRow;
    if (tableView == self.detailTableView && selectedMasterRow > -1) {
        const id<VLCMediaLibraryItemProtocol> item = self.playlists[selectedMasterRow];
        return item.mediaItems[row];
    }

    return nil;
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return NSNotFound;
    }
    return [self.playlists indexOfObjectPassingTest:^BOOL(const VLCMediaLibraryPlaylist *playlist, const NSUInteger __unused idx, BOOL * const __unused stop) {
        return playlist.libraryID == libraryItem.libraryID;
    }];
}

#pragma mark - collection view data source

- (void)setCollectionViews:(NSArray<NSCollectionView *> *)collectionViews
{
    _collectionViews = collectionViews;
    for (NSCollectionView * const collectionView in self.collectionViews) {
        [self setupCollectionView:collectionView];
    }
}

- (void)setupCollectionView:(NSCollectionView *)collectionView
{
    [collectionView registerClass:VLCLibraryCollectionViewItem.class
            forItemWithIdentifier:VLCLibraryCellIdentifier];
    [collectionView registerClass:VLCLibraryCollectionViewSupplementaryElementView.class
       forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                   withIdentifier:VLCLibrarySupplementaryElementViewIdentifier];

    NSNib * const supplementaryDetailView =
        [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewMediaItemListSupplementaryDetailView" bundle:nil];
    [collectionView registerNib:supplementaryDetailView
      forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind
                  withIdentifier:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewIdentifier];

    NSCollectionViewFlowLayout * const layout = collectionView.collectionViewLayout;
    layout.headerReferenceSize = VLCLibraryCollectionViewSupplementaryElementView.defaultHeaderSize;

    collectionView.dataSource = self;
    [collectionView reloadData];
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    return self.playlists.count;
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return 1;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem * const viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier
                                                                              forIndexPath:indexPath];
    const id<VLCMediaLibraryItemProtocol> libraryItem = self.playlists[indexPath.item];
    // NOTE: Unknown parent type represented items default to playing the represented item only.
    // We want this behaviour as it feels unnatural to handle any parent types for playlists
    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem
                                                                                             parentType:VLCMediaLibraryParentGroupTypeUnknown];
    viewItem.representedItem = representedItem;
    return viewItem;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView
viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
               atIndexPath:(NSIndexPath *)indexPath
{
    if([kind isEqualToString:NSCollectionElementKindSectionHeader]) {
        VLCLibraryCollectionViewSupplementaryElementView * const sectionHeadingView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibrarySupplementaryElementViewIdentifier forIndexPath:indexPath];

        sectionHeadingView.stringValue = _NS("Playlists");
        return sectionHeadingView;

    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind]) {
        NSString * const supplementaryDetailViewIdentifier =
            VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewIdentifier;
        VLCLibraryCollectionViewMediaItemListSupplementaryDetailView * const supplementaryDetailView =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:supplementaryDetailViewIdentifier
                                           forIndexPath:indexPath];
        const id<VLCMediaLibraryItemProtocol> item =
            [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        VLCLibraryRepresentedItem * const representedItem =
            [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:self.currentParentType];
        supplementaryDetailView.representedItem = representedItem;
        supplementaryDetailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return supplementaryDetailView;
    }

    return nil;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    const NSUInteger indexPathItem = indexPath.item;

    if (indexPathItem < 0 || indexPathItem >= self.playlists.count) {
        return nil;
    }

    return self.playlists[indexPathItem];
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    const NSUInteger idx = [self.playlists indexOfObject:libraryItem];
    if (idx == NSNotFound) {
        return nil;
    }

    return [NSIndexPath indexPathForItem:idx inSection:0];
}

- (NSArray<VLCLibraryRepresentedItem *> *)representedItemsAtIndexPaths:(NSSet<NSIndexPath *> *const)indexPaths
                                                     forCollectionView:(NSCollectionView *)collectionView
{
    NSMutableArray<VLCLibraryRepresentedItem *> * const representedItems =
        [NSMutableArray arrayWithCapacity:indexPaths.count];

    for (NSIndexPath * const indexPath in indexPaths) {
        const id<VLCMediaLibraryItemProtocol> libraryItem =
            [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
        VLCLibraryRepresentedItem * const representedItem =
            [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem
                                                 parentType:self.currentParentType];
        [representedItems addObject:representedItem];
    }

    return representedItems;
}

- (VLCMediaLibraryParentGroupType)currentParentType
{
    return VLCMediaLibraryParentGroupTypePlaylist;
}

- (NSString *)supplementaryDetailViewKind
{
    return VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind;
}

- (void)setPlaylistType:(vlc_ml_playlist_type_t)playlistType
{
    if (self.playlistType == playlistType) {
        return;
    }

    _playlistType = playlistType;
    [self reloadData];
}

@end
