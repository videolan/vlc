/*****************************************************************************
 * NSScreen+VLCAdditions.m: Category with some additions to NSScreen
 *****************************************************************************
 * Copyright (C) 2003-2015 VLC authors and VideoLAN
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
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

#import "NSScreen+VLCAdditions.h"
#import "main/CompatibilityFixes.h"
#import "windows/video/VLCWindow.h"

@implementation NSScreen (VLCAdditions)

static NSMutableArray *blackoutWindows = NULL;

static bool b_old_spaces_style = YES;

+ (void)load
{
    /* init our fake object attribute */
    blackoutWindows = [[NSMutableArray alloc] initWithCapacity:1];

    NSUserDefaults *userDefaults = [[NSUserDefaults alloc] init];
    [userDefaults addSuiteNamed:@"com.apple.spaces"];
    /* this is system settings -> mission control -> monitors using different spaces */
    NSNumber *o_span_displays = [userDefaults objectForKey:@"spans-displays"];

    b_old_spaces_style = [o_span_displays boolValue];
}

+ (NSScreen *)screenWithDisplayID: (CGDirectDisplayID)displayID
{
    NSUInteger count = [[NSScreen screens] count];

    for ( NSUInteger i = 0; i < count; i++ ) {
        NSScreen *screen = [[NSScreen screens] objectAtIndex:i];
        if ([screen displayID] == displayID)
            return screen;
    }
    return nil;
}

- (BOOL)hasMenuBar
{
    if (b_old_spaces_style)
        return ([self displayID] == [[[NSScreen screens] firstObject] displayID]);
    else
        return YES;
}

- (BOOL)hasDock
{
    NSRect screen_frame = [self frame];
    NSRect screen_visible_frame = [self visibleFrame];
    CGFloat f_menu_bar_thickness = [self hasMenuBar] ? [[NSStatusBar systemStatusBar] thickness] : 0.0;

    BOOL b_found_dock = NO;
    if (screen_visible_frame.size.width < screen_frame.size.width)
        b_found_dock = YES;
    else if (screen_visible_frame.size.height + f_menu_bar_thickness < screen_frame.size.height)
        b_found_dock = YES;

    return b_found_dock;
}

- (BOOL)isScreen: (NSScreen*)screen
{
    return ([self displayID] == [screen displayID]);
}

- (CGDirectDisplayID)displayID
{
    return (CGDirectDisplayID)[[[self deviceDescription] objectForKey: @"NSScreenNumber"] intValue];
}

- (void)blackoutOtherScreens
{
    /* Free our previous blackout window (follow blackoutWindow alloc strategy) */
    [blackoutWindows makeObjectsPerformSelector:@selector(close)];
    [blackoutWindows removeAllObjects];

    NSUInteger screenCount = [[NSScreen screens] count];
    for (NSUInteger i = 0; i < screenCount; i++) {
        NSScreen *screen = [[NSScreen screens] objectAtIndex:i];
        VLCWindow *blackoutWindow;
        NSRect screen_rect;

        if ([self isScreen: screen])
            continue;

        screen_rect = [screen frame];
        screen_rect.origin.x = screen_rect.origin.y = 0;

        /* blackoutWindow alloc strategy
         - The NSMutableArray blackoutWindows has the blackoutWindow references
         - blackoutOtherDisplays is responsible for alloc/releasing its Windows
         */
        blackoutWindow = [[VLCWindow alloc] initWithContentRect: screen_rect styleMask: NSBorderlessWindowMask
                                                        backing: NSBackingStoreBuffered defer: NO screen: screen];
        [blackoutWindow setBackgroundColor:[NSColor blackColor]];
        [blackoutWindow setLevel: NSFloatingWindowLevel]; /* Disappear when Expose is triggered */
        [blackoutWindow setReleasedWhenClosed:NO]; // window is released when deleted from array above

        [blackoutWindow displayIfNeeded];
        [blackoutWindow orderFront: self animate: YES];

        [blackoutWindows addObject: blackoutWindow];

        [screen setFullscreenPresentationOptions];
    }
}

+ (void)unblackoutScreens
{
    NSUInteger blackoutWindowCount = [blackoutWindows count];

    for (NSUInteger i = 0; i < blackoutWindowCount; i++) {
        VLCWindow *blackoutWindow = [blackoutWindows objectAtIndex:i];
        [[blackoutWindow screen] setNonFullscreenPresentationOptions];
        [blackoutWindow closeAndAnimate: YES];
    }
}

- (void)setFullscreenPresentationOptions
{
    NSApplicationPresentationOptions presentationOpts = [NSApp presentationOptions];
    if ([self hasMenuBar])
        presentationOpts |= NSApplicationPresentationAutoHideMenuBar;
    if ([self hasMenuBar] || [self hasDock])
        presentationOpts |= NSApplicationPresentationAutoHideDock;
    [NSApp setPresentationOptions:presentationOpts];
}

- (void)setNonFullscreenPresentationOptions
{
    NSApplicationPresentationOptions presentationOpts = [NSApp presentationOptions];
    if ([self hasMenuBar])
        presentationOpts &= (~NSApplicationPresentationAutoHideMenuBar);
    if ([self hasMenuBar] || [self hasDock])
        presentationOpts &= (~NSApplicationPresentationAutoHideDock);
    [NSApp setPresentationOptions:presentationOpts];
}


@end
