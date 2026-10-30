/*****************************************************************************
 * VLCLibraryHomeVideoCarouselTest.m: Home video carousel lifecycle tests
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

#import <objc/runtime.h>

#import "extensions/NSView+VLCAdditions.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryHeroView.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryRepresentedItemCollectionViewItem.h"
#import "library/home-library/VLCLibraryHomeViewMediaContainerViewDataSource.h"
#import "library/home-library/VLCLibraryHomeViewVideoCarouselContainerView.h"
#import "views/VLCSubScrollView.h"
#import "views/VLCUIUnits.h"
#import "main/VLCMain.h"

// Model snapshots are controlled values; no backing media library is needed.
@interface VLCHomeLifecycleTestMediaItem : NSObject
@property int64_t libraryID;
@property float progress;
@property vlc_ml_media_type_t mediaType;
@property (copy) NSString *displayString;
@property (copy) NSString *primaryDetailString;
@end

@implementation VLCHomeLifecycleTestMediaItem
@end

static VLCMediaLibraryMediaItem *VLCHomeLifecycleMediaItem(int64_t libraryID, NSString *title)
{
    VLCHomeLifecycleTestMediaItem * const item = [[VLCHomeLifecycleTestMediaItem alloc] init];
    item.libraryID = libraryID;
    item.mediaType = VLC_ML_MEDIA_TYPE_VIDEO;
    item.displayString = title;
    item.primaryDetailString = @"Video details";
    return (id)item;
}

// Exercise the production container and its supplementary-view data source with
// controlled model snapshots. Selection, filtering, counts, and nib decoding
// use their production implementations; rendering uses simple test cards.
static NSUserInterfaceItemIdentifier const VLCHomeLifecycleTestItemIdentifier = @"VLCHomeLifecycleTestItem";

@interface VLCHomeLifecycleTestItem : NSCollectionViewItem<VLCLibraryRepresentedItemCollectionViewItem>
@property (strong, nonatomic) VLCLibraryRepresentedItem *representedItem;
@end

@implementation VLCHomeLifecycleTestItem

- (void)loadView
{
    self.view = [[NSView alloc] initWithFrame:NSZeroRect];
}

@end

@interface VLCHomeLifecycleTestModel : NSObject
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *recentItems;
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *videoItems;
@end

@implementation VLCHomeLifecycleTestModel

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfRecentMedia
{
    return self.recentItems;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfVideoMedia
{
    return self.videoItems;
}

@end

@interface VLCHomeNibLifecycleTestContainerView : VLCLibraryHomeViewVideoCarouselContainerView
@property NSUInteger testDataSourceSetupCount;
@property NSUInteger testNibAwakeningCount;
@end

@implementation VLCHomeNibLifecycleTestContainerView

- (void)setupDataSource
{
    // A nested nib can awaken its supplied owner before setup has finished.
    // Count that reentry, but bound it so a regression fails without overflowing
    // the test process's stack.
    if (++_testDataSourceSetupCount == 1) {
        [super setupDataSource];
    }
}

- (void)awakeFromNib
{
    ++_testNibAwakeningCount;
    [super awakeFromNib];
}

@end

@interface VLCLibraryHomeVideoCarouselTest : VLCAppKitMockingTestCase
{
    VLCHomeNibLifecycleTestContainerView *_container;
    NSWindow *_window;
    NSUInteger _heroNibLoadCount;
    NSUInteger _heroItemSelectionCount;
    NSUInteger _dataSourceReloadCount;
    VLCHomeLifecycleTestModel *_model;
    __weak id _heroNibOwner;
}
@end

@implementation VLCLibraryHomeVideoCarouselTest

- (void)setUp
{
    [super setUp];
    _model = [[VLCHomeLifecycleTestModel alloc] init];
    _model.recentItems = @[];
    _model.videoItems = @[];
    __weak typeof(self) weakSelf = self;
    [self replaceClassMethod:@selector(sharedInstance)
                     onClass:VLCMain.class
                   withBlock:^id(id __unused receiver) {
        return weakSelf;
    }];
    SEL const selectionSelector = @selector(setOptimalRepresentedItem);
    IMP const selectItem = class_getMethodImplementation(VLCLibraryHeroView.class, selectionSelector);
    [self replaceInstanceMethod:selectionSelector
                        onClass:VLCLibraryHeroView.class
                      withBlock:^(id receiver) {
        VLCLibraryHomeVideoCarouselTest * const test = weakSelf;
        if (test != nil) {
            ++test->_heroItemSelectionCount;
        }
        ((void (*)(id, SEL))selectItem)(receiver, selectionSelector);
    }];
    SEL const reloadSelector = @selector(reloadData);
    IMP const reloadData = class_getMethodImplementation(VLCLibraryHomeViewMediaContainerViewDataSource.class, reloadSelector);
    [self replaceInstanceMethod:reloadSelector
                        onClass:VLCLibraryHomeViewMediaContainerViewDataSource.class
                      withBlock:^(id receiver) {
        VLCLibraryHomeVideoCarouselTest * const test = weakSelf;
        if (test != nil) {
            ++test->_dataSourceReloadCount;
        }
        ((void (*)(id, SEL))reloadData)(receiver, reloadSelector);
    }];
    [self replaceInstanceMethod:@selector(setup)
                        onClass:VLCLibraryHomeViewMediaContainerViewDataSource.class
                      withBlock:^(VLCLibraryHomeViewMediaContainerViewDataSource *dataSource) {
        // Keep the real source and index mapping, avoiding the unrelated grid
        // card and expandable-detail nib dependencies.
        [dataSource.collectionView registerClass:VLCHomeLifecycleTestItem.class
                           forItemWithIdentifier:dataSource.collectionViewItemIdentifier];
        dataSource.collectionView.dataSource = dataSource;
    }];
    [self replaceInstanceMethod:NSSelectorFromString(@"updateRepresentedItem")
                        onClass:VLCLibraryHeroView.class
                      withBlock:^(id __unused receiver) {
        // Selection and media metadata are exercised by the real Hero tests.
    }];
    [self replaceClassMethod:@selector(fromNibWithOwner:)
                     onClass:VLCLibraryHeroView.class
                   withBlock:^id(id __unused receiver, id owner) {
        // Only redirect resource lookup into the test bundle. Preserve actual
        // NSNib decoding, outlets, and awakening of the supplied owner.
        return [weakSelf loadHeroNibWithOwner:owner];
    }];
    _container = [[VLCHomeNibLifecycleTestContainerView alloc] init];
    XCTestExpectation * const initialLoad = [self expectSourceItems:@[]];
    _container.videoGroup = VLCMediaLibraryParentGroupTypeRecentVideos;
    [self waitForExpectations:@[initialLoad] timeout:5.];
    [self addTeardownBlock:^{
        VLCLibraryHomeVideoCarouselTest * const test = weakSelf;
        if (test != nil) {
            [test->_container disconnect];
            test->_container.collectionView.dataSource = nil;
            [test->_window close];
            test->_container = nil;
            test->_window = nil;
            [test drainMainQueue];
        }
    }];
}

- (VLCLibraryHeroView *)loadHeroNibWithOwner:(id)owner
{
    ++_heroNibLoadCount;
    _heroNibOwner = owner;
    NSBundle * const bundle = [NSBundle bundleForClass:VLCLibraryHomeVideoCarouselTest.class];
    NSNib * const nib = [[NSNib alloc] initWithNibNamed:@"VLCLibraryHeroView" bundle:bundle];
    XCTAssertNotNil(nib);
    NSArray *topLevelObjects;
    XCTAssertTrue([nib instantiateWithOwner:owner topLevelObjects:&topLevelObjects]);
    for (id object in topLevelObjects) {
        if ([object isKindOfClass:VLCLibraryHeroView.class]) {
            return object;
        }
    }
    XCTFail(@"The real Hero nib must contain its Hero view");
    return nil;
}

- (void)layoutContainer
{
    if (_window == nil) {
        _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0., 0., 900., 450.)
                                              styleMask:NSWindowStyleMaskBorderless
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
        _window.releasedWhenClosed = NO;
        _container.translatesAutoresizingMaskIntoConstraints = NO;
        [_window.contentView addSubview:_container];
        [NSLayoutConstraint activateConstraints:@[
            [_container.leadingAnchor constraintEqualToAnchor:_window.contentView.leadingAnchor],
            [_container.trailingAnchor constraintEqualToAnchor:_window.contentView.trailingAnchor],
            [_container.topAnchor constraintEqualToAnchor:_window.contentView.topAnchor],
        ]];
    }
    [_window.contentView layoutSubtreeIfNeeded];
    [self drainMainQueue];
    [_window.contentView layoutSubtreeIfNeeded];
}

- (id)libraryController
{
    return self;
}

- (id)libraryModel
{
    return _model;
}

- (XCTestExpectation *)expectSourceItems:(NSArray<VLCMediaLibraryMediaItem *> *)items
{
    __block BOOL observedSnapshot = NO;
    return [self expectationForNotification:VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification
                                     object:_container.dataSource
                                    handler:^BOOL(NSNotification *notification) {
        VLCLibraryHomeViewMediaContainerViewDataSource * const source = notification.object;
        if (!observedSnapshot && source.numberOfMediaItems == items.count && source.firstMediaItem == items.firstObject) {
            observedSnapshot = YES;
            return YES;
        }
        return NO;
    }];
}

- (void)setRecentItems:(NSArray<VLCMediaLibraryMediaItem *> *)items
{
    _model.recentItems = items;
    XCTestExpectation * const loaded = [self expectSourceItems:items];
    [_container.dataSource reloadData];
    [self waitForExpectations:@[loaded] timeout:5.];
    [self layoutContainer];
    XCTAssertEqual(_container.headerView.itemCount, items.count);
}

- (void)setRecentItemCount:(NSUInteger)count
{
    NSMutableArray * const items = NSMutableArray.array;
    for (NSUInteger index = 0; index < count; ++index) {
        [items addObject:VLCHomeLifecycleMediaItem(100 + index,
            [NSString stringWithFormat:@"Recent video %lu", (unsigned long)index])];
    }
    [self setRecentItems:items];
}

- (void)addUnrelatedLibraryVideo
{
    VLCMediaLibraryMediaItem * const item = VLCHomeLifecycleMediaItem(1000, @"Random library video");
    _model.videoItems = @[item];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelVideoMediaListReset object:_model];
}

- (NSArray<NSNumber *> *)standardVideoIDs
{
    NSMutableArray * const identifiers = NSMutableArray.array;
    const NSUInteger count = [_container.dataSource collectionView:_container.collectionView numberOfItemsInSection:0];
    for (NSUInteger index = 0; index < count; ++index) {
        id<VLCMediaLibraryItemProtocol> const item = [_container.dataSource
            libraryItemAtIndexPath:[NSIndexPath indexPathForItem:index inSection:0]
                 forCollectionView:_container.collectionView];
        [identifiers addObject:@(item.libraryID)];
    }
    return identifiers;
}

- (void)assertHeroIsEmbeddedInSectionHeader
{
    [self layoutContainer];
    NSIndexPath * const path = [NSIndexPath indexPathForItem:0 inSection:0];
    NSView * const header = [_container.collectionView supplementaryViewForElementKind:NSCollectionElementKindSectionHeader
                                                                           atIndexPath:path];
    XCTAssertNotNil(header);
    XCTAssertEqual(_container.heroView.superview, header);
    XCTAssertTrue([_container.heroView isDescendantOf:_container.collectionView]);
    XCTAssertEqual(_container.heroView.carouselView, _container.collectionView);
    XCTAssertEqualWithAccuracy(NSMinX(_container.heroView.frame), VLCUIUnits.mediumSpacing, .01);
    XCTAssertEqualWithAccuracy(NSMaxX(_container.heroView.frame), NSWidth(header.bounds), .01);
    XCTAssertEqualWithAccuracy(NSHeight(_container.heroView.frame), NSHeight(_container.scrollView.contentView.bounds), .01);
    XCTAssertFalse(_container.hidden);
    XCTAssertFalse(_container.heroView.hidden);
    XCTAssertEqual(_container.heroView.selectionMode, VLCLibraryHeroViewSelectionModeMostRecentVideo);
}

- (void)assertCarouselAndEmbeddedHeroAreHidden
{
    [self layoutContainer];
    XCTAssertTrue(_container.hidden);
    XCTAssertTrue(_container.heroView.hidden);
    XCTAssertNil(_container.heroView.representedItem);
    XCTAssertEqual(_container.heroView.carouselView, _container.collectionView);
    XCTAssertEqual(_container.heroView.selectionMode, VLCLibraryHeroViewSelectionModeMostRecentVideo);
    XCTAssertEqual(_container.dataSource.numberOfMediaItems, (NSUInteger)0);
    XCTAssertEqual(_container.headerView.itemCount, (NSUInteger)0);
    XCTAssertEqual(_container.dataSource.excludedLibraryID, (int64_t)0);
    XCTAssertTrue(NSEqualSizes(_container.collectionViewLayout.headerReferenceSize, NSZeroSize));
}

- (void)testEmptyRecentVideosHideTheSectionAndNewItemsRestoreTwoRows
{
    XCTAssertEqual(_container.numberOfRows, (NSUInteger)2);
    XCTAssertTrue(_container.hidden);
    XCTAssertTrue(NSEqualSizes(_container.collectionViewLayout.headerReferenceSize, NSZeroSize));
    [self setRecentItemCount:3];
    XCTAssertFalse(_container.hidden);
    [self assertHeroIsEmbeddedInSectionHeader];
    XCTAssertEqualWithAccuracy(NSHeight(_container.scrollView.contentView.bounds),
                               _container.itemHeight * 2. + _container.collectionViewLayout.minimumInteritemSpacing, .01);
    [self setRecentItemCount:0];
    [self assertCarouselAndEmbeddedHeroAreHidden];
}

- (void)testRealHeroNibInitializesDataSourceOnceWithoutAwakeningContainer
{
    XCTAssertEqual(_heroNibLoadCount, (NSUInteger)1);
    XCTAssertNil(_heroNibOwner);
    XCTAssertEqual(_container.testNibAwakeningCount, (NSUInteger)0);
    XCTAssertEqual(_container.testDataSourceSetupCount, (NSUInteger)1);
    XCTAssertNotNil(_container.dataSource);
    XCTAssertNotNil(_container.heroView.largeImageView);
    XCTAssertNotNil(_container.heroView.explanationTextField);
    XCTAssertNotNil(_container.heroView.titleTextField);
    XCTAssertNotNil(_container.heroView.detailTextField);
    XCTAssertNotNil(_container.heroView.playButton);
    XCTAssertEqual(_container.heroView.playButton.target, _container.heroView);
    XCTAssertEqual(_container.heroView.playButton.action, @selector(playRepresentedItem:));
    [self addUnrelatedLibraryVideo];
    [self assertCarouselAndEmbeddedHeroAreHidden];
    [self setRecentItemCount:1];
    [self assertHeroIsEmbeddedInSectionHeader];
}

- (void)testRealHeroNibWithUnfinishedContainerOwnerReentersSetup
{
    // Positive control reproducing the original crash's callback sequence.
    // The counting subclass stops the second setup before stack overflow.
    VLCHomeNibLifecycleTestContainerView * const owner = [VLCHomeNibLifecycleTestContainerView alloc];
    __weak typeof(self) weakSelf = self;
    [self replaceClassMethod:@selector(fromNibWithOwner:)
                     onClass:VLCLibraryHeroView.class
                   withBlock:^id(id __unused receiver, id __unused suppliedOwner) {
        return [weakSelf loadHeroNibWithOwner:owner];
    }];
    VLCHomeNibLifecycleTestContainerView * const initializedContainer = [owner init];
    XCTAssertEqual(initializedContainer, owner);
    XCTAssertEqual(owner.testNibAwakeningCount, (NSUInteger)1);
    XCTAssertEqual(owner.testDataSourceSetupCount, (NSUInteger)2);
    XCTAssertNotNil(owner.heroView);
    XCTAssertNotNil(owner.dataSource);
    [owner disconnect];
}

- (void)testEmbeddedHeroKeepsItsCarouselOwnerAcrossEmptyAndRestoredHistory
{
    [self addUnrelatedLibraryVideo];
    [self assertCarouselAndEmbeddedHeroAreHidden];

    [self setRecentItemCount:3];
    [self assertHeroIsEmbeddedInSectionHeader];
    VLCLibraryHeroView * const hero = _container.heroView;
    NSView * const header = hero.superview;
    [self setRecentItemCount:0];
    [self assertCarouselAndEmbeddedHeroAreHidden];
    XCTAssertEqual(_container.heroView, hero);
    XCTAssertEqual(hero.superview, header);
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelVideoMediaItemUpdated
                                                      object:_model.videoItems.firstObject];
    [self assertCarouselAndEmbeddedHeroAreHidden];
    XCTAssertEqual(hero.superview, header);

    [self setRecentItemCount:2];
    [self assertHeroIsEmbeddedInSectionHeader];
    XCTAssertEqual(_container.heroView, hero);
}

- (void)testRecentSnapshotRestoresTheFeaturedHeroWhenItsRepresentationIsCleared
{
    [self setRecentItemCount:3];
    [self assertHeroIsEmbeddedInSectionHeader];
    VLCLibraryHeroView * const hero = _container.heroView;
    _container.heroView.representedItem = nil;
    [self assertHeroIsEmbeddedInSectionHeader];
    XCTAssertEqual(_container.heroView, hero);
    XCTAssertEqual(_container.heroView.representedItem.item, _container.dataSource.firstMediaItem);
    XCTAssertEqualObjects(self.standardVideoIDs, (@[@101, @102]));
}

- (void)testOnlyRecentVideosUseTwoRowsAndAnEmbeddedHero
{
    [self addUnrelatedLibraryVideo];
    [self setRecentItemCount:3];
    [self assertHeroIsEmbeddedInSectionHeader];
    XCTestExpectation * const videoLoad = [self expectSourceItems:_model.videoItems];
    _container.videoGroup = VLCMediaLibraryParentGroupTypeVideoLibrary;
    [self waitForExpectations:@[videoLoad] timeout:5.];
    [self layoutContainer];
    XCTAssertEqual(_container.numberOfRows, (NSUInteger)1);
    XCTAssertEqual(_container.heroView.selectionMode, VLCLibraryHeroViewSelectionModeMostRecentVideo);
    XCTAssertTrue(NSEqualSizes(_container.collectionViewLayout.headerReferenceSize, NSZeroSize));
    XCTAssertEqualWithAccuracy(NSHeight(_container.scrollView.contentView.bounds), _container.itemHeight, .01);
    XCTestExpectation * const recentLoad = [self expectSourceItems:_model.recentItems];
    _container.videoGroup = VLCMediaLibraryParentGroupTypeRecentVideos;
    [self waitForExpectations:@[recentLoad] timeout:5.];
    [self assertHeroIsEmbeddedInSectionHeader];
    XCTAssertEqual(_container.numberOfRows, (NSUInteger)2);
}

- (void)testReloadSelectsHeroAndReconnectRefreshesTheDisplayedItemCount
{
    [self addUnrelatedLibraryVideo];
    [self setRecentItemCount:3];
    const NSUInteger initialSelections = _heroItemSelectionCount;
    const NSUInteger initialReloads = _dataSourceReloadCount;
    XCTestExpectation * const reloaded = [self expectSourceItems:_model.recentItems];
    [_container reloadData];
    [self waitForExpectations:@[reloaded] timeout:5.];
    XCTAssertGreaterThan(_heroItemSelectionCount, initialSelections);
    XCTAssertEqual(_dataSourceReloadCount, initialReloads + 1);
    [_container disconnect];
    _model.recentItems = @[];
    XCTestExpectation * const reconnected = [self expectSourceItems:@[]];
    [_container connect];
    [self waitForExpectations:@[reconnected] timeout:5.];
    XCTAssertEqual(_container.headerView.itemCount, (NSUInteger)0);
    [self assertCarouselAndEmbeddedHeroAreHidden];
}

- (void)testOnlyRecentVideoRemainsFeaturedWhenItHasNoStandardCard
{
    [self addUnrelatedLibraryVideo];
    [self setRecentItemCount:1];
    XCTAssertEqual(_container.dataSource.numberOfMediaItems, (NSUInteger)1);
    XCTAssertEqual(_container.headerView.itemCount, (NSUInteger)1);
    XCTAssertEqualObjects(self.standardVideoIDs, @[]);
    XCTAssertEqual(_container.heroView.representedItem.item.libraryID, (int64_t)100);
    [self assertHeroIsEmbeddedInSectionHeader];

    VLCMediaLibraryMediaItem * const onlyRecentItem = _model.recentItems.firstObject;
    _model.recentItems = @[];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentsMediaItemDeleted object:onlyRecentItem];
    XCTAssertEqual(_container.dataSource.numberOfMediaItems, (NSUInteger)0);
    XCTAssertEqual(_container.headerView.itemCount, (NSUInteger)0);
    [self assertCarouselAndEmbeddedHeroAreHidden];
}

- (void)testRecentHeroSwapAndDeletionKeepFeaturedAndStandardVideosUnique
{
    [self setRecentItemCount:3];
    VLCMediaLibraryMediaItem * const first = _model.recentItems[0];
    VLCMediaLibraryMediaItem * const second = _model.recentItems[1];
    VLCMediaLibraryMediaItem * const third = _model.recentItems[2];
    XCTAssertEqual(_container.heroView.representedItem.item, first);
    XCTAssertEqualObjects(self.standardVideoIDs, (@[@101, @102]));

    [self setRecentItems:@[third, first, second]];
    XCTAssertEqual(_container.heroView.representedItem.item, third);
    XCTAssertEqualObjects(self.standardVideoIDs, (@[@100, @101]));
    XCTAssertEqual(_container.headerView.itemCount, (NSUInteger)3);
    _model.recentItems = @[first, second];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelRecentsMediaItemDeleted object:third];
    XCTAssertEqual(_container.heroView.representedItem.item, first);
    XCTAssertEqualObjects(self.standardVideoIDs, (@[@101]));
    XCTAssertEqual(_container.headerView.itemCount, (NSUInteger)2);
    [self assertHeroIsEmbeddedInSectionHeader];
}

@end
