/*****************************************************************************
 * VLCVideoOutputProvider.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2012-2019 VLC authors and VideoLAN
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
#import "windows/video/VLCDetachedVideoWindow.h"
#import "windows/video/VLCVoutView.h"
#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "library/VLCLibraryWindow.h"
#import "os-integration/VLCKeyboardBacklightControl.h"

#import "panels/VLCVideoEffectsWindowController.h"
#import "panels/VLCAudioEffectsWindowController.h"
#import "panels/VLCBookmarksWindowController.h"
#import "panels/VLCTrackSynchronizationWindowController.h"

NSString *VLCWindowShouldUpdateLevel = @"VLCWindowShouldUpdateLevel";
NSString *VLCWindowLevelKey = @"VLCWindowLevelKey";

static int WindowEnable(vlc_window_t *p_wnd, const vlc_window_cfg_t *cfg)
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
        vlc_window_SetFullScreen(p_wnd, NULL);
    return VLC_SUCCESS;
}

static void WindowDisable(vlc_window_t *p_wnd)
{
    @autoreleasepool {
        VLCVideoOutputProvider *voutProvider = [[VLCMain sharedInstance] voutProvider];

        dispatch_async(dispatch_get_main_queue(), ^{
            [voutProvider removeVoutForDisplay:[NSValue valueWithPointer:p_wnd]];
        });
    }
}

static void WindowResize(vlc_window_t *p_wnd,
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

static void WindowSetState(vlc_window_t *p_wnd, unsigned i_state)
{
    if (i_state & VLC_WINDOW_STATE_BELOW)
        msg_Dbg(p_wnd, "Ignore change to VLC_WINDOW_STATE_BELOW");

    @autoreleasepool {
        VLCVideoOutputProvider *voutProvider = [[VLCMain sharedInstance] voutProvider];

        NSInteger i_cocoa_level = NSNormalWindowLevel;

        if (i_state & VLC_WINDOW_STATE_ABOVE)
            i_cocoa_level = NSStatusWindowLevel;

        dispatch_async(dispatch_get_main_queue(), ^{
            [voutProvider setWindowLevel:i_cocoa_level forWindow:p_wnd];
        });
    }
}

static const char windowed;

static void WindowSetFullscreen(vlc_window_t *p_wnd, const char *psz_id)
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

static void WindowUnsetFullscreen(vlc_window_t *wnd)
{
    WindowSetFullscreen(wnd, &windowed);
}

static atomic_bool b_intf_starting = ATOMIC_VAR_INIT(false);

static const struct vlc_window_operations ops = {
    WindowEnable,
    WindowDisable,
    WindowResize,
    NULL,
    WindowSetState,
    WindowUnsetFullscreen,
    WindowSetFullscreen,
};

int WindowOpen(vlc_window_t *p_wnd)
{
    if (!atomic_load(&b_intf_starting)) {
        msg_Err(p_wnd, "Cannot create vout as Mac OS X interface was not found");
        return VLC_EGENERIC;
    }

    p_wnd->type = VLC_WINDOW_TYPE_NSOBJECT;
    p_wnd->ops = &ops;
    return VLC_SUCCESS;
}

@interface VLCVideoOutputProvider ()
{
    NSMutableDictionary *_voutWindows;
    VLCKeyboardBacklightControl *_keyboardBacklight;

    NSPoint _topLeftPoint;

    // save the status level if at least one video window is on status level
    NSUInteger _statusLevelWindowCounter;
    NSInteger _currentWindowLevel;

    BOOL b_mainWindowHasVideo;

    VLCPlayerController *_playerController;
}
@end

@implementation VLCVideoOutputProvider

- (id)init
{
    self = [super init];
    if (self) {
        atomic_store(&b_intf_starting, true);
        _voutWindows = [[NSMutableDictionary alloc] init];
        _keyboardBacklight = [[VLCKeyboardBacklightControl alloc] init];
        _currentWindowLevel = NSNormalWindowLevel;
        _currentStatusWindowLevel = NSFloatingWindowLevel;
    }
    return self;
}

- (void)dealloc
{
    NSArray *keys = [_voutWindows allKeys];
    for (NSValue *key in keys)
        [self removeVoutForDisplay:key];

    if (var_InheritBool(getIntf(), "macosx-dim-keyboard")) {
        [_keyboardBacklight switchLightsInstantly:YES];
    }
}

#pragma mark -
#pragma mark Methods for vout provider

- (VLCVideoWindowCommon *)borderlessVideoWindowAsVideoWallpaper:(BOOL)asVideoWallpaper withWindowDecorations:(BOOL)withWindowDecorations
{
    VLCMain *mainInstance = [VLCMain sharedInstance];

    // videoWallpaper is priorized over !windowDecorations
    msg_Dbg(getIntf(), "Creating background / blank window");
    NSScreen *screen = [NSScreen screenWithDisplayID:(CGDirectDisplayID)var_InheritInteger(getIntf(), "macosx-vdev")];
    if (!screen) {
        screen = mainInstance.libraryWindow.screen;
    }

    NSRect window_rect = asVideoWallpaper ? screen.frame : mainInstance.libraryWindow.frame;
    NSUInteger mask = withWindowDecorations ? NSBorderlessWindowMask | NSResizableWindowMask : NSBorderlessWindowMask;

    VLCVideoWindowCommon *newVideoWindow = [[VLCVideoWindowCommon alloc] initWithContentRect:window_rect styleMask:mask backing:NSBackingStoreBuffered defer:YES];
    newVideoWindow.delegate = newVideoWindow;
    newVideoWindow.releasedWhenClosed = NO;            

    newVideoWindow.backgroundColor = [NSColor blackColor];
    newVideoWindow.canBecomeKeyWindow = !asVideoWallpaper;
    newVideoWindow.canBecomeMainWindow = !asVideoWallpaper;
    newVideoWindow.acceptsMouseMovedEvents = !asVideoWallpaper;
    newVideoWindow.movableByWindowBackground = !asVideoWallpaper;

    VLCVoutView *voutView = [[VLCVoutView alloc] initWithFrame:newVideoWindow.contentView.bounds];
    voutView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    [newVideoWindow.contentView addSubview:voutView positioned:NSWindowAbove relativeTo:nil];
    newVideoWindow.videoView = voutView;

    if (asVideoWallpaper) {
        [newVideoWindow setLevel:CGWindowLevelForKey(kCGDesktopWindowLevelKey) + 1];
        [newVideoWindow orderBack:nil];
    } else {
        BOOL multipleVoutWindows = _voutWindows.count > 0;
        // no frame autosave for additional vout windows
        if (!multipleVoutWindows) {
            // initial window position
            [newVideoWindow center];
            newVideoWindow.frameAutosaveName = @"extra-videowindow";
        }

        newVideoWindow.contentMinSize = NSMakeSize(VLCVideoWindowCommonMinimalHeight, VLCVideoWindowCommonMinimalHeight);
    }

    return newVideoWindow;
}

- (VLCVideoWindowCommon *)setupMainLibraryVideoWindow
{
    VLCMain *mainInstance = [VLCMain sharedInstance];
    
    // should be called before any window resizing occurs
    [mainInstance.libraryWindow videoPlaybackWillBeStarted];
    b_mainWindowHasVideo = YES;
    
    return mainInstance.libraryWindow;
}

- (VLCVideoWindowCommon *)setupDetachedVideoWindow
{
    BOOL multipleVoutWindows = _voutWindows.count > 0;
    // setup detached window with controls
    NSWindowController *o_controller = [[NSWindowController alloc] initWithWindowNibName:@"DetachedVideoWindow"];
    [o_controller loadWindow];
    VLCVideoWindowCommon *newVideoWindow = (VLCDetachedVideoWindow *)o_controller.window;

    // no frame autosave for additional vout windows
    if (multipleVoutWindows) {
        newVideoWindow.frameAutosaveName = @"";
    }

    newVideoWindow.delegate = newVideoWindow;
    newVideoWindow.level = NSNormalWindowLevel;
    return newVideoWindow;
}

- (VLCVideoWindowCommon *)setupVideoWindow
{
    BOOL isNativeFullscreen = var_InheritBool(getIntf(), "macosx-nativefullscreenmode");
    BOOL windowDecorations = var_InheritBool(getIntf(), "video-deco");
    BOOL videoWallpaper = var_InheritBool(getIntf(), "video-wallpaper");

    // TODO: make lion fullscreen compatible with video-wallpaper
    if ((videoWallpaper || !windowDecorations) && !isNativeFullscreen) {
        return [self borderlessVideoWindowAsVideoWallpaper:videoWallpaper withWindowDecorations:windowDecorations];
    }
    
    BOOL isEmbedded = var_InheritBool(getIntf(), "embedded-video") && !b_mainWindowHasVideo;
    if (isEmbedded) {
        return [self setupMainLibraryVideoWindow];
    }
    
    return [self setupDetachedVideoWindow];
}

- (void)setupWindowOriginForVideoWindow:(VLCVideoWindowCommon *)videoWindow
                             atPosition:(NSRect)videoViewPosition
{
    NSRect window_rect = [videoWindow frame];
    if (videoViewPosition.origin.x > 0.)
        window_rect.origin.x = videoViewPosition.origin.x;
    if (videoViewPosition.origin.y > 0.)
        window_rect.origin.y = videoViewPosition.origin.y;

    [videoWindow setFrame:window_rect display:YES];
}

- (void)cascadeVoutWindowsForVideoWindow:(VLCVideoWindowCommon *)videoWindow
{
    if (_voutWindows.count == 1) {
        NSWindow * firstWindow = [_voutWindows objectForKey:_voutWindows.allKeys.firstObject];

        NSRect topleftBaseRect = NSMakeRect(0, firstWindow.frame.size.height, 0, 0);
        _topLeftPoint = [firstWindow convertRectToScreen:topleftBaseRect].origin;
    }

    _topLeftPoint = [videoWindow cascadeTopLeftFromPoint:_topLeftPoint];
    [videoWindow setFrameTopLeftPoint:_topLeftPoint];
}

- (void)setupPositionAndSizeForVideoWindow:(VLCVideoWindowCommon *)videoWindow
                                atPosition:(NSRect)videoViewPosition
{
    BOOL isEmbedded = [videoWindow isKindOfClass:[VLCLibraryWindow class]];
    BOOL multipleVoutWindows = _voutWindows.count > 0;
    NSSize videoViewSize = NSMakeSize(videoViewPosition.size.width, videoViewPosition.size.height);
    
    // set (only!) window origin if specified
    if (!isEmbedded) {
        [self setupWindowOriginForVideoWindow:videoWindow
                                   atPosition:videoViewPosition];
    }

    // cascade windows if we have more than one vout
    if (multipleVoutWindows) {
        [self cascadeVoutWindowsForVideoWindow:videoWindow];
    }

    // resize window
    [videoWindow setNativeVideoSize:videoViewSize];
    [videoWindow makeKeyAndOrderFront: self];
}

- (void)setupVideoOutputForVideoWindow:(VLCVideoWindowCommon *)videoWindow withVlcWindow:(vlc_window_t *)p_wnd
{
    VLCVoutView *voutView = videoWindow.videoView;
    
    [videoWindow setAlphaValue:config_GetFloat("macosx-opaqueness")];
    [_voutWindows setObject:videoWindow forKey:[NSValue valueWithPointer:p_wnd]];
    [voutView setVoutThread:(vout_thread_t *)vlc_object_parent(p_wnd)];
    videoWindow.hasActiveVideo = YES;
    _playerController.activeVideoPlayback = YES;
    [VLCMain sharedInstance].libraryWindow.nonembedded = !b_mainWindowHasVideo;
}

- (VLCVoutView *)setupVoutForWindow:(vlc_window_t *)p_wnd withProposedVideoViewPosition:(NSRect)videoViewPosition
{
    _playerController = [VLCMain sharedInstance].playlistController.playerController;
    VLCVideoWindowCommon *newVideoWindow = [self setupVideoWindow];
    VLCVoutView *voutView = newVideoWindow.videoView;

    BOOL isEmbedded = [newVideoWindow isKindOfClass:[VLCLibraryWindow class]];
    BOOL multipleVoutWindows = _voutWindows.count > 0;
    BOOL videoWallpaper = var_InheritBool(getIntf(), "video-wallpaper") && !multipleVoutWindows;

    // Avoid flashes if video will directly start in fullscreen
    [NSAnimationContext beginGrouping];

    if (!videoWallpaper) {
        [self setupPositionAndSizeForVideoWindow:newVideoWindow atPosition:videoViewPosition];
    }

    [self setupVideoOutputForVideoWindow:newVideoWindow withVlcWindow:p_wnd];
    
    // TODO: find a cleaner way for "start in fullscreen"
    // Start in fs, because either prefs settings, or fullscreen button was pressed before
    /* detect the video-splitter and prevent starts in fullscreen if it is enabled */
    char *psz_splitter = var_GetString(voutView.voutThread, "video-splitter");
    BOOL b_have_splitter = psz_splitter != NULL && strcmp(psz_splitter, "none");
    free(psz_splitter);

    if (!videoWallpaper && !b_have_splitter && (var_InheritBool(getIntf(), "fullscreen") || _playerController.fullscreen)) {
        // this is not set when we start in fullscreen because of
        // fullscreen settings in video prefs the second time
        var_SetBool(vlc_object_parent(p_wnd), "fullscreen", 1);
        [self setFullscreen:1 forWindow:p_wnd withAnimation:NO];
    }

    [NSAnimationContext endGrouping];
    return voutView;
}

- (void)removeVoutForDisplay:(NSValue *)key
{
    VLCMain *mainInstance = [VLCMain sharedInstance];
    VLCVideoWindowCommon *videoWindow = [_voutWindows objectForKey:key];
    if (!videoWindow) {
        msg_Err(getIntf(), "Cannot close nonexisting window");
        return;
    }

    [[videoWindow videoView] releaseVoutThread];

    // set active video to no BEFORE closing the window and exiting fullscreen
    // (avoid stopping playback due to NSWindowWillCloseNotification, preserving fullscreen state)
    [videoWindow setHasActiveVideo: NO];

    // prevent visible extra window if in fullscreen
    [NSAnimationContext beginGrouping];
    BOOL b_native = var_InheritBool(getIntf(), "macosx-nativefullscreenmode");

    // close fullscreen, without changing fullscreen vars
    if (!b_native && ([videoWindow fullscreen] || [videoWindow inFullscreenTransition]))
        [videoWindow leaveFullscreenWithAnimation:NO];

    // native fullscreen window will not be closed if
    // fullscreen was triggered without video
    if ((b_native && [videoWindow class] == [VLCLibraryWindow class] && [videoWindow fullscreen] && [videoWindow windowShouldExitFullscreenWhenFinished])) {
        [videoWindow toggleFullScreen:self];
    }

    if ([videoWindow class] != [VLCLibraryWindow class]) {
        [videoWindow close];
    }
    [NSAnimationContext endGrouping];

    [_voutWindows removeObjectForKey:key];
    if ([_voutWindows count] == 0) {
        [_playerController setActiveVideoPlayback:NO];
        _statusLevelWindowCounter = 0;
    }

    if ([videoWindow class] == [VLCLibraryWindow class]) {
        b_mainWindowHasVideo = NO;

        // video in main window might get stopped while another vout is open
        if ([_voutWindows count] > 0)
            [[mainInstance libraryWindow] setNonembedded:YES];
    }
}


- (void)setNativeVideoSize:(NSSize)size forWindow:(vlc_window_t *)p_wnd
{
    VLCVideoWindowCommon *o_window = [_voutWindows objectForKey:[NSValue valueWithPointer:p_wnd]];
    if (!o_window) {
        msg_Err(getIntf(), "Cannot set size for nonexisting window");
        return;
    }

    [o_window setNativeVideoSize:size];
}

- (void)setWindowLevel:(NSInteger)i_level forWindow:(vlc_window_t *)p_wnd
{
    VLCVideoWindowCommon *o_window = [_voutWindows objectForKey:[NSValue valueWithPointer:p_wnd]];
    if (!o_window) {
        msg_Err(getIntf(), "Cannot set level for nonexisting window");
        return;
    }

    // only set level for helper windows to normal if no status vout window exist anymore
    if(i_level == NSStatusWindowLevel) {
        _statusLevelWindowCounter++;
        // window level need to stay on normal in fullscreen mode
        if (![o_window fullscreen] && ![o_window inFullscreenTransition])
            [self updateWindowLevelForHelperWindows:i_level];
    } else {
        if (_statusLevelWindowCounter > 0)
            _statusLevelWindowCounter--;

        if (_statusLevelWindowCounter == 0) {
            [self updateWindowLevelForHelperWindows:i_level];
        }
    }

    [o_window setWindowLevel:i_level];
}

- (void)setFullscreen:(int)i_full forWindow:(vlc_window_t *)p_wnd withAnimation:(BOOL)b_animation
{
    intf_thread_t *p_intf = getIntf();
    BOOL b_nativeFullscreenMode = var_InheritBool(getIntf(), "macosx-nativefullscreenmode");
    BOOL b_fullscreen = i_full != 0;

    if (var_InheritBool(p_intf, "macosx-dim-keyboard")) {
        [_keyboardBacklight switchLightsAsync:!b_fullscreen];
    }

    if (!p_intf || (!b_nativeFullscreenMode && !p_wnd))
        return;

    if (!_playerController.fullscreen != !b_fullscreen) {
        _playerController.fullscreen = b_fullscreen;
    }

    VLCVideoWindowCommon *o_current_window = nil;
    if (p_wnd) {
        o_current_window = [_voutWindows objectForKey:[NSValue valueWithPointer:p_wnd]];
    }

    if (b_nativeFullscreenMode) {
        if(!o_current_window)
            o_current_window = [[VLCMain sharedInstance] libraryWindow] ;
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
            if (_playerController.playerState != VLC_PLAYER_STATE_STOPPED && [_playerController activeVideoPlayback]) {
                // activate app, as method can also be triggered from outside the app (prevents nasty window layout)
                [NSApp activateIgnoringOtherApps:YES];
                [o_current_window enterFullscreenWithAnimation:b_animation];
            }
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
    [_voutWindows enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {

        if ([obj respondsToSelector:@selector(controlsBar)]) {
            VLCControlsBarCommon *o_controlsBar = [obj controlsBar];
            if (o_controlsBar && block)
                block(o_controlsBar);
        }
    }];
}

- (void)updateWindowsUsingBlock:(void (^)(VLCVideoWindowCommon *o_window))windowUpdater
{
    [_voutWindows enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        if ([obj isKindOfClass: [NSWindow class]])
            windowUpdater(obj);
    }];
}

- (void)updateWindowLevelForHelperWindows:(NSInteger)i_level
{
    if (var_InheritBool(getIntf(), "video-wallpaper"))
        return;

    _currentWindowLevel = i_level;
    if (i_level == NSNormalWindowLevel) {
        _currentStatusWindowLevel = NSFloatingWindowLevel;
    } else {
        _currentStatusWindowLevel = i_level + 1;
    }

    VLCMain *main = [VLCMain sharedInstance];
    [[main libraryWindow] setWindowLevel:i_level];

    [[NSNotificationCenter defaultCenter] postNotificationName:VLCWindowShouldUpdateLevel object:self userInfo:@{VLCWindowLevelKey : @(_currentWindowLevel)}];
}

@end
