/*****************************************************************************
 * intf.h: MacOS X interface plugin
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: intf.h,v 1.2 2002/10/02 22:56:53 massiot Exp $
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
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

- (void)setIntf:(intf_thread_t *)p_intf;
- (intf_thread_t *)getIntf;

@end

#define _NS(s) [NSApp localizedString: _(s)]

/*****************************************************************************
 * intf_sys_t: description and status of the interface
 *****************************************************************************/
struct intf_sys_t
{
    NSAutoreleasePool * o_pool;
    NSPort * o_sendport;

    /* special actions */
    vlc_bool_t b_loop;
    vlc_bool_t b_playing;
    vlc_bool_t b_mute;

    /* menus handlers */
    vlc_bool_t b_chapter_update;
    vlc_bool_t b_program_update;
    vlc_bool_t b_title_update;
    vlc_bool_t b_audio_update;
    vlc_bool_t b_spu_update;

    /* The input thread */
    input_thread_t * p_input;

    /* The messages window */
    msg_subscription_t * p_sub;

    /* DVD mode */
    int i_part;
};

/*****************************************************************************
 * VLCMain interface 
 *****************************************************************************/
@interface VLCMain : NSObject
{
    IBOutlet id o_window;       /* main window    */

    IBOutlet id o_controls;     /* VLCControls    */
    IBOutlet id o_playlist;     /* VLCPlaylist    */

    IBOutlet id o_messages;     /* messages tv    */
    IBOutlet id o_msgs_panel;   /* messages panel */
    IBOutlet id o_msgs_btn_ok;  /* messages btn   */

    /* main menu */

    IBOutlet id o_mi_about;
    IBOutlet id o_mi_hide;
    IBOutlet id o_mi_hide_others;
    IBOutlet id o_mi_show_all;
    IBOutlet id o_mi_quit;

    IBOutlet id o_mu_file;
    IBOutlet id o_mi_open_file;
    IBOutlet id o_mi_open_disc;
    IBOutlet id o_mi_open_net;
    IBOutlet id o_mi_open_quickly;
    IBOutlet id o_mi_open_recent;
    IBOutlet id o_mi_open_recent_cm;

    IBOutlet id o_mu_edit;
    IBOutlet id o_mi_cut;
    IBOutlet id o_mi_copy;
    IBOutlet id o_mi_paste;
    IBOutlet id o_mi_clear;
    IBOutlet id o_mi_select_all;

    IBOutlet id o_mu_view;
    IBOutlet id o_mi_playlist;
    IBOutlet id o_mi_messages;

    IBOutlet id o_mu_controls;
    IBOutlet id o_mi_play;
    IBOutlet id o_mi_pause;
    IBOutlet id o_mi_stop;
    IBOutlet id o_mi_faster;
    IBOutlet id o_mi_slower;
    IBOutlet id o_mi_previous;
    IBOutlet id o_mi_next;
    IBOutlet id o_mi_loop;
    IBOutlet id o_mi_vol_up;
    IBOutlet id o_mi_vol_down;
    IBOutlet id o_mi_mute;
    IBOutlet id o_mi_fullscreen;
    IBOutlet id o_mi_deinterlace;
    IBOutlet id o_mi_program;
    IBOutlet id o_mi_title;
    IBOutlet id o_mi_chapter;
    IBOutlet id o_mi_language;
    IBOutlet id o_mi_subtitle;

    IBOutlet id o_mu_window;
    IBOutlet id o_mi_minimize;
    IBOutlet id o_mi_bring_atf;

    /* dock menu */
    IBOutlet id o_dmi_play;
    IBOutlet id o_dmi_pause;
    IBOutlet id o_dmi_stop;

    id asystm;			// MacOSXAudioSystem
}

- (void)terminate;

- (void)manage;
- (void)manageMode;

- (void)setupMenus;
- (void)setupLangMenu:(NSMenuItem *)o_mi
                      es:(es_descriptor_t *)p_es
                      category:(int)i_cat
                      selector:(SEL)pf_callback;

- (IBAction)clearRecentItems:(id)sender;
- (void)openRecentItem:(id)sender;

//- (void)selectAction:(id)sender;

@end

@interface VLCMain (Internal)
- (void)handlePortMessage:(NSPortMessage *)o_msg;
@end
