/*****************************************************************************
 * VLCLibraryAudioDataSource.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan -dot- org>
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

#import "VLCLibraryAudioDataSource.h"

#import "main/VLCMain.h"

#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryNavigationStack.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"

#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryCollectionViewAlbumSupplementaryDetailView.h"
#import "library/audio-library/VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.h"

#import "extensions/NSString+Helpers.h"
#import "views/VLCImageView.h"
#import "views/VLCSubScrollView.h"

@interface VLCLibraryAudioDataSource ()
{
    VLCLibraryCollectionViewFlowLayout *_collectionViewFlowLayout;
    NSArray *_displayedCollection;
    enum vlc_ml_parent_type _currentParentType;
}
@end

@implementation VLCLibraryAudioDataSource

- (instancetype)init
{
    self = [super init];
    if(self) {
        NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelAudioMediaListUpdated
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelArtistListUpdated
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelAlbumListUpdated
                                 object:nil];
        [notificationCenter addObserver:self
                               selector:@selector(libraryModelUpdated:)
                                   name:VLCLibraryModelGenreListUpdated
                                 object:nil];
    }

    return self;
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    if(self.libraryModel == nil) {
        return;
    }
    
    NSArray *collectionToDisplay;

    switch(_currentParentType) {
        case VLC_ML_PARENT_UNKNOWN:
            collectionToDisplay = [self.libraryModel listOfAudioMedia];
            break;
        case VLC_ML_PARENT_ALBUM:
            collectionToDisplay = [self.libraryModel listOfAlbums];
            break;
        case VLC_ML_PARENT_ARTIST:
            collectionToDisplay = [self.libraryModel listOfArtists];
            break;
        case VLC_ML_PARENT_GENRE:
            collectionToDisplay = [self.libraryModel listOfGenres];
            break;
        default:
            return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        NSSet* originalCollectionSet = [[NSSet alloc] initWithArray:_displayedCollection];
        NSSet* newCollectionSet = [[NSSet alloc] initWithArray:collectionToDisplay];

        if([originalCollectionSet isEqual:newCollectionSet]) {
            return;
        }

        id<VLCMediaLibraryItemProtocol> selectedCollectionViewItem;
        if(_collectionView.selectionIndexPaths.count > 0 && !_collectionView.hidden) {
            selectedCollectionViewItem = [self selectedCollectionViewItem];
        }

        _displayedCollection = collectionToDisplay;
        [self reloadData];

        // Reopen supplementary view in the collection views
        if(selectedCollectionViewItem) {
            NSUInteger newIndexOfSelectedItem = [_displayedCollection indexOfObjectPassingTest:^BOOL(id element, NSUInteger idx, BOOL *stop) {
                id<VLCMediaLibraryItemProtocol> itemElement = (id<VLCMediaLibraryItemProtocol>)element;
                return itemElement.libraryID == selectedCollectionViewItem.libraryID;
            }];

            if(newIndexOfSelectedItem == NSNotFound) {
                return;
            }

            NSIndexPath *newIndexPath = [NSIndexPath indexPathForItem:newIndexOfSelectedItem inSection:0];
            NSSet *indexPathSet = [NSSet setWithObject:newIndexPath];
            [_collectionView selectItemsAtIndexPaths:indexPathSet scrollPosition:NSCollectionViewScrollPositionTop];
            // selectItemsAtIndexPaths does not call any delegate methods so we do it manually
            [self collectionView:_collectionView didSelectItemsAtIndexPaths:indexPathSet];
        }
    });
}

- (id<VLCMediaLibraryItemProtocol>)selectedCollectionViewItem
{
    NSIndexPath *indexPath = _collectionView.selectionIndexPaths.anyObject;
    if (!indexPath) {
        return nil;
    }

    return _displayedCollection[indexPath.item];
}

- (void)setup
{
    _collectionView.dataSource = self;
    _collectionView.delegate = self;

    [_collectionView registerClass:[VLCLibraryCollectionViewItem class] forItemWithIdentifier:VLCLibraryCellIdentifier];

    NSNib *albumSupplementaryDetailView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewAlbumSupplementaryDetailView" bundle:nil];
    [_collectionView registerNib:albumSupplementaryDetailView
      forSupplementaryViewOfKind:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind 
                  withIdentifier:VLCLibraryCollectionViewAlbumSupplementaryDetailViewIdentifier];

    NSNib *audioGroupSupplementaryDetailView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewAudioGroupSupplementaryDetailView" bundle:nil];
    [_collectionView registerNib:audioGroupSupplementaryDetailView
      forSupplementaryViewOfKind:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind 
                  withIdentifier:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewIdentifier];

    NSNib *mediaItemSupplementaryDetailView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewMediaItemSupplementaryDetailView" bundle:nil];
    [_collectionView registerNib:mediaItemSupplementaryDetailView
      forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                  withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier];

    _collectionViewFlowLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];
    _collectionView.collectionViewLayout = _collectionViewFlowLayout;

    _groupSelectionTableView.target = self;
    _groupSelectionTableView.doubleAction = @selector(groubSelectionDoubleClickAction:);
    _collectionSelectionTableView.target = self;
    _collectionSelectionTableView.doubleAction = @selector(collectionSelectionDoubleClickAction:);

    _audioLibrarySegment = -1; // Force setAudioLibrarySegment to do something always on first try
}

- (void)reloadData
{
    [_collectionViewFlowLayout resetLayout];
    [self.collectionView reloadData];
    [self.collectionSelectionTableView reloadData];
    [self.groupSelectionTableView reloadData];
}

- (void)setAudioLibrarySegment:(VLCAudioLibrarySegment)audioLibrarySegment
{
    if (audioLibrarySegment == _audioLibrarySegment) {
        return;
    }

    _audioLibrarySegment = audioLibrarySegment;
    switch (_audioLibrarySegment) {
        case VLCAudioLibraryArtistsSegment:
            _displayedCollection = [self.libraryModel listOfArtists];
            _currentParentType = VLC_ML_PARENT_ARTIST;
            break;
        case VLCAudioLibraryAlbumsSegment:
            _displayedCollection = [self.libraryModel listOfAlbums];
            _currentParentType = VLC_ML_PARENT_ALBUM;
            break;
        case VLCAudioLibrarySongsSegment:
            _displayedCollection = [self.libraryModel listOfAudioMedia];
            _currentParentType = VLC_ML_PARENT_UNKNOWN;
            break;
        case VLCAudioLibraryGenresSegment:
            _displayedCollection = [self.libraryModel listOfGenres];
            _currentParentType = VLC_ML_PARENT_GENRE;
            break;

        default:
            NSAssert(1, @"reached the unreachable");
            break;
    }

    _groupDataSource.representedListOfAlbums = nil; // Clear whatever was being shown before
    [self reloadData];
}

#pragma mark - table view data source and delegation

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _displayedCollection.count;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibraryTableCellView *cellView = [tableView makeViewWithIdentifier:VLCAudioLibraryCellIdentifier owner:self];

    if (cellView == nil) {
        cellView = [VLCLibraryTableCellView fromNibWithOwner:self];
        cellView.identifier = VLCAudioLibraryCellIdentifier;
    }

    [cellView setRepresentedItem:[self libraryItemAtRow:row]];
    return cellView;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
{
    return _displayedCollection[row];
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    id<VLCMediaLibraryItemProtocol> libraryItem = _displayedCollection[self.collectionSelectionTableView.selectedRow];

    if (_currentParentType == VLC_ML_PARENT_ALBUM) {
        _groupDataSource.representedListOfAlbums = @[(VLCMediaLibraryAlbum *)libraryItem];
    } else if(_currentParentType != VLC_ML_PARENT_UNKNOWN) {
        _groupDataSource.representedListOfAlbums = [_libraryModel listAlbumsOfParentType:_currentParentType forID:libraryItem.libraryID];
    } else { // FIXME: we have nothing to show here
        _groupDataSource.representedListOfAlbums = nil;
    }

    [self.groupSelectionTableView reloadData];
}

#pragma mark - table view double click actions

- (void)groubSelectionDoubleClickAction:(id)sender
{
    NSArray *listOfAlbums = _groupDataSource.representedListOfAlbums;
    NSUInteger albumCount = listOfAlbums.count;
    NSInteger clickedRow = _groupSelectionTableView.clickedRow;

    if (!listOfAlbums || albumCount == 0 || clickedRow > albumCount) {
        return;
    }

    NSArray *tracks = [listOfAlbums[clickedRow] tracksAsMediaItems];
    [[[VLCMain sharedInstance] libraryController] appendItemsToPlaylist:tracks playFirstItemImmediately:YES];
}

- (void)collectionSelectionDoubleClickAction:(id)sender
{
    id<VLCMediaLibraryItemProtocol> libraryItem = _displayedCollection[self.collectionSelectionTableView.selectedRow];
    
    [libraryItem iterateMediaItemsWithBlock:^(VLCMediaLibraryMediaItem* mediaItem) {
        [[[VLCMain sharedInstance] libraryController] appendItemToPlaylist:mediaItem playImmediately:YES];
    }];
}

#pragma mark - collection view data source and delegation

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    return _displayedCollection.count;
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return 1;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    viewItem.representedItem = _displayedCollection[indexPath.item];
    return viewItem;
}

- (void)collectionView:(NSCollectionView *)collectionView didSelectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath *indexPath = indexPaths.anyObject;
    if (!indexPath) {
        return;
    }

    [_collectionViewFlowLayout expandDetailSectionAtIndex:indexPath];
}

- (void)collectionView:(NSCollectionView *)collectionView didDeselectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    NSIndexPath *indexPath = indexPaths.anyObject;
    if (!indexPath) {
        return;
    }

    [_collectionViewFlowLayout collapseDetailSectionAtIndex:indexPath];
}

- (NSView *)collectionView:(NSCollectionView *)collectionView
viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
               atIndexPath:(NSIndexPath *)indexPath
{
    if ([kind isEqualToString:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind]) {

        VLCLibraryCollectionViewAlbumSupplementaryDetailView* albumSupplementaryDetailView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind forIndexPath:indexPath];

        VLCMediaLibraryAlbum *album = _displayedCollection[indexPath.item];
        albumSupplementaryDetailView.representedAlbum = album;
        albumSupplementaryDetailView.selectedItem = [collectionView itemAtIndex:indexPath.item];
        albumSupplementaryDetailView.parentScrollView = [VLCMain sharedInstance].libraryWindow.audioCollectionViewScrollView;
        albumSupplementaryDetailView.internalScrollView.scrollParentY = YES;

        return albumSupplementaryDetailView;

    } else if ([kind isEqualToString:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind]) {

        VLCLibraryCollectionViewAudioGroupSupplementaryDetailView* audioGroupSupplementaryDetailView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind forIndexPath:indexPath];

        id<VLCMediaLibraryAudioGroupProtocol> audioGroup = _displayedCollection[indexPath.item];
        audioGroupSupplementaryDetailView.representedAudioGroup = audioGroup;
        audioGroupSupplementaryDetailView.selectedItem = [collectionView itemAtIndex:indexPath.item];
        audioGroupSupplementaryDetailView.parentScrollView = [VLCMain sharedInstance].libraryWindow.audioCollectionViewScrollView;
        audioGroupSupplementaryDetailView.internalScrollView.scrollParentY = YES;

        return audioGroupSupplementaryDetailView;

    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {

        VLCLibraryCollectionViewMediaItemSupplementaryDetailView* mediaItemSupplementaryDetailView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind forIndexPath:indexPath];

        VLCMediaLibraryMediaItem *mediaItem = _displayedCollection[indexPath.item];
        mediaItemSupplementaryDetailView.representedMediaItem = mediaItem;
        mediaItemSupplementaryDetailView.selectedItem = [collectionView itemAtIndex:indexPath.item];

        return mediaItemSupplementaryDetailView;
    }

    return nil;
}

@end

@implementation VLCLibraryGroupDataSource

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (_representedListOfAlbums != nil) {
        return _representedListOfAlbums.count;
    }

    return 0;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibraryAlbumTableCellView *cellView = [tableView makeViewWithIdentifier:VLCAudioLibraryCellIdentifier owner:self];

    if (cellView == nil) {
        cellView = [VLCLibraryAlbumTableCellView fromNibWithOwner:self];
        cellView.identifier = VLCAudioLibraryCellIdentifier;
    }

    cellView.representedAlbum = (VLCMediaLibraryAlbum *)[self libraryItemAtRow:row];
    return cellView;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
{
    return _representedListOfAlbums[row];
}

- (CGFloat)tableView:(NSTableView *)tableView heightOfRow:(NSInteger)row
{
    VLCLibraryAlbumTableCellView *cellView = (VLCLibraryAlbumTableCellView *)[self tableView:tableView viewForTableColumn:[[NSTableColumn alloc] initWithIdentifier:VLCLibraryAlbumTableCellTableViewColumnIdentifier] row:row];
    return cellView == nil ? -1 : cellView.height;
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)rowIndex
{
    // We use this with nested table views, since the table view cell is the VLCLibraryAlbumTableCellView.
    // We don't want to select the outer cell, only the inner cells in the album view's table.
    return NO;
}

@end
