/*****************************************************************************
 * intf.h: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002-2003 VideoLAN
 * $Id: intf.h,v 1.31 2003/03/06 11:43:07 hartman Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
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

#include <Cocoa/Cocoa.h>

/*****************************************************************************
 * VLCApplication interface 
 *****************************************************************************/
@interface VLCApplication : NSApplication
{
    NSStringEncoding i_encoding;
    intf_thread_t *p_intf;
}

- (void)initIntlSupport;
- (NSString *)localizedString:(char *)psz;
- (char *)delocalizeString:(NSString *)psz;
- (NSStringEncoding)getEncoding;

- (void)setIntf:(intf_thread_t *)p_intf;
- (intf_thread_t *)getIntf;

@end

#define _NS(s) [NSApp localizedString: _(s)]
/* Get an alternate version of the string.
 * This string is stored as '1:string' but when displayed it only displays
 * the translated string. the translation should be '1:translatedstring' though */
#define _ANS(s) [[NSApp localizedString: _(s)] substringFromIndex:2]

int ExecuteOnMainThread( id target, SEL sel, void * p_arg );

/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
struct intf_sys_t
{
    NSAutoreleasePool * o_pool;
    NSPort * o_sendport;

    /* special actions */
    vlc_bool_t b_playing;
    vlc_bool_t b_mute;

    /* interface update */
    vlc_bool_t b_intf_update;
    vlc_bool_t b_play_status;
    vlc_bool_t b_playlist_update;
    vlc_bool_t b_current_title_update;
    
    /* menus handlers */
    vlc_bool_t b_chapter_update;
    vlc_bool_t b_program_update;
    vlc_bool_t b_title_update;
    vlc_bool_t b_audio_update;
    vlc_bool_t b_spu_update;
    vlc_bool_t b_aout_update;
    vlc_bool_t b_vout_update;

    /* The messages window */
    msg_subscription_t * p_sub;

    /* DVD mode */
    unsigned int i_part;
};

/*****************************************************************************
 * VLCMain interface 
 *****************************************************************************/
@interface VLCMain : NSObject
{
    id o_prefs;                 /* VLCPrefs       */

    IBOutlet id o_window;       /* main window    */
    IBOutlet id o_timefield;    /* time field     */
    IBOutlet id o_timeslider;   /* time slider    */
    float f_slider;             /* slider value   */
    float f_slider_old;         /* old slider val */ 
    IBOutlet id o_volumeslider; /* volume slider  */

    IBOutlet id o_btn_playlist; /* btn playlist   */
    IBOutlet id o_btn_prev;     /* btn previous   */
    IBOutlet id o_btn_slower;   /* btn slower     */
    IBOutlet id o_btn_play;     /* btn play       */
    IBOutlet id o_btn_stop;     /* btn stop       */
    IBOutlet id o_btn_faster;   /* btn faster     */
    IBOutlet id o_btn_next;     /* btn next       */
    IBOutlet id o_btn_prefs;    /* btn prefs      */

    NSImage * o_img_play;       /* btn play img   */
    NSImage * o_img_pause;      /* btn pause img  */

    IBOutlet id o_controls;     /* VLCControls    */
    IBOutlet id o_playlist;     /* VLCPlaylist    */
    IBOutlet id o_info;		/* VLCInfo        */

    IBOutlet id o_messages;     /* messages tv    */
    IBOutlet id o_msgs_panel;   /* messages panel */
    IBOutlet id o_msgs_btn_ok;  /* messages btn   */
    NSMutableArray * o_msg_arr; /* messages array */
    NSLock * o_msg_lock;        /* messages lock  */
    IBOutlet id o_msgs_btn_crashlog;	/* messages open crashlog */

    IBOutlet id o_error;        /* error panel    */
    IBOutlet id o_err_msg;      /* NSTextView     */
    IBOutlet id o_err_lbl;
    IBOutlet id o_err_bug_lbl;
    IBOutlet id o_err_btn_msgs; /* Open Messages  */
    IBOutlet id o_err_btn_dismiss;
    
    IBOutlet id o_info_window;	/* Info panel     */

    /* main menu */

    IBOutlet id o_mi_about;
    IBOutlet id o_mi_prefs;
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
    IBOutlet id o_mi_loop;
    IBOutlet id o_mi_fwd;
    IBOutlet id o_mi_bwd;
    IBOutlet id o_mi_program;
    IBOutlet id o_mi_title;
    IBOutlet id o_mi_chapter;
    IBOutlet id o_mi_language;
    IBOutlet id o_mi_subtitle;

    IBOutlet id o_mu_audio;
    IBOutlet id o_mi_vol_up;
    IBOutlet id o_mi_vol_down;
    IBOutlet id o_mi_mute;
    IBOutlet id o_mi_channels;
    IBOutlet id o_mi_device;

    IBOutlet id o_mu_video;
    IBOutlet id o_mi_half_window;
    IBOutlet id o_mi_normal_window;
    IBOutlet id o_mi_double_window;
    IBOutlet id o_mi_fullscreen;
    IBOutlet id o_mi_screen;
    IBOutlet id o_mi_deinterlace;

    IBOutlet id o_mu_window;
    IBOutlet id o_mi_minimize;
    IBOutlet id o_mi_close_window;
    IBOutlet id o_mi_controller;
    IBOutlet id o_mi_playlist;
    IBOutlet id o_mi_info;
    IBOutlet id o_mi_messages;
    IBOutlet id o_mi_bring_atf;
    
    IBOutlet id o_mu_help;
    IBOutlet id o_mi_readme;
    IBOutlet id o_mi_reportabug;
    IBOutlet id o_mi_website;
    IBOutlet id o_mi_license;

    /* dock menu */
    IBOutlet id o_dmi_play;
    IBOutlet id o_dmi_stop;
    IBOutlet id o_dmi_next;
    IBOutlet id o_dmi_previous;
}

- (id)getControls;

- (void)terminate;

- (void)manage;
- (void)manage:(playlist_t *)p_playlist;
- (void)manageMode:(playlist_t *)p_playlist;
- (void)manageIntf:(NSTimer *)o_timer;

- (void)updateMessageArray;
- (void)playStatusUpdated:(BOOL)b_pause;
- (void)setSubmenusEnabled:(BOOL)b_enabled;
- (void)manageVolumeSlider;

- (void)setupMenus:(input_thread_t *)p_input;
- (void)setupLangMenu:(input_thread_t *)p_input
                      mi:(NSMenuItem *)o_mi
                      es:(es_descriptor_t *)p_es
                      category:(int)i_cat
                      selector:(SEL)pf_callback;
- (void)setupVarMenu:(NSMenuItem *)o_mi
                     target:(vlc_object_t *)p_object
                     var:(const char *)psz_var
                     selector:(SEL)pf_callback;

- (IBAction)clearRecentItems:(id)sender;
- (void)openRecentItem:(id)sender;

- (IBAction)viewPreferences:(id)sender;

- (IBAction)timesliderUpdate:(id)sender;

- (IBAction)closeError:(id)sender;

- (IBAction)openReadMe:(id)sender;
- (IBAction)reportABug:(id)sender;
- (IBAction)openWebsite:(id)sender;
- (IBAction)openLicense:(id)sender;
- (IBAction)openCrashLog:(id)sender;

- (void)windowDidBecomeKey:(NSNotification *)o_notification;

@end

@interface VLCMain (Internal)
- (void)handlePortMessage:(NSPortMessage *)o_msg;
@end
