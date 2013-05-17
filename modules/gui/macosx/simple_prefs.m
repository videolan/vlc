/*****************************************************************************
* simple_prefs.m: Simple Preferences for Mac OS X
*****************************************************************************
* Copyright (C) 2008-2013 VLC authors and VideoLAN
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#import "CompatibilityFixes.h"
#import "simple_prefs.h"
#import "prefs.h"
#import <vlc_keys.h>
#import <vlc_interface.h>
#import <vlc_dialog.h>
#import <vlc_modules.h>
#import <vlc_plugin.h>
#import <vlc_config_cat.h>
#import "misc.h"
#import "intf.h"
#import "AppleRemote.h"
#import "CoreInteraction.h"

#import <Sparkle/Sparkle.h>                        //for o_intf_last_update_lbl

static NSString* VLCSPrefsToolbarIdentifier = @"Our Simple Preferences Toolbar Identifier";
static NSString* VLCIntfSettingToolbarIdentifier = @"Intf Settings Item Identifier";
static NSString* VLCAudioSettingToolbarIdentifier = @"Audio Settings Item Identifier";
static NSString* VLCVideoSettingToolbarIdentifier = @"Video Settings Item Identifier";
static NSString* VLCOSDSettingToolbarIdentifier = @"Subtitles Settings Item Identifier";
static NSString* VLCInputSettingToolbarIdentifier = @"Input Settings Item Identifier";
static NSString* VLCHotkeysSettingToolbarIdentifier = @"Hotkeys Settings Item Identifier";

@implementation VLCSimplePrefs

static VLCSimplePrefs *_o_sharedInstance = nil;

#pragma mark Initialisation

+ (VLCSimplePrefs *)sharedInstance
{
    return _o_sharedInstance ? _o_sharedInstance : [[self alloc] init];
}

- (id)init
{
    if (_o_sharedInstance)
        [self dealloc];
    else {
        _o_sharedInstance = [super init];
        p_intf = VLCIntf;
    }

    return _o_sharedInstance;
}

- (void)dealloc
{
    [o_currentlyShownCategoryView release];

    [o_hotkeySettings release];
    [o_hotkeyDescriptions release];
    [o_hotkeyNames release];
    [o_hotkeysNonUseableKeys release];

    [o_keyInTransition release];

    [super dealloc];
}

- (void)awakeFromNib
{
    [self initStrings];

    /* setup the toolbar */
    NSToolbar * o_sprefs_toolbar = [[[NSToolbar alloc] initWithIdentifier: VLCSPrefsToolbarIdentifier] autorelease];
    [o_sprefs_toolbar setAllowsUserCustomization: NO];
    [o_sprefs_toolbar setAutosavesConfiguration: NO];
    [o_sprefs_toolbar setDisplayMode: NSToolbarDisplayModeIconAndLabel];
    [o_sprefs_toolbar setSizeMode: NSToolbarSizeModeRegular];
    [o_sprefs_toolbar setDelegate: self];
    [o_sprefs_win setToolbar: o_sprefs_toolbar];

    if (!OSX_SNOW_LEOPARD)
        [o_sprefs_win setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];

    [o_hotkeys_listbox setTarget:self];
    [o_hotkeys_listbox setDoubleAction:@selector(hotkeyTableDoubleClick:)];

    /* setup useful stuff */
    o_hotkeysNonUseableKeys = [@[@"Command-c", @"Command-x", @"Command-v", @"Command-a", @"Command-," , @"Command-h", @"Command-Alt-h", @"Command-Shift-o", @"Command-o", @"Command-d", @"Command-n", @"Command-s", @"Command-z", @"Command-l", @"Command-r", @"Command-3", @"Command-m", @"Command-w", @"Command-Shift-w", @"Command-Shift-c", @"Command-Shift-p", @"Command-i", @"Command-e", @"Command-Shift-e", @"Command-b", @"Command-Shift-m", @"Command-Ctrl-m", @"Command-?", @"Command-Alt-?"] retain];
}

#define CreateToolbarItem(o_name, o_desc, o_img, sel) \
    o_toolbarItem = create_toolbar_item(o_itemIdent, o_name, o_desc, o_img, self, @selector(sel));
static inline NSToolbarItem *
create_toolbar_item(NSString * o_itemIdent, NSString * o_name, NSString * o_desc, NSString * o_img, id target, SEL selector)
{
    NSToolbarItem *o_toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: o_itemIdent] autorelease]; \

    [o_toolbarItem setLabel: o_name];
    [o_toolbarItem setPaletteLabel: o_desc];

    [o_toolbarItem setToolTip: o_desc];
    [o_toolbarItem setImage: [NSImage imageNamed: o_img]];

    [o_toolbarItem setTarget: target];
    [o_toolbarItem setAction: selector];

    [o_toolbarItem setEnabled: YES];
    [o_toolbarItem setAutovalidates: YES];

    return o_toolbarItem;
}

- (NSToolbarItem *) toolbar: (NSToolbar *)o_sprefs_toolbar
      itemForItemIdentifier: (NSString *)o_itemIdent
  willBeInsertedIntoToolbar: (BOOL)b_willBeInserted
{
    NSToolbarItem *o_toolbarItem = nil;

    if ([o_itemIdent isEqual: VLCIntfSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS("Interface"), _NS("Interface Settings"), @"spref_cone_Interface_64", showInterfaceSettings);
    } else if ([o_itemIdent isEqual: VLCAudioSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS("Audio"), _NS("Audio Settings"), @"spref_cone_Audio_64", showAudioSettings);
    } else if ([o_itemIdent isEqual: VLCVideoSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS("Video"), _NS("Video Settings"), @"spref_cone_Video_64", showVideoSettings);
    } else if ([o_itemIdent isEqual: VLCOSDSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS(SUBPIC_TITLE), _NS("Subtitle & On Screen Display Settings"), @"spref_cone_Subtitles_64", showOSDSettings);
    } else if ([o_itemIdent isEqual: VLCInputSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS(INPUT_TITLE), _NS("Input & Codec Settings"), @"spref_cone_Input_64", showInputSettings);
    } else if ([o_itemIdent isEqual: VLCHotkeysSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS("Hotkeys"), _NS("Hotkeys settings"), @"spref_cone_Hotkeys_64", showHotkeySettings);
    }

    return o_toolbarItem;
}

- (NSArray *)toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar
{
    return @[VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier,
             VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, VLCHotkeysSettingToolbarIdentifier,
             NSToolbarFlexibleSpaceItemIdentifier];
}

- (NSArray *)toolbarAllowedItemIdentifiers: (NSToolbar *)toolbar
{
    return @[VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier,
             VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, VLCHotkeysSettingToolbarIdentifier,
             NSToolbarFlexibleSpaceItemIdentifier];
}

- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
    return @[VLCIntfSettingToolbarIdentifier, VLCAudioSettingToolbarIdentifier, VLCVideoSettingToolbarIdentifier,
             VLCOSDSettingToolbarIdentifier, VLCInputSettingToolbarIdentifier, VLCHotkeysSettingToolbarIdentifier];
}

- (void)initStrings
{
    /* audio */
    [o_audio_dolby_txt setStringValue: _NS("Force detection of Dolby Surround")];
    [o_audio_effects_box setTitle: _NS("Effects")];
    [o_audio_enable_ckb setTitle: _NS("Enable audio")];
    [o_audio_general_box setTitle: _NS("General Audio")];
    [o_audio_lang_txt setStringValue: _NS("Preferred Audio language")];
    [o_audio_last_ckb setTitle: _NS("Enable Last.fm submissions")];
    [o_audio_lastpwd_txt setStringValue: _NS("Password")];
    [o_audio_lastuser_txt setStringValue: _NS("User name")];
    [o_audio_spdif_ckb setTitle: _NS("Use S/PDIF when available")];
    [o_audio_visual_txt setStringValue: _NS("Visualization")];
    [o_audio_autosavevol_yes_bcell setTitle: _NS("Keep audio level between sessions")];
    [o_audio_autosavevol_no_bcell setTitle: _NS("Always reset audio start level to:")];

    /* hotkeys */
    [o_hotkeys_change_btn setTitle: _NS("Change")];
    [o_hotkeys_change_win setTitle: _NS("Change Hotkey")];
    [o_hotkeys_change_cancel_btn setTitle: _NS("Cancel")];
    [o_hotkeys_change_ok_btn setTitle: _NS("OK")];
    [o_hotkeys_clear_btn setTitle: _NS("Clear")];
    [o_hotkeys_lbl setStringValue: _NS("Select an action to change the associated hotkey:")];
    [[[o_hotkeys_listbox tableColumnWithIdentifier: @"action"] headerCell] setStringValue: _NS("Action")];
    [[[o_hotkeys_listbox tableColumnWithIdentifier: @"shortcut"] headerCell] setStringValue: _NS("Shortcut")];

    /* input */
    [o_input_record_box setTitle: _NS("Record directory or filename")];
    [o_input_record_btn setTitle: _NS("Browse...")];
    [o_input_record_btn setToolTip: _NS("Directory or filename where the records will be stored")];
    [o_input_avi_txt setStringValue: _NS("Repair AVI Files")];
    [o_input_cachelevel_txt setStringValue: _NS("Default Caching Level")];
    [o_input_caching_box setTitle: _NS("Caching")];
    [o_input_cachelevel_custom_txt setStringValue: _NS("Use the complete preferences to configure custom caching values for each access module.")];
    [o_input_mux_box setTitle: _NS("Codecs / Muxers")];
    [o_input_net_box setTitle: _NS("Network")];
    [o_input_avcodec_hw_txt setStringValue: _NS("Hardware Acceleration")];
    [o_input_postproc_txt setStringValue: _NS("Post-Processing Quality")];
    [o_input_rtsp_ckb setTitle: _NS("Use RTP over RTSP (TCP)")];
    [o_input_skipLoop_txt setStringValue: _NS("Skip the loop filter for H.264 decoding")];
    [o_input_mkv_preload_dir_ckb setTitle: _NS("Preload MKV files in the same directory")];
    [o_input_urlhandler_btn setTitle: _NS("Edit default application settings for network protocols")];

    /* url handler */
    [o_urlhandler_title_txt setStringValue: _NS("Open network streams using the following protocols")];
    [o_urlhandler_subtitle_txt setStringValue: _NS("Note that these are system-wide settings.")];
    [o_urlhandler_save_btn setTitle: _NS("Save")];
    [o_urlhandler_cancel_btn setTitle: _NS("Cancel")];

    /* interface */
    [o_intf_style_txt setStringValue: _NS("Interface style")];
    [o_intf_style_dark_bcell setTitle: _NS("Dark")];
    [o_intf_style_bright_bcell setTitle: _NS("Bright")];
    [o_intf_art_txt setStringValue: _NS("Album art download policy")];
    [o_intf_embedded_ckb setTitle: _NS("Show video within the main window")];
    [o_intf_nativefullscreen_ckb setTitle: _NS("Use the native fullscreen mode")];
    [o_intf_fspanel_ckb setTitle: _NS("Show Fullscreen Controller")];
    [o_intf_network_box setTitle: _NS("Privacy / Network Interaction")];
    [o_intf_appleremote_ckb setTitle: _NS("Control playback with the Apple Remote")];
    [o_intf_appleremote_sysvol_ckb setTitle: _NS("Control system volume with the Apple Remote")];
    [o_intf_mediakeys_ckb setTitle: _NS("Control playback with media keys")];
    [o_intf_update_ckb setTitle: _NS("Automatically check for updates")];
    [o_intf_last_update_lbl setStringValue: @""];
    [o_intf_enableGrowl_ckb setTitle: _NS("Enable Growl notifications (on playlist item change)")];
    [o_intf_autoresize_ckb setTitle: _NS("Resize interface to the native video size")];
    [o_intf_pauseminimized_ckb setTitle: _NS("Pause the video playback when minimized")];

    /* Subtitles and OSD */
    [o_osd_encoding_txt setStringValue: _NS("Default Encoding")];
    [o_osd_font_box setTitle: _NS("Display Settings")];
    [o_osd_font_btn setTitle: _NS("Choose...")];
    [o_osd_font_color_txt setStringValue: _NS("Font color")];
    [o_osd_font_size_txt setStringValue: _NS("Font size")];
    [o_osd_font_txt setStringValue: _NS("Font")];
    [o_osd_lang_box setTitle: _NS("Subtitle languages")];
    [o_osd_lang_txt setStringValue: _NS("Preferred subtitle language")];
    [o_osd_osd_box setTitle: _NS("On Screen Display")];
    [o_osd_osd_ckb setTitle: _NS("Enable OSD")];
    [o_osd_opacity_txt setStringValue: _NS("Opacity")];
    [o_osd_forcebold_ckb setTitle: _NS("Force bold")];
    [o_osd_outline_color_txt setStringValue: _NS("Outline color")];
    [o_osd_outline_thickness_txt setStringValue: _NS("Outline thickness")];

    /* video */
    [o_video_black_ckb setTitle: _NS("Black screens in Fullscreen mode")];
    [o_video_device_txt setStringValue: _NS("Fullscreen Video Device")];
    [o_video_display_box setTitle: _NS("Display")];
    [o_video_enable_ckb setTitle: _NS("Enable video")];
    [o_video_fullscreen_ckb setTitle: _NS("Fullscreen")];
    [o_video_videodeco_ckb setTitle: _NS("Window decorations")];
    [o_video_onTop_ckb setTitle: _NS("Always on top")];
    [o_video_output_txt setStringValue: _NS("Output module")];
    [o_video_skipFrames_ckb setTitle: _NS("Skip frames")];
    [o_video_snap_box setTitle: _NS("Video snapshots")];
    [o_video_snap_folder_btn setTitle: _NS("Browse...")];
    [o_video_snap_folder_txt setStringValue: _NS("Folder")];
    [o_video_snap_format_txt setStringValue: _NS("Format")];
    [o_video_snap_prefix_txt setStringValue: _NS("Prefix")];
    [o_video_snap_seqnum_ckb setTitle: _NS("Sequential numbering")];
    [o_video_deinterlace_txt setStringValue: _NS("Deinterlace")];
    [o_video_deinterlace_mode_txt setStringValue: _NS("Deinterlace mode")];
    [o_video_video_box setTitle: _NS("Video")];

    /* generic stuff */
    [o_sprefs_showAll_btn setTitle: _NS("Show All")];
    [o_sprefs_cancel_btn setTitle: _NS("Cancel")];
    [o_sprefs_reset_btn setTitle: _NS("Reset All")];
    [o_sprefs_save_btn setTitle: _NS("Save")];
    [o_sprefs_win setTitle: _NS("Preferences")];
}

/* TODO: move this part to core */
#define config_GetLabel(a,b) __config_GetLabel(VLC_OBJECT(a),b)
static inline char * __config_GetLabel(vlc_object_t *p_this, const char *psz_name)
{
    module_config_t *p_config;

    p_config = config_FindConfig(p_this, psz_name);

    /* sanity checks */
    if (!p_config) {
        msg_Err(p_this, "option %s does not exist", psz_name);
        return NULL;
    }

    if (p_config->psz_longtext)
        return p_config->psz_longtext;
    else if (p_config->psz_text)
        return p_config->psz_text;
    else
        msg_Warn(p_this, "option %s does not include any help", psz_name);

    return NULL;
}

#pragma mark -
#pragma mark Setup controls

- (void)setupButton: (NSPopUpButton *)object forStringList: (const char *)name
{
    module_config_t *p_item;

    [object removeAllItems];
    p_item = config_FindConfig(VLC_OBJECT(p_intf), name);

    /* serious problem, if no item found */
    assert(p_item);

    for (int i = 0; i < p_item->list_count; i++) {
        NSMenuItem *mi;
        if (p_item->list_text != NULL)
            mi = [[NSMenuItem alloc] initWithTitle: _NS(p_item->list_text[i]) action:NULL keyEquivalent: @""];
        else if (p_item->list.psz[i] && strcmp(p_item->list.psz[i],"") == 0) {
            [[object menu] addItem: [NSMenuItem separatorItem]];
            continue;
        }
        else if (p_item->list.psz[i])
            mi = [[NSMenuItem alloc] initWithTitle: @(p_item->list.psz[i]) action:NULL keyEquivalent: @""];
        else
            msg_Err(p_intf, "item %d of pref %s failed to be created", i, name);
        [mi setRepresentedObject:@(p_item->list.psz[i])];
        [[object menu] addItem: [mi autorelease]];
        if (p_item->value.psz && !strcmp(p_item->value.psz, p_item->list.psz[i]))
            [object selectItem:[object lastItem]];
    }
    [object setToolTip: _NS(p_item->psz_longtext)];
}

- (void)setupButton: (NSPopUpButton *)object forIntList: (const char *)name
{
    module_config_t *p_item;

    [object removeAllItems];
    p_item = config_FindConfig(VLC_OBJECT(p_intf), name);

    /* serious problem, if no item found */
    assert(p_item);

    for (int i = 0; i < p_item->list_count; i++) {
        NSMenuItem *mi;
        if (p_item->list_text != NULL)
            mi = [[NSMenuItem alloc] initWithTitle: _NS(p_item->list_text[i]) action:NULL keyEquivalent: @""];
        else if (p_item->list.i[i])
            mi = [[NSMenuItem alloc] initWithTitle: [NSString stringWithFormat: @"%d", p_item->list.i[i]] action:NULL keyEquivalent: @""];
        else
            msg_Err(p_intf, "item %d of pref %s failed to be created", i, name);
        [mi setRepresentedObject:@(p_item->list.i[i])];
        [[object menu] addItem: [mi autorelease]];
        if (p_item->value.i == p_item->list.i[i])
            [object selectItem:[object lastItem]];
    }
    [object setToolTip: _NS(p_item->psz_longtext)];
}

- (void)setupButton: (NSPopUpButton *)object forModuleList: (const char *)name
{
    module_config_t *p_item;
    module_t *p_parser, **p_list;
    int y = 0;

    [object removeAllItems];

    p_item = config_FindConfig(VLC_OBJECT(p_intf), name);
    size_t count;
    p_list = module_list_get(&count);
    if (!p_item ||!p_list) {
        if (p_list) module_list_free(p_list);
        msg_Err(p_intf, "serious problem, item or list not found");
        return;
    }

    [object addItemWithTitle: _NS("Default")];
    for (size_t i_index = 0; i_index < count; i_index++) {
        p_parser = p_list[i_index];
        if (module_provides(p_parser, p_item->psz_type)) {
            [object addItemWithTitle: @(_(module_GetLongName(p_parser)) ?: "")];
            if (p_item->value.psz && !strcmp(p_item->value.psz, module_get_name(p_parser, false)))
                [object selectItem: [object lastItem]];
        }
    }
    module_list_free(p_list);
    [object setToolTip: _NS(p_item->psz_longtext)];
}

- (void)setupButton: (NSButton *)object forBoolValue: (const char *)name
{
    [object setState: config_GetInt(p_intf, name)];
    [object setToolTip: _NS(config_GetLabel(p_intf, name))];
}

- (void)setupField:(NSTextField *)o_object forOption:(const char *)psz_option
{
    char *psz_tmp = config_GetPsz(p_intf, psz_option);
    [o_object setStringValue: @(psz_tmp ?: "")];
    [o_object setToolTip: _NS(config_GetLabel(p_intf, psz_option))];
    free(psz_tmp);
}

- (void)resetControls
{
    module_config_t *p_item;
    int i, y = 0;
    char *psz_tmp;

    /**********************
     * interface settings *
     **********************/
    [self setupButton: o_intf_art_pop forIntList: "album-art"];

    [self setupButton: o_intf_fspanel_ckb forBoolValue: "macosx-fspanel"];

    [self setupButton: o_intf_nativefullscreen_ckb forBoolValue: "macosx-nativefullscreenmode"];
    BOOL b_correct_sdk = NO;
#ifdef MAC_OS_X_VERSION_10_7
    b_correct_sdk = YES;
#endif
    if (!(b_correct_sdk && !OSX_SNOW_LEOPARD)) {
        [o_intf_nativefullscreen_ckb setState: NSOffState];
        [o_intf_nativefullscreen_ckb setEnabled: NO];
    }

    [self setupButton: o_intf_embedded_ckb forBoolValue: "embedded-video"];

    [self setupButton: o_intf_appleremote_ckb forBoolValue: "macosx-appleremote"];
    [self setupButton: o_intf_appleremote_sysvol_ckb forBoolValue: "macosx-appleremote-sysvol"];

    [self setupButton: o_intf_mediakeys_ckb forBoolValue: "macosx-mediakeys"];
    if ([[SUUpdater sharedUpdater] lastUpdateCheckDate] != NULL)
        [o_intf_last_update_lbl setStringValue: [NSString stringWithFormat: _NS("Last check on: %@"), [[[SUUpdater sharedUpdater] lastUpdateCheckDate] descriptionWithLocale: [[NSUserDefaults standardUserDefaults] dictionaryRepresentation]]]];
    else
        [o_intf_last_update_lbl setStringValue: _NS("No check was performed yet.")];
    psz_tmp = config_GetPsz(p_intf, "control");
    if (psz_tmp) {
        [o_intf_enableGrowl_ckb setState: (NSInteger)strstr(psz_tmp, "growl")];
        free(psz_tmp);
    } else
        [o_intf_enableGrowl_ckb setState: NSOffState];
    if (config_GetInt(p_intf, "macosx-interfacestyle")) {
        [o_intf_style_dark_bcell setState: YES];
        [o_intf_style_bright_bcell setState: NO];
    } else {
        [o_intf_style_dark_bcell setState: NO];
        [o_intf_style_bright_bcell setState: YES];
    }
    [self setupButton: o_intf_autoresize_ckb forBoolValue: "macosx-video-autoresize"];
    [self setupButton: o_intf_pauseminimized_ckb forBoolValue: "macosx-pause-minimized"];

    /******************
     * audio settings *
     ******************/
    [self setupButton: o_audio_enable_ckb forBoolValue: "audio"];

    if (config_GetInt(p_intf, "volume-save")) {
        [o_audio_autosavevol_yes_bcell setState: NSOnState];
        [o_audio_autosavevol_no_bcell setState: NSOffState];
        [o_audio_vol_fld setEnabled: NO];
        [o_audio_vol_sld setEnabled: NO];

        [o_audio_vol_sld setIntValue: 100];
        [o_audio_vol_fld setIntValue: 100];
    } else {
        [o_audio_autosavevol_yes_bcell setState: NSOffState];
        [o_audio_autosavevol_no_bcell setState: NSOnState];
        [o_audio_vol_fld setEnabled: YES];
        [o_audio_vol_sld setEnabled: YES];

        i = var_InheritInteger(p_intf, "auhal-volume");
        i = i * 200. / AOUT_VOLUME_MAX;
        [o_audio_vol_sld setIntValue: i];
        [o_audio_vol_fld setIntValue: i];
    }

    [self setupButton: o_audio_spdif_ckb forBoolValue: "spdif"];

    [self setupButton: o_audio_dolby_pop forIntList: "force-dolby-surround"];
    [self setupField: o_audio_lang_fld forOption: "audio-language"];

    [self setupButton: o_audio_visual_pop forModuleList: "audio-visual"];

    /* Last.FM is optional */
    if (module_exists("audioscrobbler")) {
        [self setupField: o_audio_lastuser_fld forOption:"lastfm-username"];
        [self setupField: o_audio_lastpwd_sfld forOption:"lastfm-password"];

        if (config_ExistIntf(VLC_OBJECT(p_intf), "audioscrobbler")) {
            [o_audio_last_ckb setState: NSOnState];
            [o_audio_lastuser_fld setEnabled: YES];
            [o_audio_lastpwd_sfld setEnabled: YES];
        } else {
            [o_audio_last_ckb setState: NSOffState];
            [o_audio_lastuser_fld setEnabled: NO];
            [o_audio_lastpwd_sfld setEnabled: NO];
        }
    } else
        [o_audio_last_ckb setEnabled: NO];

    /******************
     * video settings *
     ******************/
    [self setupButton: o_video_enable_ckb forBoolValue: "video"];
    [self setupButton: o_video_fullscreen_ckb forBoolValue: "fullscreen"];
    [self setupButton: o_video_onTop_ckb forBoolValue: "video-on-top"];
    [self setupButton: o_video_skipFrames_ckb forBoolValue: "skip-frames"];
    [self setupButton: o_video_black_ckb forBoolValue: "macosx-black"];
    [self setupButton: o_video_videodeco_ckb forBoolValue: "video-deco"];

    [self setupButton: o_video_output_pop forModuleList: "vout"];

    [o_video_device_pop removeAllItems];
    i = 0;
    y = [[NSScreen screens] count];
    [o_video_device_pop addItemWithTitle: _NS("Default")];
    [[o_video_device_pop lastItem] setTag: 0];
    while (i < y) {
        NSRect s_rect = [[[NSScreen screens] objectAtIndex:i] frame];
        [o_video_device_pop addItemWithTitle:
         [NSString stringWithFormat: @"%@ %i (%ix%i)", _NS("Screen"), i+1,
                   (int)s_rect.size.width, (int)s_rect.size.height]];
        [[o_video_device_pop lastItem] setTag: (int)[[[NSScreen screens] objectAtIndex:i] displayID]];
        i++;
    }
    [o_video_device_pop selectItemAtIndex: 0];
    [o_video_device_pop selectItemWithTag: config_GetInt(p_intf, "macosx-vdev")];

    [self setupField: o_video_snap_folder_fld forOption:"snapshot-path"];
    [self setupField: o_video_snap_prefix_fld forOption:"snapshot-prefix"];
    [self setupButton: o_video_snap_seqnum_ckb forBoolValue: "snapshot-sequential"];
    [self setupButton: o_video_snap_format_pop forStringList: "snapshot-format"];
    [self setupButton: o_video_deinterlace_pop forIntList: "deinterlace"];
    [self setupButton: o_video_deinterlace_mode_pop forStringList: "deinterlace-mode"];

    /***************************
     * input & codecs settings *
     ***************************/
    [self setupField: o_input_record_fld forOption:"input-record-path"];
    [o_input_postproc_fld setIntValue: config_GetInt(p_intf, "postproc-q")];
    [o_input_postproc_fld setToolTip: _NS(config_GetLabel(p_intf, "postproc-q"))];
    [self setupButton: o_input_avcodec_hw_pop forModuleList: "avcodec-hw"];

    [self setupButton: o_input_avi_pop forIntList: "avi-index"];

    [self setupButton: o_input_rtsp_ckb forBoolValue: "rtsp-tcp"];
    [self setupButton: o_input_skipLoop_pop forIntList: "avcodec-skiploopfilter"];

    [self setupButton: o_input_mkv_preload_dir_ckb forBoolValue: "mkv-preload-local-dir"];

    [o_input_cachelevel_pop removeAllItems];
    [o_input_cachelevel_pop addItemsWithTitles: @[_NS("Custom"), _NS("Lowest latency"),
     _NS("Low latency"), _NS("Normal"), _NS("High latency"), _NS("Higher latency")]];
    [[o_input_cachelevel_pop itemAtIndex: 0] setTag: 0];
    [[o_input_cachelevel_pop itemAtIndex: 1] setTag: 100];
    [[o_input_cachelevel_pop itemAtIndex: 2] setTag: 200];
    [[o_input_cachelevel_pop itemAtIndex: 3] setTag: 300];
    [[o_input_cachelevel_pop itemAtIndex: 4] setTag: 500];
    [[o_input_cachelevel_pop itemAtIndex: 5] setTag: 1000];

    #define TestCaC(name, factor) \
    b_cache_equal =  b_cache_equal && \
    (i_cache * factor == config_GetInt(p_intf, name));

    /* Select the accurate value of the PopupButton */
    bool b_cache_equal = true;
    int i_cache = config_GetInt(p_intf, "file-caching");

    TestCaC("network-caching", 10/3);
    TestCaC("disc-caching", 1);
    TestCaC("live-caching", 1);
    if (b_cache_equal) {
        [o_input_cachelevel_pop selectItemWithTag: i_cache];
        [o_input_cachelevel_custom_txt setHidden: YES];
    } else {
        [o_input_cachelevel_pop selectItemWithTitle: _NS("Custom")];
        [o_input_cachelevel_custom_txt setHidden: NO];
    }
    #undef TestCaC

    /*********************
     * subtitle settings *
     *********************/
    [self setupButton: o_osd_osd_ckb forBoolValue: "osd"];

    [self setupButton: o_osd_encoding_pop forStringList: "subsdec-encoding"];
    [self setupField: o_osd_lang_fld forOption: "sub-language" ];

    [self setupField: o_osd_font_fld forOption: "freetype-font"];
    [self setupButton: o_osd_font_color_pop forIntList: "freetype-color"];
    [self setupButton: o_osd_font_size_pop forIntList: "freetype-rel-fontsize"];
    i = config_GetInt(p_intf, "freetype-opacity") * 100.0 / 255.0 + 0.5;
    [o_osd_opacity_fld setIntValue: i];
    [o_osd_opacity_sld setIntValue: i];
    [o_osd_opacity_sld setToolTip: _NS(config_GetLabel(p_intf, "freetype-opacity"))];
    [o_osd_opacity_fld setToolTip: [o_osd_opacity_sld toolTip]];
    [self setupButton: o_osd_forcebold_ckb forBoolValue: "freetype-bold"];
    [self setupButton: o_osd_outline_color_pop forIntList: "freetype-outline-color"];
    [self setupButton: o_osd_outline_thickness_pop forIntList: "freetype-outline-thickness"];

    /********************
     * hotkeys settings *
     ********************/
    const struct hotkey *p_hotkeys = p_intf->p_libvlc->p_hotkeys;
    [o_hotkeySettings release];
    o_hotkeySettings = [[NSMutableArray alloc] init];
    NSMutableArray *o_tempArray_desc = [[NSMutableArray alloc] init];
    NSMutableArray *o_tempArray_names = [[NSMutableArray alloc] init];

    /* Get the main Module */
    module_t *p_main = module_get_main();
    assert(p_main);
    unsigned confsize;
    module_config_t *p_config;

    p_config = module_config_get (p_main, &confsize);

    for (size_t i = 0; i < confsize; i++) {
        module_config_t *p_item = p_config + i;

        if (CONFIG_ITEM(p_item->i_type) && p_item->psz_name != NULL
           && !strncmp(p_item->psz_name , "key-", 4)
           && !EMPTY_STR(p_item->psz_text)) {
            [o_tempArray_desc addObject: _NS(p_item->psz_text)];
            [o_tempArray_names addObject: @(p_item->psz_name)];
            if (p_item->value.psz)
                [o_hotkeySettings addObject: @(p_item->value.psz)];
            else
                [o_hotkeySettings addObject: [NSString string]];
        }
    }
    module_config_free (p_config);

    [o_hotkeyDescriptions release];
    o_hotkeyDescriptions = [[NSArray alloc] initWithArray: o_tempArray_desc copyItems: YES];
    [o_tempArray_desc release];
    [o_hotkeyNames release];
    o_hotkeyNames = [[NSArray alloc] initWithArray: o_tempArray_names copyItems: YES];
    [o_tempArray_names release];
    [o_hotkeys_listbox reloadData];
}

#pragma mark -
#pragma mark General actions

- (void)showSimplePrefs
{
    /* we want to show the interface settings, if no category was chosen */
    if ([[o_sprefs_win toolbar] selectedItemIdentifier] == nil) {
        [[o_sprefs_win toolbar] setSelectedItemIdentifier: VLCIntfSettingToolbarIdentifier];
        [self showInterfaceSettings];
    }

    [self resetControls];

    [o_sprefs_win center];
    [o_sprefs_win makeKeyAndOrderFront: self];
}

- (void)showSimplePrefsWithLevel:(NSInteger)i_window_level
{
    [o_sprefs_win setLevel: i_window_level];
    [self showSimplePrefs];
}

- (IBAction)buttonAction:(id)sender
{
    if (sender == o_sprefs_cancel_btn) {
        [[NSFontPanel sharedFontPanel] close];
        [o_sprefs_win orderOut: sender];
    } else if (sender == o_sprefs_save_btn) {
        [self saveChangedSettings];
        [[NSFontPanel sharedFontPanel] close];
        [o_sprefs_win orderOut: sender];
    } else if (sender == o_sprefs_showAll_btn) {
        [o_sprefs_win orderOut: self];
        [[[VLCMain sharedInstance] preferences] showPrefsWithLevel:[o_sprefs_win level]];
    } else
        msg_Warn(p_intf, "unknown buttonAction sender");
}

- (IBAction)resetPreferences:(NSControl *)sender
{
    NSBeginInformationalAlertSheet(_NS("Reset Preferences"), _NS("Cancel"),
                                   _NS("Continue"), nil, [sender window], self,
                                   @selector(sheetDidEnd: returnCode: contextInfo:), NULL, nil, @"%@",
                                   _NS("This will reset VLC media player's preferences.\n\n"
                                       "Note that VLC will restart during the process, so your current "
                                       "playlist will be emptied and eventual playback, streaming or "
                                       "transcoding activities will stop immediately.\n\n"
                                       "The Media Library will not be affected.\n\n"
                                       "Are you sure you want to continue?"));
}

- (void)sheetDidEnd:(NSWindow *)o_sheet
         returnCode:(int)i_return
        contextInfo:(void *)o_context
{
    if (i_return == NSAlertAlternateReturn) {
        /* reset VLC's config */
        config_ResetAll(p_intf);
        [self resetControls];

        /* force config file creation, since libvlc won't exit normally */
        config_SaveConfigFile(p_intf);

        /* reset OS X defaults */
        [NSUserDefaults resetStandardUserDefaults];
        [[NSUserDefaults standardUserDefaults] synchronize];

        /* Relaunch now */
        const char * path = [[[NSBundle mainBundle] executablePath] UTF8String];

        /* For some reason we need to fork(), not just execl(), which reports a ENOTSUP then. */
        if (fork() != 0) {
            exit(0);
            return;
        }
        execl(path, path, NULL);
    }
}

static inline void save_int_list(intf_thread_t * p_intf, id object, const char * name)
{
    NSNumber *p_valueobject;
    module_config_t *p_item;
    p_item = config_FindConfig(VLC_OBJECT(p_intf), name);
    p_valueobject = (NSNumber *)[[object selectedItem] representedObject];
    assert([p_valueobject isKindOfClass:[NSNumber class]]);
    if (p_valueobject) config_PutInt(p_intf, name, [p_valueobject intValue]);
}

static inline void save_string_list(intf_thread_t * p_intf, id object, const char * name)
{
    NSString *p_stringobject;
    module_config_t *p_item;
    p_item = config_FindConfig(VLC_OBJECT(p_intf), name);
    p_stringobject = (NSString *)[[object selectedItem] representedObject];
    assert([p_stringobject isKindOfClass:[NSString class]]);
    if (p_stringobject) {
        config_PutPsz(p_intf, name, [p_stringobject UTF8String]);
    }
}

static inline void save_module_list(intf_thread_t * p_intf, id object, const char * name)
{
    module_config_t *p_item;
    module_t *p_parser, **p_list;
    NSString * objectTitle = [[object selectedItem] title];

    p_item = config_FindConfig(VLC_OBJECT(p_intf), name);

    size_t count;
    p_list = module_list_get(&count);
    for (size_t i_module_index = 0; i_module_index < count; i_module_index++) {
        p_parser = p_list[i_module_index];

        if (p_item->i_type == CONFIG_ITEM_MODULE && module_provides(p_parser, p_item->psz_type)) {
            if ([objectTitle isEqualToString: _NS(module_GetLongName(p_parser))]) {
                config_PutPsz(p_intf, name, strdup(module_get_name(p_parser, false)));
                break;
            }
        }
    }
    module_list_free(p_list);
    if ([objectTitle isEqualToString: _NS("Default")]) {
        if (!strcmp(name, "vout"))
            config_PutPsz(p_intf, name, "");
        else
            config_PutPsz(p_intf, name, "none");
    }
}

- (void)saveChangedSettings
{
    NSString *tmpString;
    NSRange tmpRange;

#define SaveIntList(object, name) save_int_list(p_intf, object, name)

#define SaveStringList(object, name) save_string_list(p_intf, object, name)

#define SaveModuleList(object, name) save_module_list(p_intf, object, name)

#define getString(name) [NSString stringWithFormat:@"%s", config_GetPsz(p_intf, name)]

    /**********************
     * interface settings *
     **********************/
    if (b_intfSettingChanged) {
        SaveIntList(o_intf_art_pop, "album-art");

        config_PutInt(p_intf, "macosx-fspanel", [o_intf_fspanel_ckb state]);
        config_PutInt(p_intf, "embedded-video", [o_intf_embedded_ckb state]);

        config_PutInt(p_intf, "macosx-appleremote", [o_intf_appleremote_ckb state]);
        config_PutInt(p_intf, "macosx-appleremote-sysvol", [o_intf_appleremote_sysvol_ckb state]);
        config_PutInt(p_intf, "macosx-mediakeys", [o_intf_mediakeys_ckb state]);
        config_PutInt(p_intf, "macosx-interfacestyle", [o_intf_style_dark_bcell state]);
        config_PutInt(p_intf, "macosx-nativefullscreenmode", [o_intf_nativefullscreen_ckb state]);
        config_PutInt(p_intf, "macosx-pause-minimized", [o_intf_pauseminimized_ckb state]);
        config_PutInt(p_intf, "macosx-video-autoresize", [o_intf_autoresize_ckb state]);
        if ([o_intf_enableGrowl_ckb state] == NSOnState) {
            tmpString = getString("control");
            tmpRange = [tmpString rangeOfString:@"growl"];
            if ([tmpString length] > 0 && tmpRange.location == NSNotFound)
            {
                tmpString = [tmpString stringByAppendingString: @":growl"];
                config_PutPsz(p_intf, "control", [tmpString UTF8String]);
            }
            else
                config_PutPsz(p_intf, "control", "growl");
        } else {
            tmpString = getString("control");
            if (! [tmpString isEqualToString:@""])
            {
                tmpString = [tmpString stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@":growl"]];
                tmpString = [tmpString stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@"growl:"]];
                tmpString = [tmpString stringByTrimmingCharactersInSet: [NSCharacterSet characterSetWithCharactersInString:@"growl"]];
                config_PutPsz(p_intf, "control", [tmpString UTF8String]);
            }
        }

        /* activate stuff without restart */
        if ([o_intf_appleremote_ckb state] == YES)
            [[[VLCMain sharedInstance] appleRemoteController] startListening: [VLCMain sharedInstance]];
        else
            [[[VLCMain sharedInstance] appleRemoteController] stopListening: [VLCMain sharedInstance]];
        b_intfSettingChanged = NO;
    }

    /******************
     * audio settings *
     ******************/
    if (b_audioSettingChanged) {
        config_PutInt(p_intf, "audio", [o_audio_enable_ckb state]);
        config_PutInt(p_intf, "volume-save", [o_audio_autosavevol_yes_bcell state]);
        var_SetBool(p_intf, "volume-save", [o_audio_autosavevol_yes_bcell state]);
        config_PutInt(p_intf, "spdif", [o_audio_spdif_ckb state]);
        if ([o_audio_vol_fld isEnabled])
            config_PutInt(p_intf, "auhal-volume", ([o_audio_vol_fld intValue] * AOUT_VOLUME_MAX) / 200);

        SaveIntList(o_audio_dolby_pop, "force-dolby-surround");

        config_PutPsz(p_intf, "audio-language", [[o_audio_lang_fld stringValue] UTF8String]);

        SaveModuleList(o_audio_visual_pop, "audio-visual");

        /* Last.FM is optional */
        if (module_exists("audioscrobbler")) {
            [o_audio_last_ckb setEnabled: YES];
            if ([o_audio_last_ckb state] == NSOnState)
                config_AddIntf(p_intf, "audioscrobbler");
            else
                config_RemoveIntf(p_intf, "audioscrobbler");

            config_PutPsz(p_intf, "lastfm-username", [[o_audio_lastuser_fld stringValue] UTF8String]);
            config_PutPsz(p_intf, "lastfm-password", [[o_audio_lastpwd_sfld stringValue] UTF8String]);
        }
        else
            [o_audio_last_ckb setEnabled: NO];
        b_audioSettingChanged = NO;
    }

    /******************
     * video settings *
     ******************/
    if (b_videoSettingChanged) {
        config_PutInt(p_intf, "video", [o_video_enable_ckb state]);
        config_PutInt(p_intf, "fullscreen", [o_video_fullscreen_ckb state]);
        config_PutInt(p_intf, "video-deco", [o_video_videodeco_ckb state]);
        config_PutInt(p_intf, "video-on-top", [o_video_onTop_ckb state]);
        config_PutInt(p_intf, "skip-frames", [o_video_skipFrames_ckb state]);
        config_PutInt(p_intf, "macosx-black", [o_video_black_ckb state]);

        SaveModuleList(o_video_output_pop, "vout");
        config_PutInt(p_intf, "macosx-vdev", [[o_video_device_pop selectedItem] tag]);

        config_PutPsz(p_intf, "snapshot-path", [[o_video_snap_folder_fld stringValue] UTF8String]);
        config_PutPsz(p_intf, "snapshot-prefix", [[o_video_snap_prefix_fld stringValue] UTF8String]);
        config_PutInt(p_intf, "snapshot-sequential", [o_video_snap_seqnum_ckb state]);
        SaveStringList(o_video_snap_format_pop, "snapshot-format");
        SaveIntList(o_video_deinterlace_pop, "deinterlace");
        SaveStringList(o_video_deinterlace_mode_pop, "deinterlace-mode");
        b_videoSettingChanged = NO;
    }

    /***************************
     * input & codecs settings *
     ***************************/
    if (b_inputSettingChanged) {
        config_PutPsz(p_intf, "input-record-path", [[o_input_record_fld stringValue] UTF8String]);
        config_PutInt(p_intf, "postproc-q", [o_input_postproc_fld intValue]);

        SaveIntList(o_input_avi_pop, "avi-index");

        config_PutInt(p_intf, "rtsp-tcp", [o_input_rtsp_ckb state]);
        SaveModuleList(o_input_avcodec_hw_pop, "avcodec-hw");
        SaveIntList(o_input_skipLoop_pop, "avcodec-skiploopfilter");

        config_PutInt(p_intf, "mkv-preload-local-dir", [o_input_mkv_preload_dir_ckb state]);

        #define CaC(name, factor) config_PutInt(p_intf, name, [[o_input_cachelevel_pop selectedItem] tag] * factor)
        if ([[o_input_cachelevel_pop selectedItem] tag] == 0) {
            msg_Dbg(p_intf, "Custom chosen, not adjusting cache values");
        } else {
            msg_Dbg(p_intf, "Adjusting all cache values to: %i", (int)[[o_input_cachelevel_pop selectedItem] tag]);
            CaC("file-caching", 1);
            CaC("network-caching", 10/3);
            CaC("disc-caching", 1);
            CaC("live-caching", 1);
        }
        #undef CaC
        b_inputSettingChanged = NO;
    }

    /**********************
     * subtitles settings *
     **********************/
    if (b_osdSettingChanged) {
        config_PutInt(p_intf, "osd", [o_osd_osd_ckb state]);

        if ([o_osd_encoding_pop indexOfSelectedItem] >= 0)
            SaveStringList(o_osd_encoding_pop, "subsdec-encoding");
        else
            config_PutPsz(p_intf, "subsdec-encoding", "");

        config_PutPsz(p_intf, "sub-language", [[o_osd_lang_fld stringValue] UTF8String]);

        config_PutPsz(p_intf, "freetype-font", [[o_osd_font_fld stringValue] UTF8String]);
        SaveIntList(o_osd_font_color_pop, "freetype-color");
        SaveIntList(o_osd_font_size_pop, "freetype-rel-fontsize");
        config_PutInt(p_intf, "freetype-opacity", [o_osd_opacity_fld intValue] * 255.0 / 100.0 + 0.5);
        config_PutInt(p_intf, "freetype-bold", [o_osd_forcebold_ckb state]);
        SaveIntList(o_osd_outline_color_pop, "freetype-outline-color");
        SaveIntList(o_osd_outline_thickness_pop, "freetype-outline-thickness");
        b_osdSettingChanged = NO;
    }

    /********************
     * hotkeys settings *
     ********************/
    if (b_hotkeyChanged) {
        NSUInteger hotKeyCount = [o_hotkeySettings count];
        for (NSUInteger i = 0; i < hotKeyCount; i++)
            config_PutPsz(p_intf, [[o_hotkeyNames objectAtIndex:i] UTF8String], [[o_hotkeySettings objectAtIndex:i]UTF8String]);
        b_hotkeyChanged = NO;
    }

    [[VLCCoreInteraction sharedInstance] fixPreferences];

    /* okay, let's save our changes to vlcrc */
    config_SaveConfigFile(p_intf);

    [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCMediaKeySupportSettingChanged"
                                                            object: nil
                                                          userInfo: nil];
}

- (void)showSettingsForCategory: (id)o_new_category_view
{
    NSRect o_win_rect, o_view_rect, o_old_view_rect;
    o_win_rect = [o_sprefs_win frame];
    o_view_rect = [o_new_category_view frame];

    if (o_currentlyShownCategoryView != nil) {
        /* restore our window's height, if we've shown another category previously */
        o_old_view_rect = [o_currentlyShownCategoryView frame];
        o_win_rect.size.height = o_win_rect.size.height - o_old_view_rect.size.height;
        o_win_rect.origin.y = (o_win_rect.origin.y + o_old_view_rect.size.height) - o_view_rect.size.height;
    }

    o_win_rect.size.height = o_win_rect.size.height + o_view_rect.size.height;

    [o_new_category_view setFrame: NSMakeRect(0,
                                               [o_sprefs_controls_box frame].size.height,
                                               o_view_rect.size.width,
                                               o_view_rect.size.height)];
    [o_new_category_view setAutoresizesSubviews: YES];
    if (o_currentlyShownCategoryView) {
        [[[o_sprefs_win contentView] animator] replaceSubview: o_currentlyShownCategoryView with: o_new_category_view];
        [o_currentlyShownCategoryView release];
        [[o_sprefs_win animator] setFrame: o_win_rect display:YES];
    } else {
        [[o_sprefs_win contentView] addSubview: o_new_category_view];
        [o_sprefs_win setFrame: o_win_rect display:YES animate:NO];
    }

    /* keep our current category for further reference */
    o_currentlyShownCategoryView = o_new_category_view;
    [o_currentlyShownCategoryView retain];
}

#pragma mark -
#pragma mark Specific actions

- (IBAction)interfaceSettingChanged:(id)sender
{
    b_intfSettingChanged = YES;
}

- (void)showInterfaceSettings
{
    [self showSettingsForCategory: o_intf_view];
}

- (IBAction)audioSettingChanged:(id)sender
{
    if (sender == o_audio_vol_sld)
        [o_audio_vol_fld setIntValue: [o_audio_vol_sld intValue]];

    if (sender == o_audio_vol_fld)
        [o_audio_vol_sld setIntValue: [o_audio_vol_fld intValue]];

    if (sender == o_audio_last_ckb) {
        if ([o_audio_last_ckb state] == NSOnState) {
            [o_audio_lastpwd_sfld setEnabled: YES];
            [o_audio_lastuser_fld setEnabled: YES];
        } else {
            [o_audio_lastpwd_sfld setEnabled: NO];
            [o_audio_lastuser_fld setEnabled: NO];
        }
    }

    if (sender == o_audio_autosavevol_matrix) {
        BOOL enableVolumeSlider = [o_audio_autosavevol_matrix selectedTag] == 1;
        [o_audio_vol_fld setEnabled: enableVolumeSlider];
        [o_audio_vol_sld setEnabled: enableVolumeSlider];
    }

    b_audioSettingChanged = YES;
}

- (void)showAudioSettings
{
    [self showSettingsForCategory: o_audio_view];
}

- (IBAction)videoSettingChanged:(id)sender
{
    if (sender == o_video_snap_folder_btn) {
        o_selectFolderPanel = [[NSOpenPanel alloc] init];
        [o_selectFolderPanel setCanChooseDirectories: YES];
        [o_selectFolderPanel setCanChooseFiles: NO];
        [o_selectFolderPanel setResolvesAliases: YES];
        [o_selectFolderPanel setAllowsMultipleSelection: NO];
        [o_selectFolderPanel setMessage: _NS("Choose the folder to save your video snapshots to.")];
        [o_selectFolderPanel setCanCreateDirectories: YES];
        [o_selectFolderPanel setPrompt: _NS("Choose")];
        [o_selectFolderPanel beginSheetModalForWindow: o_sprefs_win completionHandler: ^(NSInteger returnCode) {
            if (returnCode == NSOKButton)
            {
                [o_video_snap_folder_fld setStringValue: [[o_selectFolderPanel URL] path]];
                b_videoSettingChanged = YES;
            }
        }];
        [o_selectFolderPanel release];
    } else
        b_videoSettingChanged = YES;
}

- (void)showVideoSettings
{
    [self showSettingsForCategory: o_video_view];
}

- (IBAction)osdSettingChanged:(id)sender
{
    if (sender == o_osd_opacity_fld)
        [o_osd_opacity_sld setIntValue: [o_osd_opacity_fld intValue]];

    if (sender == o_osd_opacity_sld)
        [o_osd_opacity_fld setIntValue: [o_osd_opacity_sld intValue]];

    b_osdSettingChanged = YES;
}

- (void)showOSDSettings
{
    [self showSettingsForCategory: o_osd_view];
}

- (void)controlTextDidChange:(NSNotification *)o_notification
{
    id notificationObject = [o_notification object];
    if (notificationObject == o_audio_lang_fld ||
       notificationObject ==  o_audio_lastpwd_sfld ||
       notificationObject ==  o_audio_lastuser_fld ||
       notificationObject == o_audio_vol_fld)
        b_audioSettingChanged = YES;
    else if (notificationObject == o_input_record_fld ||
            notificationObject == o_input_postproc_fld)
        b_inputSettingChanged = YES;
    else if (notificationObject == o_osd_font_fld ||
            notificationObject == o_osd_lang_fld ||
            notificationObject == o_osd_opacity_fld)
        b_osdSettingChanged = YES;
    else if (notificationObject == o_video_snap_folder_fld ||
            notificationObject == o_video_snap_prefix_fld)
        b_videoSettingChanged = YES;
}

- (IBAction)showFontPicker:(id)sender
{
    char * font = config_GetPsz(p_intf, "freetype-font");
    NSString * fontName = font ? @(font) : nil;
    free(font);
    if (fontName) {
        NSFont * font = [NSFont fontWithName:fontName size:0.0];
        [[NSFontManager sharedFontManager] setSelectedFont:font isMultiple:NO];
    }
    [[NSFontManager sharedFontManager] setTarget: self];
    [[NSFontPanel sharedFontPanel] orderFront:self];
}

- (void)changeFont:(id)sender
{
    NSFont * font = [sender convertFont:[[NSFontManager sharedFontManager] selectedFont]];
    [o_osd_font_fld setStringValue:[font fontName]];
    [self osdSettingChanged:self];
}

- (IBAction)inputSettingChanged:(id)sender
{
    if (sender == o_input_cachelevel_pop) {
        if ([[[o_input_cachelevel_pop selectedItem] title] isEqualToString: _NS("Custom")])
            [o_input_cachelevel_custom_txt setHidden: NO];
        else
            [o_input_cachelevel_custom_txt setHidden: YES];
    } else if (sender == o_input_record_btn) {
        o_selectFolderPanel = [[NSOpenPanel alloc] init];
        [o_selectFolderPanel setCanChooseDirectories: YES];
        [o_selectFolderPanel setCanChooseFiles: YES];
        [o_selectFolderPanel setResolvesAliases: YES];
        [o_selectFolderPanel setAllowsMultipleSelection: NO];
        [o_selectFolderPanel setMessage: _NS("Choose the directory or filename where the records will be stored.")];
        [o_selectFolderPanel setCanCreateDirectories: YES];
        [o_selectFolderPanel setPrompt: _NS("Choose")];
        [o_selectFolderPanel beginSheetModalForWindow: o_sprefs_win completionHandler: ^(NSInteger returnCode) {
            if (returnCode == NSOKButton)
            {
                [o_input_record_fld setStringValue: [[o_selectFolderPanel URL] path]];
                b_inputSettingChanged = YES;
            }
        }];
        [o_selectFolderPanel release];

        return;
    }

    b_inputSettingChanged = YES;
}

- (void)showInputSettings
{
    [self showSettingsForCategory: o_input_view];
}

- (NSString *)bundleIdentifierForApplicationName:(NSString *)appName
{
    NSWorkspace * workspace = [NSWorkspace sharedWorkspace];
    NSString * appPath = [workspace fullPathForApplication:appName];
    if (appPath) {
        NSBundle * appBundle = [NSBundle bundleWithPath:appPath];
        return [appBundle bundleIdentifier];
    }
    return nil;
}

- (NSString *)applicationNameForBundleIdentifier:(NSString *)bundleIdentifier
{
    return [[[NSFileManager defaultManager] displayNameAtPath:[[NSWorkspace sharedWorkspace] absolutePathForAppBundleWithIdentifier:bundleIdentifier]] stringByDeletingPathExtension];
}

- (NSImage *)iconForBundleIdentifier:(NSString *)bundleIdentifier
{
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSSize iconSize = NSMakeSize(16., 16.);
    NSImage *icon = [workspace iconForFile:[workspace absolutePathForAppBundleWithIdentifier:bundleIdentifier]];
    [icon setSize:iconSize];
    return icon;
}

- (IBAction)urlHandlerAction:(id)sender
{
    NSString *bundleIdentifier = [[NSBundle mainBundle] bundleIdentifier];

    if (sender == o_input_urlhandler_btn) {
        NSArray *handlers;
        NSString *handler;
        NSString *rawhandler;
        NSMutableArray *rawHandlers;
        NSUInteger count;

#define fillUrlHandlerPopup( protocol, object ) \
        handlers = (NSArray *)LSCopyAllHandlersForURLScheme(CFSTR( protocol )); \
        rawHandlers = [[NSMutableArray alloc] init]; \
        [object removeAllItems]; \
        count = [handlers count]; \
        for (NSUInteger x = 0; x < count; x++) { \
            rawhandler = [handlers objectAtIndex:x]; \
            handler = [self applicationNameForBundleIdentifier:rawhandler]; \
            if (handler && ![handler isEqualToString:@""]) { \
                [object addItemWithTitle:handler]; \
                [[object lastItem] setImage: [self iconForBundleIdentifier:[handlers objectAtIndex:x]]]; \
                [rawHandlers addObject: rawhandler]; \
            } \
        } \
        [object selectItemAtIndex: [rawHandlers indexOfObject:(id)LSCopyDefaultHandlerForURLScheme(CFSTR( protocol ))]]; \
        [rawHandlers release]

        fillUrlHandlerPopup( "ftp", o_urlhandler_ftp_pop);
        fillUrlHandlerPopup( "mms", o_urlhandler_mms_pop);
        fillUrlHandlerPopup( "rtmp", o_urlhandler_rtmp_pop);
        fillUrlHandlerPopup( "rtp", o_urlhandler_rtp_pop);
        fillUrlHandlerPopup( "rtsp", o_urlhandler_rtsp_pop);
        fillUrlHandlerPopup( "sftp", o_urlhandler_sftp_pop);
        fillUrlHandlerPopup( "smb", o_urlhandler_smb_pop);
        fillUrlHandlerPopup( "udp", o_urlhandler_udp_pop);

#undef fillUrlHandlerPopup

        [NSApp beginSheet:o_urlhandler_win modalForWindow:o_sprefs_win modalDelegate:self didEndSelector:NULL contextInfo:nil];
    } else {
        [o_urlhandler_win orderOut:sender];
        [NSApp endSheet: o_urlhandler_win];

        if (sender == o_urlhandler_save_btn) {
            LSSetDefaultHandlerForURLScheme(CFSTR("ftp"), (CFStringRef)[self bundleIdentifierForApplicationName:[[o_urlhandler_ftp_pop selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("mms"), (CFStringRef)[self bundleIdentifierForApplicationName:[[o_urlhandler_mms_pop selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("mmsh"), (CFStringRef)[self bundleIdentifierForApplicationName:[[o_urlhandler_mms_pop selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("rtmp"), (CFStringRef)[self bundleIdentifierForApplicationName:[[o_urlhandler_rtmp_pop selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("rtp"), (CFStringRef)[self bundleIdentifierForApplicationName:[[o_urlhandler_rtp_pop selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("rtsp"), (CFStringRef)[self bundleIdentifierForApplicationName:[[o_urlhandler_rtsp_pop selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("sftp"), (CFStringRef)[self bundleIdentifierForApplicationName:[[o_urlhandler_sftp_pop selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("smb"), (CFStringRef)[self bundleIdentifierForApplicationName:[[o_urlhandler_smb_pop selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("udp"), (CFStringRef)[self bundleIdentifierForApplicationName:[[o_urlhandler_udp_pop selectedItem] title]]);
        }
    }
}

#pragma mark -
#pragma mark Hotkey actions

- (void)hotkeyTableDoubleClick:(id)object
{
    // -1 is header
    if ([o_hotkeys_listbox clickedRow] >= 0)
        [self hotkeySettingChanged:o_hotkeys_listbox];
}

- (IBAction)hotkeySettingChanged:(id)sender
{
    if (sender == o_hotkeys_change_btn || sender == o_hotkeys_listbox) {
        [o_hotkeys_change_lbl setStringValue: [NSString stringWithFormat: _NS("Press new keys for\n\"%@\""),
                                               [o_hotkeyDescriptions objectAtIndex:[o_hotkeys_listbox selectedRow]]]];
        [o_hotkeys_change_keys_lbl setStringValue: [[VLCStringUtility sharedInstance] OSXStringKeyToString:[o_hotkeySettings objectAtIndex:[o_hotkeys_listbox selectedRow]]]];
        [o_hotkeys_change_taken_lbl setStringValue: @""];
        [o_hotkeys_change_win setInitialFirstResponder: [o_hotkeys_change_win contentView]];
        [o_hotkeys_change_win makeFirstResponder: [o_hotkeys_change_win contentView]];
        [NSApp runModalForWindow: o_hotkeys_change_win];
    } else if (sender == o_hotkeys_change_cancel_btn) {
        [NSApp stopModal];
        [o_hotkeys_change_win close];
    } else if (sender == o_hotkeys_change_ok_btn) {
        NSInteger i_returnValue;
        if (! o_keyInTransition) {
            [NSApp stopModal];
            [o_hotkeys_change_win close];
            msg_Err(p_intf, "internal error prevented the hotkey switch");
            return;
        }

        b_hotkeyChanged = YES;

        i_returnValue = [o_hotkeySettings indexOfObject: o_keyInTransition];
        if (i_returnValue != NSNotFound)
            [o_hotkeySettings replaceObjectAtIndex: i_returnValue withObject: [NSString string]];
        NSString *tempString;
        tempString = [o_keyInTransition stringByReplacingOccurrencesOfString:@"-" withString:@"+"];
        i_returnValue = [o_hotkeySettings indexOfObject: tempString];
        if (i_returnValue != NSNotFound)
            [o_hotkeySettings replaceObjectAtIndex: i_returnValue withObject: [NSString string]];

        [o_hotkeySettings replaceObjectAtIndex: [o_hotkeys_listbox selectedRow] withObject: [o_keyInTransition retain]];

        [NSApp stopModal];
        [o_hotkeys_change_win close];

        [o_hotkeys_listbox reloadData];
    } else if (sender == o_hotkeys_clear_btn) {
        [o_hotkeySettings replaceObjectAtIndex: [o_hotkeys_listbox selectedRow] withObject: [NSString string]];
        [o_hotkeys_listbox reloadData];
        b_hotkeyChanged = YES;
    }

    [[NSNotificationCenter defaultCenter] postNotificationName: @"VLCMediaKeySupportSettingChanged"
                                                        object: nil
                                                      userInfo: nil];
}

- (void)showHotkeySettings
{
    [self showSettingsForCategory: o_hotkeys_view];
}

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [o_hotkeySettings count];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
    NSString * identifier = [aTableColumn identifier];

    if ([identifier isEqualToString: @"action"])
        return [o_hotkeyDescriptions objectAtIndex:rowIndex];
    else if ([identifier isEqualToString: @"shortcut"])
        return [[VLCStringUtility sharedInstance] OSXStringKeyToString:[o_hotkeySettings objectAtIndex:rowIndex]];
    else {
        msg_Err(p_intf, "unknown TableColumn identifier (%s)!", [identifier UTF8String]);
        return NULL;
    }
}

- (BOOL)changeHotkeyTo: (NSString *)theKey
{
    NSInteger i_returnValue, i_returnValue2;
    i_returnValue = [o_hotkeysNonUseableKeys indexOfObject: theKey];

    if (i_returnValue != NSNotFound || [theKey isEqualToString:@""]) {
        [o_hotkeys_change_keys_lbl setStringValue: _NS("Invalid combination")];
        [o_hotkeys_change_taken_lbl setStringValue: _NS("Regrettably, these keys cannot be assigned as hotkey shortcuts.")];
        [o_hotkeys_change_ok_btn setEnabled: NO];
        return NO;
    } else {
        [o_hotkeys_change_keys_lbl setStringValue: [[VLCStringUtility sharedInstance] OSXStringKeyToString:theKey]];

        i_returnValue = [o_hotkeySettings indexOfObject: theKey];
        i_returnValue2 = [o_hotkeySettings indexOfObject: [theKey stringByReplacingOccurrencesOfString:@"-" withString:@"+"]];
        if (i_returnValue != NSNotFound)
            [o_hotkeys_change_taken_lbl setStringValue: [NSString stringWithFormat:
                                                         _NS("This combination is already taken by \"%@\"."),
                                                         [o_hotkeyDescriptions objectAtIndex:i_returnValue]]];
        else if (i_returnValue2 != NSNotFound)
            [o_hotkeys_change_taken_lbl setStringValue: [NSString stringWithFormat:
                                                         _NS("This combination is already taken by \"%@\"."),
                                                         [o_hotkeyDescriptions objectAtIndex:i_returnValue2]]];
        else
            [o_hotkeys_change_taken_lbl setStringValue: @""];

        [o_hotkeys_change_ok_btn setEnabled: YES];
        [o_keyInTransition release];
        o_keyInTransition = theKey;
        [o_keyInTransition retain];
        return YES;
    }
}

@end

/********************
 * hotkeys settings *
 ********************/

@implementation VLCHotkeyChangeWindow

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    return YES;
}

- (BOOL)resignFirstResponder
{
    /* We need to stay the first responder or we'll miss the user's input */
    return NO;
}

- (BOOL)performKeyEquivalent:(NSEvent *)o_theEvent
{
    NSMutableString *tempString = [[[NSMutableString alloc] init] autorelease];
    NSString *keyString = [o_theEvent characters];

    unichar key = [keyString characterAtIndex:0];
    NSUInteger i_modifiers = [o_theEvent modifierFlags];

    /* modifiers */
    if (i_modifiers & NSControlKeyMask)
        [tempString appendString:@"Ctrl-"];
    if (i_modifiers & NSAlternateKeyMask )
        [tempString appendString:@"Alt-"];
    if (i_modifiers & NSShiftKeyMask)
        [tempString appendString:@"Shift-"];
    if (i_modifiers & NSCommandKeyMask)
        [tempString appendString:@"Command-"];

    /* non character keys */
    if (key == NSUpArrowFunctionKey)
        [tempString appendString:@"Up"];
    else if (key == NSDownArrowFunctionKey)
        [tempString appendString:@"Down"];
    else if (key == NSLeftArrowFunctionKey)
        [tempString appendString:@"Left"];
    else if (key == NSRightArrowFunctionKey)
        [tempString appendString:@"Right"];
    else if (key == NSF1FunctionKey)
        [tempString appendString:@"F1"];
    else if (key == NSF2FunctionKey)
        [tempString appendString:@"F2"];
    else if (key == NSF3FunctionKey)
        [tempString appendString:@"F3"];
    else if (key == NSF4FunctionKey)
        [tempString appendString:@"F4"];
    else if (key == NSF5FunctionKey)
        [tempString appendString:@"F5"];
    else if (key == NSF6FunctionKey)
        [tempString appendString:@"F6"];
    else if (key == NSF7FunctionKey)
        [tempString appendString:@"F7"];
    else if (key == NSF8FunctionKey)
        [tempString appendString:@"F8"];
    else if (key == NSF9FunctionKey)
        [tempString appendString:@"F9"];
    else if (key == NSF10FunctionKey)
        [tempString appendString:@"F10"];
    else if (key == NSF11FunctionKey)
        [tempString appendString:@"F11"];
    else if (key == NSF12FunctionKey)
        [tempString appendString:@"F12"];
    else if (key == NSInsertFunctionKey)
        [tempString appendString:@"Insert"];
    else if (key == NSHomeFunctionKey)
        [tempString appendString:@"Home"];
    else if (key == NSEndFunctionKey)
        [tempString appendString:@"End"];
    else if (key == NSPageUpFunctionKey)
        [tempString appendString:@"Pageup"];
    else if (key == NSPageDownFunctionKey)
        [tempString appendString:@"Pagedown"];
    else if (key == NSMenuFunctionKey)
        [tempString appendString:@"Menu"];
    else if (key == NSTabCharacter)
        [tempString appendString:@"Tab"];
    else if (key == NSCarriageReturnCharacter)
        [tempString appendString:@"Enter"];
    else if (key == NSEnterCharacter)
        [tempString appendString:@"Enter"];
    else if (key == NSDeleteCharacter)
        [tempString appendString:@"Delete"];
    else if (key == NSBackspaceCharacter)
        [tempString appendString:@"Backspace"];
    else if (key == 0x001B)
        [tempString appendString:@"Esc"];
    else if (key == ' ')
        [tempString appendString:@"Space"];
    else if (![[[o_theEvent charactersIgnoringModifiers] lowercaseString] isEqualToString:@""]) //plain characters
        [tempString appendString:[[o_theEvent charactersIgnoringModifiers] lowercaseString]];
    else
        return NO;

    return [[[VLCMain sharedInstance] simplePreferences] changeHotkeyTo: tempString];
}

@end

@implementation VLCSimplePrefsWindow

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)changeFont:(id)sender
{
    [[[VLCMain sharedInstance] simplePreferences] changeFont: sender];
}
@end
