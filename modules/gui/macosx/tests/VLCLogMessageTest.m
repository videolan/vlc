/****************************************************************************
 * VLCLogMessageTest.m: VLC log message native tests
 ****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * Authors: Claudio Cambra <developer@claudiocambra.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any later
 * version.
 *****************************************************************************/

#import <XCTest/XCTest.h>

#import "windows/logging/VLCLogMessage.h"
#import <vlc_messages.h>

@interface VLCLogMessageTest : XCTestCase
@end

@implementation VLCLogMessageTest

- (vlc_log_t)logInfo
{
    vlc_log_t info = { 0 };
    info.psz_module = "core";
    info.file = "input.c";
    info.line = 42;
    info.func = "OpenInput";
    return info;
}

- (void)testMessageProperties
{
    vlc_log_t info = [self logInfo];
    char message[] = "Could not open media";

    VLCLogMessage * const logMessage =
        [[VLCLogMessage alloc] initWithMessage:message
                                           type:VLC_MSG_ERR
                                           info:&info];

    XCTAssertNotNil(logMessage);
    XCTAssertEqual(logMessage.type, VLC_MSG_ERR);
    XCTAssertEqualObjects(logMessage.message, @"Could not open media");
    XCTAssertEqualObjects(logMessage.component, @"core");
    XCTAssertEqualObjects(logMessage.function, @"OpenInput");
    XCTAssertEqualObjects(logMessage.location, @"input.c:42");
    XCTAssertEqualObjects(logMessage.typeName, @"error");
    XCTAssertEqualObjects(logMessage.fullMessage, @"core error: Could not open media");
}

- (void)testFactoryMethod
{
    vlc_log_t info = [self logInfo];
    char message[] = "Started";

    VLCLogMessage * const logMessage = [VLCLogMessage logMessage:message
                                                              type:VLC_MSG_INFO
                                                              info:&info];

    XCTAssertNotNil(logMessage);
    XCTAssertEqualObjects(logMessage.fullMessage, @"core info: Started");
}

- (void)testTypeNames
{
    const struct {
        int type;
        NSString *name;
    } cases[] = {
        { VLC_MSG_INFO, @"info" },
        { VLC_MSG_ERR, @"error" },
        { VLC_MSG_WARN, @"warning" },
        { VLC_MSG_DBG, @"debug" },
        { 99, @"unknown" },
    };

    for (NSUInteger index = 0; index < sizeof(cases) / sizeof(cases[0]); index++) {
        vlc_log_t info = [self logInfo];
        char message[] = "Message";
        VLCLogMessage * const logMessage =
            [[VLCLogMessage alloc] initWithMessage:message
                                               type:cases[index].type
                                               info:&info];

        XCTAssertEqualObjects(logMessage.typeName, cases[index].name);
    }
}

- (void)testEmptyMessage
{
    vlc_log_t info = [self logInfo];
    char message[] = "";

    VLCLogMessage * const logMessage =
        [[VLCLogMessage alloc] initWithMessage:message
                                           type:VLC_MSG_INFO
                                           info:&info];

    XCTAssertNotNil(logMessage);
    XCTAssertEqualObjects(logMessage.message, @"");
    XCTAssertEqualObjects(logMessage.fullMessage, @"core info: ");
}

- (void)testRejectsMissingMessageOrLogInfo
{
    vlc_log_t info = [self logInfo];

    XCTAssertNil([[VLCLogMessage alloc] initWithMessage:NULL
                                                   type:VLC_MSG_INFO
                                                   info:&info]);
    XCTAssertNil([[VLCLogMessage alloc] initWithMessage:(char *)"Message"
                                                   type:VLC_MSG_INFO
                                                   info:NULL]);

    info.psz_module = NULL;
    XCTAssertNil([[VLCLogMessage alloc] initWithMessage:(char *)"Message"
                                                   type:VLC_MSG_INFO
                                                   info:&info]);
}

@end
