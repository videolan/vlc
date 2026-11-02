/*****************************************************************************
 * VLCLibraryHistoryDataSourceTest.m: history view data source tests
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
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryWindowPersistentPreferences.h"
#import "library/history-library/VLCLibraryHistoryDataSource.h"

// The shared Home fixtures provide the other collection view identifiers.
NSString *VLCLibrarySupplementaryElementViewIdentifier = @"VLCLibrarySupplementaryElementViewIdentifier";

@interface VLCLibraryHistoryDataSourceTestMedia : VLCMediaLibraryMediaItem
@property time_t playedDate;
@property int64_t identifier;
@property vlc_ml_media_type_t type;
@end
@implementation VLCLibraryHistoryDataSourceTestMedia
- (time_t)lastPlayedDate { return self.playedDate; }
- (int64_t)libraryID { return self.identifier; }
- (vlc_ml_media_type_t)mediaType { return self.type; }
@end

@interface VLCLibraryHistoryDataSourceTestModel : NSObject
@property (copy) NSArray<VLCMediaLibraryMediaItem *> *listOfHistoryMedia;
@end
@implementation VLCLibraryHistoryDataSourceTestModel
@end

// Keep selection and reload behavior deterministic without loading media nibs.
@interface VLCLibraryHistoryDataSourceTestCollection : NSObject
@property (weak) id dataSource;
@property (copy) NSSet<NSIndexPath *> *selectionIndexPaths;
@property NSUInteger reloadCount;
@end
@implementation VLCLibraryHistoryDataSourceTestCollection
- (id)collectionViewLayout { return nil; }
- (void)reloadData { self.reloadCount++; }
- (void)deselectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)paths
{
    NSMutableSet * const selected = self.selectionIndexPaths.mutableCopy;
    [selected minusSet:paths];
    self.selectionIndexPaths = selected.copy;
}
- (void)selectItemsAtIndexPaths:(NSSet<NSIndexPath *> *)paths
                 scrollPosition:(NSCollectionViewScrollPosition __unused)position
{
    self.selectionIndexPaths = paths.copy;
}
@end

@interface VLCLibraryHistoryDataSourceTestTable : NSObject
@property (weak) id dataSource;
@property (copy) NSIndexSet *selectedRowIndexes;
@property NSInteger clickedRow;
@property NSUInteger reloadCount;
@end
@implementation VLCLibraryHistoryDataSourceTestTable
- (void)reloadData { self.reloadCount++; }
- (void)selectRowIndexes:(NSIndexSet *)indexes byExtendingSelection:(BOOL)extend
{
    NSMutableIndexSet * const selected = extend ? self.selectedRowIndexes.mutableCopy : NSMutableIndexSet.indexSet;
    [selected addIndexes:indexes];
    self.selectedRowIndexes = selected.copy;
}
@end

@interface VLCLibraryHistoryDataSourceTest : VLCAppKitMockingTestCase
@property VLCLibraryHistoryDataSource *source;
@property VLCLibraryHistoryDataSourceTestModel *model;
@property VLCLibraryHistoryDataSourceTestCollection *collection;
@property VLCLibraryHistoryDataSourceTestTable *table;
@property NSArray<VLCLibraryHistoryDataSourceTestMedia *> *media;
@property NSDate *today;
@end

@implementation VLCLibraryHistoryDataSourceTest

- (VLCLibraryHistoryDataSourceTestMedia *)mediaWithID:(int64_t)identifier
                                                 type:(vlc_ml_media_type_t)type
                                                 date:(NSDate *)date
{
    VLCLibraryHistoryDataSourceTestMedia * const item = VLCLibraryHistoryDataSourceTestMedia.new;
    item.identifier = identifier;
    item.type = type;
    item.playedDate = date.timeIntervalSince1970;
    return item;
}

- (void)setUp
{
    [super setUp];
    NSCalendar * const calendar = NSCalendar.currentCalendar;
    self.today = [calendar dateBySettingHour:12 minute:0 second:0 ofDate:NSDate.date options:0];
    NSDate * const yesterday = [calendar dateByAddingUnit:NSCalendarUnitDay value:-1 toDate:self.today options:0];
    NSDate * const older = [calendar dateByAddingUnit:NSCalendarUnitDay value:-5 toDate:self.today options:0];
    self.media = @[
        [self mediaWithID:1 type:VLC_ML_MEDIA_TYPE_AUDIO date:self.today],
        [self mediaWithID:2 type:VLC_ML_MEDIA_TYPE_VIDEO date:[self.today dateByAddingTimeInterval:60]],
        [self mediaWithID:3 type:VLC_ML_MEDIA_TYPE_UNKNOWN date:yesterday],
        [self mediaWithID:4 type:VLC_ML_MEDIA_TYPE_AUDIO date:older]
    ];
    self.model = VLCLibraryHistoryDataSourceTestModel.new;
    self.model.listOfHistoryMedia = @[self.media[3], self.media[0], self.media[2], self.media[1]];
    self.collection = VLCLibraryHistoryDataSourceTestCollection.new;
    self.collection.selectionIndexPaths = NSSet.set;
    self.table = VLCLibraryHistoryDataSourceTestTable.new;
    self.table.selectedRowIndexes = NSIndexSet.indexSet;
    self.table.clickedRow = -1;
    self.source = VLCLibraryHistoryDataSource.new;
    self.source.libraryModel = (VLCLibraryModel *)self.model;
    self.source.collectionView = (NSCollectionView *)self.collection;
    self.source.tableView = (NSTableView *)self.table;
    self.collection.dataSource = self.source;
    self.table.dataSource = self.source;
    [self.source connect];
}

- (void)tearDown
{
    [self.source disconnect];
    self.source = nil;
    [super tearDown];
}

- (void)selectTableItem:(VLCMediaLibraryMediaItem *)item
{
    NSInteger row = [self.source rowForLibraryItem:item];
    XCTAssertNotEqual(row, NSNotFound);
    self.table.selectedRowIndexes = [NSIndexSet indexSetWithIndex:row];
}

- (void)selectCollectionItem:(VLCMediaLibraryMediaItem *)item
{
    NSIndexPath * const path = [self.source indexPathForLibraryItem:item];
    XCTAssertNotNil(path);
    self.collection.selectionIndexPaths = [NSSet setWithObject:path];
}

- (void)testHistoryViewModeDefaultsToGrid
{
    VLCLibraryWindowPersistentPreferences * const preferences = VLCLibraryWindowPersistentPreferences.new;
    XCTAssertEqual(preferences.historyLibraryViewMode, VLCLibraryGridViewModeSegment);
}

- (void)testHistoryViewModePersistsIndependentlyOfFavorites
{
    VLCLibraryWindowPersistentPreferences * const preferences = VLCLibraryWindowPersistentPreferences.new;
    preferences.favoritesLibraryViewMode = VLCLibraryGridViewModeSegment;
    preferences.historyLibraryViewMode = VLCLibraryListViewModeSegment;
    VLCLibraryWindowPersistentPreferences * const restored = VLCLibraryWindowPersistentPreferences.new;
    XCTAssertEqual(restored.historyLibraryViewMode, VLCLibraryListViewModeSegment);
    XCTAssertEqual(restored.favoritesLibraryViewMode, VLCLibraryGridViewModeSegment);
    restored.historyLibraryViewMode = VLCLibraryGridViewModeSegment;
    restored.favoritesLibraryViewMode = VLCLibraryListViewModeSegment;
    XCTAssertEqual(preferences.historyLibraryViewMode, VLCLibraryGridViewModeSegment);
    XCTAssertEqual(preferences.favoritesLibraryViewMode, VLCLibraryListViewModeSegment);
}

- (void)testTableRowsShareDateGroupsAndNewestFirstMixedMediaOrdering
{
    XCTAssertEqual([self.source numberOfRowsInTableView:(NSTableView *)self.table], 7);
    XCTAssertEqual(self.source.sections.count, 3U);
    for (NSNumber *row in @[@0, @3, @5]) {
        XCTAssertTrue([self.source isHeaderRow:row.integerValue]);
        XCTAssertNotEqual([self.source titleForRow:row.integerValue].length, 0U);
        XCTAssertNil([self.source libraryItemAtRow:row.integerValue forTableView:(NSTableView *)self.table]);
    }
    NSArray * const expected = @[self.media[1], self.media[0], self.media[2], self.media[3]];
    NSArray * const rows = @[@1, @2, @4, @6];
    [rows enumerateObjectsUsingBlock:^(NSNumber *row, NSUInteger index, BOOL * const __unused stop) {
        XCTAssertFalse([self.source isHeaderRow:row.integerValue]);
        XCTAssertEqual([self.source libraryItemAtRow:row.integerValue forTableView:(NSTableView *)self.table], expected[index]);
        XCTAssertEqual([self.source rowForLibraryItem:expected[index]], row.integerValue);
    }];
    XCTAssertEqual(self.source.currentParentType, VLCMediaLibraryParentGroupTypeUnknown);
}

- (void)testInvalidAndHeaderRowsDoNotProduceDragPayloads
{
    for (NSNumber *row in @[@(-1), @0, @3, @5, @7, @(NSIntegerMax)]) {
        XCTAssertNil([self.source tableView:(NSTableView *)self.table pasteboardWriterForRow:row.integerValue]);
        XCTAssertNil([self.source libraryItemAtRow:row.integerValue forTableView:(NSTableView *)self.table]);
    }
    NSPasteboardItem * const writer = (id)[self.source tableView:(NSTableView *)self.table pasteboardWriterForRow:4];
    XCTAssertNotNil([writer dataForType:VLCMediaLibraryMediaItemUTI]);
    XCTAssertEqual([self.source rowForLibraryItem:[self mediaWithID:99 type:VLC_ML_MEDIA_TYPE_AUDIO date:self.today]], NSNotFound);
}

- (void)testDoubleClickUsesClickedMediaRowAndIgnoresHeadersAndOtherSenders
{
    NSMutableArray<VLCLibraryRepresentedItem *> * const played = NSMutableArray.array;
    [self replaceInstanceMethod:@selector(play)
                        onClass:VLCLibraryRepresentedItem.class
                      withBlock:^(VLCLibraryRepresentedItem *item) { [played addObject:item]; }];
    for (NSNumber *row in @[@(-1), @0, @3, @5, @7]) {
        self.table.clickedRow = row.integerValue;
        [self.source doubleClickAction:self.table];
    }
    self.table.clickedRow = 4;
    [self.source doubleClickAction:NSObject.new];
    XCTAssertEqual(played.count, 0U);
    [self.source doubleClickAction:self.table];
    XCTAssertEqual(played.count, 1U);
    XCTAssertEqual(played.firstObject.item, self.media[2]);
    XCTAssertEqual(played.firstObject.parentType, VLCMediaLibraryParentGroupTypeUnknown);
}

- (void)testModeSwitchTransfersOnlyTheActiveViewSelection
{
    [self selectCollectionItem:self.media[1]];
    self.source.tableViewActive = YES;
    XCTAssertEqualObjects(self.table.selectedRowIndexes, [NSIndexSet indexSetWithIndex:1]);
    [self selectTableItem:self.media[2]];
    self.source.tableViewActive = NO;
    XCTAssertEqualObjects(self.collection.selectionIndexPaths,
                          [NSSet setWithObject:[self.source indexPathForLibraryItem:self.media[2]]]);
    self.collection.selectionIndexPaths = NSSet.set;
    self.source.tableViewActive = YES;
    XCTAssertEqual(self.table.selectedRowIndexes.count, 0U);
}

- (void)testTableSelectionFollowsMediaIDAfterReplayChangesDateAndRow
{
    self.source.tableViewActive = YES;
    [self selectTableItem:self.media[2]];
    VLCLibraryHistoryDataSourceTestMedia * const replayed =
        [self mediaWithID:3 type:VLC_ML_MEDIA_TYPE_UNKNOWN date:[self.today dateByAddingTimeInterval:120]];
    self.model.listOfHistoryMedia = @[self.media[0], self.media[1], replayed, self.media[3]];
    [NSNotificationCenter.defaultCenter postNotificationName:VLCLibraryModelHistoryMediaListReset object:self.model];
    XCTAssertEqual(self.source.sections.count, 2U);
    XCTAssertEqual([self.source rowForLibraryItem:replayed], 1);
    XCTAssertEqualObjects(self.table.selectedRowIndexes, [NSIndexSet indexSetWithIndex:1]);
    XCTAssertEqualObjects(self.collection.selectionIndexPaths,
                          [NSSet setWithObject:[NSIndexPath indexPathForItem:0 inSection:0]]);
    self.source.tableViewActive = NO;
    XCTAssertEqualObjects(self.collection.selectionIndexPaths,
                          [NSSet setWithObject:[NSIndexPath indexPathForItem:0 inSection:0]]);
}

- (void)testReloadDoesNotRestoreStaleHiddenTableSelection
{
    self.source.tableViewActive = YES;
    [self selectTableItem:self.media[2]];
    self.source.tableViewActive = NO;
    [self selectCollectionItem:self.media[0]];
    [self.source reloadData];
    XCTAssertEqualObjects(self.table.selectedRowIndexes,
                          [NSIndexSet indexSetWithIndex:[self.source rowForLibraryItem:self.media[0]]]);
    XCTAssertEqualObjects(self.collection.selectionIndexPaths,
                          [NSSet setWithObject:[self.source indexPathForLibraryItem:self.media[0]]]);
}

- (void)testRemovingSelectedMediaClearsBothSelectionsAndEmptyHistoryRemovesHeaders
{
    self.source.tableViewActive = YES;
    [self selectTableItem:self.media[0]];
    self.model.listOfHistoryMedia = @[self.media[1]];
    [self.source reloadData];
    XCTAssertEqual(self.table.selectedRowIndexes.count, 0U);
    XCTAssertEqual(self.collection.selectionIndexPaths.count, 0U);
    XCTAssertEqual([self.source numberOfRowsInTableView:(NSTableView *)self.table], 2);
    self.model.listOfHistoryMedia = @[];
    [self.source reloadData];
    XCTAssertEqual([self.source numberOfRowsInTableView:(NSTableView *)self.table], 0);
    XCTAssertEqual(self.source.sections.count, 0U);
}

@end
