/*****************************************************************************
 * VLCSimplePrefsController.h: Simple Preferences for Mac OS X
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

#import <Cocoa/Cocoa.h>

#import <vlc_common.h>

#import "main/VLCMain.h"

@interface VLCSimplePrefsController : NSWindowController

// Overall window
@property (readwrite, weak) IBOutlet NSButton *showAllButton;
@property (readwrite, weak) IBOutlet NSButton *cancelButton;
@property (readwrite, weak) IBOutlet NSScrollView *scrollView;
@property (readwrite, weak) IBOutlet NSView *contentView;
@property (readwrite, weak) IBOutlet NSButton *resetButton;
@property (readwrite, weak) IBOutlet NSButton *saveButton;

// Audio pane
@property (readwrite, strong) IBOutlet NSView *audioView;

@property (readwrite, weak) IBOutlet NSBox *audio_effectsBox;
@property (readwrite, weak) IBOutlet NSButton *audio_enableCheckbox;
@property (readwrite, weak) IBOutlet NSBox *audio_generalBox;
@property (readwrite, weak) IBOutlet NSTextField *audio_langTextField;
@property (readwrite, weak) IBOutlet NSTextField *audio_langLabel;
@property (readwrite, weak) IBOutlet NSBox *audio_lastBox;
@property (readwrite, weak) IBOutlet NSButton *audio_lastCheckbox;
@property (readwrite, weak) IBOutlet NSSecureTextField *audio_lastpwdSecureTextField;
@property (readwrite, weak) IBOutlet NSTextField *audio_lastpwdLabel;
@property (readwrite, weak) IBOutlet NSTextField *audio_lastuserTextField;
@property (readwrite, weak) IBOutlet NSTextField *audio_lastuserLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *audio_visualPopup;
@property (readwrite, weak) IBOutlet NSTextField *audio_visualLabel;
@property (readwrite, weak) IBOutlet NSTextField *audio_volTextField;
@property (readwrite, weak) IBOutlet NSSlider *audio_volSlider;
@property (readwrite, weak) IBOutlet NSMatrix *audio_autosavevolMatrix;
@property (readwrite, weak) IBOutlet NSButtonCell *audio_autosavevol_yesButtonCell;
@property (readwrite, weak) IBOutlet NSButtonCell *audio_autosavevol_noButtonCell;

// library pane
@property (readwrite, strong) IBOutlet NSView *mediaLibraryView;
@property (readwrite, weak) IBOutlet NSTableView *mediaLibraryFolderTableView;
@property (readwrite, weak) IBOutlet NSTableColumn *mediaLibraryNameTableColumn;
@property (readwrite, weak) IBOutlet NSTableColumn *mediaLibraryPathTableColumn;
@property (readwrite, weak) IBOutlet NSTableColumn *mediaLibraryPresentTableColumn;
@property (readwrite, weak) IBOutlet NSTableColumn *mediaLibraryBannedTableColumn;
@property (readwrite, weak) IBOutlet NSButton *mediaLibraryAddFolderButton;
@property (readwrite, weak) IBOutlet NSButton *mediaLibraryRemoveFolderButton;
@property (readwrite, weak) IBOutlet NSButton *mediaLibraryBanFolderButton;

// hotkeys pane
@property (readwrite, strong) IBOutlet NSView *hotkeysView;

@property (readwrite, strong) IBOutlet NSWindow *hotkeys_change_win;
@property (readwrite, weak) IBOutlet NSTextField *hotkeys_changeLabel;
@property (readwrite, weak) IBOutlet NSTextField *hotkeys_change_keysLabel;
@property (readwrite, weak) IBOutlet NSTextField *hotkeys_change_takenLabel;
@property (readwrite, weak) IBOutlet NSButton *hotkeys_change_cancelButton;
@property (readwrite, weak) IBOutlet NSButton *hotkeys_change_okButton;
@property (readwrite, weak) IBOutlet NSButton *hotkeys_change_clearButton;
@property (readwrite, weak) IBOutlet NSTextField *hotkeysLabel;
@property (readwrite, weak) IBOutlet NSTableView *hotkeys_listbox;
@property (readwrite, weak) IBOutlet NSButton *hotkeys_mediakeysCheckbox;

// input pane
@property (readwrite, strong) IBOutlet NSView *inputView;

@property (readwrite, weak) IBOutlet NSBox *input_recordBox;
@property (readwrite, weak) IBOutlet NSTextField *input_recordTextField;
@property (readwrite, weak) IBOutlet NSButton *input_recordButton;
@property (readwrite, weak) IBOutlet NSPopUpButton *input_aviPopup;
@property (readwrite, weak) IBOutlet NSTextField *input_aviLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *input_cachelevelPopup;
@property (readwrite, weak) IBOutlet NSTextField *input_cachelevelLabel;
@property (readwrite, weak) IBOutlet NSTextField *input_cachelevel_customLabel;
@property (readwrite, weak) IBOutlet NSBox *input_cachingBox;
@property (readwrite, weak) IBOutlet NSBox *input_muxBox;
@property (readwrite, weak) IBOutlet NSBox *input_netBox;
@property (readwrite, weak) IBOutlet NSButton *input_hardwareAccelerationCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *input_skipLoopLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *input_skipLoopPopup;
@property (readwrite, weak) IBOutlet NSButton *input_urlhandlerButton;
@property (readwrite, weak) IBOutlet NSButton *input_skipFramesCheckbox;
@property (readwrite, weak) IBOutlet NSButton *input_fastSeekCheckbox;

// intf pane - general box
@property (readwrite, strong) IBOutlet NSView *intfView;

@property (readwrite, weak) IBOutlet NSBox *intf_generalSettingsBox;
@property (readwrite, weak) IBOutlet NSPopUpButton *intf_languagePopup;
@property (readwrite, weak) IBOutlet NSTextField *intf_languageLabel;
@property (readwrite, weak) IBOutlet NSButton *intf_statusIconCheckbox;
@property (readwrite, weak) IBOutlet NSButton *intf_largeFontInListsCheckbox;

// intf pane - control box
@property (readwrite, weak) IBOutlet NSBox *intf_playbackControlBox;
@property (readwrite, weak) IBOutlet NSTextField *intf_continueplaybackLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *intf_continueplaybackPopup;

// intf pane - behaviour box
@property (readwrite, weak) IBOutlet NSBox *intf_playbackBehaviourBox;
@property (readwrite, weak) IBOutlet NSButton *intf_enableNotificationsCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *intf_pauseitunesLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *intf_pauseitunesPopup;

// intf pane - network box
@property (readwrite, weak) IBOutlet NSBox *intf_networkBox;
@property (readwrite, weak) IBOutlet NSButton *intf_artCheckbox;
@property (readwrite, weak) IBOutlet NSButton *intf_updateCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *intf_last_updateLabel;

// intf pane - http interface box
@property (readwrite, weak) IBOutlet NSBox *intf_luahttpBox;
@property (readwrite, weak) IBOutlet NSButton *intf_enableluahttpCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *intf_luahttppwdLabel;
@property (readwrite, weak) IBOutlet NSTextField *intf_luahttppwdTextField;

// osd pane
@property (readwrite, strong) IBOutlet NSView *osdView;

@property (readwrite, weak) IBOutlet NSPopUpButton *osd_encodingPopup;
@property (readwrite, weak) IBOutlet NSTextField *osd_encodingLabel;
@property (readwrite, weak) IBOutlet NSBox *osd_fontBox;
@property (readwrite, weak) IBOutlet NSButton *osd_fontButton;
@property (readwrite, weak) IBOutlet NSPopUpButton *osd_font_colorPopup;
@property (readwrite, weak) IBOutlet NSTextField *osd_font_colorLabel;
@property (readwrite, weak) IBOutlet NSTextField *osd_fontTextField;
@property (readwrite, weak) IBOutlet NSSlider *osd_font_sizeSlider;
@property (readwrite, weak) IBOutlet NSTextField *osd_font_sizeTextField;
@property (readwrite, weak) IBOutlet NSTextField *osd_font_sizeLabel;
@property (readwrite, weak) IBOutlet NSTextField *osd_fontLabel;
@property (readwrite, weak) IBOutlet NSBox *osd_langBox;
@property (readwrite, weak) IBOutlet NSTextField *osd_langTextField;
@property (readwrite, weak) IBOutlet NSTextField *osd_langLabel;
@property (readwrite, weak) IBOutlet NSTextField *osd_opacityLabel;
@property (readwrite, weak) IBOutlet NSTextField *osd_opacityTextField;
@property (readwrite, weak) IBOutlet NSSlider *osd_opacitySlider;
@property (readwrite, weak) IBOutlet NSPopUpButton *osd_outline_colorPopup;
@property (readwrite, weak) IBOutlet NSTextField *osd_outline_colorLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *osd_outline_thicknessPopup;
@property (readwrite, weak) IBOutlet NSTextField *osd_outline_thicknessLabel;
@property (readwrite, weak) IBOutlet NSButton *osd_forceboldCheckbox;
@property (readwrite, weak) IBOutlet NSBox *osd_osdBox;
@property (readwrite, weak) IBOutlet NSButton *osd_osdCheckbox;

// video pane
@property (readwrite, strong) IBOutlet NSView *videoView;

@property (readwrite, weak) IBOutlet NSButton *video_enableCheckbox;
// video pane - display box
@property (readwrite, weak) IBOutlet NSBox *video_displayBox;
@property (readwrite, weak) IBOutlet NSButton *video_embeddedCheckbox;
@property (readwrite, weak) IBOutlet NSButton *video_pauseWhenMinimizedCheckbox;
@property (readwrite, weak) IBOutlet NSButton *video_onTopCheckbox;
@property (readwrite, weak) IBOutlet NSButton *video_videodecoCheckbox;
@property (readwrite, weak) IBOutlet NSButton *video_resizeToNativeSizeCheckbox;

// video pane - fullscreen box
@property (readwrite, weak) IBOutlet NSBox *video_fullscreenBox;
@property (readwrite, weak) IBOutlet NSButton *video_startInFullscreenCheckbox;
@property (readwrite, weak) IBOutlet NSButton *video_blackScreenCheckbox;
@property (readwrite, weak) IBOutlet NSButton *video_nativeFullscreenCheckbox;
@property (readwrite, weak) IBOutlet NSTextField *video_deviceLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *video_devicePopup;

// video pane - video box
@property (readwrite, weak) IBOutlet NSBox *video_videoBox;
@property (readwrite, weak) IBOutlet NSTextField *video_deinterlaceLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *video_deinterlacePopup;
@property (readwrite, weak) IBOutlet NSTextField *video_deinterlace_modeLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *video_deinterlace_modePopup;

// video pane - snapshot box
@property (readwrite, weak) IBOutlet NSBox *video_snapBox;
@property (readwrite, weak) IBOutlet NSButton *video_snap_folderButton;
@property (readwrite, weak) IBOutlet NSTextField *video_snap_folderTextField;
@property (readwrite, weak) IBOutlet NSTextField *video_snap_folderLabel;
@property (readwrite, weak) IBOutlet NSPopUpButton *video_snap_formatPopup;
@property (readwrite, weak) IBOutlet NSTextField *video_snap_formatLabel;
@property (readwrite, weak) IBOutlet NSTextField *video_snap_prefixTextField;
@property (readwrite, weak) IBOutlet NSTextField *video_snap_prefixLabel;
@property (readwrite, weak) IBOutlet NSButton *video_snap_seqnumCheckbox;

// URL handler popup window
@property (readwrite, strong) IBOutlet NSWindow *urlhandler_win;

@property (readwrite, weak) IBOutlet NSTextField *urlhandler_titleLabel;
@property (readwrite, weak) IBOutlet NSTextField *urlhandler_subtitleLabel;
@property (readwrite, weak) IBOutlet NSButton *urlhandler_saveButton;
@property (readwrite, weak) IBOutlet NSButton *urlhandler_cancelButton;
@property (readwrite, weak) IBOutlet NSPopUpButton *urlhandler_ftpPopup;
@property (readwrite, weak) IBOutlet NSPopUpButton *urlhandler_mmsPopup;
@property (readwrite, weak) IBOutlet NSPopUpButton *urlhandler_rtmpPopup;
@property (readwrite, weak) IBOutlet NSPopUpButton *urlhandler_rtpPopup;
@property (readwrite, weak) IBOutlet NSPopUpButton *urlhandler_rtspPopup;
@property (readwrite, weak) IBOutlet NSPopUpButton *urlhandler_sftpPopup;
@property (readwrite, weak) IBOutlet NSPopUpButton *urlhandler_smbPopup;
@property (readwrite, weak) IBOutlet NSPopUpButton *urlhandler_udpPopup;

/* toolbar */
- (NSToolbarItem *)toolbar:(NSToolbar *)o_toolbar
     itemForItemIdentifier:(NSString *)o_itemIdent
 willBeInsertedIntoToolbar:(BOOL)b_willBeInserted;
- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar;
- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar;

- (void)showSimplePrefs;
- (void)showSimplePrefsWithLevel:(NSInteger)i_window_level;

- (IBAction)buttonAction:(id)sender;
- (IBAction)resetPreferences:(id)sender;

/* interface */
- (IBAction)interfaceSettingChanged:(id)sender;

/* audio */
- (IBAction)audioSettingChanged:(id)sender;

/* video */
- (IBAction)videoSettingChanged:(id)sender;

/* OSD / subtitles */
- (IBAction)osdSettingChanged:(id)sender;
- (IBAction)showFontPicker:(id)sender;
- (void)changeFont:(id)sender;

/* input & codecs */
- (IBAction)inputSettingChanged:(id)sender;
- (IBAction)urlHandlerAction:(id)sender;

/* hotkeys */
- (IBAction)hotkeySettingChanged:(id)sender;
- (BOOL)changeHotkeyTo: (NSString *)theKey;

/**
 * Updates right to left UI setting according to currently set language code
 * \return true if specific language was selected and RTL UI settings were updated
 */
+ (BOOL)updateRightToLeftSettings;

@end
