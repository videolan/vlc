/*****************************************************************************
 * VLCLibraryHomeVideoCarouselLayoutTest.m: Two-row Home carousel layout tests
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

#import <objc/runtime.h>

#include <math.h>

#import "library/VLCLibraryCollectionViewDelegate.h"
#import "library/VLCLibraryCollectionViewFlowLayout.h"
#import "library/home-library/VLCLibraryHomeViewBaseCarouselContainerView.h"
#import "views/VLCSubScrollView.h"
#import "views/VLCUIUnits.h"

static NSUserInterfaceItemIdentifier const VLCHomeLayoutTestItemIdentifier = @"VLCHomeLayoutTestItem";
static NSUserInterfaceItemIdentifier const VLCHomeLayoutTestHeaderIdentifier = @"VLCHomeLayoutTestHeader";
static const CGFloat VLCHomeLayoutTestHeroWidth = 656.;

@interface VLCHomeLayoutTestItem : NSCollectionViewItem
@end

@implementation VLCHomeLayoutTestItem

- (void)loadView
{
    self.view = [[NSView alloc] initWithFrame:NSZeroRect];
}

@end

@interface VLCLibraryHomeVideoCarouselLayoutTest : VLCAppKitMockingTestCase<NSCollectionViewDataSource>
{
    VLCLibraryHomeViewBaseCarouselContainerView *_container;
    NSWindow *_window;
    NSUInteger _itemCount;
}
@end

@implementation VLCLibraryHomeVideoCarouselLayoutTest

- (void)setUp
{
    [super setUp];
    _itemCount = 5;
    _container = [[VLCLibraryHomeViewBaseCarouselContainerView alloc] init];
    _container.headerView.title = @"Recent videos";
    _container.collectionViewDelegate.staticItemSize =
        NSMakeSize(VLCUIUnits.carouselViewVideoItemViewWidth, VLCUIUnits.carouselViewItemViewHeight);
    [_container.collectionView registerClass:VLCHomeLayoutTestItem.class
                       forItemWithIdentifier:VLCHomeLayoutTestItemIdentifier];
    [_container.collectionView registerClass:NSView.class
                  forSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                              withIdentifier:VLCHomeLayoutTestHeaderIdentifier];
    _container.collectionView.dataSource = self;

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

    __weak typeof(self) weakSelf = self;
    [self addTeardownBlock:^{
        VLCLibraryHomeVideoCarouselLayoutTest * const test = weakSelf;
        if (test != nil) {
            test->_container.collectionView.dataSource = nil;
            [test->_window close];
            test->_container = nil;
            test->_window = nil;
            [test drainMainQueue];
        }
    }];
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
    return _itemCount;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    return [collectionView makeItemWithIdentifier:VLCHomeLayoutTestItemIdentifier forIndexPath:indexPath];
}

- (NSView *)
                       collectionView:(NSCollectionView *)collectionView
    viewForSupplementaryElementOfKind:(NSCollectionViewSupplementaryElementKind)kind
                          atIndexPath:(NSIndexPath *)indexPath
{
    return [collectionView makeSupplementaryViewOfKind:kind
                                        withIdentifier:VLCHomeLayoutTestHeaderIdentifier
                                          forIndexPath:indexPath];
}

- (void)layoutContainer
{
    [_window.contentView layoutSubtreeIfNeeded];
    [_container.collectionView layoutSubtreeIfNeeded];
    [self drainMainQueue];
    [_window.contentView layoutSubtreeIfNeeded];
    [_container.collectionView layoutSubtreeIfNeeded];
}

- (void)configureTwoRowsWithHero:(BOOL)showHero
{
    _container.numberOfRows = 2;
    const CGFloat height = _container.itemHeight * 2. + _container.collectionViewLayout.minimumInteritemSpacing;
    _container.collectionViewLayout.headerReferenceSize = showHero
        ? NSMakeSize(VLCHomeLayoutTestHeroWidth, height)
        : NSZeroSize;
    [_container.collectionView reloadData];
    [self layoutContainer];
}

- (NSRect)frameForItem:(NSUInteger)item
{
    NSIndexPath * const path = [NSIndexPath indexPathForItem:item inSection:0];
    NSCollectionViewLayoutAttributes * const attributes =
        [_container.collectionViewLayout layoutAttributesForItemAtIndexPath:path];
    XCTAssertNotNil(attributes);
    return attributes.frame;
}

- (void)assertTwoRowsWithHero:(BOOL)showHero
{
    const NSSize itemSize = _container.collectionViewDelegate.staticItemSize;
    const CGFloat interitemSpacing = _container.collectionViewLayout.minimumInteritemSpacing;
    const CGFloat lineSpacing = _container.collectionViewLayout.minimumLineSpacing;
    const CGFloat expectedHeight = itemSize.height * 2. + interitemSpacing;
    XCTAssertEqualWithAccuracy(NSHeight(_container.scrollView.contentView.bounds), expectedHeight, .01);
    XCTAssertEqualWithAccuracy(_container.collectionViewLayout.collectionViewContentSize.height,
                               expectedHeight, .01);

    const NSRect first = [self frameForItem:0];
    const NSRect second = [self frameForItem:1];
    const NSRect third = [self frameForItem:2];
    const NSRect fourth = [self frameForItem:3];
    const NSRect fifth = [self frameForItem:4];
    const CGFloat firstColumnX = (showHero
        ? VLCHomeLayoutTestHeroWidth
        : 0.) + _container.collectionViewLayout.sectionInset.left;
    XCTAssertEqualWithAccuracy(NSMinX(first), firstColumnX, .01);
    XCTAssertEqualWithAccuracy(NSMinX(first), NSMinX(second), .01);
    XCTAssertEqualWithAccuracy(fabs(NSMinY(first) - NSMinY(second)), itemSize.height + interitemSpacing, .01);
    XCTAssertEqualWithAccuracy(NSMinX(third), NSMinX(first) + itemSize.width + lineSpacing, .01);
    XCTAssertEqualWithAccuracy(NSMinX(third), NSMinX(fourth), .01);
    XCTAssertEqualWithAccuracy(NSMinY(third), NSMinY(first), .01);
    XCTAssertEqualWithAccuracy(NSMinY(fourth), NSMinY(second), .01);
    XCTAssertEqualWithAccuracy(NSMinX(fifth), NSMinX(third) + itemSize.width + lineSpacing, .01);

    for (NSUInteger index = 0; index < _itemCount; ++index) {
        const NSRect frame = [self frameForItem:index];
        XCTAssertEqualWithAccuracy(NSWidth(frame), itemSize.width, .01);
        XCTAssertEqualWithAccuracy(NSHeight(frame), itemSize.height, .01);
        XCTAssertGreaterThanOrEqual(NSMinY(frame), 0.);
        XCTAssertLessThanOrEqual(NSMaxY(frame), expectedHeight);
    }

    if (showHero) {
        NSIndexPath * const path = [NSIndexPath indexPathForItem:0 inSection:0];
        NSCollectionViewLayoutAttributes * const hero =
            [_container.collectionViewLayout layoutAttributesForSupplementaryViewOfKind:NSCollectionElementKindSectionHeader
                                                                            atIndexPath:path];
        XCTAssertNotNil(hero);
        XCTAssertEqualWithAccuracy(NSMinX(hero.frame), 0., .01);
        XCTAssertEqualWithAccuracy(NSMinY(hero.frame), 0., .01);
        XCTAssertEqualWithAccuracy(NSWidth(hero.frame), VLCHomeLayoutTestHeroWidth, .01);
        XCTAssertEqualWithAccuracy(NSHeight(hero.frame), expectedHeight, .01);
        XCTAssertFalse(NSIntersectsRect(hero.frame, first));
        XCTAssertFalse(NSIntersectsRect(hero.frame, second));
        XCTAssertNotNil([_container.collectionView supplementaryViewForElementKind:NSCollectionElementKindSectionHeader
                                                                       atIndexPath:path]);
    }
}

- (void)testHeroSpansTwoRowsAndVideoColumnsSurviveWindowResizing
{
    [self configureTwoRowsWithHero:YES];
    [self assertTwoRowsWithHero:YES];
    for (NSNumber * const width in @[@520., @1200., @900.]) {
        [_window setContentSize:NSMakeSize(width.doubleValue, 450.)];
        [self layoutContainer];
        [self assertTwoRowsWithHero:YES];
    }
}

- (void)testRemovingAndRestoringHeroClosesAndRestoresItsLeadingSpace
{
    [self configureTwoRowsWithHero:YES];
    [self assertTwoRowsWithHero:YES];
    [self configureTwoRowsWithHero:NO];
    [self assertTwoRowsWithHero:NO];
    [self configureTwoRowsWithHero:YES];
    [self assertTwoRowsWithHero:YES];
}

- (void)testOtherCarouselsKeepOneRowAndChangingRowCountUpdatesHeight
{
    XCTAssertEqual(_container.numberOfRows, (NSUInteger)1);
    [_container.collectionView reloadData];
    [self layoutContainer];
    XCTAssertEqualWithAccuracy(NSHeight(_container.scrollView.contentView.bounds), _container.itemHeight, .01);
    XCTAssertEqualWithAccuracy(NSMinY([self frameForItem:0]), NSMinY([self frameForItem:1]), .01);
    XCTAssertGreaterThan(NSMinX([self frameForItem:1]), NSMinX([self frameForItem:0]));

    [self configureTwoRowsWithHero:NO];
    [self assertTwoRowsWithHero:NO];
    _container.numberOfRows = 1;
    [self layoutContainer];
    XCTAssertEqualWithAccuracy(NSHeight(_container.scrollView.contentView.bounds), _container.itemHeight, .01);
    XCTAssertEqualWithAccuracy(NSMinY([self frameForItem:0]), NSMinY([self frameForItem:1]), .01);
}

- (void)disableNavigationAnimation
{
    SEL const selector = NSSelectorFromString(@"scrollToHorizontalOffset:animated:duration:");
    IMP const implementation = class_getMethodImplementation(VLCCarouselView.class, selector);
    [self replaceInstanceMethod:selector
                        onClass:VLCCarouselView.class
                      withBlock:^(id receiver, CGFloat offset, BOOL __unused animated, NSTimeInterval __unused duration) {
        // Exercise navigation and clip-view scrolling synchronously.
        ((void (*)(id, SEL, CGFloat, BOOL, NSTimeInterval))implementation)(receiver, selector, offset, NO, 0.);
    }];
}

- (void)testBackwardNavigationFromFirstVideoColumnReturnsToHero
{
    [self configureTwoRowsWithHero:YES];
    [self disableNavigationAnimation];

    [_container.collectionView scrollByOneItemForward:YES];
    XCTAssertEqualWithAccuracy(NSMinX(_container.scrollView.documentVisibleRect), VLCHomeLayoutTestHeroWidth, .01);
    XCTAssertFalse(_container.leftButton.hidden);
    [_container.collectionView scrollByOneItemForward:NO];
    XCTAssertEqualWithAccuracy(NSMinX(_container.scrollView.documentVisibleRect), 0., .01);
    XCTAssertTrue(_container.leftButton.hidden);
}

- (void)testHeroWithoutRecentVideosScrollsToBothEdgesInANarrowWindow
{
    _itemCount = 0;
    [_window setContentSize:NSMakeSize(520., 450.)];
    [self configureTwoRowsWithHero:YES];
    [self disableNavigationAnimation];
    const CGFloat maximumOffset = NSMaxX(_container.collectionView.frame) -
        NSWidth(_container.scrollView.documentVisibleRect);
    XCTAssertGreaterThan(maximumOffset, 0.);
    XCTAssertTrue(_container.leftButton.hidden);
    XCTAssertFalse(_container.rightButton.hidden);

    [_container.collectionView scrollByOneItemForward:YES];
    XCTAssertEqualWithAccuracy(NSMinX(_container.scrollView.documentVisibleRect), maximumOffset, .01);
    XCTAssertFalse(_container.leftButton.hidden);
    XCTAssertTrue(_container.rightButton.hidden);
    [_container.collectionView scrollByOneItemForward:NO];
    XCTAssertEqualWithAccuracy(NSMinX(_container.scrollView.documentVisibleRect), 0., .01);
    XCTAssertTrue(_container.leftButton.hidden);
    XCTAssertFalse(_container.rightButton.hidden);
}

@end
