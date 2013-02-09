/*****************************************************************************
 * MainMenu.m: MacOS X interface module
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

#import "MainMenu.h"
#import <vlc_common.h>
#import <vlc_playlist.h>

#import "intf.h"
#import "open.h"
#import "wizard.h"
#import "about.h"
#import "AudioEffects.h"
#import "TrackSynchronization.h"
#import "VideoEffects.h"
#import "bookmarks.h"
#import "simple_prefs.h"
#import "coredialogs.h"
#import "controls.h"
#import "playlist.h"
#import "playlistinfo.h"
#import "VideoView.h"
#import "CoreInteraction.h"
#import "MainWindow.h"
#import "ControlsBar.h"
#import "ExtensionsManager.h"
#import "ConvertAndSave.h"

@implementation VLCMainMenu
static VLCMainMenu *_o_sharedInstance = nil;

+ (VLCMainMenu *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

#pragma mark -
#pragma mark Initialization

- (id)init
{
    if (_o_sharedInstance) {
        [self dealloc];
        return _o_sharedInstance;
    } else {
        _o_sharedInstance = [super init];

        o_ptc_translation_dict = [NSDictionary dictionaryWithObjectsAndKeys:
                      _NS("Track Number"),  TRACKNUM_COLUMN,
                      _NS("Title"),         TITLE_COLUMN,
                      _NS("Author"),        ARTIST_COLUMN,
                      _NS("Duration"),      DURATION_COLUMN,
                      _NS("Genre"),         GENRE_COLUMN,
                      _NS("Album"),         ALBUM_COLUMN,
                      _NS("Description"),   DESCRIPTION_COLUMN,
                      _NS("Date"),          DATE_COLUMN,
                      _NS("Language"),      LANGUAGE_COLUMN,
                      _NS("URI"),           URI_COLUMN,
                      nil];
        // this array also assigns tags (index) to type of menu item
        o_ptc_menuorder = [NSArray arrayWithObjects:TRACKNUM_COLUMN, TITLE_COLUMN, ARTIST_COLUMN, DURATION_COLUMN,
                       GENRE_COLUMN, ALBUM_COLUMN, DESCRIPTION_COLUMN, DATE_COLUMN, LANGUAGE_COLUMN, URI_COLUMN, nil];
    }

    return _o_sharedInstance;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    if (b_nib_about_loaded)
        [o_about release];

    if (b_nib_videoeffects_loaded)
        [o_videoeffects release];

    if (b_nib_audioeffects_loaded)
        [o_audioeffects release];

    if (b_nib_tracksynchro_loaded)
        [o_trackSynchronization release];

    if (b_nib_convertandsave_loaded)
        [o_convertandsave release];

    [o_extMgr release];

    if (o_mu_playlistTableColumnsContextMenu)
        [o_mu_playlistTableColumnsContextMenu release];

    [self releaseRepresentedObjects:[NSApp mainMenu]];

    [super dealloc];
}

- (void)awakeFromNib
{
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(applicationWillFinishLaunching:)
                                                 name: NSApplicationWillFinishLaunchingNotification
                                               object: nil];

    /* check whether the user runs OSX with a RTL language */
    NSArray* languages = [NSLocale preferredLanguages];
    NSString* preferredLanguage = [languages objectAtIndex:0];

    if ([NSLocale characterDirectionForLanguage:preferredLanguage] == NSLocaleLanguageDirectionRightToLeft) {
        msg_Dbg(VLCIntf, "adapting interface since '%s' is a RTL language", [preferredLanguage UTF8String]);
        [o_mi_rate_fld setAlignment: NSLeftTextAlignment];
    }
}

- (void)applicationWillFinishLaunching:(NSNotification *)o_notification
{
    p_intf = VLCIntf;

    NSString* o_key;
    playlist_t *p_playlist;
    vlc_value_t val;
    id o_vlcstringutility = [VLCStringUtility sharedInstance];
    char * key;

    /* Check if we already did this once. Opening the other nibs calls it too,
     because VLCMain is the owner */
    if (b_mainMenu_setup)
        return;

    /* Get ExtensionsManager */
    o_extMgr = [ExtensionsManager getInstance:p_intf];
    [o_extMgr retain];

    [self initStrings];

    key = config_GetPsz(p_intf, "key-quit");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_quit setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_quit setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-play-pause");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_play setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_play setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-stop");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_stop setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_stop setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-prev");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_previous setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_previous setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-next");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_next setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_next setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-jump+short");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_fwd setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_fwd setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-jump-short");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_bwd setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_bwd setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-vol-up");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_vol_up setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_vol_up setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-vol-down");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_vol_down setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_vol_down setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-vol-mute");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_mute setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_mute setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-toggle-fullscreen");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_fullscreen setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_fullscreen setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-snapshot");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_snapshot setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_snapshot setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-random");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_random setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_random setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-zoom-half");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_half_window setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_half_window setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-zoom-original");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_normal_window setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_normal_window setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    key = config_GetPsz(p_intf, "key-zoom-double");
    o_key = [NSString stringWithFormat:@"%s", key];
    [o_mi_double_window setKeyEquivalent: [o_vlcstringutility VLCKeyToString: o_key]];
    [o_mi_double_window setKeyEquivalentModifierMask: [o_vlcstringutility VLCModifiersToCocoa:o_key]];
    FREENULL(key);

    [self setSubmenusEnabled: FALSE];

    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector(refreshVoutDeviceMenu:)
                                                 name: NSApplicationDidChangeScreenParametersNotification
                                               object: nil];

    /* we're done */
    b_mainMenu_setup = YES;

    [self setupVarMenuItem: o_mi_add_intf target: (vlc_object_t *)p_intf
                             var: "intf-add" selector: @selector(toggleVar:)];

    [self setupExtensionsMenu];

    [self refreshAudioDeviceList];
}

- (void)initStrings
{
    /* main menu */
    [o_mi_about setTitle: [_NS("About VLC media player") \
                           stringByAppendingString: @"..."]];
    [o_mi_checkForUpdate setTitle: _NS("Check for Update...")];
    [o_mi_prefs setTitle: _NS("Preferences...")];
    [o_mi_extensions setTitle: _NS("Extensions")];
    [o_mu_extensions setTitle: _NS("Extensions")];
    [o_mi_add_intf setTitle: _NS("Add Interface")];
    [o_mu_add_intf setTitle: _NS("Add Interface")];
    [o_mi_services setTitle: _NS("Services")];
    [o_mi_hide setTitle: _NS("Hide VLC")];
    [o_mi_hide_others setTitle: _NS("Hide Others")];
    [o_mi_show_all setTitle: _NS("Show All")];
    [o_mi_quit setTitle: _NS("Quit VLC")];

    [o_mu_file setTitle: _ANS("1:File")];
    [o_mi_open_generic setTitle: _NS("Advanced Open File...")];
    [o_mi_open_file setTitle: _NS("Open File...")];
    [o_mi_open_disc setTitle: _NS("Open Disc...")];
    [o_mi_open_net setTitle: _NS("Open Network...")];
    [o_mi_open_capture setTitle: _NS("Open Capture Device...")];
    [o_mi_open_recent setTitle: _NS("Open Recent")];
    [o_mi_open_wizard setTitle: _NS("Streaming/Exporting Wizard...")];
    [o_mi_convertandsave setTitle: _NS("Convert / Stream...")];

    [o_mu_edit setTitle: _NS("Edit")];
    [o_mi_cut setTitle: _NS("Cut")];
    [o_mi_copy setTitle: _NS("Copy")];
    [o_mi_paste setTitle: _NS("Paste")];
    [o_mi_clear setTitle: _NS("Clear")];
    [o_mi_select_all setTitle: _NS("Select All")];

    [o_mu_view setTitle: _NS("View")];
    [o_mi_toggleJumpButtons setTitle: _NS("Show Previous & Next Buttons")];
    [o_mi_toggleJumpButtons setState: config_GetInt(VLCIntf, "macosx-show-playback-buttons")];
    [o_mi_togglePlaymodeButtons setTitle: _NS("Show Shuffle & Repeat Buttons")];
    [o_mi_togglePlaymodeButtons setState: config_GetInt(VLCIntf, "macosx-show-playmode-buttons")];
    [o_mi_toggleSidebar setTitle: _NS("Show Sidebar")];
    [o_mi_toggleSidebar setState: config_GetInt(VLCIntf, "macosx-show-sidebar")];
    [o_mu_playlistTableColumns setTitle: _NS("Playlist Table Columns")];

    [o_mu_controls setTitle: _NS("Playback")];
    [o_mi_play setTitle: _NS("Play")];
    [o_mi_stop setTitle: _NS("Stop")];
    [o_mi_record setTitle: _NS("Record")];
    [o_mi_rate setView: o_mi_rate_view];
    [o_mi_rate_lbl setStringValue: _NS("Playback Speed")];
    [o_mi_rate_lbl_gray setStringValue: _NS("Playback Speed")];
    [o_mi_rate_slower_lbl setStringValue: _NS("Slower")];
    [o_mi_rate_normal_lbl setStringValue: _NS("Normal")];
    [o_mi_rate_faster_lbl setStringValue: _NS("Faster")];
    [o_mi_trackSynchronization setTitle: _NS("Track Synchronization")];
    [o_mi_previous setTitle: _NS("Previous")];
    [o_mi_next setTitle: _NS("Next")];
    [o_mi_random setTitle: _NS("Random")];
    [o_mi_repeat setTitle: _NS("Repeat One")];
    [o_mi_loop setTitle: _NS("Repeat All")];
    [o_mi_AtoBloop setTitle: _NS("A→B Loop")];
    [o_mi_quitAfterPB setTitle: _NS("Quit after Playback")];
    [o_mi_fwd setTitle: _NS("Step Forward")];
    [o_mi_bwd setTitle: _NS("Step Backward")];

    [o_mi_program setTitle: _NS("Program")];
    [o_mu_program setTitle: _NS("Program")];
    [o_mi_title setTitle: _NS("Title")];
    [o_mu_title setTitle: _NS("Title")];
    [o_mi_chapter setTitle: _NS("Chapter")];
    [o_mu_chapter setTitle: _NS("Chapter")];

    [o_mu_audio setTitle: _NS("Audio")];
    [o_mi_vol_up setTitle: _NS("Increase Volume")];
    [o_mi_vol_down setTitle: _NS("Decrease Volume")];
    [o_mi_mute setTitle: _NS("Mute")];
    [o_mi_audiotrack setTitle: _NS("Audio Track")];
    [o_mu_audiotrack setTitle: _NS("Audio Track")];
    [o_mi_channels setTitle: _NS("Audio Channels")];
    [o_mu_channels setTitle: _NS("Audio Channels")];
    [o_mi_device setTitle: _NS("Audio Device")];
    [o_mu_device setTitle: _NS("Audio Device")];
    [o_mi_visual setTitle: _NS("Visualizations")];
    [o_mu_visual setTitle: _NS("Visualizations")];

    [o_mu_video setTitle: _NS("Video")];
    [o_mi_half_window setTitle: _NS("Half Size")];
    [o_mi_normal_window setTitle: _NS("Normal Size")];
    [o_mi_double_window setTitle: _NS("Double Size")];
    [o_mi_fittoscreen setTitle: _NS("Fit to Screen")];
    [o_mi_fullscreen setTitle: _NS("Fullscreen")];
    [o_mi_floatontop setTitle: _NS("Float on Top")];
    [o_mi_snapshot setTitle: _NS("Snapshot")];
    [o_mi_videotrack setTitle: _NS("Video Track")];
    [o_mu_videotrack setTitle: _NS("Video Track")];
    [o_mi_aspect_ratio setTitle: _NS("Aspect-ratio")];
    [o_mu_aspect_ratio setTitle: _NS("Aspect-ratio")];
    [o_mi_crop setTitle: _NS("Crop")];
    [o_mu_crop setTitle: _NS("Crop")];
    [o_mi_screen setTitle: _NS("Fullscreen Video Device")];
    [o_mu_screen setTitle: _NS("Fullscreen Video Device")];
    [o_mi_subtitle setTitle: _NS("Subtitles Track")];
    [o_mu_subtitle setTitle: _NS("Subtitles Track")];
    [o_mi_addSub setTitle: _NS("Open File...")];
    [o_mi_deinterlace setTitle: _NS("Deinterlace")];
    [o_mu_deinterlace setTitle: _NS("Deinterlace")];
    [o_mi_deinterlace_mode setTitle: _NS("Deinterlace mode")];
    [o_mu_deinterlace_mode setTitle: _NS("Deinterlace mode")];
    [o_mi_ffmpeg_pp setTitle: _NS("Post processing")];
    [o_mu_ffmpeg_pp setTitle: _NS("Post processing")];
    [o_mi_teletext setTitle: _NS("Teletext")];
    [o_mi_teletext_transparent setTitle: _NS("Transparent")];
    [o_mi_teletext_index setTitle: _NS("Index")];
    [o_mi_teletext_red setTitle: _NS("Red")];
    [o_mi_teletext_green setTitle: _NS("Green")];
    [o_mi_teletext_yellow setTitle: _NS("Yellow")];
    [o_mi_teletext_blue setTitle: _NS("Blue")];

    [o_mu_window setTitle: _NS("Window")];
    [o_mi_minimize setTitle: _NS("Minimize Window")];
    [o_mi_close_window setTitle: _NS("Close Window")];
    [o_mi_player setTitle: _NS("Player...")];
    [o_mi_controller setTitle: _NS("Main Window...")];
    [o_mi_audioeffects setTitle: _NS("Audio Effects...")];
    [o_mi_videoeffects setTitle: _NS("Video Effects...")];
    [o_mi_bookmarks setTitle: _NS("Bookmarks...")];
    [o_mi_playlist setTitle: _NS("Playlist...")];
    [o_mi_info setTitle: _NS("Media Information...")];
    [o_mi_messages setTitle: _NS("Messages...")];
    [o_mi_errorsAndWarnings setTitle: _NS("Errors and Warnings...")];

    [o_mi_bring_atf setTitle: _NS("Bring All to Front")];

    [o_mu_help setTitle: _NS("Help")];
    [o_mi_help setTitle: _NS("VLC media player Help...")];
    [o_mi_readme setTitle: _NS("ReadMe / FAQ...")];
    [o_mi_license setTitle: _NS("License")];
    [o_mi_documentation setTitle: _NS("Online Documentation...")];
    [o_mi_website setTitle: _NS("VideoLAN Website...")];
    [o_mi_donation setTitle: _NS("Make a donation...")];
    [o_mi_forum setTitle: _NS("Online Forum...")];

    /* dock menu */
    [o_dmi_play setTitle: _NS("Play")];
    [o_dmi_stop setTitle: _NS("Stop")];
    [o_dmi_next setTitle: _NS("Next")];
    [o_dmi_previous setTitle: _NS("Previous")];
    [o_dmi_mute setTitle: _NS("Mute")];

    /* vout menu */
    [o_vmi_play setTitle: _NS("Play")];
    [o_vmi_stop setTitle: _NS("Stop")];
    [o_vmi_prev setTitle: _NS("Previous")];
    [o_vmi_next setTitle: _NS("Next")];
    [o_vmi_volup setTitle: _NS("Volume Up")];
    [o_vmi_voldown setTitle: _NS("Volume Down")];
    [o_vmi_mute setTitle: _NS("Mute")];
    [o_vmi_fullscreen setTitle: _NS("Fullscreen")];
    [o_vmi_snapshot setTitle: _NS("Snapshot")];
}

- (NSMenu *)setupPlaylistTableColumnsMenu
{
    NSMenu *o_context_menu = [[NSMenu alloc] init];

    NSMenuItem *o_mi_tmp;
    NSUInteger count = [o_ptc_menuorder count];
    for (NSUInteger i = 0; i < count; i++) {
        NSString *o_title = [o_ptc_translation_dict objectForKey:[o_ptc_menuorder objectAtIndex:i]];
        o_mi_tmp = [o_mu_playlistTableColumns addItemWithTitle:o_title
                                                        action:@selector(togglePlaylistColumnTable:)
                                                 keyEquivalent:@""];
        /* don't set a valid target for the title column selector, since we want it to be disabled */
        if (![[o_ptc_menuorder objectAtIndex:i] isEqualToString: TITLE_COLUMN])
            [o_mi_tmp setTarget:self];
        [o_mi_tmp setTag:i];

        o_mi_tmp = [o_context_menu addItemWithTitle:o_title
                                             action:@selector(togglePlaylistColumnTable:)
                                      keyEquivalent:@""];
        /* don't set a valid target for the title column selector, since we want it to be disabled */
        if (![[o_ptc_menuorder objectAtIndex:i] isEqualToString: TITLE_COLUMN])
            [o_mi_tmp setTarget:self];
        [o_mi_tmp setTag:i];
    }
    if (!o_mu_playlistTableColumnsContextMenu)
        o_mu_playlistTableColumnsContextMenu = [o_context_menu retain];
    return [o_context_menu autorelease];
}

#pragma mark -
#pragma mark Termination

- (void)releaseRepresentedObjects:(NSMenu *)the_menu
{
    if (!p_intf) return;

    NSArray *menuitems_array = [the_menu itemArray];
    NSUInteger menuItemCount = [menuitems_array count];
    for (NSUInteger i=0; i < menuItemCount; i++) {
        NSMenuItem *one_item = [menuitems_array objectAtIndex: i];
        if ([one_item hasSubmenu])
            [self releaseRepresentedObjects: [one_item submenu]];

        [one_item setRepresentedObject:NULL];
    }
}

#pragma mark -
#pragma mark Interface update

- (void)setupMenus
{
    playlist_t * p_playlist = pl_Get(p_intf);
    input_thread_t * p_input = playlist_CurrentInput(p_playlist);
    if (p_input != NULL) {
        [o_mi_record setEnabled: var_GetBool(p_input, "can-record")];

        [self setupVarMenuItem: o_mi_program target: (vlc_object_t *)p_input
                                 var: "program" selector: @selector(toggleVar:)];

        [self setupVarMenuItem: o_mi_title target: (vlc_object_t *)p_input
                                 var: "title" selector: @selector(toggleVar:)];

        [self setupVarMenuItem: o_mi_chapter target: (vlc_object_t *)p_input
                                 var: "chapter" selector: @selector(toggleVar:)];

        [self setupVarMenuItem: o_mi_audiotrack target: (vlc_object_t *)p_input
                                 var: "audio-es" selector: @selector(toggleVar:)];

        [self setupVarMenuItem: o_mi_videotrack target: (vlc_object_t *)p_input
                                 var: "video-es" selector: @selector(toggleVar:)];

        [self setupVarMenuItem: o_mi_subtitle target: (vlc_object_t *)p_input
                                 var: "spu-es" selector: @selector(toggleVar:)];

        /* special case for "Open File" inside the subtitles menu item */
        if ([o_mi_videotrack isEnabled] == YES)
            [o_mi_subtitle setEnabled: YES];

        audio_output_t * p_aout = playlist_GetAout(p_playlist);
        if (p_aout != NULL) {
            [self setupVarMenuItem: o_mi_channels target: (vlc_object_t *)p_aout
                                     var: "stereo-mode" selector: @selector(toggleVar:)];

            [self setupVarMenuItem: o_mi_visual target: (vlc_object_t *)p_aout
                                     var: "visual" selector: @selector(toggleVar:)];
            vlc_object_release(p_aout);
        }

        vout_thread_t * p_vout = getVoutForActiveWindow();

        if (p_vout != NULL) {
            [self setupVarMenuItem: o_mi_aspect_ratio target: (vlc_object_t *)p_vout
                                     var: "aspect-ratio" selector: @selector(toggleVar:)];

            [self setupVarMenuItem: o_mi_crop target: (vlc_object_t *) p_vout
                                     var: "crop" selector: @selector(toggleVar:)];

            [self setupVarMenuItem: o_mi_deinterlace target: (vlc_object_t *)p_vout
                                     var: "deinterlace" selector: @selector(toggleVar:)];

            [self setupVarMenuItem: o_mi_deinterlace_mode target: (vlc_object_t *)p_vout
                                     var: "deinterlace-mode" selector: @selector(toggleVar:)];

#if 1
            [self setupVarMenuItem: o_mi_ffmpeg_pp target:
             (vlc_object_t *)p_vout var:"postprocess" selector:
             @selector(toggleVar:)];
#endif
            vlc_object_release(p_vout);

            [self refreshVoutDeviceMenu:nil];
        }
        vlc_object_release(p_input);
    }
    else
        [o_mi_record setEnabled: NO];
}

- (void)refreshVoutDeviceMenu:(NSNotification *)o_notification
{
    NSUInteger count = [o_mu_screen numberOfItems];
    NSMenu * o_submenu = o_mu_screen;
    if (count > 0)
        [o_submenu removeAllItems];

    NSArray * o_screens = [NSScreen screens];
    NSMenuItem * o_mitem;
    count = [o_screens count];
    [o_mi_screen setEnabled: YES];
    [o_submenu addItemWithTitle: _NS("Default") action:@selector(toggleFullscreenDevice:) keyEquivalent:@""];
    o_mitem = [o_submenu itemAtIndex: 0];
    [o_mitem setTag: 0];
    [o_mitem setEnabled: YES];
    [o_mitem setTarget: self];
    NSRect s_rect;
    for (NSUInteger i = 0; i < count; i++) {
        s_rect = [[o_screens objectAtIndex: i] frame];
        [o_submenu addItemWithTitle: [NSString stringWithFormat: @"%@ %li (%ix%i)", _NS("Screen"), i+1,
                                      (int)s_rect.size.width, (int)s_rect.size.height] action:@selector(toggleFullscreenDevice:) keyEquivalent:@""];
        o_mitem = [o_submenu itemAtIndex:i+1];
        [o_mitem setTag: (int)[[o_screens objectAtIndex: i] displayID]];
        [o_mitem setEnabled: YES];
        [o_mitem setTarget: self];
    }
    [[o_submenu itemWithTag: var_InheritInteger(VLCIntf, "macosx-vdev")] setState: NSOnState];
}

- (void)setSubmenusEnabled:(BOOL)b_enabled
{
    [o_mi_program setEnabled: b_enabled];
    [o_mi_title setEnabled: b_enabled];
    [o_mi_chapter setEnabled: b_enabled];
    [o_mi_audiotrack setEnabled: b_enabled];
    [o_mi_visual setEnabled: b_enabled];
    [o_mi_videotrack setEnabled: b_enabled];
    [o_mi_subtitle setEnabled: b_enabled];
    [o_mi_channels setEnabled: b_enabled];
    [o_mi_deinterlace setEnabled: b_enabled];
    [o_mi_deinterlace_mode setEnabled: b_enabled];
    [o_mi_ffmpeg_pp setEnabled: b_enabled];
    [o_mi_screen setEnabled: b_enabled];
    [o_mi_aspect_ratio setEnabled: b_enabled];
    [o_mi_crop setEnabled: b_enabled];
    [o_mi_teletext setEnabled: b_enabled];
}

- (void)setRateControlsEnabled:(BOOL)b_enabled
{
    NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];
    [o_mi_rate_sld setEnabled: b_enabled];
    [o_mi_rate_sld setIntValue: [[VLCCoreInteraction sharedInstance] playbackRate]];
    int i = [[VLCCoreInteraction sharedInstance] playbackRate];
    double speed =  pow(2, (double)i / 17);
    [o_mi_rate_fld setStringValue: [NSString stringWithFormat:@"%.2fx", speed]];
    if (b_enabled) {
        [o_mi_rate_lbl setHidden: NO];
        [o_mi_rate_lbl_gray setHidden: YES];
    } else {
        [o_mi_rate_lbl setHidden: YES];
        [o_mi_rate_lbl_gray setHidden: NO];
    }
    [o_pool release];
}

#pragma mark -
#pragma mark Extensions

- (void)setupExtensionsMenu
{
    /* Load extensions if needed */
    // TODO: Implement preference for autoloading extensions on mac

    // if (!var_InheritBool(p_intf, "qt-autoload-extensions")
    //     && ![o_extMgr isLoaded])
    // {
    //     return;
    // }

    if (![o_extMgr isLoaded] && ![o_extMgr cannotLoad]) {
        [o_extMgr loadExtensions];
    }

    /* Let the ExtensionsManager itself build the menu */
    [o_extMgr buildMenu:o_mu_extensions];
    [o_mi_extensions setEnabled: ([o_mu_extensions numberOfItems] > 0)];
}

#pragma mark -
#pragma mark View
- (IBAction)toggleJumpButtons:(id)sender
{
    BOOL b_value = !config_GetInt(VLCIntf, "macosx-show-playback-buttons");
    config_PutInt(VLCIntf, "macosx-show-playback-buttons", b_value);
    [[[[VLCMain sharedInstance] mainWindow] controlsBar] toggleJumpButtons];
    [o_mi_toggleJumpButtons setState: b_value];
}

- (IBAction)togglePlaymodeButtons:(id)sender
{
    BOOL b_value = !config_GetInt(VLCIntf, "macosx-show-playmode-buttons");
    config_PutInt(VLCIntf, "macosx-show-playmode-buttons", b_value);
    [[[[VLCMain sharedInstance] mainWindow] controlsBar] togglePlaymodeButtons];
    [o_mi_togglePlaymodeButtons setState: b_value];
}

- (IBAction)toggleSidebar:(id)sender
{
    BOOL b_value = !config_GetInt(VLCIntf, "macosx-show-sidebar");
    config_PutInt(VLCIntf, "macosx-show-sidebar", b_value);
    [[[VLCMain sharedInstance] mainWindow] toggleLeftSubSplitView];
    [o_mi_toggleSidebar setState: b_value];
}

- (void)updateSidebarMenuItem
{
    [o_mi_toggleSidebar setState: config_GetInt(VLCIntf, "macosx-show-sidebar")];
}

- (IBAction)togglePlaylistColumnTable:(id)sender
{
    NSInteger i_new_state = ![sender state];
    NSInteger i_tag = [sender tag];
    [[o_mu_playlistTableColumns            itemWithTag: i_tag] setState: i_new_state];
    [[o_mu_playlistTableColumnsContextMenu itemWithTag: i_tag] setState: i_new_state];

    NSString *o_column = [o_ptc_menuorder objectAtIndex: i_tag];
    [[[VLCMain sharedInstance] playlist] setColumn: o_column state: i_new_state translationDict: o_ptc_translation_dict];
}

- (void)setPlaylistColumnTableState:(NSInteger)i_state forColumn:(NSString *)o_column
{
    NSInteger i_tag = [o_ptc_menuorder indexOfObject: o_column];
    [[o_mu_playlistTableColumns            itemWithTag: i_tag] setState: i_state];
    [[o_mu_playlistTableColumnsContextMenu itemWithTag: i_tag] setState: i_state];
    [[[VLCMain sharedInstance] playlist] setColumn: o_column state: i_state translationDict: o_ptc_translation_dict];
}

#pragma mark -
#pragma mark Playback
- (IBAction)toggleRecord:(id)sender
{
    [[VLCCoreInteraction sharedInstance] toggleRecord];
}

- (void)updateRecordState:(BOOL)b_value
{
    [o_mi_record setState:b_value];
}

- (IBAction)setPlaybackRate:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setPlaybackRate: [o_mi_rate_sld intValue]];
    int i = [[VLCCoreInteraction sharedInstance] playbackRate];
    double speed =  pow(2, (double)i / 17);
    [o_mi_rate_fld setStringValue: [NSString stringWithFormat:@"%.2fx", speed]];
}

- (void)updatePlaybackRate
{
    int i = [[VLCCoreInteraction sharedInstance] playbackRate];
    double speed =  pow(2, (double)i / 17);
    [o_mi_rate_fld setStringValue: [NSString stringWithFormat:@"%.2fx", speed]];
    [o_mi_rate_sld setIntValue: i];
}

- (IBAction)toggleAtoBloop:(id)sender
{
    [[VLCCoreInteraction sharedInstance] setAtoB];
}

#pragma mark -
#pragma mark audio menu
- (void)refreshAudioDeviceList
{
    char **ids, **names;
    char *currentDevice;

    [o_mu_device removeAllItems];

    audio_output_t * p_aout = getAout();
    if (!p_aout)
        return;

    int n = aout_DevicesList(p_aout, &ids, &names);
    if (n == -1)
        return;

    currentDevice = aout_DeviceGet(p_aout);

    NSMenuItem * o_mi_tmp;
    o_mi_tmp = [o_mu_device addItemWithTitle:_NS("Default") action:@selector(toggleAudioDevice:) keyEquivalent:@""];
    [o_mi_tmp setTarget:self];
    if (!currentDevice)
        [o_mi_tmp setState:NSOnState];

    for (NSUInteger x = 0; x < n; x++) {
        o_mi_tmp = [o_mu_device addItemWithTitle:[NSString stringWithFormat:@"%s", names[x]] action:@selector(toggleAudioDevice:) keyEquivalent:@""];
        [o_mi_tmp setTarget:self];
        [o_mi_tmp setTag:[[NSString stringWithFormat:@"%s", ids[x]] intValue]];
        if (currentDevice) {
            if (!strcmp(ids[x], currentDevice))
                [o_mi_tmp setState: NSOnState];
        }
    }
    vlc_object_release(p_aout);

    for (NSUInteger x = 0; x < n; x++) {
        free(ids[x]);
        free(names[x]);
    }
    free(ids);
    free(names);

    [o_mu_device setAutoenablesItems:YES];
    [o_mi_device setEnabled:YES];
}

- (IBAction)toggleAudioDevice:(id)sender
{
    audio_output_t * p_aout = getAout();

    int returnValue = 0;

    if ([sender tag] > 0)
        aout_DeviceSet(p_aout, [[NSString stringWithFormat:@"%li", [sender tag]] UTF8String]);
    else
        aout_DeviceSet(p_aout, NULL);

    if (returnValue != 0)
        msg_Warn(VLCIntf, "failed to set audio device %li", [sender tag]);

    [self refreshAudioDeviceList];
}

#pragma mark -
#pragma mark video menu

- (IBAction)toggleFullscreen:(id)sender
{
    [[VLCCoreInteraction sharedInstance] toggleFullscreen];
}

- (IBAction)resizeVideoWindow:(id)sender
{
    input_thread_t *p_input = pl_CurrentInput(VLCIntf);
    if (p_input) {
        vout_thread_t *p_vout = getVoutForActiveWindow();
        if (p_vout) {
            if (sender == o_mi_half_window)
                var_SetFloat(p_vout, "zoom", 0.5);
            else if (sender == o_mi_normal_window)
                var_SetFloat(p_vout, "zoom", 1.0);
            else if (sender == o_mi_double_window)
                var_SetFloat(p_vout, "zoom", 2.0);
            else
            {
                [[NSApp keyWindow] performZoom:sender];
            }
            vlc_object_release(p_vout);
        }
        vlc_object_release(p_input);
    }
}

- (IBAction)floatOnTop:(id)sender
{
    input_thread_t *p_input = pl_CurrentInput(VLCIntf);
    if (p_input) {
        vout_thread_t *p_vout = getVoutForActiveWindow();
        if (p_vout) {
            var_ToggleBool(p_vout, "video-on-top");
            vlc_object_release(p_vout);
        }
        vlc_object_release(p_input);
    }
}

- (IBAction)createVideoSnapshot:(id)sender
{
    input_thread_t *p_input = pl_CurrentInput(VLCIntf);
    if (p_input) {
        vout_thread_t *p_vout = getVoutForActiveWindow();
        if (p_vout) {
            var_TriggerCallback(p_vout, "video-snapshot");
            vlc_object_release(p_vout);
        }
        vlc_object_release(p_input);
    }
}

- (IBAction)toggleFullscreenDevice:(id)sender
{
    config_PutInt(VLCIntf, "macosx-vdev", [sender tag]);
    [self refreshVoutDeviceMenu: nil];
}

- (id)voutMenu
{
    return o_vout_menu;
}

#pragma mark -
#pragma mark Panels

- (IBAction)intfOpenFile:(id)sender
{
    [[[VLCMain sharedInstance] open] openFile];
}

- (IBAction)intfOpenFileGeneric:(id)sender
{
    [[[VLCMain sharedInstance] open] openFileGeneric];
}

- (IBAction)intfOpenDisc:(id)sender
{
    [[[VLCMain sharedInstance] open] openDisc];
}

- (IBAction)intfOpenNet:(id)sender
{
    [[[VLCMain sharedInstance] open] openNet];
}

- (IBAction)intfOpenCapture:(id)sender
{
    [[[VLCMain sharedInstance] open] openCapture];
}

- (IBAction)showWizard:(id)sender
{
    [[[VLCMain sharedInstance] wizard] resetWizard];
    [[[VLCMain sharedInstance] wizard] showWizard];
}

- (IBAction)showConvertAndSave:(id)sender
{
    if (o_convertandsave == nil)
        o_convertandsave = [[VLCConvertAndSave alloc] init];

    if (!b_nib_convertandsave_loaded)
        b_nib_convertandsave_loaded = [NSBundle loadNibNamed:@"ConvertAndSave" owner: NSApp];

    [o_convertandsave toggleWindow];
}

- (IBAction)showVideoEffects:(id)sender
{
    if (o_videoeffects == nil)
        o_videoeffects = [[VLCVideoEffects alloc] init];

    if (!b_nib_videoeffects_loaded)
        b_nib_videoeffects_loaded = [NSBundle loadNibNamed:@"VideoEffects" owner: NSApp];

    [o_videoeffects toggleWindow:sender];
}

- (IBAction)showTrackSynchronization:(id)sender
{
    if (!o_trackSynchronization)
        o_trackSynchronization = [[VLCTrackSynchronization alloc] init];

    if (!b_nib_tracksynchro_loaded)
        b_nib_tracksynchro_loaded = [NSBundle loadNibNamed:@"SyncTracks" owner:NSApp];

    [o_trackSynchronization toggleWindow:sender];
}

- (IBAction)showAudioEffects:(id)sender
{
    if (!o_audioeffects)
        o_audioeffects = [[VLCAudioEffects alloc] init];

    if (!b_nib_audioeffects_loaded)
        b_nib_audioeffects_loaded = [NSBundle loadNibNamed:@"AudioEffects" owner:NSApp];

    [o_audioeffects toggleWindow:sender];
}

- (IBAction)showBookmarks:(id)sender
{
    [[[VLCMain sharedInstance] bookmarks] showBookmarks];
}

- (IBAction)viewPreferences:(id)sender
{
    NSInteger i_level = [[[VLCMain sharedInstance] voutController] currentWindowLevel];
    [[[VLCMain sharedInstance] simplePreferences] showSimplePrefsWithLevel:i_level];
}

#pragma mark -
#pragma mark Help and Docs

- (void)initAbout
{
    if (! o_about)
        o_about = [[VLAboutBox alloc] init];

    if (!b_nib_about_loaded)
        b_nib_about_loaded = [NSBundle loadNibNamed:@"About" owner: NSApp];
}

- (IBAction)viewAbout:(id)sender
{
    [self initAbout];
    [o_about showAbout];
}

- (IBAction)showLicense:(id)sender
{
    [self initAbout];
    [o_about showGPL: sender];
}

- (IBAction)viewHelp:(id)sender
{
    [self initAbout];
    [o_about showHelp];
}

- (IBAction)openReadMe:(id)sender
{
    NSString * o_path = [[NSBundle mainBundle] pathForResource: @"README.MacOSX" ofType: @"rtf"];

    [[NSWorkspace sharedWorkspace] openFile: o_path withApplication: @"TextEdit"];
}

- (IBAction)openDocumentation:(id)sender
{
    NSURL * o_url = [NSURL URLWithString: @"http://www.videolan.org/doc/"];

    [[NSWorkspace sharedWorkspace] openURL: o_url];
}

- (IBAction)openWebsite:(id)sender
{
    NSURL * o_url = [NSURL URLWithString: @"http://www.videolan.org/"];

    [[NSWorkspace sharedWorkspace] openURL: o_url];
}

- (IBAction)openForum:(id)sender
{
    NSURL * o_url = [NSURL URLWithString: @"http://forum.videolan.org/"];

    [[NSWorkspace sharedWorkspace] openURL: o_url];
}

- (IBAction)openDonate:(id)sender
{
    NSURL * o_url = [NSURL URLWithString: @"http://www.videolan.org/contribute.html#paypal"];

    [[NSWorkspace sharedWorkspace] openURL: o_url];
}

#pragma mark -
#pragma mark Errors, warnings and messages

- (IBAction)viewErrorsAndWarnings:(id)sender
{
    [[[[VLCMain sharedInstance] coreDialogProvider] errorPanel] showPanel];
}

- (IBAction)showInformationPanel:(id)sender
{
    [[[VLCMain sharedInstance] info] initPanel];
}

#pragma mark -
#pragma mark convinience stuff for other objects
- (void)setPlay
{
    [o_mi_play setTitle: _NS("Play")];
    [o_dmi_play setTitle: _NS("Play")];
    [o_vmi_play setTitle: _NS("Play")];
}

- (void)setPause
{
    [o_mi_play setTitle: _NS("Pause")];
    [o_dmi_play setTitle: _NS("Pause")];
    [o_vmi_play setTitle: _NS("Pause")];
}

- (void)setRepeatOne
{
    [o_mi_repeat setState: NSOnState];
    [o_mi_loop setState: NSOffState];
}

- (void)setRepeatAll
{
    [o_mi_repeat setState: NSOffState];
    [o_mi_loop setState: NSOnState];
}

- (void)setRepeatOff
{
    [o_mi_repeat setState: NSOffState];
    [o_mi_loop setState: NSOffState];
}

- (void)setShuffle
{
    bool b_value;
    playlist_t *p_playlist = pl_Get(VLCIntf);
    b_value = var_GetBool(p_playlist, "random");

    [o_mi_random setState: b_value];
}

#pragma mark -
#pragma mark Dynamic menu creation and validation

- (void)setupVarMenuItem:(NSMenuItem *)o_mi
                  target:(vlc_object_t *)p_object
                     var:(const char *)psz_variable
                selector:(SEL)pf_callback
{
    vlc_value_t val, text;
    int i_type = var_Type(p_object, psz_variable);

    switch(i_type & VLC_VAR_TYPE) {
        case VLC_VAR_VOID:
        case VLC_VAR_BOOL:
        case VLC_VAR_VARIABLE:
        case VLC_VAR_STRING:
        case VLC_VAR_INTEGER:
            break;
        default:
            /* Variable doesn't exist or isn't handled */
            return;
    }

    /* Get the descriptive name of the variable */
    var_Change(p_object, psz_variable, VLC_VAR_GETTEXT, &text, NULL);
    [o_mi setTitle: _NS(text.psz_string ? text.psz_string : psz_variable)];

    if (i_type & VLC_VAR_HASCHOICE) {
        NSMenu *o_menu = [o_mi submenu];

        [self setupVarMenu: o_menu forMenuItem: o_mi target:p_object
                       var:psz_variable selector:pf_callback];

        free(text.psz_string);
        return;
    }

    if (var_Get(p_object, psz_variable, &val) < 0) {
        return;
    }

    VLCAutoGeneratedMenuContent *o_data;
    switch(i_type & VLC_VAR_TYPE) {
        case VLC_VAR_VOID:
            o_data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                                                                      andValue: val ofType: i_type];
            [o_mi setRepresentedObject: [o_data autorelease]];
            break;

        case VLC_VAR_BOOL:
            o_data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                                                                      andValue: val ofType: i_type];
            [o_mi setRepresentedObject: [o_data autorelease]];
            if (!(i_type & VLC_VAR_ISCOMMAND))
                [o_mi setState: val.b_bool ? TRUE : FALSE ];
            break;

        default:
            break;
    }

    if ((i_type & VLC_VAR_TYPE) == VLC_VAR_STRING) free(val.psz_string);
    free(text.psz_string);
}


- (void)setupVarMenu:(NSMenu *)o_menu
         forMenuItem: (NSMenuItem *)o_parent
              target:(vlc_object_t *)p_object
                 var:(const char *)psz_variable
            selector:(SEL)pf_callback
{
    vlc_value_t val, val_list, text_list;
    int i_type, i;

    /* remove previous items */
    [o_menu removeAllItems];

    /* we disable everything here, and enable it again when needed, below */
    [o_parent setEnabled:NO];

    /* Aspect Ratio */
    if ([[o_parent title] isEqualToString: _NS("Aspect-ratio")] == YES) {
        NSMenuItem *o_lmi_tmp2;
        o_lmi_tmp2 = [o_menu addItemWithTitle: _NS("Lock Aspect Ratio") action: @selector(lockVideosAspectRatio:) keyEquivalent: @""];
        [o_lmi_tmp2 setTarget: [[VLCMain sharedInstance] controls]];
        [o_lmi_tmp2 setEnabled: YES];
        [o_lmi_tmp2 setState: [[VLCCoreInteraction sharedInstance] aspectRatioIsLocked]];
        [o_parent setEnabled: YES];
        [o_menu addItem: [NSMenuItem separatorItem]];
    }

    /* special case for the subtitles item */
    if ([[o_parent title] isEqualToString: _NS("Subtitles Track")] == YES) {
        NSMenuItem * o_lmi_tmp;
        o_lmi_tmp = [o_menu addItemWithTitle: _NS("Open File...") action: @selector(addSubtitleFile:) keyEquivalent: @""];
        [o_lmi_tmp setTarget: [[VLCMain sharedInstance] controls]];
        [o_lmi_tmp setEnabled: YES];
        [o_parent setEnabled: YES];
    }

    /* Check the type of the object variable */
    i_type = var_Type(p_object, psz_variable);

    /* Make sure we want to display the variable */
    if (i_type & VLC_VAR_HASCHOICE) {
        var_Change(p_object, psz_variable, VLC_VAR_CHOICESCOUNT, &val, NULL);
        if (val.i_int == 0)
            return;
        if ((i_type & VLC_VAR_TYPE) != VLC_VAR_VARIABLE && val.i_int == 1)
            return;
    }
    else
        return;

    switch(i_type & VLC_VAR_TYPE) {
        case VLC_VAR_VOID:
        case VLC_VAR_BOOL:
        case VLC_VAR_VARIABLE:
        case VLC_VAR_STRING:
        case VLC_VAR_INTEGER:
            break;
        default:
            /* Variable doesn't exist or isn't handled */
            return;
    }

    if (var_Get(p_object, psz_variable, &val) < 0) {
        return;
    }

    if (var_Change(p_object, psz_variable, VLC_VAR_GETLIST,
                   &val_list, &text_list) < 0) {
        if ((i_type & VLC_VAR_TYPE) == VLC_VAR_STRING) free(val.psz_string);
        return;
    }

    /* make (un)sensitive */
    [o_parent setEnabled: (val_list.p_list->i_count > 1)];

    /* another special case for the subtitles item */
    if ([[o_parent title] isEqualToString: _NS("Subtitles Track")] == YES)
        [o_menu addItem: [NSMenuItem separatorItem]];

    for (i = 0; i < val_list.p_list->i_count; i++) {
        NSMenuItem * o_lmi;
        NSString *o_title = @"";
        VLCAutoGeneratedMenuContent *o_data;

        switch(i_type & VLC_VAR_TYPE) {
            case VLC_VAR_STRING:

                o_title = _NS(text_list.p_list->p_values[i].psz_string ? text_list.p_list->p_values[i].psz_string : val_list.p_list->p_values[i].psz_string);

                o_lmi = [o_menu addItemWithTitle: o_title action: pf_callback keyEquivalent: @""];
                o_data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                                                                          andValue: val_list.p_list->p_values[i] ofType: i_type];
                [o_lmi setRepresentedObject: [o_data autorelease]];
                [o_lmi setTarget: self];

                if (!strcmp(val.psz_string, val_list.p_list->p_values[i].psz_string) && !(i_type & VLC_VAR_ISCOMMAND))
                    [o_lmi setState: TRUE ];

                break;

            case VLC_VAR_INTEGER:

                o_title = text_list.p_list->p_values[i].psz_string ?
                _NS(text_list.p_list->p_values[i].psz_string) : [NSString stringWithFormat: @"%"PRId64, val_list.p_list->p_values[i].i_int];

                o_lmi = [o_menu addItemWithTitle: o_title action: pf_callback keyEquivalent: @""];
                o_data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                                                                          andValue: val_list.p_list->p_values[i] ofType: i_type];
                [o_lmi setRepresentedObject: [o_data autorelease]];
                [o_lmi setTarget: self];

                if (val_list.p_list->p_values[i].i_int == val.i_int && !(i_type & VLC_VAR_ISCOMMAND))
                    [o_lmi setState: TRUE ];
                break;

            default:
                break;
        }
    }

    /* special case for the subtitles sub-menu
     * In case that we don't have any subs, we don't want a separator item at the end */
    if ([[o_parent title] isEqualToString: _NS("Subtitles Track")] == YES) {
        if ([o_menu numberOfItems] == 2)
            [o_menu removeItemAtIndex: 1];
    }

    /* clean up everything */
    if ((i_type & VLC_VAR_TYPE) == VLC_VAR_STRING) free(val.psz_string);
    var_FreeList(&val_list, &text_list);
}

- (IBAction)toggleVar:(id)sender
{
    NSMenuItem *o_mi = (NSMenuItem *)sender;
    VLCAutoGeneratedMenuContent *o_data = [o_mi representedObject];
    [NSThread detachNewThreadSelector: @selector(toggleVarThread:)
                             toTarget: self withObject: o_data];

    return;
}

- (int)toggleVarThread: (id)data
{
    vlc_object_t *p_object;
    NSAutoreleasePool * o_pool = [[NSAutoreleasePool alloc] init];

    assert([data isKindOfClass:[VLCAutoGeneratedMenuContent class]]);
    VLCAutoGeneratedMenuContent *menuContent = (VLCAutoGeneratedMenuContent *)data;

    /* Preserve settings across vouts via the playlist object: */
    if (!strcmp([menuContent name], "fullscreen") || !strcmp([menuContent name], "video-on-top"))
        var_Set(pl_Get(VLCIntf), [menuContent name] , [menuContent value]);

    p_object = [menuContent vlcObject];

    if (p_object != NULL) {
        var_Set(p_object, [menuContent name], [menuContent value]);
        vlc_object_release(p_object);
        [o_pool release];
        return true;
    }
    [o_pool release];
    return VLC_EGENERIC;
}

@end

@implementation VLCMainMenu (NSMenuValidation)

- (BOOL)validateMenuItem:(NSMenuItem *)o_mi
{
    NSString *o_title = [o_mi title];
    BOOL bEnabled = TRUE;
    vlc_value_t val;
    playlist_t * p_playlist = pl_Get(p_intf);
    input_thread_t * p_input = playlist_CurrentInput(p_playlist);

    if ([o_title isEqualToString: _NS("Stop")]) {
        if (p_input == NULL) {
            bEnabled = FALSE;
        }
        [self setupMenus]; /* Make sure input menu is up to date */
    } else if ([o_title isEqualToString: _NS("Previous")] ||
            [o_title isEqualToString: _NS("Next")]) {
        PL_LOCK;
        bEnabled = playlist_CurrentSize(p_playlist) > 1;
        PL_UNLOCK;
    } else if ([o_title isEqualToString: _NS("Random")]) {
        int i_state;
        var_Get(p_playlist, "random", &val);
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    } else if ([o_title isEqualToString: _NS("Repeat One")]) {
        int i_state;
        var_Get(p_playlist, "repeat", &val);
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    } else if ([o_title isEqualToString: _NS("Repeat All")]) {
        int i_state;
        var_Get(p_playlist, "loop", &val);
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    } else if ([o_title isEqualToString: _NS("Quit after Playback")]) {
        int i_state;
        var_Get(p_playlist, "play-and-exit", &val);
        i_state = val.b_bool ? NSOnState : NSOffState;
        [o_mi setState: i_state];
    } else if ([o_title isEqualToString: _NS("Step Forward")] ||
               [o_title isEqualToString: _NS("Step Backward")] ||
               [o_title isEqualToString: _NS("Jump To Time")]) {
        if (p_input != NULL) {
            var_Get(p_input, "can-seek", &val);
            bEnabled = val.b_bool;
        }
        else bEnabled = FALSE;
    } else if ([o_title isEqualToString: _NS("Mute")]) {
        [o_mi setState: [[VLCCoreInteraction sharedInstance] mute] ? NSOnState : NSOffState];
        [self setupMenus]; /* Make sure audio menu is up to date */
    } else if ([o_title isEqualToString: _NS("Half Size")] ||
               [o_title isEqualToString: _NS("Normal Size")] ||
               [o_title isEqualToString: _NS("Double Size")] ||
               [o_title isEqualToString: _NS("Fit to Screen")] ||
               [o_title isEqualToString: _NS("Snapshot")] ||
               [o_title isEqualToString: _NS("Fullscreen")] ||
               [o_title isEqualToString: _NS("Float on Top")]) {
        bEnabled = FALSE;

        if (p_input != NULL) {
            vout_thread_t *p_vout = getVoutForActiveWindow();
            if (p_vout != NULL) {
                if ([o_title isEqualToString: _NS("Float on Top")])
                    [o_mi setState: var_GetBool(p_vout, "video-on-top")];

                if ([o_title isEqualToString: _NS("Fullscreen")])
                    [o_mi setState: var_GetBool(p_vout, "fullscreen")];

                bEnabled = TRUE;
                vlc_object_release(p_vout);
            }
        }
        
        [self setupMenus]; /* Make sure video menu is up to date */
    }

    /* Special case for telx menu */
    if ([o_title isEqualToString: _NS("Normal Size")]) {
        NSMenuItem *item = [[o_mi menu] itemWithTitle:_NS("Teletext")];
        bool b_telx = p_input && var_GetInteger(p_input, "teletext-es") >= 0;

        [[item submenu] setAutoenablesItems:NO];

        for (int k=0; k < [[item submenu] numberOfItems]; k++)
            [[[item submenu] itemAtIndex:k] setEnabled: b_telx];
    }

    if (p_input)
        vlc_object_release(p_input);

    return bEnabled ;
}

@end


/*****************************************************************************
 * VLCAutoGeneratedMenuContent implementation
 *****************************************************************************
 * Object connected to a playlistitem which remembers the data belonging to
 * the variable of the autogenerated menu
 *****************************************************************************/
@implementation VLCAutoGeneratedMenuContent

-(id) initWithVariableName:(const char *)name ofObject:(vlc_object_t *)object
                  andValue:(vlc_value_t)val ofType:(int)type
{
    self = [super init];

    if (self != nil) {
        _vlc_object = vlc_object_hold(object);
        psz_name = strdup(name);
        i_type = type;
        value = val;
        if ((i_type & VLC_VAR_TYPE) == VLC_VAR_STRING)
            value.psz_string = strdup(val.psz_string);
    }

    return(self);
}

- (void)dealloc
{
    if (_vlc_object)
        vlc_object_release(_vlc_object);
    if ((i_type & VLC_VAR_TYPE) == VLC_VAR_STRING)
        free(value.psz_string);
    free(psz_name);
    [super dealloc];
}

- (const char *)name
{
    return psz_name;
}

- (vlc_value_t)value
{
    return value;
}

- (vlc_object_t *)vlcObject
{
    return vlc_object_hold(_vlc_object);
}


- (int)type
{
    return i_type;
}

@end
