/*****************************************************************************
 * VLCLibraryHomeVideoDataSourceTest.m: Recently Added source tests
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
#import "library/VLCLibraryRepresentedItem.h"
#import "library/home-library/VLCLibraryHomeViewVideoContainerViewDataSource.h"
#import "library/video-library/VLCLibraryVideoGroupDescriptor.h"
#import "main/VLCMain.h"

@interface VLCHomeDataSourceTestMediaItem : NSObject
@property int64_t libraryID;
@property vlc_ml_media_type_t mediaType;
@end

@implementation VLCHomeDataSourceTestMediaItem
@end

static VLCMediaLibraryMediaItem *VLCHomeDataSourceMediaItem(int64_t libraryID, vlc_ml_media_type_t type)
{
    VLCHomeDataSourceTestMediaItem * const item = [[VLCHomeDataSourceTestMediaItem alloc] init];
    item.libraryID = libraryID;
    item.mediaType = type;
    return (id)item;
}

// The production source registers these classes, but card/nib rendering is
// separate from the ordered snapshots and represented-item context tested here.
NSString *VLCLibraryCollectionViewItemIdentifier = @"VLCLibraryCollectionViewItemIdentifier";
NSString *const VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier =
    @"VLCLibraryCollectionViewMediaItemSupplementaryDetailViewIdentifier";

@implementation VLCMediaItemCollectionViewItem
@end

@implementation VLCLibraryCollectionViewItem
@end

@interface VLCHomeDataSourceTestCard : NSCollectionViewItem<VLCLibraryRepresentedItemCollectionViewItem>
@property (strong, nonatomic) VLCLibraryRepresentedItem *representedItem;
@end

@implementation VLCHomeDataSourceTestCard
@end

@interface VLCHomeDataSourceTestCollection : NSCollectionView
@end

@implementation VLCHomeDataSourceTestCollection

- (void)reloadData {}

- (NSCollectionViewItem *)makeItemWithIdentifier:(NSUserInterfaceItemIdentifier)identifier
                                    forIndexPath:(NSIndexPath *)indexPath
{
    return [[VLCHomeDataSourceTestCard alloc] init];
}

@end

@interface VLCHomeDataSourceTestModel : NSObject
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *recentlyAddedItems;
@property (strong) dispatch_semaphore_t readGate;
@property (copy) dispatch_block_t readCaptured;
@end

@implementation VLCHomeDataSourceTestModel

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentlyAddedMedia
{
    NSArray *items;
    dispatch_semaphore_t gate;
    dispatch_block_t captured;
    @synchronized (self) {
        items = self.recentlyAddedItems.copy;
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

@end

@interface VLCLibraryHomeVideoDataSourceTest : VLCAppKitMockingTestCase
{
    VLCHomeDataSourceTestModel *_model;
    VLCHomeDataSourceTestCollection *_collection;
    VLCLibraryHomeViewVideoContainerViewDataSource *_dataSource;
    VLCLibraryVideoCollectionViewGroupDescriptor *_groupDescriptor;
}
@end

@implementation VLCLibraryHomeVideoDataSourceTest

- (void)setUp
{
    [super setUp];
    _model = [[VLCHomeDataSourceTestModel alloc] init];
    _model.recentlyAddedItems = @[
        VLCHomeDataSourceMediaItem(10, VLC_ML_MEDIA_TYPE_AUDIO),
        VLCHomeDataSourceMediaItem(20, VLC_ML_MEDIA_TYPE_VIDEO),
    ];
    __weak typeof(self) weakSelf = self;
    [self replaceClassMethod:@selector(sharedInstance)
                     onClass:VLCMain.class
                   withBlock:^id(id __unused receiver) {
        return weakSelf;
    }];
    _collection = [[VLCHomeDataSourceTestCollection alloc] init];
    _dataSource = [[VLCLibraryHomeViewVideoContainerViewDataSource alloc] init];
    _dataSource.collectionView = _collection;
    _collection.dataSource = _dataSource;
    [self addTeardownBlock:^{
        VLCLibraryHomeVideoDataSourceTest * const test = weakSelf;
        [test->_dataSource disconnect];
        test->_collection.dataSource = nil;
        test->_dataSource = nil;
        [test drainMainQueue];
    }];
    XCTestExpectation * const loaded = [self expectDisplayedChange];
    _groupDescriptor = [[VLCLibraryVideoCollectionViewGroupDescriptor alloc]
        initWithVLCVideoLibraryGroup:VLCMediaLibraryParentGroupTypeRecentlyAdded];
    _dataSource.groupDescriptor = _groupDescriptor;
    [self waitForExpectations:@[loaded] timeout:5.];
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
    return [self expectationForNotification:VLCLibraryVideoCollectionViewDataSourceDisplayedCollectionChangedNotification
                                     object:_dataSource
                                    handler:nil];
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

- (void)testRecentlyAddedKeepsMixedMediaOrderAndParentContext
{
    XCTAssertEqualObjects(self.displayedIDs, (@[@10, @20]));
    for (NSUInteger index = 0; index < 2; ++index) {
        NSIndexPath * const path = [NSIndexPath indexPathForItem:index inSection:0];
        VLCHomeDataSourceTestCard * const card = (id)[_dataSource collectionView:_collection
                                             itemForRepresentedObjectAtIndexPath:path];
        XCTAssertEqual(card.representedItem.item, _model.recentlyAddedItems[index]);
        XCTAssertEqual(card.representedItem.parentType, VLCMediaLibraryParentGroupTypeRecentlyAdded);
        NSArray<VLCLibraryRepresentedItem *> * const actions = [_dataSource
            representedItemsAtIndexPaths:[NSSet setWithObject:path]
                       forCollectionView:_collection];
        XCTAssertEqual(actions.firstObject.item, _model.recentlyAddedItems[index]);
        XCTAssertEqual(actions.firstObject.parentType, VLCMediaLibraryParentGroupTypeRecentlyAdded);
        XCTAssertEqualObjects([_dataSource indexPathForLibraryItem:_model.recentlyAddedItems[index]], path);
    }
}

- (void)testRecentlyAddedResetAndCacheDropRefreshTheDisplayedSnapshot
{
    VLCMediaLibraryMediaItem * const updated = VLCHomeDataSourceMediaItem(20, VLC_ML_MEDIA_TYPE_VIDEO);
    VLCMediaLibraryMediaItem * const newest = VLCHomeDataSourceMediaItem(30, VLC_ML_MEDIA_TYPE_AUDIO);
    _model.recentlyAddedItems = @[newest, updated];
    XCTestExpectation * const reset = [self expectDisplayedChange];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentlyAddedMediaListReset object:_model];
    [self waitForExpectations:@[reset] timeout:5.];
    XCTAssertEqualObjects(self.displayedIDs, (@[@30, @20]));
    XCTAssertEqual([_dataSource libraryItemAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:0]
                                     forCollectionView:_collection], updated);

    _model.recentlyAddedItems = @[];
    XCTestExpectation * const dropped = [self expectDisplayedChange];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelAllCachesDropped object:_model];
    [self waitForExpectations:@[dropped] timeout:5.];
    XCTAssertEqualObjects(self.displayedIDs, @[]);
}

- (void)testRecentlyAddedReconnectLoadsChangesMissedWhileDisconnected
{
    [_dataSource disconnect];
    _model.recentlyAddedItems = @[_model.recentlyAddedItems.lastObject];
    XCTestExpectation * const disconnected = [self expectDisplayedChange];
    disconnected.inverted = YES;
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentlyAddedMediaListReset object:_model];
    [self waitForExpectations:@[disconnected] timeout:.2];
    XCTAssertEqualObjects(self.displayedIDs, (@[@10, @20]));

    XCTestExpectation * const reconnected = [self expectDisplayedChange];
    [_dataSource connect];
    [self waitForExpectations:@[reconnected] timeout:5.];
    XCTAssertEqualObjects(self.displayedIDs, (@[@20]));
}

- (dispatch_semaphore_t)holdNextSnapshot
{
    dispatch_semaphore_t const gate = dispatch_semaphore_create(0);
    XCTestExpectation * const captured = [self expectationWithDescription:@"Older snapshot captured"];
    _model.readGate = gate;
    _model.readCaptured = ^{ [captured fulfill]; };
    [self addTeardownBlock:^{ dispatch_semaphore_signal(gate); }];
    [_dataSource reloadData];
    [self waitForExpectations:@[captured] timeout:5.];
    return gate;
}

- (void)testRecentlyAddedDisconnectRejectsPendingSnapshot
{
    dispatch_semaphore_t const gate = [self holdNextSnapshot];
    [_dataSource disconnect];
    XCTestExpectation * const late = [self expectDisplayedChange];
    late.inverted = YES;
    dispatch_semaphore_signal(gate);
    [self waitForExpectations:@[late] timeout:.2];
    XCTAssertEqualObjects(self.displayedIDs, (@[@10, @20]));
}

- (void)testRecentlyAddedNewerReloadRejectsOlderSnapshot
{
    dispatch_semaphore_t const gate = [self holdNextSnapshot];
    _model.recentlyAddedItems = @[VLCHomeDataSourceMediaItem(30, VLC_ML_MEDIA_TYPE_VIDEO)];
    XCTestExpectation * const latest = [self expectDisplayedChange];
    [_dataSource reloadData];
    [self waitForExpectations:@[latest] timeout:5.];
    XCTAssertEqualObjects(self.displayedIDs, (@[@30]));

    XCTestExpectation * const late = [self expectDisplayedChange];
    late.inverted = YES;
    dispatch_semaphore_signal(gate);
    [self waitForExpectations:@[late] timeout:.2];
    XCTAssertEqualObjects(self.displayedIDs, (@[@30]));
}

@end
