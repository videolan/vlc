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
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "main/VLCMain.h"

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

@interface VLCLibraryHeroViewTest : VLCAppKitMockingTestCase
@property (strong) VLCLibraryHeroView *hero;
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *listOfVideoMedia;
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *listOfRecentMedia;
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

@end
