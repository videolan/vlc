/*****************************************************************************
 * VLCLibraryHeroViewTest.m: Hero selection mode and update tests
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

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryHeroView.h"
#import "library/VLCLibraryMenuController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "main/VLCMain.h"
#import "views/VLCCarouselView.h"

#import <objc/runtime.h>

@interface VLCLibraryHeroView (TestAccess)
- (VLCMediaLibraryMediaItem *)randomItem;
@end

@interface VLCHeroTestMediaItem : NSObject
@property int64_t libraryID;
@property float progress;
@property (copy) NSString *displayString;
@property (copy) NSString *primaryDetailString;
@end

@implementation VLCHeroTestMediaItem
@end

@interface VLCHeroInteractionTestWindow : NSWindow
@property (strong) NSMutableArray<NSEvent *> *mouseEvents;
@property (copy) dispatch_block_t beforeNextEvent;
@end

@implementation VLCHeroInteractionTestWindow

- (NSEvent *)nextEventMatchingMask:(NSEventMask)mask
                         untilDate:(NSDate *)expiration
                            inMode:(NSRunLoopMode)mode
                           dequeue:(BOOL)dequeue
{
    dispatch_block_t const beforeNextEvent = self.beforeNextEvent;
    self.beforeNextEvent = nil;
    if (beforeNextEvent != nil) {
        beforeNextEvent();
    }
    NSEvent * const event = self.mouseEvents.firstObject;
    if (event != nil && dequeue) {
        [self.mouseEvents removeObjectAtIndex:0];
    }
    return event;
}

@end

@interface VLCHeroInteractionTestItem : NSCollectionViewItem
@end

@implementation VLCHeroInteractionTestItem

- (void)loadView
{
    self.view = [[NSView alloc] initWithFrame:NSZeroRect];
}

@end

@interface VLCLibraryHeroViewTest : VLCAppKitMockingTestCase<NSCollectionViewDataSource>
@property (strong) VLCLibraryHeroView *hero;
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *listOfVideoMedia;
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *listOfRecentMedia;
@property (strong) VLCHeroInteractionTestWindow *interactionWindow;
@property (strong) VLCCarouselView *interactionCarousel;
@property (strong) NSMutableDictionary<NSNumber *, NSNumber *> *mouseEventDeltas;
@property NSInteger mouseEventNumber;
@property NSUInteger playCount;
@property NSUInteger contextMenuCount;
@property (strong) VLCLibraryRepresentedItem *playedItem;
@end

@implementation VLCLibraryHeroViewTest

- (void)setUp
{
    [super setUp];
    self.listOfVideoMedia = @[];
    self.listOfRecentMedia = @[];
    __weak typeof(self) weakSelf = self;
    [self replaceClassMethod:@selector(sharedInstance)
                     onClass:VLCMain.class
                   withBlock:^id(id __unused receiver) {
        return weakSelf;
    }];
    NSBundle * const bundle = [NSBundle bundleForClass:VLCLibraryHeroViewTest.class];
    NSNib * const nib = [[NSNib alloc] initWithNibNamed:@"VLCLibraryHeroView" bundle:bundle];
    XCTAssertNotNil(nib);
    NSArray *objects;
    XCTAssertTrue([nib instantiateWithOwner:nil topLevelObjects:&objects]);
    for (id object in objects) {
        if ([object isKindOfClass:VLCLibraryHeroView.class]) {
            self.hero = object;
        }
    }
    XCTAssertNotNil(self.hero);
    [self addTeardownBlock:^{
        [weakSelf.interactionWindow close];
        weakSelf.interactionWindow = nil;
        weakSelf.interactionCarousel = nil;
        weakSelf.hero = nil;
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

- (VLCMediaLibraryMediaItem *)itemWithID:(int64_t)libraryID progress:(float)progress
{
    VLCHeroTestMediaItem * const item = [[VLCHeroTestMediaItem alloc] init];
    item.libraryID = libraryID;
    item.progress = progress;
    item.displayString = [NSString stringWithFormat:@"Video %lld", libraryID];
    item.primaryDetailString = @"Video details";
    return (id)item;
}

- (void)postNotification:(NSNotificationName)name item:(VLCMediaLibraryMediaItem *)item
{
    [NSNotificationCenter.defaultCenter postNotificationName:name object:item];
}

- (void)assertHeroItem:(VLCMediaLibraryMediaItem *)item
            parentType:(VLCMediaLibraryParentGroupType)parentType
{
    XCTAssertEqual(self.hero.representedItem.item, item);
    XCTAssertEqual(self.hero.representedItem.parentType, parentType);
    XCTAssertEqualObjects(self.hero.titleTextField.stringValue, item.displayString);
    XCTAssertEqualObjects(self.hero.detailTextField.stringValue, item.primaryDetailString);
    XCTAssertFalse(self.hero.hidden);
    XCTAssertTrue(self.hero.playButton.enabled);
}

- (void)testDefaultModeSelectsFromAllVideosInsteadOfPartiallyPlayedRecents
{
    VLCMediaLibraryMediaItem * const video = [self itemWithID:1 progress:0.];
    self.listOfVideoMedia = @[video];
    self.listOfRecentMedia = @[[self itemWithID:2 progress:.45]];
    XCTAssertEqual(self.hero.selectionMode, VLCLibraryHeroViewSelectionModeRandomItem);
    [self.hero setOptimalRepresentedItem];
    [self assertHeroItem:video parentType:VLCMediaLibraryParentGroupTypeVideoLibrary];
    XCTAssertEqualObjects(self.hero.explanationTextField.stringValue, _NS("From your library"));
    XCTAssertEqualObjects(self.hero.playButton.title, _NS("Play now"));
}

- (void)testMostRecentModeUsesFirstVideoEvenWhenAnOlderVideoIsPartiallyPlayed
{
    VLCMediaLibraryMediaItem * const latest = [self itemWithID:1 progress:1.];
    self.listOfVideoMedia = @[[self itemWithID:3 progress:.25]];
    self.listOfRecentMedia = @[latest, [self itemWithID:2 progress:.45]];
    self.hero.selectionMode = VLCLibraryHeroViewSelectionModeMostRecentVideo;
    [self assertHeroItem:latest parentType:VLCMediaLibraryParentGroupTypeRecentVideos];
    XCTAssertEqualObjects(self.hero.explanationTextField.stringValue, _NS("Last watched"));
    XCTAssertEqualObjects(self.hero.playButton.title, _NS("Play now"));
}

- (void)testMostRecentModeHasNoRandomFallbackAndAppearsWhenRecentsArrive
{
    self.listOfVideoMedia = @[[self itemWithID:1 progress:.25]];
    self.hero.selectionMode = VLCLibraryHeroViewSelectionModeMostRecentVideo;
    XCTAssertNil(self.hero.representedItem);
    XCTAssertTrue(self.hero.hidden);
    XCTAssertFalse(self.hero.playButton.enabled);
    XCTAssertEqualObjects(self.hero.titleTextField.stringValue, @"");
    XCTAssertEqualObjects(self.hero.playButton.title, @"");

    VLCMediaLibraryMediaItem * const recent = [self itemWithID:2 progress:.20];
    self.listOfRecentMedia = @[recent];
    [self postNotification:VLCLibraryModelRecentsMediaListReset item:nil];
    [self assertHeroItem:recent parentType:VLCMediaLibraryParentGroupTypeRecentVideos];
    XCTAssertEqualObjects(self.hero.playButton.title, _NS("Resume playing"));

    self.listOfRecentMedia = @[];
    [self postNotification:VLCLibraryModelRecentsMediaItemDeleted item:recent];
    XCTAssertNil(self.hero.representedItem);
    XCTAssertTrue(self.hero.hidden);
}

- (void)testModeChangesRefreshSelectionAndAssigningTheSameModeKeepsItStable
{
    VLCMediaLibraryMediaItem * const video = [self itemWithID:1 progress:.15];
    VLCMediaLibraryMediaItem * const recent = [self itemWithID:2 progress:.40];
    self.listOfVideoMedia = @[video];
    self.listOfRecentMedia = @[recent];
    [self.hero setOptimalRepresentedItem];
    VLCLibraryRepresentedItem * const initial = self.hero.representedItem;
    self.hero.selectionMode = VLCLibraryHeroViewSelectionModeRandomItem;
    XCTAssertEqual(self.hero.representedItem, initial);

    self.hero.selectionMode = VLCLibraryHeroViewSelectionModeMostRecentVideo;
    [self assertHeroItem:recent parentType:VLCMediaLibraryParentGroupTypeRecentVideos];
    VLCLibraryRepresentedItem * const mostRecent = self.hero.representedItem;
    self.hero.selectionMode = VLCLibraryHeroViewSelectionModeMostRecentVideo;
    XCTAssertEqual(self.hero.representedItem, mostRecent);
    self.hero.selectionMode = VLCLibraryHeroViewSelectionModeRandomItem;
    [self assertHeroItem:video parentType:VLCMediaLibraryParentGroupTypeVideoLibrary];
}

- (void)testRecentUpdatesResetsAndDeletionsFollowTheLatestVideo
{
    VLCMediaLibraryMediaItem * const first = [self itemWithID:1 progress:.15];
    VLCMediaLibraryMediaItem * const second = [self itemWithID:2 progress:.35];
    self.listOfRecentMedia = @[first];
    self.hero.selectionMode = VLCLibraryHeroViewSelectionModeMostRecentVideo;
    self.listOfRecentMedia = @[second, first];
    [self postNotification:VLCLibraryModelRecentsMediaItemUpdated item:second];
    [self assertHeroItem:second parentType:VLCMediaLibraryParentGroupTypeRecentVideos];

    VLCMediaLibraryMediaItem * const newest = [self itemWithID:3 progress:0.];
    self.listOfRecentMedia = @[newest, second, first];
    [self postNotification:VLCLibraryModelVideoMediaItemUpdated item:newest];
    [self assertHeroItem:newest parentType:VLCMediaLibraryParentGroupTypeRecentVideos];
    XCTAssertEqualObjects(self.hero.playButton.title, _NS("Play now"));

    self.listOfRecentMedia = @[second, first];
    [self postNotification:VLCLibraryModelVideoMediaItemDeleted item:newest];
    [self assertHeroItem:second parentType:VLCMediaLibraryParentGroupTypeRecentVideos];
    self.listOfRecentMedia = @[first, second];
    [self postNotification:VLCLibraryModelRecentsMediaListReset item:nil];
    [self assertHeroItem:first parentType:VLCMediaLibraryParentGroupTypeRecentVideos];
}

- (void)testProgressUpdatesRefreshPlayTitleWithoutRerollingRandomSelection
{
    VLCMediaLibraryMediaItem * const video = [self itemWithID:1 progress:.35];
    self.listOfVideoMedia = @[video, [self itemWithID:2 progress:0.]];
    __block NSUInteger randomSelections = 0;
    [self replaceInstanceMethod:@selector(randomItem)
                        onClass:VLCLibraryHeroView.class
                      withBlock:^id(id __unused receiver) {
        ++randomSelections;
        return video;
    }];
    [self.hero setOptimalRepresentedItem];
    for (NSNumber * const progress in @[@0., @1., @.45]) {
        VLCMediaLibraryMediaItem * const updated = [self itemWithID:1 progress:progress.floatValue];
        [self postNotification:VLCLibraryModelVideoMediaItemUpdated item:updated];
        [self assertHeroItem:updated parentType:VLCMediaLibraryParentGroupTypeVideoLibrary];
        NSString * const expectedTitle = progress.floatValue > 0. && progress.floatValue < 1.
            ? _NS("Resume playing")
            : _NS("Play now");
        XCTAssertEqualObjects(self.hero.playButton.title, expectedTitle);
    }
    [self postNotification:VLCLibraryModelRecentsMediaItemUpdated item:[self itemWithID:2 progress:.50]];
    XCTAssertEqual(randomSelections, (NSUInteger)1);
    XCTAssertEqual(self.hero.representedItem.item.libraryID, (int64_t)1);
}

- (void)testRandomRecommendationSurvivesRefreshAndReselectsOnlyWhenItDisappears
{
    VLCMediaLibraryMediaItem * const first = [self itemWithID:1 progress:.35];
    VLCMediaLibraryMediaItem * const second = [self itemWithID:2 progress:0.];
    self.listOfVideoMedia = @[first, second];
    __block NSUInteger randomSelections = 0;
    __weak typeof(self) weakSelf = self;
    [self replaceInstanceMethod:@selector(randomItem)
                        onClass:VLCLibraryHeroView.class
                      withBlock:^id(id __unused receiver) {
        ++randomSelections;
        return weakSelf.listOfVideoMedia.firstObject;
    }];
    [self.hero setOptimalRepresentedItem];
    VLCMediaLibraryMediaItem * const refreshed = [self itemWithID:1 progress:1.];
    self.listOfVideoMedia = @[refreshed, second];
    [self postNotification:VLCLibraryModelVideoMediaListReset item:nil];
    [self assertHeroItem:refreshed parentType:VLCMediaLibraryParentGroupTypeVideoLibrary];
    [self postNotification:VLCLibraryModelRecentsMediaListReset item:nil];
    XCTAssertEqual(randomSelections, (NSUInteger)1);
    self.listOfVideoMedia = @[second];
    [self postNotification:VLCLibraryModelVideoMediaItemDeleted item:refreshed];
    [self assertHeroItem:second parentType:VLCMediaLibraryParentGroupTypeVideoLibrary];
    XCTAssertEqual(randomSelections, (NSUInteger)2);
}

- (void)testEmptyRandomModeRecoversWhenVideosArrive
{
    [self.hero setOptimalRepresentedItem];
    XCTAssertNil(self.hero.representedItem);
    VLCMediaLibraryMediaItem * const video = [self itemWithID:1 progress:.40];
    self.listOfVideoMedia = @[video];
    [self postNotification:VLCLibraryModelVideoMediaListReset item:nil];
    [self assertHeroItem:video parentType:VLCMediaLibraryParentGroupTypeVideoLibrary];
    self.listOfVideoMedia = nil;
    [self postNotification:VLCLibraryModelAllCachesDropped item:nil];
    XCTAssertNil(self.hero.representedItem);
}

- (void)testRandomSelectionCanReachTheLastVideo
{
    VLCMediaLibraryMediaItem * const first = [self itemWithID:1 progress:0.];
    VLCMediaLibraryMediaItem * const last = [self itemWithID:2 progress:0.];
    self.listOfVideoMedia = @[first, last];
    BOOL selectedLast = NO;
    for (NSUInteger attempt = 0; attempt < 128 && !selectedLast; ++attempt) {
        VLCMediaLibraryMediaItem * const selected = [self.hero randomItem];
        XCTAssertTrue(selected == first || selected == last);
        selectedLast = selected == last;
    }
    XCTAssertTrue(selectedLast, @"The last video must be included in the random selection range");
}

- (void)prepareMouseInteractionInCarousel:(BOOL)embedded
{
    [self.hero removeFromSuperview];
    [self.interactionWindow close];
    self.hero.carouselView = nil;
    self.interactionCarousel = nil;
    self.playCount = 0;
    self.contextMenuCount = 0;
    self.playedItem = nil;
    self.mouseEventNumber = 0;
    self.mouseEventDeltas = NSMutableDictionary.dictionary;
    self.listOfVideoMedia = @[[self itemWithID:1 progress:.25]];
    [self.hero setOptimalRepresentedItem];
    self.interactionWindow = [[VLCHeroInteractionTestWindow alloc] initWithContentRect:NSMakeRect(0., 0., 900., 400.)
                                                                             styleMask:NSWindowStyleMaskBorderless
                                                                               backing:NSBackingStoreBuffered
                                                                                 defer:NO];
    self.interactionWindow.releasedWhenClosed = NO;
    self.interactionWindow.mouseEvents = NSMutableArray.array;
    NSView *parent = self.interactionWindow.contentView;
    if (embedded) {
        NSScrollView * const scrollView = [[NSScrollView alloc] initWithFrame:parent.bounds];
        scrollView.automaticallyAdjustsContentInsets = NO;
        self.interactionCarousel = [[VLCCarouselView alloc] initWithFrame:NSMakeRect(0., 0., 2200., 400.)];
        NSCollectionViewFlowLayout * const layout = [[NSCollectionViewFlowLayout alloc] init];
        layout.scrollDirection = NSCollectionViewScrollDirectionHorizontal;
        layout.itemSize = NSMakeSize(2200., 368.);
        self.interactionCarousel.collectionViewLayout = layout;
        [self.interactionCarousel registerClass:VLCHeroInteractionTestItem.class
                          forItemWithIdentifier:@"VLCHeroInteractionTestItem"];
        self.interactionCarousel.dataSource = self;
        scrollView.documentView = self.interactionCarousel;
        [parent addSubview:scrollView];
        [self.interactionCarousel reloadData];
        parent = self.interactionCarousel;
    }
    self.hero.carouselView = self.interactionCarousel;
    self.hero.translatesAutoresizingMaskIntoConstraints = YES;
    self.hero.frame = NSMakeRect(0., 0., 600., 368.);
    [parent addSubview:self.hero];
    [self.hero layoutSubtreeIfNeeded];

    __weak typeof(self) weakSelf = self;
    [self replaceInstanceMethod:@selector(play)
                        onClass:VLCLibraryRepresentedItem.class
                      withBlock:^(VLCLibraryRepresentedItem *item) {
        ++weakSelf.playCount;
        weakSelf.playedItem = item;
    }];
    [self replaceInstanceMethod:@selector(popupMenuWithEvent:forView:)
                        onClass:VLCLibraryMenuController.class
                      withBlock:^(id __unused receiver, NSEvent * __unused event, NSView * __unused view) {
        ++weakSelf.contextMenuCount;
    }];
    NSEvent * const sample = [self mouseEventWithType:NSEventTypeLeftMouseDown
                                                point:NSMakePoint(300., 200.)
                                            modifiers:0
                                               deltaX:0.];
    [self replaceInstanceMethod:@selector(deltaX)
                        onClass:object_getClass(sample)
                      withBlock:^CGFloat(NSEvent *event) {
        return weakSelf.mouseEventDeltas[@(event.eventNumber)].doubleValue;
    }];
}

- (NSInteger)collectionView:(NSCollectionView *)collectionView numberOfItemsInSection:(NSInteger)section
{
    return 1;
}

- (NSCollectionViewItem *)collectionView:(NSCollectionView *)collectionView
     itemForRepresentedObjectAtIndexPath:(NSIndexPath *)indexPath
{
    return [collectionView makeItemWithIdentifier:@"VLCHeroInteractionTestItem" forIndexPath:indexPath];
}

- (NSEvent *)mouseEventWithType:(NSEventType)type
                          point:(NSPoint)point
                      modifiers:(NSEventModifierFlags)modifiers
                         deltaX:(CGFloat)deltaX
{
    const NSInteger eventNumber = ++self.mouseEventNumber;
    self.mouseEventDeltas[@(eventNumber)] = @(deltaX);
    return [NSEvent mouseEventWithType:type
                              location:[self.hero convertPoint:point toView:nil]
                         modifierFlags:modifiers
                             timestamp:eventNumber * .05
                          windowNumber:self.interactionWindow.windowNumber
                               context:nil
                           eventNumber:eventNumber
                            clickCount:1
                              pressure:1.];
}

- (void)sendMouseDown:(NSEvent *)event followedBy:(NSArray<NSEvent *> *)events
{
    [self.interactionWindow.mouseEvents setArray:events];
    NSView * const contentView = self.interactionWindow.contentView;
    NSView * const target = [contentView hitTest:[contentView convertPoint:event.locationInWindow fromView:nil]];
    XCTAssertNotNil(target);
    [target mouseDown:event];
    XCTAssertEqual(self.interactionWindow.mouseEvents.count, (NSUInteger)0);
}

- (void)testClickingHeroArtworkAndLabelsPlaysExactlyTheRepresentedItemOnce
{
    for (NSNumber * const embedded in @[@NO, @YES]) {
        [self prepareMouseInteractionInCarousel:embedded.boolValue];
        NSArray<NSValue *> * const points = @[
            [NSValue valueWithPoint:NSMakePoint(300., 200.)],
            [NSValue valueWithPoint:[self.hero.titleTextField convertPoint:NSMakePoint(5., 5.) toView:self.hero]],
            [NSValue valueWithPoint:[self.hero.detailTextField convertPoint:NSMakePoint(5., 5.) toView:self.hero]],
            [NSValue valueWithPoint:[self.hero.explanationTextField convertPoint:NSMakePoint(5., 5.) toView:self.hero]],
        ];
        for (NSValue * const point in points) {
            const NSUInteger previousCount = self.playCount;
            NSEvent * const down = [self mouseEventWithType:NSEventTypeLeftMouseDown point:point.pointValue modifiers:0 deltaX:0.];
            NSEvent * const up = [self mouseEventWithType:NSEventTypeLeftMouseUp point:point.pointValue modifiers:0 deltaX:0.];
            [self sendMouseDown:down followedBy:@[up]];
            XCTAssertEqual(self.playCount, previousCount + 1);
            XCTAssertEqual(self.playedItem, self.hero.representedItem);
        }
    }
}

- (void)testSmallMouseMovementStillCountsAsAHeroClick
{
    for (NSNumber * const embedded in @[@NO, @YES]) {
        [self prepareMouseInteractionInCarousel:embedded.boolValue];
        NSEvent * const down = [self mouseEventWithType:NSEventTypeLeftMouseDown point:NSMakePoint(300., 200.) modifiers:0 deltaX:0.];
        NSEvent * const drag = [self mouseEventWithType:NSEventTypeLeftMouseDragged point:NSMakePoint(302., 201.) modifiers:0 deltaX:2.];
        NSEvent * const up = [self mouseEventWithType:NSEventTypeLeftMouseUp point:NSMakePoint(302., 201.) modifiers:0 deltaX:0.];
        [self sendMouseDown:down followedBy:@[drag, up]];
        XCTAssertEqual(self.playCount, (NSUInteger)1);
    }
}

- (void)testDraggingHeroNeverPlaysEvenAfterReturningToTheStartingPoint
{
    for (NSNumber * const embedded in @[@NO, @YES]) {
        [self prepareMouseInteractionInCarousel:embedded.boolValue];
        for (NSValue * const displacement in @[[NSValue valueWithPoint:NSMakePoint(-20., 0.)],
                                               [NSValue valueWithPoint:NSMakePoint(0., 20.)],
                                               [NSValue valueWithPoint:NSMakePoint(3., 0.)]]) {
            const NSPoint start = NSMakePoint(300., 200.);
            const NSPoint end = NSMakePoint(start.x + displacement.pointValue.x, start.y + displacement.pointValue.y);
            NSEvent * const down = [self mouseEventWithType:NSEventTypeLeftMouseDown point:start modifiers:0 deltaX:0.];
            NSEvent * const drag = [self mouseEventWithType:NSEventTypeLeftMouseDragged point:end modifiers:0 deltaX:displacement.pointValue.x];
            NSEvent * const returned = [self mouseEventWithType:NSEventTypeLeftMouseDragged point:start modifiers:0 deltaX:-displacement.pointValue.x];
            NSEvent * const up = [self mouseEventWithType:NSEventTypeLeftMouseUp point:start modifiers:0 deltaX:0.];
            [self sendMouseDown:down followedBy:@[drag, returned, up]];
            XCTAssertEqual(self.playCount, (NSUInteger)0);
        }
    }
}

- (void)testEmbeddedHeroDragScrollsTheCarouselAndStartsInertia
{
    [self prepareMouseInteractionInCarousel:YES];
    NSEvent * const down = [self mouseEventWithType:NSEventTypeLeftMouseDown point:NSMakePoint(300., 200.) modifiers:0 deltaX:0.];
    NSEvent * const drag = [self mouseEventWithType:NSEventTypeLeftMouseDragged point:NSMakePoint(280., 200.) modifiers:0 deltaX:-20.];
    NSEvent * const up = [self mouseEventWithType:NSEventTypeLeftMouseUp point:NSMakePoint(280., 200.) modifiers:0 deltaX:0.];
    [self sendMouseDown:down followedBy:@[drag, up]];
    XCTAssertGreaterThan(NSMinX(self.interactionCarousel.enclosingScrollView.documentVisibleRect), 0.);
    NSTimer * const timer = [self.interactionCarousel valueForKey:@"decelerationTimer"];
    XCTAssertTrue(timer.isValid);
    XCTAssertEqual(self.playCount, (NSUInteger)0);
}

- (void)testHeroContextMenuClicksNeverPlay
{
    for (NSNumber * const embedded in @[@NO, @YES]) {
        [self prepareMouseInteractionInCarousel:embedded.boolValue];
        NSEvent * const controlDown = [self mouseEventWithType:NSEventTypeLeftMouseDown point:NSMakePoint(300., 200.) modifiers:NSEventModifierFlagControl deltaX:0.];
        [self sendMouseDown:controlDown followedBy:@[]];
        NSEvent * const rightDown = [self mouseEventWithType:NSEventTypeRightMouseDown point:NSMakePoint(300., 200.) modifiers:0 deltaX:0.];
        [self.hero rightMouseDown:rightDown];
        NSEvent * const down = [self mouseEventWithType:NSEventTypeLeftMouseDown point:NSMakePoint(300., 200.) modifiers:0 deltaX:0.];
        NSEvent * const controlUp = [self mouseEventWithType:NSEventTypeLeftMouseUp point:NSMakePoint(300., 200.) modifiers:NSEventModifierFlagControl deltaX:0.];
        [self sendMouseDown:down followedBy:@[controlUp]];
        XCTAssertEqual(self.contextMenuCount, (NSUInteger)3);
        XCTAssertEqual(self.playCount, (NSUInteger)0);
    }
}

- (void)testCancelledTrackingAndReleaseOutsideHeroDoNotPlay
{
    for (NSNumber * const embedded in @[@NO, @YES]) {
        [self prepareMouseInteractionInCarousel:embedded.boolValue];
        NSEvent * const down = [self mouseEventWithType:NSEventTypeLeftMouseDown point:NSMakePoint(599., 200.) modifiers:0 deltaX:0.];
        NSEvent * const outsideUp = [self mouseEventWithType:NSEventTypeLeftMouseUp point:NSMakePoint(601., 200.) modifiers:0 deltaX:0.];
        [self sendMouseDown:down followedBy:@[outsideUp]];
        [self sendMouseDown:down followedBy:@[]];
        XCTAssertEqual(self.playCount, (NSUInteger)0);
    }
}

- (void)testMetadataRefreshKeepsTheClickTargetButReplacingTheVideoCancelsIt
{
    for (NSNumber * const embedded in @[@NO, @YES]) {
        [self prepareMouseInteractionInCarousel:embedded.boolValue];
        for (NSNumber * const libraryID in @[@1, @2]) {
            __weak typeof(self) weakSelf = self;
            self.interactionWindow.beforeNextEvent = ^{
                weakSelf.hero.representedItem = [[VLCLibraryRepresentedItem alloc] initWithItem:[weakSelf itemWithID:libraryID.longLongValue progress:.5]
                                                                                     parentType:VLCMediaLibraryParentGroupTypeVideoLibrary];
            };
            NSEvent * const down = [self mouseEventWithType:NSEventTypeLeftMouseDown point:NSMakePoint(300., 200.) modifiers:0 deltaX:0.];
            NSEvent * const up = [self mouseEventWithType:NSEventTypeLeftMouseUp point:NSMakePoint(300., 200.) modifiers:0 deltaX:0.];
            [self sendMouseDown:down followedBy:@[up]];
            XCTAssertEqual(self.playCount, (NSUInteger)1);
            if (libraryID.longLongValue == 1) {
                XCTAssertEqual(self.playedItem, self.hero.representedItem);
            }
        }
    }
}

- (void)testHeroPlayButtonKeepsItsOwnHitTargetAndPlaysOnce
{
    for (NSNumber * const embedded in @[@NO, @YES]) {
        [self prepareMouseInteractionInCarousel:embedded.boolValue];
        const NSPoint buttonPoint = [self.hero.playButton convertPoint:NSMakePoint(5., 5.) toView:self.hero.superview];
        NSView * const hit = [self.hero hitTest:buttonPoint];
        XCTAssertTrue(hit == self.hero.playButton || [hit isDescendantOf:self.hero.playButton]);
        [self.hero.playButton performClick:nil];
        XCTAssertEqual(self.playCount, (NSUInteger)1);
        XCTAssertEqual(self.playedItem, self.hero.representedItem);
    }
}

- (void)testHeroDoesNotKeepItsCarouselAlive
{
    __weak VLCCarouselView *weakCarousel;
    @autoreleasepool {
        VLCCarouselView * const carousel = [[VLCCarouselView alloc] initWithFrame:NSZeroRect];
        self.hero.carouselView = carousel;
        weakCarousel = carousel;
        XCTAssertEqual(self.hero.carouselView, carousel);
    }
    XCTAssertNil(weakCarousel);
    XCTAssertNil(self.hero.carouselView);
}

- (void)testOrdinaryCarouselCardsStillActivateOnClickAndIgnoreDragging
{
    [self prepareMouseInteractionInCarousel:YES];
    NSIndexPath * const indexPath = [NSIndexPath indexPathForItem:0 inSection:0];
    [self replaceInstanceMethod:@selector(indexPathForItemAtPoint:)
                        onClass:VLCCarouselView.class
                      withBlock:^NSIndexPath *(id __unused receiver, NSPoint __unused point) {
        return indexPath;
    }];
    __block NSUInteger activations = 0;
    [self replaceInstanceMethod:@selector(activateItemAtIndexPath:)
                        onClass:VLCCarouselView.class
                      withBlock:^(id __unused receiver, NSIndexPath *activatedIndexPath) {
        XCTAssertEqualObjects(activatedIndexPath, indexPath);
        ++activations;
    }];
    for (NSNumber * const drag in @[@NO, @YES]) {
        NSEvent * const down = [self mouseEventWithType:NSEventTypeLeftMouseDown point:NSMakePoint(300., 200.) modifiers:0 deltaX:0.];
        NSMutableArray<NSEvent *> * const events = NSMutableArray.array;
        NSPoint end = NSMakePoint(300., 200.);
        if (drag.boolValue) {
            end.x -= 20.;
            [events addObject:[self mouseEventWithType:NSEventTypeLeftMouseDragged point:end modifiers:0 deltaX:-20.]];
        }
        [events addObject:[self mouseEventWithType:NSEventTypeLeftMouseUp point:end modifiers:0 deltaX:0.]];
        [self.interactionWindow.mouseEvents setArray:events];
        [self.interactionCarousel mouseDown:down];
        XCTAssertEqual(activations, (NSUInteger)1);
        XCTAssertEqual(self.interactionWindow.mouseEvents.count, (NSUInteger)0);
    }
}

@end
