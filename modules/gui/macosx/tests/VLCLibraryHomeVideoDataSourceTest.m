/*****************************************************************************
 * VLCLibraryHomeVideoDataSourceTest.m: Home video source filtering tests
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import "VLCAppKitMockingTestCase.h"
#import "library/VLCLibraryModel.h"

#import "library/VLCLibraryCollectionViewItem.h"
#import "library/VLCLibraryCollectionViewMediaItemSupplementaryDetailView.h"
#import "library/VLCLibraryMediaGroupDescriptor.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/home-library/VLCLibraryHomeViewMediaContainerViewDataSource.h"
#import "main/VLCMain.h"

// Model snapshots are controlled values; no backing media library is needed.
@interface VLCHomeFeaturedVideoTestMediaItem : NSObject
@property int64_t libraryID;
@property float progress;
@property vlc_ml_media_type_t mediaType;
@property (copy) NSString *displayString;
@property (copy) NSString *primaryDetailString;
@end

@implementation VLCHomeFeaturedVideoTestMediaItem
@end

static VLCMediaLibraryMediaItem *VLCHomeFeaturedVideoMediaItem(int64_t libraryID, NSString *title)
{
    VLCHomeFeaturedVideoTestMediaItem * const item = [[VLCHomeFeaturedVideoTestMediaItem alloc] init];
    item.libraryID = libraryID;
    item.mediaType = VLC_ML_MEDIA_TYPE_VIDEO;
    item.displayString = title;
    item.primaryDetailString = @"Video details";
    return (id)item;
}

@interface VLCHomeFeaturedVideoTestCard : NSCollectionViewItem<VLCLibraryRepresentedItemCollectionViewItem>
@property (strong, nonatomic) VLCLibraryRepresentedItem *representedItem;
@end

@implementation VLCHomeFeaturedVideoTestCard
@end

@interface VLCHomeFeaturedVideoTestDetail : NSView
@property (strong) VLCLibraryRepresentedItem *representedItem;
@property (weak) NSCollectionViewItem *selectedItem;
@property NSCollectionViewScrollDirection layoutScrollDirection;
@end

@implementation VLCHomeFeaturedVideoTestDetail
@end

@interface VLCHomeFeaturedVideoTestCollection : NSCollectionView
@property NSUInteger reloadCount;
@property (copy) NSSet<NSIndexPath *> *reloadedIndexPaths;
@property (copy) NSSet<NSIndexPath *> *deletedIndexPaths;
@end

@implementation VLCHomeFeaturedVideoTestCollection

- (void)reloadData
{
    ++self.reloadCount;
}

- (void)reloadItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    self.reloadedIndexPaths = indexPaths;
}

- (void)deleteItemsAtIndexPaths:(NSSet<NSIndexPath *> *)indexPaths
{
    self.deletedIndexPaths = indexPaths;
}

- (NSCollectionViewItem *)makeItemWithIdentifier:(NSUserInterfaceItemIdentifier)identifier
                                    forIndexPath:(NSIndexPath *)indexPath
{
    return [[VLCHomeFeaturedVideoTestCard alloc] init];
}

- (NSView *)makeSupplementaryViewOfKind:(NSCollectionViewSupplementaryElementKind)kind
                         withIdentifier:(NSUserInterfaceItemIdentifier)identifier
                           forIndexPath:(NSIndexPath *)indexPath
{
    return [[VLCHomeFeaturedVideoTestDetail alloc] init];
}

- (NSCollectionViewItem *)itemAtIndexPath:(NSIndexPath *)indexPath
{
    return nil;
}

@end

@interface VLCHomeFeaturedVideoTestModel : NSObject
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *recentItems;
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *videoItems;
@property (strong) dispatch_semaphore_t readGate;
@property (copy) dispatch_block_t readCaptured;
@end

@implementation VLCHomeFeaturedVideoTestModel

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentMedia
{
    NSArray *items;
    dispatch_semaphore_t gate;
    dispatch_block_t captured;
    @synchronized (self) {
        items = self.recentItems.copy;
        gate = self.readGate;
        captured = self.readCaptured;
        self.readGate = nil;
        self.readCaptured = nil;
    }
    if (gate != nil) {
        dispatch_async(dispatch_get_main_queue(), captured);
        dispatch_semaphore_wait(gate, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
    }
    return items;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfVideoMedia
{
    return self.videoItems;
}

@end

@interface VLCLibraryHomeVideoDataSourceTest : VLCAppKitMockingTestCase
{
    VLCHomeFeaturedVideoTestModel *_model;
    VLCHomeFeaturedVideoTestCollection *_collection;
    VLCLibraryHomeViewMediaContainerViewDataSource *_dataSource;
    VLCLibraryMediaCollectionViewGroupDescriptor *_groupDescriptor;
}
@end

@implementation VLCLibraryHomeVideoDataSourceTest

- (void)setUp
{
    [super setUp];
    _model = [[VLCHomeFeaturedVideoTestModel alloc] init];
    _model.recentItems = @[
        VLCHomeFeaturedVideoMediaItem(10, @"First"),
        VLCHomeFeaturedVideoMediaItem(20, @"Second"),
        VLCHomeFeaturedVideoMediaItem(30, @"Third"),
    ];
    _model.videoItems = _model.recentItems;
    __weak typeof(self) weakSelf = self;
    [self replaceClassMethod:@selector(sharedInstance)
                     onClass:VLCMain.class
                   withBlock:^id(id __unused receiver) {
        return weakSelf;
    }];
    _collection = [[VLCHomeFeaturedVideoTestCollection alloc] init];
    _dataSource = [[VLCLibraryHomeViewMediaContainerViewDataSource alloc] init];
    _dataSource.collectionView = _collection;
    _collection.dataSource = _dataSource;
    [self addTeardownBlock:^{
        VLCLibraryHomeVideoDataSourceTest * const test = weakSelf;
        [test->_dataSource disconnect];
        test->_collection.dataSource = nil;
        test->_dataSource = nil;
        [test drainMainQueue];
    }];
    [self loadGroup:VLCMediaLibraryParentGroupTypeRecentVideos];
}

- (id)libraryController
{
    return self;
}

- (id)libraryModel
{
    return _model;
}

- (XCTestExpectation *)expectDisplayedChange
{
    return [self expectationForNotification:VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification
                                     object:_dataSource
                                    handler:nil];
}

- (void)loadGroup:(VLCMediaLibraryParentGroupType)group
{
    XCTestExpectation * const changed = [self expectDisplayedChange];
    _groupDescriptor = [[VLCLibraryMediaCollectionViewGroupDescriptor alloc] initWithLibraryGroup:group];
    _dataSource.groupDescriptor = _groupDescriptor;
    [self waitForExpectations:@[changed] timeout:5.];
}

- (void)reloadSource
{
    XCTestExpectation * const changed = [self expectDisplayedChange];
    [_dataSource reloadData];
    [self waitForExpectations:@[changed] timeout:5.];
}

- (NSArray<NSNumber *> *)displayedIDs
{
    NSMutableArray<NSNumber *> * const identifiers = NSMutableArray.array;
    const NSUInteger count = [_dataSource collectionView:_collection numberOfItemsInSection:0];
    for (NSUInteger index = 0; index < count; ++index) {
        id<VLCMediaLibraryItemProtocol> const item = [_dataSource
            libraryItemAtIndexPath:[NSIndexPath indexPathForItem:index inSection:0]
                 forCollectionView:_collection];
        [identifiers addObject:@(item.libraryID)];
    }
    return identifiers;
}

- (void)testExcludingHeroPreservesFullSourceAndRestoresItemsOnHeroChanges
{
    XCTAssertEqualObjects(self.displayedIDs, (@[@10, @20, @30]));
    XCTAssertEqual(_dataSource.numberOfMediaItems, (NSUInteger)3);
    XCTAssertEqual(_dataSource.firstMediaItem.libraryID, (int64_t)10);
    _dataSource.excludedLibraryID = 10;
    XCTAssertEqualObjects(self.displayedIDs, (@[@20, @30]));
    XCTAssertEqual(_dataSource.numberOfMediaItems, (NSUInteger)3);
    XCTAssertEqual(_dataSource.firstMediaItem.libraryID, (int64_t)10);

    _dataSource.excludedLibraryID = 20;
    XCTAssertEqualObjects(self.displayedIDs, (@[@10, @30]));
    _dataSource.excludedLibraryID = 0;
    XCTAssertEqualObjects(self.displayedIDs, (@[@10, @20, @30]));
    _dataSource.excludedLibraryID = 999;
    XCTAssertEqualObjects(self.displayedIDs, (@[@10, @20, @30]));
    const NSUInteger reloadCount = _collection.reloadCount;
    _dataSource.excludedLibraryID = 999;
    XCTAssertEqual(_collection.reloadCount, reloadCount);
}

- (void)testFilteredIndicesDriveCardsDetailsAndRepresentedItemActions
{
    _dataSource.excludedLibraryID = 10;
    NSIndexPath * const firstPath = [NSIndexPath indexPathForItem:0 inSection:0];
    NSIndexPath * const secondPath = [NSIndexPath indexPathForItem:1 inSection:0];
    XCTAssertNil([_dataSource indexPathForLibraryItem:_model.recentItems[0]]);
    XCTAssertEqualObjects([_dataSource indexPathForLibraryItem:_model.recentItems[1]], firstPath);
    XCTAssertEqualObjects([_dataSource indexPathForLibraryItem:_model.recentItems[2]], secondPath);
    VLCHomeFeaturedVideoTestCard * const card = (id)[_dataSource collectionView:_collection
                                            itemForRepresentedObjectAtIndexPath:firstPath];
    XCTAssertEqual(card.representedItem.item.libraryID, (int64_t)20);
    NSArray<VLCLibraryRepresentedItem *> * const actions = [_dataSource
        representedItemsAtIndexPaths:[NSSet setWithObject:secondPath]
                   forCollectionView:_collection];
    XCTAssertEqual(actions.firstObject.item.libraryID, (int64_t)30);
    VLCHomeFeaturedVideoTestDetail * const detail = (id)[_dataSource collectionView:_collection
                                                  viewForSupplementaryElementOfKind:VLCLibraryCollectionViewMediaItemSupplementaryDetailViewKind
                                                                        atIndexPath:firstPath];
    XCTAssertEqual(detail.representedItem.item.libraryID, (int64_t)20);
    XCTAssertNil([_dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]
                                   forCollectionView:_collection]);
    XCTAssertNil([_dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:2 inSection:0]
                                   forCollectionView:_collection]);
}

- (void)testOnlyRecentVideoRemainsInFullSourceWhenItsCardIsExcluded
{
    _model.recentItems = @[_model.recentItems.firstObject];
    [self reloadSource];
    _dataSource.excludedLibraryID = 10;
    XCTAssertEqualObjects(self.displayedIDs, @[]);
    XCTAssertEqual(_dataSource.numberOfMediaItems, (NSUInteger)1);
    XCTAssertEqual(_dataSource.firstMediaItem.libraryID, (int64_t)10);
    _dataSource.excludedLibraryID = 0;
    XCTAssertEqualObjects(self.displayedIDs, (@[@10]));
}

- (void)testUpdatesAndDeletionsUseDisplayIndicesAndKeepExcludedSourceCurrent
{
    _dataSource.excludedLibraryID = 10;
    VLCMediaLibraryMediaItem * const updated = VLCHomeFeaturedVideoMediaItem(20, @"Updated");
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentsMediaItemUpdated object:updated];
    XCTAssertEqualObjects(_collection.reloadedIndexPaths, [NSSet setWithObject:[NSIndexPath indexPathForItem:0 inSection:0]]);
    XCTAssertEqual([_dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]
                                     forCollectionView:_collection], updated);
    VLCMediaLibraryMediaItem * const updatedHero = VLCHomeFeaturedVideoMediaItem(10, @"Updated Hero");
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentsMediaItemUpdated object:updatedHero];
    XCTAssertEqual(_dataSource.firstMediaItem, updatedHero);
    _dataSource.excludedLibraryID = 20;
    XCTAssertEqual([_dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]
                                     forCollectionView:_collection], updatedHero);
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentsMediaItemDeleted object:updatedHero];
    XCTAssertEqual(_dataSource.numberOfMediaItems, (NSUInteger)2);
    XCTAssertEqualObjects(_collection.deletedIndexPaths, [NSSet setWithObject:[NSIndexPath indexPathForItem:0 inSection:0]]);
    XCTAssertEqualObjects(self.displayedIDs, (@[@30]));
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentsMediaItemDeleted object:updated];
    XCTAssertEqual(_dataSource.numberOfMediaItems, (NSUInteger)1);
    XCTAssertEqual(_dataSource.firstMediaItem.libraryID, (int64_t)30);
    _dataSource.excludedLibraryID = 0;
    XCTAssertEqualObjects(self.displayedIDs, (@[@30]));
}

- (void)testDefaultVideoGridDoesNotExcludeAnyItems
{
    [self loadGroup:VLCMediaLibraryParentGroupTypeVideoLibrary];
    XCTAssertEqual(_dataSource.excludedLibraryID, (int64_t)0);
    XCTAssertEqualObjects(self.displayedIDs, (@[@10, @20, @30]));
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelVideoMediaItemDeleted object:_model.videoItems[1]];
    XCTAssertEqualObjects(self.displayedIDs, (@[@10, @30]));
    XCTAssertEqual(_dataSource.numberOfMediaItems, (NSUInteger)2);
}

- (void)testSupersededReloadCannotRestoreAnOldSourceOrExcludedHero
{
    dispatch_semaphore_t const gate = dispatch_semaphore_create(0);
    XCTestExpectation * const captured = [self expectationWithDescription:@"older recent snapshot captured"];
    _model.readGate = gate;
    _model.readCaptured = ^{
        [captured fulfill];
    };
    [_dataSource reloadData];
    [self waitForExpectations:@[captured] timeout:5.];
    _dataSource.excludedLibraryID = 20;
    _model.recentItems = @[_model.recentItems[1], _model.recentItems[2]];
    [self reloadSource];
    XCTAssertEqual(_dataSource.numberOfMediaItems, (NSUInteger)2);
    XCTAssertEqual(_dataSource.firstMediaItem.libraryID, (int64_t)20);
    XCTAssertEqualObjects(self.displayedIDs, (@[@30]));

    XCTestExpectation * const stale = [self expectDisplayedChange];
    stale.inverted = YES;
    dispatch_semaphore_signal(gate);
    [self waitForExpectations:@[stale] timeout:.2];
    XCTAssertEqual(_dataSource.numberOfMediaItems, (NSUInteger)2);
    XCTAssertEqualObjects(self.displayedIDs, (@[@30]));
}

- (void)testPendingReloadPreservesNewItemsAndConcurrentUpdatesAndDeletions
{
    VLCMediaLibraryMediaItem * const newItem = VLCHomeFeaturedVideoMediaItem(40, @"New first");
    _model.recentItems = [@[newItem] arrayByAddingObjectsFromArray:_model.recentItems];
    dispatch_semaphore_t const gate = dispatch_semaphore_create(0);
    XCTestExpectation * const captured = [self expectationWithDescription:@"pending recent snapshot captured"];
    _model.readGate = gate;
    _model.readCaptured = ^{
        [captured fulfill];
    };
    [_dataSource reloadData];
    [self waitForExpectations:@[captured] timeout:5.];

    VLCMediaLibraryMediaItem * const updated = VLCHomeFeaturedVideoMediaItem(20, @"Updated during reload");
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentsMediaItemUpdated object:updated];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentsMediaItemDeleted object:_model.recentItems[1]];
    _dataSource.excludedLibraryID = 40;
    XCTestExpectation * const changed = [self expectDisplayedChange];
    dispatch_semaphore_signal(gate);
    [self waitForExpectations:@[changed] timeout:5.];

    XCTAssertEqual(_dataSource.numberOfMediaItems, (NSUInteger)3);
    XCTAssertEqual(_dataSource.firstMediaItem, newItem);
    XCTAssertEqualObjects(self.displayedIDs, (@[@20, @30]));
    XCTAssertEqual([_dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]
                                     forCollectionView:_collection], updated);
}

@end
