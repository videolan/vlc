/*****************************************************************************
 * VLCCarouselViewTest.m: tests for inset-aware native carousel navigation
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

#import <Cocoa/Cocoa.h>
#import <XCTest/XCTest.h>

#import "views/VLCCarouselView.h"

@interface VLCCarouselView (Testing)
- (void)scrollToHorizontalOffset:(CGFloat)offset
                        animated:(BOOL)animated
                        duration:(NSTimeInterval)duration;
@end

@interface VLCCarouselViewTestNonanimatedView : VLCCarouselView
@end

@implementation VLCCarouselViewTestNonanimatedView

- (void)scrollToHorizontalOffset:(CGFloat)offset
                        animated:(__unused BOOL)animated
                        duration:(__unused NSTimeInterval)duration
{
    // An unordered test window does not run implicit view animations. Exercise
    // the real navigation and bounds helper synchronously without displaying UI.
    [super scrollToHorizontalOffset:offset animated:NO duration:0.];
}

@end

@interface VLCCarouselViewTestItem : NSCollectionViewItem
@end

@implementation VLCCarouselViewTestItem

- (void)loadView
{
    self.view = [[NSView alloc] initWithFrame:NSMakeRect(0., 0., 180., 180.)];
}

@end

@interface VLCCarouselViewTestDataSource : NSObject<NSCollectionViewDataSource>
@property (nonatomic) NSInteger itemCount;
@end

@implementation VLCCarouselViewTestDataSource

- (NSInteger)collectionView:(__unused NSCollectionView *)collectionView
     numberOfItemsInSection:(__unused NSInteger)section
{
    return self.itemCount;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    return [collectionView makeItemWithIdentifier:@"CarouselTestItem" forIndexPath:indexPath];
}

@end

@interface VLCCarouselViewTest : XCTestCase
@property (nonatomic) NSWindow *window;
@property (nonatomic) NSScrollView *scrollView;
@property (nonatomic) VLCCarouselViewTestNonanimatedView *carouselView;
@property (nonatomic) NSCollectionViewFlowLayout *layout;
@property (nonatomic) VLCCarouselViewTestDataSource *dataSource;
@end

@implementation VLCCarouselViewTest

- (void)setUp
{
    [super setUp];
    [NSApplication sharedApplication];
}

- (void)tearDown
{
    [self.window close];
    self.window = nil;
    self.carouselView = nil;
    self.scrollView = nil;
    self.layout = nil;
    self.dataSource = nil;
    [super tearDown];
}

- (void)configureWithItemCount:(NSInteger)itemCount
                  leadingInset:(CGFloat)leadingInset
                 trailingInset:(CGFloat)trailingInset
{
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0., 0., 800., 180.)
                                              styleMask:NSWindowStyleMaskBorderless
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.releasedWhenClosed = NO;
    self.scrollView = [[NSScrollView alloc] initWithFrame:self.window.contentView.bounds];
    self.scrollView.automaticallyAdjustsContentInsets = NO;
    self.scrollView.contentInsets = NSEdgeInsetsMake(0., leadingInset, 0., trailingInset);
    self.scrollView.hasVerticalScroller = NO;
    self.scrollView.hasHorizontalScroller = NO;
    [self.window.contentView addSubview:self.scrollView];

    self.carouselView = [[VLCCarouselViewTestNonanimatedView alloc] initWithFrame:self.scrollView.bounds];
    self.layout = [[NSCollectionViewFlowLayout alloc] init];
    self.layout.scrollDirection = NSCollectionViewScrollDirectionHorizontal;
    self.layout.itemSize = NSMakeSize(180., 180.);
    self.layout.minimumLineSpacing = 8.;
    self.layout.minimumInteritemSpacing = 8.;
    self.layout.sectionInset = NSEdgeInsetsMake(0., 8., 0., 8.);
    self.carouselView.collectionViewLayout = self.layout;
    self.carouselView.leadingItemInset = 8.;
    [self.carouselView registerClass:VLCCarouselViewTestItem.class forItemWithIdentifier:@"CarouselTestItem"];
    self.dataSource = [[VLCCarouselViewTestDataSource alloc] init];
    self.dataSource.itemCount = itemCount;
    self.carouselView.dataSource = self.dataSource;
    self.scrollView.documentView = self.carouselView;
    [self.carouselView reloadData];
    [self.window.contentView layoutSubtreeIfNeeded];
    [self.layout prepareLayout];

    const CGFloat contentWidth = MAX(0., NSWidth(self.scrollView.bounds) - leadingInset - trailingInset);
    self.carouselView.frame = NSMakeRect(0., 0., MAX(contentWidth, self.layout.collectionViewContentSize.width), 180.);
    [self.scrollView.contentView scrollToPoint:NSMakePoint(-leadingInset, 0.)];
    [self.scrollView reflectScrolledClipView:self.scrollView.contentView];
}

- (void)testArrowNavigationKeepsNextCardOutsideLeadingSidebar
{
    [self configureWithItemCount:10 leadingInset:200. trailingInset:0.];
    XCTAssertEqualWithAccuracy(NSMinX(self.scrollView.documentVisibleRect), -200., .01);
    XCTAssertFalse(self.carouselView.canScrollBackward);
    XCTAssertTrue(self.carouselView.canScrollForward);

    [self.carouselView scrollByOneItemForward:YES];
    const CGFloat offset = NSMinX(self.scrollView.documentVisibleRect);
    const NSRect firstFrame = [self.layout layoutAttributesForItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:0]].frame;
    const NSRect secondFrame = [self.layout layoutAttributesForItemAtIndexPath:[NSIndexPath indexPathForItem:1 inSection:0]].frame;
    XCTAssertEqualWithAccuracy(offset, -12., .01);
    XCTAssertEqualWithAccuracy(NSMinX(secondFrame) - offset, 208., .01);
    XCTAssertEqualWithAccuracy(NSMinX(firstFrame) - offset, 20., .01);
    XCTAssertTrue(self.carouselView.canScrollBackward);

    [self.carouselView scrollByOneItemForward:NO];
    XCTAssertEqualWithAccuracy(NSMinX(self.scrollView.documentVisibleRect), -200., .01);
    XCTAssertFalse(self.carouselView.canScrollBackward);
}

- (void)testScrollingHonorsLeadingAndTrailingNativeBounds
{
    [self configureWithItemCount:10 leadingInset:200. trailingInset:120.];
    [self.carouselView scrollToHorizontalOffset:100000. animated:NO duration:0.];
    const CGFloat offset = NSMinX(self.scrollView.documentVisibleRect);
    const NSRect lastFrame = [self.layout layoutAttributesForItemAtIndexPath:[NSIndexPath indexPathForItem:9 inSection:0]].frame;
    XCTAssertEqualWithAccuracy(offset, 1208., .01);
    XCTAssertEqualWithAccuracy(NSMaxX(lastFrame) - offset, 672., .01);
    XCTAssertTrue(self.carouselView.canScrollBackward);
    XCTAssertFalse(self.carouselView.canScrollForward);

    [self.carouselView scrollToHorizontalOffset:-100000. animated:NO duration:0.];
    XCTAssertEqualWithAccuracy(NSMinX(self.scrollView.documentVisibleRect), -200., .01);
    XCTAssertFalse(self.carouselView.canScrollBackward);
    XCTAssertTrue(self.carouselView.canScrollForward);
}

- (void)testArrowNavigationWithoutSidebarInsets
{
    [self configureWithItemCount:10 leadingInset:0. trailingInset:0.];
    [self.carouselView scrollByOneItemForward:YES];
    XCTAssertEqualWithAccuracy(NSMinX(self.scrollView.documentVisibleRect), 188., .01);
    XCTAssertTrue(self.carouselView.canScrollBackward);
    [self.carouselView scrollByOneItemForward:NO];
    XCTAssertEqualWithAccuracy(NSMinX(self.scrollView.documentVisibleRect), 0., .01);
    XCTAssertFalse(self.carouselView.canScrollBackward);
}

- (void)testShortDocumentCannotScroll
{
    [self configureWithItemCount:1 leadingInset:200. trailingInset:120.];
    XCTAssertFalse(self.carouselView.canScrollBackward);
    XCTAssertFalse(self.carouselView.canScrollForward);
    [self.carouselView scrollByOneItemForward:YES];
    XCTAssertEqualWithAccuracy(NSMinX(self.scrollView.documentVisibleRect), -200., .01);
}

- (void)testEmptyDocumentCannotScrollEvenWithStaleFrame
{
    [self configureWithItemCount:0 leadingInset:200. trailingInset:120.];
    self.carouselView.frame = NSMakeRect(0., 0., 1888., 180.);
    XCTAssertFalse(self.carouselView.canScrollBackward);
    XCTAssertFalse(self.carouselView.canScrollForward);
    [self.carouselView scrollByOneItemForward:YES];
    XCTAssertEqualWithAccuracy(NSMinX(self.scrollView.documentVisibleRect), -200., .01);
}

- (void)testDetachedCarouselCannotScroll
{
    VLCCarouselView * const carouselView = [[VLCCarouselView alloc] initWithFrame:NSZeroRect];
    XCTAssertFalse(carouselView.canScrollBackward);
    XCTAssertFalse(carouselView.canScrollForward);
}

@end
