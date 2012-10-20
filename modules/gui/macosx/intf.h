/*****************************************************************************
 * intf.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#import <vlc_common.h>
#import <vlc_interface.h>
#import <vlc_playlist.h>
#import <vlc_vout.h>
#import <vlc_aout.h>
#import <vlc_input.h>
#import <vlc_vout_window.h>

#import <Cocoa/Cocoa.h>
#import "CompatibilityFixes.h"
#import "SPMediaKeyTap.h"                   /* for the media key support */
#import "misc.h"
#import "MainWindow.h"
#import "StringUtility.h"

#import <IOKit/pwr_mgt/IOPMLib.h>           /* for sleep prevention */

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
#define VLCIntf [[VLCMain sharedInstance] intf]

// You need to release those objects after use
input_thread_t *getInput(void);
vout_thread_t *getVout(void);
vout_thread_t *getVoutForActiveWindow(void);
audio_output_t *getAout(void);

/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
struct intf_sys_t
{
    /* menus handlers */
    bool b_input_update;
    bool b_aout_update;
    bool b_vout_update;

    /* The messages window */
    msg_subscription_t sub;
};

/*****************************************************************************
 * VLCMain interface
 *****************************************************************************/
@class AppleRemote;
@class VLCInformation;
@class VLCEmbeddedWindow;
@class VLCControls;
@class VLCPlaylist;
@class VLCVoutWindowController;

@interface VLCMain : NSObject <NSWindowDelegate, NSApplicationDelegate>
{
    intf_thread_t *p_intf;      /* The main intf object */
    input_thread_t *p_current_input;
    id o_mainmenu;              /* VLCMainMenu */
    id o_prefs;                 /* VLCPrefs       */
    id o_sprefs;                /* VLCSimplePrefs */
    id o_open;                  /* VLCOpen        */
    id o_wizard;                /* VLCWizard      */
    id o_coredialogs;           /* VLCCoreDialogProvider */
    id o_info;                  /* VLCInformation */
    id o_eyetv;                 /* VLCEyeTVController */
    id o_bookmarks;             /* VLCBookmarks */
    id o_coreinteraction;       /* VLCCoreInteraction */
    BOOL nib_main_loaded;       /* main nibfile */
    BOOL nib_open_loaded;       /* open nibfile */
    BOOL nib_about_loaded;      /* about nibfile */
    BOOL nib_wizard_loaded;     /* wizard nibfile */
    BOOL nib_prefs_loaded;      /* preferences nibfile */
    BOOL nib_info_loaded;       /* information panel nibfile */
    BOOL nib_coredialogs_loaded; /* CoreDialogs nibfile */
    BOOL nib_bookmarks_loaded;   /* Bookmarks nibfile */
    BOOL b_active_videoplayback;
    BOOL b_nativeFullscreenMode;

    VLCMainWindow *o_mainwindow;            /* VLCMainWindow */

    IBOutlet VLCControls * o_controls;     /* VLCControls    */
    IBOutlet VLCPlaylist * o_playlist;     /* VLCPlaylist    */

    /* messages panel */
    IBOutlet NSWindow * o_msgs_panel;           /* messages panel */
    NSMutableArray * o_msg_arr;                 /* messages array */
    NSLock * o_msg_lock;                        /* messages lock */
    BOOL b_msg_arr_changed;                     /* did the array change? */
    IBOutlet NSButton * o_msgs_crashlog_btn;    /* messages open crashlog */
    IBOutlet NSButton * o_msgs_save_btn;        /* save the log as rtf */
    IBOutlet NSButton * o_msgs_refresh_btn;     /* update the panel */
    IBOutlet id o_msgs_table;

    /* CrashReporter panel */
    IBOutlet NSButton * o_crashrep_dontSend_btn;
    IBOutlet NSButton * o_crashrep_send_btn;
    IBOutlet NSTextView * o_crashrep_fld;
    IBOutlet NSTextField * o_crashrep_title_txt;
    IBOutlet NSTextField * o_crashrep_desc_txt;
    IBOutlet NSWindow * o_crashrep_win;
    IBOutlet NSButton * o_crashrep_includeEmail_ckb;
    IBOutlet NSButton * o_crashrep_dontaskagain_ckb;
    IBOutlet NSTextField * o_crashrep_includeEmail_txt;
    NSURLConnection * crashLogURLConnection;

    AppleRemote * o_remote;
    BOOL b_remote_button_hold; /* true as long as the user holds the left,right,plus or minus on the remote control */

    /* media key support */
    BOOL b_mediaKeySupport;
    BOOL b_mediakeyJustJumped;
    SPMediaKeyTap * o_mediaKeyController;

    NSArray *o_usedHotkeys;

    /* sleep management */
    IOPMAssertionID systemSleepAssertionID;

    VLCVoutWindowController *o_vout_controller;
}

@property (readonly) VLCVoutWindowController* voutController;

+ (VLCMain *)sharedInstance;

- (intf_thread_t *)intf;
- (void)setIntf:(intf_thread_t *)p_mainintf;

- (id)mainMenu;
- (VLCMainWindow *)mainWindow;
- (id)controls;
- (id)bookmarks;
- (id)open;
- (id)simplePreferences;
- (id)preferences;
- (id)playlist;
- (id)info;
- (id)wizard;
- (id)getVideoViewAtPositionX: (int *)pi_x Y: (int *)pi_y withWidth: (unsigned int*)pi_width andHeight: (unsigned int*)pi_height forWindow:(vout_window_t *)p_wnd;
- (id)coreDialogProvider;
- (id)eyeTVController;
- (id)appleRemoteController;
- (void)setActiveVideoPlayback:(BOOL)b_value;
- (BOOL)activeVideoPlayback;
- (void)applicationWillTerminate:(NSNotification *)notification;
- (void)updateCurrentlyUsedHotkeys;
- (BOOL)hasDefinedShortcutKey:(NSEvent *)o_event force:(BOOL)b_force;

- (void)setFullscreen:(int)i_full forWindow:(vout_window_t *)p_wnd;

- (void)PlaylistItemChanged;
- (void)playbackStatusUpdated;
- (void)sendDistributedNotificationWithUpdatedPlaybackStatus;
- (void)playbackModeUpdated;
- (void)updateVolume;
- (void)updatePlaybackPosition;
- (void)updateName;
- (void)playlistUpdated;
- (void)updateRecordState: (BOOL)b_value;
- (void)updateInfoandMetaPanel;
- (void)updateMainMenu;
- (void)updateMainWindow;
- (void)showMainWindow;
- (void)showFullscreenController;
- (void)updateDelays;
- (void)initStrings;

- (IBAction)crashReporterAction:(id)sender;
- (IBAction)openCrashLog:(id)sender;
- (IBAction)saveDebugLog:(id)sender;
- (IBAction)showMessagesPanel:(id)sender;
- (IBAction)updateMessagesPanel:(id)sender;

- (void)processReceivedlibvlcMessage:(const msg_item_t *) item ofType: (int)type withStr: (char *)str;

- (void)updateTogglePlaylistState;

- (void)windowDidBecomeKey:(NSNotification *)o_notification;

- (void)mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event;
@end

@interface VLCMain (Internal)
- (void)handlePortMessage:(NSPortMessage *)o_msg;
- (void)resetMediaKeyJump;
- (void)coreChangedMediaKeySupportSetting: (NSNotification *)o_notification;
@end

/*****************************************************************************
 * VLCApplication interface
 *****************************************************************************/

@interface VLCApplication : NSApplication

@end
