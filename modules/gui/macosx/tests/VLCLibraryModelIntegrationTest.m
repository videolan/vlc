/****************************************************************************
 * VLCLibraryModelIntegrationTest.m: medialibrary-backed model tests
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

#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryModelChangeDelegate.h"
#import "tests/VLCLibraryDataTypesIntegrationTestSupport.h"

@interface VLCLibraryModel ()
- (void)applicationWillTerminate:(NSNotification *)notification;
@end

@interface VLCLibraryModelIntegrationTest : XCTestCase
@property (nonatomic, strong) VLCLibraryModel *model;
@end

@implementation VLCLibraryModelIntegrationTest

+ (void)setUp
{
    [super setUp];
    XCTAssertTrue(VLCLibraryDataTypesIntegrationStart());
    XCTAssertTrue(VLCLibraryDataTypesIntegrationPrepareFactoryFixtures());
}

+ (void)tearDown
{
    VLCLibraryDataTypesIntegrationStop();
    [super tearDown];
}

- (void)setUp
{
    [super setUp];

    self.model = [[VLCLibraryModel alloc]
                  initWithLibrary:VLCLibraryDataTypesIntegrationMediaLibrary()];
    self.model.changeDelegate.throttleInterval = 0.;
}

- (void)tearDown
{
    [self.model applicationWillTerminate:nil];
    self.model = nil;
    [super tearDown];
}

- (NSArray<VLCMediaLibraryMediaItem *> *)audioMediaAfterFilter:(NSString *)filter
{
    XCTestExpectation * const expectation =
        [self expectationForNotification:VLCLibraryModelAudioMediaListReset
                                  object:self.model
                                 handler:nil];

    self.model.filterString = filter;
    [self.model listOfAudioMedia];
    [self waitForExpectations:@[ expectation ] timeout:5.];
    return self.model.listOfAudioMedia;
}

- (NSArray<VLCMediaLibraryMediaItem *> *)videoMediaAfterFilter:(NSString *)filter
{
    XCTestExpectation * const expectation =
        [self expectationForNotification:VLCLibraryModelVideoMediaListReset
                                  object:self.model
                                 handler:nil];

    self.model.filterString = filter;
    [self.model listOfVideoMedia];
    [self waitForExpectations:@[ expectation ] timeout:5.];
    return self.model.listOfVideoMedia;
}

- (void)testAudioMediaQueryReturnsMatchingFixtureMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self audioMediaAfterFilter:@"Factory Track"];

    XCTAssertEqual(media.count, (NSUInteger)1);
    XCTAssertEqualObjects(media.firstObject.displayString, @"Factory Track");
}

- (void)testAudioMediaQueryRejectsNonMatchingFilter
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self audioMediaAfterFilter:@"No Matching Fixture Media"];

    XCTAssertEqual(media.count, (NSUInteger)0);
}

- (void)testVideoMediaQueryReturnsMatchingFixtureMedia
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self videoMediaAfterFilter:@"Factory Show"];

    XCTAssertEqual(media.count, (NSUInteger)1);
}

- (void)testVideoMediaQueryRejectsNonMatchingFilter
{
    NSArray<VLCMediaLibraryMediaItem *> * const media =
        [self videoMediaAfterFilter:@"No Matching Fixture Media"];

    XCTAssertEqual(media.count, (NSUInteger)0);
}

@end
