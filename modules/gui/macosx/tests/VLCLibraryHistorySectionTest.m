/*****************************************************************************
 * VLCLibraryHistorySectionTest.m: history date grouping tests
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

#import <XCTest/XCTest.h>

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/history-library/VLCLibraryHistorySection.h"

@interface VLCLibraryHistoryTestMediaItem : VLCMediaLibraryMediaItem
@property time_t playedDate;
@property int64_t identifier;
@end

@implementation VLCLibraryHistoryTestMediaItem
- (time_t)lastPlayedDate { return self.playedDate; }
- (int64_t)libraryID { return self.identifier; }
@end

@interface VLCLibraryHistorySectionTest : XCTestCase
@end

@implementation VLCLibraryHistorySectionTest

- (NSCalendar *)calendarWithTimeZone:(NSString *)name
{
    NSCalendar * const calendar = [[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian];
    calendar.timeZone = [NSTimeZone timeZoneWithName:name];
    return calendar;
}

- (NSDate *)date:(NSString *)isoDate
{
    NSDate * const date = [NSISO8601DateFormatter.new dateFromString:isoDate];
    XCTAssertNotNil(date);
    return date;
}

- (VLCMediaLibraryMediaItem *)itemWithID:(int64_t)identifier date:(NSString *)isoDate
{
    VLCLibraryHistoryTestMediaItem * const item = VLCLibraryHistoryTestMediaItem.new;
    item.identifier = identifier;
    item.playedDate = [self date:isoDate].timeIntervalSince1970;
    return item;
}

- (NSArray<NSNumber *> *)IDsInSection:(VLCLibraryHistorySection *)section
{
    return [section.mediaItems valueForKey:@"libraryID"];
}

- (void)testEmptyAndUnplayedItemsHaveNoSections
{
    NSCalendar * const calendar = [self calendarWithTimeZone:@"Asia/Hong_Kong"];
    XCTAssertEqual([VLCLibraryHistorySection sectionsForMediaItems:@[] calendar:calendar].count, 0U);
    VLCLibraryHistoryTestMediaItem * const unplayed = VLCLibraryHistoryTestMediaItem.new;
    XCTAssertEqual([VLCLibraryHistorySection sectionsForMediaItems:@[unplayed] calendar:calendar].count, 0U);
}

- (void)testGroupsAtLocalMidnightAndOrdersNewestFirst
{
    NSCalendar * const calendar = [self calendarWithTimeZone:@"Asia/Hong_Kong"];
    NSArray * const items = @[
        [self itemWithID:1 date:@"2026-11-02T23:59:59+08:00"],
        [self itemWithID:2 date:@"2026-11-03T00:01:00+08:00"],
        [self itemWithID:3 date:@"2026-11-03T12:00:00+08:00"],
        [self itemWithID:4 date:@"2026-11-01T12:00:00+08:00"],
    ];
    NSArray<VLCLibraryHistorySection *> * const sections =
        [VLCLibraryHistorySection sectionsForMediaItems:items calendar:calendar];
    XCTAssertEqual(sections.count, 3U);
    XCTAssertEqualObjects([self IDsInSection:sections[0]], (@[@3, @2]));
    XCTAssertEqualObjects([self IDsInSection:sections[1]], (@[@1]));
    XCTAssertEqualObjects([self IDsInSection:sections[2]], (@[@4]));
    XCTAssertEqualObjects(sections[0].date, [self date:@"2026-11-03T00:00:00+08:00"]);

    NSDate * const now = [self date:@"2026-11-03T18:00:00+08:00"];
    XCTAssertEqualObjects([sections[0] titleWithCalendar:calendar now:now], _NS("Today"));
    XCTAssertEqualObjects([sections[1] titleWithCalendar:calendar now:now], _NS("Yesterday"));
    NSDateFormatter * const formatter = NSDateFormatter.new;
    formatter.calendar = calendar;
    formatter.timeZone = calendar.timeZone;
    formatter.dateStyle = NSDateFormatterFullStyle;
    formatter.timeStyle = NSDateFormatterNoStyle;
    XCTAssertEqualObjects([sections[2] titleWithCalendar:calendar now:now],
                          [formatter stringFromDate:sections[2].date]);
}

- (void)testTimestampTiesPreserveQueryOrder
{
    NSCalendar * const calendar = [self calendarWithTimeZone:@"UTC"];
    NSArray * const items = @[
        [self itemWithID:3 date:@"2026-11-03T00:00:00Z"],
        [self itemWithID:1 date:@"2026-11-03T00:00:00Z"],
        [self itemWithID:2 date:@"2026-11-03T00:00:00Z"],
    ];
    NSArray * const sections = [VLCLibraryHistorySection sectionsForMediaItems:items calendar:calendar];
    XCTAssertEqualObjects([self IDsInSection:sections.firstObject], (@[@3, @1, @2]));
}

- (void)testTimeZoneChangesRegroupExistingItems
{
    NSArray * const items = @[
        [self itemWithID:1 date:@"2026-11-02T23:30:00Z"],
        [self itemWithID:2 date:@"2026-11-03T00:30:00Z"],
    ];
    NSArray * const utcSections =
        [VLCLibraryHistorySection sectionsForMediaItems:items calendar:[self calendarWithTimeZone:@"UTC"]];
    NSArray * const hongKongSections =
        [VLCLibraryHistorySection sectionsForMediaItems:items calendar:[self calendarWithTimeZone:@"Asia/Hong_Kong"]];
    XCTAssertEqual(utcSections.count, 2U);
    XCTAssertEqual(hongKongSections.count, 1U);
    XCTAssertEqualObjects([self IDsInSection:hongKongSections.firstObject], (@[@2, @1]));
}

- (void)testRepeatedHourAtEndOfDaylightSavingBelongsToOneDay
{
    NSCalendar * const calendar = [self calendarWithTimeZone:@"America/Los_Angeles"];
    NSArray * const items = @[
        [self itemWithID:1 date:@"2026-11-01T01:30:00-07:00"],
        [self itemWithID:2 date:@"2026-11-01T01:30:00-08:00"],
    ];
    NSArray<VLCLibraryHistorySection *> * const sections =
        [VLCLibraryHistorySection sectionsForMediaItems:items calendar:calendar];
    XCTAssertEqual(sections.count, 1U);
    XCTAssertEqualObjects([self IDsInSection:sections.firstObject], (@[@2, @1]));
    XCTAssertEqualObjects(sections.firstObject.date, [self date:@"2026-11-01T00:00:00-07:00"]);
}

- (void)testYesterdayUsesCalendarDaysAcrossDaylightSavingChanges
{
    NSCalendar * const calendar = [self calendarWithTimeZone:@"America/Los_Angeles"];
    for (NSArray<NSString *> * const dates in @[
        @[@"2026-03-08T00:15:00-08:00", @"2026-03-09T00:30:00-07:00"],
        @[@"2026-11-01T00:15:00-07:00", @"2026-11-02T00:30:00-08:00"],
    ]) {
        NSArray<VLCLibraryHistorySection *> * const sections =
            [VLCLibraryHistorySection sectionsForMediaItems:@[[self itemWithID:1 date:dates[0]]]
                                                   calendar:calendar];
        XCTAssertEqualObjects([sections.firstObject titleWithCalendar:calendar now:[self date:dates[1]]],
                              _NS("Yesterday"));
    }
}

- (void)testRelativeHeadersAdvanceAtMidnightWithoutPlaybackChanges
{
    NSCalendar * const calendar = [self calendarWithTimeZone:@"Asia/Hong_Kong"];
    NSArray<VLCLibraryHistorySection *> * const sections =
        [VLCLibraryHistorySection sectionsForMediaItems:@[[self itemWithID:1 date:@"2026-11-03T18:00:00+08:00"]]
                                               calendar:calendar];
    VLCLibraryHistorySection * const section = sections.firstObject;
    XCTAssertEqualObjects([section titleWithCalendar:calendar now:[self date:@"2026-11-03T23:59:59+08:00"]],
                          _NS("Today"));
    XCTAssertEqualObjects([section titleWithCalendar:calendar now:[self date:@"2026-11-04T00:00:00+08:00"]],
                          _NS("Yesterday"));
}

@end
