/*****************************************************************************
 * VLCLibraryAudioGroupDataSource.m: MacOS X interface module
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

#import "VLCLibraryAudioGroupDataSource.h"

#import "extensions/NSPasteboardItem+VLCAdditions.h"

#import "main/VLCMain.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryWindow.h"

#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryAudioGroupHeaderView.h"

#import "views/VLCSubScrollView.h"

@interface VLCLibraryAudioGroupDataSource ()
{
    id<VLCMediaLibraryAudioGroupProtocol> _representedAudioGroup;
}
@property (readwrite, atomic, strong) NSArray<VLCMediaLibraryAlbum *> *representedListOfAlbums;

@end

@implementation VLCLibraryAudioGroupDataSource

@synthesize currentParentType = _currentParentType;

+ (void)setupCollectionView:(NSCollectionView *)collectionView
{
    NSNib * const audioGroupHeaderView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryAudioGroupHeaderView"
                                                                  bundle:nil];
    [collectionView registerNib:audioGroupHeaderView
     forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                 withIdentifier:VLCLibraryAudioGroupHeaderViewIdentifier];
}

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
    [self addObserver:self forKeyPath:@"collectionViews" options:NSKeyValueObservingOptionNew context:nil];
    [self addObserver:self forKeyPath:@"tableViews" options:NSKeyValueObservingOptionNew context:nil];
    [self connect];
}

- (void)observeValueForKeyPath:(NSString *)keyPath
                      ofObject:(id)object
                        change:(NSDictionary *)change
                       context:(void *)context
{
    if (object != self) {
        return;
    }

    if ([keyPath isEqualToString:@"collectionViews"]) {
        [self reloadCollectionViews];
    } else if ([keyPath isEqualToString:@"tableViews"]) {
        [self reloadTableViews];
    }
}

- (void)connect
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelAudioMediaItemsReset:)
                               name:VLCLibraryModelAudioMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelAudioMediaItemUpdated:)
                               name:VLCLibraryModelAudioMediaItemUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelAudioMediaItemDeleted:)
                               name:VLCLibraryModelAudioMediaItemDeleted
                             object:nil];

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelAlbumsReset:)
                               name:VLCLibraryModelAlbumListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelAlbumUpdated:)
                               name:VLCLibraryModelAlbumUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelAlbumDeleted:)
                               name:VLCLibraryModelAlbumDeleted
                             object:nil];

    [self reloadData];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (NSInteger)rowContainingMediaItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    return [self.representedAudioGroup.albums indexOfObjectPassingTest:^BOOL(VLCMediaLibraryAlbum * const album, const NSUInteger __unused idx, BOOL * const __unused stop) {
        return [album.mediaItems indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem * const item, const NSUInteger __unused idx, BOOL * const __unused stop) {
            return item.libraryID == libraryItem.libraryID;
        }] != NSNotFound;
    }];
}

- (void)handleAlbumUpdateInRow:(NSInteger)row
{
    NSParameterAssert(row >= 0 && (NSUInteger)row < self.representedListOfAlbums.count);
    NSIndexSet * const indexSet = [NSIndexSet indexSetWithIndex:row];
    NSIndexSet * const columnIndexSet = [NSIndexSet indexSetWithIndex:0];
    NSSet * const indexPaths = [NSSet setWithObject:[NSIndexPath indexPathForItem:row inSection:0]];

    [self performActionOnTableViews:^(NSTableView * const tableView){
        [tableView reloadDataForRowIndexes:indexSet columnIndexes:columnIndexSet];
    } onCollectionViews:^(NSCollectionView * const collectionView){
        [collectionView reloadItemsAtIndexPaths:indexPaths];
    }];
}

- (void)handleLibraryItemChange:(id<VLCMediaLibraryItemProtocol>)item
{
    NSParameterAssert(item != nil);
    const NSInteger row = [self rowContainingMediaItem:item];
    if (row == NSNotFound) {
        NSLog(@"VLCLibraryAudioGroupDataSource: Unable to find row for library item, can't change");
        return;
    }
    [self handleAlbumUpdateInRow:row];
}

- (void)performActionOnTableViews:(void (^)(NSTableView *))tableViewAction
                onCollectionViews:(void (^)(NSCollectionView *))collectionViewAction
{
    NSParameterAssert(tableViewAction != nil && collectionViewAction != nil);

    NSArray<NSTableView *> * const tableViews = self.tableViews;
    for (NSTableView * const tableView in tableViews) {
        tableViewAction(tableView);
    }

    NSArray<NSCollectionView *> * const collectionViews = self.collectionViews;
    for (NSCollectionView * const collectionView in collectionViews) {
        collectionViewAction(collectionView);
    }
}

- (void)libraryModelAudioMediaItemsReset:(NSNotification *)notification
{
    [self updateRepresentedListOfAlbums];
}


- (void)libraryModelAudioMediaItemUpdated:(NSNotification *)notification
{
    [self handleLibraryItemChange:notification.object];
}

- (void)libraryModelAudioMediaItemDeleted:(NSNotification *)notification
{
    [self handleLibraryItemChange:notification.object];
}

- (void)libraryModelAlbumsReset:(NSNotification *)notification
{
    [self updateRepresentedListOfAlbums];
}

- (void)libraryModelAlbumUpdated:(NSNotification *)notification
{
    const NSInteger row = [self rowForLibraryItem:notification.object];
    if (row == NSNotFound) {
        NSLog(@"VLCLibraryAudioGroupDataSource: Unable to find row for library item, can't update");
        return;
    }

    [self handleAlbumUpdateInRow:row];
}

- (void)libraryModelAlbumDeleted:(NSNotification *)notification
{
    const NSInteger row = [self rowForLibraryItem:notification.object];
    if (row == NSNotFound) {
        NSLog(@"VLCLibraryAudioGroupDataSource: Unable to find row for library item, can't delete");
        return;
    }

    NSIndexSet * const indexSet = [NSIndexSet indexSetWithIndex:row];
    NSSet * const indexPaths = [NSSet setWithObject:[NSIndexPath indexPathForItem:row inSection:0]];

    [self performActionOnTableViews:^(NSTableView * const tableView){
        [tableView removeRowsAtIndexes:indexSet withAnimation:NSTableViewAnimationSlideUp];
    } onCollectionViews:^(NSCollectionView * const collectionView){
        [collectionView deleteItemsAtIndexPaths:indexPaths];
    }];
}

- (void)reloadTableViews
{
    NSArray<NSTableView *> * const tableViews = self.tableViews;
    for (NSTableView * const tableView in tableViews) {
        [tableView reloadData];
    }
}

- (void)reloadCollectionViews
{
    NSArray<NSCollectionView *> * const collectionViews = self.collectionViews;
    for (NSCollectionView * const collectionView in collectionViews) {
        NSCollectionViewLayout * const collectionViewLayout = collectionView.collectionViewLayout;
        if ([collectionViewLayout isKindOfClass:VLCLibraryCollectionViewFlowLayout.class]) {
            [(VLCLibraryCollectionViewFlowLayout *)collectionViewLayout resetLayout];
        }
        [collectionView reloadData];
    }
}

- (void)reloadData
{
    [self reloadTableViews];
    [self reloadCollectionViews];
}

- (void)updateRepresentedListOfAlbums
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
        if (self.representedAudioGroup == nil || self.currentParentType == VLCMediaLibraryParentGroupTypeUnknown) {
            self.representedListOfAlbums = libraryModel.listOfAlbums;
        } else if (self.representedAudioGroup.albums.count == 0) {
            dispatch_sync(dispatch_get_main_queue(), ^{
                self.representedListOfAlbums = [libraryModel listAlbumsOfParentType:self.currentParentType forID:self.representedAudioGroup.libraryID];
            });
        } else {
            self.representedListOfAlbums = self.representedAudioGroup.albums;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            [self reloadData];
        });
    });
}

- (id<VLCMediaLibraryAudioGroupProtocol>)representedAudioGroup
{
    @synchronized (self) {
        return _representedAudioGroup;
    }
}

- (void)setRepresentedAudioGroup:(VLCAbstractMediaLibraryAudioGroup *)representedAudioGroup
{
    @synchronized (self) {
        if (_representedAudioGroup == representedAudioGroup) {
            return;
        }

        _representedAudioGroup = representedAudioGroup;

        if ([representedAudioGroup isKindOfClass:VLCMediaLibraryAlbum.class]) {
            _currentParentType = VLCMediaLibraryParentGroupTypeAlbum;
        } else if ([representedAudioGroup isKindOfClass:VLCMediaLibraryArtist.class]) {
            _currentParentType = VLCMediaLibraryParentGroupTypeArtist;
        } else if ([representedAudioGroup isKindOfClass:VLCMediaLibraryGenre.class]) {
            _currentParentType = VLCMediaLibraryParentGroupTypeGenre;
        } else {
            NSAssert(1, @"Current parent type should be a valid audio group type");
        }

        [self updateRepresentedListOfAlbums];
    }
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (self.representedListOfAlbums != nil) {
        return self.representedListOfAlbums.count;
    }

    return 0;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    if (row < 0 || (NSUInteger)row >= self.representedListOfAlbums.count) {
        return nil;
    }

    return self.representedListOfAlbums[row];
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return NSNotFound;
    }

    NSArray<id<VLCMediaLibraryItemProtocol>> * const libraryItems = self.representedListOfAlbums;
    const NSUInteger itemCount = libraryItems.count;

    for (NSUInteger i = 0; i < itemCount; ++i) {
        const id<VLCMediaLibraryItemProtocol> collectionItem = [libraryItems objectAtIndex:i];
        if (collectionItem.libraryID == libraryItem.libraryID) {
            return i;
        }
    }

    return NSNotFound;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    return self.representedListOfAlbums.count;
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return 1;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem * const viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    const id<VLCMediaLibraryItemProtocol> libraryItem = [self libraryItemAtIndexPath:indexPath forCollectionView:collectionView];
    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem parentType:_currentParentType];
    viewItem.representedItem = representedItem;
    return viewItem;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView
viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
               atIndexPath:(NSIndexPath *)indexPath
{
    if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind]) {
        NSArray<VLCMediaLibraryAlbum *> * const albums = self.representedListOfAlbums;
        if (albums == nil || albums.count == 0 || indexPath.item < 0 || (NSUInteger)indexPath.item >= albums.count) {
            return nil;
        }

        VLCLibraryCollectionViewMediaItemListSupplementaryDetailView * const albumSupplementaryDetailView =
            [collectionView makeSupplementaryViewOfKind:kind
                                         withIdentifier:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind
                                           forIndexPath:indexPath];

        VLCMediaLibraryAlbum * const album = albums[indexPath.item];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:album parentType:_currentParentType];

        albumSupplementaryDetailView.representedItem = representedItem;
        albumSupplementaryDetailView.selectedItem = [collectionView itemAtIndex:indexPath.item];
        albumSupplementaryDetailView.parentScrollView = VLCMain.sharedInstance.libraryWindow.audioCollectionViewScrollView;
        albumSupplementaryDetailView.internalScrollView.scrollParentY = YES;

        return albumSupplementaryDetailView;

    } else if ([kind isEqualToString:NSCollectionElementKindSectionHeader]) {
        VLCLibraryAudioGroupHeaderView * const headerView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryAudioGroupHeaderViewIdentifier forIndexPath:indexPath];

        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:_representedAudioGroup parentType:_currentParentType];
        headerView.representedItem = representedItem;
        return headerView;
    }

    return nil;
}

- (id<NSPasteboardWriting>)tableView:(NSTableView *)tableView pasteboardWriterForRow:(NSInteger)row
{
    const id<VLCMediaLibraryItemProtocol> libraryItem = [self libraryItemAtRow:row forTableView:tableView];

    return [NSPasteboardItem pasteboardItemWithLibraryItem:libraryItem];
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    const NSUInteger indexPathItem = indexPath.item;

    if (indexPathItem < 0 || indexPathItem >= self.representedListOfAlbums.count) {
        return nil;
    }

    return self.representedListOfAlbums[indexPathItem];
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return nil;
    }

    const NSInteger arrayIdx = [self rowForLibraryItem:libraryItem];
    return [NSIndexPath indexPathForItem:arrayIdx inSection:0];
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

- (NSString *)supplementaryDetailViewKind
{
    return VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind;
}

@end
