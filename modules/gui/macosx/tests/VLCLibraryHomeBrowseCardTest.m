/****************************************************************************
 * VLCLibraryHomeBrowseCardTest.m: Home bookmarked location card tests
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

#import "VLCAppKitMockingTestCase.h"

#import <vlc_common.h>
#import <objc/runtime.h>

#import "library/home-library/VLCLibraryHomeBrowseCollectionViewItem.h"
#import "library/home-library/VLCLibraryHomeViewBrowseCarouselItemCell.h"
#import "library/home-library/VLCLibraryHomeViewBrowseCarouselItemView.h"
#import "views/VLCCarouselView.h"
#import "views/VLCUIUnits.h"

static const NSTimeInterval VLCHomeBrowseAsyncTimeout = 5.;

static NSString *VLCHomeBrowseLocalized(const char *message)
{
    return [NSString stringWithUTF8String:vlc_gettext(message)];
}

static NSEvent *VLCHomeBrowseMouseDownEvent(void)
{
    return [NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
                              location:NSZeroPoint
                         modifierFlags:0
                             timestamp:0.
                          windowNumber:0
                               context:nil
                           eventNumber:1
                            clickCount:1
                              pressure:1.];
}

static NSBitmapImageRep *VLCHomeBrowseDrawCard(NSButton *card)
{
    const NSSize size = card.bounds.size;
    NSBitmapImageRep * const bitmap =
        [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
                                                pixelsWide:(NSInteger)ceil(size.width)
                                                pixelsHigh:(NSInteger)ceil(size.height)
                                             bitsPerSample:8
                                           samplesPerPixel:4
                                                  hasAlpha:YES
                                                  isPlanar:NO
                                            colorSpaceName:NSCalibratedRGBColorSpace
                                               bytesPerRow:0
                                              bitsPerPixel:0];
    bitmap.size = size;
    NSGraphicsContext * const bitmapContext =
        [NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
    CGContextTranslateCTM(bitmapContext.CGContext, 0., size.height);
    CGContextScaleCTM(bitmapContext.CGContext, 1., -1.);
    NSGraphicsContext * const context =
        [NSGraphicsContext graphicsContextWithCGContext:bitmapContext.CGContext flipped:YES];
    [NSGraphicsContext saveGraphicsState];
    @try {
        NSGraphicsContext.currentContext = context;
        context.imageInterpolation = NSImageInterpolationNone;
        [NSColor.clearColor setFill];
        NSRectFillUsingOperation(card.bounds, NSCompositingOperationCopy);
        [card.cell drawWithFrame:card.bounds inView:card];
    } @finally {
        [NSGraphicsContext restoreGraphicsState];
    }
    return bitmap;
}

static NSRect VLCHomeBrowseOpaqueRedBounds(NSBitmapImageRep *bitmap, NSRect tileRect)
{
    NSInteger minX = NSIntegerMax;
    NSInteger minY = NSIntegerMax;
    NSInteger maxX = -1;
    NSInteger maxY = -1;
    for (NSInteger y = (NSInteger)floor(NSMinY(tileRect)); y < (NSInteger)ceil(NSMaxY(tileRect)); ++y) {
        for (NSInteger x = (NSInteger)floor(NSMinX(tileRect)); x < (NSInteger)ceil(NSMaxX(tileRect)); ++x) {
            NSColor * const color = [[bitmap colorAtX:x y:y] colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
            // The translucent tile is red too; only count the opaque icon.
            if (color.alphaComponent > .9 && color.redComponent > .9 &&
                color.greenComponent < .35 && color.blueComponent < .35) {
                minX = MIN(minX, x);
                minY = MIN(minY, y);
                maxX = MAX(maxX, x);
                maxY = MAX(maxY, y);
            }
        }
    }
    return maxX < minX ? NSZeroRect : NSMakeRect(minX, minY, maxX - minX + 1, maxY - minY + 1);
}

@interface VLCLibraryHomeBrowseCardTest : VLCAppKitMockingTestCase

@property (nonatomic, strong) NSURL *temporaryDirectory;
@property (nonatomic) NSUInteger activationCount;
@property (nonatomic) NSUInteger nativeActivationCount;
@property (nonatomic, copy) NSString *activatedMrl;

@end

@implementation VLCLibraryHomeBrowseCardTest

- (void)setUp
{
    [super setUp];
    NSString * const tempDirPathString =
        [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    NSURL * const temporaryDirectory = [NSURL fileURLWithPath:tempDirPathString isDirectory:YES];
    self.temporaryDirectory = temporaryDirectory;
    [self addTeardownBlock:^{
        NSError *error = nil;
        XCTAssertTrue([NSFileManager.defaultManager removeItemAtURL:temporaryDirectory
                                                              error:&error], @"%@", error);
    }];
    NSError *error = nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:self.temporaryDirectory
                                         withIntermediateDirectories:YES
                                                          attributes:nil
                                                               error:&error], @"%@", error);
}

- (VLCLibraryHomeViewBrowseCarouselItemView *)newCard
{
    return [[VLCLibraryHomeViewBrowseCarouselItemView alloc]
        initWithFrame:(NSRect){NSZeroPoint, VLCUIUnits.libraryCompactCardSize}];
}

- (XCTestExpectation *)expectationForCard:(VLCLibraryHomeViewBrowseCarouselItemView *)card
                                 withName:(NSString *)name
                                 subtitle:(NSString *)subtitle
{
    NSPredicate * const predicate = [NSPredicate predicateWithBlock:^BOOL(id object, NSDictionary * __unused bindings) {
        VLCLibraryHomeViewBrowseCarouselItemView * const observedCard = object;
        return [observedCard.title.precomposedStringWithCanonicalMapping
                    isEqualToString:name.precomposedStringWithCanonicalMapping] &&
               [observedCard.subtitle isEqualToString:subtitle];
    }];
    return [self expectationForPredicate:predicate evaluatedWithObject:card handler:nil];
}

- (void)mockResourceValuesForURL:(NSURL *)url
                       withBlock:(NSDictionary<NSURLResourceKey, id> *(^)(void))block
{
    const SEL selector = @selector(resourceValuesForKeys:error:);
    const Class urlClass = url.class;
    const IMP original = class_getMethodImplementation(urlClass, selector);
    [self replaceInstanceMethod:selector
                        onClass:urlClass
                      withBlock:^NSDictionary *(NSURL *queriedURL, NSArray *keys, NSError **error) {
        if ([queriedURL isEqual:url]) {
            return block();
        }
        return ((NSDictionary *(*)(id, SEL, NSArray *, NSError **))original)(queriedURL, selector, keys, error);
    }];
}

- (void)assertCard:(VLCLibraryHomeViewBrowseCarouselItemView *)card
           hasName:(NSString *)name
          subtitle:(NSString *)subtitle
           tooltip:(NSString *)tooltip
{
    // File systems can normalize Unicode while preserving the displayed name.
    XCTAssertEqualObjects(card.title.precomposedStringWithCanonicalMapping,
                          name.precomposedStringWithCanonicalMapping);
    XCTAssertEqualObjects(card.subtitle, subtitle);
    XCTAssertEqualObjects(card.toolTip, tooltip);
    XCTAssertEqualObjects(card.accessibilityLabel, card.title);
    NSString * const expectedHelp = [NSString stringWithFormat:@"%@ — %@", subtitle, tooltip];
    XCTAssertEqualObjects(card.accessibilityHelp, expectedHelp);
    XCTAssertNotNil(card.image);
}

- (void)recordActivation:(VLCLibraryHomeViewBrowseCarouselItemView *)sender
{
    ++self.activationCount;
    self.activatedMrl = sender.mrl;
}

- (void)recordNativeActivation:(id __unused)sender
{
    ++self.nativeActivationCount;
}

- (void)testAccessibilityRoleAndPressMatchNativeButton
{
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    NSString * const mrl = @"smb://bookmarks.invalid/Accessible%20Library/";
    card.mrl = mrl;
    card.target = self;
    card.action = @selector(recordActivation:);
    NSButton * const nativeButton = [[NSButton alloc] initWithFrame:card.frame];
    [nativeButton setButtonType:NSButtonTypeMomentaryPushIn];
    nativeButton.bordered = NO;
    nativeButton.target = self;
    nativeButton.action = @selector(recordNativeActivation:);
    // Command-line AppKit tests can report AXUnknown for native buttons too.
    // Compare controls in the same process instead of assuming AXButton.
    XCTAssertEqualObjects(card.accessibilityRole, nativeButton.accessibilityRole);
    const BOOL nativeResult = [nativeButton accessibilityPerformPress];
    const BOOL cardResult = [card accessibilityPerformPress];
    [self drainMainQueue];
    // Native AppKit can perform the action while returning NO here.
    XCTAssertEqual(cardResult, nativeResult);
    XCTAssertEqual(self.nativeActivationCount, 1u);
    XCTAssertEqual(self.activationCount, 1u);
    XCTAssertEqualObjects(self.activatedMrl, mrl);
}

- (void)testLocalFolderAcceptsRawAndEscapedPaths
{
    NSString * const name = @"Música & Cinema";
    NSURL * const directory =
        [self.temporaryDirectory URLByAppendingPathComponent:name isDirectory:YES];
    NSError *error = nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory
                                         withIntermediateDirectories:NO
                                                          attributes:nil
                                                               error:&error], @"%@", error);
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    for (NSString * const mrl in @[directory.path, directory.absoluteString]) {
        card.mrl = mrl;
        XCTAssertEqualObjects(card.mrl, mrl);
        [self assertCard:card
                 hasName:name
                subtitle:VLCHomeBrowseLocalized("Folder")
                 tooltip:directory.path];
    }
}

- (void)testLocalFileUsesFilePresentation
{
    NSString * const name = @"Épisode 1.mov";
    NSURL * const file = [self.temporaryDirectory URLByAppendingPathComponent:name isDirectory:NO];
    NSError *error = nil;
    XCTAssertTrue([NSData.data writeToURL:file options:0 error:&error], @"%@", error);
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    XCTestExpectation * const metadataApplied = [self expectationForCard:card
                                                                withName:name
                                                                subtitle:VLCHomeBrowseLocalized("File")];
    card.mrl = file.absoluteString;
    [self assertCard:card hasName:name subtitle:VLCHomeBrowseLocalized("Folder") tooltip:file.path];
    [self waitForExpectations:@[metadataApplied] timeout:VLCHomeBrowseAsyncTimeout];
    [self assertCard:card hasName:name subtitle:VLCHomeBrowseLocalized("File") tooltip:file.path];
}

- (void)testUnavailableLocalFolderKeepsURLFallback
{
    NSURL * const missing =
        [self.temporaryDirectory URLByAppendingPathComponent:@"Unavailable Music" isDirectory:YES];
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    XCTestExpectation * const lookupReturned =
        [self expectationWithDescription:@"unavailable metadata lookup returned"];
    [self mockResourceValuesForURL:missing withBlock:^NSDictionary *{
        [lookupReturned fulfill];
        return nil;
    }];
    card.mrl = missing.absoluteString;
    [self waitForExpectations:@[lookupReturned] timeout:VLCHomeBrowseAsyncTimeout];
    [self drainMainQueue];
    [self assertCard:card
             hasName:@"Unavailable Music"
            subtitle:VLCHomeBrowseLocalized("Folder")
             tooltip:missing.path];
}

- (void)testFolderNamesDoNotDetermineLocationType
{
    NSString * const name = @"DVD NAS OneDrive";
    NSURL * const directory =
        [self.temporaryDirectory URLByAppendingPathComponent:name isDirectory:YES];
    NSError *error = nil;
    XCTAssertTrue([NSFileManager.defaultManager createDirectoryAtURL:directory
                                         withIntermediateDirectories:NO
                                                          attributes:nil
                                                               error:&error], @"%@", error);
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    card.mrl = directory.absoluteString;
    [self assertCard:card
             hasName:name
            subtitle:VLCHomeBrowseLocalized("Folder")
             tooltip:directory.path];
}

- (void)testLocalMetadataLookupDoesNotBlockTheMainThread
{
    NSURL * const url =
        [self.temporaryDirectory URLByAppendingPathComponent:@"Media" isDirectory:YES];
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    XCTestExpectation * const lookupStarted =
        [self expectationWithDescription:@"background metadata lookup started"];
    XCTestExpectation * const metadataApplied = [self expectationForCard:card
                                                                withName:@"Localized Media"
                                                                subtitle:VLCHomeBrowseLocalized("Folder")];
    dispatch_semaphore_t const lookupGate = dispatch_semaphore_create(0);
    dispatch_semaphore_t const lookupFinished = dispatch_semaphore_create(0);
    __weak typeof(self) weakSelf = self;
    [self addTeardownBlock:^{
        dispatch_semaphore_signal(lookupGate);
        XCTAssertEqual(dispatch_semaphore_wait(lookupFinished, dispatch_time(DISPATCH_TIME_NOW, VLCHomeBrowseAsyncTimeout * NSEC_PER_SEC)), 0l);
        [weakSelf drainMainQueue];
    }];
    __block BOOL lookupOnMainThread = NO;
    [self mockResourceValuesForURL:url withBlock:^NSDictionary *{
        lookupOnMainThread = NSThread.isMainThread;
        [lookupStarted fulfill];
        // A synchronous regression must fail without deadlocking the test.
        if (!lookupOnMainThread) {
            dispatch_semaphore_wait(lookupGate, dispatch_time(DISPATCH_TIME_NOW, VLCHomeBrowseAsyncTimeout * NSEC_PER_SEC));
        }
        NSDictionary * const values =
            @{NSURLLocalizedNameKey: @"Localized Media", NSURLIsDirectoryKey: @YES};
        dispatch_semaphore_signal(lookupFinished);
        return values;
    }];

    card.mrl = url.absoluteString;
    [self assertCard:card
             hasName:@"Media"
            subtitle:VLCHomeBrowseLocalized("Folder")
             tooltip:url.path];
    [self waitForExpectations:@[lookupStarted] timeout:VLCHomeBrowseAsyncTimeout];
    XCTAssertFalse(lookupOnMainThread);
    dispatch_semaphore_signal(lookupGate);
    [self waitForExpectations:@[metadataApplied] timeout:VLCHomeBrowseAsyncTimeout];
    [self assertCard:card
             hasName:@"Localized Media"
            subtitle:VLCHomeBrowseLocalized("Folder")
             tooltip:url.path];
    [self drainMainQueue];
}

- (void)testChangingLocationRejectsOldMetadata
{
    NSURL * const url =
        [self.temporaryDirectory URLByAppendingPathComponent:@"Media.mov" isDirectory:NO];
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    XCTestExpectation * const lookupStarted =
        [self expectationWithDescription:@"old location lookup started"];
    XCTestExpectation * const lookupReturned =
        [self expectationWithDescription:@"old location lookup returned"];
    dispatch_semaphore_t const lookupGate = dispatch_semaphore_create(0);
    dispatch_semaphore_t const lookupFinished = dispatch_semaphore_create(0);
    __weak typeof(self) weakSelf = self;
    [self addTeardownBlock:^{
        dispatch_semaphore_signal(lookupGate);
        XCTAssertEqual(dispatch_semaphore_wait(lookupFinished, dispatch_time(DISPATCH_TIME_NOW, VLCHomeBrowseAsyncTimeout * NSEC_PER_SEC)), 0l);
        [weakSelf drainMainQueue];
    }];
    __block BOOL lookupOnMainThread = NO;
    [self mockResourceValuesForURL:url withBlock:^NSDictionary *{
        lookupOnMainThread = NSThread.isMainThread;
        [lookupStarted fulfill];
        if (!lookupOnMainThread) {
            dispatch_semaphore_wait(lookupGate, dispatch_time(DISPATCH_TIME_NOW, VLCHomeBrowseAsyncTimeout * NSEC_PER_SEC));
        }
        NSDictionary * const values =
            @{NSURLLocalizedNameKey: @"Outdated Media", NSURLIsDirectoryKey: @NO};
        [lookupReturned fulfill];
        dispatch_semaphore_signal(lookupFinished);
        return values;
    }];
    card.mrl = url.absoluteString;
    [self waitForExpectations:@[lookupStarted] timeout:VLCHomeBrowseAsyncTimeout];
    XCTAssertFalse(lookupOnMainThread);

    NSString * const replacement = @"smb://replacement.invalid/Current%20Library/";
    card.mrl = replacement;
    XCTestExpectation * const staleMetadataApplied =
        [self keyValueObservingExpectationForObject:card
                                            keyPath:@"title"
                                            handler:^BOOL(id __unused object, NSDictionary * __unused change) {
        return [card.title isEqualToString:@"Outdated Media"];
    }];
    staleMetadataApplied.inverted = YES;
    dispatch_semaphore_signal(lookupGate);
    [self waitForExpectations:@[lookupReturned, staleMetadataApplied]
                      timeout:VLCHomeBrowseAsyncTimeout];
    [self drainMainQueue];
    [self assertCard:card
             hasName:@"Current Library"
            subtitle:VLCHomeBrowseLocalized("Network location")
             tooltip:replacement];
}

- (void)testCollectionItemReuseRejectsOldMetadataForTheSameLocation
{
    NSURL * const url =
        [self.temporaryDirectory URLByAppendingPathComponent:@"Media.mov" isDirectory:NO];
    VLCLibraryHomeBrowseCollectionViewItem * const item =
        [[VLCLibraryHomeBrowseCollectionViewItem alloc] init];
    VLCLibraryHomeViewBrowseCarouselItemView * const card = (id)item.view;
    XCTestExpectation * const oldLookupStarted =
        [self expectationWithDescription:@"old metadata lookup started"];
    XCTestExpectation * const oldLookupReturned =
        [self expectationWithDescription:@"old metadata lookup returned"];
    dispatch_semaphore_t const oldLookupGate = dispatch_semaphore_create(0);
    dispatch_semaphore_t const oldLookupFinished = dispatch_semaphore_create(0);
    __weak typeof(self) weakSelf = self;
    [self addTeardownBlock:^{
        dispatch_semaphore_signal(oldLookupGate);
        XCTAssertEqual(dispatch_semaphore_wait(oldLookupFinished, dispatch_time(DISPATCH_TIME_NOW, VLCHomeBrowseAsyncTimeout * NSEC_PER_SEC)), 0l);
        [weakSelf drainMainQueue];
    }];
    __block NSUInteger lookupCount = 0;
    __block BOOL oldLookupOnMainThread = NO;
    [self mockResourceValuesForURL:url withBlock:^NSDictionary *{
        const NSUInteger lookup = ++lookupCount;
        if (lookup == 1) {
            oldLookupOnMainThread = NSThread.isMainThread;
            [oldLookupStarted fulfill];
            if (!oldLookupOnMainThread) {
                dispatch_semaphore_wait(oldLookupGate, dispatch_time(DISPATCH_TIME_NOW, VLCHomeBrowseAsyncTimeout * NSEC_PER_SEC));
            }
            NSDictionary * const values =
                @{NSURLLocalizedNameKey: @"Outdated Media", NSURLIsDirectoryKey: @NO};
            [oldLookupReturned fulfill];
            dispatch_semaphore_signal(oldLookupFinished);
            return values;
        }
        return @{NSURLLocalizedNameKey: @"Current Media", NSURLIsDirectoryKey: @NO};
    }];

    card.mrl = url.absoluteString;
    [self waitForExpectations:@[oldLookupStarted] timeout:VLCHomeBrowseAsyncTimeout];
    XCTAssertFalse(oldLookupOnMainThread);
    [item prepareForReuse];
    // Reusing the same MRL must invalidate the first request too (A -> empty -> A).
    XCTestExpectation * const currentMetadataApplied = [self expectationForCard:card
                                                                       withName:@"Current Media"
                                                                       subtitle:VLCHomeBrowseLocalized("File")];
    card.mrl = url.absoluteString;
    [self waitForExpectations:@[currentMetadataApplied] timeout:VLCHomeBrowseAsyncTimeout];
    [self assertCard:card
             hasName:@"Current Media"
            subtitle:VLCHomeBrowseLocalized("File")
             tooltip:url.path];

    XCTestExpectation * const staleMetadataApplied =
        [self keyValueObservingExpectationForObject:card
                                            keyPath:@"title"
                                            handler:^BOOL(id __unused object, NSDictionary * __unused change) {
        return [card.title isEqualToString:@"Outdated Media"];
    }];
    staleMetadataApplied.inverted = YES;
    dispatch_semaphore_signal(oldLookupGate);
    [self waitForExpectations:@[oldLookupReturned, staleMetadataApplied] timeout:VLCHomeBrowseAsyncTimeout];
    [self drainMainQueue];
    [self assertCard:card
             hasName:@"Current Media"
            subtitle:VLCHomeBrowseLocalized("File")
             tooltip:url.path];
}

- (void)testRemoteURLsUseDecodedNamesAndKeepExactLocation
{
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    for (NSString * const mrl in @[@"smb://bookmarks.invalid/Shared%20Library/",
                                   @"https://bookmarks.invalid/Shared%20Library/"]) {
        card.mrl = mrl;
        XCTAssertEqualObjects(card.mrl, mrl);
        [self assertCard:card
                 hasName:@"Shared Library"
                subtitle:VLCHomeBrowseLocalized("Network location")
                 tooltip:mrl];
    }
}

- (void)testDiscSchemesUseTheirDiscPresentation
{
    NSArray<NSDictionary<NSString *, NSString *> *> * const cases = @[
        @{@"mrl": @"dvd:///Volumes/Feature%20Film/", @"name": @"Feature Film", @"subtitle": @"DVD"},
        @{@"mrl": @"dvdnav:///Volumes/Feature%20Film/", @"name": @"Feature Film", @"subtitle": @"DVD"},
        @{@"mrl": @"cdda:///dev/rdisk7", @"name": @"rdisk7", @"subtitle": @"Audio CD"},
        @{@"mrl": @"vcd:///Volumes/Classic%20Disc/", @"name": @"Classic Disc", @"subtitle": @"Disc"},
        @{@"mrl": @"bluray:///Volumes/Feature%20Film/", @"name": @"Feature Film", @"subtitle": @"Disc"}
    ];
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    for (NSDictionary<NSString *, NSString *> * const test in cases) {
        card.mrl = test[@"mrl"];
        [self assertCard:card
                 hasName:test[@"name"]
                subtitle:VLCHomeBrowseLocalized(test[@"subtitle"].UTF8String)
                 tooltip:test[@"mrl"]];
    }
}

- (void)testMRLIsCopiedAndSubtitleReadsCellStorage
{
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    NSString * const original = @"smb://bookmarks.invalid/Shared%20Library/";
    NSMutableString * const mutableMrl = original.mutableCopy;
    card.mrl = mutableMrl;
    [mutableMrl appendString:@"replacement"];
    XCTAssertEqualObjects(card.mrl, original);
    XCTAssertEqualObjects(card.toolTip, original);

    VLCLibraryHomeViewBrowseCarouselItemCell * const cell = (id)card.cell;
    NSMutableString * const subtitle = [@"Canonical subtitle" mutableCopy];
    cell.subtitle = subtitle;
    [subtitle appendString:@" changed"];
    XCTAssertEqualObjects(card.subtitle, @"Canonical subtitle");
    XCTAssertEqualObjects(card.subtitle, cell.subtitle);
}

- (void)testCollectionItemReuseClearsLocationAndCanBeReconfigured
{
    VLCLibraryHomeBrowseCollectionViewItem * const item =
        [[VLCLibraryHomeBrowseCollectionViewItem alloc] init];
    VLCLibraryHomeViewBrowseCarouselItemView * const card = (id)item.view;
    XCTAssertTrue([card isKindOfClass:VLCLibraryHomeViewBrowseCarouselItemView.class]);
    VLCCarouselView * const carousel = [[VLCCarouselView alloc] initWithFrame:NSZeroRect];
    card.mrl = @"smb://bookmarks.invalid/Original/";
    card.target = self;
    card.action = @selector(recordActivation:);
    card.carouselView = carousel;
    item.representedObject = card.mrl;

    [item prepareForReuse];
    XCTAssertNil(item.representedObject);
    XCTAssertEqualObjects(card.mrl, @"");
    XCTAssertNil(card.target);
    XCTAssertEqual(card.action, (SEL)NULL);
    XCTAssertNil(card.carouselView);
    [self assertCard:card hasName:@"" subtitle:VLCHomeBrowseLocalized("Folder") tooltip:@""];
    [card performClick:nil];
    XCTAssertEqual(self.activationCount, 0u);

    NSString * const replacement = @"smb://replacement.invalid/New%20Library/";
    card.mrl = replacement;
    card.target = self;
    card.action = @selector(recordActivation:);
    [card performClick:nil];
    XCTAssertEqual(self.activationCount, 1u);
    XCTAssertEqualObjects(self.activatedMrl, replacement);
    [self assertCard:card
             hasName:@"New Library"
            subtitle:VLCHomeBrowseLocalized("Network location")
             tooltip:replacement];
}

- (void)testMouseForwardingHonorsEnabledStateAndClearsHighlight
{
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    VLCCarouselView * const carousel = [[VLCCarouselView alloc] initWithFrame:NSZeroRect];
    card.carouselView = carousel;
    NSEvent * const event = VLCHomeBrowseMouseDownEvent();
    __block NSUInteger forwardingCount = 0;
    __block VLCCarouselView *forwardedCarousel = nil;
    __block NSEvent *forwardedEvent = nil;
    __block BOOL highlightedDuringForwarding = NO;
    [self replaceInstanceMethod:@selector(mouseDown:)
                        onClass:VLCCarouselView.class
                      withBlock:^(VLCCarouselView *sender, NSEvent *receivedEvent) {
        ++forwardingCount;
        forwardedCarousel = sender;
        forwardedEvent = receivedEvent;
        highlightedDuringForwarding = card.cell.isHighlighted;
    }];

    card.enabled = NO;
    [card mouseDown:event];
    XCTAssertEqual(forwardingCount, 0u);
    XCTAssertFalse(card.cell.isHighlighted);

    card.enabled = YES;
    [card mouseDown:event];
    XCTAssertEqual(forwardingCount, 1u);
    XCTAssertEqual(forwardedCarousel, carousel);
    XCTAssertEqual(forwardedEvent, event);
    XCTAssertTrue(highlightedDuringForwarding);
    XCTAssertFalse(card.cell.isHighlighted);
}

- (void)testIconRenderingPreservesWideTallAndSquareAspectRatios
{
    const CGFloat iconSize = VLCUIUnits.mediumIconSize;
    const CGFloat tileSize = VLCUIUnits.largeIconContainerSize;
    const CGFloat padding = VLCUIUnits.mediumSpacing + VLCUIUnits.smallSpacing;
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    card.title = @"";
    VLCLibraryHomeViewBrowseCarouselItemCell * const cell = (id)card.cell;
    cell.subtitle = @"";
    cell.iconTint = NSColor.redColor;
    const NSRect tileRect =
        NSMakeRect(padding, NSMidY(card.bounds) - tileSize / 2., tileSize, tileSize);
    const NSSize shapes[] = {NSMakeSize(40., 20.), NSMakeSize(20., 40.), NSMakeSize(20., 20.)};
    for (NSUInteger index = 0; index < sizeof(shapes) / sizeof(shapes[0]); ++index) {
        const NSSize shape = shapes[index];
        card.image = [NSImage imageWithSize:shape flipped:NO drawingHandler:^BOOL(NSRect rect) {
            [NSColor.blackColor setFill];
            NSRectFill(rect);
            return YES;
        }];
        NSRect const ink = VLCHomeBrowseOpaqueRedBounds(VLCHomeBrowseDrawCard(card), tileRect);
        XCTAssertFalse(NSIsEmptyRect(ink), @"%@ source must produce icon", NSStringFromSize(shape));
        const CGFloat scale = iconSize / MAX(shape.width, shape.height);
        XCTAssertEqualWithAccuracy(NSWidth(ink), shape.width * scale, 1.);
        XCTAssertEqualWithAccuracy(NSHeight(ink), shape.height * scale, 1.);
        XCTAssertEqualWithAccuracy(NSMidX(ink), NSMidX(tileRect), 1.);
        XCTAssertEqualWithAccuracy(NSMidY(ink), NSMidY(tileRect), 1.);
        XCTAssertTrue(NSContainsRect(tileRect, ink));
    }
}

- (void)testMissingOrZeroSizeIconRendersWithoutOpaqueIconPixels
{
    const CGFloat tileSize = VLCUIUnits.largeIconContainerSize;
    const CGFloat padding = VLCUIUnits.mediumSpacing + VLCUIUnits.smallSpacing;
    VLCLibraryHomeViewBrowseCarouselItemView * const card = [self newCard];
    VLCLibraryHomeViewBrowseCarouselItemCell * const cell = (id)card.cell;
    cell.iconTint = NSColor.redColor;
    const NSRect tileRect =
        NSMakeRect(padding, NSMidY(card.bounds) - tileSize / 2., tileSize, tileSize);
    card.image = nil;
    XCTAssertTrue(NSIsEmptyRect(VLCHomeBrowseOpaqueRedBounds(VLCHomeBrowseDrawCard(card), tileRect)));
    card.image = [[NSImage alloc] initWithSize:NSZeroSize];
    XCTAssertTrue(NSIsEmptyRect(VLCHomeBrowseOpaqueRedBounds(VLCHomeBrowseDrawCard(card), tileRect)));
}

@end
