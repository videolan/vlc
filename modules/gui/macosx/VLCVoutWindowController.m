/*****************************************************************************
 * VLCVoutWindowController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
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

#import "VLCVoutWindowController.h"
#import "intf.h"
#import "MainWindow.h"
#import "VideoView.h"

#import "VideoEffects.h"
#import "AudioEffects.h"
#import "playlistinfo.h"
#import "bookmarks.h"
#import "TrackSynchronization.h"

@implementation VLCVoutWindowController

- (id)init
{
    self = [super init];
    o_vout_dict = [[NSMutableDictionary alloc] init];
    i_currentWindowLevel = NSNormalWindowLevel;
    return self;
}

- (void)dealloc
{
    NSArray *keys = [o_vout_dict allKeys];
    for (NSValue *key in keys)
        [self removeVoutforDisplay:key];

    [o_vout_dict release];
    [super dealloc];
}


- (VLCVoutView *)setupVoutForWindow:(vout_window_t *)p_wnd withProposedVideoViewPosition:(NSRect)videoViewPosition
{
    BOOL b_nonembedded = NO;
    BOOL b_nativeFullscreenMode = [[VLCMain sharedInstance] nativeFullscreenMode];
    BOOL b_video_deco = var_InheritBool(VLCIntf, "video-deco");
    BOOL b_video_wallpaper = var_InheritBool(VLCIntf, "video-wallpaper");
    BOOL b_multiple_vout_windows = [o_vout_dict count] > 0;
    VLCVoutView *o_vout_view;
    VLCVideoWindowCommon *o_new_video_window;

    if (b_multiple_vout_windows && b_video_wallpaper)
        b_video_wallpaper = false;

    // TODO: make lion fullscreen compatible with video-wallpaper and !embedded-video
    if ((b_video_wallpaper || !b_video_deco) && !b_nativeFullscreenMode) {
        // b_video_wallpaper is priorized over !b_video_deco

        msg_Dbg(VLCIntf, "Creating background / blank window");
        NSScreen *screen = [NSScreen screenWithDisplayID:(CGDirectDisplayID)var_InheritInteger(VLCIntf, "macosx-vdev")];
        if (!screen)
            screen = [[VLCMainWindow sharedInstance] screen];

        NSRect window_rect;
        if (b_video_wallpaper)
            window_rect = [screen frame];
        else
            window_rect = [[VLCMainWindow sharedInstance] frame];

        NSUInteger mask = NSBorderlessWindowMask;
        if (!OSX_SNOW_LEOPARD && !b_video_deco)
            mask |= NSResizableWindowMask;

        BOOL b_no_video_deco_only = !b_video_wallpaper;
        o_new_video_window = [[VLCVideoWindowCommon alloc] initWithContentRect:window_rect styleMask:mask backing:NSBackingStoreBuffered defer:YES];
        [o_new_video_window setDelegate:o_new_video_window];

        if (b_video_wallpaper)
            [o_new_video_window setLevel:CGWindowLevelForKey(kCGDesktopWindowLevelKey) + 1];

        [o_new_video_window setBackgroundColor: [NSColor blackColor]];
        [o_new_video_window setCanBecomeKeyWindow: !b_video_wallpaper];
        [o_new_video_window setCanBecomeMainWindow: !b_video_wallpaper];
        [o_new_video_window setAcceptsMouseMovedEvents: !b_video_wallpaper];
        [o_new_video_window setMovableByWindowBackground: !b_video_wallpaper];
        [o_new_video_window useOptimizedDrawing: YES];

        o_vout_view = [[VLCVoutView alloc] initWithFrame:[[o_new_video_window contentView] bounds]];
        [o_vout_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [[o_new_video_window contentView] addSubview:o_vout_view positioned:NSWindowAbove relativeTo:nil];
        [o_new_video_window setVideoView:o_vout_view];


        if (b_video_wallpaper)
            [o_new_video_window orderBack:nil];
        else {
            // no frame autosave for additional vout windows
            if (!b_multiple_vout_windows) {
                // initial window position
                [o_new_video_window center];
                [o_new_video_window setFrameAutosaveName:@"extra-videowindow"];
            }
            
            [o_new_video_window setContentMinSize: NSMakeSize(f_min_video_height, f_min_video_height)];
        }

        [[VLCMainWindow sharedInstance] setNonembedded:YES];
        b_nonembedded = YES;
    } else {
        if ((var_InheritBool(VLCIntf, "embedded-video") && !b_multiple_vout_windows)) {
            // setup embedded video
            o_vout_view = [[[VLCMainWindow sharedInstance] videoView] retain];
            o_new_video_window = [[VLCMainWindow sharedInstance] retain];
            b_nonembedded = NO;
        } else {
            // setup detached window with controls
            NSWindowController *o_controller = [[NSWindowController alloc] initWithWindowNibName:@"DetachedVideoWindow"];
            [o_controller loadWindow];
            o_new_video_window = [(VLCDetachedVideoWindow *)[o_controller window] retain];
            [o_controller release];

            // no frame autosave for additional vout windows
            if (b_multiple_vout_windows)
                [o_new_video_window setFrameAutosaveName:@""];

            [o_new_video_window setDelegate: o_new_video_window];
            [o_new_video_window setLevel:NSNormalWindowLevel];
            [o_new_video_window useOptimizedDrawing: YES];
            o_vout_view = [[o_new_video_window videoView] retain];
            b_nonembedded = YES;
        }
    }

    if (!b_video_wallpaper) {
        // set window size
        NSSize videoViewSize = NSMakeSize(videoViewPosition.size.width, videoViewPosition.size.height);

        if (b_nonembedded) {
            NSRect window_rect = [o_new_video_window getWindowRectForProposedVideoViewSize:videoViewSize];
            [o_new_video_window setFrame:window_rect display:YES];
        }

        // cascade windows if we have more than one vout
        if (b_multiple_vout_windows) {
            if ([o_vout_dict count] == 1) {
                NSWindow * o_first_window = [o_vout_dict objectForKey: [[o_vout_dict allKeys] objectAtIndex: 0]];

                NSPoint topleftbase = NSMakePoint(0, [o_first_window frame].size.height);
                top_left_point = [o_first_window convertBaseToScreen: topleftbase];
            }

            top_left_point = [o_new_video_window cascadeTopLeftFromPoint: top_left_point];
            [o_new_video_window setFrameTopLeftPoint: top_left_point];
        }
        
        [o_new_video_window setNativeVideoSize:videoViewSize];

        [o_new_video_window makeKeyAndOrderFront: self];
    }

    [o_new_video_window setAlphaValue: config_GetFloat(VLCIntf, "macosx-opaqueness")];

    if (!b_multiple_vout_windows)
        [[VLCMainWindow sharedInstance] setNonembedded:b_nonembedded];

    [o_vout_view setVoutThread:(vout_thread_t *)p_wnd->p_parent];
    [o_new_video_window setHasActiveVideo: YES];
    [o_vout_dict setObject:[o_new_video_window autorelease] forKey:[NSValue valueWithPointer:p_wnd]];

    if (b_nonembedded) {
        // event occurs before window is created, so call again
        [[VLCMain sharedInstance] playlistUpdated];
    }

    return [o_vout_view autorelease];
}

- (void)removeVoutforDisplay:(NSValue *)o_key
{
    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:o_key];
    if (!o_window) {
        msg_Err(VLCIntf, "Cannot close nonexisting window");
        return;
    }

    if ([[VLCMainWindow sharedInstance] fullscreen] && ![[VLCMainWindow sharedInstance] nativeFullscreenMode])
        [o_window leaveFullscreen];

    if ([[VLCMainWindow sharedInstance] fullscreen] && [[VLCMainWindow sharedInstance] nativeFullscreenMode])
        [o_window toggleFullScreen: self];


    if (![NSStringFromClass([o_window class]) isEqualToString:@"VLCMainWindow"]) {
        [o_window orderOut:self];
    }

    [[o_window videoView] releaseVoutThread];
    [o_window setHasActiveVideo: NO];
    [o_vout_dict removeObjectForKey:o_key];

    if ([o_vout_dict count] == 0)
        [[VLCMain sharedInstance] setActiveVideoPlayback:NO];
}

- (void)updateWindowsControlsBarWithSelector:(SEL)aSel
{
    [o_vout_dict enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        if ([obj respondsToSelector:@selector(controlsBar)]) {
            id o_controlsBar = [obj controlsBar];
            if (o_controlsBar)
                [o_controlsBar performSelector:aSel];
        }
    }];
}

- (void)updateWindow:(vout_window_t *)p_wnd withSelector:(SEL)aSel;
{
    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:[NSValue valueWithPointer:p_wnd]];
    if (!o_window) {
        msg_Err(VLCIntf, "Cannot call selector for nonexisting window");
        return;
    }

    [o_window performSelector:aSel];
}

- (VLCVideoWindowCommon *)getWindow:(vout_window_t *)p_wnd
{
    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:[NSValue valueWithPointer:p_wnd]];
    assert(o_window);
    return o_window;

}

- (void)updateWindowsUsingBlock:(void (^)(VLCVideoWindowCommon *o_window))windowUpdater
{
    [o_vout_dict enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        if ([obj isKindOfClass: [NSWindow class]])
            windowUpdater(obj);
    }];
}

- (void)setNativeVideoSize:(NSSize)size forWindow:(vout_window_t *)p_wnd
{
    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:[NSValue valueWithPointer:p_wnd]];
    if (!o_window) {
        msg_Err(VLCIntf, "Cannot set size for nonexisting window");
        return;
    }

    [o_window setNativeVideoSize:size];
}

- (void)setWindowLevel:(NSInteger)i_level forWindow:(vout_window_t *)p_wnd
{
    // only set level for helper windows to normal if no status vout window exist anymore
    if(i_level == NSStatusWindowLevel) {
        i_statusLevelWindowCounter++;
        [self updateWindowLevelForHelperWindows:i_level];
    } else {
        i_statusLevelWindowCounter--;
        if (i_statusLevelWindowCounter == 0) {
            [self updateWindowLevelForHelperWindows:i_level];
        }
    }

    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:[NSValue valueWithPointer:p_wnd]];
    if (!o_window) {
        msg_Err(VLCIntf, "Cannot set size for nonexisting window");
        return;
    }

    [o_window setWindowLevel:i_level];
}

- (void)updateWindowLevelForHelperWindows:(NSInteger)i_level
{
    if (var_InheritBool(VLCIntf, "video-wallpaper"))
        return;

    i_currentWindowLevel = i_level;

    [[VLCMainWindow sharedInstance] setWindowLevel:i_level];
    [[VLCVideoEffects sharedInstance] updateCocoaWindowLevel:i_level];
    [[VLCAudioEffects sharedInstance] updateCocoaWindowLevel:i_level];
    [[[VLCMain sharedInstance] info] updateCocoaWindowLevel:i_level];
    [[VLCBookmarks sharedInstance] updateCocoaWindowLevel:i_level];
    [[VLCTrackSynchronization sharedInstance] updateCocoaWindowLevel:i_level];
}

@synthesize currentWindowLevel=i_currentWindowLevel;


@end
