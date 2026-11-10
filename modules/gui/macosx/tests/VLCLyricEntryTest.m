/*****************************************************************************
 * VLCLyricEntryTest.m: synchronized lyrics tests
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Serhii Bykov <esphynox@gmail.com>
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

#import "windows/video/VLCLyricEntry.h"

@interface VLCLyricEntryTest : XCTestCase
@end

@implementation VLCLyricEntryTest

- (void)testParsesEntriesInOrder
{
    NSArray<VLCLyricEntry *> * const entries =
        [VLCLyricEntry entriesFromSYLTData:@"1234\x1F" "First line\x1E" "5678\x1F" "Second line"];
    XCTAssertEqual(entries.count, 2);
    XCTAssertEqual(entries[0].timeMs, 1234);
    XCTAssertEqualObjects(entries[0].text, @"First line");
    XCTAssertEqual(entries[1].timeMs, 5678);
    XCTAssertEqualObjects(entries[1].text, @"Second line");
}

- (void)testEmptyAndMalformedEntries
{
    XCTAssertEqual([VLCLyricEntry entriesFromSYLTData:nil].count, 0);
    XCTAssertEqual([VLCLyricEntry entriesFromSYLTData:@""].count, 0);
    NSArray<VLCLyricEntry *> * const entries =
        [VLCLyricEntry entriesFromSYLTData:@"\x1E" "missing separator\x1E" "bad\x1F" "text\x1F" "ignored\x1E" "9\x1F"];
    XCTAssertEqual(entries.count, 2);
    XCTAssertEqual(entries[0].timeMs, 0);
    XCTAssertEqualObjects(entries[0].text, @"text");
    XCTAssertEqual(entries[1].timeMs, 9);
    XCTAssertEqualObjects(entries[1].text, @"");
}

- (void)testInitWithParts
{
    XCTAssertNil([[VLCLyricEntry alloc] initWithParts:@[]]);
    XCTAssertNil([[VLCLyricEntry alloc] initWithParts:@[@"12"]]);

    VLCLyricEntry * const entry = [[VLCLyricEntry alloc] initWithParts:@[@"123", @"line", @"ignored"]];
    XCTAssertEqual(entry.timeMs, 123);
    XCTAssertEqualObjects(entry.text, @"line");

    VLCLyricEntry * const invalidTime = [[VLCLyricEntry alloc] initWithParts:@[@"bad", @"line"]];
    XCTAssertEqual(invalidTime.timeMs, 0);
}

- (void)testEntryCopiesText
{
    NSMutableString * const text = [NSMutableString stringWithString:@"line"];
    VLCLyricEntry * const entry = [[VLCLyricEntry alloc] initWithTimeMs:42 text:text];
    [text appendString:@" changed"];
    XCTAssertEqual(entry.timeMs, 42);
    XCTAssertEqualObjects(entry.text, @"line");
}

@end
