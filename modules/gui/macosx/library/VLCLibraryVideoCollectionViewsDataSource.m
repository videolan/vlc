/*****************************************************************************
 * VLCLibraryVideoDataSource.h: MacOS X interface module
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

#import "VLCLibraryVideoCollectionViewsDataSource.h"

#import "extensions/NSString+Helpers.h"

#import "library/VLCLibraryController.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryCollectionViewSupplementaryElementView.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryTableCellView.h"
#import "library/VLCLibraryVideoDataSource.h"

#import "main/VLCMain.h"

@interface VLCLibraryVideoCollectionViewTableViewCellDataSource : NSObject <NSCollectionViewDataSource, NSCollectionViewDelegate>

@property (readwrite, assign) NSCollectionView *collectionView;
@property (readwrite, assign) VLCLibraryModel *libraryModel;

- (void)setup;
- (void)reloadData;

@end

@interface VLCLibraryVideoCollectionViewTableViewCellDataSource ()
{
    NSArray *_collectionArray;
}
@end

@implementation VLCLibraryVideoCollectionViewTableViewCellDataSource

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
    if(!_libraryModel || !_collectionView) {
        NSLog(@"Null library model or collection view");
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
        // TODO: do per collection specifically
        self->_collectionArray = [self->_libraryModel listOfVideoMedia];
        [self->_collectionView reloadData];
    });
}

- (void)setup
{
    _collectionView.dataSource = self;
    _collectionView.delegate = self;

    NSCollectionViewFlowLayout *collectionViewLayout = [[NSCollectionViewFlowLayout alloc] init];
    collectionViewLayout.itemSize = CGSizeMake(214., 246.);
    _collectionView.collectionViewLayout = collectionViewLayout;

    [_collectionView registerClass:[VLCLibraryCollectionViewItem class] forItemWithIdentifier:VLCLibraryCellIdentifier];
    [self reloadData];
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

@end

@interface VLCLibraryVideoCollectionViewTableViewCell : NSTableCellView

@property (readonly) NSCollectionView *collectionView;
@property (readonly) NSScrollView *scrollView;
@property (readonly) VLCLibraryVideoCollectionViewTableViewCellDataSource *dataSource;

@end

@implementation VLCLibraryVideoCollectionViewTableViewCell

- (instancetype)init
{
    self = [super init];

    if(self) {
        [self setupCollectionView];
        [self setupDataSource];
    }

    return self;
}

- (void)setupCollectionView
{
    _collectionView = [[NSCollectionView alloc] initWithFrame:NSZeroRect];
    _scrollView = [[NSScrollView alloc] init];
    _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrollView.documentView = _collectionView;

    [self addSubview:_scrollView];
    [self addConstraints:@[
        [NSLayoutConstraint constraintWithItem:_scrollView
                                     attribute:NSLayoutAttributeTop
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeTop
                                    multiplier:1
                                      constant:0
        ],
        [NSLayoutConstraint constraintWithItem:_scrollView
                                     attribute:NSLayoutAttributeBottom
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeBottom
                                    multiplier:1
                                      constant:0
        ],
        [NSLayoutConstraint constraintWithItem:_scrollView
                                     attribute:NSLayoutAttributeLeft
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeLeft
                                    multiplier:1
                                      constant:0
        ],
        [NSLayoutConstraint constraintWithItem:_scrollView
                                     attribute:NSLayoutAttributeRight
                                     relatedBy:NSLayoutRelationEqual
                                        toItem:self
                                     attribute:NSLayoutAttributeRight
                                    multiplier:1
                                      constant:0
        ],
    ]];
}

- (void)setupDataSource
{
    _dataSource = [[VLCLibraryVideoCollectionViewTableViewCellDataSource alloc] init];
    _dataSource.collectionView = _collectionView;
    [_dataSource setup];
}

@end

@implementation VLCLibraryVideoCollectionViewsDataSource

- (void)setCollectionsTableView:(NSTableView *)collectionsTableView
{
    _collectionsTableView = collectionsTableView;
    _collectionsTableView.dataSource = self;
    _collectionsTableView.delegate = self;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return 1;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibraryVideoCollectionViewTableViewCell *cell = [[VLCLibraryVideoCollectionViewTableViewCell alloc] init];
    cell.identifier = @"VLCLibraryVideoCollectionViewTableViewCellIdentifier";
    [cell.collectionView reloadData];
    return cell;
}


@end
