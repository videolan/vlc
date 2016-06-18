/*****************************************************************************
 * VLCVoutWindowController.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2014 VLC authors and VideoLAN
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

#include <vlc_vout_display.h>

#import "CompatibilityFixes.h"
#import "VLCVoutWindowController.h"
#import "intf.h"
#import "MainWindow.h"
#import "VideoView.h"

#import "VideoEffects.h"
#import "AudioEffects.h"
#import "VLCPlaylistInfo.h"
#import "bookmarks.h"
#import "TrackSynchronization.h"
#import "ResumeDialogController.h"
#import "VLCPlaylist.h"

static atomic_bool b_intf_starting = ATOMIC_VAR_INIT(false);

static int WindowControl(vout_window_t *, int i_query, va_list);

int WindowOpen(vout_window_t *p_wnd, const vout_window_cfg_t *cfg)
{
    @autoreleasepool {
        if (cfg->type != VOUT_WINDOW_TYPE_INVALID
            && cfg->type != VOUT_WINDOW_TYPE_NSOBJECT)
            return VLC_EGENERIC;

        msg_Dbg(p_wnd, "Opening video window");

        if (!atomic_load(&b_intf_starting)) {
            msg_Err(p_wnd, "Cannot create vout as Mac OS X interface was not found");
            return VLC_EGENERIC;
        }

        NSRect proposedVideoViewPosition = NSMakeRect(cfg->x, cfg->y, cfg->width, cfg->height);

        VLCVoutWindowController *voutController = [[VLCMain sharedInstance] voutController];
        if (!voutController) {
            return VLC_EGENERIC;
        }
        [voutController.lock lock];

        SEL sel = @selector(setupVoutForWindow:withProposedVideoViewPosition:);
        NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[voutController methodSignatureForSelector:sel]];
        [inv setTarget:voutController];
        [inv setSelector:sel];
        [inv setArgument:&p_wnd atIndex:2]; // starting at 2!
        [inv setArgument:&proposedVideoViewPosition atIndex:3];

        [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                           waitUntilDone:YES];

        VLCVoutView *videoView = nil;
        [inv getReturnValue:&videoView];

        // this method is not supposed to fail
        assert(videoView != nil);

        msg_Dbg(getIntf(), "returning videoview with proposed position x=%i, y=%i, width=%i, height=%i", cfg->x, cfg->y, cfg->width, cfg->height);
        p_wnd->handle.nsobject = (void *)CFBridgingRetain(videoView);

        [voutController.lock unlock];

        p_wnd->type = VOUT_WINDOW_TYPE_NSOBJECT;
        p_wnd->control = WindowControl;
    }
    vout_window_SetFullScreen(p_wnd, cfg->is_fullscreen);
    return VLC_SUCCESS;
}

static int WindowControl(vout_window_t *p_wnd, int i_query, va_list args)
{
    @autoreleasepool {
        VLCVoutWindowController *voutController = [[VLCMain sharedInstance] voutController];
        if (!voutController) {
            return VLC_EGENERIC;
        }
        [voutController.lock lock];

        switch(i_query) {
            case VOUT_WINDOW_SET_STATE:
            {
                unsigned i_state = va_arg(args, unsigned);

                if (i_state & VOUT_WINDOW_STATE_BELOW)
                {
                    msg_Dbg(p_wnd, "Ignore change to VOUT_WINDOW_STATE_BELOW");
                    goto out;
                }

                NSInteger i_cooca_level = NSNormalWindowLevel;
                if (i_state & VOUT_WINDOW_STATE_ABOVE)
                    i_cooca_level = NSStatusWindowLevel;

                SEL sel = @selector(setWindowLevel:forWindow:);
                NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[voutController methodSignatureForSelector:sel]];
                [inv setTarget:voutController];
                [inv setSelector:sel];
                [inv setArgument:&i_cooca_level atIndex:2]; // starting at 2!
                [inv setArgument:&p_wnd atIndex:3];
                [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                                   waitUntilDone:NO];

                break;
            }
            case VOUT_WINDOW_SET_SIZE:
            {
                unsigned int i_width  = va_arg(args, unsigned int);
                unsigned int i_height = va_arg(args, unsigned int);

                NSSize newSize = NSMakeSize(i_width, i_height);
                SEL sel = @selector(setNativeVideoSize:forWindow:);
                NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[voutController methodSignatureForSelector:sel]];
                [inv setTarget:voutController];
                [inv setSelector:sel];
                [inv setArgument:&newSize atIndex:2]; // starting at 2!
                [inv setArgument:&p_wnd atIndex:3];
                [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                                   waitUntilDone:NO];

                break;
            }
            case VOUT_WINDOW_SET_FULLSCREEN:
            {
                if (var_InheritBool(getIntf(), "video-wallpaper")) {
                    msg_Dbg(p_wnd, "Ignore fullscreen event as video-wallpaper is on");
                    goto out;
                }

                int i_full = va_arg(args, int);
                BOOL b_animation = YES;

                SEL sel = @selector(setFullscreen:forWindow:withAnimation:);
                NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[voutController methodSignatureForSelector:sel]];
                [inv setTarget:voutController];
                [inv setSelector:sel];
                [inv setArgument:&i_full atIndex:2]; // starting at 2!
                [inv setArgument:&p_wnd atIndex:3];
                [inv setArgument:&b_animation atIndex:4];
                [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                                   waitUntilDone:NO];

                break;
            }
            default:
            {
                msg_Warn(p_wnd, "unsupported control query");
                [voutController.lock unlock];
                return VLC_EGENERIC;
            }
        }

        out:
        [voutController.lock unlock];
        return VLC_SUCCESS;
    }
}

void WindowClose(vout_window_t *p_wnd)
{
    @autoreleasepool {
        VLCVoutWindowController *voutController = [[VLCMain sharedInstance] voutController];
        if (!voutController) {
            return;
        }

        [voutController.lock lock];
        [voutController performSelectorOnMainThread:@selector(removeVoutforDisplay:) withObject:[NSValue valueWithPointer:p_wnd] waitUntilDone:NO];
        [voutController.lock unlock];
    }
}

@interface VLCVoutWindowController ()
{
    NSMutableDictionary *o_vout_dict;
    KeyboardBacklight *o_keyboard_backlight;

    NSPoint top_left_point;

    // save the status level if at least one video window is on status level
    NSUInteger i_statusLevelWindowCounter;
    NSInteger i_currentWindowLevel;

    BOOL b_mainwindow_has_video;
}
@end

@implementation VLCVoutWindowController

- (id)init
{
    self = [super init];
    if (self) {
        atomic_store(&b_intf_starting, true);
        o_vout_dict = [[NSMutableDictionary alloc] init];
        o_keyboard_backlight = [[KeyboardBacklight alloc] init];
        i_currentWindowLevel = NSNormalWindowLevel;
        _currentStatusWindowLevel = NSFloatingWindowLevel;
    }
    return self;
}

- (void)dealloc
{
    NSArray *keys = [o_vout_dict allKeys];
    for (NSValue *key in keys)
        [self removeVoutforDisplay:key];

    if (var_InheritBool(getIntf(), "macosx-dim-keyboard")) {
        [o_keyboard_backlight switchLightsInstantly:YES];
    }
}

#pragma mark -
#pragma mark Methods for vout provider

- (VLCVoutView *)setupVoutForWindow:(vout_window_t *)p_wnd withProposedVideoViewPosition:(NSRect)videoViewPosition
{
    BOOL b_nonembedded = NO;
    BOOL b_nativeFullscreenMode = [[VLCMain sharedInstance] nativeFullscreenMode];
    BOOL b_video_deco = var_InheritBool(getIntf(), "video-deco");
    BOOL b_video_wallpaper = var_InheritBool(getIntf(), "video-wallpaper");
    BOOL b_multiple_vout_windows = [o_vout_dict count] > 0;
    VLCVoutView *o_vout_view;
    VLCVideoWindowCommon *o_new_video_window;

    // should be called before any window resizing occurs
    if (!b_multiple_vout_windows)
        [[[VLCMain sharedInstance] mainWindow] videoplayWillBeStarted];

    if (b_multiple_vout_windows && b_video_wallpaper)
        b_video_wallpaper = false;

    // TODO: make lion fullscreen compatible with video-wallpaper
    if ((b_video_wallpaper || !b_video_deco) && !b_nativeFullscreenMode) {
        // b_video_wallpaper is priorized over !b_video_deco

        msg_Dbg(getIntf(), "Creating background / blank window");
        NSScreen *screen = [NSScreen screenWithDisplayID:(CGDirectDisplayID)var_InheritInteger(getIntf(), "macosx-vdev")];
        if (!screen)
            screen = [[[VLCMain sharedInstance] mainWindow] screen];

        NSRect window_rect;
        if (b_video_wallpaper)
            window_rect = [screen frame];
        else
            window_rect = [[[VLCMain sharedInstance] mainWindow] frame];

        NSUInteger mask = NSBorderlessWindowMask;
        if (!b_video_deco)
            mask |= NSResizableWindowMask;

        BOOL b_no_video_deco_only = !b_video_wallpaper;
        o_new_video_window = [[VLCVideoWindowCommon alloc] initWithContentRect:window_rect styleMask:mask backing:NSBackingStoreBuffered defer:YES];
        [o_new_video_window setDelegate:o_new_video_window];
        [o_new_video_window setReleasedWhenClosed: NO];

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

        b_nonembedded = YES;
    } else {
        if ((var_InheritBool(getIntf(), "embedded-video") && !b_mainwindow_has_video)) {
            // setup embedded video
            o_new_video_window = [[VLCMain sharedInstance] mainWindow] ;
            o_vout_view = [o_new_video_window videoView];
            b_mainwindow_has_video = YES;
            b_nonembedded = NO;
        } else {
            // setup detached window with controls
            NSWindowController *o_controller = [[NSWindowController alloc] initWithWindowNibName:@"DetachedVideoWindow"];
            [o_controller loadWindow];
            o_new_video_window = (VLCDetachedVideoWindow *)[o_controller window];

            // no frame autosave for additional vout windows
            if (b_multiple_vout_windows)
                [o_new_video_window setFrameAutosaveName:@""];

            [o_new_video_window setDelegate: o_new_video_window];
            [o_new_video_window setLevel:NSNormalWindowLevel];
            [o_new_video_window useOptimizedDrawing: YES];
            o_vout_view = [o_new_video_window videoView];
            b_nonembedded = YES;
        }
    }

    NSSize videoViewSize = NSMakeSize(videoViewPosition.size.width, videoViewPosition.size.height);

    // Avoid flashes if video will directly start in fullscreen
    NSDisableScreenUpdates();

    if (!b_video_wallpaper) {
        // set (only!) window origin if specified
        if (b_nonembedded) {
            NSRect window_rect = [o_new_video_window frame];
            if (videoViewPosition.origin.x > 0.)
                window_rect.origin.x = videoViewPosition.origin.x;
            if (videoViewPosition.origin.y > 0.)
                window_rect.origin.y = videoViewPosition.origin.y;

            [o_new_video_window setFrame:window_rect display:YES];
        }

        // cascade windows if we have more than one vout
        if (b_multiple_vout_windows) {
            if ([o_vout_dict count] == 1) {
                NSWindow * o_first_window = [o_vout_dict objectForKey: [[o_vout_dict allKeys] firstObject]];

                NSRect topleftBaseRect = NSMakeRect(0, [o_first_window frame].size.height, 0, 0);
                top_left_point = [o_first_window convertRectToScreen: topleftBaseRect].origin;
            }

            top_left_point = [o_new_video_window cascadeTopLeftFromPoint: top_left_point];
            [o_new_video_window setFrameTopLeftPoint: top_left_point];
        }

        // resize window
        [o_new_video_window setNativeVideoSize:videoViewSize];

        [o_new_video_window makeKeyAndOrderFront: self];
    }

    [o_new_video_window setAlphaValue: config_GetFloat(getIntf(), "macosx-opaqueness")];

    [o_vout_view setVoutThread:(vout_thread_t *)p_wnd->obj.parent];
    [o_new_video_window setHasActiveVideo: YES];
    [o_vout_dict setObject:o_new_video_window forKey:[NSValue valueWithPointer:p_wnd]];

    [[VLCMain sharedInstance] setActiveVideoPlayback: YES];
    [[[VLCMain sharedInstance] mainWindow] setNonembedded:!b_mainwindow_has_video];

    // beware of order, setActiveVideoPlayback:, setHasActiveVideo: and setNonembedded: must be called before
    if ([o_new_video_window class] == [VLCMainWindow class])
        [[[VLCMain sharedInstance] mainWindow] changePlaylistState: psVideoStartedOrStoppedEvent];

    if (b_nonembedded) {
        // events might be posted before window is created, so call them again
        [[[VLCMain sharedInstance] mainWindow] updateName];
        [[[VLCMain sharedInstance] mainWindow] updateWindow]; // update controls bar
    }

    // TODO: find a cleaner way for "start in fullscreen"
    // Start in fs, because either prefs settings, or fullscreen button was pressed before
    char *psz_splitter = var_GetString(pl_Get(getIntf()), "video-splitter");
    BOOL b_have_splitter = psz_splitter != NULL && *psz_splitter != '\0';
    free(psz_splitter);

    if (!b_video_wallpaper && !b_have_splitter && (var_InheritBool(getIntf(), "fullscreen") || var_GetBool(pl_Get(getIntf()), "fullscreen"))) {

        // this is not set when we start in fullscreen because of
        // fullscreen settings in video prefs the second time
        var_SetBool(p_wnd->obj.parent, "fullscreen", 1);

        [self setFullscreen:1 forWindow:p_wnd withAnimation:NO];
    }

    NSEnableScreenUpdates();

    return o_vout_view;
}

- (void)removeVoutforDisplay:(NSValue *)o_key
{
    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:o_key];
    if (!o_window) {
        msg_Err(getIntf(), "Cannot close nonexisting window");
        return;
    }

    [[o_window videoView] releaseVoutThread];

    // set active video to no BEFORE closing the window and exiting fullscreen
    // (avoid stopping playback due to NSWindowWillCloseNotification, preserving fullscreen state)
    [o_window setHasActiveVideo: NO];

    // prevent visible extra window if in fullscreen
    NSDisableScreenUpdates();
    BOOL b_native = [[[VLCMain sharedInstance] mainWindow] nativeFullscreenMode];

    // close fullscreen, without changing fullscreen vars
    if (!b_native && ([o_window fullscreen] || [o_window inFullscreenTransition]))
        [o_window leaveFullscreenWithAnimation:NO];

    // native fullscreen window will not be closed if
    // fullscreen was triggered without video
    if ((b_native && [o_window class] == [VLCMainWindow class] && [o_window fullscreen] && [o_window windowShouldExitFullscreenWhenFinished])) {
        [o_window toggleFullScreen:self];
    }

    if ([o_window class] != [VLCMainWindow class]) {
        [o_window close];
    }
    NSEnableScreenUpdates();

    [o_vout_dict removeObjectForKey:o_key];
    if ([o_vout_dict count] == 0) {
        [[VLCMain sharedInstance] setActiveVideoPlayback:NO];
        i_statusLevelWindowCounter = 0;
    }

    if ([o_window class] == [VLCMainWindow class]) {
        b_mainwindow_has_video = NO;

        // video in main window might get stopped while another vout is open
        if ([o_vout_dict count] > 0)
            [[[VLCMain sharedInstance] mainWindow] setNonembedded:YES];

        // beware of order, setActiveVideoPlayback:, setHasActiveVideo: and setNonembedded: must be called before
        [[[VLCMain sharedInstance] mainWindow] changePlaylistState: psVideoStartedOrStoppedEvent];
    }
}


- (void)setNativeVideoSize:(NSSize)size forWindow:(vout_window_t *)p_wnd
{
    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:[NSValue valueWithPointer:p_wnd]];
    if (!o_window) {
        msg_Err(getIntf(), "Cannot set size for nonexisting window");
        return;
    }

    [o_window setNativeVideoSize:size];
}

- (void)setWindowLevel:(NSInteger)i_level forWindow:(vout_window_t *)p_wnd
{
    VLCVideoWindowCommon *o_window = [o_vout_dict objectForKey:[NSValue valueWithPointer:p_wnd]];
    if (!o_window) {
        msg_Err(getIntf(), "Cannot set level for nonexisting window");
        return;
    }

    // only set level for helper windows to normal if no status vout window exist anymore
    if(i_level == NSStatusWindowLevel) {
        i_statusLevelWindowCounter++;
        // window level need to stay on normal in fullscreen mode
        if (![o_window fullscreen] && ![o_window inFullscreenTransition])
            [self updateWindowLevelForHelperWindows:i_level];
    } else {
        if (i_statusLevelWindowCounter > 0)
            i_statusLevelWindowCounter--;

        if (i_statusLevelWindowCounter == 0) {
            [self updateWindowLevelForHelperWindows:i_level];
        }
    }

    [o_window setWindowLevel:i_level];
}

- (void)setFullscreen:(int)i_full forWindow:(vout_window_t *)p_wnd withAnimation:(BOOL)b_animation
{
    intf_thread_t *p_intf = getIntf();
    BOOL b_nativeFullscreenMode = [[VLCMain sharedInstance] nativeFullscreenMode];

    if (!p_intf || (!b_nativeFullscreenMode && !p_wnd))
        return;
    playlist_t *p_playlist = pl_Get(p_intf);
    BOOL b_fullscreen = i_full != 0;

    if (!var_GetBool(p_playlist, "fullscreen") != !b_fullscreen)
        var_SetBool(p_playlist, "fullscreen", b_fullscreen);

    VLCVideoWindowCommon *o_current_window = nil;
    if(p_wnd)
        o_current_window = [o_vout_dict objectForKey:[NSValue valueWithPointer:p_wnd]];

    if (var_InheritBool(p_intf, "macosx-dim-keyboard")) {
        [o_keyboard_backlight switchLightsAsync:!b_fullscreen];
    }

    if (b_nativeFullscreenMode) {
        if(!o_current_window)
            o_current_window = [[VLCMain sharedInstance] mainWindow] ;
        assert(o_current_window);

        // fullscreen might be triggered twice (vout event)
        // so ignore duplicate events here
        if((b_fullscreen && !([o_current_window fullscreen] || [o_current_window inFullscreenTransition])) ||
           (!b_fullscreen && [o_current_window fullscreen])) {

            [o_current_window toggleFullScreen:self];
        }
    } else {
        assert(o_current_window);

        if (b_fullscreen) {
            input_thread_t * p_input = pl_CurrentInput(p_intf);
            if (p_input != NULL && [[VLCMain sharedInstance] activeVideoPlayback]) {
                // activate app, as method can also be triggered from outside the app (prevents nasty window layout)
                [NSApp activateIgnoringOtherApps:YES];
                [o_current_window enterFullscreenWithAnimation:b_animation];

            }
            if (p_input)
                vlc_object_release(p_input);
        } else {
            // leaving fullscreen is always allowed
            [o_current_window leaveFullscreenWithAnimation:YES];
        }
    }
}

#pragma mark -
#pragma mark Misc methods

- (void)updateControlsBarsUsingBlock:(void (^)(VLCControlsBarCommon *controlsBar))block
{
    [o_vout_dict enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {

        if ([obj respondsToSelector:@selector(controlsBar)]) {
            VLCControlsBarCommon *o_controlsBar = [obj controlsBar];
            if (o_controlsBar && block)
                block(o_controlsBar);
        }
    }];
}

- (void)updateWindowsUsingBlock:(void (^)(VLCVideoWindowCommon *o_window))windowUpdater
{
    [o_vout_dict enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        if ([obj isKindOfClass: [NSWindow class]])
            windowUpdater(obj);
    }];
}

- (void)updateWindowLevelForHelperWindows:(NSInteger)i_level
{
    if (var_InheritBool(getIntf(), "video-wallpaper"))
        return;

    i_currentWindowLevel = i_level;
    if (i_level == NSNormalWindowLevel) {
        _currentStatusWindowLevel = NSFloatingWindowLevel;
    } else {
        _currentStatusWindowLevel = i_level + 1;
    }

    NSInteger currentStatusWindowLevel = self.currentStatusWindowLevel;

    VLCMain *main = [VLCMain sharedInstance];
    [[[VLCMain sharedInstance] mainWindow] setWindowLevel:i_level];
    [[main videoEffectsPanel] updateCocoaWindowLevel:currentStatusWindowLevel];
    [[main audioEffectsPanel] updateCocoaWindowLevel:currentStatusWindowLevel];
    [[main currentMediaInfoPanel] updateCocoaWindowLevel:currentStatusWindowLevel];
    [[main bookmarks] updateCocoaWindowLevel:currentStatusWindowLevel];
    [[main trackSyncPanel] updateCocoaWindowLevel:currentStatusWindowLevel];
    [[main resumeDialog] updateCocoaWindowLevel:currentStatusWindowLevel];
}

@end
