/*****************************************************************************
 *MainMenu.m: MacOS X interface module
 *****************************************************************************
 *Copyright (C) 2011-2019 Felix Paul Kühne
 *
 *Authors: Felix Paul Kühne <fkuehne -at- videolan -dot- org>
 *
 *This program is free software; you can redistribute it and/or modify
 *it under the terms of the GNU General Public License as published by
 *the Free Software Foundation; either version 2 of the License, or
 *(at your option) any later version.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program; if not, write to the Free Software
 *Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#import "VLCMainMenu.h"
#import "main/VLCMain.h"

#import <vlc_common.h>
#import <vlc_input.h>
#import <vlc_playlist_legacy.h>

#import "coreinteraction/VLCCoreInteraction.h"
#import "coreinteraction/VLCVideoFilterHelper.h"

#import "extensions/NSScreen+VLCAdditions.h"
#import "library/VLCLibraryWindow.h"

#import "menus/renderers/VLCRendererMenuController.h"

#import "panels/VLCAudioEffectsWindowController.h"
#import "panels/VLCTrackSynchronizationWindowController.h"
#import "panels/VLCVideoEffectsWindowController.h"
#import "panels/VLCBookmarksWindowController.h"
#import "panels/dialogs/VLCCoreDialogProvider.h"
#import "panels/VLCInformationWindowController.h"
#import "panels/VLCTimeSelectionPanelController.h"

#import "playlist/VLCPlaylistController.h"
#import "playlist/VLCPlayerController.h"
#import "preferences/VLCSimplePrefsController.h"

#import "windows/VLCAboutWindowController.h"
#import "windows/VLCOpenWindowController.h"
#import "windows/VLCErrorWindowController.h"
#import "windows/VLCHelpWindowController.h"
#import "windows/mainwindow/VLCMainWindow.h"
#import "windows/mainwindow/VLCMainWindowControlsBar.h"
#import "windows/extensions/VLCExtensionsManager.h"
#import "windows/video/VLCVoutView.h"
#import "windows/convertandsave/VLCConvertAndSaveWindowController.h"
#import "windows/logging/VLCLogWindowController.h"
#import "windows/addons/VLCAddonsWindowController.h"

#ifdef HAVE_SPARKLE
#import <Sparkle/Sparkle.h>
#endif

@interface VLCMainMenu() <NSMenuDelegate>
{
    VLCAboutWindowController *_aboutWindowController;
    VLCHelpWindowController  *_helpWindowController;
    VLCAddonsWindowController *_addonsController;
    VLCRendererMenuController *_rendererMenuController;
    VLCPlaylistController *_playlistController;
    VLCPlayerController *_playerController;
    NSTimer *_cancelRendererDiscoveryTimer;

    NSMenu *_playlistTableColumnsContextMenu;

    __strong VLCTimeSelectionPanelController *_timeSelectionPanel;
}
@end

@implementation VLCMainMenu

#pragma mark - Initialization

- (void)dealloc
{
    msg_Dbg(getIntf(), "Deinitializing main menu");
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    [self releaseRepresentedObjects:[NSApp mainMenu]];
}

- (void)awakeFromNib
{
    _timeSelectionPanel = [[VLCTimeSelectionPanelController alloc] init];
    _playlistController = [[VLCMain sharedInstance] playlistController];
    _playerController = _playlistController.playerController;

    /* check whether the user runs OSX with a RTL language */
    NSArray* languages = [NSLocale preferredLanguages];
    NSString* preferredLanguage = [languages firstObject];

    if ([NSLocale characterDirectionForLanguage:preferredLanguage] == NSLocaleLanguageDirectionRightToLeft) {
        msg_Dbg(getIntf(), "adapting interface since '%s' is a RTL language", [preferredLanguage UTF8String]);
        [_rateTextField setAlignment:NSLeftTextAlignment];
    }

    [self setRateControlsEnabled:NO];

#ifdef HAVE_SPARKLE
    [_checkForUpdate setAction:@selector(checkForUpdates:)];
    [_checkForUpdate setTarget:[SUUpdater sharedUpdater]];
#else
    [_checkForUpdate setEnabled:NO];
#endif

    NSString* keyString;
    char *key;

    /* Get ExtensionsManager */
    intf_thread_t *p_intf = getIntf();

    [self initStrings];

    key = config_GetPsz("key-quit");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_quit setKeyEquivalent: VLCKeyToString(keyString)];
    [_quit setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    // do not assign play/pause key

    key = config_GetPsz("key-stop");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_stop setKeyEquivalent: VLCKeyToString(keyString)];
    [_stop setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-prev");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_previous setKeyEquivalent: VLCKeyToString(keyString)];
    [_previous setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-next");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_next setKeyEquivalent: VLCKeyToString(keyString)];
    [_next setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-jump+short");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_fwd setKeyEquivalent: VLCKeyToString(keyString)];
    [_fwd setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-jump-short");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_bwd setKeyEquivalent: VLCKeyToString(keyString)];
    [_bwd setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-vol-up");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_vol_up setKeyEquivalent: VLCKeyToString(keyString)];
    [_vol_up setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-vol-down");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_vol_down setKeyEquivalent: VLCKeyToString(keyString)];
    [_vol_down setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-vol-mute");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_mute setKeyEquivalent: VLCKeyToString(keyString)];
    [_mute setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-toggle-fullscreen");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_fullscreenItem setKeyEquivalent: VLCKeyToString(keyString)];
    [_fullscreenItem setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-snapshot");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_snapshot setKeyEquivalent: VLCKeyToString(keyString)];
    [_snapshot setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-random");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_random setKeyEquivalent: VLCKeyToString(keyString)];
    [_random setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-zoom-half");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_half_window setKeyEquivalent: VLCKeyToString(keyString)];
    [_half_window setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-zoom-original");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_normal_window setKeyEquivalent: VLCKeyToString(keyString)];
    [_normal_window setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    key = config_GetPsz("key-zoom-double");
    keyString = [NSString stringWithFormat:@"%s", key];
    [_double_window setKeyEquivalent: VLCKeyToString(keyString)];
    [_double_window setKeyEquivalentModifierMask: VLCModifiersToCocoa(keyString)];
    FREENULL(key);

    [self setSubmenusEnabled: FALSE];

    /* configure playback / controls menu */
    self.controlsMenu.delegate = self;
    [_rendererNoneItem setState:NSOnState];
    _rendererMenuController = [[VLCRendererMenuController alloc] init];
    _rendererMenuController.rendererNoneItem = _rendererNoneItem;
    _rendererMenuController.rendererMenu = _rendererMenu;

    NSNotificationCenter *notificationCenter = [NSNotificationCenter defaultCenter];
    [notificationCenter addObserver:self
                           selector:@selector(refreshVoutDeviceMenu:)
                               name:NSApplicationDidChangeScreenParametersNotification
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updatePlaybackRate)
                               name:VLCPlayerRateChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(updateRecordState)
                               name:VLCPlayerRecordingChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playbackStateChanged:)
                               name:VLCPlayerStateChanged
                             object:nil];
    [notificationCenter addObserver:self
                           selector:@selector(playModeChanged:)
                               name:VLCPlaybackRepeatChanged
                             object:self];
    [notificationCenter addObserver:self
                           selector:@selector(playOrderChanged:)
                               name:VLCPlaybackOrderChanged
                             object:self];

    [self setupVarMenuItem:_add_intf target: (vlc_object_t *)p_intf
                             var:"intf-add" selector: @selector(toggleVar:)];

    /* setup extensions menu */
    /* Let the ExtensionsManager itself build the menu */
    VLCExtensionsManager *extMgr = [[VLCMain sharedInstance] extensionsManager];
    [extMgr buildMenu:_extensionsMenu];
    [_extensions setEnabled: ([_extensionsMenu numberOfItems] > 0)];

    // FIXME: Implement preference for autoloading extensions on mac
    if (![extMgr isLoaded] && ![extMgr cannotLoad])
        [extMgr loadExtensions];

    /* setup post-proc menu */
    NSUInteger count = (NSUInteger) [_postprocessingMenu numberOfItems];
    if (count > 0)
        [_postprocessingMenu removeAllItems];

    // FIXME: re-write the following using VLCPlayerController
    NSMenuItem *mitem;
    [_postprocessingMenu setAutoenablesItems: YES];
    [_postprocessingMenu addItemWithTitle: _NS("Disable") action:@selector(togglePostProcessing:) keyEquivalent:@""];
    mitem = [_postprocessingMenu itemAtIndex: 0];
    [mitem setTag: -1];
    [mitem setEnabled: YES];
    [mitem setTarget: self];
    for (NSUInteger x = 1; x < 7; x++) {
        [_postprocessingMenu addItemWithTitle:[NSString stringWithFormat:_NS("Level %i"), x]
                                               action:@selector(togglePostProcessing:)
                                        keyEquivalent:@""];
        mitem = [_postprocessingMenu itemAtIndex:x];
        [mitem setEnabled:YES];
        [mitem setTag:x];
        [mitem setTarget:self];
    }
    char *psz_config = config_GetPsz("video-filter");
    if (psz_config) {
        if (!strstr(psz_config, "postproc"))
            [[_postprocessingMenu itemAtIndex:0] setState:NSOnState];
        else
            [[_postprocessingMenu itemWithTag:config_GetInt("postproc-q")] setState:NSOnState];
        free(psz_config);
    } else
        [[_postprocessingMenu itemAtIndex:0] setState:NSOnState];
    [_postprocessing setEnabled: NO];

    [self refreshAudioDeviceList];

    /* setup subtitles menu */
    // Persist those variables on the playlist
    playlist_t *p_playlist = pl_Get(getIntf());
    var_Create(p_playlist, "freetype-color", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create(p_playlist, "freetype-background-opacity", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create(p_playlist, "freetype-background-color", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);
    var_Create(p_playlist, "freetype-outline-thickness", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT);

    [self setupMenu: _subtitle_textcolorMenu withIntList:"freetype-color" andSelector:@selector(switchSubtitleOption:)];
    [_subtitle_bgopacity_sld setIntegerValue: config_GetInt("freetype-background-opacity")];
    [self setupMenu: _subtitle_bgcolorMenu withIntList:"freetype-background-color" andSelector:@selector(switchSubtitleOption:)];
    [self setupMenu: _subtitle_outlinethicknessMenu withIntList:"freetype-outline-thickness" andSelector:@selector(switchSubtitleOption:)];

    /* Build size menu based on different scale factors */
    struct {
        const char *const name;
        int scaleValue;
    } scaleValues[] = {
        { N_("Smaller"), 50},
        { N_("Small"),   75},
        { N_("Normal"), 100},
        { N_("Large"),  125},
        { N_("Larger"), 150},
        { NULL, 0 }
    };

    for(int i = 0; scaleValues[i].name; i++) {
        NSMenuItem *menuItem = [_subtitle_sizeMenu addItemWithTitle: _NS(scaleValues[i].name) action:@selector(switchSubtitleSize:) keyEquivalent:@""];
        [menuItem setTag:scaleValues[i].scaleValue];
        [menuItem setTarget: self];
    }
}

- (void)setupMenu: (NSMenu*)menu withIntList: (char *)psz_name andSelector:(SEL)selector
{
    module_config_t *p_item;

    [menu removeAllItems];
    p_item = config_FindConfig(psz_name);

    if (!p_item) {
        msg_Err(getIntf(), "couldn't create menu int list for item '%s' as it does not exist", psz_name);
        return;
    }

    for (int i = 0; i < p_item->list_count; i++) {
        NSMenuItem *mi;
        if (p_item->list_text != NULL)
            mi = [[NSMenuItem alloc] initWithTitle: _NS(p_item->list_text[i]) action:NULL keyEquivalent: @""];
        else if (p_item->list.i[i])
            mi = [[NSMenuItem alloc] initWithTitle: [NSString stringWithFormat: @"%d", p_item->list.i[i]] action:NULL keyEquivalent: @""];
        else {
            msg_Err(getIntf(), "item %d of pref %s failed to be created", i, psz_name);
            continue;
        }

        [mi setTarget:self];
        [mi setAction:selector];
        [mi setTag:p_item->list.i[i]];
        [mi setRepresentedObject:toNSStr(psz_name)];
        [menu addItem:mi];
        if (p_item->value.i == p_item->list.i[i])
            [mi setState:NSOnState];
    }
}

- (void)initStrings
{
    /* main menu */
    [_about setTitle: [_NS("About VLC media player") stringByAppendingString: @"..."]];
    [_checkForUpdate setTitle: _NS("Check for Update...")];
    [_prefs setTitle: _NS("Preferences...")];
    [_extensions setTitle: _NS("Extensions")];
    [_extensionsMenu setTitle: _NS("Extensions")];
    [_addonManager setTitle: _NS("Addons Manager")];
    [_add_intf setTitle: _NS("Add Interface")];
    [_add_intfMenu setTitle: _NS("Add Interface")];
    [_services setTitle: _NS("Services")];
    [_hide setTitle: _NS("Hide VLC")];
    [_hide_others setTitle: _NS("Hide Others")];
    [_show_all setTitle: _NS("Show All")];
    [_quit setTitle: _NS("Quit VLC")];

    /* this special case is needed to due to archiac legacy translations of the File menu
     * on the Mac to the German translation which resulted in 'Ablage' instead of 'Datei'.
     * This remains until the present day and does not affect the Windows world. */
    [_fileMenu setTitle: _ANS("1:File")];
    [_open_generic setTitle: _NS("Advanced Open File...")];
    [_open_file setTitle: _NS("Open File...")];
    [_open_disc setTitle: _NS("Open Disc...")];
    [_open_net setTitle: _NS("Open Network...")];
    [_open_capture setTitle: _NS("Open Capture Device...")];
    [_open_recent setTitle: _NS("Open Recent")];
    [_close_window setTitle: _NS("Close Window")];
    [_convertandsave setTitle: _NS("Convert / Stream...")];
    [_save_playlist setTitle: _NS("Save Playlist...")];
    [_revealInFinder setTitle: _NS("Reveal in Finder")];

    [_editMenu setTitle: _NS("Edit")];
    [_cutItem setTitle: _NS("Cut")];
    [_mcopyItem setTitle: _NS("Copy")];
    [_pasteItem setTitle: _NS("Paste")];
    [_clearItem setTitle: _NS("Delete")];
    [_select_all setTitle: _NS("Select All")];
    [_findItem setTitle: _NS("Find")];

    [_viewMenu setTitle: _NS("View")];
    [_toggleJumpButtons setTitle: _NS("Show Previous & Next Buttons")];
    [_toggleJumpButtons setState: var_InheritBool(getIntf(), "macosx-show-playback-buttons")];
    [_togglePlaymodeButtons setTitle: _NS("Show Shuffle & Repeat Buttons")];
    [_togglePlaymodeButtons setState: var_InheritBool(getIntf(), "macosx-show-playmode-buttons")];
    [_toggleEffectsButton setTitle: _NS("Show Audio Effects Button")];
    [_toggleEffectsButton setState: var_InheritBool(getIntf(), "macosx-show-effects-button")];
    [_toggleSidebar setTitle: _NS("Show Sidebar")];
    [_playlistTableColumns setTitle: _NS("Playlist Table Columns")];

    [_controlsMenu setTitle: _NS("Playback")];
    [_play setTitle: _NS("Play")];
    [_stop setTitle: _NS("Stop")];
    [_record setTitle: _NS("Record")];
    [_rate_view setAutoresizingMask:NSViewWidthSizable];
    [_rate setView: _rate_view];
    [_rateLabel setStringValue: _NS("Playback Speed")];
    [_rate_slowerLabel setStringValue: _NS("Slower")];
    [_rate_normalLabel setStringValue: _NS("Normal")];
    [_rate_fasterLabel setStringValue: _NS("Faster")];
    [_trackSynchronization setTitle: _NS("Track Synchronization")];
    [_previous setTitle: _NS("Previous")];
    [_next setTitle: _NS("Next")];
    [_random setTitle: _NS("Random")];
    [_repeat setTitle: _NS("Repeat One")];
    [_loop setTitle: _NS("Repeat All")];
    [_AtoBloop setTitle: _NS("A→B Loop")];
    [_quitAfterPB setTitle: _NS("Quit after Playback")];
    [_fwd setTitle: _NS("Step Forward")];
    [_bwd setTitle: _NS("Step Backward")];
    [_jumpToTime setTitle: _NS("Jump to Time")];
    [_rendererMenuItem setTitle:_NS("Renderer")];
    [_rendererNoneItem setTitle:_NS("No renderer")];
    [_program setTitle: _NS("Program")];
    [_programMenu setTitle: _NS("Program")];
    [_title setTitle: _NS("Title")];
    [_titleMenu setTitle: _NS("Title")];
    [_chapter setTitle: _NS("Chapter")];
    [_chapterMenu setTitle: _NS("Chapter")];

    [_audioMenu setTitle: _NS("Audio")];
    [_vol_up setTitle: _NS("Increase Volume")];
    [_vol_down setTitle: _NS("Decrease Volume")];
    [_mute setTitle: _NS("Mute")];
    [_audiotrack setTitle: _NS("Audio Track")];
    [_audiotrackMenu setTitle: _NS("Audio Track")];
    [_channels setTitle: _NS("Stereo audio mode")];
    [_channelsMenu setTitle: _NS("Stereo audio mode")];
    [_audioDevice setTitle: _NS("Audio Device")];
    [_audioDeviceMenu setTitle: _NS("Audio Device")];
    [_visual setTitle: _NS("Visualizations")];
    [_visualMenu setTitle: _NS("Visualizations")];

    [_videoMenu setTitle: _NS("Video")];
    [_half_window setTitle: _NS("Half Size")];
    [_normal_window setTitle: _NS("Normal Size")];
    [_double_window setTitle: _NS("Double Size")];
    [_fittoscreen setTitle: _NS("Fit to Screen")];
    [_fullscreenItem setTitle: _NS("Fullscreen")];
    [_floatontop setTitle: _NS("Float on Top")];
    [_snapshot setTitle: _NS("Snapshot")];
    [_videotrack setTitle: _NS("Video Track")];
    [_videotrackMenu setTitle: _NS("Video Track")];
    [_aspect_ratio setTitle: _NS("Aspect ratio")];
    [_aspect_ratioMenu setTitle: _NS("Aspect ratio")];
    [_crop setTitle: _NS("Crop")];
    [_cropMenu setTitle: _NS("Crop")];
    [_screen setTitle: _NS("Fullscreen Video Device")];
    [_screenMenu setTitle: _NS("Fullscreen Video Device")];
    [_deinterlace setTitle: _NS("Deinterlace")];
    [_deinterlaceMenu setTitle: _NS("Deinterlace")];
    [_deinterlace_mode setTitle: _NS("Deinterlace mode")];
    [_deinterlace_modeMenu setTitle: _NS("Deinterlace mode")];
    [_postprocessing setTitle: _NS("Post processing")];
    [_postprocessingMenu setTitle: _NS("Post processing")];

    [_subtitlesMenu setTitle:_NS("Subtitles")];
    [_openSubtitleFile setTitle: _NS("Add Subtitle File...")];
    [_subtitle_track setTitle: _NS("Subtitles Track")];
    [_subtitle_tracksMenu setTitle: _NS("Subtitles Track")];
    [_subtitle_size setTitle: _NS("Text Size")];
    [_subtitle_textcolor setTitle: _NS("Text Color")];
    [_subtitle_outlinethickness setTitle: _NS("Outline Thickness")];

    // Autoresizing with constraints does not work on 10.7,
    // translate autoresizing mask to constriaints for now
    [_subtitle_bgopacity_view setAutoresizingMask:NSViewWidthSizable];
    [_subtitle_bgopacity setView: _subtitle_bgopacity_view];
    [_subtitle_bgopacityLabel setStringValue: _NS("Background Opacity")];
    [_subtitle_bgopacityLabel_gray setStringValue: _NS("Background Opacity")];
    [_subtitle_bgcolor setTitle: _NS("Background Color")];
    [_teletext setTitle: _NS("Teletext")];
    [_teletext_transparent setTitle: _NS("Transparent")];
    [_teletext_index setTitle: _NS("Index")];
    [_teletext_red setTitle: _NS("Red")];
    [_teletext_green setTitle: _NS("Green")];
    [_teletext_yellow setTitle: _NS("Yellow")];
    [_teletext_blue setTitle: _NS("Blue")];

    [_windowMenu setTitle: _NS("Window")];
    [_minimize setTitle: _NS("Minimize")];
    [_zoom_window setTitle: _NS("Zoom")];
    [_player setTitle: _NS("Player...")];
    [_controller setTitle: _NS("Main Window...")];
    [_audioeffects setTitle: _NS("Audio Effects...")];
    [_videoeffects setTitle: _NS("Video Effects...")];
    [_bookmarks setTitle: _NS("Bookmarks...")];
    [_playlist setTitle: _NS("Playlist...")];
    [_info setTitle: _NS("Media Information...")];
    [_messages setTitle: _NS("Messages...")];
    [_errorsAndWarnings setTitle: _NS("Errors and Warnings...")];

    [_bring_atf setTitle: _NS("Bring All to Front")];

    [_helpMenu setTitle: _NS("Help")];
    [_help setTitle: _NS("VLC media player Help...")];
    [_license setTitle: _NS("License")];
    [_documentation setTitle: _NS("Online Documentation...")];
    [_website setTitle: _NS("VideoLAN Website...")];
    [_donation setTitle: _NS("Make a donation...")];
    [_forum setTitle: _NS("Online Forum...")];

    /* dock menu */
    [_dockMenuplay setTitle: _NS("Play")];
    [_dockMenustop setTitle: _NS("Stop")];
    [_dockMenunext setTitle: _NS("Next")];
    [_dockMenuprevious setTitle: _NS("Previous")];
    [_dockMenumute setTitle: _NS("Mute")];

    /* vout menu */
    [_voutMenuplay setTitle: _NS("Play")];
    [_voutMenustop setTitle: _NS("Stop")];
    [_voutMenuprev setTitle: _NS("Previous")];
    [_voutMenunext setTitle: _NS("Next")];
    [_voutMenuvolup setTitle: _NS("Volume Up")];
    [_voutMenuvoldown setTitle: _NS("Volume Down")];
    [_voutMenumute setTitle: _NS("Mute")];
    [_voutMenufullscreen setTitle: _NS("Fullscreen")];
    [_voutMenusnapshot setTitle: _NS("Snapshot")];
}

#pragma mark - Termination

- (void)releaseRepresentedObjects:(NSMenu *)the_menu
{
    NSArray *menuitems_array = [the_menu itemArray];
    NSUInteger menuItemCount = [menuitems_array count];
    for (NSUInteger i=0; i < menuItemCount; i++) {
        NSMenuItem *one_item = [menuitems_array objectAtIndex:i];
        if ([one_item hasSubmenu])
            [self releaseRepresentedObjects: [one_item submenu]];

        [one_item setRepresentedObject:NULL];
    }
}

#pragma mark - Interface update

- (void)setupMenus
{
    playlist_t *p_playlist = pl_Get(getIntf());
    input_thread_t *p_input = playlist_CurrentInput(p_playlist);
    if (p_input != NULL) {
        [self setupVarMenuItem:_program target: (vlc_object_t *)p_input
                                 var:"program" selector: @selector(toggleVar:)];

        [self setupVarMenuItem:_title target: (vlc_object_t *)p_input
                                 var:"title" selector: @selector(toggleVar:)];

        [self setupVarMenuItem:_chapter target: (vlc_object_t *)p_input
                                 var:"chapter" selector: @selector(toggleVar:)];

        [self setupVarMenuItem:_audiotrack target: (vlc_object_t *)p_input
                                 var:"audio-es" selector: @selector(toggleVar:)];

        [self setupVarMenuItem:_videotrack target: (vlc_object_t *)p_input
                                 var:"video-es" selector: @selector(toggleVar:)];

        [self setupVarMenuItem:_subtitle_track target: (vlc_object_t *)p_input
                                 var:"spu-es" selector: @selector(toggleVar:)];

        audio_output_t *p_aout = [_playerController mainAudioOutput];
        if (p_aout != NULL) {
            [self setupVarMenuItem:_channels target: (vlc_object_t *)p_aout
                                     var:"stereo-mode" selector: @selector(toggleVar:)];

            [self setupVarMenuItem:_visual target: (vlc_object_t *)p_aout
                                     var:"visual" selector: @selector(toggleVar:)];
            aout_Release(p_aout);
        }

        vout_thread_t *p_vout = [_playerController videoOutputThreadForKeyWindow];

        if (p_vout != NULL) {
            [self setupVarMenuItem:_aspect_ratio target: (vlc_object_t *)p_vout
                                     var:"aspect-ratio" selector: @selector(toggleVar:)];

            [self setupVarMenuItem:_crop target: (vlc_object_t *) p_vout
                                     var:"crop" selector: @selector(toggleVar:)];

            [self setupVarMenuItem:_deinterlace target: (vlc_object_t *)p_vout
                                     var:"deinterlace" selector: @selector(toggleVar:)];

            [self setupVarMenuItem:_deinterlace_mode target: (vlc_object_t *)p_vout
                                     var:"deinterlace-mode" selector: @selector(toggleVar:)];

            vout_Release(p_vout);

            [self refreshVoutDeviceMenu:nil];
        }
        [_postprocessing setEnabled:YES];
        input_Release(p_input);
    } else {
        [_postprocessing setEnabled:NO];
    }
}

- (void)refreshVoutDeviceMenu:(NSNotification *)notification
{
    NSUInteger count = (NSUInteger) [_screenMenu numberOfItems];
    NSMenu *submenu = _screenMenu;
    if (count > 0)
        [submenu removeAllItems];

    NSArray *screens = [NSScreen screens];
    NSMenuItem *mitem;
    count = [screens count];
    [_screen setEnabled: YES];
    [submenu addItemWithTitle: _NS("Default") action:@selector(toggleFullscreenDevice:) keyEquivalent:@""];
    mitem = [submenu itemAtIndex: 0];
    [mitem setTag: 0];
    [mitem setEnabled: YES];
    [mitem setTarget: self];
    NSRect s_rect;
    for (NSUInteger i = 0; i < count; i++) {
        s_rect = [[screens objectAtIndex:i] frame];
        [submenu addItemWithTitle: [NSString stringWithFormat: @"%@ %li (%ix%i)", _NS("Screen"), i+1,
                                      (int)s_rect.size.width, (int)s_rect.size.height] action:@selector(toggleFullscreenDevice:) keyEquivalent:@""];
        mitem = [submenu itemAtIndex:i+1];
        [mitem setTag: (int)[[screens objectAtIndex:i] displayID]];
        [mitem setEnabled: YES];
        [mitem setTarget: self];
    }
    [[submenu itemWithTag: var_InheritInteger(getIntf(), "macosx-vdev")] setState: NSOnState];
}

- (void)setSubmenusEnabled:(BOOL)b_enabled
{
    [_program setEnabled: b_enabled];
    [_title setEnabled: b_enabled];
    [_chapter setEnabled: b_enabled];
    [_audiotrack setEnabled: b_enabled];
    [_visual setEnabled: b_enabled];
    [_videotrack setEnabled: b_enabled];
    [_subtitle_track setEnabled: b_enabled];
    [_channels setEnabled: b_enabled];
    [_deinterlace setEnabled: b_enabled];
    [_deinterlace_mode setEnabled: b_enabled];
    [_screen setEnabled: b_enabled];
    [_aspect_ratio setEnabled: b_enabled];
    [_crop setEnabled: b_enabled];
}

- (void)setSubtitleMenuEnabled:(BOOL)b_enabled
{
    [_openSubtitleFile setEnabled: b_enabled];
    if (b_enabled) {
        [_subtitle_bgopacityLabel_gray setHidden: YES];
        [_subtitle_bgopacityLabel setHidden: NO];
    } else {
        [_subtitle_bgopacityLabel_gray setHidden: NO];
        [_subtitle_bgopacityLabel setHidden: YES];
    }
    [_subtitle_bgopacity_sld setEnabled: b_enabled];
    [_teletext setEnabled:_playerController.teletextMenuAvailable];
}

- (void)setRateControlsEnabled:(BOOL)b_enabled
{
    [_rate_sld setEnabled: b_enabled];
    [self updatePlaybackRate];

    NSColor *color = b_enabled ? [NSColor controlTextColor] : [NSColor disabledControlTextColor];

    [_rateLabel setTextColor:color];
    [_rate_slowerLabel setTextColor:color];
    [_rate_normalLabel setTextColor:color];
    [_rate_fasterLabel setTextColor:color];
    [_rateTextField setTextColor:color];

    [self setSubtitleMenuEnabled: b_enabled];
}

#pragma mark - View

- (IBAction)toggleEffectsButton:(id)sender
{
    BOOL b_value = !var_InheritBool(getIntf(), "macosx-show-effects-button");
    config_PutInt("macosx-show-effects-button", b_value);
    [(VLCMainWindowControlsBar *)[[[VLCMain sharedInstance] mainWindow] controlsBar] toggleEffectsButton];
    [_toggleEffectsButton setState: b_value];
}

- (IBAction)toggleJumpButtons:(id)sender
{
    BOOL b_value = !var_InheritBool(getIntf(), "macosx-show-playback-buttons");
    config_PutInt("macosx-show-playback-buttons", b_value);

    [(VLCMainWindowControlsBar *)[[[VLCMain sharedInstance] mainWindow] controlsBar] toggleJumpButtons];
    [[[VLCMain sharedInstance] voutProvider] updateWindowsUsingBlock:^(VLCVideoWindowCommon *window) {
        [[window controlsBar] toggleForwardBackwardMode: b_value];
    }];

    [_toggleJumpButtons setState: b_value];
}

- (IBAction)togglePlaymodeButtons:(id)sender
{
    BOOL b_value = !var_InheritBool(getIntf(), "macosx-show-playmode-buttons");
    config_PutInt("macosx-show-playmode-buttons", b_value);
    [(VLCMainWindowControlsBar *)[[[VLCMain sharedInstance] mainWindow] controlsBar] togglePlaymodeButtons];
    [_togglePlaymodeButtons setState: b_value];
}

- (IBAction)toggleSidebar:(id)sender
{
    [[[VLCMain sharedInstance] mainWindow] toggleLeftSubSplitView];
}

- (void)updateSidebarMenuItem:(BOOL)show;
{
    [_toggleSidebar setState:show];
}

#pragma mark - Playback

- (IBAction)play:(id)sender
{
    [[VLCCoreInteraction sharedInstance] playOrPause];
}

- (IBAction)stop:(id)sender
{
    [[VLCCoreInteraction sharedInstance] stop];
}

- (IBAction)prev:(id)sender
{
    [[VLCCoreInteraction sharedInstance] previous];
}

- (IBAction)next:(id)sender
{
    [[VLCCoreInteraction sharedInstance] next];
}

- (IBAction)random:(id)sender
{
    [[VLCCoreInteraction sharedInstance] shuffle];
}

- (IBAction)repeat:(id)sender
{
    if (_playlistController.playbackRepeat == VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT) {
        _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
    } else {
        _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT;
    }
}

- (IBAction)loop:(id)sender
{
    if (_playlistController.playbackRepeat == VLC_PLAYLIST_PLAYBACK_REPEAT_ALL) {
        _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;
    } else {
        _playlistController.playbackRepeat = VLC_PLAYLIST_PLAYBACK_REPEAT_ALL;
    }
}

- (IBAction)forward:(id)sender
{
    [_playerController jumpForwardShort];
}

- (IBAction)backward:(id)sender
{
    [_playerController jumpBackwardShort];
}

- (IBAction)volumeUp:(id)sender
{
    [_playerController incrementVolume];
}

- (IBAction)volumeDown:(id)sender
{
    [_playerController decrementVolume];
}

- (IBAction)mute:(id)sender
{
    [_playerController toggleMute];
}

- (void)lockVideosAspectRatio:(id)sender
{
    // FIXME: re-write the following using VLCPlayerController
    [[VLCCoreInteraction sharedInstance] setAspectRatioIsLocked: ![sender state]];
    [sender setState: [[VLCCoreInteraction sharedInstance] aspectRatioIsLocked]];
}

- (IBAction)quitAfterPlayback:(id)sender
{
    _playerController.actionAfterStop = VLC_PLAYER_MEDIA_STOPPED_EXIT;
}

- (IBAction)toggleRecord:(id)sender
{
    [_playerController toggleRecord];
}

- (void)updateRecordState
{
    [_record setState:_playerController.enableRecording];
}

- (IBAction)setPlaybackRate:(id)sender
{
    double speed =  pow(2, (double)[_rate_sld intValue] / 17);
    _playerController.playbackRate = speed;
    [_rateTextField setStringValue: [NSString stringWithFormat:@"%.2fx", speed]];
}

- (void)updatePlaybackRate
{
    double playbackRate = _playerController.playbackRate;
    double value = 17 * log(playbackRate) / log(2.);
    int intValue = (int) ((value > 0) ? value + .5 : value - .5);

    if (intValue < -34)
        intValue = -34;
    else if (intValue > 34)
        intValue = 34;

    [_rateTextField setStringValue: [NSString stringWithFormat:@"%.2fx", playbackRate]];
    [_rate_sld setIntValue: intValue];
}

- (IBAction)toggleAtoBloop:(id)sender
{
    // re-write the following using VLCPlayerController
    [[VLCCoreInteraction sharedInstance] setAtoB];
}

- (IBAction)goToSpecificTime:(id)sender
{
    vlc_tick_t length = _playerController.length;
    [_timeSelectionPanel setMaxTime:(int)SEC_FROM_VLC_TICK(length)];
    vlc_tick_t time = _playerController.time;
    [_timeSelectionPanel setPosition: (int)SEC_FROM_VLC_TICK(time)];
    [_timeSelectionPanel runModalForWindow:[NSApp mainWindow]
                         completionHandler:^(NSInteger returnCode, int64_t returnTime) {
                             if (returnCode != NSModalResponseOK)
                                 return;
                             [self->_playerController setTimePrecise:vlc_tick_from_sec(returnTime)];
                         }];
}

- (IBAction)selectRenderer:(id)sender
{
    [_rendererMenuController selectRenderer:sender];
}

#pragma mark - audio menu

- (void)refreshAudioDeviceList
{
    char **ids, **names;
    char *currentDevice;

    [_audioDeviceMenu removeAllItems];

    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (!p_aout)
        return;

    int n = aout_DevicesList(p_aout, &ids, &names);
    if (n == -1) {
        aout_Release(p_aout);
        return;
    }

    currentDevice = aout_DeviceGet(p_aout);
    NSMenuItem *_tmp;

    for (NSUInteger x = 0; x < n; x++) {
        _tmp = [_audioDeviceMenu addItemWithTitle:toNSStr(names[x]) action:@selector(toggleAudioDevice:) keyEquivalent:@""];
        [_tmp setTarget:self];
        [_tmp setTag:[[NSString stringWithFormat:@"%s", ids[x]] intValue]];
    }
    aout_Release(p_aout);

    [[_audioDeviceMenu itemWithTag:[[NSString stringWithFormat:@"%s", currentDevice] intValue]] setState:NSOnState];

    free(currentDevice);

    for (NSUInteger x = 0; x < n; x++) {
        free(ids[x]);
        free(names[x]);
    }
    free(ids);
    free(names);

    [_audioDeviceMenu setAutoenablesItems:YES];
    [_audioDevice setEnabled:YES];
}

- (void)toggleAudioDevice:(id)sender
{
    audio_output_t *p_aout = [_playerController mainAudioOutput];
    if (!p_aout)
        return;

    int returnValue = 0;

    if ([sender tag] > 0)
        returnValue = aout_DeviceSet(p_aout, [[NSString stringWithFormat:@"%li", [sender tag]] UTF8String]);
    else
        returnValue = aout_DeviceSet(p_aout, NULL);

    if (returnValue != 0)
        msg_Warn(getIntf(), "failed to set audio device %li", [sender tag]);

    aout_Release(p_aout);
    [self refreshAudioDeviceList];
}

#pragma mark - video menu

- (IBAction)toggleFullscreen:(id)sender
{
    [_playerController toggleFullscreen];
}

- (IBAction)resizeVideoWindow:(id)sender
{
    vout_thread_t *p_vout = [_playerController videoOutputThreadForKeyWindow];
    if (p_vout) {
        if (sender == _half_window)
            var_SetFloat(p_vout, "zoom", 0.5);
        else if (sender == _normal_window)
            var_SetFloat(p_vout, "zoom", 1.0);
        else if (sender == _double_window)
            var_SetFloat(p_vout, "zoom", 2.0);
        else
        {
            [[NSApp keyWindow] performZoom:sender];
        }
        vout_Release(p_vout);
    }
}

- (IBAction)floatOnTop:(id)sender
{
    // FIXME re-write using VLCPlayerController
    input_thread_t *p_input = pl_CurrentInput(getIntf());
    if (p_input) {
        vout_thread_t *p_vout = [_playerController videoOutputThreadForKeyWindow];
        if (p_vout) {
            BOOL b_fs = var_ToggleBool(p_vout, "video-on-top");
            var_SetBool(pl_Get(getIntf()), "video-on-top", b_fs);

            vout_Release(p_vout);
        }
        input_Release(p_input);
    }
}

- (IBAction)createVideoSnapshot:(id)sender
{
    [_playerController takeSnapshot];
}

- (void)_disablePostProcessing
{
    // FIXME re-write using VLCPlayerController
    [VLCVideoFilterHelper setVideoFilter:"postproc" on:false];
}

- (void)_enablePostProcessing
{
    // FIXME re-write using VLCPlayerController
    [VLCVideoFilterHelper setVideoFilter:"postproc" on:true];
}

- (void)togglePostProcessing:(id)sender
{
    // FIXME re-write using VLCPlayerController
    NSInteger count = [_postprocessingMenu numberOfItems];
    for (NSUInteger x = 0; x < count; x++)
        [[_postprocessingMenu itemAtIndex:x] setState:NSOffState];

    if ([sender tag] == -1) {
        [self _disablePostProcessing];
        [sender setState:NSOnState];
    } else {
        [self _enablePostProcessing];
        [sender setState:NSOnState];

        [VLCVideoFilterHelper setVideoFilterProperty:"postproc-q" forFilter:"postproc" withValue:(vlc_value_t){ .i_int = [sender tag] }];
    }
}

- (void)toggleFullscreenDevice:(id)sender
{
    config_PutInt("macosx-vdev", [sender tag]);
    [self refreshVoutDeviceMenu: nil];
}

#pragma mark - Subtitles Menu

- (IBAction)addSubtitleFile:(id)sender
{
    NSInteger i_returnValue = 0;
    input_thread_t *p_input = pl_CurrentInput(getIntf());
    if (!p_input)
        return;

    input_item_t *p_item = input_GetItem(p_input);
    if (!p_item) {
        input_Release(p_input);
        return;
    }

    char *path = input_item_GetURI(p_item);

    if (!path)
        path = strdup("");

    NSOpenPanel *openPanel = [NSOpenPanel openPanel];
    [openPanel setCanChooseFiles: YES];
    [openPanel setCanChooseDirectories: NO];
    [openPanel setAllowsMultipleSelection: YES];

    [openPanel setAllowedFileTypes: [NSArray arrayWithObjects:@"cdg",@"idx",@"srt",@"sub",@"utf",@"ass",@"ssa",@"aqt",@"jss",@"psb",@"rt",@"smi",@"txt",@"smil",@"stl",@"usf",@"dks",@"pjs",@"mpl2",@"mks",@"vtt",@"ttml",@"dfxp",nil]];

    NSURL *url = [NSURL URLWithString:[toNSStr(path) stringByExpandingTildeInPath]];
    url = [url URLByDeletingLastPathComponent];
    [openPanel setDirectoryURL: url];
    free(path);
    input_Release(p_input);

    i_returnValue = [openPanel runModal];

    // FIXME: this cannot work anymore
    if (i_returnValue == NSModalResponseOK)
        [[VLCCoreInteraction sharedInstance] addSubtitlesToCurrentInput:[openPanel URLs]];
}

- (void)switchSubtitleSize:(id)sender
{
    _playerController.subtitleTextScalingFactor = (unsigned int)[sender tag];
}

- (void)switchSubtitleOption:(id)sender
{
    NSInteger intValue = [sender tag];
    NSString *representedObject = [sender representedObject];

    var_SetInteger(pl_Get(getIntf()), [representedObject UTF8String], intValue);

    NSMenu *menu = [sender menu];
    NSUInteger count = (NSUInteger) [menu numberOfItems];
    for (NSUInteger x = 0; x < count; x++)
        [[menu itemAtIndex:x] setState:NSOffState];
    [[menu itemWithTag:intValue] setState:NSOnState];
}

- (IBAction)switchSubtitleBackgroundOpacity:(id)sender
{
    var_SetInteger(pl_Get(getIntf()), "freetype-background-opacity", [sender intValue]);
}

- (IBAction)telxTransparent:(id)sender
{
    _playerController.teletextTransparent = !_playerController.teletextTransparent;
}

- (IBAction)telxNavLink:(id)sender
{
    unsigned int page = 0;

    if ([[sender title] isEqualToString: _NS("Index")])
        page = VLC_PLAYER_TELETEXT_KEY_INDEX;
    else if ([[sender title] isEqualToString: _NS("Red")])
        page = VLC_PLAYER_TELETEXT_KEY_RED;
    else if ([[sender title] isEqualToString: _NS("Green")])
        page = VLC_PLAYER_TELETEXT_KEY_GREEN;
    else if ([[sender title] isEqualToString: _NS("Yellow")])
        page = VLC_PLAYER_TELETEXT_KEY_YELLOW;
    else if ([[sender title] isEqualToString: _NS("Blue")])
        page = VLC_PLAYER_TELETEXT_KEY_BLUE;

    _playerController.teletextPage = page;
}

#pragma mark - Panels

- (IBAction)intfOpenFile:(id)sender
{
    [[[VLCMain sharedInstance] open] openFileWithAction:^(NSArray *files) {
        [self->_playlistController addPlaylistItems:files];
    }];
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

- (IBAction)savePlaylist:(id)sender
{
    playlist_t *p_playlist = pl_Get(getIntf());

    NSSavePanel *savePanel = [NSSavePanel savePanel];
    NSString * name = [NSString stringWithFormat: @"%@", _NS("Untitled")];

    static dispatch_once_t once;
    dispatch_once(&once, ^{
        [[NSBundle mainBundle] loadNibNamed:@"PlaylistAccessoryView" owner:self topLevelObjects:nil];
    });

    [_playlistSaveAccessoryText setStringValue: _NS("File Format:")];
    [[_playlistSaveAccessoryPopup itemAtIndex:0] setTitle: _NS("Extended M3U")];
    [[_playlistSaveAccessoryPopup itemAtIndex:1] setTitle: _NS("XML Shareable Playlist Format (XSPF)")];
    [[_playlistSaveAccessoryPopup itemAtIndex:2] setTitle: _NS("HTML playlist")];

    [savePanel setTitle: _NS("Save Playlist")];
    [savePanel setPrompt: _NS("Save")];
    [savePanel setAccessoryView: _playlistSaveAccessoryView];
    [savePanel setNameFieldStringValue: name];

    if ([savePanel runModal] == NSFileHandlingPanelOKButton) {
        NSString *filename = [[savePanel URL] path];
        NSString *ext;
        char const* psz_module;

        switch ([_playlistSaveAccessoryPopup indexOfSelectedItem]) {
            case 0: psz_module = "export-m3u";
                    ext = @"m3u";
                    break;
            case 1: psz_module = "export-xspf";
                    ext = @"xspf";
                    break;
            case 2: psz_module = "export-html";
                    ext = @"html";
                    break;
            default:
                    return;
        }

        NSString *actualFilename = filename;

        if ([[filename pathExtension] caseInsensitiveCompare:ext] != NSOrderedSame)
            actualFilename = [NSString stringWithFormat: @"%@.%@", filename, ext];

        // FIXME: This will always export an empty playlist unless we do something about it
        playlist_Export(p_playlist,
                        [actualFilename fileSystemRepresentation],
                        psz_module);
    }
}

- (IBAction)showConvertAndSave:(id)sender
{
    [[[VLCMain sharedInstance] convertAndSaveWindow] showWindow:self];
}

- (IBAction)showVideoEffects:(id)sender
{
    [[[VLCMain sharedInstance] videoEffectsPanel] toggleWindow:sender];
}

- (IBAction)showTrackSynchronization:(id)sender
{
    [[[VLCMain sharedInstance] trackSyncPanel] toggleWindow:sender];
}

- (IBAction)showAudioEffects:(id)sender
{
    [[[VLCMain sharedInstance] audioEffectsPanel] toggleWindow:sender];
}

- (IBAction)showBookmarks:(id)sender
{
    [[[VLCMain sharedInstance] bookmarks] toggleWindow:sender];
}

- (IBAction)showPreferences:(id)sender
{
    NSInteger i_level = [[[VLCMain sharedInstance] voutProvider] currentStatusWindowLevel];
    [[[VLCMain sharedInstance] simplePreferences] showSimplePrefsWithLevel:i_level];
}

- (IBAction)openAddonManager:(id)sender
{
    if (!_addonsController)
        _addonsController = [[VLCAddonsWindowController alloc] init];

    [_addonsController showWindow:self];
}

- (IBAction)showErrorsAndWarnings:(id)sender
{
    [[[[VLCMain sharedInstance] coreDialogProvider] errorPanel] showWindow:self];
}

- (IBAction)showMessagesPanel:(id)showMessagesPanel
{
    [[[VLCMain sharedInstance] debugMsgPanel] showWindow:self];
}

- (IBAction)showMainWindow:(id)sender
{
    [[[VLCMain sharedInstance] mainWindow] makeKeyAndOrderFront:sender];
}

- (IBAction)showPlaylist:(id)sender
{
    [[[[VLCMain sharedInstance] libraryWindowController] window] makeKeyAndOrderFront:sender];
}

#pragma mark - Help and Docs

- (IBAction)showAbout:(id)sender
{
    if (!_aboutWindowController)
        _aboutWindowController = [[VLCAboutWindowController alloc] init];

    [_aboutWindowController showAbout];
}

- (IBAction)showLicense:(id)sender
{
    if (!_aboutWindowController)
        _aboutWindowController = [[VLCAboutWindowController alloc] init];

    [_aboutWindowController showGPL];
}

- (IBAction)showHelp:(id)sender
{
    if (!_helpWindowController)
        _helpWindowController = [[VLCHelpWindowController alloc] init];

    [_helpWindowController showHelp];
}

- (IBAction)openDocumentation:(id)sender
{
    NSURL *url = [NSURL URLWithString: @"http://www.videolan.org/doc/"];

    [[NSWorkspace sharedWorkspace] openURL: url];
}

- (IBAction)openWebsite:(id)sender
{
    NSURL *url = [NSURL URLWithString: @"http://www.videolan.org/"];

    [[NSWorkspace sharedWorkspace] openURL: url];
}

- (IBAction)openForum:(id)sender
{
    NSURL *url = [NSURL URLWithString: @"http://forum.videolan.org/"];

    [[NSWorkspace sharedWorkspace] openURL: url];
}

- (IBAction)openDonate:(id)sender
{
    NSURL *url = [NSURL URLWithString: @"http://www.videolan.org/contribute.html#paypal"];

    [[NSWorkspace sharedWorkspace] openURL: url];
}

- (IBAction)showInformationPanel:(id)sender
{
    [[[VLCMain sharedInstance] currentMediaInfoPanel] toggleWindow:sender];
}

#pragma mark - playback state

- (void)playbackStateChanged:(NSNotification *)aNotification
{
    enum vlc_player_state playerState = [_playlistController playerController].playerState;
    if (playerState == VLC_PLAYER_STATE_PLAYING) {
        [self setPause];
    } else {
        [self setPlay];
    }
}

- (void)playModeChanged:(NSNotification *)aNotification
{
    enum vlc_playlist_playback_repeat repeatState = _playlistController.playbackRepeat;
    switch (repeatState) {
        case VLC_PLAYLIST_PLAYBACK_REPEAT_ALL:
            [self setRepeatAll];
            break;

        case VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT:
            [self setRepeatOne];
            break;

        default:
            [self setRepeatOff];
            break;
    }
}

- (void)playOrderChanged:(NSNotification *)aNotification
{
    [_random setState:_playlistController.playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM];
}

- (void)setPlay
{
    [_play setTitle: _NS("Play")];
    [_dockMenuplay setTitle: _NS("Play")];
    [_voutMenuplay setTitle: _NS("Play")];
}

- (void)setPause
{
    [_play setTitle: _NS("Pause")];
    [_dockMenuplay setTitle: _NS("Pause")];
    [_voutMenuplay setTitle: _NS("Pause")];
}

- (void)setRepeatOne
{
    [_repeat setState: NSOnState];
    [_loop setState: NSOffState];
}

- (void)setRepeatAll
{
    [_repeat setState: NSOffState];
    [_loop setState: NSOnState];
}

- (void)setRepeatOff
{
    [_repeat setState: NSOffState];
    [_loop setState: NSOffState];
}

#pragma mark - Dynamic menu creation and validation

- (void)setupVarMenuItem:(NSMenuItem *)mi
                  target:(vlc_object_t *)p_object
                     var:(const char *)psz_variable
                selector:(SEL)pf_callback
{
    vlc_value_t val;
    char *text;
    int i_type = var_Type(p_object, psz_variable);

    switch(i_type & VLC_VAR_TYPE) {
        case VLC_VAR_VOID:
        case VLC_VAR_BOOL:
        case VLC_VAR_STRING:
        case VLC_VAR_INTEGER:
            break;
        default:
            /* Variable doesn't exist or isn't handled */
            msg_Warn(p_object, "variable %s doesn't exist or isn't handled", psz_variable);
            return;
    }

    /* Get the descriptive name of the variable */
    var_Change(p_object, psz_variable, VLC_VAR_GETTEXT, &text);
    [mi setTitle: _NS(text ? text : psz_variable)];

    if (i_type & VLC_VAR_HASCHOICE) {
        NSMenu *menu = [mi submenu];

        [self setupVarMenu:menu forMenuItem:mi target:p_object
                       var:psz_variable selector:pf_callback];

        free(text);
        return;
    }

    if (var_Get(p_object, psz_variable, &val) < 0)
        return;

    VLCAutoGeneratedMenuContent *data;
    switch(i_type & VLC_VAR_TYPE) {
        case VLC_VAR_VOID:
            data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                                                                      andValue: val ofType: i_type];
            [mi setRepresentedObject:data];
            break;

        case VLC_VAR_BOOL:
            data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                                                                      andValue: val ofType: i_type];
            [mi setRepresentedObject:data];
            if (!(i_type & VLC_VAR_ISCOMMAND))
                [mi setState: val.b_bool ? TRUE : FALSE ];
            break;

        default:
            break;
    }

    if ((i_type & VLC_VAR_TYPE) == VLC_VAR_STRING) free(val.psz_string);
    free(text);
}


- (void)setupVarMenu:(NSMenu *)menu
         forMenuItem: (NSMenuItem *)parent
              target:(vlc_object_t *)p_object
                 var:(const char *)psz_variable
            selector:(SEL)pf_callback
{
    vlc_value_t val;
    vlc_value_t *val_list;
    char **text_list;
    size_t count, i;
    int i_type;

    /* remove previous items */
    [menu removeAllItems];

    /* we disable everything here, and enable it again when needed, below */
    [parent setEnabled:NO];

    /* Aspect Ratio */
    if ([[parent title] isEqualToString: _NS("Aspect ratio")] == YES) {
        NSMenuItem *lmi_tmp2;
        lmi_tmp2 = [menu addItemWithTitle: _NS("Lock Aspect Ratio") action: @selector(lockVideosAspectRatio:) keyEquivalent: @""];
        [lmi_tmp2 setTarget: self];
        [lmi_tmp2 setEnabled: YES];
        [lmi_tmp2 setState: [[VLCCoreInteraction sharedInstance] aspectRatioIsLocked]];
        [parent setEnabled: YES];
        [menu addItem: [NSMenuItem separatorItem]];
    }

    /* Check the type of the object variable */
    i_type = var_Type(p_object, psz_variable);

    /* Make sure we want to display the variable */
    if (i_type & VLC_VAR_HASCHOICE) {
        size_t count;

        var_Change(p_object, psz_variable, VLC_VAR_CHOICESCOUNT, &count);
        if (count <= 1)
            return;
    }
    else
        return;

    switch(i_type & VLC_VAR_TYPE) {
        case VLC_VAR_VOID:
        case VLC_VAR_BOOL:
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

    if (var_Change(p_object, psz_variable, VLC_VAR_GETCHOICES,
                   &count, &val_list, &text_list) < 0) {
        if ((i_type & VLC_VAR_TYPE) == VLC_VAR_STRING) free(val.psz_string);
        return;
    }

    /* make (un)sensitive */
    [parent setEnabled: (count > 1)];

    for (i = 0; i < count; i++) {
        NSMenuItem *lmi;
        NSString *title = @"";
        VLCAutoGeneratedMenuContent *data;

        switch(i_type & VLC_VAR_TYPE) {
            case VLC_VAR_STRING:

                title = _NS(text_list[i] ? text_list[i] : val_list[i].psz_string);

                lmi = [menu addItemWithTitle: title action: pf_callback keyEquivalent: @""];
                data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                                                                          andValue: val_list[i] ofType: i_type];
                [lmi setRepresentedObject:data];
                [lmi setTarget: self];

                if (!strcmp(val.psz_string, val_list[i].psz_string) && !(i_type & VLC_VAR_ISCOMMAND))
                    [lmi setState: TRUE ];

                free(text_list[i]);
                free(val_list[i].psz_string);
                break;

            case VLC_VAR_INTEGER:

                title = text_list[i] ?
                _NS(text_list[i]) : [NSString stringWithFormat: @"%"PRId64, val_list[i].i_int];

                lmi = [menu addItemWithTitle: title action: pf_callback keyEquivalent: @""];
                data = [[VLCAutoGeneratedMenuContent alloc] initWithVariableName: psz_variable ofObject: p_object
                                                                          andValue: val_list[i] ofType: i_type];
                [lmi setRepresentedObject:data];
                [lmi setTarget: self];

                if (val_list[i].i_int == val.i_int && !(i_type & VLC_VAR_ISCOMMAND))
                    [lmi setState: TRUE ];

                free(text_list[i]);
                break;

            default:
                break;
        }
    }

    /* clean up everything */
    if ((i_type & VLC_VAR_TYPE) == VLC_VAR_STRING) free(val.psz_string);
    free(text_list);
    free(val_list);
}

- (void)toggleVar:(id)sender
{
    NSMenuItem *mi = (NSMenuItem *)sender;
    VLCAutoGeneratedMenuContent *data = [mi representedObject];
    [NSThread detachNewThreadSelector: @selector(toggleVarThread:)
                             toTarget: self withObject: data];

    return;
}

- (int)toggleVarThread: (id)data
{
    @autoreleasepool {
        vlc_object_t *p_object;

        assert([data isKindOfClass:[VLCAutoGeneratedMenuContent class]]);
        VLCAutoGeneratedMenuContent *menuContent = (VLCAutoGeneratedMenuContent *)data;

        p_object = [menuContent vlcObject];
        var_Set(p_object, [menuContent name], [menuContent value]);
        vlc_object_release(p_object);
        return true;
    }
}

#pragma mark - menu delegation

- (void)menuWillOpen:(NSMenu *)menu
{
    [_cancelRendererDiscoveryTimer invalidate];
    [_rendererMenuController startRendererDiscoveries];
}

- (void)menuDidClose:(NSMenu *)menu
{
    _cancelRendererDiscoveryTimer = [NSTimer scheduledTimerWithTimeInterval:20.
                                                                     target:self
                                                                   selector:@selector(cancelRendererDiscovery)
                                                                   userInfo:nil
                                                                    repeats:NO];
}

- (void)cancelRendererDiscovery
{
    [_rendererMenuController stopRendererDiscoveries];
}

@end

@implementation VLCMainMenu (NSMenuValidation)

- (BOOL)validateMenuItem:(NSMenuItem *)mi
{
    BOOL enabled = YES;
    input_item_t *inputItem = _playlistController.currentlyPlayingInputItem;

    if (mi == _stop || mi == _voutMenustop || mi == _dockMenustop) {
        if (!inputItem)
            enabled = NO;
        [self setupMenus]; /* Make sure input menu is up to date */
    } else if (mi == _previous          ||
               mi == _voutMenuprev      ||
               mi == _dockMenuprevious) {
        enabled = _playlistController.hasPreviousPlaylistItem;
    } else if (
               mi == _next              ||
               mi == _voutMenunext      ||
               mi == _dockMenunext) {
        enabled = _playlistController.hasNextPlaylistItem;
    } else if (mi == _record) {
        enabled = _playerController.recordable;
    } else if (mi == _random) {
        enum vlc_playlist_playback_order playbackOrder = [_playlistController playbackOrder];
        [mi setState: playbackOrder == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM ? NSOnState : NSOffState];
    } else if (mi == _repeat) {
        enum vlc_playlist_playback_repeat playbackRepeat = [_playlistController playbackRepeat];
        [mi setState: playbackRepeat == VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT ? NSOnState : NSOffState];
    } else if (mi == _loop) {
        enum vlc_playlist_playback_repeat playbackRepeat = [_playlistController playbackRepeat];
        [mi setState: playbackRepeat == VLC_PLAYLIST_PLAYBACK_REPEAT_ALL ? NSOnState : NSOffState];
    } else if (mi == _quitAfterPB) {
        BOOL state = _playerController.actionAfterStop == VLC_PLAYER_MEDIA_STOPPED_EXIT;
        [mi setState: state ? NSOnState : NSOffState];
    } else if (mi == _fwd || mi == _bwd || mi == _jumpToTime) {
        enabled = _playerController.seekable;
    } else if (mi == _mute || mi == _dockMenumute || mi == _voutMenumute) {
        [mi setState: _playerController.mute ? NSOnState : NSOffState];
        [self setupMenus]; /* Make sure audio menu is up to date */
        [self refreshAudioDeviceList];
    } else if (mi == _half_window           ||
               mi == _normal_window         ||
               mi == _double_window         ||
               mi == _fittoscreen           ||
               mi == _snapshot              ||
               mi == _voutMenusnapshot      ||
               mi == _fullscreenItem        ||
               mi == _voutMenufullscreen    ||
               mi == _floatontop
               ) {

        vout_thread_t *p_vout = [_playerController videoOutputThreadForKeyWindow];
        if (p_vout != NULL) {
            // FIXME: re-write using VLCPlayerController
            if (mi == _floatontop)
                [mi setState: var_GetBool(p_vout, "video-on-top")];

            if (mi == _fullscreenItem || mi == _voutMenufullscreen)
                [mi setState: _playerController.fullscreen];

            enabled = YES;
            vout_Release(p_vout);
        }

        [self setupMenus]; /* Make sure video menu is up to date */

    } else if (mi == _openSubtitleFile) {
        enabled = [mi isEnabled];
        [self setupMenus]; /* Make sure subtitles menu is up to date */
    } else {
        NSMenuItem *_parent = [mi parentItem];
        if (_parent == _subtitle_size || mi == _subtitle_size           ||
            _parent == _subtitle_textcolor || mi == _subtitle_textcolor ||
            _parent == _subtitle_bgcolor || mi == _subtitle_bgcolor     ||
            _parent == _subtitle_bgopacity || mi == _subtitle_bgopacity ||
            _parent == _subtitle_outlinethickness || mi == _subtitle_outlinethickness
            ) {
            enabled = _openSubtitleFile.isEnabled;
        } else if (_parent == _teletext || mi == _teletext) {
            enabled = _playerController.teletextMenuAvailable;
        }
    }

    if (inputItem) {
        input_item_Release(inputItem);
    }

    return enabled;
}

@end


/*****************************************************************************
 *VLCAutoGeneratedMenuContent implementation
 *****************************************************************************
 *Object connected to a playlistitem which remembers the data belonging to
 *the variable of the autogenerated menu
 *****************************************************************************/

@interface VLCAutoGeneratedMenuContent ()
{
    char *psz_name;
    vlc_object_t *vlc_object;
    vlc_value_t value;
    int i_type;
}
@end
@implementation VLCAutoGeneratedMenuContent

-(id) initWithVariableName:(const char *)name ofObject:(vlc_object_t *)object
                  andValue:(vlc_value_t)val ofType:(int)type
{
    self = [super init];

    if (self != nil) {
        vlc_object = vlc_object_hold(object);
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
    if (vlc_object)
        vlc_object_release(vlc_object);
    if ((i_type & VLC_VAR_TYPE) == VLC_VAR_STRING)
        free(value.psz_string);
    free(psz_name);
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
    return vlc_object_hold(vlc_object);
}

- (int)type
{
    return i_type;
}

@end
