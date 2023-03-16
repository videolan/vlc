/*****************************************************************************
 * VLCLibraryVideoCollectionViewContainerViewDataSource.m: MacOS X interface module
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

#import "VLCLibraryVideoCollectionViewContainerViewDataSource.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"

#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"

#import "main/VLCMain.h"

@interface VLCLibraryVideoCollectionViewContainerViewDataSource ()
{
    VLCLibraryCollectionViewFlowLayout *_collectionViewFlowLayout;
    VLCLibraryModel *_libraryModel;
}

@property (readwrite, atomic) NSArray *collectionArray;

@end

@implementation VLCLibraryVideoCollectionViewContainerViewDataSource

- (instancetype)init
{
    self = [super init];
    if(self) {
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelVideoListReset:)
                                   name:VLCLibraryModelVideoMediaListReset
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

        _libraryModel = [VLCMain sharedInstance].libraryController.libraryModel;
        self.collectionArray = [NSArray array];
    }
    return self;
}

- (NSUInteger)indexOfMediaItemInCollection:(const NSUInteger)libraryId
{
    return [self.collectionArray indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem * const findMediaItem, const NSUInteger idx, BOOL * const stop) {
        NSAssert(findMediaItem != nil, @"Collection should not contain nil media items");
        return findMediaItem.libraryID == libraryId;
    }];
}

- (void)libraryModelVideoListReset:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCLibraryVideoLibraryGroup) {
        return;
    }

    [self reloadData];
}

- (void)libraryModelVideoItemUpdated:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCLibraryVideoLibraryGroup) {
        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item updated notification should carry valid media item");

    [self reloadDataForMediaItem:notificationMediaItem];
}

- (void)libraryModelVideoItemDeleted:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCLibraryVideoLibraryGroup) {
        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item deleted notification should carry valid media item");

    [self deleteDataForMediaItem:notificationMediaItem];
}

- (void)libraryModelRecentsListReset:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCLibraryVideoRecentsGroup) {
        return;
    }

    [self reloadData];
}

- (void)libraryModelRecentsItemUpdated:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCLibraryVideoRecentsGroup) {
        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item updated notification should carry valid media item");

    [self reloadDataForMediaItem:notificationMediaItem];
}

- (void)libraryModelRecentsItemDeleted:(NSNotification * const)aNotification
{
    if (_groupDescriptor.group != VLCLibraryVideoRecentsGroup) {
        return;
    }

    NSParameterAssert(aNotification);
    VLCMediaLibraryMediaItem *notificationMediaItem = aNotification.object;
    NSAssert(notificationMediaItem != nil, @"Media item deleted notification should carry valid media item");

    [self deleteDataForMediaItem:notificationMediaItem];
}

- (void)reloadData
{
    if(!_collectionView || !_groupDescriptor) {
        NSLog(@"Null collection view or video group descriptor");
        return;
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        switch(self->_groupDescriptor.group) {
            case VLCLibraryVideoLibraryGroup:
                self.collectionArray = self->_libraryModel.listOfVideoMedia;
                break;
            case VLCLibraryVideoRecentsGroup:
                self.collectionArray = self->_libraryModel.listOfRecentMedia;
                break;
            default:
                return;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            [self->_collectionView reloadData];
        });
    });
}

- (void)reloadDataForMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
{
    NSUInteger mediaItemIndex = [self indexOfMediaItemInCollection:mediaItem.libraryID];
    if (mediaItemIndex == NSNotFound) {
        return;
    }

    NSMutableArray * const mutableCollectionCopy = [self.collectionArray mutableCopy];
    [mutableCollectionCopy replaceObjectAtIndex:mediaItemIndex withObject:mediaItem];
    self.collectionArray = [mutableCollectionCopy copy];

    NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:mediaItemIndex inSection:0];
    [self->_collectionView reloadItemsAtIndexPaths:[NSSet setWithObject:indexPath]];
}

- (void)deleteDataForMediaItem:(VLCMediaLibraryMediaItem * const)mediaItem
{
    NSUInteger mediaItemIndex = [self indexOfMediaItemInCollection:mediaItem.libraryID];
    if (mediaItemIndex == NSNotFound) {
        return;
    }

    NSMutableArray * const mutableCollectionCopy = [self.collectionArray mutableCopy];
    [mutableCollectionCopy removeObjectAtIndex:mediaItemIndex];
    self.collectionArray = [mutableCollectionCopy copy];

    NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:mediaItemIndex inSection:0];
    [self->_collectionView deleteItemsAtIndexPaths:[NSSet setWithObject:indexPath]];
}

- (void)setGroupDescriptor:(VLCLibraryVideoCollectionViewGroupDescriptor *)groupDescriptor
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
    VLCLibraryCollectionViewFlowLayout *collectionViewLayout = (VLCLibraryCollectionViewFlowLayout*)_collectionView.collectionViewLayout;
    NSAssert(collectionViewLayout, @"Collection view must have a VLCLibraryCollectionViewFlowLayout!");

    _collectionViewFlowLayout = collectionViewLayout;
    _collectionView.dataSource = self;

    [_collectionView registerClass:[VLCLibraryCollectionViewItem class]
             forItemWithIdentifier:VLCLibraryCellIdentifier];

    [_collectionView registerClass:[VLCLibraryCollectionViewSupplementaryElementView class]
        forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                    withIdentifier:VLCLibrarySupplementaryElementViewIdentifier];

    NSNib *mediaItemSupplementaryDetailView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewMediaItemSupplementaryDetailView" bundle:nil];
    [_collectionView registerNib:mediaItemSupplementaryDetailView
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
    VLCLibraryCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    viewItem.representedItem = self.collectionArray[indexPath.item];
    return viewItem;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView
viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
               atIndexPath:(NSIndexPath *)indexPath
{
    if([kind isEqualToString:NSCollectionElementKindSectionHeader]) {
        VLCLibraryCollectionViewSupplementaryElementView *sectionHeadingView = [collectionView makeSupplementaryViewOfKind:kind
                                                                                                            withIdentifier:VLCLibrarySupplementaryElementViewIdentifier
                                                                                                              forIndexPath:indexPath];

        sectionHeadingView.stringValue = _groupDescriptor.name;
        return sectionHeadingView;

    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView* mediaItemSupplementaryDetailView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind forIndexPath:indexPath];

        mediaItemSupplementaryDetailView.representedMediaItem = self.collectionArray[indexPath.item];
        mediaItemSupplementaryDetailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return mediaItemSupplementaryDetailView;
    }

    return nil;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    return self.collectionArray[indexPath.item];
}

@end
