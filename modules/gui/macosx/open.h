/*****************************************************************************
 * open.h: MacOS X module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
 *          Felix Kühne <fkuehne at videolan dot org>
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

NSArray *GetEjectableMediaOfClass( const char *psz_class );

/*****************************************************************************
 * Intf_Open interface
 *****************************************************************************/
@interface VLCOpen : NSObject
{
    IBOutlet id o_playlist;

    IBOutlet id o_panel;

    IBOutlet id o_mrl;
    IBOutlet id o_mrl_lbl;
    IBOutlet id o_tabview;

    IBOutlet id o_btn_ok;
    IBOutlet id o_btn_cancel;

    IBOutlet id o_output_ckbox;
    IBOutlet id o_sout_options;

    /* open file */
    IBOutlet id o_file_path;
    IBOutlet id o_file_btn_browse;
    IBOutlet id o_file_stream;

    /* open disc */
    IBOutlet id o_disc_type;
    IBOutlet id o_disc_device;
    IBOutlet id o_disc_device_lbl;
    IBOutlet id o_disc_title;
    IBOutlet id o_disc_title_lbl;
    IBOutlet id o_disc_title_stp;
    IBOutlet id o_disc_chapter;
    IBOutlet id o_disc_chapter_lbl;
    IBOutlet id o_disc_chapter_stp;
    IBOutlet id o_disc_videots_folder;
    IBOutlet id o_disc_videots_btn_browse;
    IBOutlet id o_disc_dvd_menus;

    /* open network */
    IBOutlet id o_net_mode;
    IBOutlet id o_net_udp_port;
    IBOutlet id o_net_udp_port_lbl;
    IBOutlet id o_net_udp_port_stp;
    IBOutlet id o_net_udpm_addr;
    IBOutlet id o_net_udpm_addr_lbl;
    IBOutlet id o_net_udpm_port;
    IBOutlet id o_net_udpm_port_lbl;
    IBOutlet id o_net_udpm_port_stp;
    IBOutlet id o_net_http_url;
    IBOutlet id o_net_http_url_lbl;
    IBOutlet id o_net_timeshift_ckbox;

    /* open subtitle file */
    IBOutlet id o_file_sub_ckbox;
    IBOutlet id o_file_sub_btn_settings;
    IBOutlet id o_file_sub_sheet;
    IBOutlet id o_file_sub_path;
    IBOutlet id o_file_sub_btn_browse;
    IBOutlet id o_file_sub_override;
    IBOutlet id o_file_sub_delay;
    IBOutlet id o_file_sub_delay_lbl;
    IBOutlet id o_file_sub_delay_stp;
    IBOutlet id o_file_sub_fps;
    IBOutlet id o_file_sub_fps_lbl;
    IBOutlet id o_file_sub_fps_stp;
    IBOutlet id o_file_sub_encoding_pop;
    IBOutlet id o_file_sub_encoding_lbl;
    IBOutlet id o_file_sub_size_pop;
    IBOutlet id o_file_sub_size_lbl;
    IBOutlet id o_file_sub_align_pop;
    IBOutlet id o_file_sub_align_lbl;
    IBOutlet id o_file_sub_ok_btn;
    IBOutlet id o_file_sub_font_box;
    IBOutlet id o_file_sub_file_box;

    /* generic capturing stuff */
    IBOutlet id o_capture_lbl;
    IBOutlet id o_capture_long_lbl;
    IBOutlet id o_capture_mode_pop;
    IBOutlet id o_capture_label_view;

    /* eyetv support */
    IBOutlet id o_eyetv_notLaunched_view;
    IBOutlet id o_eyetv_running_view;
    IBOutlet id o_eyetv_channels_pop;
    IBOutlet id o_eyetv_currentChannel_lbl;
    IBOutlet id o_eyetv_chn_status_txt;
    IBOutlet id o_eyetv_chn_bgbar;
    IBOutlet id o_eyetv_launchEyeTV_btn;
    IBOutlet id o_eyetv_getPlugin_btn;
    IBOutlet id o_eyetv_nextProgram_btn;
    IBOutlet id o_eyetv_noInstance_lbl;
    IBOutlet id o_eyetv_noInstanceLong_lbl;
    IBOutlet id o_eyetv_previousProgram_btn;

    /* screen support */
    IBOutlet id o_screen_view;
    IBOutlet id o_screen_lbl;
    IBOutlet id o_screen_long_lbl;
    IBOutlet id o_screen_fps_fld;
    IBOutlet id o_screen_fps_lbl;
    IBOutlet id o_screen_fps_stp;
    IBOutlet id o_screen_left_fld;
    IBOutlet id o_screen_left_lbl;
    IBOutlet id o_screen_left_stp;
    IBOutlet id o_screen_top_fld;
    IBOutlet id o_screen_top_lbl;
    IBOutlet id o_screen_top_stp;
    IBOutlet id o_screen_width_fld;
    IBOutlet id o_screen_width_lbl;
    IBOutlet id o_screen_width_stp;
    IBOutlet id o_screen_height_fld;
    IBOutlet id o_screen_height_lbl;
    IBOutlet id o_screen_height_stp;
    IBOutlet id o_screen_follow_mouse_ckb;

    BOOL b_autoplay;
    id o_currentCaptureView;
    intf_thread_t * p_intf;
}

+ (VLCOpen *)sharedInstance;

- (void)setSubPanel;
- (void)openTarget:(int)i_type;
- (void)tabView:(NSTabView *)o_tv didSelectTabViewItem:(NSTabViewItem *)o_tvi;
- (void)textFieldWasClicked:(NSNotification *)o_notification;

- (void)openFileGeneric;
- (void)openFilePathChanged:(NSNotification *)o_notification;
- (IBAction)openFileBrowse:(id)sender;
- (void)pathChosenInPanel: (NSOpenPanel *)sheet withReturn:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction)openFileStreamChanged:(id)sender;

- (void)openDisc;
- (IBAction)openDiscTypeChanged:(id)sender;
- (IBAction)openDiscStepperChanged:(id)sender;
- (void)openDiscInfoChanged:(NSNotification *)o_notification;
- (IBAction)openDiscMenusChanged:(id)sender;
- (IBAction)openVTSBrowse:(id)sender;

- (void)openNet;
- (IBAction)openNetModeChanged:(id)sender;
- (IBAction)openNetStepperChanged:(id)sender;
- (void)openNetInfoChanged:(NSNotification *)o_notification;

- (void)openCapture;
- (void)showCaptureView: theView;
- (IBAction)openCaptureModeChanged:(id)sender;
- (IBAction)eyetvSwitchChannel:(id)sender;
- (IBAction)eyetvLaunch:(id)sender;
- (IBAction)eyetvGetPlugin:(id)sender;
- (void)eyetvChanged:(NSNotification *)o_notification;
- (void)setupChannelInfo;
- (IBAction)screenStepperChanged:(id)sender;
- (void)screenFPSfieldChanged:(NSNotification *)o_notification;

- (IBAction)subsChanged:(id)sender;
- (IBAction)subSettings:(id)sender;
- (IBAction)subFileBrowse:(id)sender;
- (IBAction)subOverride:(id)sender;
- (IBAction)subDelayStepperChanged:(id)sender;
- (IBAction)subFpsStepperChanged:(id)sender;
- (IBAction)subCloseSheet:(id)sender;

- (IBAction)panelCancel:(id)sender;
- (IBAction)panelOk:(id)sender;

- (void)openFile;
@end

@interface VLCOpenTextField : NSTextField
{
}
- (void)mouseDown:(NSEvent *)theEvent;
@end
