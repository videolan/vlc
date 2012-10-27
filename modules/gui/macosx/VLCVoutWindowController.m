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

@implementation VLCVoutWindowController

- (id)init
{
    self = [super init];
    o_vout_dict = [[NSMutableDictionary alloc] init];
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


- (VLCVoutView *)setupVout:(vout_window_t *)p_wnd
{
    BOOL b_nonembedded = NO;
    BOOL b_nativeFullscreenMode = [[VLCMain sharedInstance] nativeFullscreenMode];
    BOOL b_video_deco = var_InheritBool(VLCIntf, "video-deco");
    BOOL b_video_wallpaper = var_InheritBool(VLCIntf, "video-wallpaper");
    BOOL b_multiple_vout_windows = [o_vout_dict count] > 0;
    VLCVoutView *o_vout_view;
    VLCVideoWindowCommon *o_new_video_window;

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
            [o_new_video_window center];
            [o_new_video_window setFrameAutosaveName:@"extra-videowindow"];
            [o_new_video_window setContentMinSize: NSMakeSize(f_min_video_height, f_min_video_height)];
        }

        [[VLCMainWindow sharedInstance] setNonembedded:YES];
        b_nonembedded = YES;
    } else {
        if ((var_InheritBool(VLCIntf, "embedded-video") && !b_multiple_vout_windows) || b_nativeFullscreenMode) {
            o_vout_view = [[[VLCMainWindow sharedInstance] videoView] retain];
            o_new_video_window = [[VLCMainWindow sharedInstance] retain];
            b_nonembedded = NO;
        } else {
            NSWindowController *o_controller = [[NSWindowController alloc] initWithWindowNibName:@"DetachedVideoWindow"];
            [o_controller loadWindow];
            o_new_video_window = [(VLCDetachedVideoWindow *)[o_controller window] retain];
            [o_controller release];

            [o_new_video_window setDelegate: o_new_video_window];
            [o_new_video_window setLevel:NSNormalWindowLevel];
            [o_new_video_window useOptimizedDrawing: YES];
            o_vout_view = [[o_new_video_window videoView] retain];
            b_nonembedded = YES;
        }
    }

    if (!b_video_wallpaper) {
        [o_new_video_window makeKeyAndOrderFront: self];

        vout_thread_t *p_vout = getVout();
        if (p_vout) {
            if (var_GetBool(p_vout, "video-on-top"))
                [o_new_video_window setLevel: NSStatusWindowLevel];
            else
                [o_new_video_window setLevel: NSNormalWindowLevel];
            vlc_object_release(p_vout);
        }
    }
    [o_new_video_window setAlphaValue: config_GetFloat(VLCIntf, "macosx-opaqueness")];

    if (!b_multiple_vout_windows)
        [[VLCMainWindow sharedInstance] setNonembedded:b_nonembedded];

    [o_vout_view setVoutThread:(vout_thread_t *)p_wnd->p_parent];
    [o_vout_dict setObject:[o_new_video_window autorelease] forKey:[NSValue valueWithPointer:p_wnd]];

    if(b_nonembedded) {
        // event occurs before window is created, so call again
        [[VLCMain sharedInstance] playlistUpdated];
    }

    return [o_vout_view autorelease];
}

- (void)removeVoutforDisplay:(NSValue *)o_key
{
    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:o_key];
    if(!o_window) {
        msg_Err(VLCIntf, "Cannot close nonexisting window");
        return;
    }

    if ([[VLCMainWindow sharedInstance] fullscreen] && ![[VLCMainWindow sharedInstance] nativeFullscreenMode])
        [o_window leaveFullscreen];

    if (![NSStringFromClass([o_window class]) isEqualToString:@"VLCMainWindow"]) {
        [o_window orderOut:self];
    }

    [[o_window videoView] releaseVoutThread];
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
    if(!o_window) {
        msg_Err(VLCIntf, "Cannot call selector for nonexisting window");
        return;
    }

    [o_window performSelector:aSel];
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
    if(!o_window) {
        msg_Err(VLCIntf, "Cannot set size for nonexisting window");
        return;
    }

    [o_window setNativeVideoSize:size];
}

@end