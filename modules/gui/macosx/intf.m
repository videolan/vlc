/*****************************************************************************
 * intf.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont # videolan org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>                                      /* malloc(), free() */
#include <sys/param.h>                                    /* for MAXPATHLEN */
#include <string.h>
#include <vlc_common.h>
#include <vlc_keys.h>
#include <vlc_dialog.h>
#include <vlc_url.h>
#include <vlc_modules.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <unistd.h> /* execl() */

#import "CompatibilityFixes.h"
#import "intf.h"
#import "StringUtility.h"
#import "MainMenu.h"
#import "VideoView.h"
#import "prefs.h"
#import "playlist.h"
#import "playlistinfo.h"
#import "controls.h"
#import "open.h"
#import "wizard.h"
#import "bookmarks.h"
#import "coredialogs.h"
#import "AppleRemote.h"
#import "eyetv.h"
#import "simple_prefs.h"
#import "CoreInteraction.h"
#import "TrackSynchronization.h"
#import "VLCVoutWindowController.h"
#import "ExtensionsManager.h"

#import "VideoEffects.h"
#import "AudioEffects.h"

#import <AddressBook/AddressBook.h>         /* for crashlog send mechanism */
#import <Sparkle/Sparkle.h>                 /* we're the update delegate */

#import "iTunes.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static void Run (intf_thread_t *p_intf);

static void updateProgressPanel (void *, const char *, float);
static bool checkProgressPanel (void *);
static void destroyProgressPanel (void *);

static void MsgCallback(void *data, int type, const vlc_log_t *item, const char *format, va_list ap);

static int InputEvent(vlc_object_t *, const char *,
                      vlc_value_t, vlc_value_t, void *);
static int PLItemChanged(vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void *);
static int PlaylistUpdated(vlc_object_t *, const char *,
                           vlc_value_t, vlc_value_t, void *);
static int PlaybackModeUpdated(vlc_object_t *, const char *,
                               vlc_value_t, vlc_value_t, void *);
static int VolumeUpdated(vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void *);
static int BossCallback(vlc_object_t *, const char *,
                         vlc_value_t, vlc_value_t, void *);

#pragma mark -
#pragma mark VLC Interface Object Callbacks

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int OpenIntf (vlc_object_t *p_this)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    [VLCApplication sharedApplication];

    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    p_intf->p_sys = malloc(sizeof(intf_sys_t));
    if (p_intf->p_sys == NULL)
        return VLC_ENOMEM;

    memset(p_intf->p_sys, 0, sizeof(*p_intf->p_sys));

    Run(p_intf);

    [o_pool release];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy interface
 *****************************************************************************/
void CloseIntf (vlc_object_t *p_this)
{
    intf_thread_t *p_intf = (intf_thread_t*) p_this;

    free(p_intf->p_sys);
}

static NSLock * o_vout_provider_lock = nil;

static int WindowControl(vout_window_t *, int i_query, va_list);

int WindowOpen(vout_window_t *p_wnd, const vout_window_cfg_t *cfg)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf) {
        msg_Err(p_wnd, "Mac OS X interface not found");
        [o_pool release];
        return VLC_EGENERIC;
    }
    NSRect proposedVideoViewPosition = NSMakeRect(cfg->x, cfg->y, cfg->width, cfg->height);

    [o_vout_provider_lock lock];
    VLCVoutWindowController *o_vout_controller = [[VLCMain sharedInstance] voutController];
    if (!o_vout_controller) {
        [o_vout_provider_lock unlock];
        [o_pool release];
        return VLC_EGENERIC;
    }

    SEL sel = @selector(setupVoutForWindow:withProposedVideoViewPosition:);
    NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[o_vout_controller methodSignatureForSelector:sel]];
    [inv setTarget:o_vout_controller];
    [inv setSelector:sel];
    [inv setArgument:&p_wnd atIndex:2]; // starting at 2!
    [inv setArgument:&proposedVideoViewPosition atIndex:3];

    [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                       waitUntilDone:YES];

    VLCVoutView *videoView = nil;
    [inv getReturnValue:&videoView];

    if (!videoView) {
        msg_Err(p_wnd, "got no video view from the interface");
        [o_vout_provider_lock unlock];
        [o_pool release];
        return VLC_EGENERIC;
    }

    msg_Dbg(VLCIntf, "returning videoview with proposed position x=%i, y=%i, width=%i, height=%i", cfg->x, cfg->y, cfg->width, cfg->height);
    p_wnd->handle.nsobject = videoView;

    // TODO: find a cleaner way for "start in fullscreen"
    // either prefs settings, or fullscreen button was pressed before
    if (var_InheritBool(VLCIntf, "fullscreen") || var_GetBool(pl_Get(VLCIntf), "fullscreen")) {

        // this is not set when we start in fullscreen because of
        // fullscreen settings in video prefs the second time
        var_SetBool(p_wnd->p_parent, "fullscreen", 1);

        int i_full = 1;

        SEL sel = @selector(setFullscreen:forWindow:);
        NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[o_vout_controller methodSignatureForSelector:sel]];
        [inv setTarget:o_vout_controller];
        [inv setSelector:sel];
        [inv setArgument:&i_full atIndex:2];
        [inv setArgument:&p_wnd atIndex:3];
        [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                           waitUntilDone:NO];
    }
    [o_vout_provider_lock unlock];

    [[VLCMain sharedInstance] setActiveVideoPlayback: YES];
    p_wnd->control = WindowControl;

    [o_pool release];
    return VLC_SUCCESS;
}

static int WindowControl(vout_window_t *p_wnd, int i_query, va_list args)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    [o_vout_provider_lock lock];
    VLCVoutWindowController *o_vout_controller = [[VLCMain sharedInstance] voutController];
    if (!o_vout_controller) {
        [o_vout_provider_lock unlock];
        [o_pool release];
        return VLC_EGENERIC;
    }

    switch(i_query) {
        case VOUT_WINDOW_SET_STATE:
        {
            unsigned i_state = va_arg(args, unsigned);

            NSInteger i_cooca_level = NSNormalWindowLevel;
            if (i_state & VOUT_WINDOW_STATE_ABOVE)
                i_cooca_level = NSStatusWindowLevel;

            SEL sel = @selector(setWindowLevel:forWindow:);
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[o_vout_controller methodSignatureForSelector:sel]];
            [inv setTarget:o_vout_controller];
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
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[o_vout_controller methodSignatureForSelector:sel]];
            [inv setTarget:o_vout_controller];
            [inv setSelector:sel];
            [inv setArgument:&newSize atIndex:2]; // starting at 2!
            [inv setArgument:&p_wnd atIndex:3];
            [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                               waitUntilDone:NO];

            break;
        }
        case VOUT_WINDOW_SET_FULLSCREEN:
        {
            NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
            int i_full = va_arg(args, int);

            SEL sel = @selector(setFullscreen:forWindow:);
            NSInvocation *inv = [NSInvocation invocationWithMethodSignature:[o_vout_controller methodSignatureForSelector:sel]];
            [inv setTarget:o_vout_controller];
            [inv setSelector:sel];
            [inv setArgument:&i_full atIndex:2]; // starting at 2!
            [inv setArgument:&p_wnd atIndex:3];
            [inv performSelectorOnMainThread:@selector(invoke) withObject:nil
                               waitUntilDone:NO];

            break;
        }
        default:
        {
            msg_Warn(p_wnd, "unsupported control query");
            [o_vout_provider_lock unlock];
            [o_pool release];
            return VLC_EGENERIC;
        }
    }

    [o_vout_provider_lock unlock];
    [o_pool release];
    return VLC_SUCCESS;
}

void WindowClose(vout_window_t *p_wnd)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    [o_vout_provider_lock lock];
    VLCVoutWindowController *o_vout_controller = [[VLCMain sharedInstance] voutController];
    if (!o_vout_controller) {
        [o_vout_provider_lock unlock];
        [o_pool release];
        return;
    }

    [o_vout_controller performSelectorOnMainThread:@selector(removeVoutforDisplay:) withObject:[NSValue valueWithPointer:p_wnd] waitUntilDone:NO];
    [o_vout_provider_lock unlock];

    [o_pool release];
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************/
static NSLock * o_appLock = nil;    // controls access to f_appExit
static NSLock * o_plItemChangedLock = nil;

static void Run(intf_thread_t *p_intf)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [VLCApplication sharedApplication];

    o_appLock = [[NSLock alloc] init];
    o_plItemChangedLock = [[NSLock alloc] init];
    o_vout_provider_lock = [[NSLock alloc] init];

    [[VLCMain sharedInstance] setIntf: p_intf];

    [NSBundle loadNibNamed: @"MainMenu" owner: NSApp];

    [NSApp run];
    [[VLCMain sharedInstance] applicationWillTerminate:nil];
    [o_plItemChangedLock release];
    [o_appLock release];
    [o_vout_provider_lock release];
    o_vout_provider_lock = nil;
    [o_pool release];

    raise(SIGTERM);
}

#pragma mark -
#pragma mark Variables Callback

/*****************************************************************************
 * MsgCallback: Callback triggered by the core once a new debug message is
 * ready to be displayed. We store everything in a NSArray in our Cocoa part
 * of this file.
 *****************************************************************************/
static void MsgCallback(void *data, int type, const vlc_log_t *item, const char *format, va_list ap)
{
    int canc = vlc_savecancel();
    char *str;

    if (vasprintf(&str, format, ap) == -1) {
        vlc_restorecancel(canc);
        return;
    }

    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] processReceivedlibvlcMessage: item ofType: type withStr: str];
    [o_pool release];

    vlc_restorecancel(canc);
    free(str);
}

static int InputEvent(vlc_object_t *p_this, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    switch (new_val.i_int) {
        case INPUT_EVENT_STATE:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(playbackStatusUpdated) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_RATE:
            [[[VLCMain sharedInstance] mainMenu] performSelectorOnMainThread:@selector(updatePlaybackRate) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_POSITION:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updatePlaybackPosition) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_TITLE:
        case INPUT_EVENT_CHAPTER:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_CACHE:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateMainWindow) withObject: nil waitUntilDone: NO];
            break;
        case INPUT_EVENT_STATISTICS:
            [[[VLCMain sharedInstance] info] performSelectorOnMainThread:@selector(updateStatistics) withObject: nil waitUntilDone: NO];
            break;
        case INPUT_EVENT_ES:
            break;
        case INPUT_EVENT_TELETEXT:
            break;
        case INPUT_EVENT_AOUT:
            break;
        case INPUT_EVENT_VOUT:
            break;
        case INPUT_EVENT_ITEM_META:
        case INPUT_EVENT_ITEM_INFO:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateInfoandMetaPanel) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_BOOKMARK:
            break;
        case INPUT_EVENT_RECORD:
            [[VLCMain sharedInstance] updateRecordState: var_GetBool(p_this, "record")];
            break;
        case INPUT_EVENT_PROGRAM:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateMainMenu) withObject: nil waitUntilDone:NO];
            break;
        case INPUT_EVENT_ITEM_EPG:
            break;
        case INPUT_EVENT_SIGNAL:
            break;

        case INPUT_EVENT_ITEM_NAME:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(playlistUpdated) withObject: nil waitUntilDone:NO];
            break;

        case INPUT_EVENT_AUDIO_DELAY:
        case INPUT_EVENT_SUBTITLE_DELAY:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateDelays) withObject:nil waitUntilDone:NO];
            break;

        case INPUT_EVENT_DEAD:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updatePlaybackPosition) withObject:nil waitUntilDone:NO];
            break;

        case INPUT_EVENT_ABORT:
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateName) withObject: nil waitUntilDone:NO];
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updatePlaybackPosition) withObject:nil waitUntilDone:NO];
            break;

        default:
            //msg_Warn(p_this, "unhandled input event (%lld)", new_val.i_int);
            break;
    }

    [o_pool release];
    return VLC_SUCCESS;
}

static int PLItemChanged(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    /* Due to constraints within NSAttributedString's main loop runtime handling
     * and other issues, we need to wait for -PlaylistItemChanged to finish and
     * then -informInputChanged on this non-main thread. */
    [o_plItemChangedLock lock];
    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(PlaylistItemChanged) withObject:nil waitUntilDone:YES];
    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(informInputChanged) withObject:nil waitUntilDone:YES];
    [o_plItemChangedLock unlock];

    [o_pool release];
    return VLC_SUCCESS;
}

static int PlaylistUpdated(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    /* Avoid event queue flooding with playlistUpdated selectors, leading to UI freezes.
     * Therefore, only enqueue if no selector already enqueued.
     */
    VLCMain *o_main = [VLCMain sharedInstance];
    @synchronized(o_main) {
        if(![o_main playlistUpdatedSelectorInQueue]) {
            [o_main setPlaylistUpdatedSelectorInQueue:YES];
            [o_main performSelectorOnMainThread:@selector(playlistUpdated) withObject:nil waitUntilDone:NO];
        }
    }

    [o_pool release];
    return VLC_SUCCESS;
}

static int PlaybackModeUpdated(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(playbackModeUpdated) withObject:nil waitUntilDone:NO];

    [o_pool release];
    return VLC_SUCCESS;
}

static int VolumeUpdated(vlc_object_t *p_this, const char *psz_var,
                         vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateVolume) withObject:nil waitUntilDone:NO];

    [o_pool release];
    return VLC_SUCCESS;
}

static int BossCallback(vlc_object_t *p_this, const char *psz_var,
                        vlc_value_t oldval, vlc_value_t new_val, void *param)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    [[VLCCoreInteraction sharedInstance] performSelectorOnMainThread:@selector(pause) withObject:nil waitUntilDone:NO];
    [[VLCApplication sharedApplication] hide:nil];

    [o_pool release];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * ShowController: Callback triggered by the show-intf playlist variable
 * through the ShowIntf-control-intf, to let us show the controller-win;
 * usually when in fullscreen-mode
 *****************************************************************************/
static int ShowController(vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param)
{
    intf_thread_t * p_intf = VLCIntf;
    if (p_intf && p_intf->p_sys) {
        playlist_t * p_playlist = pl_Get(p_intf);
        BOOL b_fullscreen = var_GetBool(p_playlist, "fullscreen");
        if (strcmp(psz_variable, "intf-toggle-fscontrol") || b_fullscreen)
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(showFullscreenController) withObject:nil waitUntilDone:NO];
        else
            [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(showMainWindow) withObject:nil waitUntilDone:NO];
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * DialogCallback: Callback triggered by the "dialog-*" variables
 * to let the intf display error and interaction dialogs
 *****************************************************************************/
static int DialogCallback(vlc_object_t *p_this, const char *type, vlc_value_t previous, vlc_value_t value, void *data)
{
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];
    VLCMain *interface = (VLCMain *)data;

    if ([@(type) isEqualToString: @"dialog-progress-bar"]) {
        /* the progress panel needs to update itself and therefore wants special treatment within this context */
        dialog_progress_bar_t *p_dialog = (dialog_progress_bar_t *)value.p_address;

        p_dialog->pf_update = updateProgressPanel;
        p_dialog->pf_check = checkProgressPanel;
        p_dialog->pf_destroy = destroyProgressPanel;
        p_dialog->p_sys = VLCIntf->p_libvlc;
    }

    NSValue *o_value = [NSValue valueWithPointer:value.p_address];
    [[VLCCoreDialogProvider sharedInstance] performEventWithObject: o_value ofType: type];

    [o_pool release];
    return VLC_SUCCESS;
}

void updateProgressPanel (void *priv, const char *text, float value)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    NSString *o_txt;
    if (text != NULL)
        o_txt = @(text);
    else
        o_txt = @"";

    [[[VLCMain sharedInstance] coreDialogProvider] updateProgressPanelWithText: o_txt andNumber: (double)(value * 1000.)];

    [o_pool release];
}

void destroyProgressPanel (void *priv)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

    if ([[NSApplication sharedApplication] isRunning])
        [[[VLCMain sharedInstance] coreDialogProvider] performSelectorOnMainThread:@selector(destroyProgressPanel) withObject:nil waitUntilDone:YES];

    [o_pool release];
}

bool checkProgressPanel (void *priv)
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    return [[[VLCMain sharedInstance] coreDialogProvider] progressCancelled];
    [o_pool release];
}

#pragma mark -
#pragma mark Helpers

input_thread_t *getInput(void)
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return NULL;
    return pl_CurrentInput(p_intf);
}

vout_thread_t *getVout(void)
{
    input_thread_t *p_input = getInput();
    if (!p_input)
        return NULL;
    vout_thread_t *p_vout = input_GetVout(p_input);
    vlc_object_release(p_input);
    return p_vout;
}

vout_thread_t *getVoutForActiveWindow(void)
{
    vout_thread_t *p_vout = nil;

    id currentWindow = [NSApp keyWindow];
    if ([currentWindow respondsToSelector:@selector(videoView)]) {
        VLCVoutView *videoView = [currentWindow videoView];
        if (videoView) {
            p_vout = [videoView voutThread];
        }
    }

    if (!p_vout)
        p_vout = getVout();

    return p_vout;
}

audio_output_t *getAout(void)
{
    intf_thread_t *p_intf = VLCIntf;
    if (!p_intf)
        return NULL;
    return playlist_GetAout(pl_Get(p_intf));
}

#pragma mark -
#pragma mark Private

@interface VLCMain ()
- (void)removeOldPreferences;
@end

@interface VLCMain (Internal)
- (void)handlePortMessage:(NSPortMessage *)o_msg;
- (void)resetMediaKeyJump;
- (void)coreChangedMediaKeySupportSetting: (NSNotification *)o_notification;
@end

/*****************************************************************************
 * VLCMain implementation
 *****************************************************************************/
@implementation VLCMain

@synthesize voutController=o_vout_controller;
@synthesize nativeFullscreenMode=b_nativeFullscreenMode;
@synthesize playlistUpdatedSelectorInQueue=b_playlist_updated_selector_in_queue;

#pragma mark -
#pragma mark Initialization

static VLCMain *_o_sharedMainInstance = nil;

+ (VLCMain *)sharedInstance
{
    return _o_sharedMainInstance ? _o_sharedMainInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedMainInstance) {
        [self dealloc];
        return _o_sharedMainInstance;
    } else
        _o_sharedMainInstance = [super init];

    p_intf = NULL;
    p_current_input = p_input_changed = NULL;

    o_msg_lock = [[NSLock alloc] init];
    o_msg_arr = [[NSMutableArray arrayWithCapacity: 600] retain];

    o_open = [[VLCOpen alloc] init];
    o_coredialogs = [[VLCCoreDialogProvider alloc] init];
    o_info = [[VLCInfo alloc] init];
    o_mainmenu = [[VLCMainMenu alloc] init];
    o_coreinteraction = [[VLCCoreInteraction alloc] init];
    o_eyetv = [[VLCEyeTVController alloc] init];
    o_mainwindow = [[VLCMainWindow alloc] init];

    /* announce our launch to a potential eyetv plugin */
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName: @"VLCOSXGUIInit"
                                                                   object: @"VLCEyeTVSupport"
                                                                 userInfo: NULL
                                                       deliverImmediately: YES];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:@"NO" forKey:@"LiveUpdateTheMessagesPanel"];
    [defaults registerDefaults:appDefaults];

    o_vout_controller = [[VLCVoutWindowController alloc] init];

    return _o_sharedMainInstance;
}

- (void)setIntf: (intf_thread_t *)p_mainintf
{
    p_intf = p_mainintf;
}

- (intf_thread_t *)intf
{
    return p_intf;
}

- (void)awakeFromNib
{
    playlist_t *p_playlist;
    vlc_value_t val;
    if (!p_intf) return;
    var_Create(p_intf, "intf-change", VLC_VAR_BOOL);

    /* Check if we already did this once. Opening the other nibs calls it too,
     because VLCMain is the owner */
    if (nib_main_loaded)
        return;

    [o_msgs_panel setExcludedFromWindowsMenu: YES];
    [o_msgs_panel setDelegate: self];

    p_playlist = pl_Get(p_intf);

    val.b_bool = false;

    var_AddCallback(p_intf->p_libvlc, "intf-toggle-fscontrol", ShowController, self);
    var_AddCallback(p_intf->p_libvlc, "intf-show", ShowController, self);
    var_AddCallback(p_intf->p_libvlc, "intf-boss", BossCallback, self);
    //    var_AddCallback(p_playlist, "item-change", PLItemChanged, self);
    var_AddCallback(p_playlist, "activity", PLItemChanged, self);
    var_AddCallback(p_playlist, "leaf-to-parent", PlaylistUpdated, self);
    var_AddCallback(p_playlist, "playlist-item-append", PlaylistUpdated, self);
    var_AddCallback(p_playlist, "playlist-item-deleted", PlaylistUpdated, self);
    var_AddCallback(p_playlist, "random", PlaybackModeUpdated, self);
    var_AddCallback(p_playlist, "repeat", PlaybackModeUpdated, self);
    var_AddCallback(p_playlist, "loop", PlaybackModeUpdated, self);
    var_AddCallback(p_playlist, "volume", VolumeUpdated, self);
    var_AddCallback(p_playlist, "mute", VolumeUpdated, self);

    if (!OSX_SNOW_LEOPARD) {
        if ([NSApp currentSystemPresentationOptions] & NSApplicationPresentationFullScreen)
            var_SetBool(p_playlist, "fullscreen", YES);
    }

    /* load our Core and Shared Dialogs nibs */
    nib_coredialogs_loaded = [NSBundle loadNibNamed:@"CoreDialogs" owner: NSApp];
    [NSBundle loadNibNamed:@"SharedDialogs" owner: NSApp];

    /* subscribe to various interactive dialogues */
    var_Create(p_intf, "dialog-error", VLC_VAR_ADDRESS);
    var_AddCallback(p_intf, "dialog-error", DialogCallback, self);
    var_Create(p_intf, "dialog-critical", VLC_VAR_ADDRESS);
    var_AddCallback(p_intf, "dialog-critical", DialogCallback, self);
    var_Create(p_intf, "dialog-login", VLC_VAR_ADDRESS);
    var_AddCallback(p_intf, "dialog-login", DialogCallback, self);
    var_Create(p_intf, "dialog-question", VLC_VAR_ADDRESS);
    var_AddCallback(p_intf, "dialog-question", DialogCallback, self);
    var_Create(p_intf, "dialog-progress-bar", VLC_VAR_ADDRESS);
    var_AddCallback(p_intf, "dialog-progress-bar", DialogCallback, self);
    dialog_Register(p_intf);

    /* init Apple Remote support */
    o_remote = [[AppleRemote alloc] init];
    [o_remote setClickCountEnabledButtons: kRemoteButtonPlay];
    [o_remote setDelegate: _o_sharedMainInstance];

    [o_msgs_refresh_btn setImage: [NSImage imageNamed: NSImageNameRefreshTemplate]];

    /* yeah, we are done */
    b_nativeFullscreenMode = NO;
#ifdef MAC_OS_X_VERSION_10_7
    if (!OSX_SNOW_LEOPARD)
        b_nativeFullscreenMode = var_InheritBool(p_intf, "macosx-nativefullscreenmode");
#endif

    if (config_GetInt(VLCIntf, "macosx-icon-change")) {
        /* After day 354 of the year, the usual VLC cone is replaced by another cone
         * wearing a Father Xmas hat.
         * Note: this icon doesn't represent an endorsement of The Coca-Cola Company.
         */
        NSCalendar *gregorian =
        [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar];
        NSUInteger dayOfYear = [gregorian ordinalityOfUnit:NSDayCalendarUnit inUnit:NSYearCalendarUnit forDate:[NSDate date]];
        [gregorian release];

        if (dayOfYear >= 354)
            [[VLCApplication sharedApplication] setApplicationIconImage: [NSImage imageNamed:@"vlc-xmas"]];
    }

    [self initStrings];

    nib_main_loaded = TRUE;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    if (!p_intf)
        return;

    [self updateCurrentlyUsedHotkeys];

    /* init media key support */
    b_mediaKeySupport = var_InheritBool(VLCIntf, "macosx-mediakeys");
    if (b_mediaKeySupport) {
        o_mediaKeyController = [[SPMediaKeyTap alloc] initWithDelegate:self];
        [[NSUserDefaults standardUserDefaults] registerDefaults:[NSDictionary dictionaryWithObjectsAndKeys:
                                                                 [SPMediaKeyTap defaultMediaKeyUserBundleIdentifiers], kMediaKeyUsingBundleIdentifiersDefaultsKey,
                                                                 nil]];
    }
    [[NSNotificationCenter defaultCenter] addObserver: self selector: @selector(coreChangedMediaKeySupportSetting:) name: @"VLCMediaKeySupportSettingChanged" object: nil];

    [self removeOldPreferences];

    /* Handle sleep notification */
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(computerWillSleep:)
           name:NSWorkspaceWillSleepNotification object:nil];

    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(lookForCrashLog) withObject:nil waitUntilDone:NO];

    /* we will need this, so let's load it here so the interface appears to be more responsive */
    nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: NSApp];

    /* update the main window */
    [o_mainwindow updateWindow];
    [o_mainwindow updateTimeSlider];
    [o_mainwindow updateVolumeSlider];
}

- (void)initStrings
{
    if (!p_intf)
        return;

    /* messages panel */
    [o_msgs_panel setTitle: _NS("Messages")];
    [o_msgs_crashlog_btn setTitle: _NS("Open CrashLog...")];
    [o_msgs_save_btn setTitle: _NS("Save this Log...")];

    /* crash reporter panel */
    [o_crashrep_send_btn setTitle: _NS("Send")];
    [o_crashrep_dontSend_btn setTitle: _NS("Don't Send")];
    [o_crashrep_title_txt setStringValue: _NS("VLC crashed previously")];
    [o_crashrep_win setTitle: _NS("VLC crashed previously")];
    [o_crashrep_desc_txt setStringValue: _NS("Do you want to send details on the crash to VLC's development team?\n\nIf you want, you can enter a few lines on what you did before VLC crashed along with other helpful information: a link to download a sample file, a URL of a network stream, ...")];
    [o_crashrep_includeEmail_ckb setTitle: _NS("I agree to be possibly contacted about this bugreport.")];
    [o_crashrep_includeEmail_txt setStringValue: _NS("Only your default E-Mail address will be submitted, including no further information.")];
    [o_crashrep_dontaskagain_ckb setTitle: _NS("Don't ask again")];
}

#pragma mark -
#pragma mark Termination

- (void)applicationWillTerminate:(NSNotification *)notification
{
    /* don't allow a double termination call. If the user has
     * already invoked the quit then simply return this time. */
    static bool f_appExit = false;
    bool isTerminating;

    [o_appLock lock];
    isTerminating = f_appExit;
    f_appExit = true;
    [o_appLock unlock];

    if (isTerminating)
        return;

    [self resumeItunesPlayback:nil];

    if (notification == nil)
        [[NSNotificationCenter defaultCenter] postNotificationName: NSApplicationWillTerminateNotification object: nil];

    playlist_t * p_playlist = pl_Get(p_intf);
    int returnedValue = 0;

    /* always exit fullscreen on quit, otherwise we get ugly artifacts on the next launch */
    if (b_nativeFullscreenMode) {
        [o_mainwindow toggleFullScreen: self];
        [NSApp setPresentationOptions:(NSApplicationPresentationDefault)];
    }

    /* save current video and audio profiles */
    [[VLCVideoEffects sharedInstance] saveCurrentProfile];
    [[VLCAudioEffects sharedInstance] saveCurrentProfile];

    /* Save some interface state in configuration, at module quit */
    config_PutInt(p_intf, "random", var_GetBool(p_playlist, "random"));
    config_PutInt(p_intf, "loop", var_GetBool(p_playlist, "loop"));
    config_PutInt(p_intf, "repeat", var_GetBool(p_playlist, "repeat"));

    msg_Dbg(p_intf, "Terminating");

    /* unsubscribe from the interactive dialogues */
    dialog_Unregister(p_intf);
    var_DelCallback(p_intf, "dialog-error", DialogCallback, self);
    var_DelCallback(p_intf, "dialog-critical", DialogCallback, self);
    var_DelCallback(p_intf, "dialog-login", DialogCallback, self);
    var_DelCallback(p_intf, "dialog-question", DialogCallback, self);
    var_DelCallback(p_intf, "dialog-progress-bar", DialogCallback, self);
    //var_DelCallback(p_playlist, "item-change", PLItemChanged, self);
    var_DelCallback(p_playlist, "activity", PLItemChanged, self);
    var_DelCallback(p_playlist, "leaf-to-parent", PlaylistUpdated, self);
    var_DelCallback(p_playlist, "playlist-item-append", PlaylistUpdated, self);
    var_DelCallback(p_playlist, "playlist-item-deleted", PlaylistUpdated, self);
    var_DelCallback(p_playlist, "random", PlaybackModeUpdated, self);
    var_DelCallback(p_playlist, "repeat", PlaybackModeUpdated, self);
    var_DelCallback(p_playlist, "loop", PlaybackModeUpdated, self);
    var_DelCallback(p_playlist, "volume", VolumeUpdated, self);
    var_DelCallback(p_playlist, "mute", VolumeUpdated, self);
    var_DelCallback(p_intf->p_libvlc, "intf-toggle-fscontrol", ShowController, self);
    var_DelCallback(p_intf->p_libvlc, "intf-show", ShowController, self);
    var_DelCallback(p_intf->p_libvlc, "intf-boss", BossCallback, self);

    if (p_current_input) {
        var_DelCallback(p_current_input, "intf-event", InputEvent, [VLCMain sharedInstance]);
        vlc_object_release(p_current_input);
        p_current_input = NULL;
    }

    /* remove global observer watching for vout device changes correctly */
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    [o_vout_provider_lock lock];
    // release before o_info!
    [o_vout_controller release];
    o_vout_controller = nil;
    [o_vout_provider_lock unlock];

    /* release some other objects here, because it isn't sure whether dealloc
     * will be called later on */
    if (o_sprefs)
        [o_sprefs release];

    if (o_prefs)
        [o_prefs release];

    [o_open release];

    if (o_info)
        [o_info release];

    if (o_wizard)
        [o_wizard release];

    [crashLogURLConnection cancel];
    [crashLogURLConnection release];

    [o_coredialogs release];
    [o_eyetv release];

    /* unsubscribe from libvlc's debug messages */
    vlc_LogSet(p_intf->p_libvlc, NULL, NULL);

    [o_msg_arr removeAllObjects];
    [o_msg_arr release];
    o_msg_arr = NULL;
    [o_usedHotkeys release];
    o_usedHotkeys = NULL;

    [o_mediaKeyController release];

    [o_msg_lock release];

    /* write cached user defaults to disk */
    [[NSUserDefaults standardUserDefaults] synchronize];


    [o_mainmenu release];

    libvlc_Quit(p_intf->p_libvlc);

    [o_mainwindow release];
    o_mainwindow = NULL;

    [self setIntf:nil];
}

#pragma mark -
#pragma mark Sparkle delegate
/* received directly before the update gets installed, so let's shut down a bit */
- (void)updater:(SUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update
{
    [NSApp activateIgnoringOtherApps:YES];
    [o_remote stopListening: self];
    [[VLCCoreInteraction sharedInstance] stop];
}

#pragma mark -
#pragma mark Media Key support

-(void)mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event
{
    if (b_mediaKeySupport) {
        assert([event type] == NSSystemDefined && [event subtype] == SPSystemDefinedEventMediaKeys);

        int keyCode = (([event data1] & 0xFFFF0000) >> 16);
        int keyFlags = ([event data1] & 0x0000FFFF);
        int keyState = (((keyFlags & 0xFF00) >> 8)) == 0xA;
        int keyRepeat = (keyFlags & 0x1);

        if (keyCode == NX_KEYTYPE_PLAY && keyState == 0)
            [[VLCCoreInteraction sharedInstance] playOrPause];

        if ((keyCode == NX_KEYTYPE_FAST || keyCode == NX_KEYTYPE_NEXT) && !b_mediakeyJustJumped) {
            if (keyState == 0 && keyRepeat == 0)
                [[VLCCoreInteraction sharedInstance] next];
            else if (keyRepeat == 1) {
                [[VLCCoreInteraction sharedInstance] forwardShort];
                b_mediakeyJustJumped = YES;
                [self performSelector:@selector(resetMediaKeyJump)
                           withObject: NULL
                           afterDelay:0.25];
            }
        }

        if ((keyCode == NX_KEYTYPE_REWIND || keyCode == NX_KEYTYPE_PREVIOUS) && !b_mediakeyJustJumped) {
            if (keyState == 0 && keyRepeat == 0)
                [[VLCCoreInteraction sharedInstance] previous];
            else if (keyRepeat == 1) {
                [[VLCCoreInteraction sharedInstance] backwardShort];
                b_mediakeyJustJumped = YES;
                [self performSelector:@selector(resetMediaKeyJump)
                           withObject: NULL
                           afterDelay:0.25];
            }
        }
    }
}

#pragma mark -
#pragma mark Other notification

/* Listen to the remote in exclusive mode, only when VLC is the active
   application */
- (void)applicationDidBecomeActive:(NSNotification *)aNotification
{
    if (!p_intf)
        return;
    if (var_InheritBool(p_intf, "macosx-appleremote") == YES)
        [o_remote startListening: self];
}
- (void)applicationDidResignActive:(NSNotification *)aNotification
{
    if (!p_intf)
        return;
    [o_remote stopListening: self];
}

/* Triggered when the computer goes to sleep */
- (void)computerWillSleep: (NSNotification *)notification
{
    [[VLCCoreInteraction sharedInstance] pause];
}

#pragma mark -
#pragma mark File opening over dock icon

- (void)application:(NSApplication *)o_app openFiles:(NSArray *)o_names
{
    BOOL b_autoplay = config_GetInt(VLCIntf, "macosx-autoplay");
    char *psz_uri = vlc_path2uri([[o_names objectAtIndex:0] UTF8String], "file");

    // try to add file as subtitle
    if ([o_names count] == 1 && psz_uri) {
        input_thread_t * p_input = pl_CurrentInput(VLCIntf);
        if (p_input) {
            BOOL b_returned = NO;
            b_returned = input_AddSubtitle(p_input, psz_uri, true);
            vlc_object_release(p_input);
            if (!b_returned) {
                free(psz_uri);
                return;
            }
        }
    }
    free(psz_uri);

    NSArray *o_sorted_names = [o_names sortedArrayUsingSelector: @selector(caseInsensitiveCompare:)];
    NSMutableArray *o_result = [NSMutableArray arrayWithCapacity: [o_sorted_names count]];
    for (int i = 0; i < [o_sorted_names count]; i++) {
        psz_uri = vlc_path2uri([[o_sorted_names objectAtIndex:i] UTF8String], "file");
        if (!psz_uri)
            continue;

        NSDictionary *o_dic = [NSDictionary dictionaryWithObject:[NSString stringWithCString:psz_uri encoding:NSUTF8StringEncoding] forKey:@"ITEM_URL"];
        free(psz_uri);
        [o_result addObject: o_dic];
    }

    if (b_autoplay)
        [o_playlist appendArray: o_result atPos: -1 enqueue: NO];
    else
        [o_playlist appendArray: o_result atPos: -1 enqueue: YES];

    return;
}

/* When user click in the Dock icon our double click in the finder */
- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)hasVisibleWindows
{
    if (!hasVisibleWindows)
        [o_mainwindow makeKeyAndOrderFront:self];

    return YES;
}

#pragma mark -
#pragma mark Apple Remote Control

/* Helper method for the remote control interface in order to trigger forward/backward and volume
   increase/decrease as long as the user holds the left/right, plus/minus button */
- (void) executeHoldActionForRemoteButton: (NSNumber*) buttonIdentifierNumber
{
    if (b_remote_button_hold) {
        switch([buttonIdentifierNumber intValue]) {
            case kRemoteButtonRight_Hold:
                [[VLCCoreInteraction sharedInstance] forward];
                break;
            case kRemoteButtonLeft_Hold:
                [[VLCCoreInteraction sharedInstance] backward];
                break;
            case kRemoteButtonVolume_Plus_Hold:
                if (p_intf)
                    var_SetInteger(p_intf->p_libvlc, "key-action", ACTIONID_VOL_UP);
                break;
            case kRemoteButtonVolume_Minus_Hold:
                if (p_intf)
                    var_SetInteger(p_intf->p_libvlc, "key-action", ACTIONID_VOL_DOWN);
                break;
        }
        if (b_remote_button_hold) {
            /* trigger event */
            [self performSelector:@selector(executeHoldActionForRemoteButton:)
                         withObject:buttonIdentifierNumber
                         afterDelay:0.25];
        }
    }
}

/* Apple Remote callback */
- (void) appleRemoteButton: (AppleRemoteEventIdentifier)buttonIdentifier
               pressedDown: (BOOL) pressedDown
                clickCount: (unsigned int) count
{
    switch(buttonIdentifier) {
        case k2009RemoteButtonFullscreen:
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            break;
        case k2009RemoteButtonPlay:
            [[VLCCoreInteraction sharedInstance] playOrPause];
            break;
        case kRemoteButtonPlay:
            if (count >= 2)
                [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            else
                [[VLCCoreInteraction sharedInstance] playOrPause];
            break;
        case kRemoteButtonVolume_Plus:
            if (config_GetInt(VLCIntf, "macosx-appleremote-sysvol"))
                [NSSound increaseSystemVolume];
            else
                if (p_intf)
                    var_SetInteger(p_intf->p_libvlc, "key-action", ACTIONID_VOL_UP);
            break;
        case kRemoteButtonVolume_Minus:
            if (config_GetInt(VLCIntf, "macosx-appleremote-sysvol"))
                [NSSound decreaseSystemVolume];
            else
                if (p_intf)
                    var_SetInteger(p_intf->p_libvlc, "key-action", ACTIONID_VOL_DOWN);
            break;
        case kRemoteButtonRight:
            if (config_GetInt(VLCIntf, "macosx-appleremote-prevnext"))
                [[VLCCoreInteraction sharedInstance] forward];
            else
                [[VLCCoreInteraction sharedInstance] next];
            break;
        case kRemoteButtonLeft:
            if (config_GetInt(VLCIntf, "macosx-appleremote-prevnext"))
                [[VLCCoreInteraction sharedInstance] backward];
            else
                [[VLCCoreInteraction sharedInstance] previous];
            break;
        case kRemoteButtonRight_Hold:
        case kRemoteButtonLeft_Hold:
        case kRemoteButtonVolume_Plus_Hold:
        case kRemoteButtonVolume_Minus_Hold:
            /* simulate an event as long as the user holds the button */
            b_remote_button_hold = pressedDown;
            if (pressedDown) {
                NSNumber* buttonIdentifierNumber = @(buttonIdentifier);
                [self performSelector:@selector(executeHoldActionForRemoteButton:)
                           withObject:buttonIdentifierNumber];
            }
            break;
        case kRemoteButtonMenu:
            [o_controls showPosition: self]; //FIXME
            break;
        case kRemoteButtonPlay_Sleep:
        {
            NSAppleScript * script = [[NSAppleScript alloc] initWithSource:@"tell application \"System Events\" to sleep"];
            [script executeAndReturnError:nil];
            [script release];
            break;
        }
        default:
            /* Add here whatever you want other buttons to do */
            break;
    }
}

#pragma mark -
#pragma mark Key Shortcuts

/*****************************************************************************
 * hasDefinedShortcutKey: Check to see if the key press is a defined VLC
 * shortcut key.  If it is, pass it off to VLC for handling and return YES,
 * otherwise ignore it and return NO (where it will get handled by Cocoa).
 *****************************************************************************/
- (BOOL)hasDefinedShortcutKey:(NSEvent *)o_event force:(BOOL)b_force
{
    unichar key = 0;
    vlc_value_t val;
    unsigned int i_pressed_modifiers = 0;

    val.i_int = 0;
    i_pressed_modifiers = [o_event modifierFlags];

    if (i_pressed_modifiers & NSControlKeyMask)
        val.i_int |= KEY_MODIFIER_CTRL;

    if (i_pressed_modifiers & NSAlternateKeyMask)
        val.i_int |= KEY_MODIFIER_ALT;

    if (i_pressed_modifiers & NSShiftKeyMask)
        val.i_int |= KEY_MODIFIER_SHIFT;

    if (i_pressed_modifiers & NSCommandKeyMask)
        val.i_int |= KEY_MODIFIER_COMMAND;

    NSString * characters = [o_event charactersIgnoringModifiers];
    if ([characters length] > 0) {
        key = [[characters lowercaseString] characterAtIndex: 0];

        /* handle Lion's default key combo for fullscreen-toggle in addition to our own hotkeys */
        if (key == 'f' && i_pressed_modifiers & NSControlKeyMask && i_pressed_modifiers & NSCommandKeyMask) {
            [[VLCCoreInteraction sharedInstance] toggleFullscreen];
            return YES;
        }

        if (!b_force) {
            switch(key) {
                case NSDeleteCharacter:
                case NSDeleteFunctionKey:
                case NSDeleteCharFunctionKey:
                case NSBackspaceCharacter:
                case NSUpArrowFunctionKey:
                case NSDownArrowFunctionKey:
                case NSEnterCharacter:
                case NSCarriageReturnCharacter:
                    return NO;
            }
        }

        val.i_int |= CocoaKeyToVLC(key);

        BOOL b_found_key = NO;
        for (int i = 0; i < [o_usedHotkeys count]; i++) {
            NSString *str = [o_usedHotkeys objectAtIndex:i];
            unsigned int i_keyModifiers = [[VLCStringUtility sharedInstance] VLCModifiersToCocoa: str];

            if ([[characters lowercaseString] isEqualToString: [[VLCStringUtility sharedInstance] VLCKeyToString: str]] &&
               (i_keyModifiers & NSShiftKeyMask)     == (i_pressed_modifiers & NSShiftKeyMask) &&
               (i_keyModifiers & NSControlKeyMask)   == (i_pressed_modifiers & NSControlKeyMask) &&
               (i_keyModifiers & NSAlternateKeyMask) == (i_pressed_modifiers & NSAlternateKeyMask) &&
               (i_keyModifiers & NSCommandKeyMask)   == (i_pressed_modifiers & NSCommandKeyMask)) {
                b_found_key = YES;
                break;
            }
        }

        if (b_found_key) {
            var_SetInteger(p_intf->p_libvlc, "key-pressed", val.i_int);
            return YES;
        }
    }

    return NO;
}

- (void)updateCurrentlyUsedHotkeys
{
    NSMutableArray *o_tempArray = [[NSMutableArray alloc] init];
    /* Get the main Module */
    module_t *p_main = module_get_main();
    assert(p_main);
    unsigned confsize;
    module_config_t *p_config;

    p_config = module_config_get (p_main, &confsize);

    for (size_t i = 0; i < confsize; i++) {
        module_config_t *p_item = p_config + i;

        if (CONFIG_ITEM(p_item->i_type) && p_item->psz_name != NULL
           && !strncmp(p_item->psz_name , "key-", 4)
           && !EMPTY_STR(p_item->psz_text)) {
            if (p_item->value.psz)
                [o_tempArray addObject: @(p_item->value.psz)];
        }
    }
    module_config_free (p_config);

    if (o_usedHotkeys)
        [o_usedHotkeys release];
    o_usedHotkeys = [[NSArray alloc] initWithArray: o_tempArray copyItems: YES];
    [o_tempArray release];
}

#pragma mark -
#pragma mark Interface updaters

- (void)PlaylistItemChanged
{
    if (p_current_input && (p_current_input->b_dead || !vlc_object_alive(p_current_input))) {
        var_DelCallback(p_current_input, "intf-event", InputEvent, [VLCMain sharedInstance]);
        p_input_changed = p_current_input;
        p_current_input = NULL;

        [o_mainmenu setRateControlsEnabled: NO];
    }
    else if (!p_current_input) {
        // object is hold here and released then it is dead
        p_current_input = playlist_CurrentInput(pl_Get(VLCIntf));
        if (p_current_input) {
            var_AddCallback(p_current_input, "intf-event", InputEvent, [VLCMain sharedInstance]);
            [self playbackStatusUpdated];
            [o_mainmenu setRateControlsEnabled: YES];
            if ([self activeVideoPlayback] && [[o_mainwindow videoView] isHidden])
                [o_mainwindow performSelectorOnMainThread:@selector(togglePlaylist:) withObject: nil waitUntilDone:NO];
            p_input_changed = vlc_object_hold(p_current_input);
        }
    }

    [o_playlist updateRowSelection];
    [o_mainwindow updateWindow];
    [self updateDelays];
    [self updateMainMenu];
}

- (void)informInputChanged
{
    if (p_input_changed) {
        [[ExtensionsManager getInstance:p_intf] inputChanged:p_input_changed];
        vlc_object_release(p_input_changed);
        p_input_changed = NULL;
    }
}

- (void)updateMainMenu
{
    [o_mainmenu setupMenus];
    [o_mainmenu updatePlaybackRate];
    [[VLCCoreInteraction sharedInstance] resetAtoB];
}

- (void)updateMainWindow
{
    [o_mainwindow updateWindow];
}

- (void)showMainWindow
{
    [o_mainwindow performSelectorOnMainThread:@selector(makeKeyAndOrderFront:) withObject:nil waitUntilDone:NO];
}

- (void)showFullscreenController
{
    // defer selector here (possibly another time) to ensure that keyWindow is set properly
    // (needed for NSApplicationDidBecomeActiveNotification)
    [o_mainwindow performSelectorOnMainThread:@selector(showFullscreenController) withObject:nil waitUntilDone:NO];
}

- (void)updateDelays
{
    [[VLCTrackSynchronization sharedInstance] performSelectorOnMainThread: @selector(updateValues) withObject: nil waitUntilDone:NO];
}

- (void)updateName
{
    [o_mainwindow updateName];
}

- (void)updatePlaybackPosition
{
    [o_mainwindow updateTimeSlider];
    [[VLCCoreInteraction sharedInstance] updateAtoB];
}

- (void)updateVolume
{
    [o_mainwindow updateVolumeSlider];
}

- (void)playlistUpdated
{
    @synchronized(self) {
        b_playlist_updated_selector_in_queue = NO;
    }

    [self playbackStatusUpdated];
    [o_playlist playlistUpdated];
    [o_mainwindow updateWindow];
    [o_mainwindow updateName];

    [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCMediaKeySupportSettingChanged"
                                                        object: nil
                                                      userInfo: nil];
}

- (void)updateRecordState: (BOOL)b_value
{
    [o_mainmenu updateRecordState:b_value];
}

- (void)updateInfoandMetaPanel
{
    [o_playlist outlineViewSelectionDidChange:nil];
}

- (void)resumeItunesPlayback:(id)sender
{
    if (b_has_itunes_paused && var_InheritInteger(p_intf, "macosx-control-itunes") > 1) {
        iTunesApplication *iTunesApp = [SBApplication applicationWithBundleIdentifier:@"com.apple.iTunes"];
        if (iTunesApp && [iTunesApp isRunning]) {
            if ([iTunesApp playerState] == iTunesEPlSPaused) {
                msg_Dbg(p_intf, "Unpause iTunes...");
                [iTunesApp playpause];
            }
        }

    }

    b_has_itunes_paused = NO;
    o_itunes_play_timer = nil;
}

- (void)playbackStatusUpdated
{
    int state = -1;
    if (p_current_input) {
        state = var_GetInteger(p_current_input, "state");
    }

    int i_control_itunes = var_InheritInteger(p_intf, "macosx-control-itunes");
    // cancel itunes timer if next item starts playing
    if (state > -1 && state != END_S && i_control_itunes > 0) {
        if (o_itunes_play_timer) {
            [o_itunes_play_timer invalidate];
            o_itunes_play_timer = nil;
        }
    }

    if (state == PLAYING_S) {
        // pause iTunes
        if (i_control_itunes > 0 && !b_has_itunes_paused) {
            iTunesApplication *iTunesApp = [SBApplication applicationWithBundleIdentifier:@"com.apple.iTunes"];
            if (iTunesApp && [iTunesApp isRunning]) {
                if ([iTunesApp playerState] == iTunesEPlSPlaying) {
                    msg_Dbg(p_intf, "Pause iTunes...");
                    [iTunesApp pause];
                    b_has_itunes_paused = YES;
                }
            }
        }


        /* Declare user activity.
         This wakes the display if it is off, and postpones display sleep according to the users system preferences
         Available from 10.7.3 */
#ifdef MAC_OS_X_VERSION_10_7
        if ([self activeVideoPlayback] && IOPMAssertionDeclareUserActivity)
        {
            CFStringRef reasonForActivity = CFStringCreateWithCString(kCFAllocatorDefault, _("VLC media playback"), kCFStringEncodingUTF8);
            IOPMAssertionDeclareUserActivity(reasonForActivity,
                                             kIOPMUserActiveLocal,
                                             &userActivityAssertionID);
            CFRelease(reasonForActivity);
        }
#endif

        /* prevent the system from sleeping */
        if (systemSleepAssertionID > 0) {
            msg_Dbg(VLCIntf, "releasing old sleep blocker (%i)" , systemSleepAssertionID);
            IOPMAssertionRelease(systemSleepAssertionID);
        }

        IOReturn success;
        /* work-around a bug in 10.7.4 and 10.7.5, so check for 10.7.x < 10.7.4, 10.8 and 10.6 */
        if ((NSAppKitVersionNumber >= 1115.2 && NSAppKitVersionNumber < 1138.45) || OSX_MOUNTAIN_LION || OSX_SNOW_LEOPARD) {
            CFStringRef reasonForActivity = CFStringCreateWithCString(kCFAllocatorDefault, _("VLC media playback"), kCFStringEncodingUTF8);
            if ([self activeVideoPlayback])
                success = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, reasonForActivity, &systemSleepAssertionID);
            else
                success = IOPMAssertionCreateWithName(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, reasonForActivity, &systemSleepAssertionID);
            CFRelease(reasonForActivity);
        } else {
            /* fall-back on the 10.5 mode, which also works on 10.7.4 and 10.7.5 */
            if ([self activeVideoPlayback])
                success = IOPMAssertionCreate(kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn, &systemSleepAssertionID);
            else
                success = IOPMAssertionCreate(kIOPMAssertionTypeNoIdleSleep, kIOPMAssertionLevelOn, &systemSleepAssertionID);
        }

        if (success == kIOReturnSuccess)
            msg_Dbg(VLCIntf, "prevented sleep through IOKit (%i)", systemSleepAssertionID);
        else
            msg_Warn(VLCIntf, "failed to prevent system sleep through IOKit");

        [[self mainMenu] setPause];
        [o_mainwindow setPause];
    } else {
        [o_mainmenu setSubmenusEnabled: FALSE];
        [[self mainMenu] setPlay];
        [o_mainwindow setPlay];

        /* allow the system to sleep again */
        if (systemSleepAssertionID > 0) {
            msg_Dbg(VLCIntf, "releasing sleep blocker (%i)" , systemSleepAssertionID);
            IOPMAssertionRelease(systemSleepAssertionID);
        }

        if (state == END_S || state == -1) {
            if (i_control_itunes > 0) {
                if (o_itunes_play_timer) {
                    [o_itunes_play_timer invalidate];
                }
                o_itunes_play_timer = [NSTimer scheduledTimerWithTimeInterval: 0.5
                                                                       target: self
                                                                     selector: @selector(resumeItunesPlayback:)
                                                                     userInfo: nil
                                                                      repeats: NO];
            }
        }
    }

    [[VLCMain sharedInstance] performSelectorOnMainThread:@selector(updateMainWindow) withObject: nil waitUntilDone: NO];
    [self performSelectorOnMainThread:@selector(sendDistributedNotificationWithUpdatedPlaybackStatus) withObject: nil waitUntilDone: NO];
}

- (void)sendDistributedNotificationWithUpdatedPlaybackStatus
{
    [[NSDistributedNotificationCenter defaultCenter] postNotificationName:@"VLCPlayerStateDidChange"
                                                                   object:nil
                                                                 userInfo:nil
                                                       deliverImmediately:YES];
}

- (void)playbackModeUpdated
{
    vlc_value_t looping,repeating;
    playlist_t * p_playlist = pl_Get(VLCIntf);

    bool loop = var_GetBool(p_playlist, "loop");
    bool repeat = var_GetBool(p_playlist, "repeat");
    if (repeat) {
        [[o_mainwindow controlsBar] setRepeatOne];
        [o_mainmenu setRepeatOne];
    } else if (loop) {
        [[o_mainwindow controlsBar] setRepeatAll];
        [o_mainmenu setRepeatAll];
    } else {
        [[o_mainwindow controlsBar] setRepeatOff];
        [o_mainmenu setRepeatOff];
    }

    [[o_mainwindow controlsBar] setShuffle];
    [o_mainmenu setShuffle];
}

#pragma mark -
#pragma mark Window updater

- (void)setActiveVideoPlayback:(BOOL)b_value
{
    b_active_videoplayback = b_value;
    if (o_mainwindow) {
        [o_mainwindow performSelectorOnMainThread:@selector(setVideoplayEnabled) withObject:nil waitUntilDone:YES];
        [o_mainwindow performSelectorOnMainThread:@selector(togglePlaylist:) withObject:nil waitUntilDone:NO];
    }

    // update sleep blockers
    [self performSelectorOnMainThread:@selector(playbackStatusUpdated) withObject:nil waitUntilDone:NO];
}

#pragma mark -
#pragma mark Other objects getters

- (id)mainMenu
{
    return o_mainmenu;
}

- (VLCMainWindow *)mainWindow
{
    return o_mainwindow;
}

- (id)controls
{
    if (o_controls)
        return o_controls;

    return nil;
}

- (id)bookmarks
{
    if (!o_bookmarks)
        o_bookmarks = [[VLCBookmarks alloc] init];

    if (!nib_bookmarks_loaded)
        nib_bookmarks_loaded = [NSBundle loadNibNamed:@"Bookmarks" owner: NSApp];

    return o_bookmarks;
}

- (id)open
{
    if (!o_open)
        return nil;

    if (!nib_open_loaded)
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: NSApp];

    return o_open;
}

- (id)simplePreferences
{
    if (!o_sprefs)
        o_sprefs = [[VLCSimplePrefs alloc] init];

    if (!nib_prefs_loaded)
        nib_prefs_loaded = [NSBundle loadNibNamed:@"Preferences" owner: NSApp];

    return o_sprefs;
}

- (id)preferences
{
    if (!o_prefs)
        o_prefs = [[VLCPrefs alloc] init];

    if (!nib_prefs_loaded)
        nib_prefs_loaded = [NSBundle loadNibNamed:@"Preferences" owner: NSApp];

    return o_prefs;
}

- (id)playlist
{
    if (o_playlist)
        return o_playlist;

    return nil;
}

- (id)info
{
    if (! nib_info_loaded)
        nib_info_loaded = [NSBundle loadNibNamed:@"MediaInfo" owner: NSApp];

    if (o_info)
        return o_info;

    return nil;
}

- (id)wizard
{
    if (!o_wizard)
        o_wizard = [[VLCWizard alloc] init];

    if (!nib_wizard_loaded) {
        nib_wizard_loaded = [NSBundle loadNibNamed:@"Wizard" owner: NSApp];
        [o_wizard initStrings];
    }
    return o_wizard;
}

- (id)coreDialogProvider
{
    if (o_coredialogs)
        return o_coredialogs;

    return nil;
}

- (id)eyeTVController
{
    if (o_eyetv)
        return o_eyetv;

    return nil;
}

- (id)appleRemoteController
{
    return o_remote;
}

- (BOOL)activeVideoPlayback
{
    return b_active_videoplayback;
}

#pragma mark -
#pragma mark Crash Log
- (void)sendCrashLog:(NSString *)crashLog withUserComment:(NSString *)userComment
{
    NSString *urlStr = @"http://crash.videolan.org/crashlog/sendcrashreport.php";
    NSURL *url = [NSURL URLWithString:urlStr];

    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:url];
    [req setHTTPMethod:@"POST"];

    NSString * email;
    if ([o_crashrep_includeEmail_ckb state] == NSOnState) {
        ABPerson * contact = [[ABAddressBook sharedAddressBook] me];
        ABMultiValue *emails = [contact valueForProperty:kABEmailProperty];
        email = [emails valueAtIndex:[emails indexForIdentifier:
                    [emails primaryIdentifier]]];
    }
    else
        email = [NSString string];

    NSString *postBody;
    postBody = [NSString stringWithFormat:@"CrashLog=%@&Comment=%@&Email=%@\r\n",
            [crashLog stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
            [userComment stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding],
            [email stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];

    [req setHTTPBody:[postBody dataUsingEncoding:NSUTF8StringEncoding]];

    /* Released from delegate */
    crashLogURLConnection = [[NSURLConnection alloc] initWithRequest:req delegate:self];
}

- (void)connectionDidFinishLoading:(NSURLConnection *)connection
{
    msg_Dbg(p_intf, "crash report successfully sent");
    [crashLogURLConnection release];
    crashLogURLConnection = nil;
}

- (void)connection:(NSURLConnection *)connection didFailWithError:(NSError *)error
{
    msg_Warn (p_intf, "Error when sending the crash report: %s (%li)", [[error localizedDescription] UTF8String], [error code]);
    [crashLogURLConnection release];
    crashLogURLConnection = nil;
}

- (NSString *)latestCrashLogPathPreviouslySeen:(BOOL)previouslySeen
{
    NSString * crashReporter;
    if (OSX_MOUNTAIN_LION)
        crashReporter = [@"~/Library/Logs/DiagnosticReports" stringByExpandingTildeInPath];
    else
        crashReporter = [@"~/Library/Logs/CrashReporter" stringByExpandingTildeInPath];
    NSDirectoryEnumerator *direnum = [[NSFileManager defaultManager] enumeratorAtPath:crashReporter];
    NSString *fname;
    NSString * latestLog = nil;
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    int year  = !previouslySeen ? [defaults integerForKey:@"LatestCrashReportYear"] : 0;
    int month = !previouslySeen ? [defaults integerForKey:@"LatestCrashReportMonth"]: 0;
    int day   = !previouslySeen ? [defaults integerForKey:@"LatestCrashReportDay"]  : 0;
    int hours = !previouslySeen ? [defaults integerForKey:@"LatestCrashReportHours"]: 0;

    while (fname = [direnum nextObject]) {
        [direnum skipDescendents];
        if ([fname hasPrefix:@"VLC"] && [fname hasSuffix:@"crash"]) {
            NSArray * compo = [fname componentsSeparatedByString:@"_"];
            if ([compo count] < 3)
                continue;
            compo = [[compo objectAtIndex:1] componentsSeparatedByString:@"-"];
            if ([compo count] < 4)
                continue;

            // Dooh. ugly.
            if (year < [[compo objectAtIndex:0] intValue] ||
                (year ==[[compo objectAtIndex:0] intValue] &&
                 (month < [[compo objectAtIndex:1] intValue] ||
                  (month ==[[compo objectAtIndex:1] intValue] &&
                   (day   < [[compo objectAtIndex:2] intValue] ||
                    (day   ==[[compo objectAtIndex:2] intValue] &&
                      hours < [[compo objectAtIndex:3] intValue])))))) {
                year  = [[compo objectAtIndex:0] intValue];
                month = [[compo objectAtIndex:1] intValue];
                day   = [[compo objectAtIndex:2] intValue];
                hours = [[compo objectAtIndex:3] intValue];
                latestLog = [crashReporter stringByAppendingPathComponent:fname];
            }
        }
    }

    if (!(latestLog && [[NSFileManager defaultManager] fileExistsAtPath:latestLog]))
        return nil;

    if (!previouslySeen) {
        [defaults setInteger:year  forKey:@"LatestCrashReportYear"];
        [defaults setInteger:month forKey:@"LatestCrashReportMonth"];
        [defaults setInteger:day   forKey:@"LatestCrashReportDay"];
        [defaults setInteger:hours forKey:@"LatestCrashReportHours"];
    }
    return latestLog;
}

- (NSString *)latestCrashLogPath
{
    return [self latestCrashLogPathPreviouslySeen:YES];
}

- (void)lookForCrashLog
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    // This pref key doesn't exists? this VLC is an upgrade, and this crash log come from previous version
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    BOOL areCrashLogsTooOld = ![defaults integerForKey:@"LatestCrashReportYear"];
    NSString * latestLog = [self latestCrashLogPathPreviouslySeen:NO];
    if (latestLog && !areCrashLogsTooOld) {
        if ([defaults integerForKey:@"AlwaysSendCrashReports"] > 0)
            [self sendCrashLog:[NSString stringWithContentsOfFile: [self latestCrashLogPath] encoding: NSUTF8StringEncoding error: NULL] withUserComment: [o_crashrep_fld string]];
        else if ([defaults integerForKey:@"AlwaysSendCrashReports"] == 0)
            [NSApp runModalForWindow: o_crashrep_win];
        // bail out, the user doesn't want us to send reports
    }

    [o_pool release];
}

- (IBAction)crashReporterAction:(id)sender
{
    if (sender == o_crashrep_send_btn) {
        [self sendCrashLog:[NSString stringWithContentsOfFile: [self latestCrashLogPath] encoding: NSUTF8StringEncoding error: NULL] withUserComment: [o_crashrep_fld string]];
        if ([o_crashrep_dontaskagain_ckb state])
            [[NSUserDefaults standardUserDefaults] setInteger:1 forKey:@"AlwaysSendCrashReports"];
    } else {
        if ([o_crashrep_dontaskagain_ckb state])
            [[NSUserDefaults standardUserDefaults] setInteger:-1 forKey:@"AlwaysSendCrashReports"];
    }

    [NSApp stopModal];
    [o_crashrep_win orderOut: sender];
}

- (IBAction)openCrashLog:(id)sender
{
    NSString * latestLog = [self latestCrashLogPath];
    if (latestLog) {
        [[NSWorkspace sharedWorkspace] openFile: latestLog withApplication: @"Console"];
    } else {
        NSBeginInformationalAlertSheet(_NS("No CrashLog found"), _NS("Continue"), nil, nil, o_msgs_panel, self, NULL, NULL, nil, @"%@", _NS("Couldn't find any trace of a previous crash."));
    }
}

#pragma mark -
#pragma mark Remove old prefs

- (void)removeOldPreferences
{
    static NSString * kVLCPreferencesVersion = @"VLCPreferencesVersion";
    static const int kCurrentPreferencesVersion = 3;
    NSUserDefaults * defaults = [NSUserDefaults standardUserDefaults];
    int version = [defaults integerForKey:kVLCPreferencesVersion];
    if (version >= kCurrentPreferencesVersion)
        return;

    if (version == 1) {
        [defaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
        [defaults synchronize];

        if (![[VLCCoreInteraction sharedInstance] fixPreferences])
            return;
        else
            config_SaveConfigFile(VLCIntf); // we need to do manually, since we won't quit libvlc cleanly
    } else if (version == 2) {
        /* version 2 (used by VLC 2.0.x and early versions of 2.1) can lead to exceptions within 2.1 or later
         * so we reset the OS X specific prefs here - in practice, no user will notice */
        [NSUserDefaults resetStandardUserDefaults];

        [defaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
        [defaults synchronize];
    } else {
        NSArray *libraries = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory,
            NSUserDomainMask, YES);
        if (!libraries || [libraries count] == 0) return;
        NSString * preferences = [[libraries objectAtIndex:0] stringByAppendingPathComponent:@"Preferences"];

        /* File not found, don't attempt anything */
        if (![[NSFileManager defaultManager] fileExistsAtPath:[preferences stringByAppendingPathComponent:@"org.videolan.vlc"]] &&
           ![[NSFileManager defaultManager] fileExistsAtPath:[preferences stringByAppendingPathComponent:@"org.videolan.vlc.plist"]]) {
            [defaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
            return;
        }

        int res = NSRunInformationalAlertPanel(_NS("Remove old preferences?"),
                    _NS("We just found an older version of VLC's preferences files."),
                    _NS("Move To Trash and Relaunch VLC"), _NS("Ignore"), nil, nil);
        if (res != NSOKButton) {
            [defaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
            return;
        }

        NSArray * ourPreferences = @[@"org.videolan.vlc.plist", @"VLC", @"org.videolan.vlc"];

        /* Move the file to trash so that user can find them later */
        [[NSWorkspace sharedWorkspace] performFileOperation:NSWorkspaceRecycleOperation source:preferences destination:nil files:ourPreferences tag:0];

        /* really reset the defaults from now on */
        [NSUserDefaults resetStandardUserDefaults];

        [defaults setInteger:kCurrentPreferencesVersion forKey:kVLCPreferencesVersion];
        [defaults synchronize];
    }

    /* Relaunch now */
    const char * path = [[[NSBundle mainBundle] executablePath] UTF8String];

    /* For some reason we need to fork(), not just execl(), which reports a ENOTSUP then. */
    if (fork() != 0) {
        exit(0);
        return;
    }
    execl(path, path, NULL);
}

#pragma mark -
#pragma mark Errors, warnings and messages
- (IBAction)updateMessagesPanel:(id)sender
{
    [self windowDidBecomeKey:nil];
}

- (IBAction)showMessagesPanel:(id)sender
{
    /* subscribe to LibVLCCore's messages */
    vlc_LogSet(p_intf->p_libvlc, MsgCallback, NULL);

    /* show panel */
    [o_msgs_panel makeKeyAndOrderFront: sender];
}

- (void)windowDidBecomeKey:(NSNotification *)o_notification
{
    [o_msgs_table reloadData];
    [o_msgs_table scrollRowToVisible: [o_msg_arr count] - 1];
}

- (void)windowWillClose:(NSNotification *)o_notification
{
    /* unsubscribe from LibVLCCore's messages */
    vlc_LogSet( p_intf->p_libvlc, NULL, NULL );
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    if (aTableView == o_msgs_table)
        return [o_msg_arr count];
    return 0;
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
    NSMutableAttributedString *result = NULL;

    [o_msg_lock lock];
    if (rowIndex < [o_msg_arr count])
        result = [o_msg_arr objectAtIndex:rowIndex];
    [o_msg_lock unlock];

    if (result != NULL)
        return result;
    else
        return @"";
}

- (void)processReceivedlibvlcMessage:(const vlc_log_t *) item ofType: (int)i_type withStr: (char *)str
{
    if (o_msg_arr) {
        NSColor *o_white = [NSColor whiteColor];
        NSColor *o_red = [NSColor redColor];
        NSColor *o_yellow = [NSColor yellowColor];
        NSColor *o_gray = [NSColor grayColor];
        NSString * firstString, * secondString;

        NSColor * pp_color[4] = { o_white, o_red, o_yellow, o_gray };
        static const char * ppsz_type[4] = { ": ", " error: ", " warning: ", " debug: " };

        NSDictionary *o_attr;
        NSMutableAttributedString *o_msg_color;

        [o_msg_lock lock];

        if ([o_msg_arr count] > 600) {
            [o_msg_arr removeObjectAtIndex: 0];
            [o_msg_arr removeObjectAtIndex: 1];
        }
        firstString = [NSString stringWithFormat:@"%s%s", item->psz_module, ppsz_type[i_type]];
        secondString = [NSString stringWithFormat:@"%@%s\n", firstString, str];

        o_attr = [NSDictionary dictionaryWithObject: pp_color[i_type]  forKey: NSForegroundColorAttributeName];
        o_msg_color = [[NSMutableAttributedString alloc] initWithString: secondString attributes: o_attr];
        o_attr = [NSDictionary dictionaryWithObject: pp_color[3] forKey: NSForegroundColorAttributeName];
        [o_msg_color setAttributes: o_attr range: NSMakeRange(0, [firstString length])];
        [o_msg_arr addObject: [o_msg_color autorelease]];

        b_msg_arr_changed = YES;
        [o_msg_lock unlock];
    }
}

- (IBAction)saveDebugLog:(id)sender
{
    NSSavePanel * saveFolderPanel = [[NSSavePanel alloc] init];

    [saveFolderPanel setCanSelectHiddenExtension: NO];
    [saveFolderPanel setCanCreateDirectories: YES];
    [saveFolderPanel setAllowedFileTypes: @[@"rtf"]];
    [saveFolderPanel setNameFieldStringValue:[NSString stringWithFormat: _NS("VLC Debug Log (%s).rtf"), VERSION_MESSAGE]];
    [saveFolderPanel beginSheetModalForWindow: o_msgs_panel completionHandler:^(NSInteger returnCode) {
        if (returnCode == NSOKButton) {
            NSUInteger count = [o_msg_arr count];
            NSMutableAttributedString * string = [[NSMutableAttributedString alloc] init];
            for (NSUInteger i = 0; i < count; i++)
                [string appendAttributedString: [o_msg_arr objectAtIndex:i]];

            NSData *data = [string RTFFromRange:NSMakeRange(0, [string length])
                             documentAttributes:[NSDictionary dictionaryWithObject: NSRTFTextDocumentType forKey: NSDocumentTypeDocumentAttribute]];

            if ([data writeToFile: [[saveFolderPanel URL] path] atomically: YES] == NO)
                msg_Warn(p_intf, "Error while saving the debug log");

            [string release];
        }
    }];
    [saveFolderPanel release];
}

#pragma mark -
#pragma mark Playlist toggling

- (void)updateTogglePlaylistState
{
    [[self playlist] outlineViewSelectionDidChange: NULL];
}

#pragma mark -

@end

@implementation VLCMain (Internal)

- (void)handlePortMessage:(NSPortMessage *)o_msg
{
    id ** val;
    NSData * o_data;
    NSValue * o_value;
    NSInvocation * o_inv;
    NSConditionLock * o_lock;

    o_data = [[o_msg components] lastObject];
    o_inv = *((NSInvocation **)[o_data bytes]);
    [o_inv getArgument: &o_value atIndex: 2];
    val = (id **)[o_value pointerValue];
    [o_inv setArgument: val[1] atIndex: 2];
    o_lock = *(val[0]);

    [o_lock lock];
    [o_inv invoke];
    [o_lock unlockWithCondition: 1];
}

- (void)resetMediaKeyJump
{
    b_mediakeyJustJumped = NO;
}

- (void)coreChangedMediaKeySupportSetting: (NSNotification *)o_notification
{
    b_mediaKeySupport = var_InheritBool(VLCIntf, "macosx-mediakeys");
    if (b_mediaKeySupport) {
        if (!o_mediaKeyController)
            o_mediaKeyController = [[SPMediaKeyTap alloc] initWithDelegate:self];

        if ([[[VLCMain sharedInstance] playlist] currentPlaylistRoot]->i_children > 0 ||
            p_current_input)
            [o_mediaKeyController startWatchingMediaKeys];
        else
            [o_mediaKeyController stopWatchingMediaKeys];
    }
    else if (!b_mediaKeySupport && o_mediaKeyController)
        [o_mediaKeyController stopWatchingMediaKeys];
}

@end

/*****************************************************************************
 * VLCApplication interface
 *****************************************************************************/

@implementation VLCApplication
// when user selects the quit menu from dock it sends a terminate:
// but we need to send a stop: to properly exits libvlc.
// However, we are not able to change the action-method sent by this standard menu item.
// thus we override terminate: to send a stop:
// see [af97f24d528acab89969d6541d83f17ce1ecd580] that introduced the removal of setjmp() and longjmp()
- (void)terminate:(id)sender
{
    [self activateIgnoringOtherApps:YES];
    [self stop:sender];
}

@end
