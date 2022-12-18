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
    NSArray *_collectionArray;
    VLCLibraryCollectionViewFlowLayout *_collectionViewFlowLayout;
    VLCLibraryModel *_libraryModel;
}
@end

@implementation VLCLibraryVideoCollectionViewContainerViewDataSource

- (instancetype)init
{
    self = [super init];
    if(self) {
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelVideoMediaListUpdated
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelRecentMediaListUpdated
                                 object:nil];
        _libraryModel = [VLCMain sharedInstance].libraryController.libraryModel;
    }
    return self;
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    [self reloadData];
}

- (void)reloadData
{
    if(!_collectionView || !_groupDescriptor) {
        NSLog(@"Null collection view or video group descriptor");
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        NSAssert(self->_groupDescriptor.libraryModelDataMethodSignature, @"Group descriptor's library model data method signature cannot be nil");

        NSInvocation *modelDataInvocation = [NSInvocation invocationWithMethodSignature:self->_groupDescriptor.libraryModelDataMethodSignature];
        modelDataInvocation.selector = self->_groupDescriptor.libraryModelDataSelector;
        [modelDataInvocation invokeWithTarget:self->_libraryModel];
        [modelDataInvocation getReturnValue:&self->_collectionArray];

        [self->_collectionView reloadData];
    });
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
    _collectionView.delegate = self;

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

    return _collectionArray.count;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    viewItem.representedItem = _collectionArray[indexPath.item];
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

        mediaItemSupplementaryDetailView.representedMediaItem = _collectionArray[indexPath.item];
        mediaItemSupplementaryDetailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return mediaItemSupplementaryDetailView;
    }

    return nil;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath *indexPath = indexPaths.anyObject;
    if (!indexPath) {
        NSLog(@"Bad index path on item selection");
        return;
    }

    [_collectionViewFlowLayout expandDetailSectionAtIndex:indexPath];
}

- (void)collectionView:(NSCollectionView *)collectionView didDeselectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath *indexPath = indexPaths.anyObject;
    if (!indexPath) {
        NSLog(@"Bad index path on item deselection");
        return;
    }

    [_collectionViewFlowLayout collapseDetailSectionAtIndex:indexPath];
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
    NSUInteger numberOfIndexPaths = indexPaths.count;
    NSMutableArray *encodedLibraryItemsArray = [NSMutableArray arrayWithCapacity:numberOfIndexPaths];
    NSMutableArray *filePathsArray = [NSMutableArray arrayWithCapacity:numberOfIndexPaths];

    for (NSIndexPath *indexPath in indexPaths) {
        VLCMediaLibraryMediaItem *mediaItem = _collectionArray[indexPath.item];
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

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    return _collectionArray[indexPath.item];
}

@end
