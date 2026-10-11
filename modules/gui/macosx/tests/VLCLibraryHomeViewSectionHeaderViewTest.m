/****************************************************************************
 * VLCLibraryHomeViewSectionHeaderViewTest.m: Home header native tests
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

#import "extensions/NSString+Helpers.h"
#import "library/home-library/VLCLibraryHomeViewSectionHeaderView.h"

@interface VLCLibraryHomeViewSectionHeaderView (TestAccess)
@property (readonly) NSTextField *titleField;
@property (readonly) NSTextField *countField;
@end

@interface VLCLibraryHomeViewSectionHeaderViewTest : XCTestCase
@property NSWindow *window;
@property VLCLibraryHomeViewSectionHeaderView *header;
@property NSLayoutConstraint *widthConstraint;
@property NSUInteger actionCount;
@property (weak) id actionSender;
@end

@implementation VLCLibraryHomeViewSectionHeaderViewTest

- (void)setUp
{
    [super setUp];
    XCTAssertTrue(NSThread.isMainThread);
    [NSApplication sharedApplication];
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 840, 300)
                                             styleMask:NSWindowStyleMaskBorderless
                                               backing:NSBackingStoreBuffered defer:NO];
    self.window.releasedWhenClosed = NO;
    self.header = [[VLCLibraryHomeViewSectionHeaderView alloc] initWithFrame:NSZeroRect];
    self.header.translatesAutoresizingMaskIntoConstraints = NO;
    self.header.title = @"Recent videos";
    self.header.itemCount = 123;
    [self.window.contentView addSubview:self.header];
    self.widthConstraint = [self.header.widthAnchor constraintEqualToConstant:800];
    [NSLayoutConstraint activateConstraints:@[
        [self.header.leadingAnchor constraintEqualToAnchor:self.window.contentView.leadingAnchor constant:20],
        [self.header.topAnchor constraintEqualToAnchor:self.window.contentView.topAnchor constant:20],
        self.widthConstraint,
    ]];
    [self.window.contentView layoutSubtreeIfNeeded];
}

- (void)tearDown
{
    [self.window close];
    self.header = nil;
    self.widthConstraint = nil;
    self.window = nil;
    [super tearDown];
}

- (void)openLibrary:(id)sender
{
    self.actionCount++;
    self.actionSender = sender;
}

- (void)assertContentFitsHeader
{
    for (NSView *view in @[self.header.titleField, self.header.countField, self.header.libraryButton]) {
        const NSRect contentRect = [view convertRect:view.bounds toView:self.header];
        XCTAssertTrue(NSContainsRect(self.header.bounds, contentRect));
        XCTAssertFalse(view.hasAmbiguousLayout);
        XCTAssertGreaterThanOrEqual(view.frame.size.height, view.intrinsicContentSize.height);
    }
    XCTAssertFalse(self.header.hasAmbiguousLayout);
}

- (void)testTitleIsCopiedAndUpdatesLabelAndAccessibility
{
    NSMutableString * const title = [@"Recent music" mutableCopy];
    self.header.title = title;
    [title appendString:@" changed after assignment"];
    XCTAssertEqualObjects(self.header.title, @"Recent music");
    XCTAssertEqualObjects(self.header.titleField.stringValue, @"Recent music");
    XCTAssertEqualObjects(self.header.titleField.toolTip, @"Recent music");
    NSString * const accessibilityLabel =
        [NSString stringWithFormat:_NS("Open %@ in the library"), @"Recent music"];
    XCTAssertEqualObjects(self.header.libraryButton.accessibilityLabel, accessibilityLabel);
    XCTAssertEqualObjects(self.header.libraryButton.toolTip, accessibilityLabel);

    self.header.title = @"Library";
    XCTAssertEqualObjects(self.header.titleField.stringValue, @"Library");
    NSString * const libraryAccessibilityLabel =
        [NSString stringWithFormat:_NS("Open %@ in the library"), @"Library"];
    XCTAssertEqualObjects(self.header.libraryButton.accessibilityLabel, libraryAccessibilityLabel);
}

- (void)testCountUpdatesSubtitleIncludingZeroAndSingular
{
    for (NSNumber *count in @[@0, @1, @2, @123, @1, @0]) {
        const NSUInteger itemCount = count.unsignedIntegerValue;
        self.header.itemCount = itemCount;
        XCTAssertEqual(self.header.itemCount, itemCount);
        NSString * const subtitle = _NPS("%lu item", "%lu items", (unsigned long)itemCount);
        XCTAssertEqualObjects(self.header.countField.stringValue, subtitle);
    }
}

- (void)testLibraryButtonDispatchesItsAction
{
    self.header.libraryButton.target = self;
    self.header.libraryButton.action = @selector(openLibrary:);
    [self.header.libraryButton performClick:nil];
    XCTAssertEqual(self.actionCount, (NSUInteger)1);
    XCTAssertEqual(self.actionSender, self.header.libraryButton);
}

- (void)testNarrowHeaderTruncatesTitleWithoutOverlappingButton
{
    self.header.title = @"A very long recent video collection section title";
    self.widthConstraint.constant = 320;
    [self.window.contentView layoutSubtreeIfNeeded];
    [self assertContentFitsHeader];

    const NSRect buttonRect = [self.header.libraryButton convertRect:self.header.libraryButton.bounds
                                                            toView:self.header];
    for (NSTextField *label in @[self.header.titleField, self.header.countField]) {
        const NSRect labelRect = [label convertRect:label.bounds toView:self.header];
        XCTAssertLessThanOrEqual(NSMaxX(labelRect), NSMinX(buttonRect));
    }
    XCTAssertLessThan(self.header.titleField.frame.size.width,
                      self.header.titleField.intrinsicContentSize.width);
    XCTAssertEqual(self.header.titleField.lineBreakMode, NSLineBreakByTruncatingTail);
    XCTAssertGreaterThanOrEqual(self.header.libraryButton.frame.size.width,
                                self.header.libraryButton.intrinsicContentSize.width);

    self.widthConstraint.constant = 800;
    [self.window.contentView layoutSubtreeIfNeeded];
    [self assertContentFitsHeader];
}

- (void)testHeightFollowsContentAndLargerFonts
{
    const CGFloat originalHeight = self.header.frame.size.height;
    XCTAssertGreaterThan(originalHeight, 0);
    XCTAssertEqualWithAccuracy(originalHeight, self.header.fittingSize.height, 0.5);

    NSFont * const originalFont = self.header.titleField.font;
    self.header.titleField.font = [NSFont boldSystemFontOfSize:originalFont.pointSize + 12];
    [self.window.contentView layoutSubtreeIfNeeded];
    XCTAssertGreaterThan(self.header.frame.size.height, originalHeight);
    XCTAssertEqualWithAccuracy(self.header.frame.size.height, self.header.fittingSize.height, 0.5);
    [self assertContentFitsHeader];

    self.header.titleField.font = originalFont;
    [self.window.contentView layoutSubtreeIfNeeded];
    XCTAssertEqualWithAccuracy(self.header.frame.size.height, originalHeight, 0.5);
}

@end
