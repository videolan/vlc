/*****************************************************************************
 * MainMenu.h: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2011-2013 Felix Paul Kühne
 * $Id$
 *
 * Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
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
#import <vlc_common.h>
#import <vlc_interface.h>

@interface VLCMainMenu : NSObject
{
    intf_thread_t *p_intf;
    BOOL b_mainMenu_setup;
    BOOL b_nib_about_loaded;
    BOOL b_nib_videoeffects_loaded;
    BOOL b_nib_audioeffects_loaded;
    BOOL b_nib_tracksynchro_loaded;
    BOOL b_nib_bookmarks_loaded;
    BOOL b_nib_convertandsave_loaded;

    id o_about;                 /* VLAboutBox     */
    id o_videoeffects;          /* VLCVideoEffects */
    id o_audioeffects;          /* VLCAudioEffects */
    id o_trackSynchronization;  /* VLCTrackSynchronization */
    id o_bookmarks;             /* VLCBookmarks */
    id o_convertandsave;        /* VLCConvertAndSave */

    id o_extMgr;                /* Extensions Manager */

    /* main menu */

    IBOutlet NSMenuItem * o_mi_about;
    IBOutlet NSMenuItem * o_mi_prefs;
    IBOutlet NSMenuItem * o_mi_checkForUpdate;
    IBOutlet NSMenuItem * o_mi_extensions;
    IBOutlet NSMenu * o_mu_extensions;
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
    IBOutlet NSMenuItem * o_mi_open_wizard;
    IBOutlet NSMenuItem * o_mi_convertandsave;

    IBOutlet NSMenu * o_mu_edit;
    IBOutlet NSMenuItem * o_mi_cut;
    IBOutlet NSMenuItem * o_mi_copy;
    IBOutlet NSMenuItem * o_mi_paste;
    IBOutlet NSMenuItem * o_mi_clear;
    IBOutlet NSMenuItem * o_mi_select_all;

    IBOutlet NSMenu * o_mu_view;
    IBOutlet NSMenuItem * o_mi_toggleJumpButtons;
    IBOutlet NSMenuItem * o_mi_togglePlaymodeButtons;
    IBOutlet NSMenuItem * o_mi_toggleEffectsButton;
    IBOutlet NSMenuItem * o_mi_toggleSidebar;
    IBOutlet NSMenu * o_mu_playlistTableColumns;
    NSMenu * o_mu_playlistTableColumnsContextMenu;

    IBOutlet NSMenu * o_mu_controls;
    IBOutlet NSMenuItem * o_mi_play;
    IBOutlet NSMenuItem * o_mi_stop;
    IBOutlet NSMenuItem * o_mi_record;
    IBOutlet NSMenuItem * o_mi_rate;
    IBOutlet NSView * o_mi_rate_view;
    IBOutlet id o_mi_rate_lbl;
    IBOutlet id o_mi_rate_lbl_gray;
    IBOutlet id o_mi_rate_slower_lbl;
    IBOutlet id o_mi_rate_normal_lbl;
    IBOutlet id o_mi_rate_faster_lbl;
    IBOutlet id o_mi_rate_sld;
    IBOutlet id o_mi_rate_fld;
    IBOutlet NSMenuItem * o_mi_trackSynchronization;
    IBOutlet NSMenuItem * o_mi_previous;
    IBOutlet NSMenuItem * o_mi_next;
    IBOutlet NSMenuItem * o_mi_random;
    IBOutlet NSMenuItem * o_mi_repeat;
    IBOutlet NSMenuItem * o_mi_loop;
    IBOutlet NSMenuItem * o_mi_AtoBloop;
    IBOutlet NSMenuItem * o_mi_quitAfterPB;
    IBOutlet NSMenuItem * o_mi_fwd;
    IBOutlet NSMenuItem * o_mi_bwd;
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
    IBOutlet NSMenuItem * o_mi_deinterlace;
    IBOutlet NSMenu * o_mu_deinterlace;
    IBOutlet NSMenuItem * o_mi_deinterlace_mode;
    IBOutlet NSMenu * o_mu_deinterlace_mode;
    IBOutlet NSMenuItem * o_mi_ffmpeg_pp;
    IBOutlet NSMenu * o_mu_ffmpeg_pp;

    IBOutlet NSMenu * o_mu_subtitles;
    IBOutlet NSMenuItem * o_mi_subtitle_track;
    IBOutlet NSMenu * o_mu_subtitle_tracks;
    IBOutlet NSMenuItem * o_mi_openSubtitleFile;
    IBOutlet NSMenu * o_mu_subtitle_size;
    IBOutlet NSMenuItem *o_mi_subtitle_size;
    IBOutlet NSMenu * o_mu_subtitle_textcolor;
    IBOutlet NSMenuItem *o_mi_subtitle_textcolor;
    IBOutlet NSMenu * o_mu_subtitle_bgcolor;
    IBOutlet NSMenuItem * o_mi_subtitle_bgcolor;
    IBOutlet NSMenuItem * o_mi_subtitle_bgopacity;
    IBOutlet NSView * o_mi_subtitle_bgopacity_view;
    IBOutlet id o_mi_subtitle_bgopacity_lbl;
    IBOutlet id o_mi_subtitle_bgopacity_lbl_gray;
    IBOutlet id o_mi_subtitle_bgopacity_sld;
    IBOutlet NSMenu * o_mu_subtitle_outlinethickness;
    IBOutlet NSMenuItem * o_mi_subtitle_outlinethickness;
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
    IBOutlet NSMenuItem * o_mi_audioeffects;
    IBOutlet NSMenuItem * o_mi_videoeffects;
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

    // information for playlist table columns menu
    NSDictionary * o_ptc_translation_dict;
    NSArray * o_ptc_menuorder;
}
+ (VLCMainMenu *)sharedInstance;

- (void)initStrings;
- (void)releaseRepresentedObjects:(NSMenu *)the_menu;

- (void)setupMenus;
- (void)refreshVoutDeviceMenu:(NSNotification *)o_notification;
- (void)setSubmenusEnabled:(BOOL)b_enabled;
- (void)setRateControlsEnabled:(BOOL)b_enabled;
- (void)setupExtensionsMenu;
- (void)updateSidebarMenuItem;

- (IBAction)intfOpenFile:(id)sender;
- (IBAction)intfOpenFileGeneric:(id)sender;
- (IBAction)intfOpenDisc:(id)sender;
- (IBAction)intfOpenNet:(id)sender;
- (IBAction)intfOpenCapture:(id)sender;

- (IBAction)toggleEffectsButton:(id)sender;
- (IBAction)toggleJumpButtons:(id)sender;
- (IBAction)togglePlaymodeButtons:(id)sender;
- (IBAction)toggleSidebar:(id)sender;
- (IBAction)togglePlaylistColumnTable:(id)sender;
- (void)setPlaylistColumnTableState:(NSInteger)i_state forColumn:(NSString *)o_column;
- (NSMenu *)setupPlaylistTableColumnsMenu;

- (IBAction)toggleRecord:(id)sender;
- (void)updateRecordState:(BOOL)b_value;
- (IBAction)setPlaybackRate:(id)sender;
- (void)updatePlaybackRate;
- (IBAction)toggleAtoBloop:(id)sender;

- (IBAction)toggleAudioDevice:(id)sender;

- (IBAction)toggleFullscreen:(id)sender;
- (IBAction)resizeVideoWindow:(id)sender;
- (IBAction)floatOnTop:(id)sender;
- (IBAction)createVideoSnapshot:(id)sender;
- (IBAction)toggleFullscreenDevice:(id)sender;

- (IBAction)addSubtitleFile:(id)sender;
- (IBAction)switchSubtitleOption:(id)sender;
- (IBAction)switchSubtitleBackgroundOpacity:(id)sender;
- (IBAction)telxTransparent:(id)sender;
- (IBAction)telxNavLink:(id)sender;

- (IBAction)showWizard:(id)sender;
- (IBAction)showConvertAndSave:(id)sender;
- (IBAction)showVideoEffects:(id)sender;
- (IBAction)showAudioEffects:(id)sender;
- (IBAction)showTrackSynchronization:(id)sender;
- (IBAction)showBookmarks:(id)sender;
- (IBAction)showInformationPanel:(id)sender;

- (IBAction)viewAbout:(id)sender;
- (IBAction)showLicense:(id)sender;
- (IBAction)viewPreferences:(id)sender;
- (IBAction)viewHelp:(id)sender;
- (IBAction)openReadMe:(id)sender;
- (IBAction)openDocumentation:(id)sender;
- (IBAction)openWebsite:(id)sender;
- (IBAction)openForum:(id)sender;
- (IBAction)openDonate:(id)sender;
- (IBAction)viewErrorsAndWarnings:(id)sender;

- (void)setPlay;
- (void)setPause;
- (void)setRepeatOne;
- (void)setRepeatAll;
- (void)setRepeatOff;
- (void)setShuffle;

- (IBAction)toggleVar:(id)sender;
- (int)toggleVarThread:(id)_o_data;
- (void)setupVarMenuItem:(NSMenuItem *)o_mi
                  target:(vlc_object_t *)p_object
                     var:(const char *)psz_variable
                selector:(SEL)pf_callback;
- (void)setupVarMenu:(NSMenu *)o_menu
         forMenuItem: (NSMenuItem *)o_parent
              target:(vlc_object_t *)p_object
                 var:(const char *)psz_variable
            selector:(SEL)pf_callback;

- (id)voutMenu;
@end

/*****************************************************************************
 * VLCAutoGeneratedMenuContent interface
 *****************************************************************************
 * This holds our data for autogenerated menus
 *****************************************************************************/
@interface VLCAutoGeneratedMenuContent : NSObject
{
    char *psz_name;
    vlc_object_t * _vlc_object;
    vlc_value_t value;
    int i_type;
}

- (id)initWithVariableName: (const char *)name
                  ofObject: (vlc_object_t *)object
                  andValue: (vlc_value_t)value
                    ofType: (int)type;
- (const char *)name;
- (vlc_value_t)value;
- (vlc_object_t *)vlcObject;
- (int)type;

@end

