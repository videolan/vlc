/*****************************************************************************
 * VLCLibraryHomeBrowseCarouselTest.m: Home Browse carousel native tests
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
#include <Foundation/Foundation.h>

#import <objc/message.h>
#import <objc/runtime.h>

#import "library/VLCLibrarySegment.h"
#import "library/home-library/VLCLibraryHomeBrowseCarouselView.h"
#import "library/home-library/VLCLibraryHomeViewBaseCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewBrowseCarouselContainerView.h"
#import "library/home-library/VLCLibraryHomeViewBrowseCarouselItemView.h"
#import "main/VLCMain.h"

@interface VLCLibraryHomeBrowseCarouselTest : VLCAppKitMockingTestCase
{
    VLCLibraryHomeViewBrowseCarouselContainerView *_container;
    NSWindow *_window;
    NSMutableArray<NSString *> *_openedMRLs;
    NSUInteger _reloadCount;
    NSUInteger _bookmarkNotificationCount;
    NSUInteger _dataSourceReloadCount;
    NSUInteger _browseSectionOpenCount;
    id _browseSectionSender;
    BOOL _allReloadsOnMainThread;
    XCTestExpectation *_reloadExpectation;
}
@end

@implementation VLCLibraryHomeBrowseCarouselTest

- (void)setUp
{
    [super setUp];
    _openedMRLs = NSMutableArray.array;
    _allReloadsOnMainThread = YES;
    __weak typeof(self) weakSelf = self;
    [self addTeardownBlock:^{
        VLCLibraryHomeBrowseCarouselTest * const test = weakSelf;
        if (test == nil) {
            return;
        }
        [test->_container disconnect];
        [test->_window close];
        test->_container = nil;
        test->_window = nil;
        test->_reloadExpectation = nil;
        [test drainMainQueue];
    }];
}

- (id)libraryWindow
{
    return self;
}

- (void)browseFolderByMrl:(NSString *)mrl
{
    [_openedMRLs addObject:mrl];
}

- (void)goToBrowseSection:(id)sender
{
    ++_browseSectionOpenCount;
    _browseSectionSender = sender;
}

- (void)reloadData
{
    ++_dataSourceReloadCount;
}

- (void)postNotification:(NSString *)name
{
    [NSNotificationCenter.defaultCenter postNotificationName:name object:self.defaults];
    [self drainMainQueue];
}

- (void)createContainerWithLocations:(NSArray<NSString *> *)locations
{
    if (locations == nil) {
        [self.defaults removeObjectForKey:VLCLibraryBookmarkedLocationsKey];
    } else {
        [self.defaults setObject:locations forKey:VLCLibraryBookmarkedLocationsKey];
    }
    _container = [[VLCLibraryHomeViewBrowseCarouselContainerView alloc] init];
}

- (void)layoutContainer
{
    if (_window == nil) {
        _window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0., 0., 900., 200.)
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

- (void)assertLocations:(NSArray<NSString *> *)locations
{
    [self layoutContainer];
    NSCollectionView * const collection = _container.collectionView;
    XCTAssertEqual([collection numberOfItemsInSection:0], (NSInteger)locations.count);
    XCTAssertEqual(_container.headerView.itemCount, locations.count);
    for (NSUInteger index = 0; index < locations.count; ++index) {
        NSIndexPath * const path = [NSIndexPath indexPathForItem:index inSection:0];
        NSCollectionViewItem * const item = [collection itemAtIndexPath:path];
        XCTAssertNotNil(item, @"Bookmark %lu should have a visible item", (unsigned long)index);
        XCTAssertEqualObjects(item.representedObject, locations[index]);
        VLCLibraryHomeViewBrowseCarouselItemView * const card = (id)item.view;
        XCTAssertEqualObjects(card.mrl, locations[index]);
    }
}

- (void)observeReloads
{
    Class containerClass = VLCLibraryHomeViewBrowseCarouselContainerView.class;
    IMP const original = class_getMethodImplementation(containerClass, @selector(reloadData));
    __weak typeof(self) weakSelf = self;
    [self replaceInstanceMethod:@selector(reloadData)
                        onClass:containerClass
                      withBlock:^(id receiver) {
        ((void (*)(id, SEL))original)(receiver, @selector(reloadData));
        VLCLibraryHomeBrowseCarouselTest * const test = weakSelf;
        if (test != nil) {
            ++test->_reloadCount;
            test->_allReloadsOnMainThread &= NSThread.isMainThread;
            [test->_reloadExpectation fulfill];
        }
    }];
}

- (void)testAbsentAndEmptyBookmarksHideTheContainerAndNotificationsRestoreOrderedCards
{
    [self createContainerWithLocations:nil];
    XCTAssertTrue(_container.hidden);
    [self assertLocations:@[]];

    [self.defaults setObject:@[] forKey:VLCLibraryBookmarkedLocationsKey];
    [self postNotification:VLCLibraryBookmarkedLocationsChanged];
    XCTAssertTrue(_container.hidden);

    NSArray<NSString *> * const locations = @[
        @"file:///tmp/VLC%20Home%20Browse/",
        @"smb://nas.example/Media%20Library/",
        @"https://cloud.example/OneDrive/",
    ];
    [self.defaults setObject:locations forKey:VLCLibraryBookmarkedLocationsKey];
    [self postNotification:VLCLibraryBookmarkedLocationsChanged];
    XCTAssertFalse(_container.hidden);
    [self assertLocations:locations];

    [self.defaults setObject:@[] forKey:VLCLibraryBookmarkedLocationsKey];
    [self postNotification:VLCLibraryBookmarkedLocationsChanged];
    XCTAssertTrue(_container.hidden);
    [self assertLocations:@[]];
}

- (void)testDefaultsNotificationRefreshesRenamedAndReorderedBookmarksOnly
{
    NSArray<NSString *> * const locations = @[
        @"smb://nas.example/first/", @"https://cloud.example/second/",
    ];
    [self createContainerWithLocations:locations];
    [self observeReloads];

    NSArray<NSString *> * const renamed = @[
        locations[1], @"smb://nas.example/Renamed%20Folder/",
    ];
    [self.defaults setObject:renamed forKey:VLCLibraryBookmarkedLocationsKey];
    [self postNotification:NSUserDefaultsDidChangeNotification];
    XCTAssertEqual(_reloadCount, (NSUInteger)1);
    [self assertLocations:renamed];

    [self.defaults setObject:@YES forKey:@"VLCBrowseTestUnrelatedPreference"];
    [self postNotification:NSUserDefaultsDidChangeNotification];
    [self postNotification:VLCLibraryBookmarkedLocationsChanged];
    XCTAssertEqual(_reloadCount, (NSUInteger)1);
    [self assertLocations:renamed];
}

- (void)testBackgroundBookmarkNotificationReloadsOnTheMainThread
{
    [self createContainerWithLocations:@[]];
    [self observeReloads];
    NSArray<NSString *> * const locations = @[@"smb://nas.example/Background%20Update/"];
    [self.defaults setObject:locations forKey:VLCLibraryBookmarkedLocationsKey];
    _reloadExpectation =
        [self expectationWithDescription:@"Bookmark update reaches the main thread"];
    NSUserDefaults * const defaults = self.defaults;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryBookmarkedLocationsChanged
                                                          object:defaults];
    });
    [self waitForExpectations:@[_reloadExpectation] timeout:2.];
    _reloadExpectation = nil;
    XCTAssertTrue(_allReloadsOnMainThread);
    XCTAssertEqual(_reloadCount, (NSUInteger)1);
    XCTAssertFalse(_container.hidden);
    [self assertLocations:locations];
}

- (void)testDisconnectIgnoresNotificationsAndReconnectReadsCurrentBookmarks
{
    NSArray<NSString *> * const initial = @[@"smb://nas.example/original/"];
    [self createContainerWithLocations:initial];
    [self observeReloads];
    [_container disconnect];
    [self.defaults setObject:@[] forKey:VLCLibraryBookmarkedLocationsKey];
    [self postNotification:VLCLibraryBookmarkedLocationsChanged];
    [self postNotification:NSUserDefaultsDidChangeNotification];
    XCTAssertEqual(_reloadCount, (NSUInteger)0);
    XCTAssertFalse(_container.hidden);
    [self assertLocations:initial];

    [_container connect];
    XCTAssertEqual(_reloadCount, (NSUInteger)1);
    XCTAssertTrue(_container.hidden);
    [self assertLocations:@[]];

    NSArray<NSString *> * const replacement = @[@"https://cloud.example/replacement/"];
    [self.defaults setObject:replacement forKey:VLCLibraryBookmarkedLocationsKey];
    [self postNotification:NSUserDefaultsDidChangeNotification];
    XCTAssertEqual(_reloadCount, (NSUInteger)2);
    XCTAssertFalse(_container.hidden);
    [self assertLocations:replacement];
}

- (void)testRepeatedConnectDeliversEachBookmarkNotificationOnce
{
    [self createContainerWithLocations:@[]];
    Class containerClass = VLCLibraryHomeViewBrowseCarouselContainerView.class;
    SEL const selector = NSSelectorFromString(@"bookmarksChanged:");
    IMP const original = class_getMethodImplementation(containerClass, selector);
    __weak typeof(self) weakSelf = self;
    [self replaceInstanceMethod:selector
                        onClass:containerClass
                      withBlock:^(id receiver, NSNotification *notification) {
        VLCLibraryHomeBrowseCarouselTest * const test = weakSelf;
        if (test != nil && [notification.name isEqualToString:VLCLibraryBookmarkedLocationsChanged]) {
            ++test->_bookmarkNotificationCount;
        }
        ((void (*)(id, SEL, id))original)(receiver, selector, notification);
    }];
    [_container connect];
    [_container connect];
    [_container connect];

    NSArray<NSString *> * const locations = @[@"smb://nas.example/one-observer/"];
    [self.defaults setObject:locations forKey:VLCLibraryBookmarkedLocationsKey];
    [self postNotification:VLCLibraryBookmarkedLocationsChanged];
    XCTAssertEqual(_bookmarkNotificationCount, (NSUInteger)1);
    [self assertLocations:locations];
}

- (void)testHeaderShowAllOpensBrowse
{
    __weak typeof(self) weakSelf = self;
    [self replaceClassMethod:@selector(sharedInstance)
                     onClass:VLCMain.class
                   withBlock:^id(__unused id receiver) {
        return weakSelf;
    }];
    [self createContainerWithLocations:@[@"smb://nas.example/Media/"]];
    [_container.headerView.libraryButton performClick:nil];
    XCTAssertEqual(_browseSectionOpenCount, (NSUInteger)1);
    XCTAssertEqual(_browseSectionSender, _container.headerView.libraryButton);
}

- (void)testVisibleCardActivationPreservesExactURLAndRejectsInvalidIndices
{
    __weak typeof(self) weakSelf = self;
    [self replaceClassMethod:@selector(sharedInstance)
                     onClass:VLCMain.class
                   withBlock:^id(__unused id receiver) {
        return weakSelf;
    }];
    NSArray<NSString *> * const locations = @[
        @"smb://nas.example/Media%20Library/", @"https://cloud.example/OneDrive/?path=Film%20Night",
    ];
    [self createContainerWithLocations:locations];
    [self assertLocations:locations];
    VLCLibraryHomeBrowseCarouselView * const carousel = (id)_container.collectionView;

    for (NSUInteger index = 0; index < locations.count; ++index) {
        [carousel activateItemAtIndexPath:[NSIndexPath indexPathForItem:index inSection:0]];
        XCTAssertEqual(_openedMRLs.count, index + 1);
        XCTAssertEqualObjects(_openedMRLs.lastObject, locations[index]);
    }
    ((void (*)(id, SEL, id))objc_msgSend)(carousel, @selector(activateItemAtIndexPath:), nil);
    [carousel activateItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];
    [carousel activateItemAtIndexPath:[NSIndexPath indexPathForItem:locations.count inSection:0]];
    XCTAssertEqual(_openedMRLs.count, locations.count);

    NSEvent * const event = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown
                                               location:NSZeroPoint
                                          modifierFlags:0
                                              timestamp:0.
                                           windowNumber:_window.windowNumber
                                                context:nil
                                            eventNumber:0
                                             clickCount:1
                                               pressure:0.];
    NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:0 inSection:0];
    XCTAssertFalse([carousel requestContextMenuForItemAtIndexPath:indexPath event:event]);
    [self.defaults setObject:@[] forKey:VLCLibraryBookmarkedLocationsKey];
    [self postNotification:VLCLibraryBookmarkedLocationsChanged];
    [carousel activateItemAtIndexPath:indexPath];
    XCTAssertEqual(_openedMRLs.count, locations.count);
    XCTAssertTrue(_container.hidden);
}

- (void)testBaseFactoryAndReloadDelegationPreserveSubclassBehavior
{
    Class baseClass = VLCLibraryHomeViewBaseCarouselContainerView.class;
    VLCLibraryHomeViewBaseCarouselContainerView * const base =
        [[VLCLibraryHomeViewBaseCarouselContainerView alloc] init];
    XCTAssertEqual([base.collectionView class], VLCLibraryCarouselView.class);

    [self createContainerWithLocations:@[]];
    XCTAssertEqual([_container.collectionView class], VLCLibraryHomeBrowseCarouselView.class);

    __weak typeof(self) weakSelf = self;
    [self replaceInstanceMethod:@selector(dataSource)
                        onClass:baseClass
                      withBlock:^id(__unused id receiver) {
        return weakSelf;
    }];
    [base reloadData];
    XCTAssertEqual(_dataSourceReloadCount, (NSUInteger)1);

    [self.defaults setObject:@[@"smb://nas.example/subclass-refresh/"]
                      forKey:VLCLibraryBookmarkedLocationsKey];
    [_container reloadData];
    XCTAssertEqual(_dataSourceReloadCount, (NSUInteger)1);
    XCTAssertFalse(_container.hidden);
    [self assertLocations:@[@"smb://nas.example/subclass-refresh/"]];
}

@end
