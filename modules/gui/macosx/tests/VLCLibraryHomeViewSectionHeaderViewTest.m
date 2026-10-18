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
    for (NSView *view in @[self.header.countField, self.header.libraryButton]) {
        // Native controls include drawing margins outside their alignment rect.
        const NSRect alignmentRect = [view alignmentRectForFrame:view.frame];
        const NSRect contentRect = [view.superview convertRect:alignmentRect toView:self.header];
        XCTAssertTrue(NSContainsRect(self.header.bounds, contentRect), @"%@ frame %@ in header %@",
                      view.class, NSStringFromRect(contentRect), NSStringFromRect(self.header.bounds));
        XCTAssertFalse(view.hasAmbiguousLayout);
        XCTAssertGreaterThanOrEqual(view.frame.size.height, view.intrinsicContentSize.height);
    }
    XCTAssertFalse(self.header.hasAmbiguousLayout);
}

- (NSData *)renderedButtonImage
{
    NSButton * const button = self.header.libraryButton;
    NSView * const contentView = self.window.contentView;
    const NSRect buttonRect = [button convertRect:button.bounds toView:contentView];
    NSBitmapImageRep * const bitmap = [contentView bitmapImageRepForCachingDisplayInRect:buttonRect];
    [contentView cacheDisplayInRect:buttonRect toBitmapImageRep:bitmap];
    return [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
}

- (void)testTitleIsCopiedAndUpdatesButtonAndAccessibility
{
    NSMutableString * const title = [@"Recent music" mutableCopy];
    self.header.title = title;
    [title appendString:@" changed after assignment"];
    XCTAssertEqualObjects(self.header.title, @"Recent music");
    XCTAssertEqualObjects(self.header.libraryButton.title, @"Recent music");
    NSString * const accessibilityLabel =
        [NSString stringWithFormat:_NS("Open %@ in the library"), @"Recent music"];
    XCTAssertEqualObjects(self.header.libraryButton.accessibilityLabel, accessibilityLabel);
    XCTAssertEqualObjects(self.header.libraryButton.toolTip, accessibilityLabel);

    self.header.title = @"Library";
    XCTAssertEqualObjects(self.header.libraryButton.title, @"Library");
    NSString * const libraryAccessibilityLabel =
        [NSString stringWithFormat:_NS("Open %@ in the library"), @"Library"];
    XCTAssertEqualObjects(self.header.libraryButton.accessibilityLabel, libraryAccessibilityLabel);
}

- (void)testCountUpdatesIncludingZeroAndSingular
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

- (void)testPressedTitleChangesRenderingAndRestoresOnRelease
{
    NSData * const normalImage = [self renderedButtonImage];
    XCTAssertNotNil(normalImage);
    [self.header.libraryButton highlight:YES];
    NSData * const pressedImage = [self renderedButtonImage];
    XCTAssertNotEqualObjects(pressedImage, normalImage);
    [self.header.libraryButton highlight:NO];
    XCTAssertEqualObjects([self renderedButtonImage], normalImage);
    XCTAssertEqual(self.header.libraryButton.state, NSControlStateValueOff);
}

- (void)testTitleButtonHasTrailingChevronAndNoBackground
{
    XCTAssertFalse(self.header.libraryButton.bordered);
    if (@available(macOS 10.14, *)) {
        XCTAssertEqualObjects(self.header.libraryButton.contentTintColor, NSColor.labelColor);
    }
    XCTAssertNotNil(self.header.libraryButton.image);
    XCTAssertEqual(self.header.libraryButton.imagePosition, NSImageTrailing);
    XCTAssertEqual(self.header.subviews.count, (NSUInteger)2);
    XCTAssertEqual(self.header.libraryButton.superview, self.header);
    XCTAssertEqual(self.header.countField.superview, self.header);
    XCTAssertLessThan(self.header.libraryButton.frame.size.width, self.header.frame.size.width);
}

- (void)testNarrowHeaderTruncatesTitleAndKeepsCountAtTrailingEdge
{
    self.header.title = @"A very long recent video collection section title";
    self.widthConstraint.constant = 320;
    [self.window.contentView layoutSubtreeIfNeeded];
    [self assertContentFitsHeader];

    const NSRect buttonRect = [self.header.libraryButton convertRect:self.header.libraryButton.bounds
                                                            toView:self.header];
    const NSRect countRect = [self.header.countField alignmentRectForFrame:self.header.countField.frame];
    XCTAssertLessThanOrEqual(NSMaxX(buttonRect), NSMinX(countRect));
    XCTAssertEqualWithAccuracy(NSMidY(countRect), NSMidY(buttonRect), 0.5);
    XCTAssertEqualWithAccuracy(NSMaxX(countRect), NSMaxX(self.header.bounds), 0.5);
    XCTAssertGreaterThanOrEqual(countRect.size.width, self.header.countField.intrinsicContentSize.width);
    XCTAssertLessThan(self.header.libraryButton.frame.size.width,
                      self.header.libraryButton.intrinsicContentSize.width);
    XCTAssertEqual(self.header.libraryButton.lineBreakMode, NSLineBreakByTruncatingTail);

    self.widthConstraint.constant = 800;
    [self.window.contentView layoutSubtreeIfNeeded];
    [self assertContentFitsHeader];
}

- (void)testCountRemainsVisibleWhenItsWidthChanges
{
    self.header.title = @"A very long recent video collection section title";
    self.widthConstraint.constant = 320;
    for (NSNumber *count in @[@0, @1, @(NSUIntegerMax), @2]) {
        self.header.itemCount = count.unsignedIntegerValue;
        [self.window.contentView layoutSubtreeIfNeeded];
        [self assertContentFitsHeader];
        const NSRect countRect = [self.header.countField alignmentRectForFrame:self.header.countField.frame];
        XCTAssertEqualWithAccuracy(NSMaxX(countRect), NSMaxX(self.header.bounds), 0.5);
        XCTAssertGreaterThanOrEqual(countRect.size.width, self.header.countField.intrinsicContentSize.width);
        XCTAssertLessThanOrEqual(NSMaxX(self.header.libraryButton.frame), NSMinX(countRect));
    }
}

- (void)testHeightFollowsContentAndLargerFonts
{
    const CGFloat originalHeight = self.header.frame.size.height;
    XCTAssertGreaterThan(originalHeight, 0);
    XCTAssertEqualWithAccuracy(originalHeight, self.header.fittingSize.height, 0.5);

    NSFont * const originalFont = self.header.libraryButton.font;
    self.header.libraryButton.font = [NSFont boldSystemFontOfSize:originalFont.pointSize + 12];
    [self.window.contentView layoutSubtreeIfNeeded];
    XCTAssertGreaterThan(self.header.frame.size.height, originalHeight);
    XCTAssertEqualWithAccuracy(self.header.frame.size.height, self.header.fittingSize.height, 0.5);
    [self assertContentFitsHeader];

    self.header.libraryButton.font = originalFont;
    [self.window.contentView layoutSubtreeIfNeeded];
    XCTAssertEqualWithAccuracy(self.header.frame.size.height, originalHeight, 0.5);
}

@end
