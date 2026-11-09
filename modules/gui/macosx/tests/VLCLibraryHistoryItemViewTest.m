/*****************************************************************************
 * VLCLibraryHistoryItemViewTest.m: native History row and card tests
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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
#import "VLCLibraryDataTypesIntegrationTestSupport.h"

#import "extensions/NSString+Helpers.h"
#import "library/VLCLibraryDataTypes.h"
#import "library/VLCLibraryMarkUnplayedButton.h"
#import "library/VLCLibraryRepresentedItem.h"
#import "library/VLCLibraryTableCellView.h"

@interface VLCLibraryHistoryItemViewTest : VLCAppKitMockingTestCase
@end

@implementation VLCLibraryHistoryItemViewTest

- (void)setUp
{
    [super setUp];
    XCTestExpectation * const prepared = [self expectationWithDescription:@"Persisted library fixtures discovered"];
    __block BOOL ready = NO;
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        ready = VLCLibraryDataTypesIntegrationStart() && VLCLibraryDataTypesIntegrationPrepareFactoryFixtures();
        [prepared fulfill];
    });
    [self waitForExpectations:@[prepared] timeout:20.];
    XCTAssertTrue(ready);
}

- (void)tearDown
{
    VLCLibraryDataTypesIntegrationStop();
    [super tearDown];
}

- (void)testMarkUnheardButtonActsOnMediaAndResetsOnReuse
{
    const int64_t mediaID = VLCLibraryDataTypesIntegrationFactoryAudioMediaID();
    vlc_medialibrary_t * const library = VLCLibraryDataTypesIntegrationMediaLibrary();
    XCTAssertEqual(vlc_ml_media_set_played(library, mediaID, true), VLC_SUCCESS);
    VLCMediaLibraryMediaItem * const audio = [VLCMediaLibraryMediaItem mediaItemForLibraryID:mediaID];
    XCTAssertGreaterThan(audio.lastPlayedDate, (time_t)0);
    VLCLibraryRepresentedItem * const item = [[VLCLibraryRepresentedItem alloc]
        initWithItem:audio
          parentType:VLCMediaLibraryParentGroupTypeUnknown];

    NSBundle * const bundle = [NSBundle bundleForClass:self.class];
    NSNib * const nib = [[NSNib alloc] initWithNibNamed:@"VLCLibraryVideoTableCellView" bundle:bundle];
    NSArray *objects;
    XCTAssertTrue([nib instantiateWithOwner:nil topLevelObjects:&objects]);
    VLCLibraryTableCellView *cell = nil;
    for (id object in objects) {
        if ([object isKindOfClass:VLCLibraryTableCellView.class]) {
            cell = object;
            break;
        }
    }
    XCTAssertNotNil(cell.markUnplayedButton);

    cell.representedItem = item;
    cell.showsMarkUnplayedButton = YES;
    XCTAssertFalse(cell.markUnplayedButton.hidden);
    XCTAssertEqualObjects(cell.markUnplayedButton.toolTip, _NS("Mark as Unheard"));

    [cell prepareForReuse];
    cell.representedItem = item;
    XCTAssertTrue(cell.markUnplayedButton.hidden);

    cell.showsMarkUnplayedButton = YES;
    [cell.markUnplayedButton performClick:nil];
    XCTAssertEqual(audio.lastPlayedDate, (time_t)0);
    XCTAssertTrue(cell.markUnplayedButton.hidden);
}

@end
