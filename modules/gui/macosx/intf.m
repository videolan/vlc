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
#include <vlc_variables.h>

#import "CompatibilityFixes.h"
#import "InputManager.h"
#import "MainMenu.h"
#import "VideoView.h"
#import "prefs.h"
#import "VLCPlaylist.h"
#import "VLCPlaylistInfo.h"
#import "VLCPlaylistInfo.h"
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
#import "ConvertAndSave.h"

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

static intf_thread_t *p_interface_thread;

intf_thread_t *getIntf()
{
    return p_interface_thread;
}

int OpenIntf (vlc_object_t *p_this)
{
    @autoreleasepool {
        intf_thread_t *p_intf = (intf_thread_t*) p_this;
        p_interface_thread = p_intf;
        msg_Dbg(p_intf, "Starting macosx interface");

        [VLCApplication sharedApplication];
        [VLCMain sharedInstance];

        [NSBundle loadNibNamed:@"MainMenu" owner:[[VLCMain sharedInstance] mainMenu]];
        // if statusbar enabled in preferences
        if (var_InheritBool(p_intf, "macosx-statusicon")) {
            [NSBundle loadNibNamed:@"VLCStatusBarIconMainMenu" owner:[[VLCMain sharedInstance] statusBarIcon]];
        }
        [[[VLCMain sharedInstance] mainWindow] makeKeyAndOrderFront:nil];

        msg_Dbg(p_intf, "Finished loading macosx interface");
        return VLC_SUCCESS;
    }
}

void CloseIntf (vlc_object_t *p_this)
{
    @autoreleasepool {
        msg_Dbg(p_this, "Closing macosx interface");
        [[VLCMain sharedInstance] applicationWillTerminate:nil];
        [VLCMain killInstance];

        p_interface_thread = nil;
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

            intf_thread_t * p_intf = getIntf();
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

@interface VLCMain () <BWQuincyManagerDelegate
#ifdef HAVE_SPARKLE
    , SUUpdaterDelegate
#endif
>
{
    intf_thread_t *p_intf;
    BOOL launched;
    int items_at_launch;

    BOOL b_active_videoplayback;

    NSWindowController *_mainWindowController;
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
    VLCStatusBarIcon *_statusBarIcon;
    VLCTrackSynchronization *_trackSyncPanel;
    VLCAudioEffects *_audioEffectsPanel;
    VLCVideoEffects *_videoEffectsPanel;
    VLCConvertAndSave *_convertAndSaveWindow;
    ExtensionsManager *_extensionsManager;
    VLCInfo *_currentMediaInfoPanel;

    bool b_intf_terminating; /* Makes sure applicationWillTerminate will be called only once */
}

@end

/*****************************************************************************
 * VLCMain implementation
 *****************************************************************************/
@implementation VLCMain

#pragma mark -
#pragma mark Initialization

static VLCMain *sharedInstance = nil;

+ (VLCMain *)sharedInstance;
{
    static dispatch_once_t pred;
    dispatch_once(&pred, ^{
        sharedInstance = [[VLCMain alloc] init];
    });

    return sharedInstance;
}

+ (void)killInstance
{
    sharedInstance = nil;
}

- (id)init
{
    self = [super init];
    if (self) {
        p_intf = getIntf();

        [VLCApplication sharedApplication].delegate = self;

        _input_manager = [[VLCInputManager alloc] initWithMain:self];

        // first initalize extensions dialog provider, then core dialog
        // provider which will register both at the core
        _extensionsManager = [[ExtensionsManager alloc] init];
        _coredialogs = [[VLCCoreDialogProvider alloc] init];

        _mainmenu = [[VLCMainMenu alloc] init];

        // if statusbar enabled in preferences
        if (var_InheritBool(p_intf, "macosx-statusicon")) {
            _statusBarIcon = [[VLCStatusBarIcon  alloc] init];
        }

        _voutController = [[VLCVoutWindowController alloc] init];
        _playlist = [[VLCPlaylist alloc] init];

        _mainWindowController = [[NSWindowController alloc] initWithWindowNibName:@"MainWindow"];

        var_Create(p_intf, "intf-change", VLC_VAR_BOOL);

        var_AddCallback(p_intf->obj.libvlc, "intf-toggle-fscontrol", ShowController, (__bridge void *)self);
        var_AddCallback(p_intf->obj.libvlc, "intf-show", ShowController, (__bridge void *)self);

        playlist_t *p_playlist = pl_Get(p_intf);
        if ([NSApp currentSystemPresentationOptions] & NSApplicationPresentationFullScreen)
            var_SetBool(p_playlist, "fullscreen", YES);

        _nativeFullscreenMode = var_InheritBool(p_intf, "macosx-nativefullscreenmode");

        if (var_InheritInteger(p_intf, "macosx-icon-change")) {
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

        /* announce our launch to a potential eyetv plugin */
        [[NSDistributedNotificationCenter defaultCenter] postNotificationName: @"VLCOSXGUIInit"
                                                                       object: @"VLCEyeTVSupport"
                                                                     userInfo: NULL
                                                           deliverImmediately: YES];

    }

    return self;
}

- (void)dealloc
{
    msg_Dbg(getIntf(), "Deinitializing VLCMain object");
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
    _coreinteraction = [VLCCoreInteraction sharedInstance];

    playlist_t * p_playlist = pl_Get(getIntf());
    PL_LOCK;
    items_at_launch = p_playlist->p_local_category->i_children;
    PL_UNLOCK;

#ifdef HAVE_SPARKLE
    [[SUUpdater sharedUpdater] setDelegate:self];
#endif
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
    [[self mainWindow] updateWindow];
    [[self mainWindow] updateTimeSlider];
    [[self mainWindow] updateVolumeSlider];

    // respect playlist-autostart
    // note that PLAYLIST_PLAY will not stop any playback if already started
    playlist_t * p_playlist = pl_Get(getIntf());
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
    [[self videoEffectsPanel] saveCurrentProfile];
    [[self audioEffectsPanel] saveCurrentProfile];

    /* Save some interface state in configuration, at module quit */
    config_PutInt(p_intf, "random", var_GetBool(p_playlist, "random"));
    config_PutInt(p_intf, "loop", var_GetBool(p_playlist, "loop"));
    config_PutInt(p_intf, "repeat", var_GetBool(p_playlist, "repeat"));

    msg_Dbg(p_intf, "Terminating");

    var_DelCallback(p_intf->obj.libvlc, "intf-toggle-fscontrol", ShowController, (__bridge void *)self);
    var_DelCallback(p_intf->obj.libvlc, "intf-show", ShowController, (__bridge void *)self);

    [[NSNotificationCenter defaultCenter] removeObserver: self];

    [_voutController.lock lock];
    // closes all open vouts
    _voutController = nil;
    [_voutController.lock unlock];

    /* write cached user defaults to disk */
    [[NSUserDefaults standardUserDefaults] synchronize];
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
        input_thread_t * p_input = pl_CurrentInput(getIntf());
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
        [[self mainWindow] makeKeyAndOrderFront:self];

    return YES;
}

- (void)showFullscreenController
{
    // defer selector here (possibly another time) to ensure that keyWindow is set properly
    // (needed for NSApplicationDidBecomeActiveNotification)
    [[self mainWindow] performSelectorOnMainThread:@selector(showFullscreenController) withObject:nil waitUntilDone:NO];
}

- (void)setActiveVideoPlayback:(BOOL)b_value
{
    assert([NSThread isMainThread]);

    b_active_videoplayback = b_value;
    if ([self mainWindow]) {
        [[self mainWindow] setVideoplayEnabled];
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

- (VLCStatusBarIcon *)statusBarIcon
{
    return _statusBarIcon;
}

- (VLCMainWindow *)mainWindow
{
    return (VLCMainWindow *)[_mainWindowController window];
}

- (VLCInputManager *)inputManager
{
    return _input_manager;
}

- (ExtensionsManager *)extensionsManager
{
    return _extensionsManager;
}

- (VLCDebugMessageVisualizer *)debugMsgPanel
{
    if (!_messagePanelController)
        _messagePanelController = [[VLCDebugMessageVisualizer alloc] init];

    return _messagePanelController;
}

- (VLCTrackSynchronization *)trackSyncPanel
{
    if (!_trackSyncPanel)
        _trackSyncPanel = [[VLCTrackSynchronization alloc] init];

    return _trackSyncPanel;
}

- (VLCAudioEffects *)audioEffectsPanel
{
    if (!_audioEffectsPanel)
        _audioEffectsPanel = [[VLCAudioEffects alloc] init];

    return _audioEffectsPanel;
}

- (VLCVideoEffects *)videoEffectsPanel
{
    if (!_videoEffectsPanel)
        _videoEffectsPanel = [[VLCVideoEffects alloc] init];

    return _videoEffectsPanel;
}

- (VLCInfo *)currentMediaInfoPanel;
{
    if (!_currentMediaInfoPanel)
        _currentMediaInfoPanel = [[VLCInfo alloc] init];

    return _currentMediaInfoPanel;
}

- (VLCBookmarks *)bookmarks
{
    if (!_bookmarks)
        _bookmarks = [[VLCBookmarks alloc] init];

    return _bookmarks;
}

- (VLCOpen *)open
{
    if (!_open)
        _open = [[VLCOpen alloc] init];

    return _open;
}

- (VLCConvertAndSave *)convertAndSaveWindow
{
    if (_convertAndSaveWindow == nil)
        _convertAndSaveWindow = [[VLCConvertAndSave alloc] init];

    return _convertAndSaveWindow;
}

- (VLCSimplePrefs *)simplePreferences
{
    if (!_sprefs)
        _sprefs = [[VLCSimplePrefs alloc] init];

    return _sprefs;
}

- (VLCPrefs *)preferences
{
    if (!_prefs)
        _prefs = [[VLCPrefs alloc] init];

    return _prefs;
}

- (VLCPlaylist *)playlist
{
    return _playlist;
}

- (VLCCoreDialogProvider *)coreDialogProvider
{
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
