/*****************************************************************************
 * open.h: Open dialogues for VLC's MacOS X port
 *****************************************************************************
 * Copyright (C) 2002-2012 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Christophe Massiot <massiot@via.ecp.fr>
 *          Derk-Jan Hartman <thedj@users.sourceforge.net>
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

#define kVLCMediaAudioCD @"AudioCD"
#define kVLCMediaDVD @"DVD"
#define kVLCMediaVCD @"VCD"
#define kVLCMediaSVCD @"SVCD"
#define kVLCMediaBD @"Blu-ray"
#define kVLCMediaVideoTSFolder @"VIDEO_TS"
#define kVLCMediaBDMVFolder @"BDMV"
#define kVLCMediaUnknown @"Unknown"

/*****************************************************************************
 * Intf_Open interface
 *****************************************************************************/
@interface VLCOpen : NSObject
{
    IBOutlet id o_panel;

    IBOutlet id o_mrl_fld;
    IBOutlet id o_mrl_lbl;
    IBOutlet id o_mrl_view;
    IBOutlet id o_mrl_btn;
    IBOutlet id o_tabview;

    IBOutlet id o_btn_ok;
    IBOutlet id o_btn_cancel;

    /* bottom-line items */
    IBOutlet id o_output_ckbox;
    IBOutlet id o_sout_options;

    /* open file */
    IBOutlet id o_file_name;
    IBOutlet id o_file_name_stub;
    IBOutlet id o_file_icon_well;
    IBOutlet id o_file_btn_browse;
    IBOutlet id o_file_stream;
    IBOutlet id o_file_slave_ckbox;
    IBOutlet id o_file_slave_select_btn;
    IBOutlet id o_file_slave_filename_lbl;
    IBOutlet id o_file_slave_icon_well;
    IBOutlet id o_file_subtitles_filename_lbl;
    IBOutlet id o_file_subtitles_icon_well;
    IBOutlet id o_file_custom_timing_ckb;
    IBOutlet id o_file_starttime_fld;
    IBOutlet id o_file_starttime_lbl;
    IBOutlet id o_file_stoptime_fld;
    IBOutlet id o_file_stoptime_lbl;

    /* open disc */
    IBOutlet id o_disc_selector_pop;

    IBOutlet id o_disc_nodisc_view;
    IBOutlet id o_disc_nodisc_lbl;
    IBOutlet id o_disc_nodisc_videots_btn;
    IBOutlet id o_disc_nodisc_bdmv_btn;

    IBOutlet id o_disc_audiocd_view;
    IBOutlet id o_disc_audiocd_lbl;
    IBOutlet id o_disc_audiocd_trackcount_lbl;
    IBOutlet id o_disc_audiocd_videots_btn;
    IBOutlet id o_disc_audiocd_bdmv_btn;

    IBOutlet id o_disc_dvd_view;
    IBOutlet id o_disc_dvd_lbl;
    IBOutlet id o_disc_dvd_disablemenus_btn;
    IBOutlet id o_disc_dvd_videots_btn;
    IBOutlet id o_disc_dvd_bdmv_btn;

    IBOutlet id o_disc_dvdwomenus_view;
    IBOutlet id o_disc_dvdwomenus_lbl;
    IBOutlet id o_disc_dvdwomenus_enablemenus_btn;
    IBOutlet id o_disc_dvdwomenus_videots_btn;
    IBOutlet id o_disc_dvdwomenus_bdmv_btn;
    IBOutlet id o_disc_dvdwomenus_title;
    IBOutlet id o_disc_dvdwomenus_title_lbl;
    IBOutlet id o_disc_dvdwomenus_title_stp;
    IBOutlet id o_disc_dvdwomenus_chapter;
    IBOutlet id o_disc_dvdwomenus_chapter_lbl;
    IBOutlet id o_disc_dvdwomenus_chapter_stp;

    IBOutlet id o_disc_vcd_view;
    IBOutlet id o_disc_vcd_lbl;
    IBOutlet id o_disc_vcd_videots_btn;
    IBOutlet id o_disc_vcd_bdmv_btn;
    IBOutlet id o_disc_vcd_title;
    IBOutlet id o_disc_vcd_title_lbl;
    IBOutlet id o_disc_vcd_title_stp;
    IBOutlet id o_disc_vcd_chapter;
    IBOutlet id o_disc_vcd_chapter_lbl;
    IBOutlet id o_disc_vcd_chapter_stp;

    IBOutlet id o_disc_bd_view;
    IBOutlet id o_disc_bd_lbl;
    IBOutlet id o_disc_bd_videots_btn;
    IBOutlet id o_disc_bd_bdmv_btn;

    /* open network */
    IBOutlet id o_net_http_url;
    IBOutlet id o_net_http_url_lbl;
    IBOutlet id o_net_help_lbl;

    /* open UDP stuff panel */
    IBOutlet id o_net_help_udp_lbl;
    IBOutlet id o_net_udp_protocol_mat;
    IBOutlet id o_net_udp_protocol_lbl;
    IBOutlet id o_net_udp_address_lbl;
    IBOutlet id o_net_udp_mode_lbl;
    IBOutlet id o_net_mode;
    IBOutlet id o_net_openUDP_btn;
    IBOutlet id o_net_udp_cancel_btn;
    IBOutlet id o_net_udp_ok_btn;
    IBOutlet id o_net_udp_panel;
    IBOutlet id o_net_udp_port;
    IBOutlet id o_net_udp_port_lbl;
    IBOutlet id o_net_udp_port_stp;
    IBOutlet id o_net_udpm_addr;
    IBOutlet id o_net_udpm_addr_lbl;
    IBOutlet id o_net_udpm_port;
    IBOutlet id o_net_udpm_port_lbl;
    IBOutlet id o_net_udpm_port_stp;

    /* open subtitle file */
    IBOutlet id o_file_sub_ckbox;
    IBOutlet id o_file_sub_btn_settings;
    IBOutlet id o_file_sub_sheet;
    IBOutlet id o_file_sub_path_lbl;
    IBOutlet id o_file_sub_path_fld;
    IBOutlet id o_file_sub_icon_view;
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
    IBOutlet id o_screen_long_lbl;
    IBOutlet id o_screen_fps_fld;
    IBOutlet id o_screen_fps_lbl;
    IBOutlet id o_screen_fps_stp;
    IBOutlet id o_screen_screen_lbl;
    IBOutlet id o_screen_screen_pop;
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
    IBOutlet id o_screen_qtk_audio_pop;
    IBOutlet id o_screen_qtk_audio_ckb;

    /* QTK support */
    IBOutlet id o_qtk_view;
    IBOutlet id o_qtk_video_device_pop;
    IBOutlet id o_qtk_video_ckb;
    IBOutlet id o_qtk_audio_device_pop;
    IBOutlet id o_qtk_audio_ckb;
    IBOutlet id o_capture_width_lbl;
    IBOutlet id o_capture_width_fld;
    IBOutlet id o_capture_width_stp;
    IBOutlet id o_capture_height_lbl;
    IBOutlet id o_capture_height_fld;
    IBOutlet id o_capture_height_stp;

    NSArray         *qtkvideoDevices;
    NSArray         *qtkaudioDevices;
    NSString        *qtk_currdevice_uid;
    NSString        *qtkaudio_currdevice_uid;

    BOOL b_autoplay;
    BOOL b_nodvdmenus;
    id o_currentOpticalMediaView;
    id o_currentOpticalMediaIconView;
    NSMutableArray *o_allMediaDevices;
    NSArray *o_opticalDevices;
    NSMutableArray *o_specialMediaFolders;
    NSString *o_file_path;
    id o_currentCaptureView;
    NSString *o_file_slave_path;
    NSString *o_sub_path;
    NSString *o_mrl;
    NSMutableArray *o_displayInfos;
}

+ (VLCOpen *)sharedInstance;

@property (readwrite, assign) NSString *MRL;
@property (readonly) NSArray *qtkvideoDevices;
@property (readonly) NSArray *qtkaudioDevices;

/* text field / stepper binding values - subs panel */
@property (nonatomic) float fileSubDelay;
@property (nonatomic) float fileSubFps;


- (void)qtkrefreshVideoDevices;
- (void)qtkrefreshAudioDevices;

- (void)setSubPanel;
- (void)openTarget:(int)i_type;
- (void)tabView:(NSTabView *)o_tv didSelectTabViewItem:(NSTabViewItem *)o_tvi;
- (void)textFieldWasClicked:(NSNotification *)o_notification;
- (IBAction)expandMRLfieldAction:(id)sender;
- (IBAction)inputSlaveAction:(id)sender;
- (IBAction)fileTimeCustomization:(id)sender;

- (void)openFileGeneric;
- (void)openFilePathChanged:(NSNotification *)o_notification;
- (IBAction)openFileBrowse:(id)sender;
- (IBAction)openFileStreamChanged:(id)sender;

- (void)openDisc;
- (void)scanOpticalMedia:(NSNotification *)o_notification;
- (IBAction)discSelectorChanged:(id)sender;
- (IBAction)openSpecialMediaFolder:(id)sender;
- (IBAction)dvdreadOptionChanged:(id)sender;
- (IBAction)vcdOptionChanged:(id)sender;
- (NSString *)getVolumeTypeFromMountPath:(NSString *)mountPath;
- (NSString *)getBSDNodeFromMountPath:(NSString *)mountPath;

- (void)openNet;
- (IBAction)openNetModeChanged:(id)sender;
- (IBAction)openNetStepperChanged:(id)sender;
- (void)openNetInfoChanged:(NSNotification *)o_notification;
- (IBAction)openNetUDPButtonAction:(id)sender;

- (void)openCapture;
- (void)showCaptureView: theView;
- (IBAction)openCaptureModeChanged:(id)sender;
- (IBAction)qtkChanged:(id)sender;
- (IBAction)qtkAudioChanged:(id)sender;
- (IBAction)qtkToggleUIElements:(id)sender;
- (IBAction)screenChanged:(id)sender;
- (IBAction)eyetvSwitchChannel:(id)sender;
- (IBAction)eyetvLaunch:(id)sender;
- (IBAction)eyetvGetPlugin:(id)sender;
- (void)eyetvChanged:(NSNotification *)o_notification;
- (void)setupChannelInfo;
- (void)screenFPSfieldChanged:(NSNotification *)o_notification;

- (IBAction)subsChanged:(id)sender;
- (IBAction)subSettings:(id)sender;
- (IBAction)subFileBrowse:(id)sender;
- (IBAction)subOverride:(id)sender;

- (IBAction)subCloseSheet:(id)sender;

- (IBAction)panelCancel:(id)sender;
- (IBAction)panelOk:(id)sender;

- (void)openFile;
@end

@interface VLCOpenTextField : NSTextField
- (void)mouseDown:(NSEvent *)theEvent;
@end
