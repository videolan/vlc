/*****************************************************************************
 * VLCLibraryVideoDataSource.m: MacOS X interface module
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

#import "VLCLibraryVideoDataSource.h"

#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryTableCellView.h"

#import "main/CompatibilityFixes.h"
#import "extensions/NSString+Helpers.h"

@interface VLCLibraryVideoDataSource ()
{
    NSArray *_recentsArray;
    NSArray *_libraryArray;
    VLCLibraryCollectionViewFlowLayout *_collectionViewFlowLayout;
}

@end

@implementation VLCLibraryVideoDataSource

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
    }
    return self;
}

- (void)libraryModelUpdated:(NSNotification *)aNotification
{
    [self reloadData];
}

- (void)reloadData
{
    if(!_libraryModel) {
        return;
    }
    
    [_collectionViewFlowLayout resetLayout];
    
    dispatch_async(dispatch_get_main_queue(), ^{
        _recentsArray = [_libraryModel listOfRecentMedia];
        _libraryArray = [_libraryModel listOfVideoMedia];
        [_libraryMediaCollectionView reloadData];
        [_groupsTableView reloadData];
        [_groupSelectionTableView reloadData];
    });
}

- (void)setup
{
    [self setupCollectionView];
    [self setupTableViews];
}

- (void)setupCollectionView
{
    _libraryMediaCollectionView.dataSource = self;
    _libraryMediaCollectionView.delegate = self;
    
    [_libraryMediaCollectionView registerClass:[VLCLibraryCollectionViewItem class] forItemWithIdentifier:VLCLibraryCellIdentifier];
    [_libraryMediaCollectionView registerClass:[VLCLibraryCollectionViewSupplementaryElementView class]
                    forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                                withIdentifier:VLCLibrarySupplementaryElementViewIdentifier];
    
    NSNib *mediaItemSupplementaryDetailView = [[NSNib alloc] initWithNibNamed:@"VLCLibraryCollectionViewMediaItemSupplementaryDetailView" bundle:nil];
    [_libraryMediaCollectionView registerNib:mediaItemSupplementaryDetailView
                  forSupplementaryViewOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                              withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier];

    _collectionViewFlowLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];
    _collectionViewFlowLayout.headerReferenceSize = [VLCLibraryCollectionViewSupplementaryElementView defaultHeaderSize];
    _libraryMediaCollectionView.collectionViewLayout = _collectionViewFlowLayout;
}

- (void)setupTableViews
{
    _groupsTableView.dataSource = self;
    _groupsTableView.delegate = self;
    _groupsTableView.target = self;
    
    _groupSelectionTableView.dataSource = self;
    _groupSelectionTableView.delegate = self;
    _groupSelectionTableView.target = self;
}

#pragma mark - collection view data source and delegation

- (NSInteger)collectionView:(NSCollectionView *)collectionView
     numberOfItemsInSection:(NSInteger)section
{
    if (!_libraryModel) {
        return 0;
    }

    switch(section) {
        case VLCVideoLibraryRecentsSection:
            return [_libraryModel numberOfRecentMedia];
        case VLCVideoLibraryLibrarySection:
            return [_libraryModel numberOfVideoMedia];
        default:
            NSAssert(1, @"Reached unreachable case for video library section");
            break;
    }
}

- (NSInteger)numberOfSectionsInCollectionView:(NSCollectionView *)collectionView
{
    return 2;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    VLCLibraryCollectionViewItem *viewItem = [collectionView makeItemWithIdentifier:VLCLibraryCellIdentifier forIndexPath:indexPath];

    switch(indexPath.section) {
        case VLCVideoLibraryRecentsSection:
            viewItem.representedItem = _recentsArray[indexPath.item];
            break;
        case VLCVideoLibraryLibrarySection:
            viewItem.representedItem = _libraryArray[indexPath.item];
            break;
        default:
            NSAssert(1, @"Reached unreachable case for video library section");
            break;
    }

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
    if([kind isEqualToString:NSCollectionElementKindSectionHeader]) {
        VLCLibraryCollectionViewSupplementaryElementView *sectionHeadingView = [collectionView makeSupplementaryViewOfKind:kind
                                                                                                            withIdentifier:VLCLibrarySupplementaryElementViewIdentifier
                                                                                                              forIndexPath:indexPath];
        
        switch(indexPath.section) {
            case VLCVideoLibraryRecentsSection:
                sectionHeadingView.stringValue = _NS("Recent");
                break;
            case VLCVideoLibraryLibrarySection:
                sectionHeadingView.stringValue = _NS("Library");
                break;
            default:
                NSAssert(1, @"Reached unreachable case for video library section");
                break;
        }
                
        return sectionHeadingView;
        
    } else if ([kind isEqualToString:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind]) {
        VLCLibraryCollectionViewMediaItemSupplementaryDetailView* mediaItemSupplementaryDetailView = [collectionView makeSupplementaryViewOfKind:kind withIdentifier:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind forIndexPath:indexPath];
        
        switch(indexPath.section) {
            case VLCVideoLibraryRecentsSection:
                mediaItemSupplementaryDetailView.representedMediaItem = _recentsArray[indexPath.item];
                break;
            case VLCVideoLibraryLibrarySection:
                mediaItemSupplementaryDetailView.representedMediaItem = _libraryArray[indexPath.item];
                break;
            default:
                NSAssert(1, @"Reached unreachable case for video library section");
                break;
        }
        
        mediaItemSupplementaryDetailView.selectedItem = [collectionView itemAtIndexPath:indexPath];
        return mediaItemSupplementaryDetailView;
    }

    return nil;
}

#pragma mark - drag and drop support

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
        VLCMediaLibraryMediaItem *mediaItem = _libraryArray[indexPath.item];
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

#pragma mark - table view data source and delegation

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    if (tableView == _groupsTableView) {
        return 2;
    } else if (tableView == _groupSelectionTableView && _groupsTableView.selectedRow > -1) {
        switch(_groupsTableView.selectedRow) {
            case VLCVideoLibraryRecentsSection:
                return _recentsArray.count;
            case VLCVideoLibraryLibrarySection:
                return _libraryArray.count;
            default:
                NSAssert(1, @"Reached unreachable case for video library section");
                break;
        }
    }
    
    return 0;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibraryTableCellView *cellView = [tableView makeViewWithIdentifier:@"VLCVideoLibraryTableViewCellIdentifier" owner:self];
    
    if (!cellView) {
        cellView = [VLCLibraryTableCellView fromNibWithOwner:self];
        cellView.identifier = @"VLCVideoLibraryTableViewCellIdentifier";
    }
    
    if (tableView == _groupsTableView) {
        cellView.representedVideoLibrarySection = row;
    } else if (tableView == _groupSelectionTableView && _groupsTableView.selectedRow > -1) {
        switch(_groupsTableView.selectedRow) {
            case VLCVideoLibraryRecentsSection:
                cellView.representedItem = _recentsArray[row];
                break;
            case VLCVideoLibraryLibrarySection:
                cellView.representedItem = _libraryArray[row];
                break;
            default:
                NSAssert(1, @"Reached unreachable case for video library section");
                break;
        }
    }
    
    return cellView;
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    if(notification.object == _groupsTableView) {
        [_groupSelectionTableView reloadData];
    }
}

@end
