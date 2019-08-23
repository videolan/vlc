/*****************************************************************************
 * VLCSimplePrefsController.m: Simple Preferences for Mac OS X
 *****************************************************************************
 * Copyright (C) 2008-2019 VLC authors and VideoLAN
 *
 * Authors: Felix Paul Kühne <fkuehne # videolan dot org>
 *          David Fuhrmann <dfuhrmann # videolan dot org>
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

#import "VLCSimplePrefsController.h"

#ifdef HAVE_SPARKLE
#import <Sparkle/Sparkle.h>                        //for o_intf_last_updateLabel
#endif

#import <vlc_actions.h>
#import <vlc_interface.h>
#import <vlc_dialog.h>
#import <vlc_modules.h>
#import <vlc_plugin.h>
#import <vlc_config_cat.h>
#import <vlc_aout.h>

#import "extensions/NSScreen+VLCAdditions.h"
#import "extensions/NSString+Helpers.h"
#import "extensions/NSFont+VLCAdditions.h"
#import "main/VLCMain.h"
#import "main/VLCMain+OldPrefs.h"
#import "os-integration/VLCClickerManager.h"
#import "preferences/prefs.h"
#import "library/VLCLibraryController.h"
#import "library/VLCLibraryModel.h"
#import "library/VLCLibraryDataTypes.h"

static struct {
    const char iso[6];
    const char name[34];
    BOOL isRightToLeft;

} const language_map[] = {
    { "auto",  N_("Auto"),              NO },
    { "en",    "American English",      NO },
    { "ar",    "عربي",                  YES },
    { "an",    "Aragonés",              NO },
    { "as_IN", "অসমীয়া",                 NO },
    { "ast",   "Asturianu",             NO },
    { "be",    "беларуская мова",       NO },
    { "brx",   "बर'/बड़",                 NO },
    { "bn",    "বাংলা",                   NO },
    { "pt_BR", "Português Brasileiro",  NO },
    { "en_GB", "British English",       NO },
    { "el",    "Νέα Ελληνικά",          NO },
    { "bg",    "български език",        NO },
    { "ca",    "Català",                NO },
    { "zh_TW", "正體中文",                NO },
    { "cs",    "Čeština",               NO },
    { "cy",    "Cymraeg",               NO },
    { "da",    "Dansk",                 NO },
    { "nl",    "Nederlands",            NO },
    { "fi",    "Suomi",                 NO },
    { "et",    "eesti keel",            NO },
    { "eu",    "Euskara",               NO },
    { "fr",    "Français",              NO },
    { "ga",    "Gaeilge",               NO },
    { "gd",    "Gàidhlig",              NO },
    { "gl",    "Galego",                NO },
    { "gu",    "ગુજરાતી",                 NO },
    { "de",    "Deutsch",               NO },
    { "he",    "עברית",                 YES },
    { "hr",    "hrvatski",              NO },
    { "kn",    "ಕನ್ನಡ",                   NO },
    { "lv",    "Latviešu valoda",       NO },
    { "hu",    "Magyar",                NO },
    { "mr",    "मराठी",                   NO },
    { "is",    "íslenska",              NO },
    { "id",    "Bahasa Indonesia",      NO },
    { "it",    "Italiano",              NO },
    { "ja",    "日本語",                 NO },
    { "ko",    "한국어",                  NO },
    { "lt",    "lietuvių",              NO },
    { "ms",    "Melayu",                NO },
    { "nb",    "Bokmål",                NO },
    { "nn",    "Nynorsk",               NO },
    { "kk",    "Қазақ тілі",            NO },
    { "km",    "ភាសាខ្មែរ",                NO },
    { "ne",    "नेपाली",                  NO },
    { "oc",    "Occitan",               NO },
    { "pl",    "Polski",                NO },
    { "pt_PT", "Português",             NO },
    { "pa",    "ਪੰਜਾਬੀ",                  NO },
    { "ro",    "Română",                NO },
    { "ru",    "Русский",               NO },
    { "zh_CN", "简体中文",                NO },
    { "si",    "සිංහල",                   NO },
    { "sr",    "српски",                NO },
    { "sk",    "Slovensky",             NO },
    { "sl",    "slovenščina",           NO },
    { "es",    "Español",               NO },
    { "es_MX", "Español Mexicano",      NO },
    { "sv",    "Svenska",               NO },
    { "th",    "ภาษาไทย",               NO },
    { "tr",    "Türkçe",                NO },
    { "uk",    "украї́нська мо́ва",       NO },
    { "vi",    "tiếng Việt",            NO },
    { "wa",    "Walon",                 NO }
};

static NSString* VLCSPrefsToolbarIdentifier = @"Our Simple Preferences Toolbar Identifier";
static NSString* VLCIntfSettingToolbarIdentifier = @"Intf Settings Item Identifier";
static NSString* VLCAudioSettingToolbarIdentifier = @"Audio Settings Item Identifier";
static NSString* VLCVideoSettingToolbarIdentifier = @"Video Settings Item Identifier";
static NSString* VLCOSDSettingToolbarIdentifier = @"Subtitles Settings Item Identifier";
static NSString* VLCInputSettingToolbarIdentifier = @"Input Settings Item Identifier";
static NSString* VLCMediaLibrarySettingToolbarIdentifier = @"Media Library Settings Item Identifier";
static NSString* VLCHotkeysSettingToolbarIdentifier = @"Hotkeys Settings Item Identifier";

@interface VLCMediaLibraryFolderManagementController : NSObject <NSTableViewDelegate, NSTableViewDataSource>
{
    NSArray *_cachedFolderList;
    VLCLibraryController *_libraryController;
}

@property (readwrite, weak) NSTableView *libraryFolderTableView;
@property (readwrite, weak) NSTableColumn *nameTableColumn;
@property (readwrite, weak) NSTableColumn *pathTableColumn;
@property (readwrite, weak) NSTableColumn *presentTableColumn;
@property (readwrite, weak) NSTableColumn *bannedTableColumn;
@property (readwrite, weak) NSButton *removeFolderButton;
@property (readwrite, weak) NSButton *banFolderButton;

- (IBAction)addFolder:(id)sender;
- (IBAction)removeFolder:(id)sender;
- (IBAction)banFolder:(id)sender;
@end

@interface VLCSimplePrefsController() <NSToolbarDelegate, NSWindowDelegate>
{
    BOOL _audioSettingChanged;
    BOOL _intfSettingChanged;
    BOOL _videoSettingChanged;
    BOOL _osdSettingChanged;
    BOOL _inputSettingChanged;
    BOOL _hotkeyChanged;

    NSOpenPanel *_selectFolderPanel;
    NSArray *_hotkeyDescriptions;
    NSArray *_hotkeyNames;
    NSArray *_hotkeysNonUseableKeys;
    NSMutableArray *_hotkeySettings;
    NSString *_keyInTransition;
    VLCMediaLibraryFolderManagementController *_mediaLibraryManagementController;

    intf_thread_t *p_intf;
}
@end

@implementation VLCSimplePrefsController

#pragma mark Initialisation

- (id)init
{
    self = [super initWithWindowNibName:@"SimplePreferences"];
    if (self) {
        p_intf = getIntf();
    }

    return self;
}

- (void)windowDidLoad
{
    [self initStrings];

#ifdef HAVE_SPARKLE
    [_intf_updateCheckbox bind:@"value"
                   toObject:[SUUpdater sharedUpdater]
                withKeyPath:@"automaticallyChecksForUpdates"
                    options:nil];
#else
    [_intf_updateCheckbox setState:NSOffState];
    [_intf_updateCheckbox setEnabled:NO];
#endif

    /* setup the toolbar */
    NSToolbar * toolbar = [[NSToolbar alloc] initWithIdentifier: VLCSPrefsToolbarIdentifier];
    [toolbar setAllowsUserCustomization: NO];
    [toolbar setAutosavesConfiguration: NO];
    [toolbar setDisplayMode: NSToolbarDisplayModeIconAndLabel];
    [toolbar setSizeMode: NSToolbarSizeModeRegular];
    [toolbar setDelegate: self];
    [self.window setToolbar:toolbar];

    [self.window setCollectionBehavior: NSWindowCollectionBehaviorFullScreenAuxiliary];
    [self.window setHidesOnDeactivate:YES];

    [_hotkeys_listbox setTarget:self];
    [_hotkeys_listbox setDoubleAction:@selector(hotkeyTableDoubleClick:)];

    /* setup useful stuff */
    _hotkeysNonUseableKeys = [NSArray arrayWithObjects:@"Command-c", @"Command-x", @"Command-v", @"Command-a", @"Command-," , @"Command-h", @"Command-Alt-h", @"Command-Shift-o", @"Command-o", @"Command-d", @"Command-n", @"Command-s", @"Command-l", @"Command-r", @"Command-3", @"Command-m", @"Command-w", @"Command-Shift-w", @"Command-Shift-c", @"Command-Shift-p", @"Command-i", @"Command-e", @"Command-Shift-e", @"Command-b", @"Command-Shift-m", @"Command-Ctrl-m", @"Command-?", @"Command-Alt-?", @"Command-Shift-f", nil];

    // Workaround for Mac OS X Lion, which does not apply the same constraints when set in IB
    NSView *clipView = _contentView.superview;

    NSDictionary *views = @{ @"view": _contentView };
    NSArray *constraints = [NSLayoutConstraint constraintsWithVisualFormat:@"|[view]|" options:0 metrics:nil views:views];
    [clipView addConstraints:constraints];

    [self setupMediaLibraryControlInterface];
}

- (void)setupMediaLibraryControlInterface
{
    _mediaLibraryManagementController = [[VLCMediaLibraryFolderManagementController alloc] init];
    _mediaLibraryBanFolderButton.enabled = _mediaLibraryRemoveFolderButton.enabled = NO;

    _mediaLibraryFolderTableView.delegate = _mediaLibraryManagementController;
    _mediaLibraryFolderTableView.dataSource = _mediaLibraryManagementController;

    _mediaLibraryManagementController.nameTableColumn = _mediaLibraryNameTableColumn;
    _mediaLibraryManagementController.presentTableColumn = _mediaLibraryPresentTableColumn;
    _mediaLibraryManagementController.bannedTableColumn = _mediaLibraryBannedTableColumn;
    _mediaLibraryManagementController.pathTableColumn = _mediaLibraryPathTableColumn;
    _mediaLibraryManagementController.removeFolderButton = _mediaLibraryRemoveFolderButton;
    _mediaLibraryManagementController.banFolderButton = _mediaLibraryBanFolderButton;

    _mediaLibraryAddFolderButton.target = _mediaLibraryManagementController;
    _mediaLibraryAddFolderButton.action = @selector(addFolder:);
    _mediaLibraryBanFolderButton.target = _mediaLibraryManagementController;
    _mediaLibraryBanFolderButton.action = @selector(banFolder:);
    _mediaLibraryRemoveFolderButton.target = _mediaLibraryManagementController;
    _mediaLibraryRemoveFolderButton.action = @selector(removeFolder:);
}

#define CreateToolbarItem(name, desc, img, sel) \
    toolbarItem = create_toolbar_item(itemIdent, name, desc, img, self, @selector(sel));
static inline NSToolbarItem *
create_toolbar_item(NSString *itemIdent, NSString *name, NSString *desc, NSString *img, id target, SEL selector)
{
    NSToolbarItem *toolbarItem = [[NSToolbarItem alloc] initWithItemIdentifier: itemIdent];

    [toolbarItem setLabel:name];
    [toolbarItem setPaletteLabel:desc];

    [toolbarItem setToolTip:desc];
    [toolbarItem setImage:[NSImage imageNamed:img]];

    [toolbarItem setTarget:target];
    [toolbarItem setAction:selector];

    [toolbarItem setEnabled:YES];
    [toolbarItem setAutovalidates:YES];

    return toolbarItem;
}

- (NSToolbarItem *) toolbar:(NSToolbar *)toolbar
      itemForItemIdentifier:(NSString *)itemIdent
  willBeInsertedIntoToolbar:(BOOL)willBeInserted
{
    NSToolbarItem *toolbarItem = nil;

    if ([itemIdent isEqual: VLCIntfSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS("Interface"), _NS("Interface Settings"), @"VLCInterfaceCone", showInterfaceSettings);
    } else if ([itemIdent isEqual: VLCAudioSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS("Audio"), _NS("Audio Settings"), @"VLCAudioCone", showAudioSettings);
    } else if ([itemIdent isEqual: VLCVideoSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS("Video"), _NS("Video Settings"), @"VLCVideoCone", showVideoSettings);
    } else if ([itemIdent isEqual: VLCOSDSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS(SUBPIC_TITLE), _NS("Subtitle & On Screen Display Settings"), @"VLCSubtitleCone", showOSDSettings);
    } else if ([itemIdent isEqual: VLCInputSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS(INPUT_TITLE), _NS("Input & Codec Settings"), @"VLCInputCone", showInputSettings);
    } else if ([itemIdent isEqual: VLCMediaLibrarySettingToolbarIdentifier]) {
        CreateToolbarItem(_NS("Media Library"), _NS("Media Library settings"), @"NXHelpBacktrack", showMediaLibrarySettings);
    } else if ([itemIdent isEqual: VLCHotkeysSettingToolbarIdentifier]) {
        CreateToolbarItem(_NS("Hotkeys"), _NS("Hotkeys settings"), @"VLCHotkeysCone", showHotkeySettings);
    }

    return toolbarItem;
}

- (NSArray<NSString *> *)toolbarIdentifiers {
    static dispatch_once_t onceToken;
    static NSArray<NSString *> *toolbarIdentifiers = nil;

    dispatch_once(&onceToken, ^{
        if ([[[VLCMain sharedInstance] libraryController] libraryModel]) {
            toolbarIdentifiers = @[VLCIntfSettingToolbarIdentifier,
                                   VLCAudioSettingToolbarIdentifier,
                                   VLCVideoSettingToolbarIdentifier,
                                   VLCOSDSettingToolbarIdentifier,
                                   VLCInputSettingToolbarIdentifier,
                                   VLCMediaLibrarySettingToolbarIdentifier,
                                   VLCHotkeysSettingToolbarIdentifier,
                                   NSToolbarFlexibleSpaceItemIdentifier];
        } else {
            toolbarIdentifiers = @[VLCIntfSettingToolbarIdentifier,
                                   VLCAudioSettingToolbarIdentifier,
                                   VLCVideoSettingToolbarIdentifier,
                                   VLCOSDSettingToolbarIdentifier,
                                   VLCInputSettingToolbarIdentifier,
                                   VLCHotkeysSettingToolbarIdentifier,
                                   NSToolbarFlexibleSpaceItemIdentifier];
        }
    });

    return toolbarIdentifiers;
}

- (NSArray *)toolbarDefaultItemIdentifiers: (NSToolbar *)toolbar
{
    return [self toolbarIdentifiers];
}

- (NSArray *)toolbarAllowedItemIdentifiers: (NSToolbar *)toolbar
{
    return [self toolbarIdentifiers];
}

- (NSArray *)toolbarSelectableItemIdentifiers:(NSToolbar *)toolbar
{
    return [self toolbarIdentifiers];
}

- (void)initStrings
{
    /* audio */
    [_audio_effectsBox setTitle: _NS("Audio Effects")];
    [_audio_enableCheckbox setTitle: _NS("Enable audio")];
    [_audio_generalBox setTitle: _NS("General Audio")];
    [_audio_langLabel setStringValue: _NS("Preferred Audio language")];
    [_audio_lastCheckbox setTitle: _NS("Enable Last.fm submissions")];
    [_audio_lastpwdLabel setStringValue: _NS("Password")];
    [_audio_lastuserLabel setStringValue: _NS("Username")];
    [_audio_visualLabel setStringValue: _NS("Visualization")];
    [_audio_autosavevol_yesButtonCell setTitle: _NS("Keep audio level between sessions")];
    [_audio_autosavevol_noButtonCell setTitle: _NS("Always reset audio start level to:")];

    /* hotkeys */
    [_hotkeys_change_win setTitle: _NS("Change Hotkey")];
    [_hotkeys_change_cancelButton setTitle: _NS("Cancel")];
    [_hotkeys_change_okButton setTitle: _NS("OK")];
    [_hotkeys_change_clearButton setTitle: _NS("Clear")];
    [_hotkeysLabel setStringValue: _NS("Double-click an action to change the associated hotkey:")];
    [[[_hotkeys_listbox tableColumnWithIdentifier: @"action"] headerCell] setStringValue: _NS("Action")];
    [[[_hotkeys_listbox tableColumnWithIdentifier: @"shortcut"] headerCell] setStringValue: _NS("Shortcut")];
    [_hotkeys_mediakeysCheckbox setTitle: _NS("Control playback with media keys")];

    /* input */
    [_input_recordBox setTitle: _NS("Record directory or filename")];
    [_input_recordButton setTitle: _NS("Browse...")];
    [_input_recordButton setToolTip: _NS("Directory or filename where the records will be stored")];
    [_input_aviLabel setStringValue: _NS("Repair AVI Files")];
    [_input_cachelevelLabel setStringValue: _NS("Default Caching Level")];
    [_input_cachingBox setTitle: _NS("Caching")];
    [_input_cachelevel_customLabel setStringValue: _NS("Use the complete preferences to configure custom caching values for each access module.")];
    [_input_muxBox setTitle: _NS("Codecs / Muxers")];
    [_input_netBox setTitle: _NS("Network")];
    [_input_hardwareAccelerationCheckbox setTitle: _NS("Enable hardware acceleration")];
    [_input_skipLoopLabel setStringValue: _NS("Skip the loop filter for H.264 decoding")];
    [_input_urlhandlerButton setTitle: _NS("Edit default application settings for network protocols")];
    [_input_skipFramesCheckbox setTitle: _NS("Skip frames")];
    [_input_fastSeekCheckbox setTitle: _NS("Fast seek")];

    /* url handler */
    [_urlhandler_titleLabel setStringValue: _NS("Open network streams using the following protocols")];
    [_urlhandler_subtitleLabel setStringValue: _NS("Note that these are system-wide settings.")];
    [_urlhandler_saveButton setTitle: _NS("Save")];
    [_urlhandler_cancelButton setTitle: _NS("Cancel")];

    /* interface */
    [_intf_generalSettingsBox setTitle:_NS("General settings")];
    [_intf_languageLabel setStringValue: _NS("Language")];

    [_intf_playbackControlBox setTitle:_NS("Playback control")];
    [_intf_continueplaybackLabel setStringValue:_NS("Continue playback")];
    [_intf_statusIconCheckbox setTitle: _NS("Display VLC status menu icon")];
    [_intf_largeFontInListsCheckbox setTitle: _NS("Use large text for list views")];

    [_intf_playbackBehaviourBox setTitle:_NS("Playback behaviour")];
    [_intf_enableNotificationsCheckbox setTitle: _NS("Enable notifications on playlist item change")];
    [_intf_pauseitunesLabel setStringValue:_NS("Control external music players")];

    [_intf_networkBox setTitle: _NS("Privacy / Network Interaction")];
    [_intf_artCheckbox setTitle: _NS("Allow metadata network access")];
    [_intf_updateCheckbox setTitle: _NS("Automatically check for updates")];
    [_intf_last_updateLabel setStringValue: @""];

    [_intf_luahttpBox setTitle:_NS("HTTP web interface")];
    [_intf_enableluahttpCheckbox setTitle: _NS("Enable HTTP web interface")];
    [_intf_luahttppwdLabel setStringValue:_NS("Password")];

    /* Subtitles and OSD */
    [_osd_encodingLabel setStringValue: _NS("Default Encoding")];
    [_osd_fontBox setTitle: _NS("Display Settings")];
    [_osd_fontButton setTitle: _NS("Choose...")];
    [_osd_font_colorLabel setStringValue: _NS("Font color")];
    [_osd_font_sizeLabel setStringValue: _NS("Font size")];
    [_osd_fontLabel setStringValue: _NS("Font")];
    [_osd_langBox setTitle: _NS("Subtitle languages")];
    [_osd_langLabel setStringValue: _NS("Preferred subtitle language")];
    [_osd_osdBox setTitle: _NS("On Screen Display")];
    [_osd_osdCheckbox setTitle: _NS("Enable OSD")];
    [_osd_opacityLabel setStringValue: _NS("Opacity")];
    [_osd_forceboldCheckbox setTitle: _NS("Force bold")];
    [_osd_outline_colorLabel setStringValue: _NS("Outline color")];
    [_osd_outline_thicknessLabel setStringValue: _NS("Outline thickness")];

    /* video */
    [_video_enableCheckbox setTitle: _NS("Enable video")];
    [_video_displayBox setTitle: _NS("Display")];
    [_video_embeddedCheckbox setTitle: _NS("Show video within the main window")];
    [_video_pauseWhenMinimizedCheckbox setTitle:_NS("Pause the video playback when minimized")];
    [_video_resizeToNativeSizeCheckbox setTitle:_NS("Resize interface to the native video size")];
    [_video_onTopCheckbox setTitle: _NS("Float on Top")];
    [_video_videodecoCheckbox setTitle: _NS("Window decorations")];

    [_video_fullscreenBox setTitle:_NS("Fullscreen settings")];
    [_video_startInFullscreenCheckbox setTitle:_NS("Start in fullscreen")];
    [_video_blackScreenCheckbox setTitle: _NS("Black screens in Fullscreen mode")];
    [_video_nativeFullscreenCheckbox setTitle: _NS("Use the native fullscreen mode")];
    [_video_deviceLabel setStringValue: _NS("Fullscreen Video Device")];

    [_video_snapBox setTitle: _NS("Video snapshots")];
    [_video_snap_folderButton setTitle: _NS("Browse...")];
    [_video_snap_folderLabel setStringValue: _NS("Folder")];
    [_video_snap_formatLabel setStringValue: _NS("Format")];
    [_video_snap_prefixLabel setStringValue: _NS("Prefix")];
    [_video_snap_seqnumCheckbox setTitle: _NS("Sequential numbering")];
    [_video_deinterlaceLabel setStringValue: _NS("Deinterlace")];
    [_video_deinterlace_modeLabel setStringValue: _NS("Deinterlace mode")];
    [_video_videoBox setTitle: _NS("Video")];

    /* media library */
    [_mediaLibraryAddFolderButton setTitle:_NS("Add Folder...")];
    [_mediaLibraryBanFolderButton setTitle:_NS("Ban Folder")];
    [_mediaLibraryRemoveFolderButton setTitle:_NS("Remove Folder")];
    [_mediaLibraryNameTableColumn setTitle:_NS("Name")];
    [_mediaLibraryPresentTableColumn setTitle:_NS("Present")];
    [_mediaLibraryBannedTableColumn setTitle:_NS("Banned")];
    [_mediaLibraryPathTableColumn setTitle:_NS("Location")];

    /* generic stuff */
    [_showAllButton setTitle: _NS("Show All")];
    [_cancelButton setTitle: _NS("Cancel")];
    [_resetButton setTitle: _NS("Reset All")];
    [_saveButton setTitle: _NS("Save")];
    [self.window setTitle: _NS("Preferences")];
}

/* TODO: move this part to core */
#define config_GetLabel(a,b) __config_GetLabel(VLC_OBJECT(a),b)
static inline const char * __config_GetLabel(vlc_object_t *p_this, const char *psz_name)
{
    module_config_t *p_config = config_FindConfig(psz_name);

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
    p_item = config_FindConfig(name);
    /* serious problem, if no item found */
    assert(p_item);

    char **values, **texts;
    ssize_t count = config_GetPszChoices(name, &values, &texts);
    if (count < 0) {
        msg_Err(p_intf, "Cannot get choices for %s", name);
        return;
    }
    for (ssize_t i = 0; i < count && texts; i++) {
        if (texts[i] == NULL || values[i] == NULL)
            continue;

        if (strcmp(texts[i], "") != 0) {
            NSMenuItem *mi = [[NSMenuItem alloc] initWithTitle: toNSStr(texts[i]) action: NULL keyEquivalent: @""];
            [mi setRepresentedObject: toNSStr(values[i])];
            [[object menu] addItem:mi];

            if (p_item->value.psz && !strcmp(p_item->value.psz, values[i]))
                [object selectItem: [object lastItem]];
        } else {
            [[object menu] addItem: [NSMenuItem separatorItem]];
        }

        free(texts[i]);
        free(values[i]);
    }

    free(texts);
    free(values);

    if (p_item->psz_longtext)
        [object setToolTip: _NS(p_item->psz_longtext)];
}

// just for clarification that this is a module list
- (void)setupButton: (NSPopUpButton *)object forModuleList: (const char *)name
{
    [self setupButton: object forStringList: name];
}

- (void)setupButton: (NSPopUpButton *)object forIntList: (const char *)name
{
    module_config_t *p_item;

    [object removeAllItems];
    p_item = config_FindConfig(name);

    /* serious problem, if no item found */
    assert(p_item);

    int64_t *values;
    char **texts;
    ssize_t count = config_GetIntChoices(name, &values, &texts);
    for (ssize_t i = 0; i < count; i++) {
        NSMenuItem *mi = [[NSMenuItem alloc] initWithTitle: toNSStr(texts[i]) action: NULL keyEquivalent: @""];
        [mi setRepresentedObject:[NSNumber numberWithInteger:values[i]]];
        [[object menu] addItem:mi];

        if (p_item->value.i == values[i])
            [object selectItem:[object lastItem]];

        free(texts[i]);
    }
    free(texts);

    if (p_item->psz_longtext)
        [object setToolTip: _NS(p_item->psz_longtext)];
}

- (void)setupButton: (NSButton *)object forBoolValue: (const char *)name
{
    [object setState: config_GetInt(name)];
    [object setToolTip: _NS(config_GetLabel(p_intf, name))];
}

- (void)setupField:(NSTextField *)object forOption:(const char *)psz_option
{
    char *psz_tmp = config_GetPsz(psz_option);
    [object setStringValue: toNSStr(psz_tmp)];
    [object setToolTip: _NS(config_GetLabel(p_intf, psz_option))];
    free(psz_tmp);
}

- (BOOL)hasModule:(NSString *)moduleName inConfig:(NSString *)config
{
    char *value = config_GetPsz([config UTF8String]);
    NSString *modules = toNSStr(value);
    free(value);

    return [[modules componentsSeparatedByString:@":"] containsObject:moduleName];
}

- (void)changeModule:(NSString *)moduleName inConfig:(NSString *)config enable:(BOOL)enable
{
    char *value = config_GetPsz([config UTF8String]);
    NSString *modules = toNSStr(value);
    free(value);

    NSMutableArray *components = [[modules componentsSeparatedByString:@":"] mutableCopy];
    if (enable) {
        if (![components containsObject:moduleName]) {
            [components addObject:moduleName];
        }
    } else {
        [components removeObject:moduleName];
    }

    // trim empty entries
    [components removeObject:@""];

    config_PutPsz([config UTF8String], [[components componentsJoinedByString:@":"] UTF8String]);
}

- (void)resetControls
{
    int i = 0;
    NSInteger y = 0;

    /**********************
     * interface settings *
     **********************/
    NSUInteger sel = 0;
    const char *pref = NULL;
    pref = [[[NSUserDefaults standardUserDefaults] objectForKey:@"language"] UTF8String];
    for (int x = 0; x < ARRAY_SIZE(language_map); x++) {
        [_intf_languagePopup addItemWithTitle:toNSStr(language_map[x].name)];
        if (pref) {
            if (!strcmp(language_map[x].iso, pref))
                sel = x;
        }
    }
    [_intf_languagePopup selectItemAtIndex:sel];

    [self setupButton:_intf_continueplaybackPopup forIntList: "macosx-continue-playback"];
    if (!var_InheritBool(p_intf, "macosx-recentitems")) {
        [_intf_continueplaybackPopup setEnabled: NO];
        [_intf_continueplaybackPopup setToolTip: _NS("Media files cannot be resumed because keeping recent media items is disabled.")];
    } else {
        [_intf_continueplaybackPopup setEnabled: YES];
    }

    [self setupButton:_intf_statusIconCheckbox forBoolValue: "macosx-statusicon"];
    [self setupButton:_intf_largeFontInListsCheckbox forBoolValue: "macosx-large-text"];

    [self setupButton:_video_nativeFullscreenCheckbox forBoolValue: "macosx-nativefullscreenmode"];
    [self setupButton:_video_embeddedCheckbox forBoolValue: "embedded-video"];

    [self setupButton:_intf_pauseitunesPopup forIntList: "macosx-control-itunes"];

    [self setupButton:_intf_artCheckbox forBoolValue: "metadata-network-access"];


#ifdef HAVE_SPARKLE
    if ([[SUUpdater sharedUpdater] lastUpdateCheckDate] != NULL)
        [_intf_last_updateLabel setStringValue: [NSString stringWithFormat: _NS("Last check on: %@"), [[[SUUpdater sharedUpdater] lastUpdateCheckDate] descriptionWithLocale: [[NSUserDefaults standardUserDefaults] dictionaryRepresentation]]]];
    else
        [_intf_last_updateLabel setStringValue: _NS("No check was performed yet.")];
#endif

    BOOL growlEnabled = [self hasModule:@"growl" inConfig:@"control"];
    [_intf_enableNotificationsCheckbox setState: growlEnabled ? NSOnState : NSOffState];

    BOOL httpEnabled = [self hasModule:@"http" inConfig:@"extraintf"];
    [_intf_enableluahttpCheckbox setState: httpEnabled ? NSOnState : NSOffState];
    _intf_luahttppwdTextField.enabled = httpEnabled;

    [self setupField:_intf_luahttppwdTextField forOption: "http-password"];

    /******************
     * audio settings *
     ******************/
    [self setupButton:_audio_enableCheckbox forBoolValue: "audio"];

    if (config_GetInt("volume-save")) {
        [_audio_autosavevol_yesButtonCell setState: NSOnState];
        [_audio_autosavevol_noButtonCell setState: NSOffState];
        [_audio_volTextField setEnabled: NO];
        [_audio_volSlider setEnabled: NO];

        [_audio_volSlider setIntValue: 100];
        [_audio_volTextField setIntValue: 100];
    } else {
        [_audio_autosavevol_yesButtonCell setState: NSOffState];
        [_audio_autosavevol_noButtonCell setState: NSOnState];
        [_audio_volTextField setEnabled: YES];
        [_audio_volSlider setEnabled: YES];

        i = (int)var_InheritInteger(p_intf, "auhal-volume");
        i = i * 200. / AOUT_VOLUME_MAX;
        [_audio_volSlider setIntValue: i];
        [_audio_volTextField setIntValue: i];
    }

    [self setupField:_audio_langTextField forOption: "audio-language"];

    [self setupButton:_audio_visualPopup forModuleList: "audio-visual"];

    /* Last.FM is optional */
    if (module_exists("audioscrobbler")) {
        [self setupField:_audio_lastuserTextField forOption:"lastfm-username"];
        [self setupField:_audio_lastpwdSecureTextField forOption:"lastfm-password"];

        if (config_ExistIntf("audioscrobbler")) {
            [_audio_lastCheckbox setState: NSOnState];
            [_audio_lastuserTextField setEnabled: YES];
            [_audio_lastpwdSecureTextField setEnabled: YES];
        } else {
            [_audio_lastCheckbox setState: NSOffState];
            [_audio_lastuserTextField setEnabled: NO];
            [_audio_lastpwdSecureTextField setEnabled: NO];
        }
    } else
        [_audio_lastCheckbox setEnabled: NO];

    /******************
     * video settings *
     ******************/
    [self setupButton:_video_enableCheckbox forBoolValue: "video"];
    [self setupButton:_video_startInFullscreenCheckbox forBoolValue: "fullscreen"];
    [self setupButton:_video_onTopCheckbox forBoolValue: "video-on-top"];
    [self setupButton:_video_blackScreenCheckbox forBoolValue: "macosx-black"];
    [self setupButton:_video_videodecoCheckbox forBoolValue: "video-deco"];
    [self setupButton:_video_pauseWhenMinimizedCheckbox forBoolValue: "macosx-pause-minimized"];
    [self setupButton:_video_resizeToNativeSizeCheckbox forBoolValue: "macosx-video-autoresize"];

    [_video_devicePopup removeAllItems];
    i = 0;
    y = [[NSScreen screens] count];
    [_video_devicePopup addItemWithTitle: _NS("Default")];
    [[_video_devicePopup lastItem] setTag: 0];
    while (i < y) {
        NSRect s_rect = [[[NSScreen screens] objectAtIndex:i] frame];
        [_video_devicePopup addItemWithTitle:
         [NSString stringWithFormat: @"%@ %i (%ix%i)", _NS("Screen"), i+1,
                   (int)s_rect.size.width, (int)s_rect.size.height]];
        [[_video_devicePopup lastItem] setTag: (int)[[[NSScreen screens] objectAtIndex:i] displayID]];
        i++;
    }
    [_video_devicePopup selectItemAtIndex: 0];
    [_video_devicePopup selectItemWithTag: config_GetInt("macosx-vdev")];

    [self setupField:_video_snap_folderTextField forOption:"snapshot-path"];
    [self setupField:_video_snap_prefixTextField forOption:"snapshot-prefix"];
    [self setupButton:_video_snap_seqnumCheckbox forBoolValue: "snapshot-sequential"];
    [self setupButton:_video_snap_formatPopup forStringList: "snapshot-format"];
    [self setupButton:_video_deinterlacePopup forIntList: "deinterlace"];
    [self setupButton:_video_deinterlace_modePopup forStringList: "deinterlace-mode"];

    // set lion fullscreen mode restrictions
    [self enableLionFullscreenMode: [_video_nativeFullscreenCheckbox state]];

    /***************************
     * input & codecs settings *
     ***************************/
    [self setupField:_input_recordTextField forOption:"input-record-path"];

    [self setupButton:_input_hardwareAccelerationCheckbox forBoolValue: "videotoolbox"];
    [self setupButton:_input_skipFramesCheckbox forBoolValue: "skip-frames"];
    [self setupButton:_input_fastSeekCheckbox forBoolValue: "input-fast-seek"];
    [self setupButton:_input_aviPopup forIntList: "avi-index"];
    [self setupButton:_input_skipLoopPopup forIntList: "avcodec-skiploopfilter"];

    [_input_cachelevelPopup removeAllItems];
    NSMenuItem *item = [[_input_cachelevelPopup menu] addItemWithTitle:_NS("Custom") action:nil keyEquivalent:@""];
    [item setTag: 0];
    item = [[_input_cachelevelPopup menu] addItemWithTitle:_NS("Lowest Latency") action:nil keyEquivalent:@""];
    [item setTag: 100];
    item = [[_input_cachelevelPopup menu] addItemWithTitle:_NS("Low Latency") action:nil keyEquivalent:@""];
    [item setTag: 200];
    item = [[_input_cachelevelPopup menu] addItemWithTitle:_NS("Normal") action:nil keyEquivalent:@""];
    [item setTag: 300];
    item = [[_input_cachelevelPopup menu] addItemWithTitle:_NS("Higher Latency") action:nil keyEquivalent:@""];
    [item setTag: 500];
    item = [[_input_cachelevelPopup menu] addItemWithTitle:_NS("Highest Latency") action:nil keyEquivalent:@""];
    [item setTag: 1000];

    #define TestCaC(name, factor) \
    cache_equal = cache_equal && \
    (i_cache * factor == config_GetInt(name));

    /* Select the accurate value of the PopupButton */
    bool cache_equal = true;
    int i_cache = (int)config_GetInt("file-caching");

    TestCaC("network-caching", 10/3);
    TestCaC("disc-caching", 1);
    TestCaC("live-caching", 1);
    if (cache_equal) {
        [_input_cachelevelPopup selectItemWithTag: i_cache];
        [_input_cachelevel_customLabel setHidden: YES];
    } else {
        [_input_cachelevelPopup selectItemWithTitle: _NS("Custom")];
        [_input_cachelevel_customLabel setHidden: NO];
    }
    #undef TestCaC

    /*********************
     * subtitle settings *
     *********************/
    [self setupButton:_osd_osdCheckbox forBoolValue: "osd"];

    [self setupButton:_osd_encodingPopup forStringList: "subsdec-encoding"];
    [self setupField:_osd_langTextField forOption: "sub-language" ];

    [self setupField:_osd_fontTextField forOption: "freetype-font"];
    [self setupButton:_osd_font_colorPopup forIntList: "freetype-color"];
    _osd_font_sizeSlider.intValue = (int)config_GetInt("sub-text-scale");
    [_osd_font_sizeTextField setStringValue: [NSString stringWithFormat:@"%.2fx", _osd_font_sizeSlider.intValue / 100.]];
    i = config_GetInt("freetype-opacity") * 100.0 / 255.0 + 0.5;
    [_osd_opacityTextField setIntValue: i];
    [_osd_opacitySlider setIntValue: i];
    [_osd_opacitySlider setToolTip: _NS(config_GetLabel(p_intf, "freetype-opacity"))];
    [_osd_opacityTextField setToolTip: [_osd_opacitySlider toolTip]];
    [self setupButton:_osd_forceboldCheckbox forBoolValue: "freetype-bold"];
    [self setupButton:_osd_outline_colorPopup forIntList: "freetype-outline-color"];
    [self setupButton:_osd_outline_thicknessPopup forIntList: "freetype-outline-thickness"];

    /********************
     * hotkeys settings *
     ********************/
    _hotkeySettings = [[NSMutableArray alloc] init];
    NSMutableArray *tempArray_desc = [[NSMutableArray alloc] init];
    NSMutableArray *tempArray_names = [[NSMutableArray alloc] init];

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
            [tempArray_desc addObject: _NS(p_item->psz_text)];
            [tempArray_names addObject: toNSStr(p_item->psz_name)];
            if (p_item->value.psz)
                [_hotkeySettings addObject: toNSStr(p_item->value.psz)];
            else
                [_hotkeySettings addObject: [NSString string]];
        }
    }
    module_config_free (p_config);

    _hotkeyDescriptions = [[NSArray alloc] initWithArray:tempArray_desc copyItems: YES];
    _hotkeyNames = [[NSArray alloc] initWithArray:tempArray_names copyItems: YES];

    [_hotkeys_listbox reloadData];
    [self setupButton:_hotkeys_mediakeysCheckbox forBoolValue: "macosx-mediakeys"];
}

#pragma mark -
#pragma mark General actions

- (void)showSimplePrefs
{
    /* we want to show the interface settings, if no category was chosen */
    if ([[self.window toolbar] selectedItemIdentifier] == nil) {
        [[self.window toolbar] setSelectedItemIdentifier: VLCIntfSettingToolbarIdentifier];
        [self showInterfaceSettings];
    }

    [self resetControls];

    [self.window makeKeyAndOrderFront: self];
}

- (void)showSimplePrefsWithLevel:(NSInteger)i_window_level
{
    [self.window setLevel: i_window_level];
    [self showSimplePrefs];
}

- (IBAction)buttonAction:(id)sender
{
    if (sender == _cancelButton) {
        [[NSFontPanel sharedFontPanel] close];
        [self.window orderOut: sender];
    } else if (sender == _saveButton) {
        [self saveChangedSettings];
        [[NSFontPanel sharedFontPanel] close];
        [self.window orderOut: sender];
    } else if (sender == _showAllButton) {
        [self.window orderOut: self];
        [[[VLCMain sharedInstance] preferences] showPrefsWithLevel:[self.window level]];
    } else
        msg_Warn(p_intf, "unknown buttonAction sender");
}

- (IBAction)resetPreferences:(NSControl *)sender
{
    NSAlert *alert = [[NSAlert alloc] init];
    [alert setAlertStyle:NSInformationalAlertStyle];
    [alert setMessageText:_NS("Reset Preferences")];
    [alert setInformativeText:_NS("This will reset VLC media player's preferences.\n\n"
                                  "Note that VLC will restart during the process, so your current "
                                  "playlist will be emptied and eventual playback, streaming or "
                                  "transcoding activities will stop immediately.\n\n"
                                  "The Media Library will not be affected.\n\n"
                                  "Are you sure you want to continue?")];
    [alert addButtonWithTitle:_NS("Cancel")];
    [alert addButtonWithTitle:_NS("Continue")];
    [alert beginSheetModalForWindow:[sender window] completionHandler:^(NSModalResponse returnCode) {
        if (returnCode == NSAlertSecondButtonReturn) {
            [[VLCMain sharedInstance] resetPreferences];
        }
    }];
}

static inline void save_int_list(intf_thread_t * p_intf, id object, const char * name)
{
    NSNumber *p_valueobject = (NSNumber *)[[object selectedItem] representedObject];
    if (p_valueobject) {
        assert([p_valueobject isKindOfClass:[NSNumber class]]);
        config_PutInt(name, [p_valueobject intValue]);
    }
}

static inline void save_string_list(intf_thread_t * p_intf, id object, const char * name)
{
    NSString *p_stringobject = (NSString *)[[object selectedItem] representedObject];
    if (p_stringobject) {
        assert([p_stringobject isKindOfClass:[NSString class]]);
        config_PutPsz(name, [p_stringobject UTF8String]);
    }
}

+ (BOOL)updateRightToLeftSettings
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *isoCode = [defaults stringForKey:@"language"];

    if (!isoCode || [isoCode isEqualToString:@"auto"]) {
        // Automatic handling of right to left
        [defaults removeObjectForKey:@"NSForceRightToLeftWritingDirection"];
        [defaults removeObjectForKey:@"AppleTextDirection"];
    } else {
        for(int i = 0; i < ARRAY_SIZE(language_map); i++) {
            if (!strcmp(language_map[i].iso, [isoCode UTF8String])) {
                [defaults setBool:language_map[i].isRightToLeft forKey:@"NSForceRightToLeftWritingDirection"];
                [defaults setBool:language_map[i].isRightToLeft forKey:@"AppleTextDirection"];
                return YES;
            }
        }
    }

    return NO;
}

- (void)saveChangedSettings
{
#define SaveIntList(object, name) save_int_list(p_intf, object, name)

#define SaveStringList(object, name) save_string_list(p_intf, object, name)
#define SaveModuleList(object, name) SaveStringList(object, name)

    /**********************
     * interface settings *
     **********************/
    if (_intfSettingChanged) {
        NSUInteger index = [_intf_languagePopup indexOfSelectedItem];
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        [defaults setObject:toNSStr(language_map[index].iso) forKey:@"language"];
        [VLCSimplePrefsController updateRightToLeftSettings];

        config_PutInt("metadata-network-access", [_intf_artCheckbox state]);

        config_PutInt("macosx-statusicon", [_intf_statusIconCheckbox state]);
        config_PutInt("macosx-large-text", [_intf_largeFontInListsCheckbox state]);

        [self changeModule:@"growl" inConfig:@"control" enable:[_intf_enableNotificationsCheckbox state] == NSOnState];

        [self changeModule:@"http" inConfig:@"extraintf" enable:[_intf_enableluahttpCheckbox state] == NSOnState];
        config_PutPsz("http-password", [[_intf_luahttppwdTextField stringValue] UTF8String]);

        SaveIntList(_intf_pauseitunesPopup, "macosx-control-itunes");
        SaveIntList(_intf_continueplaybackPopup, "macosx-continue-playback");
        _intfSettingChanged = NO;
    }

    /******************
     * audio settings *
     ******************/
    if (_audioSettingChanged) {
        config_PutInt("audio", [_audio_enableCheckbox state]);
        config_PutInt("volume-save", [_audio_autosavevol_yesButtonCell state]);
        var_SetBool(p_intf, "volume-save", [_audio_autosavevol_yesButtonCell state]);
        if ([_audio_volTextField isEnabled])
            config_PutInt("auhal-volume", ([_audio_volTextField intValue] * AOUT_VOLUME_MAX) / 200);

        config_PutPsz("audio-language", [[_audio_langTextField stringValue] UTF8String]);

        SaveModuleList(_audio_visualPopup, "audio-visual");

        /* Last.FM is optional */
        if (module_exists("audioscrobbler")) {
            [_audio_lastCheckbox setEnabled: YES];
            if ([_audio_lastCheckbox state] == NSOnState)
                config_AddIntf("audioscrobbler");
            else
                config_RemoveIntf("audioscrobbler");

            config_PutPsz("lastfm-username", [[_audio_lastuserTextField stringValue] UTF8String]);
            config_PutPsz("lastfm-password", [[_audio_lastpwdSecureTextField stringValue] UTF8String]);
        }
        else
            [_audio_lastCheckbox setEnabled: NO];
        _audioSettingChanged = NO;
    }

    /******************
     * video settings *
     ******************/
    if (_videoSettingChanged) {
        config_PutInt("video", [_video_enableCheckbox state]);
        config_PutInt("fullscreen", [_video_startInFullscreenCheckbox state]);
        config_PutInt("video-deco", [_video_videodecoCheckbox state]);
        config_PutInt("video-on-top", [_video_onTopCheckbox state]);
        config_PutInt("macosx-black", [_video_blackScreenCheckbox state]);

        config_PutInt("macosx-pause-minimized", [_video_pauseWhenMinimizedCheckbox state]);
        config_PutInt("macosx-video-autoresize", [_video_resizeToNativeSizeCheckbox state]);

        config_PutInt("embedded-video", [_video_embeddedCheckbox state]);
        config_PutInt("macosx-nativefullscreenmode", [_video_nativeFullscreenCheckbox state]);
        config_PutInt("macosx-vdev", [[_video_devicePopup selectedItem] tag]);

        config_PutPsz("snapshot-path", [[_video_snap_folderTextField stringValue] UTF8String]);
        config_PutPsz("snapshot-prefix", [[_video_snap_prefixTextField stringValue] UTF8String]);
        config_PutInt("snapshot-sequential", [_video_snap_seqnumCheckbox state]);
        SaveStringList(_video_snap_formatPopup, "snapshot-format");
        SaveIntList(_video_deinterlacePopup, "deinterlace");
        SaveStringList(_video_deinterlace_modePopup, "deinterlace-mode");
        _videoSettingChanged = NO;
    }

    /***************************
     * input & codecs settings *
     ***************************/
    if (_inputSettingChanged) {
        config_PutPsz("input-record-path", [[_input_recordTextField stringValue] UTF8String]);

        config_PutInt("videotoolbox", [_input_hardwareAccelerationCheckbox state]);
        config_PutInt("skip-frames", [_input_skipFramesCheckbox state]);
        config_PutInt("input-fast-seek", [_input_fastSeekCheckbox state]);

        SaveIntList(_input_aviPopup, "avi-index");

        SaveIntList(_input_skipLoopPopup, "avcodec-skiploopfilter");

        #define CaC(name, factor) config_PutInt(name, [[_input_cachelevelPopup selectedItem] tag] * factor)
        if ([[_input_cachelevelPopup selectedItem] tag] == 0) {
            msg_Dbg(p_intf, "Custom chosen, not adjusting cache values");
        } else {
            msg_Dbg(p_intf, "Adjusting all cache values to: %i", (int)[[_input_cachelevelPopup selectedItem] tag]);
            CaC("file-caching", 1);
            CaC("network-caching", 10/3);
            CaC("disc-caching", 1);
            CaC("live-caching", 1);
        }
        #undef CaC
        _inputSettingChanged = NO;
    }

    /**********************
     * subtitles settings *
     **********************/
    if (_osdSettingChanged) {
        config_PutInt("osd", [_osd_osdCheckbox state]);

        if ([_osd_encodingPopup indexOfSelectedItem] >= 0)
            SaveStringList(_osd_encodingPopup, "subsdec-encoding");
        else
            config_PutPsz("subsdec-encoding", "");

        config_PutPsz("sub-language", [[_osd_langTextField stringValue] UTF8String]);

        config_PutPsz("freetype-font", [[_osd_fontTextField stringValue] UTF8String]);
        SaveIntList(_osd_font_colorPopup, "freetype-color");
        config_PutInt("sub-text-scale", _osd_font_sizeSlider.intValue);
        config_PutInt("freetype-opacity", [_osd_opacityTextField intValue] * 255.0 / 100.0 + 0.5);
        config_PutInt("freetype-bold", [_osd_forceboldCheckbox state]);
        SaveIntList(_osd_outline_colorPopup, "freetype-outline-color");
        SaveIntList(_osd_outline_thicknessPopup, "freetype-outline-thickness");
        _osdSettingChanged = NO;
    }

    /********************
     * hotkeys settings *
     ********************/
    if (_hotkeyChanged) {
        NSUInteger hotKeyCount = [_hotkeySettings count];
        for (NSUInteger i = 0; i < hotKeyCount; i++)
            config_PutPsz([[_hotkeyNames objectAtIndex:i] UTF8String], [[_hotkeySettings objectAtIndex:i]UTF8String]);
        _hotkeyChanged = NO;

        config_PutInt("macosx-mediakeys", [_hotkeys_mediakeysCheckbox state]);
    }

    fixIntfSettings();

    /* okay, let's save our changes to vlcrc */
    config_SaveConfigFile(p_intf);
    [[NSNotificationCenter defaultCenter] postNotificationName:VLCConfigurationChangedNotification object:nil];
}

- (void)showSettingsForCategory:(NSView *)categoryView
{
    [_contentView setSubviews:[NSArray array]];
    [_contentView addSubview:categoryView];

    NSDictionary *views = @{ @"view": categoryView };
    NSArray *constraints = [NSLayoutConstraint constraintsWithVisualFormat:@"|[view]|" options:0 metrics:nil views:views];
    [_contentView addConstraints:constraints];
    constraints = [NSLayoutConstraint constraintsWithVisualFormat:@"V:|[view]|" options:0 metrics:nil views:views];
    [_contentView addConstraints:constraints];

    [_scrollView layoutSubtreeIfNeeded];
    [_scrollView flashScrollers];
}

#pragma mark -
#pragma mark Specific actions

// disables some video settings which do not work in lion mode
- (void)enableLionFullscreenMode: (BOOL)_value
{
    [_video_videodecoCheckbox setEnabled: !_value];
    [_video_blackScreenCheckbox setEnabled: !_value];
    [_video_devicePopup setEnabled: !_value];

    if (_value) {
        [_video_videodecoCheckbox setState: NSOnState];
        [_video_blackScreenCheckbox setState: NSOffState];

        NSString *tooltipText = _NS("This setting cannot be changed because the native fullscreen mode is enabled.");
        [_video_videodecoCheckbox setToolTip:tooltipText];
        [_video_blackScreenCheckbox setToolTip:tooltipText];
        [_video_devicePopup setToolTip:tooltipText];

    } else {
        [self setupButton:_video_videodecoCheckbox forBoolValue: "video-deco"];
        [self setupButton:_video_blackScreenCheckbox forBoolValue: "macosx-black"];

        [_video_devicePopup setToolTip:@""];
    }
}

- (IBAction)interfaceSettingChanged:(id)sender
{
    if (sender == _intf_enableluahttpCheckbox) {
        _intf_luahttppwdTextField.enabled = [sender state] == NSOnState;
    }

    _intfSettingChanged = YES;
}

- (void)showInterfaceSettings
{
    [self showSettingsForCategory:_intfView];
}

- (IBAction)audioSettingChanged:(id)sender
{
    if (sender == _audio_volSlider)
        [_audio_volTextField setIntValue: [_audio_volSlider intValue]];

    if (sender == _audio_volTextField)
        [_audio_volSlider setIntValue: [_audio_volTextField intValue]];

    if (sender == _audio_lastCheckbox) {
        if ([_audio_lastCheckbox state] == NSOnState) {
            [_audio_lastpwdSecureTextField setEnabled: YES];
            [_audio_lastuserTextField setEnabled: YES];
        } else {
            [_audio_lastpwdSecureTextField setEnabled: NO];
            [_audio_lastuserTextField setEnabled: NO];
        }
    }

    if (sender == _audio_autosavevolMatrix) {
        BOOL enableVolumeSlider = [_audio_autosavevolMatrix selectedTag] == 1;
        [_audio_volTextField setEnabled: enableVolumeSlider];
        [_audio_volSlider setEnabled: enableVolumeSlider];
    }

    _audioSettingChanged = YES;
}

- (void)showAudioSettings
{
    [self showSettingsForCategory:_audioView];
}

- (IBAction)videoSettingChanged:(id)sender
{
    if (sender == _video_nativeFullscreenCheckbox)
        [self enableLionFullscreenMode:[sender state]];
    else if (sender == _video_snap_folderButton) {
        _selectFolderPanel = [[NSOpenPanel alloc] init];
        [_selectFolderPanel setCanChooseDirectories: YES];
        [_selectFolderPanel setCanChooseFiles: NO];
        [_selectFolderPanel setResolvesAliases: YES];
        [_selectFolderPanel setAllowsMultipleSelection: NO];
        [_selectFolderPanel setMessage: _NS("Choose the folder to save your video snapshots to.")];
        [_selectFolderPanel setCanCreateDirectories: YES];
        [_selectFolderPanel setPrompt: _NS("Choose")];
        [_selectFolderPanel beginSheetModalForWindow:self.window completionHandler: ^(NSInteger returnCode) {
            if (returnCode == NSModalResponseOK) {
                [self->_video_snap_folderTextField setStringValue: [[self->_selectFolderPanel URL] path]];
            }
        }];
    }

    _videoSettingChanged = YES;
}

- (void)showVideoSettings
{
    [self showSettingsForCategory:_videoView];
}

- (IBAction)osdSettingChanged:(id)sender
{
    if (sender == _osd_opacityTextField) {
        [_osd_opacitySlider setIntValue: [_osd_opacityTextField intValue]];
    } else if (sender == _osd_opacitySlider) {
        [_osd_opacityTextField setIntValue: [_osd_opacitySlider intValue]];
    } else if (sender == _osd_font_sizeSlider) {
        [_osd_font_sizeTextField setStringValue: [NSString stringWithFormat:@"%.2fx", _osd_font_sizeSlider.intValue / 100.]];
    }

    _osdSettingChanged = YES;
}

- (void)showOSDSettings
{
    [self showSettingsForCategory:_osdView];
}

- (void)controlTextDidChange:(NSNotification *)notification
{
    id notificationObject = [notification object];
    if (notificationObject == _audio_langTextField ||
       notificationObject ==  _audio_lastpwdSecureTextField ||
       notificationObject ==  _audio_lastuserTextField ||
       notificationObject == _audio_volTextField)
        _audioSettingChanged = YES;
    else if (notificationObject == _input_recordTextField)
        _inputSettingChanged = YES;
    else if (notificationObject == _osd_fontTextField ||
            notificationObject == _osd_langTextField ||
            notificationObject == _osd_opacityTextField)
        _osdSettingChanged = YES;
    else if (notificationObject == _video_snap_folderTextField ||
            notificationObject == _video_snap_prefixTextField)
        _videoSettingChanged = YES;
    else if (notificationObject == _intf_luahttppwdTextField)
        _intfSettingChanged = YES;
}

- (IBAction)showFontPicker:(id)sender
{
    char * font = config_GetPsz("freetype-font");
    NSString * fontName = font ? toNSStr(font) : nil;
    free(font);
    if (fontName) {
        NSFont * font = [NSFont fontWithName:fontName size:0.0];
        [[NSFontManager sharedFontManager] setSelectedFont:font isMultiple:NO];
    }
    [[NSFontManager sharedFontManager] setTarget: self];
    [[NSFontPanel sharedFontPanel] setDelegate:self];
    [[NSFontPanel sharedFontPanel] makeKeyAndOrderFront:self];
}

- (void)changeFont:(id)sender
{
    NSFont *someFont = [NSFont systemFontOfSize:12];

    // converts given font to changes in font panel. Original font is irrelevant
    NSFont *selectedFont = [sender convertFont:someFont];

    [_osd_fontTextField setStringValue:[selectedFont fontName]];
    [self osdSettingChanged:self];
}

// NSFontPanelModeMask is replacing NSUInteger, and should be backwards compatible
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
- (NSFontPanelModeMask)validModesForFontPanel:(NSFontPanel *)fontPanel
{
    return NSFontPanelFaceModeMask | NSFontPanelCollectionModeMask;
}
#pragma clang diagnostic pop

- (IBAction)inputSettingChanged:(id)sender
{
    if (sender == _input_cachelevelPopup) {
        if ([[[_input_cachelevelPopup selectedItem] title] isEqualToString: _NS("Custom")])
            [_input_cachelevel_customLabel setHidden: NO];
        else
            [_input_cachelevel_customLabel setHidden: YES];
    } else if (sender == _input_recordButton) {
        _selectFolderPanel = [[NSOpenPanel alloc] init];
        [_selectFolderPanel setCanChooseDirectories: YES];
        [_selectFolderPanel setCanChooseFiles: YES];
        [_selectFolderPanel setResolvesAliases: YES];
        [_selectFolderPanel setAllowsMultipleSelection: NO];
        [_selectFolderPanel setMessage: _NS("Choose the directory or filename where the records will be stored.")];
        [_selectFolderPanel setCanCreateDirectories: YES];
        [_selectFolderPanel setPrompt: _NS("Choose")];
        [_selectFolderPanel beginSheetModalForWindow:self.window completionHandler: ^(NSInteger returnCode) {
            if (returnCode == NSModalResponseOK)
            {
                [self->_input_recordTextField setStringValue: [[self->_selectFolderPanel URL] path]];
                self->_inputSettingChanged = YES;
            }
        }];

        return;
    }

    _inputSettingChanged = YES;
}

- (void)showInputSettings
{
    [self showSettingsForCategory:_inputView];
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
    if (sender == _input_urlhandlerButton) {

        void (^fillUrlHandlerPopup)(NSString*, NSPopUpButton*) = ^void(NSString *protocol, NSPopUpButton *object) {

            NSArray *handlers = (__bridge_transfer NSArray *)LSCopyAllHandlersForURLScheme((__bridge CFStringRef)protocol);
            NSMutableArray *rawHandlers = [[NSMutableArray alloc] init];
            [object removeAllItems];
            NSUInteger count = [handlers count];
            for (NSUInteger x = 0; x < count; x++) {
                NSString *rawhandler = [handlers objectAtIndex:x];
                NSString *handler = [self applicationNameForBundleIdentifier:rawhandler];
                if (handler && ![handler isEqualToString:@""]) {
                    [object addItemWithTitle:handler];
                    [[object lastItem] setImage: [self iconForBundleIdentifier:[handlers objectAtIndex:x]]];
                    [rawHandlers addObject: rawhandler];
                }
            }
            [object selectItemAtIndex: [rawHandlers indexOfObject:(__bridge_transfer id)LSCopyDefaultHandlerForURLScheme((__bridge CFStringRef)protocol)]];
        };

        fillUrlHandlerPopup(@"ftp", _urlhandler_ftpPopup);
        fillUrlHandlerPopup(@"mms", _urlhandler_mmsPopup);
        fillUrlHandlerPopup(@"rtmp", _urlhandler_rtmpPopup);
        fillUrlHandlerPopup(@"rtp", _urlhandler_rtpPopup);
        fillUrlHandlerPopup(@"rtsp", _urlhandler_rtspPopup);
        fillUrlHandlerPopup(@"sftp", _urlhandler_sftpPopup);
        fillUrlHandlerPopup(@"smb", _urlhandler_smbPopup);
        fillUrlHandlerPopup(@"udp", _urlhandler_udpPopup);

        [self.window beginSheet:_urlhandler_win completionHandler:nil];
    } else {
        [_urlhandler_win orderOut:sender];
        [NSApp endSheet:_urlhandler_win];

        if (sender == _urlhandler_saveButton) {
            LSSetDefaultHandlerForURLScheme(CFSTR("ftp"), (__bridge CFStringRef)[self bundleIdentifierForApplicationName:[[_urlhandler_ftpPopup selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("mms"), (__bridge CFStringRef)[self bundleIdentifierForApplicationName:[[_urlhandler_mmsPopup selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("mmsh"), (__bridge CFStringRef)[self bundleIdentifierForApplicationName:[[_urlhandler_mmsPopup selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("rtmp"), (__bridge CFStringRef)[self bundleIdentifierForApplicationName:[[_urlhandler_rtmpPopup selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("rtp"), (__bridge CFStringRef)[self bundleIdentifierForApplicationName:[[_urlhandler_rtpPopup selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("rtsp"), (__bridge CFStringRef)[self bundleIdentifierForApplicationName:[[_urlhandler_rtspPopup selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("sftp"), (__bridge CFStringRef)[self bundleIdentifierForApplicationName:[[_urlhandler_sftpPopup selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("smb"), (__bridge CFStringRef)[self bundleIdentifierForApplicationName:[[_urlhandler_smbPopup selectedItem] title]]);
            LSSetDefaultHandlerForURLScheme(CFSTR("udp"), (__bridge CFStringRef)[self bundleIdentifierForApplicationName:[[_urlhandler_udpPopup selectedItem] title]]);
        }
    }
}

#pragma mark -
#pragma mark Hotkey actions

- (void)hotkeyTableDoubleClick:(id)object
{
    // -1 is header
    if ([_hotkeys_listbox clickedRow] >= 0)
        [self hotkeySettingChanged:_hotkeys_listbox];
}

- (IBAction)hotkeySettingChanged:(id)sender
{
    if (sender == _hotkeys_listbox) {
        [_hotkeys_changeLabel setStringValue: [NSString stringWithFormat: _NS("Press new keys for\n\"%@\""),
                                               [_hotkeyDescriptions objectAtIndex:[_hotkeys_listbox selectedRow]]]];
        [_hotkeys_change_keysLabel setStringValue: OSXStringKeyToString([_hotkeySettings objectAtIndex:[_hotkeys_listbox selectedRow]])];
        [_hotkeys_change_takenLabel setStringValue: @""];
        [_hotkeys_change_win setInitialFirstResponder: [_hotkeys_change_win contentView]];
        [_hotkeys_change_win makeFirstResponder: [_hotkeys_change_win contentView]];
        [NSApp runModalForWindow:_hotkeys_change_win];
    } else if (sender == _hotkeys_change_cancelButton) {
        [NSApp stopModal];
        [_hotkeys_change_win close];
    } else if (sender == _hotkeys_change_okButton) {
        NSInteger i_returnValue;
        if (! _keyInTransition) {
            [NSApp stopModal];
            [_hotkeys_change_win close];
            NSAssert(1, @"internal error prevented the hotkey switch");
            return;
        }

        _hotkeyChanged = YES;

        i_returnValue = [_hotkeySettings indexOfObject: _keyInTransition];
        if (i_returnValue != NSNotFound)
            [_hotkeySettings replaceObjectAtIndex: i_returnValue withObject: [NSString string]];
        NSString *tempString;
        tempString = [_keyInTransition stringByReplacingOccurrencesOfString:@"-" withString:@"+"];
        i_returnValue = [_hotkeySettings indexOfObject: tempString];
        if (i_returnValue != NSNotFound)
            [_hotkeySettings replaceObjectAtIndex: i_returnValue withObject: [NSString string]];

        [_hotkeySettings replaceObjectAtIndex: [_hotkeys_listbox selectedRow] withObject:_keyInTransition];

        [NSApp stopModal];
        [_hotkeys_change_win close];

        [_hotkeys_listbox reloadData];
    } else if (sender == _hotkeys_change_clearButton) {
        [_hotkeySettings replaceObjectAtIndex: [_hotkeys_listbox selectedRow] withObject: [NSString string]];
        _hotkeyChanged = YES;
        [NSApp stopModal];
        [_hotkeys_change_win close];
        [_hotkeys_listbox reloadData];
    }
}

- (void)showHotkeySettings
{
    [self showSettingsForCategory:_hotkeysView];
}

- (NSUInteger)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [_hotkeySettings count];
}

- (id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(int)rowIndex
{
    NSString * identifier = [aTableColumn identifier];

    if ([identifier isEqualToString: @"action"])
        return [_hotkeyDescriptions objectAtIndex:rowIndex];
    else if ([identifier isEqualToString: @"shortcut"])
        return OSXStringKeyToString([_hotkeySettings objectAtIndex:rowIndex]);
    else {
        msg_Err(p_intf, "unknown TableColumn identifier (%s)!", [identifier UTF8String]);
        return NULL;
    }
}

- (BOOL)changeHotkeyTo: (NSString *)theKey
{
    NSInteger i_returnValue, i_returnValue2;
    i_returnValue = [_hotkeysNonUseableKeys indexOfObject: theKey];

    if (i_returnValue != NSNotFound || [theKey isEqualToString:@""]) {
        [_hotkeys_change_keysLabel setStringValue: _NS("Invalid combination")];
        [_hotkeys_change_takenLabel setStringValue: _NS("Regrettably, these keys cannot be assigned as hotkey shortcuts.")];
        [_hotkeys_change_okButton setEnabled: NO];
        return NO;
    } else {
        [_hotkeys_change_keysLabel setStringValue: OSXStringKeyToString(theKey)];

        i_returnValue = [_hotkeySettings indexOfObject: theKey];
        i_returnValue2 = [_hotkeySettings indexOfObject: [theKey stringByReplacingOccurrencesOfString:@"-" withString:@"+"]];
        if (i_returnValue != NSNotFound)
            [_hotkeys_change_takenLabel setStringValue: [NSString stringWithFormat:
                                                         _NS("This combination is already taken by \"%@\"."),
                                                         [_hotkeyDescriptions objectAtIndex:i_returnValue]]];
        else if (i_returnValue2 != NSNotFound)
            [_hotkeys_change_takenLabel setStringValue: [NSString stringWithFormat:
                                                         _NS("This combination is already taken by \"%@\"."),
                                                         [_hotkeyDescriptions objectAtIndex:i_returnValue2]]];
        else
            [_hotkeys_change_takenLabel setStringValue: @""];

        [_hotkeys_change_okButton setEnabled: YES];
        _keyInTransition = theKey;
        return YES;
    }
}

- (void)showMediaLibrarySettings
{
    [self showSettingsForCategory:_mediaLibraryView];
}


@end

@implementation VLCMediaLibraryFolderManagementController

- (instancetype)init
{
    self = [super init];
    if (self) {
        _libraryController = [[VLCMain sharedInstance] libraryController];
    }
    return self;
}

- (IBAction)addFolder:(id)sender
{
    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setTitle:_NS("Add Folder")];
    [openPanel setCanChooseFiles:NO];
    [openPanel setCanChooseDirectories:YES];
    [openPanel setAllowsMultipleSelection:YES];

    NSModalResponse returnValue = [openPanel runModal];

    if (returnValue == NSModalResponseOK) {
        NSArray *URLs = [openPanel URLs];
        NSUInteger count = [URLs count];
        for (NSUInteger i = 0; i < count ; i++) {
            NSURL *url = URLs[i];
            [_libraryController addFolderWithFileURL:url];
        }

        _cachedFolderList = nil;
        [self.libraryFolderTableView reloadData];
    }
}

- (IBAction)banFolder:(id)sender
{
    VLCMediaLibraryEntryPoint *entryPoint = _cachedFolderList[self.libraryFolderTableView.selectedRow];
    if (entryPoint.isBanned) {
        [_libraryController unbanFolderWithFileURL:[NSURL URLWithString:entryPoint.MRL]];
    } else {
        [_libraryController banFolderWithFileURL:[NSURL URLWithString:entryPoint.MRL]];
    }

    _cachedFolderList = nil;
    [self.libraryFolderTableView reloadData];
}

- (IBAction)removeFolder:(id)sender
{
    VLCMediaLibraryEntryPoint *entryPoint = _cachedFolderList[self.libraryFolderTableView.selectedRow];
    [_libraryController removeFolderWithFileURL:[NSURL URLWithString:entryPoint.MRL]];

    _cachedFolderList = nil;
    [self.libraryFolderTableView reloadData];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    VLCLibraryModel *libraryModel = [_libraryController libraryModel];
    if (!libraryModel) {
        return 0;
    }
    if (!_cachedFolderList) {
        _cachedFolderList = [libraryModel listOfMonitoredFolders];
    }
    return _cachedFolderList.count;
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    VLCMediaLibraryEntryPoint *entryPoint = _cachedFolderList[row];
    if (tableColumn == self.nameTableColumn) {
        return [entryPoint.decodedMRL lastPathComponent];
    } else if (tableColumn == self.presentTableColumn) {
        return entryPoint.isPresent ? @"✔" : @"✘";
    } else if (tableColumn == self.bannedTableColumn) {
        return entryPoint.isBanned ? @"✔" : @"✘";
    } else {
        return entryPoint.decodedMRL;
    }
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    NSInteger selectedRow = self.libraryFolderTableView.selectedRow;
    if (selectedRow == -1) {
        self.banFolderButton.enabled = self.removeFolderButton.enabled = NO;
        return;
    }
    self.banFolderButton.enabled = self.removeFolderButton.enabled = YES;
    VLCMediaLibraryEntryPoint *entryPoint = _cachedFolderList[selectedRow];
    [self.banFolderButton setTitle:entryPoint.isBanned ? _NS("Unban Folder") : _NS("Ban Folder")];
}

@end
