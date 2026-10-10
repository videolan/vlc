/****************************************************************************
 * VLCMediaSourceTableViewSortingTest.m: Browse table sorting tests
 ****************************************************************************
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

#import <XCTest/XCTest.h>

#import "library/media-source/VLCMediaSourceTableViewSorting.h"

static NSString * const VLCSortingNameKey = @"name";
static NSString * const VLCSortingCountKey = @"count";
static NSString * const VLCSortingKindKey = @"kind";
static NSString * const VLCSortingTagsKey = @"tags";

@interface VLCSortingItem : NSObject

@property (copy) NSDictionary<NSString *, id> *values;
@property (readonly) NSMutableDictionary<NSString *, NSNumber *> *valueReadCounts;

- (instancetype)initWithValues:(NSDictionary<NSString *, id> *)values;
- (id)sortValueForKey:(NSString *)key;

@end

@implementation VLCSortingItem

- (instancetype)initWithValues:(NSDictionary<NSString *, id> *)values
{
    self = [super init];
    if (self) {
        _values = [values copy];
        _valueReadCounts = NSMutableDictionary.dictionary;
    }
    return self;
}

- (id)sortValueForKey:(NSString *)key
{
    _valueReadCounts[key] = @(_valueReadCounts[key].unsignedIntegerValue + 1);
    return self.values[key];
}

@end

@interface VLCMediaSourceTableViewSortingTest : XCTestCase
@end

@implementation VLCMediaSourceTableViewSortingTest

- (void)testNameSortingUsesNaturalOrderAndReverses
{
    NSArray<NSDictionary *> * const items = @[
        @{VLCSortingNameKey: @"Video 10"},
        @{VLCSortingNameKey: @"video 2"},
        @{VLCSortingNameKey: @"Video 1"}
    ];
    NSSortDescriptor * const descriptor = [NSSortDescriptor sortDescriptorWithKey:VLCSortingNameKey
                                                                        ascending:YES
                                                                         selector:@selector(localizedStandardCompare:)];
    VLCMediaSourceTableViewSorting * const ascending =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:@[descriptor]];
    VLCMediaSourceTableViewSorting * const descending =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:@[descriptor.reversedSortDescriptor]];
    id (^valueForItem)(id, NSString *) = ^id(NSDictionary *item, NSString *key) {
        return item[key];
    };
    XCTAssertEqualObjects([ascending sortedItems:items valueForItem:valueForItem],
                          (@[items[2], items[1], items[0]]));
    XCTAssertEqualObjects([descending sortedItems:items valueForItem:valueForItem],
                          (@[items[0], items[1], items[2]]));
}

- (void)testCountsSortNumericallyIncludingUnknownCounts
{
    NSArray<NSDictionary *> * const items = @[
        @{VLCSortingCountKey: @10},
        @{VLCSortingCountKey: @2},
        @{VLCSortingCountKey: @(-1)},
        @{VLCSortingCountKey: @0}
    ];
    NSSortDescriptor * const descriptor = [NSSortDescriptor sortDescriptorWithKey:VLCSortingCountKey
                                                                        ascending:YES
                                                                         selector:@selector(compare:)];
    VLCMediaSourceTableViewSorting * const ascending =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:@[descriptor]];
    VLCMediaSourceTableViewSorting * const descending =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:@[descriptor.reversedSortDescriptor]];
    id (^valueForItem)(id, NSString *) = ^id(NSDictionary *item, NSString *key) {
        return item[key];
    };
    XCTAssertEqualObjects([ascending sortedItems:items valueForItem:valueForItem],
                          (@[items[2], items[3], items[1], items[0]]));
    XCTAssertEqualObjects([descending sortedItems:items valueForItem:valueForItem],
                          (@[items[0], items[1], items[3], items[2]]));
}

- (void)testSortQueriesIncludeSecondaryDescriptors
{
    NSSortDescriptor * const name = [NSSortDescriptor sortDescriptorWithKey:VLCSortingNameKey ascending:YES];
    NSSortDescriptor * const count = [NSSortDescriptor sortDescriptorWithKey:VLCSortingCountKey ascending:YES];
    VLCMediaSourceTableViewSorting * const namesAndCounts =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:@[name, count]];
    XCTAssertTrue([namesAndCounts sortsByKey:VLCSortingNameKey]);
    XCTAssertTrue([namesAndCounts sortsByKey:VLCSortingCountKey]);
    XCTAssertFalse([namesAndCounts sortsByKey:VLCSortingKindKey]);
    XCTAssertFalse([namesAndCounts sortsByKey:VLCSortingTagsKey]);

    NSSortDescriptor * const kind = [NSSortDescriptor sortDescriptorWithKey:VLCSortingKindKey ascending:YES];
    NSSortDescriptor * const tags = [NSSortDescriptor sortDescriptorWithKey:VLCSortingTagsKey ascending:NO];
    VLCMediaSourceTableViewSorting * const kindsAndTags =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:@[kind, tags]];
    XCTAssertTrue([kindsAndTags sortsByKey:VLCSortingKindKey]);
    XCTAssertTrue([kindsAndTags sortsByKey:VLCSortingTagsKey]);
    XCTAssertFalse([kindsAndTags sortsByKey:VLCSortingNameKey]);
    XCTAssertFalse([kindsAndTags sortsByKey:VLCSortingCountKey]);
}

- (void)testSecondaryDescriptorsAndStableTiesPreserveObjectIdentity
{
    NSArray<VLCSortingItem *> * const items = @[
        [[VLCSortingItem alloc] initWithValues:@{VLCSortingKindKey: @"File", VLCSortingNameKey: @"B"}],
        [[VLCSortingItem alloc] initWithValues:@{VLCSortingKindKey: @"File", VLCSortingNameKey: @"A"}],
        [[VLCSortingItem alloc] initWithValues:@{VLCSortingKindKey: @"File", VLCSortingNameKey: @"A"}]
    ];
    NSSortDescriptor * const kind = [NSSortDescriptor sortDescriptorWithKey:VLCSortingKindKey ascending:YES];
    NSSortDescriptor * const name = [NSSortDescriptor sortDescriptorWithKey:VLCSortingNameKey ascending:YES];
    VLCMediaSourceTableViewSorting * const sorting =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:@[kind, name]];
    NSArray * const sorted = [sorting sortedItems:items
                                     valueForItem:^id(VLCSortingItem *item, NSString *key) {
        return [item sortValueForKey:key];
    }];
    XCTAssertTrue(sorted[0] == items[1]);
    XCTAssertTrue(sorted[1] == items[2]);
    XCTAssertTrue(sorted[2] == items[0]);
}

- (void)testUnsortedItemsDoNotReadValues
{
    NSArray<NSDictionary *> * const items = @[
        @{VLCSortingNameKey: @"B"},
        @{VLCSortingNameKey: @"A"}
    ];
    VLCMediaSourceTableViewSorting * const sorting =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:@[]];
    XCTAssertFalse([sorting sortsByKey:VLCSortingNameKey]);
    XCTAssertFalse([sorting sortsByKey:VLCSortingCountKey]);
    XCTAssertFalse([sorting sortsByKey:VLCSortingKindKey]);
    XCTAssertFalse([sorting sortsByKey:VLCSortingTagsKey]);
    NSArray * const sorted = [sorting sortedItems:items
                                     valueForItem:^id(id __unused item, NSString * __unused key) {
        XCTFail(@"Discovery order should not read sort values");
        return nil;
    }];
    XCTAssertTrue(sorted == items);
    XCTAssertEqualObjects([sorting sortedItems:@[] valueForItem:^id(id __unused item, NSString * __unused key) {
        XCTFail(@"An empty list should not read sort values");
        return nil;
    }], @[]);
}

- (void)testOnlyActiveKeysAreReadOnceForRepeatedObjectsAndDescriptors
{
    VLCSortingItem * const first = [[VLCSortingItem alloc] initWithValues:@{
        VLCSortingNameKey: @"Folder", VLCSortingCountKey: @2,
        VLCSortingKindKey: @"Directory", VLCSortingTagsKey: @"Blue"
    }];
    VLCSortingItem * const second = [[VLCSortingItem alloc] initWithValues:@{
        VLCSortingNameKey: @"Folder", VLCSortingCountKey: @1,
        VLCSortingKindKey: @"Directory", VLCSortingTagsKey: @"Red"
    }];
    NSArray<VLCSortingItem *> * const items = @[first, second, first];
    NSSortDescriptor * const name = [NSSortDescriptor sortDescriptorWithKey:VLCSortingNameKey ascending:YES];
    NSSortDescriptor * const count = [NSSortDescriptor sortDescriptorWithKey:VLCSortingCountKey ascending:YES];
    VLCMediaSourceTableViewSorting * const sorting =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:@[name, count, name]];
    NSArray * const sorted = [sorting sortedItems:items
                                     valueForItem:^id(VLCSortingItem *item, NSString *key) {
        return [item sortValueForKey:key];
    }];
    XCTAssertTrue(sorted[0] == second);
    XCTAssertTrue(sorted[1] == first);
    XCTAssertTrue(sorted[2] == first);
    for (VLCSortingItem * const item in @[first, second]) {
        XCTAssertEqualObjects(item.valueReadCounts[VLCSortingNameKey], @1);
        XCTAssertEqualObjects(item.valueReadCounts[VLCSortingCountKey], @1);
        XCTAssertNil(item.valueReadCounts[VLCSortingKindKey]);
        XCTAssertNil(item.valueReadCounts[VLCSortingTagsKey]);
    }
}

- (void)testSortingAgainReadsUpdatedValues
{
    VLCSortingItem * const first = [[VLCSortingItem alloc] initWithValues:@{VLCSortingCountKey: @(-1)}];
    VLCSortingItem * const second = [[VLCSortingItem alloc] initWithValues:@{VLCSortingCountKey: @2}];
    NSArray<VLCSortingItem *> * const items = @[first, second];
    NSSortDescriptor * const descriptor = [NSSortDescriptor sortDescriptorWithKey:VLCSortingCountKey ascending:YES];
    VLCMediaSourceTableViewSorting * const sorting =
        [[VLCMediaSourceTableViewSorting alloc] initWithSortDescriptors:@[descriptor]];
    id (^valueForItem)(id, NSString *) = ^id(VLCSortingItem *item, NSString *key) {
        return [item sortValueForKey:key];
    };
    XCTAssertTrue([sorting sortedItems:items valueForItem:valueForItem][0] == first);
    first.values = @{VLCSortingCountKey: @10};
    XCTAssertTrue([sorting sortedItems:items valueForItem:valueForItem][0] == second);
    XCTAssertEqualObjects(first.valueReadCounts[VLCSortingCountKey], @2);
    XCTAssertEqualObjects(second.valueReadCounts[VLCSortingCountKey], @2);
}

@end
