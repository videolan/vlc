/*****************************************************************************
 * intf.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2002-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>
#include <vlc/input.h>

#include <Cocoa/Cocoa.h>

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
int ExecuteOnMainThread( id target, SEL sel, void * p_arg );
unsigned int CocoaKeyToVLC( unichar i_key );

#define VLCIntf [[VLCMain sharedInstance] getIntf]

#define _NS(s) [[VLCMain sharedInstance] localizedString: _(s)]
/* Get an alternate version of the string.
 * This string is stored as '1:string' but when displayed it only displays
 * the translated string. the translation should be '1:translatedstring' though */
#define _ANS(s) [[[VLCMain sharedInstance] localizedString: _(s)] substringFromIndex:2]

#define MACOS_VERSION [[[NSDictionary dictionaryWithContentsOfFile: \
            @"/System/Library/CoreServices/SystemVersion.plist"] \
            objectForKey: @"ProductVersion"] floatValue]

/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
struct intf_sys_t
{
    NSAutoreleasePool * o_pool;
    NSPort * o_sendport;

    /* the current input */
    input_thread_t * p_input;

    /* special actions */
    vlc_bool_t b_mute;
    int i_play_status;

    /* interface update */
    vlc_bool_t b_intf_update;
    vlc_bool_t b_playlist_update;
    vlc_bool_t b_playmode_update;
    vlc_bool_t b_current_title_update;
    vlc_bool_t b_fullscreen_update;
    vlc_bool_t b_volume_update;

    /* menus handlers */
    vlc_bool_t b_input_update;
    vlc_bool_t b_aout_update;
    vlc_bool_t b_vout_update;

    /* The messages window */
    msg_subscription_t * p_sub;

};

/*****************************************************************************
 * VLCMain interface
 *****************************************************************************/
@interface VLCMain : NSObject
{
    intf_thread_t *p_intf;      /* The main intf object */
    id o_prefs;                 /* VLCPrefs       */
    id o_about;                 /* VLAboutBox     */
    id o_open;                  /* VLCOpen        */
    id o_wizard;                /* VLCWizard      */
    id o_extended;              /* VLCExtended    */
    id o_bookmarks;             /* VLCBookmarks   */
    id o_embedded_list;         /* VLCEmbeddedList*/
    id o_interaction_list;      /* VLCInteractionList*/
    id o_sfilters;              /* VLCsFilters    */
    /*id o_update;                 VLCUpdate      */
    BOOL nib_main_loaded;       /* reference to the main-nib */
    BOOL nib_open_loaded;       /* reference to the open-nib */
    BOOL nib_about_loaded;      /* reference to the about-nib */
    BOOL nib_wizard_loaded;     /* reference to the wizard-nib */
    BOOL nib_extended_loaded;   /* reference to the extended-nib */
    BOOL nib_bookmarks_loaded;  /* reference to the bookmarks-nib */
    BOOL nib_sfilters_loaded;   /* reference to the sfilters-nib */
    /*BOOL nib_update_loaded;      reference to the update-nib */

    IBOutlet id o_window;       /* main window    */
    IBOutlet id o_playlist_view;/* playlist view  */
    IBOutlet id o_scrollfield;  /* info field     */
    IBOutlet id o_timefield;    /* time field     */
    IBOutlet id o_timeslider;   /* time slider    */
    float f_slider;             /* slider value   */
    float f_slider_old;         /* old slider val */
    IBOutlet id o_volumeslider; /* volume slider  */

    IBOutlet id o_btn_prev;     /* btn previous   */
    IBOutlet id o_btn_rewind;   /* btn rewind     */
    IBOutlet id o_btn_play;     /* btn play       */
    IBOutlet id o_btn_stop;     /* btn stop       */
    IBOutlet id o_btn_ff;       /* btn fast forward     */
    IBOutlet id o_btn_next;     /* btn next       */
    IBOutlet id o_btn_fullscreen;/* btn fullscreen      */
    IBOutlet id o_btn_playlist; /* btn playlist   */

    NSImage * o_img_play;       /* btn play img   */
    NSImage * o_img_pause;      /* btn pause img  */
    NSImage * o_img_play_pressed;       /* btn play img   */
    NSImage * o_img_pause_pressed;      /* btn pause img  */

    IBOutlet id o_controls;     /* VLCControls    */
    IBOutlet id o_playlist;     /* VLCPlaylist    */
    IBOutlet id o_info;         /* VLCInfo        */

    IBOutlet id o_messages;     /* messages tv    */
    IBOutlet id o_msgs_panel;   /* messages panel */
    NSMutableArray * o_msg_arr; /* messages array */
    NSLock * o_msg_lock;        /* messages lock  */
    IBOutlet id o_msgs_btn_crashlog;    /* messages open crashlog */

    IBOutlet id o_error;        /* error panel    */
    IBOutlet id o_err_msg;      /* NSTextView     */
    IBOutlet id o_err_lbl;
    IBOutlet id o_err_bug_lbl;
    IBOutlet id o_err_btn_msgs; /* Open Messages  */
    IBOutlet id o_err_btn_dismiss;
    IBOutlet id o_err_ckbk_surpress;

    IBOutlet id o_info_window;  /* Info panel     */

    /* main menu */

    IBOutlet id o_mi_about;
    IBOutlet id o_mi_prefs;
    /*IBOutlet id o_mi_checkForUpdate;*/
    IBOutlet id o_mi_add_intf;
    IBOutlet id o_mu_add_intf;
    IBOutlet id o_mi_services;
    IBOutlet id o_mi_hide;
    IBOutlet id o_mi_hide_others;
    IBOutlet id o_mi_show_all;
    IBOutlet id o_mi_quit;

    IBOutlet id o_mu_file;
    IBOutlet id o_mi_open_file;
    IBOutlet id o_mi_open_generic;
    IBOutlet id o_mi_open_disc;
    IBOutlet id o_mi_open_net;
    IBOutlet id o_mi_open_recent;
    IBOutlet id o_mi_open_recent_cm;
    IBOutlet id o_mi_open_wizard;

    IBOutlet id o_mu_edit;
    IBOutlet id o_mi_cut;
    IBOutlet id o_mi_copy;
    IBOutlet id o_mi_paste;
    IBOutlet id o_mi_clear;
    IBOutlet id o_mi_select_all;

    IBOutlet id o_mu_controls;
    IBOutlet id o_mi_play;
    IBOutlet id o_mi_stop;
    IBOutlet id o_mi_faster;
    IBOutlet id o_mi_slower;
    IBOutlet id o_mi_previous;
    IBOutlet id o_mi_next;
    IBOutlet id o_mi_random;
    IBOutlet id o_mi_repeat;
    IBOutlet id o_mi_loop;
    IBOutlet id o_mi_fwd;
    IBOutlet id o_mi_bwd;
    IBOutlet id o_mi_fwd1m;
    IBOutlet id o_mi_bwd1m;
    IBOutlet id o_mi_fwd5m;
    IBOutlet id o_mi_bwd5m;
    IBOutlet id o_mi_program;
    IBOutlet id o_mu_program;
    IBOutlet id o_mi_title;
    IBOutlet id o_mu_title;
    IBOutlet id o_mi_chapter;
    IBOutlet id o_mu_chapter;

    IBOutlet id o_mu_audio;
    IBOutlet id o_mi_vol_up;
    IBOutlet id o_mi_vol_down;
    IBOutlet id o_mi_mute;
    IBOutlet id o_mi_audiotrack;
    IBOutlet id o_mu_audiotrack;
    IBOutlet id o_mi_channels;
    IBOutlet id o_mu_channels;
    IBOutlet id o_mi_device;
    IBOutlet id o_mu_device;
    IBOutlet id o_mi_visual;
    IBOutlet id o_mu_visual;

    IBOutlet id o_mu_video;
    IBOutlet id o_mi_half_window;
    IBOutlet id o_mi_normal_window;
    IBOutlet id o_mi_double_window;
    IBOutlet id o_mi_fittoscreen;
    IBOutlet id o_mi_fullscreen;
    IBOutlet id o_mi_floatontop;
    IBOutlet id o_mi_snapshot;
    IBOutlet id o_mi_videotrack;
    IBOutlet id o_mu_videotrack;
    IBOutlet id o_mi_screen;
    IBOutlet id o_mu_screen;
    IBOutlet id o_mi_subtitle;
    IBOutlet id o_mu_subtitle;
    IBOutlet id o_mi_deinterlace;
    IBOutlet id o_mu_deinterlace;
    IBOutlet id o_mi_ffmpeg_pp;
    IBOutlet id o_mu_ffmpeg_pp;

    IBOutlet id o_mu_window;
    IBOutlet id o_mi_minimize;
    IBOutlet id o_mi_close_window;
    IBOutlet id o_mi_controller;
    IBOutlet id o_mi_equalizer;
    IBOutlet id o_mi_extended;
    IBOutlet id o_mi_bookmarks;
    IBOutlet id o_mi_playlist;
    IBOutlet id o_mi_info;
    IBOutlet id o_mi_messages;
    IBOutlet id o_mi_bring_atf;

    IBOutlet id o_mu_help;
    IBOutlet id o_mi_readme;
    IBOutlet id o_mi_documentation;
    IBOutlet id o_mi_reportabug;
    IBOutlet id o_mi_website;
    IBOutlet id o_mi_license;
    IBOutlet id o_mi_donation;
    IBOutlet id o_mi_forum;

    /* dock menu */
    IBOutlet id o_dmi_play;
    IBOutlet id o_dmi_stop;
    IBOutlet id o_dmi_next;
    IBOutlet id o_dmi_previous;
    IBOutlet id o_dmi_mute;

    bool b_small_window;

    mtime_t i_end_scroll;

    NSSize o_size_with_playlist;

    int     i_lastShownVolume;
}

+ (VLCMain *)sharedInstance;

- (intf_thread_t *)getIntf;
- (void)setIntf:(intf_thread_t *)p_mainintf;

- (id)getControls;
- (id)getPlaylist;
- (id)getInfo;
- (id)getWizard;
- (id)getBookmarks;
- (id)getEmbeddedList;
- (id)getInteractionList;
- (void)terminate;
- (NSString *)localizedString:(char *)psz;
- (char *)delocalizeString:(NSString *)psz;
- (NSString *)wrapString: (NSString *)o_in_string toWidth: (int)i_width;
- (BOOL)hasDefinedShortcutKey:(NSEvent *)o_event;

- (void)initStrings;

- (void)manage;
- (void)manageIntf:(NSTimer *)o_timer;
- (void)setupMenus;
- (void)setScrollField:(NSString *)o_string stopAfter:(int )timeout;
- (void)resetScrollField;

- (void)updateMessageArray;
- (void)playStatusUpdated:(int) i_status;
- (void)setSubmenusEnabled:(BOOL)b_enabled;
- (void)manageVolumeSlider;
- (IBAction)timesliderUpdate:(id)sender;

- (IBAction)clearRecentItems:(id)sender;
- (void)openRecentItem:(id)sender;

- (IBAction)intfOpenFile:(id)sender;
- (IBAction)intfOpenFileGeneric:(id)sender;
- (IBAction)intfOpenDisc:(id)sender;
- (IBAction)intfOpenNet:(id)sender;

- (IBAction)showWizard:(id)sender;
- (IBAction)showExtended:(id)sender;
- (IBAction)showBookmarks:(id)sender;
- (IBAction)showSFilters:(id)sender;

- (IBAction)viewAbout:(id)sender;
- (IBAction)viewPreferences:(id)sender;
/*- (IBAction)checkForUpdate:(id)sender;*/
- (IBAction)closeError:(id)sender;
- (IBAction)openReadMe:(id)sender;
- (IBAction)openDocumentation:(id)sender;
- (IBAction)reportABug:(id)sender;
- (IBAction)openWebsite:(id)sender;
- (IBAction)openLicense:(id)sender;
- (IBAction)openForum:(id)sender;
- (IBAction)openDonate:(id)sender;
- (IBAction)openCrashLog:(id)sender;

- (IBAction)togglePlaylist:(id)sender;
- (void)updateTogglePlaylistState;

- (void)windowDidBecomeKey:(NSNotification *)o_notification;

@end

@interface VLCMain (Internal)
- (void)handlePortMessage:(NSPortMessage *)o_msg;
@end
