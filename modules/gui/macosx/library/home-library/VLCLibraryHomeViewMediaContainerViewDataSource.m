/*****************************************************************************
 * VLCLibraryHomeViewMediaContainerViewDataSource.m: MacOS X interface module
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

#import "VLCLibraryHomeViewMediaContainerViewDataSource.h"

#import "extensions/NSView+VLCAdditions.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryImageCache.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"

#import "library/home-library/VLCLibraryHomeViewVideoCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewVideoGridContainerView.h"

#import "library/VLCLibraryMediaGroupDescriptor.h"

#import "main/VLCMain.h"

#import "views/VLCUIUnits.h"

NSString * const VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification = @"VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification";

@interface VLCLibraryHomeViewMediaContainerViewDataSource ()
{
    VLCLibraryCollectionViewFlowLayout *_collectionViewFlowLayout;
    VLCLibraryModel *_libraryModel;
    NSUInteger _reloadGeneration;
}

@property (readwrite, atomic) NSMutableArray *collectionArray;

@end

@implementation VLCLibraryHomeViewMediaContainerViewDataSource

- (instancetype)init
{
    self = [super init];
    if(self) {
        _libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
        _collectionViewItemIdentifier = VLCLibraryCollectionViewItemIdentifier;
        self.collectionArray = [NSMutableArray array];
        [self connect];
    }
    return self;
}

- (NSUInteger)indexOfMediaItemInCollection:(const int64_t)libraryId
{
    return [self.collectionArray indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem * const findMediaItem, const NSUInteger __unused idx, BOOL * const __unused stop) {
        NSAssert(findMediaItem != nil, @"Collection should not contain nil media items");
        return findMediaItem.libraryID == libraryId;
    }];
}

- (void)libraryModelVideoListReset:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCMediaLibraryParentGroupTypeVideoLibrary) {
        return;
    }

    [self reloadData];
}

- (void)libraryModelVideoItemUpdated:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCMediaLibraryParentGroupTypeVideoLibrary) {
        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item updated notification should carry valid media item");

    [self reloadDataForMediaItem:notificationMediaItem];
}

- (void)libraryModelVideoItemDeleted:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCMediaLibraryParentGroupTypeVideoLibrary) {
        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item deleted notification should carry valid media item");

    [self deleteDataForMediaItem:notificationMediaItem];
}

- (void)libraryModelRecentsListReset:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCMediaLibraryParentGroupTypeRecentVideos) {
        return;
    }

    [self reloadData];
}

- (void)libraryModelRecentsItemUpdated:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCMediaLibraryParentGroupTypeRecentVideos) {
        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item updated notification should carry valid media item");

    [self reloadDataForMediaItem:notificationMediaItem];
}

- (void)libraryModelRecentsItemDeleted:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCMediaLibraryParentGroupTypeRecentVideos) {
        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item deleted notification should carry valid media item");

    [self deleteDataForMediaItem:notificationMediaItem];
}

- (void)connect
{
    NSNotificationCenter * const notificationCenter = NSNotificationCenter.defaultCenter;

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelVideoListReset:)
                               name:VLCLibraryModelVideoMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelVideoListReset:)
                               name:VLCLibraryModelAllCachesDropped
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelVideoItemUpdated:)
                               name:VLCLibraryModelVideoMediaItemUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelVideoItemDeleted:)
                               name:VLCLibraryModelVideoMediaItemDeleted
                             object:nil];

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelRecentsListReset:)
                               name:VLCLibraryModelRecentsMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelRecentsItemUpdated:)
                               name:VLCLibraryModelRecentsMediaItemUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelRecentsItemDeleted:)
                               name:VLCLibraryModelRecentsMediaItemDeleted
                             object:nil];

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelRecentlyAddedListReset:)
                               name:VLCLibraryModelRecentlyAddedVideoMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelRecentlyAddedListReset:)
                               name:VLCLibraryModelRecentlyAddedAudioMediaListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelRecentlyAddedListReset:)
                               name:VLCLibraryModelAllCachesDropped
                             object:nil];

    [self reloadData];
}

- (void)libraryModelRecentlyAddedListReset:(NSNotification *)notification
{
    const VLCMediaLibraryParentGroupType group = self.groupDescriptor.group;
    const BOOL resetVideos = group == VLCMediaLibraryParentGroupTypeRecentlyAddedVideos &&
        [notification.name isEqualToString:VLCLibraryModelRecentlyAddedVideoMediaListReset];
    const BOOL resetAudio = group == VLCMediaLibraryParentGroupTypeRecentlyAddedAudios &&
        [notification.name isEqualToString:VLCLibraryModelRecentlyAddedAudioMediaListReset];
    const BOOL dropAll = [notification.name isEqualToString:VLCLibraryModelAllCachesDropped] &&
        (group == VLCMediaLibraryParentGroupTypeRecentlyAddedVideos ||
         group == VLCMediaLibraryParentGroupTypeRecentlyAddedAudios);
    if (resetVideos || resetAudio || dropAll) {
        [self reloadData];
    }
}

- (void)disconnect
{
    ++_reloadGeneration;
    [NSNotificationCenter.defaultCenter removeObserver:self];
}

- (void)reloadData
{
    if (!NSThread.isMainThread) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self reloadData];
        });
        return;
    }

    if(self.collectionView == nil || !_groupDescriptor) {
        NSLog(@"Null collection view or group descriptor");
        return;
    }

    const NSUInteger generation = ++_reloadGeneration;
    const VLCMediaLibraryParentGroupType group = self.groupDescriptor.group;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSArray *newCollectionArray = nil;
        switch(group) {
            case VLCMediaLibraryParentGroupTypeRecentlyAddedVideos:
                newCollectionArray = self->_libraryModel.listOfRecentlyAddedVideoMedia;
                break;
            case VLCMediaLibraryParentGroupTypeRecentlyAddedAudios:
                newCollectionArray = self->_libraryModel.listOfRecentlyAddedAudioMedia;
                break;
            case VLCMediaLibraryParentGroupTypeVideoLibrary:
                newCollectionArray = self->_libraryModel.listOfVideoMedia;
                break;
            case VLCMediaLibraryParentGroupTypeRecentVideos:
                newCollectionArray = self->_libraryModel.listOfRecentMedia;
                break;
            default:
                return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            if (generation != self->_reloadGeneration) {
                return;
            }
            self.collectionArray = [newCollectionArray mutableCopy];
            [self.collectionView reloadData];
            [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification
                                                              object:self];
        });
    });
}

- (void)reloadDataForMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
{
    const NSUInteger mediaItemIndex = [self indexOfMediaItemInCollection:mediaItem.libraryID];
    if (mediaItemIndex == NSNotFound) {
        return;
    }

    [self.collectionArray replaceObjectAtIndex:mediaItemIndex withObject:mediaItem];

    NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:mediaItemIndex inSection:0];
    [self.collectionView reloadItemsAtIndexPaths:[NSSet setWithObject:indexPath]];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification
                                                      object:self];
}

- (void)deleteDataForMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
{
    const NSUInteger mediaItemIndex = [self indexOfMediaItemInCollection:mediaItem.libraryID];
    if (mediaItemIndex == NSNotFound) {
        return;
    }

    [self.collectionArray removeObjectAtIndex:mediaItemIndex];

    NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:mediaItemIndex inSection:0];
    [self.collectionView deleteItemsAtIndexPaths:[NSSet setWithObject:indexPath]];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification
                                                      object:self];
}

- (void)setGroupDescriptor:(VLCLibraryMediaCollectionViewGroupDescriptor *)groupDescriptor
{
    if(!groupDescriptor) {
        NSLog(@"Invalid group descriptor");
        return;
    }

    _groupDescriptor = groupDescriptor;
    [self reloadData];
}

- (void)setup
{
    _collectionViewFlowLayout = _collectionView.collectionViewLayout;
    self.collectionView.dataSource = self;

    [self.collectionView registerClass:VLCLibraryCollectionViewItem.class
                 forItemWithIdentifier:VLCLibraryCollectionViewItemIdentifier];

    NSNib * const mediaItemSupplementaryDetailView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewMediaItemSupplementaryDetailView" bundle:nil];
    [self.collectionView registerNib:mediaItemSupplementaryDetailView
          forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                      withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier];
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return 1;
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    if (!_libraryModel) {
        return 0;
    }

    return self.collectionArray.count;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    NSCollectionViewItem<VLCLibraryRepresentedItemCollectionViewItem> * const viewItem =
        (NSCollectionViewItem<VLCLibraryRepresentedItemCollectionViewItem> *)[collectionView
            makeItemWithIdentifier:self.collectionViewItemIdentifier
                       forIndexPath:indexPath];
    const id<VLCMediaLibraryItemProtocol> item = self.collectionArray[indexPath.item];
    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:self.groupDescriptor.group];

    viewItem.representedItem = representedItem;
    return viewItem;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView
viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
               atIndexPath:(NSIndexPath *)indexPath
{
    if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView * const mediaItemSupplementaryDetailView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind forIndexPath:indexPath];

        const id<VLCMediaLibraryItemProtocol> item = self.collectionArray[indexPath.item];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:item parentType:self.groupDescriptor.group];

        mediaItemSupplementaryDetailView.representedItem = representedItem;
        mediaItemSupplementaryDetailView.selectedItem = [collectionView itemAtIndexPath:indexPath];

        VLCLibraryCollectionViewFlowLayout *flowLayout = (VLCLibraryCollectionViewFlowLayout*)collectionView.collectionViewLayout;
        if (flowLayout != nil) {
            mediaItemSupplementaryDetailView.layoutScrollDirection = flowLayout.scrollDirection;
        }
        
        return mediaItemSupplementaryDetailView;
    }

    return nil;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    const NSUInteger indexPathItem = indexPath.item;

    if (indexPathItem < 0 || indexPathItem >= self.collectionArray.count) {
        return nil;
    }

    return self.collectionArray[indexPath.item];
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return nil;
    }

    const NSUInteger libraryItemIndex = [self indexOfMediaItemInCollection:libraryItem.libraryID];
    if (libraryItemIndex == NSNotFound) {
        return nil;
    }

    return [NSIndexPath indexPathForItem:libraryItemIndex inSection:0];
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
                                                 parentType:self.groupDescriptor.group];
        [representedItems addObject:representedItem];
    }

    return representedItems;
}

- (NSString *)supplementaryDetailViewKind
{
    return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
}

@end
