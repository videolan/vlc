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

#import "extensions/NSPasteboardItem+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryMasterDetailViewTableViewDelegate.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "main/VLCMain.h"

typedef NS_ENUM(NSInteger, VLCLibraryDataSourceCacheAction) {
    VLCLibraryDataSourceCacheUpdateAction,
    VLCLibraryDataSourceCacheDeleteAction,
};

@interface VLCLibraryPlaylistDataSource () {
    dispatch_queue_t _playlistQueue;
    NSArray<VLCMediaLibraryPlaylist *> *_playlists;
}

@end

@implementation VLCLibraryPlaylistDataSource

@synthesize headerDelegate;

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
    _playlistQueue = dispatch_queue_create("org.videolan.vlc.libraryplaylistdatasource.queue", DISPATCH_QUEUE_CONCURRENT);
    _libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
    [self connect];
    [self reloadData];
}

- (NSArray<VLCMediaLibraryPlaylist *> *)playlists
{
    __block NSArray<VLCMediaLibraryPlaylist *> *playlists;
    dispatch_sync(_playlistQueue, ^{
        playlists = self->_playlists;
    });
    return playlists;
}

- (void)setPlaylists:(NSArray<VLCMediaLibraryPlaylist *> *)playlists
{
    dispatch_barrier_async(_playlistQueue, ^{
        self->_playlists = [playlists copy];
    });
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
    [self applyCacheAction:VLCLibraryDataSourceCacheUpdateAction
            withPlaylistID:playlist.libraryID
                  playlist:playlist];
}

- (void)playlistDeleted:(NSNotification *)notification
{
    NSParameterAssert(notification);
    NSParameterAssert((NSNumber *)notification.object != nil);

    const int64_t playlistId = [(NSNumber *)notification.object longLongValue];
    [self applyCacheAction:VLCLibraryDataSourceCacheDeleteAction
            withPlaylistID:playlistId
                  playlist:nil];
}

- (void)reloadData
{
    NSArray<VLCMediaLibraryPlaylist *> * const listOfPlaylists =
        [self.libraryModel listOfPlaylistsOfType:self.playlistType];
    self.playlists = listOfPlaylists ?: @[];
    [self reloadViews];
    [self updateHeaderInTableView:self.detailTableView forMasterSelection:self.masterTableView];
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
    NSTableView * const masterTableView = self.masterTableView;
    NSTableView * const detailTableView = self.detailTableView;
    NSIndexSet * const indexSet = [NSIndexSet indexSetWithIndex:index];
    const NSInteger selectedMasterRow = masterTableView.selectedRow;
    const BOOL affectsSelectedDetail =
        detailTableView != nil && (NSInteger)index == selectedMasterRow;

    switch (action) {
        case VLCLibraryDataSourceCacheUpdateAction: {
            NSIndexSet * const columnSet =
                [NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, masterTableView.numberOfColumns)];
            [masterTableView reloadDataForRowIndexes:indexSet columnIndexes:columnSet];
            break;
        }
        case VLCLibraryDataSourceCacheDeleteAction:
            [masterTableView removeRowsAtIndexes:indexSet
                                   withAnimation:NSTableViewAnimationEffectNone];
            break;
        default:
            NSAssert(false, @"Invalid playlist cache action");
    }

    if (affectsSelectedDetail) {
        [detailTableView reloadData];
        [self updateHeaderInTableView:detailTableView forMasterSelection:masterTableView];
    }

    NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:index inSection:0];
    NSSet<NSIndexPath *> * const indexPathSet = [NSSet setWithObject:indexPath];

    for (NSCollectionView * const collectionView in self.collectionViews) {
        switch (action) {
            case VLCLibraryDataSourceCacheUpdateAction:
                [collectionView reloadItemsAtIndexPaths:indexPathSet];
                break;
            case VLCLibraryDataSourceCacheDeleteAction:
                [(VLCLibraryCollectionViewFlowLayout *)collectionView.collectionViewLayout resetLayout];
                [collectionView deleteItemsAtIndexPaths:indexPathSet];
                break;
            default:
                NSAssert(false, @"Invalid playlist cache action");
        }
    }
}

- (void)applyCacheAction:(VLCLibraryDataSourceCacheAction)action
          withPlaylistID:(const int64_t)playlistID
                playlist:(VLCMediaLibraryPlaylist * _Nullable const)playlist
{
    dispatch_barrier_async(_playlistQueue, ^{
        NSMutableArray *mutablePlaylists = [self->_playlists mutableCopy];
        if (mutablePlaylists == nil) {
            return;
        }

        const NSUInteger idx =
            [mutablePlaylists indexOfObjectPassingTest:^BOOL(const VLCMediaLibraryPlaylist *item, 
                                                             const NSUInteger __unused index,
                                                             BOOL * const __unused stop) {
            return item.libraryID == playlistID;
        }];

        if (idx == NSNotFound) {
            return;
        }

        switch (action) {
            case VLCLibraryDataSourceCacheUpdateAction:
                NSAssert(playlist != nil, @"Playlist must not be nil for update action");
                if (playlist != nil) {
                    [mutablePlaylists replaceObjectAtIndex:idx withObject:playlist];
                }
                break;
            case VLCLibraryDataSourceCacheDeleteAction:
                [mutablePlaylists removeObjectAtIndex:idx];
                break;
            default:
                NSAssert(false, @"Invalid playlist cache action");
        }
        self->_playlists = [mutablePlaylists copy];

        dispatch_async(dispatch_get_main_queue(), ^{
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

- (id<NSPasteboardWriting>)tableView:(NSTableView *)tableView pasteboardWriterForRow:(NSInteger)row
{
    const id<VLCMediaLibraryItemProtocol> libraryItem = [self libraryItemAtRow:row forTableView:tableView];
    return [NSPasteboardItem pasteboardItemWithLibraryItem:libraryItem];
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
            forItemWithIdentifier:VLCLibraryCollectionViewItemIdentifier];
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
    VLCLibraryCollectionViewItem * const viewItem =
        [collectionView makeItemWithIdentifier:VLCLibraryCollectionViewItemIdentifier
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

- (id<VLCMediaLibraryItemProtocol>)parentItemForTableView:(NSTableView *)tableView
{
    if (tableView != self.detailTableView) {
        return nil;
    }

    const NSInteger selectedRow = self.masterTableView.selectedRow;
    if (selectedRow < 0 || (NSUInteger)selectedRow >= self.playlists.count) {
        return nil;
    }

    return self.playlists[selectedRow];
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

- (void)updateHeaderInTableView:(NSTableView *)detailTableView forMasterSelection:(NSTableView *)masterTableView
{
    if (self.headerDelegate == nil) {
        return;
    }

    const NSInteger selectedRow = masterTableView.selectedRow;
    if (selectedRow < 0 || (NSUInteger)selectedRow >= self.playlists.count) {
        [self.headerDelegate updateHeaderForTableView:detailTableView
                                  withRepresentedItem:nil
                                        fallbackTitle:_NS("Playlists")
                                       fallbackDetail:_NS("Select a playlist")];
        return;
    }

    const VLCMediaLibraryPlaylist * const playlist = self.playlists[selectedRow];
    VLCLibraryRepresentedItem * const representedItem =
        [[VLCLibraryRepresentedItem alloc] initWithItem:playlist
                                             parentType:self.currentParentType];

    [self.headerDelegate updateHeaderForTableView:detailTableView
                              withRepresentedItem:representedItem
                                    fallbackTitle:playlist.primaryDetailString
                                   fallbackDetail:playlist.secondaryDetailString];
}

@end
