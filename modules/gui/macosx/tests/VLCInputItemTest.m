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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"

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
    XCTAssertEqualObjects(inputItem.options, (@[ @"network-caching=1000" ]));
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
    XCTAssertEqualObjects(inputItem.path, @"");

    inputItem.artworkURL = [NSURL URLWithString:@"https://example.com/live.jpg"];
    XCTAssertEqualObjects(inputItem.artworkURL.absoluteString, @"https://example.com/live.jpg");

    [(id)inputItem setArtworkURL:nil];
    XCTAssertNil(inputItem.artworkURL);
}

- (void)testRejectsNilInputItem
{
    input_item_t * const nilInputItem = NULL;
    XCTAssertNil([[VLCInputItem alloc] initWithInputItem:nilInputItem]);
}

@end

#pragma clang diagnostic pop
