/*****************************************************************************
 * VLCVideoOutputProvider.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2014 VLC authors and VideoLAN
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

#import "VLCVideoOutputProvider.h"

#include <vlc_vout_display.h>

#import "extensions/NSScreen+VLCAdditions.h"

#import "main/CompatibilityFixes.h"
#import "main/VLCMain.h"
#import "windows/mainwindow/VLCMainWindow.h"
#import "windows/video/VLCDetachedVideoWindow.h"
#import "windows/video/VLCVoutView.h"

#import "panels/dialogs/VLCResumeDialogController.h"
#import "panels/VLCVideoEffectsWindowController.h"
#import "panels/VLCAudioEffectsWindowController.h"
#import "panels/VLCInformationWindowController.h"
#import "panels/VLCBookmarksWindowController.h"
#import "panels/VLCTrackSynchronizationWindowController.h"

static int WindowEnable(vout_window_t *p_wnd, const vout_window_cfg_t *cfg)
{
    @autoreleasepool {
        msg_Dbg(p_wnd, "Opening video window");

        NSRect proposedVideoViewPosition = NSMakeRect(cfg->x, cfg->y, cfg->width, cfg->height);

        VLCVideoOutputProvider *voutProvider = [[VLCMain sharedInstance] voutProvider];
        if (!voutProvider) {
            return VLC_EGENERIC;
        }

        __block VLCVoutView *videoView = nil;

        dispatch_sync(dispatch_get_main_queue(), ^{
            videoView = [voutProvider setupVoutForWindow:p_wnd
                             withProposedVideoViewPosition:proposedVideoViewPosition];
        });

        // this method is not supposed to fail
        assert(videoView != nil);

        msg_Dbg(getIntf(), "returning videoview with proposed position x=%i, y=%i, width=%i, height=%i", cfg->x, cfg->y, cfg->width, cfg->height);
        p_wnd->handle.nsobject = (void *)CFBridgingRetain(videoView);
    }
    if (cfg->is_fullscreen)
        vout_window_SetFullScreen(p_wnd, NULL);
    return VLC_SUCCESS;
}

static void WindowDisable(vout_window_t *p_wnd)
{
    @autoreleasepool {
        VLCVideoOutputProvider *voutProvider = [[VLCMain sharedInstance] voutProvider];

        dispatch_async(dispatch_get_main_queue(), ^{
            [voutProvider removeVoutForDisplay:[NSValue valueWithPointer:p_wnd]];
        });
    }
}

static void WindowResize(vout_window_t *p_wnd,
                         unsigned i_width, unsigned i_height)
{
    @autoreleasepool {
        VLCVideoOutputProvider *voutProvider = [[VLCMain sharedInstance] voutProvider];

        dispatch_async(dispatch_get_main_queue(), ^{
            [voutProvider setNativeVideoSize:NSMakeSize(i_width, i_height)
                          forWindow:p_wnd];
        });
    }
}

static void WindowSetState(vout_window_t *p_wnd, unsigned i_state)
{
    if (i_state & VOUT_WINDOW_STATE_BELOW)
        msg_Dbg(p_wnd, "Ignore change to VOUT_WINDOW_STATE_BELOW");

    @autoreleasepool {
        VLCVideoOutputProvider *voutProvider = [[VLCMain sharedInstance] voutProvider];

        NSInteger i_cooca_level = NSNormalWindowLevel;

        if (i_state & VOUT_WINDOW_STATE_ABOVE)
            i_cooca_level = NSStatusWindowLevel;

        dispatch_async(dispatch_get_main_queue(), ^{
            [voutProvider setWindowLevel:i_cooca_level forWindow:p_wnd];
        });
    }
}

static const char windowed;

static void WindowSetFullscreen(vout_window_t *p_wnd, const char *psz_id)
{
    if (var_InheritBool(getIntf(), "video-wallpaper")) {
        msg_Dbg(p_wnd, "Ignore fullscreen event as video-wallpaper is on");
        return;
    }

    int i_full = psz_id != &windowed;
    BOOL b_animation = YES;

    @autoreleasepool {
        VLCVideoOutputProvider *voutProvider = [[VLCMain sharedInstance] voutProvider];

        dispatch_async(dispatch_get_main_queue(), ^{
            [voutProvider setFullscreen:i_full
                          forWindow:p_wnd
                          withAnimation:b_animation];
        });
    }
}

static void WindowUnsetFullscreen(vout_window_t *wnd)
{
    WindowSetFullscreen(wnd, &windowed);
}

static atomic_bool b_intf_starting = ATOMIC_VAR_INIT(false);

static const struct vout_window_operations ops = {
    WindowEnable,
    WindowDisable,
    WindowResize,
    NULL,
    WindowSetState,
    WindowUnsetFullscreen,
    WindowSetFullscreen,
};

int WindowOpen(vout_window_t *p_wnd)
{
    if (!atomic_load(&b_intf_starting)) {
        msg_Err(p_wnd, "Cannot create vout as Mac OS X interface was not found");
        return VLC_EGENERIC;
    }

    p_wnd->type = VOUT_WINDOW_TYPE_NSOBJECT;
    p_wnd->ops = &ops;
    return VLC_SUCCESS;
}

@interface VLCVideoOutputProvider ()
{
    NSMutableDictionary *voutWindows;
    VLCKeyboardBacklightControl *keyboardBacklight;

    NSPoint topLeftPoint;

    // save the status level if at least one video window is on status level
    NSUInteger statusLevelWindowCounter;
    NSInteger currentWindowLevel;

    BOOL mainWindowHasVideo;
}
@end

@implementation VLCVideoOutputProvider

- (id)init
{
    self = [super init];
    if (self) {
        atomic_store(&b_intf_starting, true);
        voutWindows = [[NSMutableDictionary alloc] init];
        keyboardBacklight = [[VLCKeyboardBacklightControl alloc] init];
        currentWindowLevel = NSNormalWindowLevel;
        _currentStatusWindowLevel = NSFloatingWindowLevel;
    }
    return self;
}

- (void)dealloc
{
    NSArray *keys = [voutWindows allKeys];
    for (NSValue *key in keys)
        [self removeVoutForDisplay:key];

    if (var_InheritBool(getIntf(), "macosx-dim-keyboard")) {
        [keyboardBacklight switchLightsInstantly:YES];
    }
}

#pragma mark -
#pragma mark Methods for vout provider

- (VLCVoutView *)setupVoutForWindow:(vout_window_t *)p_wnd withProposedVideoViewPosition:(NSRect)videoViewPosition
{
    BOOL isEmbedded = YES;
    BOOL isNativeFullscreen = [[VLCMain sharedInstance] nativeFullscreenMode];
    BOOL windowDecorations = var_InheritBool(getIntf(), "video-deco");
    BOOL videoWallpaper = var_InheritBool(getIntf(), "video-wallpaper");
    BOOL multipleVoutWindows = [voutWindows count] > 0;
    VLCVoutView *voutView;
    VLCVideoWindowCommon *newVideoWindow;

    // should be called before any window resizing occurs
    if (!multipleVoutWindows)
        [[[VLCMain sharedInstance] mainWindow] videoplayWillBeStarted];

    if (multipleVoutWindows && videoWallpaper)
        videoWallpaper = false;

    // TODO: make lion fullscreen compatible with video-wallpaper
    if ((videoWallpaper || !windowDecorations) && !isNativeFullscreen) {
        // videoWallpaper is priorized over !windowDecorations

        msg_Dbg(getIntf(), "Creating background / blank window");
        NSScreen *screen = [NSScreen screenWithDisplayID:(CGDirectDisplayID)var_InheritInteger(getIntf(), "macosx-vdev")];
        if (!screen)
            screen = [[[VLCMain sharedInstance] mainWindow] screen];

        NSRect window_rect;
        if (videoWallpaper)
            window_rect = [screen frame];
        else
            window_rect = [[[VLCMain sharedInstance] mainWindow] frame];

        NSUInteger mask = NSBorderlessWindowMask;
        if (!windowDecorations)
            mask |= NSResizableWindowMask;

        newVideoWindow = [[VLCVideoWindowCommon alloc] initWithContentRect:window_rect styleMask:mask backing:NSBackingStoreBuffered defer:YES];
        [newVideoWindow setDelegate:newVideoWindow];
        [newVideoWindow setReleasedWhenClosed: NO];

        if (videoWallpaper)
            [newVideoWindow setLevel:CGWindowLevelForKey(kCGDesktopWindowLevelKey) + 1];

        [newVideoWindow setBackgroundColor: [NSColor blackColor]];
        [newVideoWindow setCanBecomeKeyWindow: !videoWallpaper];
        [newVideoWindow setCanBecomeMainWindow: !videoWallpaper];
        [newVideoWindow setAcceptsMouseMovedEvents: !videoWallpaper];
        [newVideoWindow setMovableByWindowBackground: !videoWallpaper];

        voutView = [[VLCVoutView alloc] initWithFrame:[[newVideoWindow contentView] bounds]];
        [voutView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [[newVideoWindow contentView] addSubview:voutView positioned:NSWindowAbove relativeTo:nil];
        [newVideoWindow setVideoView:voutView];


        if (videoWallpaper)
            [newVideoWindow orderBack:nil];
        else {
            // no frame autosave for additional vout windows
            if (!multipleVoutWindows) {
                // initial window position
                [newVideoWindow center];
                [newVideoWindow setFrameAutosaveName:@"extra-videowindow"];
            }

            [newVideoWindow setContentMinSize: NSMakeSize(f_min_video_height, f_min_video_height)];
        }

        isEmbedded = NO;
    } else {
        if ((var_InheritBool(getIntf(), "embedded-video") && !mainWindowHasVideo)) {
            // setup embedded video
            newVideoWindow = [[VLCMain sharedInstance] mainWindow] ;
            voutView = [newVideoWindow videoView];
            mainWindowHasVideo = YES;
            isEmbedded = YES;
        } else {
            // setup detached window with controls
            NSWindowController *o_controller = [[NSWindowController alloc] initWithWindowNibName:@"DetachedVideoWindow"];
            [o_controller loadWindow];
            newVideoWindow = (VLCDetachedVideoWindow *)[o_controller window];

            // no frame autosave for additional vout windows
            if (multipleVoutWindows)
                [newVideoWindow setFrameAutosaveName:@""];

            [newVideoWindow setDelegate: newVideoWindow];
            [newVideoWindow setLevel:NSNormalWindowLevel];
            voutView = [newVideoWindow videoView];
            isEmbedded = NO;
        }
    }

    NSSize videoViewSize = NSMakeSize(videoViewPosition.size.width, videoViewPosition.size.height);

    // Avoid flashes if video will directly start in fullscreen
    NSDisableScreenUpdates();

    if (!videoWallpaper) {
        // set (only!) window origin if specified
        if (!isEmbedded) {
            NSRect window_rect = [newVideoWindow frame];
            if (videoViewPosition.origin.x > 0.)
                window_rect.origin.x = videoViewPosition.origin.x;
            if (videoViewPosition.origin.y > 0.)
                window_rect.origin.y = videoViewPosition.origin.y;

            [newVideoWindow setFrame:window_rect display:YES];
        }

        // cascade windows if we have more than one vout
        if (multipleVoutWindows) {
            if ([voutWindows count] == 1) {
                NSWindow * firstWindow = [voutWindows objectForKey: [[voutWindows allKeys] firstObject]];

                NSRect topleftBaseRect = NSMakeRect(0, [firstWindow frame].size.height, 0, 0);
                topLeftPoint = [firstWindow convertRectToScreen: topleftBaseRect].origin;
            }

            topLeftPoint = [newVideoWindow cascadeTopLeftFromPoint: topLeftPoint];
            [newVideoWindow setFrameTopLeftPoint: topLeftPoint];
        }

        // resize window
        [newVideoWindow setNativeVideoSize:videoViewSize];

        [newVideoWindow makeKeyAndOrderFront: self];
    }

    [newVideoWindow setAlphaValue: config_GetFloat("macosx-opaqueness")];

    [voutView setVoutThread:(vout_thread_t *)vlc_object_parent(p_wnd)];
    [newVideoWindow setHasActiveVideo: YES];
    [voutWindows setObject:newVideoWindow forKey:[NSValue valueWithPointer:p_wnd]];

    [[VLCMain sharedInstance] setActiveVideoPlayback: YES];
    [[[VLCMain sharedInstance] mainWindow] setNonembedded:!mainWindowHasVideo];

    // beware of order, setActiveVideoPlayback:, setHasActiveVideo: and setNonembedded: must be called before
    if ([newVideoWindow class] == [VLCMainWindow class])
        [[[VLCMain sharedInstance] mainWindow] changePlaylistState: psVideoStartedOrStoppedEvent];

    if (!isEmbedded) {
        // events might be posted before window is created, so call them again
        [[[VLCMain sharedInstance] mainWindow] updateName];
        [[[VLCMain sharedInstance] mainWindow] updateWindow]; // update controls bar
    }

    // TODO: find a cleaner way for "start in fullscreen"
    // Start in fs, because either prefs settings, or fullscreen button was pressed before
    char *psz_splitter = var_GetString(pl_Get(getIntf()), "video-splitter");
    BOOL b_have_splitter = psz_splitter != NULL && *psz_splitter != '\0';
    free(psz_splitter);

    if (!videoWallpaper && !b_have_splitter && (var_InheritBool(getIntf(), "fullscreen") || var_GetBool(pl_Get(getIntf()), "fullscreen"))) {

        // this is not set when we start in fullscreen because of
        // fullscreen settings in video prefs the second time
        var_SetBool(vlc_object_parent(p_wnd), "fullscreen", 1);

        [self setFullscreen:1 forWindow:p_wnd withAnimation:NO];
    }

    NSEnableScreenUpdates();

    return voutView;
}

- (void)removeVoutForDisplay:(NSValue *)o_key
{
    VLCVideoWindowCommon *o_window = [voutWindows objectForKey:o_key];
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

    [voutWindows removeObjectForKey:o_key];
    if ([voutWindows count] == 0) {
        [[VLCMain sharedInstance] setActiveVideoPlayback:NO];
        statusLevelWindowCounter = 0;
    }

    if ([o_window class] == [VLCMainWindow class]) {
        mainWindowHasVideo = NO;

        // video in main window might get stopped while another vout is open
        if ([voutWindows count] > 0)
            [[[VLCMain sharedInstance] mainWindow] setNonembedded:YES];

        // beware of order, setActiveVideoPlayback:, setHasActiveVideo: and setNonembedded: must be called before
        [[[VLCMain sharedInstance] mainWindow] changePlaylistState: psVideoStartedOrStoppedEvent];
    }
}


- (void)setNativeVideoSize:(NSSize)size forWindow:(vout_window_t *)p_wnd
{
    VLCVideoWindowCommon *o_window = [voutWindows objectForKey:[NSValue valueWithPointer:p_wnd]];
    if (!o_window) {
        msg_Err(getIntf(), "Cannot set size for nonexisting window");
        return;
    }

    [o_window setNativeVideoSize:size];
}

- (void)setWindowLevel:(NSInteger)i_level forWindow:(vout_window_t *)p_wnd
{
    VLCVideoWindowCommon *o_window = [voutWindows objectForKey:[NSValue valueWithPointer:p_wnd]];
    if (!o_window) {
        msg_Err(getIntf(), "Cannot set level for nonexisting window");
        return;
    }

    // only set level for helper windows to normal if no status vout window exist anymore
    if(i_level == NSStatusWindowLevel) {
        statusLevelWindowCounter++;
        // window level need to stay on normal in fullscreen mode
        if (![o_window fullscreen] && ![o_window inFullscreenTransition])
            [self updateWindowLevelForHelperWindows:i_level];
    } else {
        if (statusLevelWindowCounter > 0)
            statusLevelWindowCounter--;

        if (statusLevelWindowCounter == 0) {
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
        o_current_window = [voutWindows objectForKey:[NSValue valueWithPointer:p_wnd]];

    if (var_InheritBool(p_intf, "macosx-dim-keyboard")) {
        [keyboardBacklight switchLightsAsync:!b_fullscreen];
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
                input_Release(p_input);
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
    [voutWindows enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {

        if ([obj respondsToSelector:@selector(controlsBar)]) {
            VLCControlsBarCommon *o_controlsBar = [obj controlsBar];
            if (o_controlsBar && block)
                block(o_controlsBar);
        }
    }];
}

- (void)updateWindowsUsingBlock:(void (^)(VLCVideoWindowCommon *o_window))windowUpdater
{
    [voutWindows enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        if ([obj isKindOfClass: [NSWindow class]])
            windowUpdater(obj);
    }];
}

- (void)updateWindowLevelForHelperWindows:(NSInteger)i_level
{
    if (var_InheritBool(getIntf(), "video-wallpaper"))
        return;

    currentWindowLevel = i_level;
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
