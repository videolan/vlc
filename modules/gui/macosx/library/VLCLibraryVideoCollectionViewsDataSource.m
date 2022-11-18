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

@interface VLCLibraryVideoCollectionViewGroupDescriptor : NSObject

@property (readonly) VLCLibraryVideoGroup group;
@property (readonly) SEL libraryModelDataSelector;
@property (readonly) NSMethodSignature *libraryModelDataMethodSignature;
@property (readonly) NSNotificationName libraryModelUpdatedNotificationName;
@property (readonly) NSString *name;
@property (readonly) BOOL isHorizontalBarCollectionView;

- (instancetype)initWithVLCVideoLibraryGroup:(VLCLibraryVideoGroup)group;

@end

@implementation VLCLibraryVideoCollectionViewGroupDescriptor

- (instancetype)initWithVLCVideoLibraryGroup:(VLCLibraryVideoGroup)group
{
    self = [super init];

    if (self) {
        _group = group;

        switch (_group) {
            case VLCLibraryVideoRecentsGroup:
                _libraryModelUpdatedNotificationName = VLCLibraryModelRecentMediaListUpdated;
                _libraryModelDataSelector = @selector(listOfRecentMedia);
                _isHorizontalBarCollectionView = YES;
                _name = _NS("Recents");
                break;
            case VLCLibraryVideoLibraryGroup:
                _libraryModelUpdatedNotificationName = VLCLibraryModelVideoMediaListUpdated;
                _libraryModelDataSelector = @selector(listOfVideoMedia);
                _isHorizontalBarCollectionView = NO;
                _name = _NS("Library");
                break;
        }

        _libraryModelDataMethodSignature = [VLCLibraryModel instanceMethodSignatureForSelector:_libraryModelDataSelector];
    }

    return self;
}

@end

@interface VLCLibraryVideoCollectionViewTableViewCellDataSource : NSObject <NSCollectionViewDataSource, NSCollectionViewDelegate>

@property (readwrite, assign) NSCollectionView *collectionView;
@property (readwrite, assign) VLCLibraryModel *libraryModel;
@property (readwrite, assign, nonatomic) VLCLibraryVideoCollectionViewGroupDescriptor *groupDescriptor;

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
    if(!_libraryModel || !_collectionView || !_groupDescriptor) {
        NSLog(@"Null library model or collection view or video group descriptor");
        return;
    }

    dispatch_async(dispatch_get_main_queue(), ^{
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
    _collectionView.dataSource = self;
    _collectionView.delegate = self;

    [_collectionView registerClass:[VLCLibraryCollectionViewItem class]
             forItemWithIdentifier:VLCLibraryCellIdentifier];
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
@property (readonly) VLCLibraryVideoCollectionViewGroupDescriptor *groupDescriptor;
@property (readwrite, assign, nonatomic) VLCLibraryVideoGroup videoGroup;

- (void)setVideoGroup:(VLCLibraryVideoGroup)group;

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
    NSCollectionViewFlowLayout *collectionViewLayout = [[NSCollectionViewFlowLayout alloc] init];
    collectionViewLayout.itemSize = CGSizeMake(214., 246.);

    _collectionView = [[NSCollectionView alloc] initWithFrame:NSZeroRect];
    _collectionView.collectionViewLayout = collectionViewLayout;

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

- (void)setGroupDescriptor:(VLCLibraryVideoCollectionViewGroupDescriptor *)groupDescriptor
{
    _groupDescriptor = groupDescriptor;
    _dataSource.groupDescriptor = groupDescriptor;

    NSCollectionViewFlowLayout *collectionViewLayout = _collectionView.collectionViewLayout;
    collectionViewLayout.scrollDirection = _groupDescriptor.isHorizontalBarCollectionView ?
                                           NSCollectionViewScrollDirectionHorizontal :
                                           NSCollectionViewScrollDirectionVertical;
}

- (void)setVideoGroup:(VLCLibraryVideoGroup)group
{
    VLCLibraryVideoCollectionViewGroupDescriptor *descriptor = [[VLCLibraryVideoCollectionViewGroupDescriptor alloc] initWithVLCVideoLibraryGroup:group];
    [self setGroupDescriptor:descriptor];
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
    NSInteger lastGroupEnumElement = VLCLibraryVideoLibraryGroup;
    return lastGroupEnumElement;
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCLibraryVideoCollectionViewTableViewCell *cell = [[VLCLibraryVideoCollectionViewTableViewCell alloc] init];
    cell.identifier = @"VLCLibraryVideoCollectionViewTableViewCellIdentifier";
    cell.videoGroup = row + 1; // The VLCVideoLibraryVideoGroup enum starts at 1, and represents our row count

    return cell;
}


@end
