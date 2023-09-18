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
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryWindow.h"

#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryAudioGroupHeaderView.h"
#import "library/audio-library/VLCLibraryCollectionViewAlbumSupplementaryDetailView.h"

#import "views/VLCSubScrollView.h"

@interface VLCLibraryAudioGroupDataSource ()
{
    id<VLCMediaLibraryAudioGroupProtocol> _representedAudioGroup;
}
@property (readwrite, atomic, strong) NSArray<VLCMediaLibraryAlbum *> *representedListOfAlbums;

@end

@implementation VLCLibraryAudioGroupDataSource

+ (void)setupCollectionView:(NSCollectionView *)collectionView
{
    NSNib * const audioGroupHeaderView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryAudioGroupHeaderView"
                                                                  bundle:nil];
    [collectionView registerNib:audioGroupHeaderView
     forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                 withIdentifier:VLCLibraryAudioGroupHeaderViewIdentifier];
}

- (void)reloadViews
{
    for (NSTableView * const tableView in _tableViews) {
        [tableView reloadData];
    }

    for (NSCollectionView * const collectionView in _collectionViews) {
        [collectionView reloadData];
    }
}

- (void)updateRepresentedListOfAlbums
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), ^{
        VLCLibraryModel * const libraryModel = VLCMain.sharedInstance.libraryController.libraryModel;
        if (self.representedAudioGroup == nil) {
            self.representedListOfAlbums = libraryModel.listOfAlbums;
        } else {
            self.representedListOfAlbums = self.representedAudioGroup.albums;
        }

        dispatch_async(dispatch_get_main_queue(), ^{
            [self reloadViews];
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
    if (row < 0 || row >= self.representedListOfAlbums.count) {
        return nil;
    }

    return self.representedListOfAlbums[row];
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
    VLCLibraryCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    viewItem.representedItem = self.representedListOfAlbums[indexPath.item];
    return viewItem;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView
viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
               atIndexPath:(NSIndexPath *)indexPath
{
    if ([kind isEqualToString:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind]) {

        VLCLibraryCollectionViewAlbumSupplementaryDetailView* albumSupplementaryDetailView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind forIndexPath:indexPath];

        VLCMediaLibraryAlbum * const album = self.representedListOfAlbums[indexPath.item];
        albumSupplementaryDetailView.representedAlbum = album;
        albumSupplementaryDetailView.selectedItem = [collectionView itemAtIndex:indexPath.item];
        albumSupplementaryDetailView.parentScrollView = VLCMain.sharedInstance.libraryWindow.audioCollectionViewScrollView;
        albumSupplementaryDetailView.internalScrollView.scrollParentY = YES;

        return albumSupplementaryDetailView;

    } else if ([kind isEqualToString:NSCollectionElementKindSectionHeader]) {
        VLCLibraryAudioGroupHeaderView * const headerView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryAudioGroupHeaderViewIdentifier forIndexPath:indexPath];
        headerView.representedItem = _representedAudioGroup;
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

@end
