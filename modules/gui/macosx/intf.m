/*****************************************************************************
 * intf.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan.org>
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

#import "intf.h"

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>
#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_keys.h>
#include <vlc_dialog.h>
#include <vlc_url.h>
#include <unistd.h> /* execl() */

#import "CompatibilityFixes.h"
#import "InputManager.h"
#import "MainMenu.h"
#import "VideoView.h"
#import "prefs.h"
#import "playlist.h"
#import "playlistinfo.h"
#import "open.h"
#import "bookmarks.h"
#import "coredialogs.h"
#import "simple_prefs.h"
#import "CoreInteraction.h"
#import "TrackSynchronization.h"
#import "ExtensionsManager.h"
#import "BWQuincyManager.h"
#import "ResumeDialogController.h"
#import "DebugMessageVisualizer.h"

#import "VideoEffects.h"
#import "AudioEffects.h"
#import "intf-prefs.h"

#ifdef HAVE_SPARKLE
#import <Sparkle/Sparkle.h>                 /* we're the update delegate */
#endif

#pragma mark -
#pragma mark VLC Interface Object Callbacks

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
int OpenIntf (vlc_object_t *p_this)
{
    @autoreleasepool {
        intf_thread_t *p_intf = (intf_thread_t*) p_this;
        msg_Dbg(p_intf, "Starting macosx interface");

        [VLCApplication sharedApplication];
        [[VLCMain sharedInstance] setIntf: p_intf];

        [NSBundle loadNibNamed:@"MainMenu" owner:[[VLCMain sharedInstance] mainMenu]];
        [NSBundle loadNibNamed:@"MainWindow" owner:[VLCMain sharedInstance]];
        [[[VLCMain sharedInstance] mainWindow] makeKeyAndOrderFront:nil];

        return VLC_SUCCESS;
    }
}

void CloseIntf (vlc_object_t *p_this)
{
    @autoreleasepool {
        msg_Dbg(p_this, "Closing macosx interface");
        [[VLCMain sharedInstance] applicationWillTerminate:nil];
    }
}

#pragma mark -
#pragma mark Variables Callback

/*****************************************************************************
 * ShowController: Callback triggered by the show-intf playlist variable
 * through the ShowIntf-control-intf, to let us show the controller-win;
 * usually when in fullscreen-mode
 *****************************************************************************/
static int ShowController(vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param)
{
    @autoreleasepool {
        dispatch_async(dispatch_get_main_queue(), ^{

            intf_thread_t * p_intf = VLCIntf;
            if (p_intf) {
                playlist_t * p_playlist = pl_Get(p_intf);
                BOOL b_fullscreen = var_GetBool(p_playlist, "fullscreen");
                if (b_fullscreen)
                    [[VLCMain sharedInstance] showFullscreenController];

                else if (!strcmp(psz_variable, "intf-show"))
                    [[[VLCMain sharedInstance] mainWindow] makeKeyAndOrderFront:nil];
            }

        });

        return VLC_SUCCESS;
    }
}

#pragma mark -
#pragma mark Private

@interface VLCMain () <BWQuincyManagerDelegate>
{
    intf_thread_t *p_intf;
    BOOL launched;
    int items_at_launch;

    BOOL nib_main_loaded;       /* main nibfile */
    BOOL nib_open_loaded;       /* open nibfile */
    BOOL nib_about_loaded;      /* about nibfile */
    BOOL nib_prefs_loaded;      /* preferences xibfile */
    BOOL nib_sprefs_loaded;      /* simple preferences xibfile */
    BOOL nib_coredialogs_loaded; /* CoreDialogs nibfile */
    BOOL nib_bookmarks_loaded;   /* Bookmarks nibfile */
    BOOL b_active_videoplayback;

    VLCMainMenu *_mainmenu;
    VLCPrefs *_prefs;
    VLCSimplePrefs *_sprefs;
    VLCOpen *_open;
    VLCCoreDialogProvider *_coredialogs;
    VLCBookmarks *_bookmarks;
    VLCCoreInteraction *_coreinteraction;
    ResumeDialogController *_resume_dialog;
    VLCInputManager *_input_manager;
    VLCPlaylist *_playlist;
    VLCDebugMessageVisualizer *_messagePanelController;

    bool b_intf_terminating; /* Makes sure applicationWillTerminate will be called only once */
}

@end

/*****************************************************************************
 * VLCMain implementation
 *****************************************************************************/
@implementation VLCMain

#pragma mark -
#pragma mark Initialization

+ (VLCMain *)sharedInstance
{
    static VLCMain *sharedInstance = nil;
    static dispatch_once_t pred;

    dispatch_once(&pred, ^{
        sharedInstance = [VLCMain new];
    });

    return sharedInstance;
}

- (id)init
{
    self = [super init];

    if (self) {
        p_intf = NULL;

        [VLCApplication sharedApplication].delegate = self;

        /* announce our launch to a potential eyetv plugin */
        [[NSDistributedNotificationCenter defaultCenter] postNotificationName: @"VLCOSXGUIInit"
                                                                       object: @"VLCEyeTVSupport"
                                                                     userInfo: NULL
                                                           deliverImmediately: YES];

        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSDictionary *appDefaults = [NSDictionary dictionaryWithObject:@"NO" forKey:@"LiveUpdateTheMessagesPanel"];
        [defaults registerDefaults:appDefaults];

        _mainmenu = [[VLCMainMenu alloc] init];
        _voutController = [[VLCVoutWindowController alloc] init];
        _playlist = [[VLCPlaylist alloc] init];
    }

    return self;
}

- (void)setIntf: (intf_thread_t *)p_mainintf
{
    p_intf = p_mainintf;
}

- (intf_thread_t *)intf
{
    return p_intf;
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
    _open = [[VLCOpen alloc] init];
    _coreinteraction = [VLCCoreInteraction sharedInstance];

    playlist_t * p_playlist = pl_Get(VLCIntf);
    PL_LOCK;
    items_at_launch = p_playlist->p_local_category->i_children;
    PL_UNLOCK;

#ifdef HAVE_SPARKLE
    [[SUUpdater sharedUpdater] setDelegate:self];
#endif
}

- (void)awakeFromNib
{
    if (!p_intf) return;
    var_Create(p_intf, "intf-change", VLC_VAR_BOOL);

    /* Check if we already did this once */
    if (nib_main_loaded)
        return;

    // TODO: take care of VLCIntf initialization order
    _input_manager = [[VLCInputManager alloc] initWithMain:self];

    var_AddCallback(p_intf->p_libvlc, "intf-toggle-fscontrol", ShowController, (__bridge void *)self);
    var_AddCallback(p_intf->p_libvlc, "intf-show", ShowController, (__bridge void *)self);

    playlist_t *p_playlist = pl_Get(p_intf);
    if ([NSApp currentSystemPresentationOptions] & NSApplicationPresentationFullScreen)
        var_SetBool(p_playlist, "fullscreen", YES);

    /* load our Shared Dialogs nib */
    [NSBundle loadNibNamed:@"SharedDialogs" owner: NSApp];

    _nativeFullscreenMode = var_InheritBool(p_intf, "macosx-nativefullscreenmode");

    if (config_GetInt(VLCIntf, "macosx-icon-change")) {
        /* After day 354 of the year, the usual VLC cone is replaced by another cone
         * wearing a Father Xmas hat.
         * Note: this icon doesn't represent an endorsement of The Coca-Cola Company.
         */
        NSCalendar *gregorian =
        [[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar];
        NSUInteger dayOfYear = [gregorian ordinalityOfUnit:NSDayCalendarUnit inUnit:NSYearCalendarUnit forDate:[NSDate date]];

        if (dayOfYear >= 354)
            [[VLCApplication sharedApplication] setApplicationIconImage: [NSImage imageNamed:@"vlc-xmas"]];
    }

    nib_main_loaded = TRUE;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification
{
    launched = YES;

    if (!p_intf)
        return;

    NSString *appVersion = [[[NSBundle mainBundle] infoDictionary] valueForKey: @"CFBundleVersion"];
    NSRange endRande = [appVersion rangeOfString:@"-"];
    if (endRande.location != NSNotFound)
        appVersion = [appVersion substringToIndex:endRande.location];

    BWQuincyManager *quincyManager = [BWQuincyManager sharedQuincyManager];
    [quincyManager setApplicationVersion:appVersion];
    [quincyManager setSubmissionURL:@"http://crash.videolan.org/crash_v200.php"];
    [quincyManager setDelegate:self];
    [quincyManager setCompanyName:@"VideoLAN"];

    [_coreinteraction updateCurrentlyUsedHotkeys];

    [self removeOldPreferences];

    /* Handle sleep notification */
    [[[NSWorkspace sharedWorkspace] notificationCenter] addObserver:self selector:@selector(computerWillSleep:)
           name:NSWorkspaceWillSleepNotification object:nil];

    /* update the main window */
    [o_mainwindow updateWindow];
    [o_mainwindow updateTimeSlider];
    [o_mainwindow updateVolumeSlider];

    /* Hack: Playlist is started before the interface.
     * Thus, call additional updaters as we might miss these events if posted before
     * the callbacks are registered.
     */
    [_input_manager inputThreadChanged];
    [_playlist playbackModeUpdated];

    // respect playlist-autostart
    // note that PLAYLIST_PLAY will not stop any playback if already started
    playlist_t * p_playlist = pl_Get(VLCIntf);
    PL_LOCK;
    BOOL kidsAround = p_playlist->p_local_category->i_children != 0;
    if (kidsAround && var_GetBool(p_playlist, "playlist-autostart"))
        playlist_Control(p_playlist, PLAYLIST_PLAY, true);
    PL_UNLOCK;
}

#pragma mark -
#pragma mark Termination

- (BOOL)isTerminating
{
    return b_intf_terminating;
}

- (void)applicationWillTerminate:(NSNotification *)notification
{
    if (b_intf_terminating)
        return;
    b_intf_terminating = true;

    [_input_manager resumeItunesPlayback:nil];

    if (notification == nil)
        [[NSNotificationCenter defaultCenter] postNotificationName: NSApplicationWillTerminateNotification object: nil];

    playlist_t * p_playlist = pl_Get(p_intf);

    /* save current video and audio profiles */
    [[VLCVideoEffects sharedInstance] saveCurrentProfile];
    [[VLCAudioEffects sharedInstance] saveCurrentProfile];

    /* Save some interface state in configuration, at module quit */
    config_PutInt(p_intf, "random", var_GetBool(p_playlist, "random"));
    config_PutInt(p_intf, "loop", var_GetBool(p_playlist, "loop"));
    config_PutInt(p_intf, "repeat", var_GetBool(p_playlist, "repeat"));

    msg_Dbg(p_intf, "Terminating");

    var_DelCallback(p_intf->p_libvlc, "intf-toggle-fscontrol", ShowController, (__bridge void *)self);
    var_DelCallback(p_intf->p_libvlc, "intf-show", ShowController, (__bridge void *)self);

    [[NSNotificationCenter defaultCenter] removeObserver: self];

    [_voutController.lock lock];
    // closes all open vouts
    _voutController = nil;
    [_voutController.lock unlock];

    /* unsubscribe from libvlc's debug messages */
    vlc_LogSet(p_intf->p_libvlc, NULL, NULL);

    /* write cached user defaults to disk */
    [[NSUserDefaults standardUserDefaults] synchronize];

    o_mainwindow = NULL;

    [self setIntf:nil];
}

#pragma mark -
#pragma mark Sparkle delegate

#ifdef HAVE_SPARKLE
/* received directly before the update gets installed, so let's shut down a bit */
- (void)updater:(SUUpdater *)updater willInstallUpdate:(SUAppcastItem *)update
{
    [NSApp activateIgnoringOtherApps:YES];
    [_coreinteraction stopListeningWithAppleRemote];
    [_coreinteraction stop];
}

/* don't be enthusiastic about an update if we currently play a video */
- (BOOL)updaterMayCheckForUpdates:(SUUpdater *)bundle
{
    if ([self activeVideoPlayback])
        return NO;

    return YES;
}
#endif

#pragma mark -
#pragma mark Other notification

/* Listen to the remote in exclusive mode, only when VLC is the active
   application */
- (void)applicationDidBecomeActive:(NSNotification *)aNotification
{
    if (!p_intf)
        return;
    if (var_InheritBool(p_intf, "macosx-appleremote") == YES)
        [_coreinteraction startListeningWithAppleRemote];
}
- (void)applicationDidResignActive:(NSNotification *)aNotification
{
    if (!p_intf)
        return;
    [_coreinteraction stopListeningWithAppleRemote];
}

/* Triggered when the computer goes to sleep */
- (void)computerWillSleep: (NSNotification *)notification
{
    [_coreinteraction pause];
}

#pragma mark -
#pragma mark File opening over dock icon

- (void)application:(NSApplication *)o_app openFiles:(NSArray *)o_names
{
    // Only add items here which are getting dropped to to the application icon
    // or are given at startup. If a file is passed via command line, libvlccore
    // will add the item, but cocoa also calls this function. In this case, the
    // invocation is ignored here.
    if (launched == NO) {
        if (items_at_launch) {
            int items = [o_names count];
            if (items > items_at_launch)
                items_at_launch = 0;
            else
                items_at_launch -= items;
            return;
        }
    }

    char *psz_uri = vlc_path2uri([[o_names firstObject] UTF8String], NULL);

    // try to add file as subtitle
    if ([o_names count] == 1 && psz_uri) {
        input_thread_t * p_input = pl_CurrentInput(VLCIntf);
        if (p_input) {
            int i_result = input_AddSubtitleOSD(p_input, [[o_names firstObject] UTF8String], true, true);
            vlc_object_release(p_input);
            if (i_result == VLC_SUCCESS) {
                free(psz_uri);
                return;
            }
        }
    }
    free(psz_uri);

    NSArray *o_sorted_names = [o_names sortedArrayUsingSelector: @selector(caseInsensitiveCompare:)];
    NSMutableArray *o_result = [NSMutableArray arrayWithCapacity: [o_sorted_names count]];
    for (NSUInteger i = 0; i < [o_sorted_names count]; i++) {
        psz_uri = vlc_path2uri([[o_sorted_names objectAtIndex:i] UTF8String], "file");
        if (!psz_uri)
            continue;

        NSDictionary *o_dic = [NSDictionary dictionaryWithObject:toNSStr(psz_uri) forKey:@"ITEM_URL"];
        free(psz_uri);
        [o_result addObject: o_dic];
    }

    [[[VLCMain sharedInstance] playlist] addPlaylistItems:o_result];
}

/* When user click in the Dock icon our double click in the finder */
- (BOOL)applicationShouldHandleReopen:(NSApplication *)theApplication hasVisibleWindows:(BOOL)hasVisibleWindows
{
    if (!hasVisibleWindows)
        [o_mainwindow makeKeyAndOrderFront:self];

    return YES;
}

- (void)showFullscreenController
{
    // defer selector here (possibly another time) to ensure that keyWindow is set properly
    // (needed for NSApplicationDidBecomeActiveNotification)
    [o_mainwindow performSelectorOnMainThread:@selector(showFullscreenController) withObject:nil waitUntilDone:NO];
}

- (void)setActiveVideoPlayback:(BOOL)b_value
{
    assert([NSThread isMainThread]);

    b_active_videoplayback = b_value;
    if (o_mainwindow) {
        [o_mainwindow setVideoplayEnabled];
    }

    // update sleep blockers
    [_input_manager playbackStatusUpdated];
}

#pragma mark -
#pragma mark Other objects getters

- (VLCMainMenu *)mainMenu
{
    return _mainmenu;
}

- (VLCMainWindow *)mainWindow
{
    return o_mainwindow;
}

- (VLCInputManager *)inputManager
{
    return _input_manager;
}

- (VLCDebugMessageVisualizer *)debugMsgPanel
{
    if (!_messagePanelController)
        _messagePanelController = [[VLCDebugMessageVisualizer alloc] init];

    return _messagePanelController;
}

- (VLCBookmarks *)bookmarks
{
    if (!_bookmarks)
        _bookmarks = [[VLCBookmarks alloc] init];

    if (!nib_bookmarks_loaded)
        nib_bookmarks_loaded = [NSBundle loadNibNamed:@"Bookmarks" owner:_bookmarks];

    return _bookmarks;
}

- (VLCOpen *)open
{
    if (!nib_open_loaded)
        nib_open_loaded = [NSBundle loadNibNamed:@"Open" owner: _open];

    return _open;
}

- (VLCSimplePrefs *)simplePreferences
{
    if (!_sprefs)
        _sprefs = [[VLCSimplePrefs alloc] init];

    if (!nib_sprefs_loaded)
        nib_sprefs_loaded = [NSBundle loadNibNamed:@"SimplePreferences" owner: _sprefs];

    return _sprefs;
}

- (VLCPrefs *)preferences
{
    if (!_prefs)
        _prefs = [[VLCPrefs alloc] init];

    if (!nib_prefs_loaded)
        nib_prefs_loaded = [NSBundle loadNibNamed:@"Preferences" owner: _prefs];

    return _prefs;
}

- (VLCPlaylist *)playlist
{
    return _playlist;
}

- (VLCCoreDialogProvider *)coreDialogProvider
{
    _coredialogs = [VLCCoreDialogProvider sharedInstance];
    if (!nib_coredialogs_loaded) {
        nib_coredialogs_loaded = [NSBundle loadNibNamed:@"CoreDialogs" owner: _coredialogs];
    }

    return _coredialogs;
}

- (ResumeDialogController *)resumeDialog
{
    if (!_resume_dialog)
        _resume_dialog = [[ResumeDialogController alloc] init];

    return _resume_dialog;
}

- (BOOL)activeVideoPlayback
{
    return b_active_videoplayback;
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
