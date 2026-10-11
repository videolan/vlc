/****************************************************************************
 * VLCVisualEffectViewTest.m: Visual effect native tests
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

#import "extensions/NSView+VLCAdditions.h"
#import "views/VLCVisualEffectView.h"

@interface VLCVisualEffectViewTest : XCTestCase
@property NSWindow *window;
@end

@implementation VLCVisualEffectViewTest

- (void)setUp
{
    [super setUp];
    XCTAssertTrue(NSThread.isMainThread);
    [NSApplication sharedApplication];
}

- (void)tearDown
{
    [self.window close];
    self.window = nil;
    [super tearDown];
}

- (void)testSelectsPlatformEffectAndHostsContent
{
    VLCVisualEffectView * const view = [[VLCVisualEffectView alloc] initWithFrame:NSZeroRect];
    NSView * const background = view.subviews.firstObject;
    XCTAssertNotNil(view.contentView);
    XCTAssertTrue([view.contentView isDescendantOf:view]);
    XCTAssertFalse(view.contentView.translatesAutoresizingMaskIntoConstraints);
    XCTAssertEqual(view.blendingMode, NSVisualEffectBlendingModeWithinWindow);
    if (@available(macOS 10.14, *)) {
        XCTAssertEqual(view.material, NSVisualEffectMaterialPopover);
    } else {
        XCTAssertEqual(view.material, NSVisualEffectMaterialSidebar);
    }

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    if (@available(macOS 26.0, *)) {
        XCTAssertTrue(view.usesGlassEffect);
        XCTAssertTrue([background isKindOfClass:NSGlassEffectView.class]);
        XCTAssertEqual(((NSGlassEffectView *)background).contentView, view.contentView);
        XCTAssertEqual(view.cornerRadius, ((NSGlassEffectView *)background).cornerRadius);
    } else
#endif
    {
        XCTAssertFalse(view.usesGlassEffect);
        XCTAssertTrue([background isKindOfClass:NSVisualEffectView.class]);
        XCTAssertEqual(view.contentView.superview, background);
        XCTAssertEqual(((NSVisualEffectView *)background).material, view.material);
        XCTAssertEqual(((NSVisualEffectView *)background).blendingMode, view.blendingMode);
    }
}

- (void)testContentFillsViewAfterResizingFromZeroFrame
{
    VLCVisualEffectView * const view = [[VLCVisualEffectView alloc] initWithFrame:NSZeroRect];
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 640, 240)
                                             styleMask:NSWindowStyleMaskBorderless
                                               backing:NSBackingStoreBuffered defer:NO];
    self.window.releasedWhenClosed = NO;
    self.window.contentView = view;

    for (NSValue *sizeValue in @[[NSValue valueWithSize:NSMakeSize(640, 240)],
                                [NSValue valueWithSize:NSMakeSize(320, 80)]]) {
        [self.window setContentSize:sizeValue.sizeValue];
        [view layoutSubtreeIfNeeded];
        XCTAssertTrue(NSEqualSizes(view.bounds.size, sizeValue.sizeValue));
        XCTAssertTrue(NSEqualRects(view.subviews.firstObject.frame, view.bounds));
        XCTAssertTrue(NSEqualRects([view.contentView convertRect:view.contentView.bounds toView:view],
                                  view.bounds));
        XCTAssertFalse(view.contentView.hasAmbiguousLayout);
    }
}

- (void)testFittingSizeFollowsContent
{
    VLCVisualEffectView * const view = [[VLCVisualEffectView alloc] initWithFrame:NSZeroRect];
    NSTextField * const label = [NSTextField labelWithString:@"A content-sized effect view"];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.font = [NSFont systemFontOfSize:18];
    [view.contentView addSubview:label];
    [label applyConstraintsToFillSuperview];

    const NSSize originalSize = view.fittingSize;
    XCTAssertEqualWithAccuracy(originalSize.height, label.intrinsicContentSize.height, 0.5);
    XCTAssertEqualWithAccuracy(originalSize.width, label.intrinsicContentSize.width, 0.5);
    label.font = [NSFont systemFontOfSize:36];
    XCTAssertGreaterThan(view.fittingSize.height, originalSize.height);
    XCTAssertGreaterThan(view.fittingSize.width, originalSize.width);
    XCTAssertEqualWithAccuracy(view.fittingSize.height, label.intrinsicContentSize.height, 0.5);
}

- (void)testConfigurationReachesBackingEffect
{
    VLCVisualEffectView * const view = [[VLCVisualEffectView alloc] initWithFrame:NSZeroRect];
    const NSVisualEffectMaterial material = NSVisualEffectMaterialSidebar;
    view.material = material;
    view.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    view.cornerRadius = 12;
    XCTAssertEqual(view.material, material);
    XCTAssertEqual(view.blendingMode, NSVisualEffectBlendingModeBehindWindow);
    XCTAssertEqual(view.cornerRadius, 12);
    NSView * const background = view.subviews.firstObject;

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    if (@available(macOS 26.0, *)) {
        XCTAssertEqual(((NSGlassEffectView *)background).cornerRadius, 12);
        view.cornerRadius = 0;
        XCTAssertEqual(((NSGlassEffectView *)background).cornerRadius, 0);
    } else
#endif
    {
        NSVisualEffectView * const visualEffect = (NSVisualEffectView *)background;
        XCTAssertEqual(visualEffect.material, material);
        XCTAssertEqual(visualEffect.blendingMode, NSVisualEffectBlendingModeBehindWindow);
        XCTAssertEqual(visualEffect.layer.cornerRadius, 12);
        XCTAssertTrue(visualEffect.layer.masksToBounds);
        view.cornerRadius = 0;
        XCTAssertEqual(visualEffect.layer.cornerRadius, 0);
    }
}

@end
