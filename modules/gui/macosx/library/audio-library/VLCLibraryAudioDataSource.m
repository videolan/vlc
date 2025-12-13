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
#import "library/VLCLibraryCarouselViewItemView.h"
#import "library/VLCLibraryWindow.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewMediaItemListSupplementaryDetailView.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryUIUnits.h"

#import "library/audio-library/VLCLibraryAlbumTableCellView.h"
#import "library/audio-library/VLCLibraryAllAudioGroupsMediaLibraryItem.h"
#import "library/audio-library/VLCLibraryAudioGroupDataSource.h"
#import "library/audio-library/VLCLibrarySongsTableViewSongPlayingTableCellView.h"

#import "library/home-library/VLCLibraryHomeViewBaseCarouselContainerView.h"

#import "extensions/NSString+Helpers.h"
#import "extensions/NSPasteboardItem+VLCAdditions.h"

#import "playqueue/VLCPlayerController.h"
#import "playqueue/VLCPlayQueueController.h"
#import "playqueue/VLCPlayQueueItem.h"
#import "playqueue/VLCPlayQueueModel.h"

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

NSString * const VLCLibraryAudioDataSourceDisplayedCollectionChangedNotification = @"VLCLibraryAudioDataSourceDisplayedCollectionChangedNotification";

@interface VLCLibraryAudioDataSource ()

@property (readwrite, atomic) NSArray *displayedCollection;
@property (readonly) BOOL displayAllArtistsGenresTableEntry;

@end

@implementation VLCLibraryAudioDataSource

@synthesize currentParentType = _currentParentType;

- (instancetype)init
{
    self = [super init];
    if(self) {
        _displayedCollectionUpdating = NO;
    }

    return self;
}

- (void)currentlyPlayingItemChanged:(NSNotification *)aNotification
{
    VLCPlayerController * const playerController = VLCMain.sharedInstance.playQueueController.playerController;
    VLCInputItem * const currentInputItem = playerController.currentMedia;
    if (!currentInputItem) {
        return;
    }

    if (self.currentParentType == VLCMediaLibraryParentGroupTypeAudioLibrary) {
        NSString * const currentItemMrl = currentInputItem.MRL;

        const NSUInteger itemIndexInDisplayedCollection = [self.displayedCollection indexOfObjectPassingTest:^BOOL(id element, NSUInteger __unused idx, BOOL * const __unused stop) {
            VLCMediaLibraryMediaItem * const mediaItem = (VLCMediaLibraryMediaItem *)element;
            return [mediaItem.inputItem.MRL isEqualToString:currentItemMrl];
        }];

        if (itemIndexInDisplayedCollection != NSNotFound) {
            [self.songsTableView scrollRowToVisible:itemIndexInDisplayedCollection];
        }
    }
}

- (size_t)collectionToDisplayCount
{
    switch(_currentParentType) {
    case VLCMediaLibraryParentGroupTypeAudioLibrary:
        return self.libraryModel.numberOfAudioMedia;
    case VLCMediaLibraryParentGroupTypeRecentAudios:
        return self.libraryModel.numberOfRecentAudioMedia;
    case VLCMediaLibraryParentGroupTypeAlbum:
        return self.libraryModel.numberOfAlbums;
    case VLCMediaLibraryParentGroupTypeArtist:
        return self.libraryModel.numberOfArtists;
    case VLCMediaLibraryParentGroupTypeGenre:
        return self.libraryModel.numberOfGenres;
    default:
        NSAssert(NO, @"current parent type should not be unknown, no collection to display");
        return 0;
    }
}

- (NSInteger)displayedCollectionCount
{
    return self.displayedCollection.count;
}

- (NSArray *)collectionToDisplay
{
    switch(_currentParentType) {
    case VLCMediaLibraryParentGroupTypeAudioLibrary:
        return self.libraryModel.listOfAudioMedia;
    case VLCMediaLibraryParentGroupTypeRecentAudios:
        return self.libraryModel.listOfRecentAudioMedia;
    case VLCMediaLibraryParentGroupTypeAlbum:
        return self.libraryModel.listOfAlbums;
    case VLCMediaLibraryParentGroupTypeArtist:
        return self.libraryModel.listOfArtists;
    case VLCMediaLibraryParentGroupTypeGenre:
        return self.libraryModel.listOfGenres;
    default:
        NSAssert(1, @"current parent type should not be unknown, no collection to display");
        return nil;
    }
}

- (void)libraryModelReset:(NSNotification * const)aNotification
{
    if(self.libraryModel == nil) {
        return;
    }

    [self reloadData];
}

- (void)libraryModelAudioMediaItemsReset:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeAudioLibrary
        && self.currentParentType != VLCMediaLibraryParentGroupTypeRecentAudios) {
        return;
    }

    [self libraryModelReset:aNotification];
}

- (void)libraryModelArtistsReset:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeArtist) {
        return;
    }

    [self libraryModelReset:aNotification];
}

- (void)libraryModelAlbumsReset:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeAlbum) {
        return;
    }

    [self libraryModelReset:aNotification];
}

- (void)libraryModelGenresReset:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeGenre) {
        return;
    }

    [self libraryModelReset:aNotification];
}

- (void)libraryModelAudioItemUpdated:(NSNotification * const)aNotification
{
    NSParameterAssert(aNotification);
    NSParameterAssert([aNotification.object conformsToProtocol:@protocol(VLCMediaLibraryItemProtocol)]);

    if(self.libraryModel == nil) {
        return;
    }

    const id<VLCMediaLibraryItemProtocol> item = (id<VLCMediaLibraryItemProtocol>)aNotification.object;
    [self reloadDataForMediaLibraryItem:item];
}

- (void)libraryModelAudioMediaItemUpdated:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeAudioLibrary
        && self.currentParentType != VLCMediaLibraryParentGroupTypeRecentAudios) {
        return;
    }

    [self libraryModelAudioItemUpdated:aNotification];
}

- (void)libraryModelArtistUpdated:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeArtist) {
        return;
    }

    [self libraryModelAudioItemUpdated:aNotification];
}

- (void)libraryModelAlbumUpdated:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeAlbum) {
        return;
    }

    [self libraryModelAudioItemUpdated:aNotification];
}

- (void)libraryModelGenreUpdated:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeGenre) {
        return;
    }

    [self libraryModelAudioItemUpdated:aNotification];
}

- (void)libraryModelAudioItemDeleted:(NSNotification * const)aNotification
{
    NSParameterAssert(aNotification);
    NSParameterAssert([aNotification.object conformsToProtocol:@protocol(VLCMediaLibraryItemProtocol)]);

    if(self.libraryModel == nil) {
        return;
    }

    const id <VLCMediaLibraryItemProtocol> item = (id<VLCMediaLibraryItemProtocol>)aNotification.object;
    [self deleteDataForMediaLibraryItem:item];
}

- (void)libraryModelAudioMediaItemDeleted:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeAudioLibrary
        && self.currentParentType != VLCMediaLibraryParentGroupTypeRecentAudios) {
        return;
    }

    [self libraryModelAudioItemDeleted:aNotification];
}

- (void)libraryModelArtistDeleted:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeArtist) {
        return;
    }

    [self libraryModelAudioItemDeleted:aNotification];
}

- (void)libraryModelAlbumDeleted:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeAlbum) {
        return;
    }

    [self libraryModelAudioItemDeleted:aNotification];
}

- (void)libraryModelGenreDeleted:(NSNotification * const)aNotification
{
    if (self.currentParentType != VLCMediaLibraryParentGroupTypeGenre) {
        return;
    }

    [self libraryModelAudioMediaItemDeleted:aNotification];
}

- (id<VLCMediaLibraryItemProtocol>)selectedCollectionViewItem
{
    NSIndexPath * const indexPath = self.collectionView.selectionIndexPaths.anyObject;
    if (!indexPath) {
        return nil;
    }

    return self.displayedCollection[indexPath.item];
}

- (NSUInteger)findSelectedItemNewIndex:(id<VLCMediaLibraryItemProtocol>)item
{
    return [self.displayedCollection indexOfObjectPassingTest:^BOOL(const id element, const NSUInteger __unused idx, BOOL * const __unused stop) {
        const id<VLCMediaLibraryItemProtocol> itemElement = (id<VLCMediaLibraryItemProtocol>)element;
        return itemElement.libraryID == item.libraryID;
    }];
}

- (void)setup
{
    [VLCLibraryAudioDataSource setupCollectionView:self.collectionView];
    [self setupTableViews];
    // Force setAudioLibrarySegment to do something always on first try
    _audioLibrarySegment = VLCAudioLibraryUnknownSegment;
    [self connect];
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
                           selector:@selector(libraryModelArtistsReset:)
                               name:VLCLibraryModelArtistListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelArtistUpdated:)
                               name:VLCLibraryModelArtistUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelArtistDeleted:)
                               name:VLCLibraryModelArtistDeleted
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

    [notificationCenter addObserver:self
                           selector:@selector(libraryModelGenresReset:)
                               name:VLCLibraryModelGenreListReset
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelGenreUpdated:)
                               name:VLCLibraryModelGenreUpdated
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(libraryModelGenreDeleted:)
                               name:VLCLibraryModelGenreDeleted
                             object:nil];

    [notificationCenter addObserver:self
                           selector:@selector(currentlyPlayingItemChanged:)
                               name:VLCPlayerCurrentMediaItemChanged
                             object:nil];

    [self.audioGroupDataSource connect];
    [self reloadData];
}

- (void)disconnect
{
    [NSNotificationCenter.defaultCenter removeObserver:self];
    [self.audioGroupDataSource disconnect];
}

+ (void)setupCollectionView:(NSCollectionView *)collectionView
{
    [collectionView registerClass:[VLCLibraryCollectionViewItem class] forItemWithIdentifier:VLCLibraryCellIdentifier];

    NSNib * const albumSupplementaryDetailView =
        [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewMediaItemListSupplementaryDetailView" bundle:nil];
    [collectionView registerNib:albumSupplementaryDetailView
      forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind
                  withIdentifier:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewIdentifier];

    NSNib * const mediaItemSupplementaryDetailView = 
        [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewMediaItemSupplementaryDetailView" bundle:nil];
    [collectionView registerNib:mediaItemSupplementaryDetailView
      forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                  withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier];
}

- (void)setupTableViews
{
    self.collectionSelectionTableView.target = self;
    self.collectionSelectionTableView.doubleAction = @selector(collectionSelectionDoubleClickAction:);

    self.gridModeListTableView.target = self;
    self.gridModeListTableView.doubleAction = @selector(groupSelectionDoubleClickAction:);

    [self setupSongsTableView];
}

- (void)setupSongsTableView
{
    self.songsTableView.target = self;
    self.songsTableView.doubleAction = @selector(songDoubleClickAction:);

    [self setupPrototypeSortDescriptorsForTableView:_songsTableView];
    [self setupExistingSortForTableView:_songsTableView];
}

- (void)setupPrototypeSortDescriptorsForTableView:(NSTableView *)tableView
{
    for(NSTableColumn * const column in tableView.tableColumns) {
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
    VLCLibraryController * const libraryController = VLCMain.sharedInstance.libraryController;
    const vlc_ml_sorting_criteria_t existingSortCriteria = libraryController.lastSortingCriteria;

    NSString * const sortDescriptorKey = [self sortDescriptorKeyFromVlcMlSortingCriteria:existingSortCriteria];
    NSSortDescriptor * const sortDescriptor = [[NSSortDescriptor alloc] initWithKey:sortDescriptorKey
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

- (void)resetLayoutsForOperation:(void(^)(void))operation
{
    VLCLibraryCollectionViewFlowLayout * const collectionViewFlowLayout = (VLCLibraryCollectionViewFlowLayout *)self.collectionView.collectionViewLayout;
    if (collectionViewFlowLayout) {
        [collectionViewFlowLayout resetLayout];
    }

    operation();
    [self setupExistingSortForTableView:self.songsTableView];
}

- (void)reloadData
{
    _displayedCollectionUpdating = YES;

    dispatch_async(dispatch_get_main_queue(), ^{
        self.displayedCollection = [self collectionToDisplay];

        if (self.displayAllArtistsGenresTableEntry) {
            NSMutableArray * const mutableCollectionCopy = self.displayedCollection.mutableCopy;
            VLCLibraryAllAudioGroupsMediaLibraryItem *group;

            if (self->_currentParentType == VLCMediaLibraryParentGroupTypeGenre) {
                group = [[VLCLibraryAllAudioGroupsMediaLibraryItem alloc] initWithDisplayString:_NS("All genres")];
            } else if (self->_currentParentType == VLCMediaLibraryParentGroupTypeArtist) {
                group = [[VLCLibraryAllAudioGroupsMediaLibraryItem alloc] initWithDisplayString:_NS("All artists")];
            }

            NSAssert(group != nil, @"All items group should not be nil");
            [mutableCollectionCopy insertObject:group atIndex:0];
            self.displayedCollection = mutableCollectionCopy;
        }

        self->_displayedCollectionUpdating = NO;

        [self resetLayoutsForOperation:^{
            [self.collectionView reloadData];
            [self.gridModeListTableView reloadData];
            [self.collectionSelectionTableView reloadData];
            [self.songsTableView reloadData];
            [self.carouselView reloadData];
        }];

        [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryAudioDataSourceDisplayedCollectionChangedNotification object:self];
    });
}

- (NSUInteger)indexForMediaLibraryItemWithId:(const int64_t)itemId
{
    return [self.displayedCollection indexOfObjectPassingTest:^BOOL(const id<VLCMediaLibraryItemProtocol> item, const NSUInteger __unused idx, BOOL * const __unused stop) {
        NSAssert(item != nil, @"Cache list should not contain nil items");
        return item.libraryID == itemId;
    }];
}

- (void)reloadDataForMediaLibraryItem:(const id<VLCMediaLibraryItemProtocol>)item
{
    [self resetLayoutsForOperation:^{
        const NSUInteger index = [self indexForMediaLibraryItemWithId:item.libraryID];
        if (index == NSNotFound) {
            return;
        }

        NSMutableArray * const mutableCollectionCopy = [self.displayedCollection mutableCopy];
        [mutableCollectionCopy replaceObjectAtIndex:index withObject:item];
        self.displayedCollection = [mutableCollectionCopy copy];

        NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:index inSection:0];
        NSIndexSet * const rowIndexSet = [NSIndexSet indexSetWithIndex:index];

        const NSRange songsTableColumnRange = NSMakeRange(0, self->_songsTableView.numberOfColumns);
        NSIndexSet * const songsTableColumnIndexSet = [NSIndexSet indexSetWithIndexesInRange:songsTableColumnRange];

        [self.collectionView reloadItemsAtIndexPaths:[NSSet setWithObject:indexPath]];
        [self.songsTableView reloadDataForRowIndexes:rowIndexSet columnIndexes:songsTableColumnIndexSet];

        // Don't update gridModeListSelectionCollectionView, let its VLCLibraryAudioGroupDataSource do it.
        // Also don't update collectionSelectionTableView, as this will only show artists/genres/albums

        [self.carouselView reloadData];
    }];
}

- (void)deleteDataForMediaLibraryItem:(const id<VLCMediaLibraryItemProtocol>)item
{
    [self resetLayoutsForOperation:^{
        const NSUInteger index = [self indexForMediaLibraryItemWithId:item.libraryID];
        if (index == NSNotFound) {
            return;
        }

        NSMutableArray * const mutableCollectionCopy = [self.displayedCollection mutableCopy];
        [mutableCollectionCopy removeObjectAtIndex:index];
        self.displayedCollection = [mutableCollectionCopy copy];

        NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:index inSection:0];
        NSIndexSet * const rowIndexSet = [NSIndexSet indexSetWithIndex:index];

        [self.collectionView deleteItemsAtIndexPaths:[NSSet setWithObject:indexPath]];
        [self.songsTableView removeRowsAtIndexes:rowIndexSet withAnimation:NSTableViewAnimationSlideUp];

        // Comment in reloadDataForMediaLibraryItem will be informative

        [self.carouselView reloadData];
    }];
}

- (void)setAudioLibrarySegment:(VLCAudioLibrarySegment)audioLibrarySegment
{
    if (audioLibrarySegment == _audioLibrarySegment) {
        return;
    }

    _displayedCollectionUpdating = YES;

    _audioLibrarySegment = audioLibrarySegment;
    switch (self.audioLibrarySegment) {
        case VLCAudioLibraryArtistsSegment:
            _currentParentType = VLCMediaLibraryParentGroupTypeArtist;
            break;
        case VLCAudioLibraryAlbumsSegment:
            _currentParentType = VLCMediaLibraryParentGroupTypeAlbum;
            break;
        case VLCAudioLibrarySongsSegment:
            _currentParentType = VLCMediaLibraryParentGroupTypeAudioLibrary;
            break;
        case VLCAudioLibraryRecentsSegment:
            _currentParentType = VLCMediaLibraryParentGroupTypeRecentAudios;
            break;
        case VLCAudioLibraryGenresSegment:
            _currentParentType = VLCMediaLibraryParentGroupTypeGenre;
            break;
        default:
            NSAssert(1, @"reached the unreachable");
            break;
    }

    self.audioGroupDataSource.representedAudioGroup = nil; // Clear whatever was being shown before

    [self.songsTableView deselectAll:self];
    [self.collectionSelectionTableView deselectAll:self];
    [self.collectionView deselectAll:self];

    [self reloadData];
}

#pragma mark - table view data source and delegation

- (BOOL)displayAllArtistsGenresTableEntry
{
    return self.currentParentType == VLCMediaLibraryParentGroupTypeGenre ||
           self.currentParentType == VLCMediaLibraryParentGroupTypeArtist;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return self.displayedCollection.count;
}

- (NSInteger)rowForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    if (libraryItem == nil) {
        return NSNotFound;
    }

    return [self indexForMediaLibraryItemWithId:libraryItem.libraryID];
}

- (id<VLCMediaLibraryItemProtocol>)libraryItemAtRow:(NSInteger)row
                                       forTableView:(NSTableView *)tableView
{
    if (row < 0 || (NSUInteger)row >= self.displayedCollection.count) {
        return nil;
    }

    return self.displayedCollection[row];
}

- (void)applySelectionForTableView:(NSTableView *)tableView
{
    if (tableView == nil ||
        (tableView != self.collectionSelectionTableView && tableView != self.gridModeListTableView)) {
        return;
    }

    const NSInteger selectedRow = tableView.selectedRow;
    if (selectedRow >= 0 && (NSUInteger)selectedRow >= self.displayedCollection.count) {
        return;
    }

    const BOOL shouldClearSelection = self.currentParentType == VLCMediaLibraryParentGroupTypeAudioLibrary ||
                                      self.currentParentType == VLCMediaLibraryParentGroupTypeRecentAudios ||
                                      selectedRow < 0 ||
                                      self.displayedCollectionUpdating;

    const id<VLCMediaLibraryAudioGroupProtocol> selectedItem =
        shouldClearSelection ? nil : self.displayedCollection[selectedRow];
    self.audioGroupDataSource.representedAudioGroup = selectedItem;

    VLCLibraryRepresentedItem *representedItem = nil;
    NSString *fallbackTitle = nil;
    NSString *fallbackDetail = nil;

    if (self.displayAllArtistsGenresTableEntry && selectedRow == 0) {
        fallbackTitle = (self.currentParentType == VLCMediaLibraryParentGroupTypeGenre)
            ? _NS("All genres")
            : _NS("All artists");
        if (selectedItem != nil) {
            fallbackDetail = selectedItem.primaryDetailString;
        }
    } else if (selectedItem != nil) {
        representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:selectedItem parentType:self.currentParentType];
        fallbackTitle = selectedItem.displayString;
        fallbackDetail = selectedItem.primaryDetailString;
    }

    [self.headerDelegate audioDataSource:self
                updateHeaderForTableView:tableView
                     withRepresentedItem:representedItem
                           fallbackTitle:fallbackTitle
                          fallbackDetail:fallbackDetail];
}

- (void)tableView:(NSTableView * const)tableView selectRowIndices:(NSIndexSet * const)indices
{
    NSParameterAssert(tableView);

    if (tableView != self.collectionSelectionTableView && tableView != self.gridModeListTableView) {
        return;
    }

    const NSInteger selectedRow = indices.firstIndex;
    if (tableView.selectedRowIndexes != indices) {
        [tableView selectRowIndexes:indices byExtendingSelection:NO];
        [tableView scrollRowToVisible:selectedRow];
    }

    if (selectedRow >= 0 && (NSUInteger)selectedRow >= self.displayedCollection.count) {
        return;
    }

    [self applySelectionForTableView:tableView];
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

- (void)groupSelectionDoubleClickAction:(id)sender
{
    NSTableView * const tableView = (NSTableView *)sender;
    NSParameterAssert(tableView != nil);

    const NSInteger clickedRow = tableView.clickedRow;
    const id<VLCMediaLibraryItemProtocol> libraryItem = self.displayedCollection[clickedRow];
    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem parentType:self.currentParentType];

    [representedItem play];
}

- (void)collectionSelectionDoubleClickAction:(id)sender
{
    const id<VLCMediaLibraryItemProtocol> libraryItem = self.displayedCollection[self.collectionSelectionTableView.selectedRow];
    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem parentType:self.currentParentType];

    [representedItem play];
}

- (void)songDoubleClickAction:(id)sender
{
    NSAssert(_audioLibrarySegment == VLCAudioLibrarySongsSegment, @"Should not be possible to trigger this action from a non-song library view");
    VLCMediaLibraryMediaItem * const mediaItem = self.displayedCollection[_songsTableView.selectedRow];
    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:mediaItem parentType:self.currentParentType];

    [representedItem play];
}

#pragma mark - collection view data source

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    return self.displayedCollection.count;
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return 1;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem * const viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];
    const id<VLCMediaLibraryItemProtocol> actualItem = self.displayedCollection[indexPath.item];
    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:actualItem parentType:_currentParentType];

    viewItem.representedItem = representedItem;
    return viewItem;
}

- (NSView *)collectionView:(NSCollectionView *)collectionView
viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
               atIndexPath:(NSIndexPath *)indexPath
{
    if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind]) {

        VLCLibraryCollectionViewMediaItemListSupplementaryDetailView * const albumSupplementaryDetailView =
            [collectionView makeSupplementaryViewOfKind:kind 
                                         withIdentifier:VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind
                                           forIndexPath:indexPath];

        VLCMediaLibraryAlbum * const album = self.displayedCollection[indexPath.item];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:album parentType:_currentParentType];

        albumSupplementaryDetailView.representedItem = representedItem;
        albumSupplementaryDetailView.selectedItem = [collectionView itemAtIndex:indexPath.item];
        albumSupplementaryDetailView.parentScrollView = VLCMain.sharedInstance.libraryWindow.audioCollectionViewScrollView;
        albumSupplementaryDetailView.internalScrollView.scrollParentY = YES;

        VLCLibraryCollectionViewFlowLayout *flowLayout = (VLCLibraryCollectionViewFlowLayout*)collectionView.collectionViewLayout;
        if (flowLayout != nil) {
            albumSupplementaryDetailView.layoutScrollDirection = flowLayout.scrollDirection;
        }

        return albumSupplementaryDetailView;

    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {

        VLCLibraryCollectionViewMediaItemSupplementaryDetailView* mediaItemSupplementaryDetailView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind forIndexPath:indexPath];

        const id<VLCMediaLibraryItemProtocol> actualItem = self.displayedCollection[indexPath.item];
        VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:actualItem parentType:_currentParentType];
        mediaItemSupplementaryDetailView.representedItem = representedItem;
        mediaItemSupplementaryDetailView.selectedItem = [collectionView itemAtIndex:indexPath.item];

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

    if (indexPathItem < 0 || indexPathItem >= self.displayedCollection.count) {
        return nil;
    }

    return self.displayedCollection[indexPathItem];
}

- (NSIndexPath *)indexPathForLibraryItem:(id<VLCMediaLibraryItemProtocol>)libraryItem
{
    const NSInteger libraryItemRow = [self rowForLibraryItem:libraryItem];
    if (libraryItemRow == NSNotFound) {
        return nil;
    }

    return [NSIndexPath indexPathForItem:libraryItemRow inSection:0];
}

// pragma mark: iCarouselDataSource methods
- (NSInteger)numberOfItemsInCarousel:(iCarousel *)carousel
{
    return self.displayedCollection.count;
}

- (NSView *)carousel:(iCarousel *)carousel viewForItemAtIndex:(NSInteger)index reusingView:(NSView *)view
{
    VLCLibraryCarouselViewItemView *carouselItemView = (VLCLibraryCarouselViewItemView *)view;
    if (carouselItemView == nil) {
        const NSRect itemFrame = NSMakeRect(0,
                                            0,
                                            VLCLibraryUIUnits.carouselViewItemViewHeight,
                                            VLCLibraryUIUnits.carouselViewItemViewHeight);
        carouselItemView = [VLCLibraryCarouselViewItemView fromNibWithOwner:self];
        carouselItemView.frame = itemFrame;
    }

    const id<VLCMediaLibraryItemProtocol> libraryItem = [self libraryItemAtRow:index forTableView:nil];
    VLCLibraryRepresentedItem * const representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:libraryItem
                                                                                             parentType:self.currentParentType];
    carouselItemView.representedItem = representedItem;
    return carouselItemView;
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
    switch (self.audioLibrarySegment) {
        case VLCAudioLibraryArtistsSegment:
        case VLCAudioLibraryGenresSegment:
            return nil;
        case VLCAudioLibraryAlbumsSegment:
            return VLCLibraryCollectionViewMediaItemListSupplementaryDetailViewKind;
        case VLCAudioLibrarySongsSegment:
        default:
            return VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind;
    }
}

@end
