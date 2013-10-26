/*****************************************************************************
 * CompatibilityFixes.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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

#import <Cocoa/Cocoa.h>

#pragma mark -
#pragma OS detection code
#define OSX_SNOW_LEOPARD (NSAppKitVersionNumber < 1115 && NSAppKitVersionNumber >= 1038)
#define OSX_LION (NSAppKitVersionNumber < 1162 && NSAppKitVersionNumber >= 1115.2)
#define OSX_MOUNTAIN_LION (NSAppKitVersionNumber < 1244 && NSAppKitVersionNumber >= 1162)
#define OSX_MAVERICKS NSAppKitVersionNumber >= 1244

#pragma mark -
#pragma Fixes for OS X Snow Leopard (10.6)

#ifndef MAC_OS_X_VERSION_10_7
enum {
    NSWindowCollectionBehaviorFullScreenPrimary = 1 << 7,
    NSWindowCollectionBehaviorFullScreenAuxiliary = 1 << 8
};

enum {
    NSApplicationPresentationFullScreen                 = (1 << 10),
    NSApplicationPresentationAutoHideToolbar            = (1 << 11)
};

enum {
    NSWindowAnimationBehaviorDefault = 0,       // let AppKit infer animation behavior for this window
    NSWindowAnimationBehaviorNone = 2,          // suppress inferred animations (don't animate)
    NSWindowAnimationBehaviorDocumentWindow = 3,
    NSWindowAnimationBehaviorUtilityWindow = 4,
    NSWindowAnimationBehaviorAlertPanel = 5
};
typedef NSInteger NSWindowAnimationBehavior;

/* the following is just to fix warnings, not for implementation! */
@interface NSWindow (IntroducedInLion)
- (void)setRestorable:(BOOL)b_value;
- (void)toggleFullScreen:(id)id_value;
- (void)windowWillEnterFullScreen:(NSNotification *)notification;
- (void)windowDidEnterFullScreen:(NSNotification *)notification;
- (void)windowWillExitFullScreen:(NSNotification *)notification;
- (void)setAnimationBehavior:(NSWindowAnimationBehavior)newAnimationBehavior;
@end

@interface NSEvent (IntroducedInLion)
- (BOOL)isDirectionInvertedFromDevice;
@end

#endif
