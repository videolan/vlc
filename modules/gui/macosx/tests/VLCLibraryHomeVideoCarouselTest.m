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

#import "extensions/NSView+VLCAdditions.h"
#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/VLCLibraryHeroView.h"
#import "library/VLCLibraryRepresentedItemCollectionViewItem.h"
#import "library/home-library/VLCLibraryHomeViewMediaContainerViewDataSource.h"
#import "library/home-library/VLCLibraryHomeViewVideoCarouselContainerView.h"
#import "main/VLCMain.h"
#import "views/VLCSubScrollView.h"
#import "views/VLCUIUnits.h"

@interface VLCHomeLifecycleTestMediaItem : NSObject
@property int64_t libraryID;
@end

@implementation VLCHomeLifecycleTestMediaItem
@end

@interface VLCHomeLifecycleTestItem : NSCollectionViewItem<VLCLibraryRepresentedItemCollectionViewItem>
@property (strong, nonatomic) VLCLibraryRepresentedItem *representedItem;
@end

@implementation VLCHomeLifecycleTestItem

- (void)loadView
{
    self.view = [[NSView alloc] initWithFrame:NSZeroRect];
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
    NSUInteger _heroSelectionCount;
    __weak id _heroNibOwner;
}
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *listOfRecentMedia;
@end

@implementation VLCLibraryHomeVideoCarouselTest

- (void)setUp
{
    [super setUp];
    self.listOfRecentMedia = @[];
    __weak typeof(self) weakSelf = self;
    [self replaceClassMethod:@selector(sharedInstance) onClass:VLCMain.class withBlock:^id(id __unused receiver) {
        return weakSelf;
    }];
    [self replaceInstanceMethod:@selector(setup)
                        onClass:VLCLibraryHomeViewMediaContainerViewDataSource.class
                      withBlock:^(VLCLibraryHomeViewMediaContainerViewDataSource *source) {
        [source.collectionView registerClass:VLCHomeLifecycleTestItem.class
                       forItemWithIdentifier:source.collectionViewItemIdentifier];
        source.collectionView.dataSource = source;
    }];
    [self replaceInstanceMethod:@selector(setOptimalRepresentedItem)
                        onClass:VLCLibraryHeroView.class
                      withBlock:^(VLCLibraryHeroView * __unused hero) {
        VLCLibraryHomeVideoCarouselTest * const test = weakSelf;
        if (test != nil) {
            ++test->_heroSelectionCount;
        }
    }];
    [self replaceClassMethod:@selector(fromNibWithOwner:)
                     onClass:VLCLibraryHeroView.class
                   withBlock:^id(id __unused receiver, id owner) {
        // Only redirect resource lookup into the test bundle. Preserve actual
        // NSNib decoding, outlets, and awakening of the supplied owner.
        return [weakSelf loadHeroNibWithOwner:owner];
    }];
    _container = [[VLCHomeNibLifecycleTestContainerView alloc] init];
    XCTestExpectation * const loaded = [self expectRecentItemCount:0];
    _container.videoGroup = VLCMediaLibraryParentGroupTypeRecentVideos;
    [self waitForExpectations:@[loaded] timeout:5.];
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

- (id)libraryController
{
    return self;
}

- (id)libraryModel
{
    return self;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)listOfVideoMedia
{
    return @[];
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

- (XCTestExpectation *)expectRecentItemCount:(NSUInteger)count
{
    return [self expectationForNotification:VLCLibraryMediaCollectionViewDataSourceDisplayedCollectionChangedNotification
                                     object:_container.dataSource
                                    handler:^BOOL(NSNotification *notification) {
        VLCLibraryHomeViewMediaContainerViewDataSource * const source = notification.object;
        return [source collectionView:source.collectionView numberOfItemsInSection:0] == (NSInteger)count;
    }];
}

- (void)setRecentItemCount:(NSUInteger)count
{
    NSMutableArray * const items = [NSMutableArray arrayWithCapacity:count];
    for (NSUInteger index = 0; index < count; ++index) {
        VLCHomeLifecycleTestMediaItem * const item = [[VLCHomeLifecycleTestMediaItem alloc] init];
        item.libraryID = index + 1;
        [items addObject:item];
    }
    self.listOfRecentMedia = items;
    XCTestExpectation * const loaded = [self expectRecentItemCount:count];
    [_container reloadData];
    [self waitForExpectations:@[loaded] timeout:5.];
    [self layoutContainer];
    XCTAssertEqual(_container.headerView.itemCount, count);
}

- (void)showHero
{
    // The container only needs the represented item's presence. The Hero stub
    // retains it through the real property setter and emits ordinary KVO.
    _container.heroView.representedItem = (id)[[NSObject alloc] init];
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
    XCTAssertEqualWithAccuracy(NSMinX(_container.heroView.frame), VLCUIUnits.mediumSpacing, .01);
    XCTAssertEqualWithAccuracy(NSMaxX(_container.heroView.frame), NSWidth(header.bounds), .01);
    XCTAssertEqualWithAccuracy(NSHeight(_container.heroView.frame), NSHeight(_container.scrollView.contentView.bounds), .01);
    XCTAssertFalse(_container.hidden);
}

- (void)testEmptyRecentVideosHideTheSectionAndNewItemsRestoreTwoRows
{
    XCTAssertEqual(_container.numberOfRows, (NSUInteger)2);
    XCTAssertTrue(_container.hidden);
    XCTAssertTrue(NSEqualSizes(_container.collectionViewLayout.headerReferenceSize, NSZeroSize));
    [self setRecentItemCount:3];
    XCTAssertFalse(_container.hidden);
    XCTAssertTrue(NSEqualSizes(_container.collectionViewLayout.headerReferenceSize, NSZeroSize));
    XCTAssertEqualWithAccuracy(NSHeight(_container.scrollView.contentView.bounds),
                               _container.itemHeight * 2. + _container.collectionViewLayout.minimumInteritemSpacing, .01);
    [self setRecentItemCount:0];
    XCTAssertTrue(_container.hidden);
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
    [self showHero];
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

- (void)testCarouselIsHiddenWithoutRecentsEvenWhenTheHeroHasAnItem
{
    [self showHero];
    XCTAssertTrue(_container.hidden);
    XCTAssertTrue(NSEqualSizes(_container.collectionViewLayout.headerReferenceSize, NSZeroSize));
    [self setRecentItemCount:3];
    [self assertHeroIsEmbeddedInSectionHeader];
    [self setRecentItemCount:0];
    XCTAssertTrue(_container.hidden);
    XCTAssertTrue(NSEqualSizes(_container.collectionViewLayout.headerReferenceSize, NSZeroSize));
}

- (void)testHeroAttachesToTheCarouselAndPopulatedSectionSurvivesItsRemoval
{
    [self setRecentItemCount:3];
    [self showHero];
    [self assertHeroIsEmbeddedInSectionHeader];
    VLCLibraryHeroView * const hero = _container.heroView;
    _container.heroView.representedItem = nil;
    [self layoutContainer];
    XCTAssertFalse(_container.hidden);
    XCTAssertTrue(NSEqualSizes(_container.collectionViewLayout.headerReferenceSize, NSZeroSize));
    const NSRect first = [_container.collectionViewLayout layoutAttributesForItemAtIndexPath:
        [NSIndexPath indexPathForItem:0 inSection:0]].frame;
    XCTAssertEqualWithAccuracy(NSMinX(first), _container.collectionViewLayout.sectionInset.left, .01);
    [self showHero];
    [self assertHeroIsEmbeddedInSectionHeader];
    XCTAssertEqual(_container.heroView, hero);
}

- (void)testOnlyRecentVideosUseTwoRowsAndAnEmbeddedHero
{
    [self showHero];
    [self setRecentItemCount:3];
    [self assertHeroIsEmbeddedInSectionHeader];
    _container.videoGroup = VLCMediaLibraryParentGroupTypeVideoLibrary;
    [self layoutContainer];
    XCTAssertEqual(_container.numberOfRows, (NSUInteger)1);
    XCTAssertTrue(NSEqualSizes(_container.collectionViewLayout.headerReferenceSize, NSZeroSize));
    XCTAssertEqualWithAccuracy(NSHeight(_container.scrollView.contentView.bounds), _container.itemHeight, .01);
    _container.videoGroup = VLCMediaLibraryParentGroupTypeRecentVideos;
    [self assertHeroIsEmbeddedInSectionHeader];
    XCTAssertEqual(_container.numberOfRows, (NSUInteger)2);
}

- (void)testReloadSelectsHeroAndReconnectRefreshesTheDisplayedItemCount
{
    const NSUInteger initialSelections = _heroSelectionCount;
    [self setRecentItemCount:3];
    XCTAssertEqual(_heroSelectionCount, initialSelections + 1);
    [_container disconnect];
    self.listOfRecentMedia = @[];
    XCTestExpectation * const reconnected = [self expectRecentItemCount:0];
    [_container connect];
    [self waitForExpectations:@[reconnected] timeout:5.];
    XCTAssertEqual(_container.headerView.itemCount, (NSUInteger)0);
    XCTAssertTrue(_container.hidden);
    [self showHero];
    XCTAssertTrue(_container.hidden);
}

@end
