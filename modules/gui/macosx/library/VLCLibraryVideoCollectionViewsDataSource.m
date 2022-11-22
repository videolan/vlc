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

#import "views/VLCSubScrollView.h"

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
            default:
                NSAssert(1, @"Cannot construct group descriptor from invalid VLCLibraryVideoGroup value");
                _group = VLCLibraryVideoInvalidGroup;
                break;
        }

        _libraryModelDataMethodSignature = [VLCLibraryModel instanceMethodSignatureForSelector:_libraryModelDataSelector];
    }

    return self;
}

@end

@interface VLCLibraryVideoCollectionViewTableViewCellDataSource ()
{
    NSArray *_collectionArray;
    VLCLibraryCollectionViewFlowLayout *_collectionViewFlowLayout;
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

@end

@interface VLCLibraryVideoCollectionViewTableViewCell : NSTableCellView

@property (readonly) NSCollectionView *collectionView;
@property (readonly) VLCSubScrollView *scrollView;
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
        [self setupView];
        [self setupDataSource];
    }

    return self;
}

- (void)setupView
{
    [self setupCollectionView];
    [self setupScrollView];

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

- (void)setupCollectionView
{
    VLCLibraryCollectionViewFlowLayout *collectionViewLayout = [[VLCLibraryCollectionViewFlowLayout alloc] init];
    collectionViewLayout.headerReferenceSize = [VLCLibraryCollectionViewSupplementaryElementView defaultHeaderSize];
    collectionViewLayout.itemSize = CGSizeMake(214., 260.);

    _collectionView = [[NSCollectionView alloc] initWithFrame:NSZeroRect];
    _collectionView.postsFrameChangedNotifications = YES;
    _collectionView.collectionViewLayout = collectionViewLayout;
    _collectionView.selectable = YES;
    _collectionView.allowsEmptySelection = YES;
    _collectionView.allowsMultipleSelection = NO;
}

- (void)setupScrollView
{
    _scrollView = [[VLCSubScrollView alloc] init];
    _scrollView.scrollParentY = YES;
    _scrollView.forceHideVerticalScroller = YES;

    _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrollView.documentView = _collectionView;
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
    if (_groupDescriptor.group == group) {
        return;
    }

    VLCLibraryVideoCollectionViewGroupDescriptor *descriptor = [[VLCLibraryVideoCollectionViewGroupDescriptor alloc] initWithVLCVideoLibraryGroup:group];
    [self setGroupDescriptor:descriptor];
}

@end

@interface VLCLibraryVideoCollectionViewsDataSource()
{
    NSArray *_collectionViewCells;
}
@end

@implementation VLCLibraryVideoCollectionViewsDataSource

- (instancetype)init
{
    self = [super init];
    if (self) {
        [self setup];
    }
    return self;
}

- (void)setup
{
    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(cellRowHeightChanged:)
                               name:NSViewFrameDidChangeNotification
                             object:nil];
    [self generateCollectionViewCells];
}

- (void)generateCollectionViewCells
{
    NSMutableArray *collectionViewCells = [[NSMutableArray alloc] init];
    NSUInteger firstRow = VLCLibraryVideoRecentsGroup;
    NSUInteger lastRow = VLCLibraryVideoLibraryGroup;

    for (NSUInteger i = firstRow; i <= lastRow; ++i) {
        VLCLibraryVideoCollectionViewTableViewCell *cellView = [[VLCLibraryVideoCollectionViewTableViewCell alloc] init];
        cellView.identifier = @"VLCLibraryVideoCollectionViewTableViewCellIdentifier";
        cellView.scrollView.parentScrollView = _collectionsTableViewScrollView;

        [collectionViewCells addObject:cellView];
    }

    _collectionViewCells = collectionViewCells;
}

- (void)reloadData
{
    dispatch_async(dispatch_get_main_queue(), ^{
        [self generateCollectionViewCells];

        if (self->_collectionsTableView) {
            [self->_collectionsTableView reloadData];

            // HACK: On app init the vertical collection views will not get their heights updated properly.
            // So let's schedule a check when the table view is set to correct this issue...
            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
                NSMutableIndexSet *indexSet = [[NSMutableIndexSet alloc] init];

                for (NSUInteger i = 0; i < self->_collectionViewCells.count; ++i) {
                    [indexSet addIndex:i];
                }

                [self->_collectionsTableView noteHeightOfRowsWithIndexesChanged:indexSet];
            });
        }
    });
}

- (void)cellRowHeightChanged:(NSNotification *)notification
{
    if (!_collectionsTableView || [notification.object class] != [VLCLibraryVideoCollectionViewTableViewCell class]) {
        return;
    }

    VLCLibraryVideoCollectionViewTableViewCell *cellView = (VLCLibraryVideoCollectionViewTableViewCell *)notification.object;
    if (!cellView) {
        return;
    }

    NSUInteger cellIndex = [_collectionViewCells indexOfObjectPassingTest:^BOOL(VLCLibraryVideoCollectionViewTableViewCell* existingCell, NSUInteger idx, BOOL *stop) {
        return existingCell.groupDescriptor.group == cellView.groupDescriptor.group;
    }];
    if (cellIndex == NSNotFound) {
        // Let's try a backup
        cellIndex = cellView.groupDescriptor.group - 1;
    }

    [self scheduleRowHeightNoteChangeForRow:cellView.groupDescriptor.group - 1];
}

- (void)scheduleRowHeightNoteChangeForRow:(NSUInteger)row
{
    NSAssert(row >= 0 && row < _collectionViewCells.count, @"Invalid row index passed to scheduleRowHeightNoteChangeForRow");
    dispatch_async(dispatch_get_main_queue(), ^{
        NSIndexSet *indexSetToNote = [NSIndexSet indexSetWithIndex:row];
        [self->_collectionsTableView noteHeightOfRowsWithIndexesChanged:indexSetToNote];
    });
}

- (void)setCollectionsTableView:(NSTableView *)collectionsTableView
{
    _collectionsTableView = collectionsTableView;
    _collectionsTableView.dataSource = self;
    _collectionsTableView.delegate = self;

    _collectionsTableView.intercellSpacing = NSMakeSize(20., 20.);
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    return _collectionViewCells.count;
}

- (NSView *)tableView:(NSTableView *)tableView
   viewForTableColumn:(NSTableColumn *)tableColumn
                  row:(NSInteger)row
{
    VLCLibraryVideoCollectionViewTableViewCell* cellView = _collectionViewCells[row];
    cellView.videoGroup = row + 1;
    return cellView;
}

- (CGFloat)tableView:(NSTableView *)tableView
         heightOfRow:(NSInteger)row
{
    CGFloat fallback = _collectionViewItemSize.height +
                       _collectionViewSectionInset.top +
                       _collectionViewSectionInset.bottom;
    
    VLCLibraryVideoCollectionViewTableViewCell* cellView = _collectionViewCells[row];
    if (cellView && cellView.collectionView.collectionViewLayout.collectionViewContentSize.height > fallback) {
        return cellView.collectionView.collectionViewLayout.collectionViewContentSize.height;
    }

    if (fallback <= 0) {
        NSLog(@"Unable to provide reasonable fallback or accurate rowheight -- providing rough rowheight");
        return 300;
    }

    return fallback;
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)rowIndex
{
    return NO;
}

@end
