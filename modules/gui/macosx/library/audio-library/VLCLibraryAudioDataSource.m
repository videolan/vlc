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

#import "library/VLCInputItem.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryTableView.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"

#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryAudioGroupDataSource.h"
#import "library/audio-library/VLCLibraryCollectionViewAlbumSupplementaryDetailView.h"
#import "library/audio-library/VLCLibraryCollectionViewAudioGroupSupplementaryDetailView.h"
#import "library/audio-library/VLCLibrarySongsTableViewSongPlayingTableCellView.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSPasteboardItem+VLCAdditions.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlaylistItem.h"
#import "playlist/VLCPlaylistModel.h"

#import "views/VLCImageView.h"
#import "views/VLCSubScrollView.h"

NSString * const VLCLibrarySongsTableViewSongPlayingColumnIdentifier = @"VLCLibrarySongsTableViewSongPlayingColumnIdentifier";
NSString * const VLCLibrarySongsTableViewTitleColumnIdentifier = @"VLCLibrarySongsTableViewTitleColumnIdentifier";
NSString * const VLCLibrarySongsTableViewDurationColumnIdentifier = @"VLCLibrarySongsTableViewDurationColumnIdentifier";
NSString * const VLCLibrarySongsTableViewArtistColumnIdentifier = @"VLCLibrarySongsTableViewArtistColumnIdentifier";
NSString * const VLCLibrarySongsTableViewAlbumColumnIdentifier = @"VLCLibrarySongsTableViewAlbumColumnIdentifier";
NSString * const VLCLibrarySongsTableViewGenreColumnIdentifier = @"VLCLibrarySongsTableViewGenreColumnIdentifier";
NSString * const VLCLibrarySongsTableViewPlayCountColumnIdentifier = @"VLCLibrarySongsTableViewPlayCountColumnIdentifier";
NSString * const VLCLibrarySongsTableViewYearColumnIdentifier = @"VLCLibrarySongsTableViewYearColumnIdentifier";

NSString * const VLCLibraryTitleSortDescriptorKey = @"VLCLibraryTitleSortDescriptorKey";
NSString * const VLCLibraryDurationSortDescriptorKey = @"VLCLibraryDurationSortDescriptorKey";
NSString * const VLCLibraryArtistSortDescriptorKey = @"VLCLibraryArtistSortDescriptorKey";
NSString * const VLCLibraryAlbumSortDescriptorKey = @"VLCLibraryAlbumSortDescriptorKey";
NSString * const VLCLibraryPlayCountSortDescriptorKey = @"VLCLibraryPlayCountSortDescriptorKey";
NSString * const VLCLibraryYearSortDescriptorKey = @"VLCLibraryYearSortDescriptorKey";
// TODO: Add sorting by genre

@interface VLCLibraryAudioDataSource ()
{
    NSArray *_displayedCollection;
    enum vlc_ml_parent_type _currentParentType;

    id<VLCMediaLibraryItemProtocol> _selectedCollectionViewItem;
    id<VLCMediaLibraryItemProtocol> _selectedCollectionSelectionTableViewItem;
    id<VLCMediaLibraryItemProtocol> _selectedGroupSelectionTableViewItem;
    id<VLCMediaLibraryItemProtocol> _selectedSongTableViewItem;
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
                                   name:VLCLibraryModelAudioMediaListReset
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
        [notificationCenter addObserver:self
                               selector:@selector(playlistItemChanged:)
                                   name:VLCPlaylistCurrentItemChanged
                                 object:nil];
    }

    return self;
}

- (void)playlistItemChanged:(NSNotification *)aNotification
{
    NSParameterAssert(aNotification);
    VLCPlaylistController *playlistController = (VLCPlaylistController *)aNotification.object;
    NSAssert(playlistController, @"Should receive valid playlist controller from notification");
    VLCPlaylistModel *playlistModel = playlistController.playlistModel;
    NSAssert(playlistModel, @"Should receive valid playlist model");

    // If we use the playlist's currentPlayingItem we will get the same item we had before.
    // Let's instead grab the playlist item from the playlist model, as we know this is
    // updated before the VLCPlaylistCurrentItemChanged notification is sent out
    size_t currentPlaylistIndex = playlistController.currentPlaylistIndex;
    if (currentPlaylistIndex < 0) {
        return;
    }

    VLCPlaylistItem *currentPlayingItem = [playlistModel playlistItemAtIndex:currentPlaylistIndex];
    if (!currentPlayingItem) {
        return;
    }

    VLCInputItem *currentInputItem = currentPlayingItem.inputItem;
    if (!currentPlayingItem) {
        return;
    }

    if (_currentParentType == VLC_ML_PARENT_UNKNOWN) {
        NSString *currentItemMrl = currentInputItem.MRL;

        NSUInteger itemIndexInDisplayedCollection = [self->_displayedCollection indexOfObjectPassingTest:^BOOL(id element, NSUInteger idx, BOOL *stop) {
            VLCMediaLibraryMediaItem *mediaItem = (VLCMediaLibraryMediaItem *)element;
            return [mediaItem.inputItem.MRL isEqualToString:currentItemMrl];
        }];

        if (itemIndexInDisplayedCollection != NSNotFound) {
            [_songsTableView scrollRowToVisible:itemIndexInDisplayedCollection];
        }
    }
}

- (NSArray *)collectionToDisplay
{
    switch(_currentParentType) {
        case VLC_ML_PARENT_UNKNOWN:
            return [_libraryModel listOfAudioMedia];
        case VLC_ML_PARENT_ALBUM:
            return [_libraryModel listOfAlbums];
        case VLC_ML_PARENT_ARTIST:
            return [_libraryModel listOfArtists];
        case VLC_ML_PARENT_GENRE:
            return [_libraryModel listOfGenres];
        default:
            return nil;
    }
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    if(_libraryModel == nil) {
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        [self reloadData];
    });
}

- (void)retainSelectedMediaItem
{
    if(_collectionView.selectionIndexPaths.count > 0 && !_collectionView.hidden) {
        _selectedCollectionViewItem = [self selectedCollectionViewItem];
    }

    const NSInteger collectionSelectionTableViewRow = _collectionSelectionTableView.selectedRow;
    if(collectionSelectionTableViewRow >= 0 && !_collectionSelectionTableView.hidden) {
        _selectedCollectionSelectionTableViewItem = [self libraryItemAtRow:collectionSelectionTableViewRow
                                                              forTableView:_collectionSelectionTableView];
    }

    const NSInteger groupSelectionTableViewRow = _groupSelectionTableView.selectedRow;
    if(groupSelectionTableViewRow >= 0 && !_groupSelectionTableView.hidden) {
        _selectedGroupSelectionTableViewItem = [self libraryItemAtRow:groupSelectionTableViewRow
                                                         forTableView:_groupSelectionTableView];
    }

    const NSInteger songsTableViewRow = _songsTableView.selectedRow;
    if(songsTableViewRow >= 0 && !_songsTableView.hidden) {
        _selectedSongTableViewItem = [self libraryItemAtRow:songsTableViewRow
                                               forTableView:_songsTableView];
    }
}

- (id<VLCMediaLibraryItemProtocol>)selectedCollectionViewItem
{
    NSIndexPath *indexPath = _collectionView.selectionIndexPaths.anyObject;
    if (!indexPath) {
        return nil;
    }

    return _displayedCollection[indexPath.item];
}

- (void)restoreSelectionState
{
    [self restoreCollectionViewSelectionState];
    [self restoreCollectionSelectionTableViewSelectionState];
    [self restoreGroupSelectionTableViewSelectionState];
    [self restoreSongTableViewSelectionState];
}

- (NSUInteger)findSelectedItemNewIndex:(id<VLCMediaLibraryItemProtocol>)item
{
    return [_displayedCollection indexOfObjectPassingTest:^BOOL(id element, NSUInteger idx, BOOL *stop) {
        id<VLCMediaLibraryItemProtocol> itemElement = (id<VLCMediaLibraryItemProtocol>)element;
        return itemElement.libraryID == item.libraryID;
    }];
}

- (void)restoreCollectionViewSelectionState
{
    if (!_selectedCollectionViewItem) {
        return;
    }

    const NSUInteger newIndexOfSelectedItem = [self findSelectedItemNewIndex:_selectedCollectionViewItem];
    if(newIndexOfSelectedItem == NSNotFound) {
        return;
    }

    NSIndexPath *newIndexPath = [NSIndexPath indexPathForItem:newIndexOfSelectedItem inSection:0];
    NSSet *indexPathSet = [NSSet setWithObject:newIndexPath];
    [_collectionView selectItemsAtIndexPaths:indexPathSet scrollPosition:NSCollectionViewScrollPositionTop];
    // selectItemsAtIndexPaths does not call any delegate methods so we do it manually
    [_collectionView.delegate collectionView:_collectionView didSelectItemsAtIndexPaths:indexPathSet];
    _selectedCollectionViewItem = nil;
}

- (void)restoreSelectionStateForTableView:(NSTableView*)tableView
                         withSelectedItem:(id<VLCMediaLibraryItemProtocol>)item
{
    const NSUInteger newIndexOfSelectedItem = [self findSelectedItemNewIndex:item];
    if(newIndexOfSelectedItem == NSNotFound || newIndexOfSelectedItem < 0) {
        return;
    }

    NSIndexSet *newSelectedRowIndexSet = [NSIndexSet indexSetWithIndex:newIndexOfSelectedItem];
    [tableView selectRowIndexes:newSelectedRowIndexSet byExtendingSelection:NO];
}

- (void)restoreCollectionSelectionTableViewSelectionState
{
    [self restoreSelectionStateForTableView:_collectionSelectionTableView
                           withSelectedItem:_selectedCollectionSelectionTableViewItem];
    _selectedCollectionSelectionTableViewItem = nil;
}

- (void)restoreGroupSelectionTableViewSelectionState
{
    [self restoreSelectionStateForTableView:_groupSelectionTableView
                           withSelectedItem:_selectedGroupSelectionTableViewItem];
    _selectedGroupSelectionTableViewItem = nil;
}

- (void)restoreSongTableViewSelectionState
{
    [self restoreSelectionStateForTableView:_songsTableView
                           withSelectedItem:_selectedSongTableViewItem];
    _selectedSongTableViewItem = nil;
}

- (void)setup
{
    [self setupCollectionView:_collectionView];
    [self setupCollectionView:_gridModeListSelectionCollectionView];
    [self setupTableViews];

    _audioLibrarySegment = -1; // Force setAudioLibrarySegment to do something always on first try
}

- (void)setupCollectionView:(NSCollectionView *)collectionView
{
    [collectionView registerClass:[VLCLibraryCollectionViewItem class] forItemWithIdentifier:VLCLibraryCellIdentifier];

    NSNib *albumSupplementaryDetailView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewAlbumSupplementaryDetailView" bundle:nil];
    [collectionView registerNib:albumSupplementaryDetailView
      forSupplementaryViewOfKind:VLCLibraryCollectionViewAlbumSupplementaryDetailViewKind
                  withIdentifier:VLCLibraryCollectionViewAlbumSupplementaryDetailViewIdentifier];

    NSNib *audioGroupSupplementaryDetailView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewAudioGroupSupplementaryDetailView" bundle:nil];
    [collectionView registerNib:audioGroupSupplementaryDetailView
      forSupplementaryViewOfKind:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewKind
                  withIdentifier:VLCLibraryCollectionViewAudioGroupSupplementaryDetailViewIdentifier];

    NSNib *mediaItemSupplementaryDetailView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewMediaItemSupplementaryDetailView" bundle:nil];
    [collectionView registerNib:mediaItemSupplementaryDetailView
      forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                  withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier];
}

- (void)setupTableViews
{
    _groupSelectionTableView.target = self;
    _groupSelectionTableView.doubleAction = @selector(groubSelectionDoubleClickAction:);

    _collectionSelectionTableView.target = self;
    _collectionSelectionTableView.doubleAction = @selector(collectionSelectionDoubleClickAction:);

    _gridModeListTableView.target = self;
    _gridModeListTableView.doubleAction = @selector(groubSelectionDoubleClickAction:);

    [self setupSongsTableView];
}

- (void)setupSongsTableView
{
    _songsTableView.target = self;
    _songsTableView.doubleAction = @selector(songDoubleClickAction:);

    [self setupPrototypeSortDescriptorsForTableView:_songsTableView];
    [self setupExistingSortForTableView:_songsTableView];
}

- (void)setupPrototypeSortDescriptorsForTableView:(NSTableView *)tableView
{
    for(NSTableColumn *column in tableView.tableColumns) {
        NSSortDescriptor * const columnSortDescriptor = [self sortDescriptorPrototypeForSongsTableViewColumnIdentifier:column.identifier];

        if(columnSortDescriptor) {
            column.sortDescriptorPrototype = columnSortDescriptor;
        }
    }
}

- (NSSortDescriptor *)sortDescriptorPrototypeForSongsTableViewColumnIdentifier:(NSString *)columnIdentifier
{
    if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewTitleColumnIdentifier]) {
        return [[NSSortDescriptor alloc] initWithKey:VLCLibraryTitleSortDescriptorKey ascending:true];

    } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewDurationColumnIdentifier]) {
        return [[NSSortDescriptor alloc] initWithKey:VLCLibraryDurationSortDescriptorKey ascending:true];

    } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewArtistColumnIdentifier]) {
        return [[NSSortDescriptor alloc] initWithKey:VLCLibraryArtistSortDescriptorKey ascending:true];

    } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewAlbumColumnIdentifier]) {
        return [[NSSortDescriptor alloc] initWithKey:VLCLibraryAlbumSortDescriptorKey ascending:true];

    } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewPlayCountColumnIdentifier]) {
        return [[NSSortDescriptor alloc] initWithKey:VLCLibraryPlayCountSortDescriptorKey ascending:true];

    } else if ([columnIdentifier isEqualToString:VLCLibrarySongsTableViewYearColumnIdentifier]) {
        return [[NSSortDescriptor alloc] initWithKey:VLCLibraryYearSortDescriptorKey ascending:true];

    }

    return nil;
}

- (void)setupExistingSortForTableView:(NSTableView *)tableView
{
    const VLCLibraryController * const libraryController = VLCMain.sharedInstance.libraryController;
    const vlc_ml_sorting_criteria_t existingSortCriteria = libraryController.lastSortingCriteria;

    NSString *sortDescriptorKey = [self sortDescriptorKeyFromVlcMlSortingCriteria:existingSortCriteria];
    const NSSortDescriptor * const sortDescriptor = [[NSSortDescriptor alloc] initWithKey:sortDescriptorKey
                                                                                ascending:!libraryController.descendingLibrarySorting];

    tableView.sortDescriptors = @[sortDescriptor];
}

- (NSString *)sortDescriptorKeyFromVlcMlSortingCriteria:(vlc_ml_sorting_criteria_t)existingSortCriteria
{
    if (existingSortCriteria == VLC_ML_SORTING_DEFAULT) {
        return VLCLibraryTitleSortDescriptorKey;

    } else if (existingSortCriteria == VLC_ML_SORTING_DURATION) {
        return VLCLibraryDurationSortDescriptorKey;

    } else if (existingSortCriteria == VLC_ML_SORTING_ARTIST) {
        return VLCLibraryArtistSortDescriptorKey;

    } else if (existingSortCriteria == VLC_ML_SORTING_ALBUM) {
        return VLCLibraryAlbumSortDescriptorKey;

    } else if (existingSortCriteria == VLC_ML_SORTING_PLAYCOUNT) {
        return VLCLibraryPlayCountSortDescriptorKey;

    } else if (existingSortCriteria == VLC_ML_SORTING_RELEASEDATE) {
        return VLCLibraryYearSortDescriptorKey;

    }

    return VLCLibraryTitleSortDescriptorKey;
}

- (void)reloadViewsWithCompletion:(void(^)(void))completionHandler
{
    VLCLibraryCollectionViewFlowLayout *collectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout *)_collectionView.collectionViewLayout;
    if (collectionViewFlowLayout) {
        [collectionViewFlowLayout resetLayout];
    }

    VLCLibraryCollectionViewFlowLayout *gridModeListSelectionCollectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout *)_gridModeListSelectionCollectionView.collectionViewLayout;
    if (gridModeListSelectionCollectionViewFlowLayout) {
        [gridModeListSelectionCollectionViewFlowLayout resetLayout];
    }

    completionHandler();
    [self setupExistingSortForTableView:_songsTableView];
}

- (void)reloadDataWithCompletion:(void(^)(void))completionHandler
{
    [self retainSelectedMediaItem];
    self->_displayedCollection = [self collectionToDisplay];
    [self reloadViewsWithCompletion:completionHandler];
    [self restoreSelectionState];
}

- (void)reloadData
{
    [self reloadDataWithCompletion:^{
        [self.collectionView reloadData];
        [self.gridModeListTableView reloadData];
        [self.gridModeListSelectionCollectionView reloadData];
        [self.collectionSelectionTableView reloadData];
        [self.groupSelectionTableView reloadData];
        [self.songsTableView reloadData];
    }];
}

- (void)reloadDataForMediaLibraryItem:(VLCMediaLibraryMediaItem*)mediaItemToReload
{
    [self reloadDataWithCompletion:^{
        const NSUInteger index = [self->_displayedCollection indexOfObjectPassingTest:^BOOL(VLCMediaLibraryMediaItem * const mediaItem, const NSUInteger idx, BOOL * const stop) {
            NSAssert(mediaItem != nil, @"Cache list should not contain nil media items");
            return mediaItem.libraryID == mediaItemToReload.libraryID;
        }];

        if (index == NSNotFound) {
            return;
        }

        NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:index inSection:0];
        NSIndexSet * const rowIndexSet = [NSIndexSet indexSetWithIndex:index];

        NSRange songsTableColumnRange = NSMakeRange(0, self->_songsTableView.numberOfColumns);
        NSIndexSet * const songsTableColumnIndexSet = [NSIndexSet indexSetWithIndexesInRange:songsTableColumnRange];

        [self.collectionView reloadItemsAtIndexPaths:[NSSet setWithObject:indexPath]];
        [self.songsTableView reloadDataForRowIndexes:rowIndexSet columnIndexes:songsTableColumnIndexSet];

        // Don't update gridModeListSelectionCollectionView, let its VLCLibraryAudioGroupDataSource do it.
        // TODO: Stop splitting functionality for these audio source selection views between this data source
        // TODO: and the VLCLibraryAudioGroupDataSource, it is super confusing

        // Also don't update:
        // - gridModeListTableView, as this will only show artists/genres
        // - collectionSelectionTableView, as this will only show artists/genres/albums
        // - groupSelectionTableView, as this shows cells for albums (and each cell has its own data source with media items)
    }];
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

    _audioGroupDataSource.representedListOfAlbums = nil; // Clear whatever was being shown before
    [self reloadData];
}

#pragma mark - table view data source and delegation

- (BOOL)displayAllArtistsGenresTableEntry
{
    return _currentParentType == VLC_ML_PARENT_GENRE ||
           _currentParentType == VLC_ML_PARENT_ARTIST;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    NSInteger numItems = _displayedCollection.count;
    return [self displayAllArtistsGenresTableEntry] ? numItems + 1 : numItems;
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    BOOL viewDisplayingAllItemsEntry = [self displayAllArtistsGenresTableEntry];
    BOOL provideAllItemsEntry = viewDisplayingAllItemsEntry && row == 0;

    if (provideAllItemsEntry && _currentParentType == VLC_ML_PARENT_GENRE) {
        return [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:_NS("All genres")
                                                      withDetailString:@""];
    } else if (provideAllItemsEntry && _currentParentType == VLC_ML_PARENT_ARTIST) {
        return [[VLCMediaLibraryDummyItem alloc] initWithDisplayString:_NS("All artists")
                                                      withDetailString:@""];
    } else if (viewDisplayingAllItemsEntry) {
        return _displayedCollection[row - 1];
    }

    return _displayedCollection[row];
}

- (void)tableView:(NSTableView * const)tableView selectRow:(NSInteger)row
{
    NSParameterAssert(tableView);
    
    if (tableView != _collectionSelectionTableView && tableView != _groupSelectionTableView && tableView != _gridModeListTableView) {
        return;
    }

    if (tableView.selectedRow != row) {
        [tableView selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
    }

    const NSInteger selectedRow = tableView.selectedRow;
    const BOOL showingAllItemsEntry = [self displayAllArtistsGenresTableEntry];
    const NSInteger libraryItemIndex = showingAllItemsEntry ? selectedRow - 1 : selectedRow;

    if (libraryItemIndex < 0 && showingAllItemsEntry) {
        _audioGroupDataSource.representedListOfAlbums = _libraryModel.listOfAlbums;
    } else {
        id<VLCMediaLibraryItemProtocol> libraryItem = _displayedCollection[libraryItemIndex];

        if (_currentParentType == VLC_ML_PARENT_ALBUM) {
            _audioGroupDataSource.representedListOfAlbums = @[(VLCMediaLibraryAlbum *)libraryItem];
        } else if(_currentParentType != VLC_ML_PARENT_UNKNOWN) {
            _audioGroupDataSource.representedListOfAlbums = [_libraryModel listAlbumsOfParentType:_currentParentType forID:libraryItem.libraryID];
        } else { // FIXME: we have nothing to show here
            _audioGroupDataSource.representedListOfAlbums = nil;
        }
    }

    [self.groupSelectionTableView reloadData];
    [self.gridModeListSelectionCollectionView reloadData];
}

- (void)tableView:(NSTableView *)tableView sortDescriptorsDidChange:(NSArray<NSSortDescriptor *> *)oldDescriptors
{
    const NSSortDescriptor * const sortDescriptor = tableView.sortDescriptors.firstObject;
    const vlc_ml_sorting_criteria_t sortCriteria = [self sortDescriptorKeyToVlcMlSortingCriteria:sortDescriptor.key];

    [VLCMain.sharedInstance.libraryController sortByCriteria:sortCriteria andDescending:!sortDescriptor.ascending];
    [self reloadData];
}

- (vlc_ml_sorting_criteria_t)sortDescriptorKeyToVlcMlSortingCriteria:(NSString *)sortDescriptorKey
{
    if ([sortDescriptorKey isEqualToString:VLCLibraryTitleSortDescriptorKey]) {
        return VLC_ML_SORTING_DEFAULT;

    } else if ([sortDescriptorKey isEqualToString:VLCLibraryDurationSortDescriptorKey]) {
        return VLC_ML_SORTING_DURATION;

    } else if ([sortDescriptorKey isEqualToString:VLCLibraryArtistSortDescriptorKey]) {
        return VLC_ML_SORTING_ARTIST;

    } else if ([sortDescriptorKey isEqualToString:VLCLibraryAlbumSortDescriptorKey]) {
        return VLC_ML_SORTING_ALBUM;

    } else if ([sortDescriptorKey isEqualToString:VLCLibraryPlayCountSortDescriptorKey]) {
        return VLC_ML_SORTING_PLAYCOUNT;

    } else if ([sortDescriptorKey isEqualToString:VLCLibraryYearSortDescriptorKey]) {
        return VLC_ML_SORTING_RELEASEDATE;

    }

    return VLC_ML_SORTING_DEFAULT;
}

- (id<NSPasteboardWriting>)tableView:(NSTableView *)tableView pasteboardWriterForRow:(NSInteger)row
{
    const id<VLCMediaLibraryItemProtocol> libraryItem = [self libraryItemAtRow:row forTableView:tableView];

    return [NSPasteboardItem pasteboardItemWithLibraryItem:libraryItem];
}

#pragma mark - table view double click actions

- (void)groubSelectionDoubleClickAction:(id)sender
{
    NSArray *listOfAlbums = _audioGroupDataSource.representedListOfAlbums;
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

- (void)songDoubleClickAction:(id)sender
{
    NSAssert(_audioLibrarySegment == VLCAudioLibrarySongsSegment, @"Should not be possible to trigger this action from a non-song library view");
    VLCMediaLibraryMediaItem *mediaItem = _displayedCollection[_songsTableView.selectedRow];
    [VLCMain.sharedInstance.libraryController appendItemToPlaylist:mediaItem playImmediately:YES];
}

#pragma mark - collection view data source

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

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtIndexPath:(NSIndexPath *)indexPath
                                        forCollectionView:(NSCollectionView *)collectionView
{
    return _displayedCollection[indexPath.item];
}

@end
