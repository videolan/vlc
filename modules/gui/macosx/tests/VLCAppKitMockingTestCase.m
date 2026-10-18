/*****************************************************************************
 * VLCAppKitMockingTestCase.m: AppKit fixture with isolated defaults and method mocks
 *****************************************************************************
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
#import "VLCMacOSXTestDefaults.h"

#import <objc/runtime.h>

@interface VLCAppKitMockingTestCase ()
@property (nonatomic, strong) NSMutableArray<dispatch_block_t> *restorations;
@end

@implementation VLCAppKitMockingTestCase

- (void)setUp
{
    [super setUp];
    self.continueAfterFailure = NO;
    XCTAssertTrue(NSThread.isMainThread, @"UI tests must run on the AppKit thread");
    [NSApplication sharedApplication];
    _restorations = NSMutableArray.array;
    _defaults = [[VLCMacOSXTestDefaults alloc] init];
    NSUserDefaults * const defaults = self.defaults;
    [self replaceClassMethod:@selector(standardUserDefaults)
                     onClass:NSUserDefaults.class
                   withBlock:^NSUserDefaults *(id __unused cls) {
        return defaults;
    }];
}

- (void)tearDown
{
    for (dispatch_block_t restore in self.restorations.reverseObjectEnumerator) {
        restore();
    }
    [self.restorations removeAllObjects];
    _defaults = nil;
    [super tearDown];
}

- (void)drainMainQueue
{
    XCTestExpectation * const drained = [self expectationWithDescription:@"pending main queue updates"];
    dispatch_async(dispatch_get_main_queue(), ^{
        [drained fulfill];
    });
    [self waitForExpectations:@[drained] timeout:5.];
}

- (void)replaceInstanceMethod:(SEL)selector onClass:(Class)cls withBlock:(id)block
{
    Method method = class_getInstanceMethod(cls, selector);
    XCTAssertTrue(method != NULL, @"Missing method %@ on %@", NSStringFromSelector(selector), cls);
    IMP original = method_getImplementation(method);
    const char * const encoding = method_getTypeEncoding(method);
    IMP replacement = imp_implementationWithBlock(block);
    // Replace on this class even when it inherits the method, so an AppKit
    // superclass is never changed by a component-specific test.
    class_replaceMethod(cls, selector, replacement, encoding);
    [self.restorations addObject:^{
        class_replaceMethod(cls, selector, original, encoding);
        imp_removeBlock(replacement);
    }];
}

- (void)replaceClassMethod:(SEL)selector onClass:(Class)cls withBlock:(id)block
{
    [self replaceInstanceMethod:selector onClass:object_getClass(cls) withBlock:block];
}

@end
