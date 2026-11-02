/*****************************************************************************
 * VLCLibraryHomeMediaDataSourceTest.m: Recently Added source tests
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
#import "library/home-library/VLCLibraryHomeViewMediaContainerViewDataSource.h"
#import "library/VLCLibraryMediaGroupDescriptor.h"
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
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *videoItems;
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *audioItems;
@property vlc_ml_media_type_t readType;
@property (strong) dispatch_semaphore_t readGate;
@property (copy) dispatch_block_t readCaptured;
@property NSUInteger videoReads;
@property NSUInteger audioReads;
@end

@implementation VLCHomeDataSourceTestModel

- (NSArray<VLCMediaLibraryMediaItem *> *)snapshotForType:(vlc_ml_media_type_t)type
{
    NSArray *items;
    dispatch_semaphore_t gate = nil;
    dispatch_block_t captured = nil;
    @synchronized (self) {
        if (type == VLC_ML_MEDIA_TYPE_VIDEO) {
            items = self.videoItems.copy;
            self.videoReads++;
        } else {
            items = self.audioItems.copy;
            self.audioReads++;
        }
        if (self.readType == type) {
            gate = self.readGate;
            captured = self.readCaptured;
            self.readGate = nil;
            self.readCaptured = nil;
        }
    }
    if (gate != nil) {
        dispatch_async(dispatch_get_main_queue(), captured);
        dispatch_semaphore_wait(gate, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
    }
    return items;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentlyAddedVideoMedia
{
    return [self snapshotForType:VLC_ML_MEDIA_TYPE_VIDEO];
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentlyAddedAudioMedia
{
    return [self snapshotForType:VLC_ML_MEDIA_TYPE_AUDIO];
}

@end

@interface VLCLibraryHomeMediaDataSourceTest : VLCAppKitMockingTestCase
{
    VLCHomeDataSourceTestModel *_model;
    VLCLibraryHomeViewMediaContainerViewDataSource *_videoSource;
    VLCLibraryHomeViewMediaContainerViewDataSource *_audioSource;
    VLCHomeDataSourceTestCollection *_videoCollection;
    VLCHomeDataSourceTestCollection *_audioCollection;
    NSMutableDictionary<NSNumber *, VLCLibraryMediaCollectionViewGroupDescriptor *> *_descriptors;
}
@end

@implementation VLCLibraryHomeMediaDataSourceTest

- (void)setUp
{
    [super setUp];
    _model = [[VLCHomeDataSourceTestModel alloc] init];
    _model.videoItems = @[
        VLCHomeDataSourceMediaItem(20, VLC_ML_MEDIA_TYPE_VIDEO),
        VLCHomeDataSourceMediaItem(30, VLC_ML_MEDIA_TYPE_VIDEO)
    ];
    _model.audioItems = @[
        VLCHomeDataSourceMediaItem(10, VLC_ML_MEDIA_TYPE_AUDIO),
        VLCHomeDataSourceMediaItem(40, VLC_ML_MEDIA_TYPE_AUDIO)
    ];
    __weak typeof(self) weakSelf = self;
    [self replaceClassMethod:@selector(sharedInstance)
                     onClass:VLCMain.class
                   withBlock:^id(id __unused receiver) {
        return weakSelf;
    }];
    _descriptors = NSMutableDictionary.dictionary;
    _videoCollection = [[VLCHomeDataSourceTestCollection alloc] init];
    _audioCollection = [[VLCHomeDataSourceTestCollection alloc] init];
    _videoSource = [[VLCLibraryHomeViewMediaContainerViewDataSource alloc] init];
    _audioSource = [[VLCLibraryHomeViewMediaContainerViewDataSource alloc] init];
    _videoSource.collectionView = _videoCollection;
    _audioSource.collectionView = _audioCollection;
    _videoCollection.dataSource = _videoSource;
    _audioCollection.dataSource = _audioSource;
    [self addTeardownBlock:^{
        VLCLibraryHomeMediaDataSourceTest * const test = weakSelf;
        [test->_videoSource disconnect];
        [test->_audioSource disconnect];
        test->_videoCollection.dataSource = nil;
        test->_audioCollection.dataSource = nil;
        test->_videoSource = nil;
        test->_audioSource = nil;
        [test drainMainQueue];
    }];
    XCTestExpectation * const videosLoaded = [self expectDisplayedChangeForSource:_videoSource];
    XCTestExpectation * const audioLoaded = [self expectDisplayedChangeForSource:_audioSource];
    [self configureSource:_videoSource group:VLCMediaLibraryParentGroupTypeRecentlyAddedVideos];
    [self configureSource:_audioSource group:VLCMediaLibraryParentGroupTypeRecentlyAddedAudios];
    [self waitForExpectations:@[
        videosLoaded,
        audioLoaded
    ]
                      timeout:5.];
}

- (id)libraryController { return self; }
- (id)libraryModel { return _model; }

- (void)configureSource:(VLCLibraryHomeViewMediaContainerViewDataSource *)source
                  group:(VLCMediaLibraryParentGroupType)group
{
    VLCLibraryMediaCollectionViewGroupDescriptor * const descriptor =
        [[VLCLibraryMediaCollectionViewGroupDescriptor alloc] initWithLibraryGroup:group];
    _descriptors[@(group)] = descriptor;
    source.groupDescriptor = descriptor;
}

- (XCTestExpectation *)expectDisplayedChangeForSource:(VLCLibraryHomeViewMediaContainerViewDataSource *)source
{
    return [self expectationForNotification:VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification
                                     object:source
                                    handler:nil];
}

- (NSArray<NSNumber *> *)displayedIDsForSource:(VLCLibraryHomeViewMediaContainerViewDataSource *)source
{
    NSMutableArray<NSNumber *> * const identifiers = NSMutableArray.array;
    const NSUInteger count = [source collectionView:source.collectionView numberOfItemsInSection:0];
    for (NSUInteger index = 0; index < count; ++index) {
        id<VLCMediaLibraryItemProtocol> const item =
            [source libraryItemAtIndexPath:[NSIndexPath indexPathForItem:index inSection:0]
                         forCollectionView:source.collectionView];
        [identifiers addObject:@(item.libraryID)];
    }
    return identifiers;
}

- (void)testRecentlyAddedTypedSourcesKeepTheirQueryOrderAndPlaybackContext
{
    for (VLCLibraryHomeViewMediaContainerViewDataSource *source in @[
        _videoSource,
        _audioSource
    ]) {
        const BOOL videos = source == _videoSource;
        NSArray<VLCMediaLibraryMediaItem *> * const items = videos ? _model.videoItems : _model.audioItems;
        const VLCMediaLibraryParentGroupType group = videos
            ? VLCMediaLibraryParentGroupTypeRecentlyAddedVideos
            : VLCMediaLibraryParentGroupTypeRecentlyAddedAudios;
        NSArray<NSNumber *> * const expectedIDs = videos ? @[@20, @30] : @[@10, @40];
        XCTAssertEqualObjects([self displayedIDsForSource:source], expectedIDs);
        for (NSUInteger index = 0; index < items.count; ++index) {
            NSIndexPath * const path = [NSIndexPath indexPathForItem:index inSection:0];
            VLCHomeDataSourceTestCard * const card = (id)[source collectionView:source.collectionView
                                            itemForRepresentedObjectAtIndexPath:path];
            XCTAssertEqual(card.representedItem.item, items[index]);
            XCTAssertEqual(card.representedItem.parentType, group);
            NSArray<VLCLibraryRepresentedItem *> * const actions =
                [source representedItemsAtIndexPaths:[NSSet setWithObject:path]
                                   forCollectionView:source.collectionView];
            XCTAssertEqual(actions.firstObject.item, items[index]);
            XCTAssertEqual(actions.firstObject.parentType, group);
            XCTAssertEqualObjects([source indexPathForLibraryItem:items[index]], path);
        }
    }
}

- (void)testTypedNotificationsRefreshOnlyTheirSourceAndCacheDropRefreshesBoth
{
    _model.videoItems = @[VLCHomeDataSourceMediaItem(50, VLC_ML_MEDIA_TYPE_VIDEO)];
    _model.audioItems = @[VLCHomeDataSourceMediaItem(60, VLC_ML_MEDIA_TYPE_AUDIO)];
    const NSUInteger audioReads = _model.audioReads;
    XCTestExpectation * const videosChanged = [self expectDisplayedChangeForSource:_videoSource];
    XCTestExpectation * const audioUnchanged = [self expectDisplayedChangeForSource:_audioSource];
    audioUnchanged.inverted = YES;
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentlyAddedVideoMediaListReset object:_model];
    [self waitForExpectations:@[videosChanged] timeout:5.];
    [self waitForExpectations:@[audioUnchanged] timeout:.2];
    XCTAssertEqualObjects([self displayedIDsForSource:_videoSource], (@[@50]));
    XCTAssertEqualObjects([self displayedIDsForSource:_audioSource], (@[@10, @40]));
    XCTAssertEqual(_model.audioReads, audioReads);

    const NSUInteger videoReads = _model.videoReads;
    XCTestExpectation * const audioChanged = [self expectDisplayedChangeForSource:_audioSource];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentlyAddedAudioMediaListReset object:_model];
    [self waitForExpectations:@[audioChanged] timeout:5.];
    XCTAssertEqualObjects([self displayedIDsForSource:_audioSource], (@[@60]));
    XCTAssertEqual(_model.videoReads, videoReads);

    _model.videoItems = @[];
    _model.audioItems = @[];
    XCTestExpectation * const videosDropped = [self expectDisplayedChangeForSource:_videoSource];
    XCTestExpectation * const audioDropped = [self expectDisplayedChangeForSource:_audioSource];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelAllCachesDropped object:_model];
    [self waitForExpectations:@[
        videosDropped,
        audioDropped
    ]
                      timeout:5.];
    XCTAssertEqualObjects([self displayedIDsForSource:_videoSource], @[]);
    XCTAssertEqualObjects([self displayedIDsForSource:_audioSource], @[]);
}

- (void)testRecentlyAddedReconnectLoadsBothTypedSnapshotsMissedWhileDisconnected
{
    [_videoSource disconnect];
    [_audioSource disconnect];
    _model.videoItems = @[_model.videoItems.lastObject];
    _model.audioItems = @[_model.audioItems.lastObject];
    XCTestExpectation * const videosDisconnected = [self expectDisplayedChangeForSource:_videoSource];
    XCTestExpectation * const audioDisconnected = [self expectDisplayedChangeForSource:_audioSource];
    videosDisconnected.inverted = YES;
    audioDisconnected.inverted = YES;
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentlyAddedVideoMediaListReset object:_model];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentlyAddedAudioMediaListReset object:_model];
    [self waitForExpectations:@[
        videosDisconnected,
        audioDisconnected
    ]
                      timeout:.2];
    XCTAssertEqualObjects([self displayedIDsForSource:_videoSource], (@[@20, @30]));
    XCTAssertEqualObjects([self displayedIDsForSource:_audioSource], (@[@10, @40]));

    XCTestExpectation * const videosReconnected = [self expectDisplayedChangeForSource:_videoSource];
    XCTestExpectation * const audioReconnected = [self expectDisplayedChangeForSource:_audioSource];
    [_videoSource connect];
    [_audioSource connect];
    [self waitForExpectations:@[
        videosReconnected,
        audioReconnected
    ]
                      timeout:5.];
    XCTAssertEqualObjects([self displayedIDsForSource:_videoSource], (@[@30]));
    XCTAssertEqualObjects([self displayedIDsForSource:_audioSource], (@[@40]));
}

- (dispatch_semaphore_t)holdNextSnapshotForSource:(VLCLibraryHomeViewMediaContainerViewDataSource *)source
{
    dispatch_semaphore_t const gate = dispatch_semaphore_create(0);
    XCTestExpectation * const captured = [self expectationWithDescription:@"Older typed snapshot captured"];
    _model.readType = source == _videoSource ? VLC_ML_MEDIA_TYPE_VIDEO : VLC_ML_MEDIA_TYPE_AUDIO;
    _model.readGate = gate;
    _model.readCaptured = ^{ [captured fulfill]; };
    [self addTeardownBlock:^{ dispatch_semaphore_signal(gate); }];
    [source reloadData];
    [self waitForExpectations:@[captured] timeout:5.];
    return gate;
}

- (void)testRecentlyAddedDisconnectRejectsBothTypedPendingSnapshots
{
    for (VLCLibraryHomeViewMediaContainerViewDataSource *source in @[
        _videoSource,
        _audioSource
    ]) {
        NSArray<NSNumber *> * const originalIDs = [self displayedIDsForSource:source];
        dispatch_semaphore_t const gate = [self holdNextSnapshotForSource:source];
        [source disconnect];
        XCTestExpectation * const late = [self expectDisplayedChangeForSource:source];
        late.inverted = YES;
        dispatch_semaphore_signal(gate);
        [self waitForExpectations:@[late] timeout:.2];
        XCTAssertEqualObjects([self displayedIDsForSource:source], originalIDs);
    }
}

- (void)testRecentlyAddedNewerTypedReloadRejectsOlderSnapshot
{
    for (VLCLibraryHomeViewMediaContainerViewDataSource *source in @[
        _videoSource,
        _audioSource
    ]) {
        dispatch_semaphore_t const gate = [self holdNextSnapshotForSource:source];
        if (source == _videoSource)
            _model.videoItems = @[VLCHomeDataSourceMediaItem(50, VLC_ML_MEDIA_TYPE_VIDEO)];
        else
            _model.audioItems = @[VLCHomeDataSourceMediaItem(60, VLC_ML_MEDIA_TYPE_AUDIO)];
        XCTestExpectation * const latest = [self expectDisplayedChangeForSource:source];
        [source reloadData];
        [self waitForExpectations:@[latest] timeout:5.];
        NSArray<NSNumber *> * const expectedIDs = source == _videoSource ? @[@50] : @[@60];
        XCTAssertEqualObjects([self displayedIDsForSource:source], expectedIDs);

        XCTestExpectation * const late = [self expectDisplayedChangeForSource:source];
        late.inverted = YES;
        dispatch_semaphore_signal(gate);
        [self waitForExpectations:@[late] timeout:.2];
        XCTAssertEqualObjects([self displayedIDsForSource:source], expectedIDs);
    }
}

@end
