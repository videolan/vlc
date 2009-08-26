/*****************************************************************************
 * intf.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2009 the VideoLAN team
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

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>
#include <vlc_aout.h>
#include <vlc_input.h>

#include <Cocoa/Cocoa.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
unsigned int CocoaKeyToVLC( unichar i_key );

#define VLCIntf [[VLCMain sharedInstance] intf]

#define _NS(s) [[VLCMain sharedInstance] localizedString: _(s)]
/* Get an alternate version of the string.
 * This string is stored as '1:string' but when displayed it only displays
 * the translated string. the translation should be '1:translatedstring' though */
#define _ANS(s) [[[VLCMain sharedInstance] localizedString: _(s)] substringFromIndex:2]

#define MACOS_VERSION [[[NSDictionary dictionaryWithContentsOfFile: \
            @"/System/Library/CoreServices/SystemVersion.plist"] \
            objectForKey: @"ProductVersion"] floatValue]


// You need to release those objects after use
input_thread_t *getInput(void);
vout_thread_t *getVout(void);
aout_instance_t *getAout(void);

/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
struct intf_sys_t
{
    /* special actions */
    bool b_mute;
    int i_play_status;

    /* interface update */
    bool b_intf_update;
    bool b_playlist_update;
    bool b_playmode_update;
    bool b_current_title_update;
    bool b_fullscreen_update;
    bool b_volume_update;
    bool b_intf_show;

    /* menus handlers */
    bool b_input_update;
    bool b_aout_update;
    bool b_vout_update;

    /* The messages window */
    msg_subscription_t * p_sub;
};

/*****************************************************************************
 * VLCMain interface
 *****************************************************************************/
@class AppleRemote;
@class VLCInformation;
@class VLCControllerWindow;
@class VLCEmbeddedWindow;
@class VLCControls;
@class VLCPlaylist;
@interface VLCMain : NSObject
{
    intf_thread_t *p_intf;      /* The main intf object */
    id o_prefs;                 /* VLCPrefs       */
    id o_sprefs;                /* VLCSimplePrefs */
    id o_about;                 /* VLAboutBox     */
    id o_open;                  /* VLCOpen        */
    id o_wizard;                /* VLCWizard      */
    id o_extended;              /* VLCExtended    */
    id o_bookmarks;             /* VLCBookmarks   */
    id o_vlm;                   /* VLCVLMController */
    id o_embedded_list;         /* VLCEmbeddedList*/
    id o_coredialogs;           /* VLCCoreDialogProvider */
    VLCInformation * o_info;    /* VLCInformation */
    id o_eyetv;                 /* VLCEyeTVController */
    BOOL nib_main_loaded;       /* main nibfile */
    BOOL nib_open_loaded;       /* open nibfile */
    BOOL nib_about_loaded;      /* about nibfile */
    BOOL nib_wizard_loaded;     /* wizard nibfile */
    BOOL nib_extended_loaded;   /* extended nibfile */
    BOOL nib_bookmarks_loaded;  /* bookmarks nibfile */
    BOOL nib_prefs_loaded;      /* preferences nibfile */
    BOOL nib_info_loaded;       /* information panel nibfile */
    BOOL nib_vlm_loaded;        /* VLM Panel nibfile */
    BOOL nib_coredialogs_loaded; /* CoreDialogs nibfile */

    IBOutlet VLCControllerWindow * o_window;                     /* main window */
    IBOutlet NSView * o_playlist_view;                          /* playlist view  */
    IBOutlet id o_scrollfield;                                  /* info field */
    IBOutlet NSTextField * o_timefield;                         /* time field */
    IBOutlet NSSlider * o_timeslider;                           /* time slider */
    BOOL b_time_remaining;                                      /* show remaining time or playtime ? */
    IBOutlet VLCEmbeddedWindow * o_embedded_window;             /* Embedded Vout Window */
    float f_slider;                                             /* slider value */
    float f_slider_old;                                         /* old slider val */
    IBOutlet NSSlider * o_volumeslider;                         /* volume slider */

    IBOutlet NSView * toolbarMediaControl;   /* view with the controls */

    IBOutlet NSProgressIndicator * o_main_pgbar;   /* playlist window progress bar */
    IBOutlet NSButton * o_btn_prev;     /* btn previous   */
    IBOutlet NSButton * o_btn_rewind;   /* btn rewind     */
    IBOutlet NSButton * o_btn_play;     /* btn play       */
    IBOutlet NSButton * o_btn_stop;     /* btn stop       */
    IBOutlet NSButton * o_btn_ff;       /* btn fast forward     */
    IBOutlet NSButton * o_btn_next;     /* btn next       */
    IBOutlet NSButton * o_btn_fullscreen;/* btn fullscreen (embedded vout window) */
    IBOutlet NSButton * o_btn_playlist; /* btn playlist   */
    IBOutlet NSButton * o_btn_equalizer; /* eq btn */

    NSImage * o_img_play;       /* btn play img   */
    NSImage * o_img_pause;      /* btn pause img  */
    NSImage * o_img_play_pressed;       /* btn play img   */
    NSImage * o_img_pause_pressed;      /* btn pause img  */

    IBOutlet VLCControls * o_controls;     /* VLCControls    */
    IBOutlet VLCPlaylist * o_playlist;     /* VLCPlaylist    */

    IBOutlet NSTextView * o_messages;           /* messages tv    */
    IBOutlet NSWindow * o_msgs_panel;           /* messages panel */
    NSMutableArray * o_msg_arr;                 /* messages array */
    NSLock * o_msg_lock;                        /* messages lock */
    BOOL b_msg_arr_changed;                     /* did the array change? */
    IBOutlet NSButton * o_msgs_crashlog_btn;    /* messages open crashlog */
    IBOutlet NSButton * o_msgs_save_btn;        /* save the log as rtf */
    
    /* CrashReporter panel */
    IBOutlet NSButton * o_crashrep_dontSend_btn;
    IBOutlet NSButton * o_crashrep_send_btn;
    IBOutlet NSTextView * o_crashrep_fld;
    IBOutlet NSTextField * o_crashrep_title_txt;
    IBOutlet NSTextField * o_crashrep_desc_txt;
    IBOutlet NSWindow * o_crashrep_win;
    IBOutlet NSButton * o_crashrep_includeEmail_ckb;
    IBOutlet NSTextField * o_crashrep_includeEmail_txt;

    /* main menu */

    IBOutlet NSMenuItem * o_mi_about;
    IBOutlet NSMenuItem * o_mi_prefs;
    IBOutlet NSMenuItem * o_mi_sprefs;
    IBOutlet NSMenuItem * o_mi_checkForUpdate;
    IBOutlet NSMenuItem * o_mi_add_intf;
    IBOutlet NSMenu * o_mu_add_intf;
    IBOutlet NSMenuItem * o_mi_services;
    IBOutlet NSMenuItem * o_mi_hide;
    IBOutlet NSMenuItem * o_mi_hide_others;
    IBOutlet NSMenuItem * o_mi_show_all;
    IBOutlet NSMenuItem * o_mi_quit;

    IBOutlet NSMenu * o_mu_file;
    IBOutlet NSMenuItem * o_mi_open_file;
    IBOutlet NSMenuItem * o_mi_open_generic;
    IBOutlet NSMenuItem * o_mi_open_disc;
    IBOutlet NSMenuItem * o_mi_open_net;
    IBOutlet NSMenuItem * o_mi_open_capture;
    IBOutlet NSMenuItem * o_mi_open_recent;
    IBOutlet NSMenuItem * o_mi_open_recent_cm;
    IBOutlet NSMenuItem * o_mi_open_wizard;
    IBOutlet NSMenuItem * o_mi_open_vlm;

    IBOutlet NSMenu * o_mu_edit;
    IBOutlet NSMenuItem * o_mi_cut;
    IBOutlet NSMenuItem * o_mi_copy;
    IBOutlet NSMenuItem * o_mi_paste;
    IBOutlet NSMenuItem * o_mi_clear;
    IBOutlet NSMenuItem * o_mi_select_all;

    IBOutlet NSMenu * o_mu_controls;
    IBOutlet NSMenuItem * o_mi_play;
    IBOutlet NSMenuItem * o_mi_stop;
    IBOutlet NSMenuItem * o_mi_faster;
    IBOutlet NSMenuItem * o_mi_slower;
    IBOutlet NSMenuItem * o_mi_previous;
    IBOutlet NSMenuItem * o_mi_next;
    IBOutlet NSMenuItem * o_mi_random;
    IBOutlet NSMenuItem * o_mi_repeat;
    IBOutlet NSMenuItem * o_mi_loop;
    IBOutlet NSMenuItem * o_mi_quitAfterPB;
    IBOutlet NSMenuItem * o_mi_fwd;
    IBOutlet NSMenuItem * o_mi_bwd;
    IBOutlet NSMenuItem * o_mi_fwd1m;
    IBOutlet NSMenuItem * o_mi_bwd1m;
    IBOutlet NSMenuItem * o_mi_fwd5m;
    IBOutlet NSMenuItem * o_mi_bwd5m;
    IBOutlet NSMenuItem * o_mi_program;
    IBOutlet NSMenu * o_mu_program;
    IBOutlet NSMenuItem * o_mi_title;
    IBOutlet NSMenu * o_mu_title;
    IBOutlet NSMenuItem * o_mi_chapter;
    IBOutlet NSMenu * o_mu_chapter;

    IBOutlet NSMenu * o_mu_audio;
    IBOutlet NSMenuItem * o_mi_vol_up;
    IBOutlet NSMenuItem * o_mi_vol_down;
    IBOutlet NSMenuItem * o_mi_mute;
    IBOutlet NSMenuItem * o_mi_audiotrack;
    IBOutlet NSMenu * o_mu_audiotrack;
    IBOutlet NSMenuItem * o_mi_channels;
    IBOutlet NSMenu * o_mu_channels;
    IBOutlet NSMenuItem * o_mi_device;
    IBOutlet NSMenu * o_mu_device;
    IBOutlet NSMenuItem * o_mi_visual;
    IBOutlet NSMenu * o_mu_visual;

    IBOutlet NSMenu * o_mu_video;
    IBOutlet NSMenuItem * o_mi_half_window;
    IBOutlet NSMenuItem * o_mi_normal_window;
    IBOutlet NSMenuItem * o_mi_double_window;
    IBOutlet NSMenuItem * o_mi_fittoscreen;
    IBOutlet NSMenuItem * o_mi_fullscreen;
    IBOutlet NSMenuItem * o_mi_floatontop;
    IBOutlet NSMenuItem * o_mi_snapshot;
    IBOutlet NSMenuItem * o_mi_videotrack;
    IBOutlet NSMenu * o_mu_videotrack;
    IBOutlet NSMenuItem * o_mi_screen;
    IBOutlet NSMenu * o_mu_screen;
    IBOutlet NSMenuItem * o_mi_aspect_ratio;
    IBOutlet NSMenu * o_mu_aspect_ratio;
    IBOutlet NSMenuItem * o_mi_crop;
    IBOutlet NSMenu * o_mu_crop;
    IBOutlet NSMenuItem * o_mi_subtitle;
    IBOutlet NSMenu * o_mu_subtitle;
    IBOutlet NSMenuItem * o_mi_addSub;
    IBOutlet NSMenuItem * o_mi_deinterlace;
    IBOutlet NSMenu * o_mu_deinterlace;
    IBOutlet NSMenuItem * o_mi_ffmpeg_pp;
    IBOutlet NSMenu * o_mu_ffmpeg_pp;
    IBOutlet NSMenuItem * o_mi_teletext;
    IBOutlet NSMenuItem * o_mi_teletext_transparent;
    IBOutlet NSMenuItem * o_mi_teletext_index;
    IBOutlet NSMenuItem * o_mi_teletext_red;
    IBOutlet NSMenuItem * o_mi_teletext_green;
    IBOutlet NSMenuItem * o_mi_teletext_yellow;
    IBOutlet NSMenuItem * o_mi_teletext_blue;

    IBOutlet NSMenu * o_mu_window;
    IBOutlet NSMenuItem * o_mi_minimize;
    IBOutlet NSMenuItem * o_mi_close_window;
    IBOutlet NSMenuItem * o_mi_player;
    IBOutlet NSMenuItem * o_mi_controller;
    IBOutlet NSMenuItem * o_mi_equalizer;
    IBOutlet NSMenuItem * o_mi_extended;
    IBOutlet NSMenuItem * o_mi_bookmarks;
    IBOutlet NSMenuItem * o_mi_playlist;
    IBOutlet NSMenuItem * o_mi_info;
    IBOutlet NSMenuItem * o_mi_messages;
    IBOutlet NSMenuItem * o_mi_bring_atf;

    IBOutlet NSMenu * o_mu_help;
    IBOutlet NSMenuItem * o_mi_help;
    IBOutlet NSMenuItem * o_mi_readme;
    IBOutlet NSMenuItem * o_mi_documentation;
    IBOutlet NSMenuItem * o_mi_license;
    IBOutlet NSMenuItem * o_mi_website;
    IBOutlet NSMenuItem * o_mi_donation;
    IBOutlet NSMenuItem * o_mi_forum;
    IBOutlet NSMenuItem * o_mi_errorsAndWarnings;

    /* dock menu */
    IBOutlet NSMenuItem * o_dmi_play;
    IBOutlet NSMenuItem * o_dmi_stop;
    IBOutlet NSMenuItem * o_dmi_next;
    IBOutlet NSMenuItem * o_dmi_previous;
    IBOutlet NSMenuItem * o_dmi_mute;

    /* vout menu */
    IBOutlet NSMenu * o_vout_menu;
    IBOutlet NSMenuItem * o_vmi_play;
    IBOutlet NSMenuItem * o_vmi_stop;
    IBOutlet NSMenuItem * o_vmi_prev;
    IBOutlet NSMenuItem * o_vmi_next;
    IBOutlet NSMenuItem * o_vmi_volup;
    IBOutlet NSMenuItem * o_vmi_voldown;
    IBOutlet NSMenuItem * o_vmi_mute;
    IBOutlet NSMenuItem * o_vmi_fullscreen;
    IBOutlet NSMenuItem * o_vmi_snapshot;

    bool b_small_window;

    bool b_restore_size;
    NSRect o_restore_rect;

    mtime_t i_end_scroll;

    NSSize o_size_with_playlist;

    int     i_lastShownVolume;

    input_state_e cachedInputState;

    /* the manage thread */
    pthread_t manage_thread;

    /* The timer that update the interface */
    NSTimer * interfaceTimer;

    NSURLConnection * crashLogURLConnection;

    AppleRemote * o_remote;
    BOOL b_remote_button_hold; /* true as long as the user holds the left,right,plus or minus on the remote control */
}

+ (VLCMain *)sharedInstance;

- (intf_thread_t *)intf;
- (void)setIntf:(intf_thread_t *)p_mainintf;

- (void)controlTintChanged;

- (id)controls;
- (id)simplePreferences;
- (id)preferences;
- (id)playlist;
- (BOOL)isPlaylistCollapsed;
- (id)info;
- (id)wizard;
- (id)bookmarks;
- (id)embeddedList;
- (id)coreDialogProvider;
- (id)mainIntfPgbar;
- (id)controllerWindow;
- (id)voutMenu;
- (id)eyeTVController;
- (id)appleRemoteController;
- (void)applicationWillTerminate:(NSNotification *)notification;
- (NSString *)localizedString:(const char *)psz;
- (char *)delocalizeString:(NSString *)psz;
- (NSString *)wrapString: (NSString *)o_in_string toWidth: (int)i_width;
- (BOOL)hasDefinedShortcutKey:(NSEvent *)o_event;

- (void)initStrings;

- (void)manage;
- (void)manageIntf:(NSTimer *)o_timer;
- (void)setupMenus;
- (void)refreshVoutDeviceMenu:(NSNotification *)o_notification;
- (void)setScrollField:(NSString *)o_string stopAfter:(int )timeout;
- (void)resetScrollField;

- (void)updateMessageDisplay;
- (void)playStatusUpdated:(int) i_status;
- (void)setSubmenusEnabled:(BOOL)b_enabled;
- (void)manageVolumeSlider;
- (IBAction)timesliderUpdate:(id)sender;
- (IBAction)timeFieldWasClicked:(id)sender;

- (IBAction)clearRecentItems:(id)sender;
- (void)openRecentItem:(id)sender;

- (IBAction)intfOpenFile:(id)sender;
- (IBAction)intfOpenFileGeneric:(id)sender;
- (IBAction)intfOpenDisc:(id)sender;
- (IBAction)intfOpenNet:(id)sender;
- (IBAction)intfOpenCapture:(id)sender;

- (IBAction)showWizard:(id)sender;
- (IBAction)showVLM:(id)sender;
- (IBAction)showExtended:(id)sender;
- (IBAction)showBookmarks:(id)sender;

- (IBAction)viewAbout:(id)sender;
- (IBAction)showLicense:(id)sender;
- (IBAction)viewPreferences:(id)sender;
- (IBAction)viewHelp:(id)sender;
- (IBAction)openReadMe:(id)sender;
- (IBAction)openDocumentation:(id)sender;
- (IBAction)openWebsite:(id)sender;
- (IBAction)openForum:(id)sender;
- (IBAction)openDonate:(id)sender;
- (IBAction)openCrashLog:(id)sender;
- (IBAction)viewErrorsAndWarnings:(id)sender;
- (IBAction)showMessagesPanel:(id)sender;
- (IBAction)showInformationPanel:(id)sender;

- (IBAction)crashReporterAction:(id)sender;
- (IBAction)saveDebugLog:(id)sender;

- (IBAction)togglePlaylist:(id)sender;
- (void)updateTogglePlaylistState;

- (void)windowDidBecomeKey:(NSNotification *)o_notification;

@end

@interface VLCMain (Internal)
- (void)handlePortMessage:(NSPortMessage *)o_msg;
@end

/*****************************************************************************
 * VLCApplication interface
 *****************************************************************************/

@interface VLCApplication : NSApplication
{
    BOOL b_justJumped;
	BOOL b_mediaKeySupport;
    BOOL b_activeInBackground;
    BOOL b_active;
}

- (void)coreChangedMediaKeySupportSetting: (NSNotification *)o_notification;
- (void)sendEvent: (NSEvent*)event;
- (void)resetJump;

@end
