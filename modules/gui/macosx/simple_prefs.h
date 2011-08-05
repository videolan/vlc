/*****************************************************************************
* simple_prefs.h: Simple Preferences for Mac OS X
*****************************************************************************
* Copyright (C) 2008-2011 the VideoLAN team
* $Id$
*
* Authors: Felix Paul Kühne <fkuehne at videolan dot org>
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

#import <Cocoa/Cocoa.h>
#import "intf.h"
#import <vlc_common.h>

#ifndef MAC_OS_X_VERSION_10_6
@protocol NSToolbarDelegate <NSObject> @end
#endif

@interface VLCSimplePrefs : NSObject <NSToolbarDelegate>
{
    IBOutlet id o_audio_dolby_pop;
    IBOutlet id o_audio_dolby_txt;
    IBOutlet id o_audio_effects_box;
    IBOutlet id o_audio_enable_ckb;
    IBOutlet id o_audio_general_box;
    IBOutlet id o_audio_lang_fld;
    IBOutlet id o_audio_lang_txt;
    IBOutlet id o_audio_last_box;
    IBOutlet id o_audio_last_ckb;
    IBOutlet id o_audio_lastpwd_sfld;
    IBOutlet id o_audio_lastpwd_txt;
    IBOutlet id o_audio_lastuser_fld;
    IBOutlet id o_audio_lastuser_txt;
    IBOutlet id o_audio_spdif_ckb;
    IBOutlet id o_audio_view;
    IBOutlet id o_audio_visual_pop;
    IBOutlet id o_audio_visual_txt;
    IBOutlet id o_audio_vol_fld;
    IBOutlet id o_audio_vol_sld;
    IBOutlet id o_audio_vol_txt;

    IBOutlet id o_hotkeys_change_btn;
    IBOutlet id o_hotkeys_change_lbl;
    IBOutlet id o_hotkeys_change_keys_lbl;
    IBOutlet id o_hotkeys_change_taken_lbl;
    IBOutlet id o_hotkeys_change_win;
    IBOutlet id o_hotkeys_change_cancel_btn;
    IBOutlet id o_hotkeys_change_ok_btn;
    IBOutlet id o_hotkeys_clear_btn;
    IBOutlet id o_hotkeys_lbl;
    IBOutlet id o_hotkeys_listbox;
    IBOutlet id o_hotkeys_view;

    IBOutlet id o_input_avi_pop;
    IBOutlet id o_input_avi_txt;
    IBOutlet id o_input_cachelevel_pop;
    IBOutlet id o_input_cachelevel_txt;
    IBOutlet id o_input_cachelevel_custom_txt;
    IBOutlet id o_input_caching_box;
    IBOutlet id o_input_httpproxy_fld;
    IBOutlet id o_input_httpproxy_txt;
    IBOutlet id o_input_httpproxypwd_sfld;
    IBOutlet id o_input_httpproxypwd_txt;
    IBOutlet id o_input_mux_box;
    IBOutlet id o_input_net_box;
    IBOutlet id o_input_postproc_fld;
    IBOutlet id o_input_postproc_txt;
    IBOutlet id o_input_rtsp_ckb;
    IBOutlet id o_input_skipLoop_txt;
    IBOutlet id o_input_skipLoop_pop;
    IBOutlet id o_input_view;

    IBOutlet id o_intf_style_txt;
    IBOutlet id o_intf_style_dark_bcell;
    IBOutlet id o_intf_style_bright_bcell;
    IBOutlet id o_intf_art_pop;
    IBOutlet id o_intf_art_txt;
    IBOutlet id o_intf_embedded_ckb;
    IBOutlet id o_intf_fspanel_ckb;
	IBOutlet id o_intf_appleremote_ckb;
	IBOutlet id o_intf_mediakeys_ckb;
    IBOutlet id o_intf_lang_pop;
    IBOutlet id o_intf_lang_txt;
    IBOutlet id o_intf_network_box;
    IBOutlet id o_intf_view;
    IBOutlet id o_intf_update_ckb;
    IBOutlet id o_intf_last_update_lbl;
    IBOutlet id o_intf_enableGrowl_ckb;

    IBOutlet id o_osd_encoding_pop;
    IBOutlet id o_osd_encoding_txt;
    IBOutlet id o_osd_font_box;
    IBOutlet id o_osd_font_btn;
    IBOutlet id o_osd_font_color_pop;
    IBOutlet id o_osd_font_color_txt;
    IBOutlet id o_osd_font_fld;
    IBOutlet id o_osd_font_size_pop;
    IBOutlet id o_osd_font_size_txt;
    IBOutlet id o_osd_font_txt;
    IBOutlet id o_osd_lang_box;
    IBOutlet id o_osd_lang_fld;
    IBOutlet id o_osd_lang_txt;
    IBOutlet id o_osd_opacity_txt;
    IBOutlet id o_osd_opacity_fld;
    IBOutlet id o_osd_opacity_sld;
    IBOutlet id o_osd_forcebold_ckb;
    IBOutlet id o_osd_moreoptions_txt;
    IBOutlet id o_osd_osd_box;
    IBOutlet id o_osd_osd_ckb;
    IBOutlet id o_osd_view;

    IBOutlet id o_sprefs_basic_box;
    IBOutlet id o_sprefs_basicFull_matrix;
    IBOutlet id o_sprefs_cancel_btn;
    IBOutlet id o_sprefs_controls_box;
    IBOutlet id o_sprefs_reset_btn;
    IBOutlet id o_sprefs_save_btn;
    IBOutlet id o_sprefs_win;

    IBOutlet id o_video_black_ckb;
    IBOutlet id o_video_device_pop;
    IBOutlet id o_video_device_txt;
    IBOutlet id o_video_display_box;
    IBOutlet id o_video_enable_ckb;
    IBOutlet id o_video_fullscreen_ckb;
    IBOutlet id o_video_onTop_ckb;
    IBOutlet id o_video_output_pop;
    IBOutlet id o_video_output_txt;
    IBOutlet id o_video_skipFrames_ckb;
    IBOutlet id o_video_snap_box;
    IBOutlet id o_video_snap_folder_btn;
    IBOutlet id o_video_snap_folder_fld;
    IBOutlet id o_video_snap_folder_txt;
    IBOutlet id o_video_snap_format_pop;
    IBOutlet id o_video_snap_format_txt;
    IBOutlet id o_video_snap_prefix_fld;
    IBOutlet id o_video_snap_prefix_txt;
    IBOutlet id o_video_snap_seqnum_ckb;
    IBOutlet id o_video_view;

    BOOL b_audioSettingChanged;
    BOOL b_intfSettingChanged;
    BOOL b_videoSettingChanged;
    BOOL b_osdSettingChanged;
    BOOL b_inputSettingChanged;
    BOOL b_hotkeyChanged;
    id o_currentlyShownCategoryView;

    NSOpenPanel *o_selectFolderPanel;
    NSArray *o_hotkeyDescriptions;
    NSArray *o_hotkeyNames;
    NSArray *o_hotkeysNonUseableKeys;
    NSMutableArray *o_hotkeySettings;
    NSString *o_keyInTransition;

    intf_thread_t *p_intf;
}
+ (VLCSimplePrefs *)sharedInstance;
- (NSString *)OSXStringKeyToString:(NSString *)theString;

/* toolbar */
- (NSToolbarItem *) toolbar: (NSToolbar *)o_toolbar 
      itemForItemIdentifier: (NSString *)o_itemIdent 
  willBeInsertedIntoToolbar: (BOOL)b_willBeInserted;
- (NSArray *)toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar;
- (NSArray *)toolbarAllowedItemIdentifiers: (NSToolbar *)toolbar;

- (void)initStrings;
- (void)resetControls;
- (void)showSimplePrefs;

- (IBAction)buttonAction:(id)sender;
- (void)sheetDidEnd:(NSWindow *)o_sheet 
         returnCode:(int)i_return
        contextInfo:(void *)o_context;

- (void)saveChangedSettings;

/* interface */
- (IBAction)interfaceSettingChanged:(id)sender;
- (void)showInterfaceSettings;

/* audio */
- (IBAction)audioSettingChanged:(id)sender;
- (void)showAudioSettings;

/* video */
- (IBAction)videoSettingChanged:(id)sender;
- (void)showVideoSettings;

/* OSD / subtitles */
- (IBAction)osdSettingChanged:(id)sender;
- (IBAction)showFontPicker:(id)sender;
- (void)showOSDSettings;
- (void)changeFont:(id)sender;

/* input & codecs */
- (IBAction)inputSettingChanged:(id)sender;
- (void)showInputSettings;

/* hotkeys */
- (IBAction)hotkeySettingChanged:(id)sender;
- (void)showHotkeySettings;
- (int)numberOfRowsInTableView:(NSTableView *)aTableView;
- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex;
- (BOOL)changeHotkeyTo: (NSString *)theKey;

@end

@interface VLCHotkeyChangeWindow : NSWindow

@end

@interface VLCSimplePrefsWindow : NSWindow

@end