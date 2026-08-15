/****************************************************************************
 * VLCPositionFormatterTest.m: VLCPositionFormatter native tests
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *****************************************************************************/

#import <XCTest/XCTest.h>

#import "extensions/VLCPositionFormatter.h"

@interface VLCPositionFormatterTest : XCTestCase
@end

@implementation VLCPositionFormatterTest

- (void)testStringForObjectValue
{
    VLCPositionFormatter *formatter = [[VLCPositionFormatter alloc] init];

    XCTAssertEqualObjects([formatter stringForObjectValue:@"01:23"], @"01:23");
    XCTAssertEqualObjects([formatter stringForObjectValue:@(83)], @"83");
    XCTAssertNil([formatter stringForObjectValue:[NSDate date]]);
    XCTAssertNil([formatter stringForObjectValue:nil]);
}

- (void)testGetObjectValue
{
    VLCPositionFormatter *formatter = [[VLCPositionFormatter alloc] init];
    id object = nil;

    XCTAssertTrue([formatter getObjectValue:&object
                                  forString:@"01:23"
                           errorDescription:nil]);
    XCTAssertEqualObjects(object, @"01:23");
}

- (void)testPartialStringValidation
{
    VLCPositionFormatter *formatter = [[VLCPositionFormatter alloc] init];

    XCTAssertTrue([formatter isPartialStringValid:@"0123456789:-"
                                  newEditingString:nil
                                  errorDescription:nil]);
    XCTAssertTrue([formatter isPartialStringValid:@""
                                  newEditingString:nil
                                  errorDescription:nil]);
    XCTAssertTrue([formatter isPartialStringValid:@"01:23"
                                  newEditingString:nil
                                  errorDescription:nil]);
    XCTAssertFalse([formatter isPartialStringValid:@"01.23"
                                   newEditingString:nil
                                   errorDescription:nil]);
    XCTAssertFalse([formatter isPartialStringValid:@"01 23"
                                   newEditingString:nil
                                   errorDescription:nil]);
    XCTAssertFalse([formatter isPartialStringValid:@"01a23"
                                   newEditingString:nil
                                   errorDescription:nil]);
}

- (void)testExpectedExceptionAssertion
{
    XCTAssertThrowsSpecificNamed([NSException raise:NSInvalidArgumentException
                                               format:@"test exception"],
                                 NSException,
                                 NSInvalidArgumentException);
}

@end
