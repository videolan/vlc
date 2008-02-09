/*****************************************************************************
* simple_prefs.h: Simple Preferences for Mac OS X
*****************************************************************************
* Copyright (C) 2008 the VideoLAN team
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
#import <vlc/vlc.h>

@interface VLCSimplePrefs : NSObject
{
    IBOutlet id o_audio_dolby_pop;
    IBOutlet id o_audio_dolby_txt;
    IBOutlet id o_audio_effects_box;
    IBOutlet id o_audio_enable_ckb;
    IBOutlet id o_audio_general_box;
    IBOutlet id o_audio_headphone_ckb;
    IBOutlet id o_audio_lang_fld;
    IBOutlet id o_audio_lang_txt;
    IBOutlet id o_audio_last_box;
    IBOutlet id o_audio_last_ckb;
    IBOutlet id o_audio_lastpwd_fld;
    IBOutlet id o_audio_lastpwd_txt;
    IBOutlet id o_audio_lastuser_fld;
    IBOutlet id o_audio_lastuser_txt;
    IBOutlet id o_audio_norm_ckb;
    IBOutlet id o_audio_norm_fld;
    IBOutlet id o_audio_spdif_ckb;
    IBOutlet id o_audio_view;
    IBOutlet id o_audio_visual_pop;
    IBOutlet id o_audio_visual_txt;
    IBOutlet id o_audio_vol_fld;
    IBOutlet id o_audio_vol_sld;
    IBOutlet id o_audio_vol_txt;
    IBOutlet id o_intf_art_pop;
    IBOutlet id o_intf_art_txt;
    IBOutlet id o_intf_fspanel_ckb;
    IBOutlet id o_intf_lang_pop;
    IBOutlet id o_intf_lang_txt;
    IBOutlet id o_intf_meta_ckb;
    IBOutlet id o_intf_network_box;
    IBOutlet id o_intf_view;
    IBOutlet id o_sprefs_basic_box;
    IBOutlet id o_sprefs_basicFull_matrix;
    IBOutlet id o_sprefs_cancel_btn;
    IBOutlet id o_sprefs_controls_box;
    IBOutlet id o_sprefs_reset_btn;
    IBOutlet id o_sprefs_save_btn;
    IBOutlet id o_sprefs_win;

    BOOL b_audioSettingChanged;
    BOOL b_intfSettingChanged;
    id o_currentlyShownCategoryView;
    
    NSToolbar *o_sprefs_toolbar;
    
    intf_thread_t *p_intf;
}
+ (VLCSimplePrefs *)sharedInstance;

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

/* subtitles */

/* input & codecs */

/* hotkeys */

@end
