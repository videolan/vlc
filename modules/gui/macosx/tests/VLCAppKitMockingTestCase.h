/*****************************************************************************
 * VLCAppKitMockingTestCase.h: AppKit fixture with isolated defaults and method mocks
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

#import <XCTest/XCTest.h>
#import <Cocoa/Cocoa.h>

// Runs on the AppKit thread with in-memory standard preferences. Method
// replacements made through this fixture are restored after each test.
@interface VLCAppKitMockingTestCase : XCTestCase

@property (readonly) NSUserDefaults *defaults;

- (void)drainMainQueue;
- (void)replaceInstanceMethod:(SEL)selector onClass:(Class)cls withBlock:(id)block;
- (void)replaceClassMethod:(SEL)selector onClass:(Class)cls withBlock:(id)block;

@end
