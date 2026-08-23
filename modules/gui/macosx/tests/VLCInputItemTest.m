/****************************************************************************
 * VLCInputItemTest.m: VLC input item native tests
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

#import "library/VLCInputItem.h"
#import "tests/VLCInputItemTestSupport.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"

@interface VLCInputItem (VLCInputItemTestPrivate)
- (void)parsingEnded:(int)status;
- (void)subTreeAdded:(input_item_node_t *)p_node;
@end

@interface VLCInputItemTest : XCTestCase
@end

@implementation VLCInputItemTest

- (input_item_t *)newFileInputItem
{
    return input_item_NewFile("file:///tmp/VLC%20Test.mp4",
                              "Original Name",
                              VLC_TICK_FROM_SEC(123),
                              ITEM_LOCAL);
}

- (void)testInitializationMapsCoreProperties
{
    input_item_t * const coreItem = [self newFileInputItem];
    XCTAssertNotEqual(coreItem, NULL);

    input_item_SetTitle(coreItem, "Metadata Title");
    input_item_SetArtist(coreItem, "Artist");
    input_item_SetAlbum(coreItem, "Album");
    input_item_SetTrackNumber(coreItem, "4");
    input_item_SetGenre(coreItem, "Genre");
    input_item_SetCopyright(coreItem, "Copyright");
    input_item_SetPublisher(coreItem, "Publisher");
    input_item_SetLanguage(coreItem, "en");
    input_item_SetDate(coreItem, "2026");
    input_item_SetDescription(coreItem, "Description");
    input_item_SetEncodedBy(coreItem, "Encoder");
    input_item_SetTrackID(coreItem, "track-id");
    input_item_SetTrackTotal(coreItem, "10");
    input_item_SetDirector(coreItem, "Director");
    input_item_SetSeason(coreItem, "2");
    input_item_SetEpisode(coreItem, "3");
    input_item_SetShowName(coreItem, "Show");
    input_item_SetActors(coreItem, "Actor");
    input_item_SetDiscNumber(coreItem, "1");
    input_item_SetDiscTotal(coreItem, "2");
    input_item_SetMetaExtra(coreItem, "custom", "value");
    input_item_AddOption(coreItem, "network-caching=1000", 0);
    input_item_AddOption(coreItem, "http-referrer=https://example.com", 0);
    input_item_SetArtworkURL(coreItem, "https://example.com/art.jpg");

    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    XCTAssertNotNil(inputItem);
    XCTAssertEqualObjects(inputItem.MRL, @"file:///tmp/VLC%20Test.mp4");
    XCTAssertEqualObjects(inputItem.decodedMRL, @"file:///tmp/VLC Test.mp4");
    XCTAssertEqualObjects(inputItem.path, @"/tmp/VLC Test.mp4");
    XCTAssertEqualObjects(inputItem.name, @"Original Name");
    XCTAssertEqualObjects(inputItem.title, @"Metadata Title");
    XCTAssertEqualObjects(inputItem.artist, @"Artist");
    XCTAssertEqualObjects(inputItem.album, @"Album");
    XCTAssertEqualObjects(inputItem.trackNumber, @"4");
    XCTAssertEqualObjects(inputItem.genre, @"Genre");
    XCTAssertEqualObjects(inputItem.copyright, @"Copyright");
    XCTAssertEqualObjects(inputItem.publisher, @"Publisher");
    XCTAssertEqualObjects(inputItem.language, @"en");
    XCTAssertEqualObjects(inputItem.date, @"2026");
    XCTAssertEqualObjects(inputItem.contentDescription, @"Description");
    XCTAssertEqualObjects(inputItem.encodedBy, @"Encoder");
    XCTAssertEqualObjects(inputItem.trackID, @"track-id");
    XCTAssertEqualObjects(inputItem.trackTotal, @"10");
    XCTAssertEqualObjects(inputItem.director, @"Director");
    XCTAssertEqualObjects(inputItem.season, @"2");
    XCTAssertEqualObjects(inputItem.episode, @"3");
    XCTAssertEqualObjects(inputItem.showName, @"Show");
    XCTAssertEqualObjects(inputItem.actors, @"Actor");
    XCTAssertEqualObjects(inputItem.discNumber, @"1");
    XCTAssertEqualObjects(inputItem.totalNumberOfDiscs, @"2");
    XCTAssertEqual(inputItem.duration, VLC_TICK_FROM_SEC(123));
    XCTAssertEqual(inputItem.inputType, ITEM_TYPE_FILE);
    XCTAssertFalse(inputItem.isStream);
    XCTAssertFalse(inputItem.preparsed);
    XCTAssertEqualObjects(inputItem.options,
                          (@[ @"network-caching=1000", @"http-referrer=https://example.com" ]));
    XCTAssertEqualObjects([inputItem extraMetaForKey:@"custom"], @"value");
    XCTAssertEqualObjects(inputItem.extraMetaNames, (@[ @"custom" ]));
    XCTAssertEqualObjects(inputItem.artworkURL.absoluteString, @"https://example.com/art.jpg");
    XCTAssertNil(inputItem.finderTags);
}

- (void)testMetadataSettersUpdateCoreItem
{
    input_item_t * const coreItem = [self newFileInputItem];
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    inputItem.name = @"New Name";
    inputItem.title = @"New Title";
    inputItem.artist = @"New Artist";
    inputItem.album = @"New Album";
    inputItem.trackNumber = @"8";
    inputItem.genre = @"New Genre";
    inputItem.copyright = @"New Copyright";
    inputItem.publisher = @"New Publisher";
    inputItem.language = @"fr";
    inputItem.date = @"2027";
    inputItem.contentDescription = @"New Description";
    inputItem.director = @"New Director";
    inputItem.showName = @"New Show";
    inputItem.actors = @"New Actor";

    XCTAssertEqualObjects(inputItem.name, @"New Name");
    XCTAssertEqualObjects(inputItem.title, @"New Title");
    XCTAssertEqualObjects(inputItem.artist, @"New Artist");
    XCTAssertEqualObjects(inputItem.album, @"New Album");
    XCTAssertEqualObjects(inputItem.trackNumber, @"8");
    XCTAssertEqualObjects(inputItem.genre, @"New Genre");
    XCTAssertEqualObjects(inputItem.copyright, @"New Copyright");
    XCTAssertEqualObjects(inputItem.publisher, @"New Publisher");
    XCTAssertEqualObjects(inputItem.language, @"fr");
    XCTAssertEqualObjects(inputItem.date, @"2027");
    XCTAssertEqualObjects(inputItem.contentDescription, @"New Description");
    XCTAssertEqualObjects(inputItem.director, @"New Director");
    XCTAssertEqualObjects(inputItem.showName, @"New Show");
    XCTAssertEqualObjects(inputItem.actors, @"New Actor");
}

- (void)testDefaultValuesAndTitleFallback
{
    input_item_t * const coreItem = [self newFileInputItem];
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    XCTAssertEqualObjects(inputItem.title, @"Original Name");
    XCTAssertEqualObjects(inputItem.options, @[]);
    XCTAssertEqualObjects(inputItem.extraMetaNames, @[]);
    XCTAssertEqualObjects([inputItem extraMetaForKey:@"missing"], @"");
    XCTAssertNil(inputItem.finderTags);
}

- (void)testCancelParsingWithoutParser
{
    input_item_t * const coreItem = [self newFileInputItem];
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    XCTAssertNoThrow([inputItem cancelParsing]);
}

- (void)testParsingEndedPostsSuccessAndFailureNotifications
{
    input_item_t * const coreItem = [self newFileInputItem];
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    NSMutableArray<NSString *> * const notificationNames = [NSMutableArray array];
    id const observer = [NSNotificationCenter.defaultCenter
        addObserverForName:nil
                    object:inputItem
                     queue:nil
                usingBlock:^(NSNotification * const notification) {
        [notificationNames addObject:notification.name];
    }];

    [inputItem parsingEnded:1];
    [inputItem parsingEnded:0];

    [NSNotificationCenter.defaultCenter removeObserver:observer];

    XCTAssertEqual(notificationNames.count, (NSUInteger)2);
    XCTAssertTrue([notificationNames[0] isEqualToString:VLCInputItemParsingSucceeded]);
    XCTAssertTrue([notificationNames[1] isEqualToString:VLCInputItemParsingFailed]);
}

- (void)testSubtreeReplacementPostsNotifications
{
    input_item_t * const coreItem = [self newFileInputItem];
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    input_item_node_t * const firstSubtree = input_item_node_Create(inputItem.vlcInputItem);
    input_item_node_t * const secondSubtree = input_item_node_Create(inputItem.vlcInputItem);
    NSMutableArray<NSNotification *> * const notifications = [NSMutableArray array];
    id const observer = [NSNotificationCenter.defaultCenter
        addObserverForName:VLCInputItemSubtreeAdded
                    object:inputItem
                     queue:nil
                usingBlock:^(NSNotification * const notification) {
        [notifications addObject:notification];
    }];

    [inputItem subTreeAdded:firstSubtree];
    XCTAssertEqual(inputItem.subTree, firstSubtree);
    [inputItem subTreeAdded:secondSubtree];
    XCTAssertEqual(inputItem.subTree, secondSubtree);

    [NSNotificationCenter.defaultCenter removeObserver:observer];

    XCTAssertEqual(notifications.count, (NSUInteger)2);
    XCTAssertEqual(notifications[0].object, inputItem);
    XCTAssertEqual(notifications[1].object, inputItem);
}

- (void)testURLFactoryAndRadioCountryCode
{
    NSURL * const url = [NSURL URLWithString:@"file:///tmp/Example%20Video.mkv"];
    VLCInputItem * const inputItem = [VLCInputItem inputItemFromURL:url];

    XCTAssertNotNil(inputItem);
    XCTAssertEqualObjects(inputItem.MRL, @"file:///tmp/Example%20Video.mkv");
    XCTAssertEqualObjects(inputItem.name, @"Example Video");
    XCTAssertNil(inputItem.radioCountryCodeForFlagArtwork);

    VLCInputItem * const radioItem =
        [VLCInputItem inputItemFromURL:[NSURL URLWithString:@"radio://us/station"]];
    XCTAssertEqualObjects(radioItem.radioCountryCodeForFlagArtwork, @"US");

    VLCInputItem * const invalidRadioItem =
        [VLCInputItem inputItemFromURL:[NSURL URLWithString:@"radio://u1/station"]];
    XCTAssertNil(invalidRadioItem.radioCountryCodeForFlagArtwork);
}

- (void)testStreamAndArtworkState
{
    input_item_t * const coreItem = input_item_NewStream("https://example.com/live", 
                                                        "Live stream",
                                                        INPUT_DURATION_INDEFINITE);
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    XCTAssertNotNil(inputItem);
    XCTAssertTrue(inputItem.isStream);
    XCTAssertEqual(inputItem.inputType, ITEM_TYPE_STREAM);
    XCTAssertEqualObjects(inputItem.decodedMRL, @"https://example.com/live");
    XCTAssertEqualObjects(inputItem.path, @"");
    XCTAssertEqualObjects(inputItem.options, @[]);
    XCTAssertNil(inputItem.finderTags);

    inputItem.artworkURL = [NSURL URLWithString:@"https://example.com/live.jpg"];
    XCTAssertEqualObjects(inputItem.artworkURL.absoluteString, @"https://example.com/live.jpg");

    [(id)inputItem setArtworkURL:nil];
    XCTAssertNil(inputItem.artworkURL);
}

- (void)testStreamThumbnailCompletesWithoutImage
{
    input_item_t * const coreItem = input_item_NewStream("https://example.com/live",
                                                        "Live stream",
                                                        INPUT_DURATION_INDEFINITE);
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    XCTestExpectation * const completionExpectation =
        [self expectationWithDescription:@"stream thumbnail completion"];
    [inputItem thumbnailWithSize:NSMakeSize(128, 128)
                completionHandler:^(NSImage *image) {
        XCTAssertNil(image);
        [completionExpectation fulfill];
    }];
    [self waitForExpectations:@[completionExpectation] timeout:1.0];
}

- (void)testLocalThumbnailCompletes
{
    VLCInputItemTestResetAppKitState();

    NSString * const path = [NSTemporaryDirectory()
        stringByAppendingPathComponent:@"VLCInputItemThumbnailTest.txt"];
    NSData * const contents = [@"VLC thumbnail test" dataUsingEncoding:NSUTF8StringEncoding];
    XCTAssertTrue([contents writeToFile:path atomically:YES]);

    NSURL * const url = [NSURL fileURLWithPath:path];
    input_item_t * const coreItem = input_item_NewFile(url.absoluteString.UTF8String,
                                                       "Thumbnail Test",
                                                       INPUT_DURATION_UNSET,
                                                       ITEM_LOCAL);
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    NSImage * const testImage = [[NSImage alloc] initWithSize:NSMakeSize(1, 1)];
    VLCInputItemTestSetWorkspaceImage(testImage);
    XCTestExpectation * const completionExpectation =
        [self expectationWithDescription:@"workspace thumbnail completion"];
    [inputItem thumbnailWithSize:NSMakeSize(128, 128)
                completionHandler:^(NSImage *image) {
        XCTAssertEqualObjects(image, testImage);
        [completionExpectation fulfill];
    }];
    [self waitForExpectations:@[completionExpectation] timeout:2.0];

    VLCInputItemTestSetWorkspaceImage(nil);
    VLCInputItemTestSetQuickLookImage(testImage);
    XCTestExpectation * const quickLookExpectation =
        [self expectationWithDescription:@"Quick Look thumbnail completion"];
    [inputItem thumbnailWithSize:NSMakeSize(128, 128)
                completionHandler:^(NSImage *image) {
        XCTAssertEqualObjects(image, testImage);
        [quickLookExpectation fulfill];
    }];
    [self waitForExpectations:@[quickLookExpectation] timeout:2.0];

    VLCInputItemTestSetQuickLookImage(nil);
    XCTestExpectation * const emptyExpectation =
        [self expectationWithDescription:@"empty thumbnail completion"];
    [inputItem thumbnailWithSize:NSMakeSize(128, 128)
                completionHandler:^(NSImage *image) {
        XCTAssertNil(image);
        [emptyExpectation fulfill];
    }];
    [self waitForExpectations:@[emptyExpectation] timeout:2.0];

    [NSFileManager.defaultManager removeItemAtPath:path error:nil];
    VLCInputItemTestResetAppKitState();
}

- (void)testLocalFileOperationsUseTheirApplicationServices
{
    VLCInputItemTestResetAppKitState();
    VLCInputItem * const inputItem =
        [VLCInputItem inputItemFromURL:[NSURL fileURLWithPath:@"/tmp/VLCInputItemTest.mkv"]];

    XCTAssertNotNil(inputItem);
    [inputItem moveToTrash];
    XCTAssertTrue(VLCInputItemTestDidReload());

    [inputItem revealInFinder];
    XCTAssertTrue(VLCInputItemTestDidReveal());

    VLCInputItemTestResetAppKitState();
}

- (void)testMissingURIShortCircuitsLocalOperations
{
    input_item_t * const coreItem = input_item_New(NULL, "No URI");
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    XCTAssertNotNil(inputItem);
    XCTAssertEqualObjects(inputItem.path, @"");
    XCTAssertNil(inputItem.finderTags);

    XCTestExpectation * const completionExpectation =
        [self expectationWithDescription:@"missing URI thumbnail completion"];
    [inputItem thumbnailWithSize:NSMakeSize(128, 128)
                completionHandler:^(NSImage *image) {
        XCTAssertNil(image);
        [completionExpectation fulfill];
    }];
    [self waitForExpectations:@[completionExpectation] timeout:1.0];
}

- (void)testMalformedLocalURIProducesNoFinderTags
{
    input_item_t * const coreItem = input_item_NewFile("file://[",
                                                       "Malformed",
                                                       INPUT_DURATION_UNSET,
                                                       ITEM_LOCAL);
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    XCTAssertNotNil(inputItem);
    XCTAssertEqualObjects(inputItem.path, @"");
    XCTAssertNil(inputItem.finderTags);

    XCTestExpectation * const completionExpectation =
        [self expectationWithDescription:@"malformed URI thumbnail completion"];
    [inputItem thumbnailWithSize:NSMakeSize(128, 128)
                completionHandler:^(NSImage *image) {
        XCTAssertNil(image);
        [completionExpectation fulfill];
    }];
    [self waitForExpectations:@[completionExpectation] timeout:1.0];
}

- (void)testWritingMetadataForStreamFailsWithoutTouchingTheFilesystem
{
    input_item_t * const coreItem = input_item_NewStream("https://example.com/live",
                                                         "Live stream",
                                                         INPUT_DURATION_INDEFINITE);
    VLCInputItem * const inputItem = [[VLCInputItem alloc] initWithInputItem:coreItem];
    input_item_Release(coreItem);

    XCTAssertNotEqual([inputItem writeMetadataToFile], VLC_SUCCESS);
}

- (void)testCommonInputItemData
{
    XCTAssertEqualObjects(commonInputItemData(@[]), @{});

    input_item_t * const firstCoreItem = [self newFileInputItem];
    VLCInputItem * const firstInputItem = [[VLCInputItem alloc] initWithInputItem:firstCoreItem];
    input_item_Release(firstCoreItem);

    XCTAssertEqualObjects(commonInputItemData(@[firstInputItem]),
                          (@{ @"inputItem": firstInputItem }));

    input_item_t * const secondCoreItem = [self newFileInputItem];
    VLCInputItem * const secondInputItem = [[VLCInputItem alloc] initWithInputItem:secondCoreItem];
    input_item_Release(secondCoreItem);

    NSDictionary<NSString *, id> *commonData =
        commonInputItemData(@[ firstInputItem, secondInputItem ]);
    XCTAssertEqualObjects(commonData[@"MRL"], firstInputItem.MRL);
    XCTAssertEqualObjects(commonData[@"title"], firstInputItem.title);
    XCTAssertEqualObjects(commonData[@"artist"], @"");
    XCTAssertEqualObjects(commonData[@"artworkURL"], @"");

    secondInputItem.artist = @"Different artist";
    secondInputItem.artworkURL = [NSURL URLWithString:@"https://example.com/art.jpg"];
    commonData = commonInputItemData(@[ firstInputItem, secondInputItem ]);
    XCTAssertEqualObjects(commonData[@"artist"], VLCInputItemCommonDataDifferingFlagString);
    XCTAssertEqualObjects(commonData[@"artworkURL"], VLCInputItemCommonDataDifferingFlagString);
}

- (void)testCommonInputItemDataMarksEveryDifferingStringProperty
{
    input_item_t * const firstCoreItem = [self newFileInputItem];
    VLCInputItem * const firstInputItem = [[VLCInputItem alloc] initWithInputItem:firstCoreItem];
    input_item_Release(firstCoreItem);

    input_item_t * const secondCoreItem = input_item_NewFile("file:///tmp/Other.mp4",
                                                             "Other Name",
                                                             VLC_TICK_FROM_SEC(123),
                                                             ITEM_LOCAL);
    input_item_SetTitle(secondCoreItem, "Other Title");
    input_item_SetArtist(secondCoreItem, "Other Artist");
    input_item_SetAlbum(secondCoreItem, "Other Album");
    input_item_SetTrackNumber(secondCoreItem, "9");
    input_item_SetTrackTotal(secondCoreItem, "11");
    input_item_SetGenre(secondCoreItem, "Other Genre");
    input_item_SetDate(secondCoreItem, "2027");
    input_item_SetEpisode(secondCoreItem, "4");
    input_item_SetActors(secondCoreItem, "Other Actor");
    input_item_SetDirector(secondCoreItem, "Other Director");
    input_item_SetShowName(secondCoreItem, "Other Show");
    input_item_SetCopyright(secondCoreItem, "Other Copyright");
    input_item_SetPublisher(secondCoreItem, "Other Publisher");
    input_item_SetNowPlaying(secondCoreItem, "Other Now Playing");
    input_item_SetLanguage(secondCoreItem, "de");
    input_item_SetDescription(secondCoreItem, "Other Description");
    input_item_SetEncodedBy(secondCoreItem, "Other Encoder");
    input_item_SetArtworkURL(secondCoreItem, "https://example.com/other-art.jpg");
    input_item_SetTrackID(secondCoreItem, "other-track-id");
    VLCInputItem * const secondInputItem = [[VLCInputItem alloc] initWithInputItem:secondCoreItem];
    input_item_Release(secondCoreItem);

    NSDictionary<NSString *, id> * const commonData =
        commonInputItemData(@[ firstInputItem, secondInputItem, secondInputItem ]);
    NSArray<NSString *> * const differingProperties = @[
        @"MRL", @"decodedMRL", @"title", @"artist", @"album", @"trackNumber",
        @"trackTotal", @"genre", @"date", @"episode", @"actors", @"director",
        @"showName", @"copyright", @"publisher", @"nowPlaying", @"language",
        @"contentDescription", @"encodedBy", @"artworkURL"
    ];
    for (NSString * const property in differingProperties) {
        XCTAssertEqualObjects(commonData[property], VLCInputItemCommonDataDifferingFlagString,
                              @"%@ should be marked as differing", property);
    }
}

- (void)testRejectsNilInputItem
{
    input_item_t * const nilInputItem = NULL;
    XCTAssertNil([[VLCInputItem alloc] initWithInputItem:nilInputItem]);
}

@end

#pragma clang diagnostic pop
