/*****************************************************************************
 *MainMenu.h: MacOS X interface module
 *****************************************************************************
 *Copyright (C) 2011-2018 Felix Paul Kühne
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

#import <Cocoa/Cocoa.h>

#import <vlc_common.h>
#import <vlc_interface.h>

@interface VLCMainMenu : NSObject

/* main menu */
@property (readwrite, weak) IBOutlet NSMenuItem *about;
@property (readwrite, weak) IBOutlet NSMenuItem *prefs;
@property (readwrite, weak) IBOutlet NSMenuItem *checkForUpdate;
@property (readwrite, weak) IBOutlet NSMenuItem *extensions;
@property (readwrite, weak) IBOutlet NSMenu *extensionsMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *addonManager;
@property (readwrite, weak) IBOutlet NSMenuItem *add_intf;
@property (readwrite, weak) IBOutlet NSMenu *add_intfMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *services;
@property (readwrite, weak) IBOutlet NSMenuItem *hide;
@property (readwrite, weak) IBOutlet NSMenuItem *hide_others;
@property (readwrite, weak) IBOutlet NSMenuItem *show_all;
@property (readwrite, weak) IBOutlet NSMenuItem *quit;

@property (readwrite, weak) IBOutlet NSMenu *fileMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *open_file;
@property (readwrite, weak) IBOutlet NSMenuItem *open_generic;
@property (readwrite, weak) IBOutlet NSMenuItem *open_disc;
@property (readwrite, weak) IBOutlet NSMenuItem *open_net;
@property (readwrite, weak) IBOutlet NSMenuItem *open_capture;
@property (readwrite, weak) IBOutlet NSMenuItem *open_recent;
@property (readwrite, weak) IBOutlet NSMenuItem *close_window;
@property (readwrite, weak) IBOutlet NSMenuItem *convertandsave;
@property (readwrite, weak) IBOutlet NSMenuItem *save_playlist;
@property (readwrite, weak) IBOutlet NSMenuItem *revealInFinder;

@property (readwrite, weak) IBOutlet NSMenu *editMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *cutItem;
@property (readwrite, weak) IBOutlet NSMenuItem *mcopyItem;
@property (readwrite, weak) IBOutlet NSMenuItem *pasteItem;
@property (readwrite, weak) IBOutlet NSMenuItem *clearItem;
@property (readwrite, weak) IBOutlet NSMenuItem *select_all;
@property (readwrite, weak) IBOutlet NSMenuItem *findItem;

@property (readwrite, weak) IBOutlet NSMenu *viewMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *toggleJumpButtons;
@property (readwrite, weak) IBOutlet NSMenuItem *togglePlaymodeButtons;
@property (readwrite, weak) IBOutlet NSMenuItem *toggleEffectsButton;
@property (readwrite, weak) IBOutlet NSMenu *playlistTableColumnsMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *playlistTableColumns;

@property (readwrite, weak) IBOutlet NSMenu *controlsMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *play;
@property (readwrite, weak) IBOutlet NSMenuItem *stop;
@property (readwrite, weak) IBOutlet NSMenuItem *record;
@property (readwrite, weak) IBOutlet NSMenuItem *rate;
@property (readwrite, weak) IBOutlet NSView *rate_view;
@property (readwrite, weak) IBOutlet NSTextField *rateLabel;
@property (readwrite, weak) IBOutlet NSTextField *rate_slowerLabel;
@property (readwrite, weak) IBOutlet NSTextField *rate_normalLabel;
@property (readwrite, weak) IBOutlet NSTextField *rate_fasterLabel;
@property (readwrite, weak) IBOutlet NSSlider *rate_sld;
@property (readwrite, weak) IBOutlet NSTextField *rateTextField;
@property (readwrite, weak) IBOutlet NSMenuItem *trackSynchronization;
@property (readwrite, weak) IBOutlet NSMenuItem *previous;
@property (readwrite, weak) IBOutlet NSMenuItem *next;
@property (readwrite, weak) IBOutlet NSMenuItem *random;
@property (readwrite, weak) IBOutlet NSMenuItem *repeat;
@property (readwrite, weak) IBOutlet NSMenuItem *AtoBloop;
@property (readwrite, weak) IBOutlet NSMenuItem *sortPlaylist;
@property (readwrite, weak) IBOutlet NSMenuItem *quitAfterPB;
@property (readwrite, weak) IBOutlet NSMenuItem *fwd;
@property (readwrite, weak) IBOutlet NSMenuItem *bwd;
@property (readwrite, weak) IBOutlet NSMenuItem *jumpToTime;
@property (readwrite, weak) IBOutlet NSMenu *rendererMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *rendererMenuItem;
@property (readwrite, weak) IBOutlet NSMenuItem *rendererNoneItem;
@property (readwrite, weak) IBOutlet NSMenuItem *program;
@property (readwrite, weak) IBOutlet NSMenu *programMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *title;
@property (readwrite, weak) IBOutlet NSMenu *titleMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *chapter;
@property (readwrite, weak) IBOutlet NSMenu *chapterMenu;

@property (readwrite, weak) IBOutlet NSMenu *audioMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *vol_up;
@property (readwrite, weak) IBOutlet NSMenuItem *vol_down;
@property (readwrite, weak) IBOutlet NSMenuItem *mute;
@property (readwrite, weak) IBOutlet NSMenuItem *audiotrack;
@property (readwrite, weak) IBOutlet NSMenu *audiotrackMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *channels;
@property (readwrite, weak) IBOutlet NSMenu *channelsMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *audioDevice;
@property (readwrite, weak) IBOutlet NSMenu *audioDeviceMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *visual;
@property (readwrite, weak) IBOutlet NSMenu *visualMenu;

@property (readwrite, weak) IBOutlet NSMenu *videoMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *half_window;
@property (readwrite, weak) IBOutlet NSMenuItem *normal_window;
@property (readwrite, weak) IBOutlet NSMenuItem *double_window;
@property (readwrite, weak) IBOutlet NSMenuItem *fittoscreen;
@property (readwrite, weak) IBOutlet NSMenuItem *fullscreenItem;
@property (readwrite, weak) IBOutlet NSMenuItem *floatontop;
@property (readwrite, weak) IBOutlet NSMenuItem *snapshot;
@property (readwrite, weak) IBOutlet NSMenuItem *videotrack;
@property (readwrite, weak) IBOutlet NSMenu *videotrackMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *screen;
@property (readwrite, weak) IBOutlet NSMenu *screenMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *aspect_ratio;
@property (readwrite, weak) IBOutlet NSMenu *aspect_ratioMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *crop;
@property (readwrite, weak) IBOutlet NSMenu *cropMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *deinterlace;
@property (readwrite, weak) IBOutlet NSMenu *deinterlaceMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *deinterlace_mode;
@property (readwrite, weak) IBOutlet NSMenu *deinterlace_modeMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *postprocessing;
@property (readwrite, weak) IBOutlet NSMenu *postprocessingMenu;

@property (readwrite, weak) IBOutlet NSMenu *subtitlesMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *subtitle_track;
@property (readwrite, weak) IBOutlet NSMenu *subtitle_tracksMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *openSubtitleFile;
@property (readwrite, weak) IBOutlet NSMenuItem *subtitleSize;
@property (readwrite, weak) IBOutlet NSView *subtitleSizeView;
@property (readwrite, weak) IBOutlet NSTextField *subtitleSizeLabel;
@property (readwrite, weak) IBOutlet NSTextField *subtitleSizeSmallerLabel;
@property (readwrite, weak) IBOutlet NSTextField *subtitleSizeLargerLabel;
@property (readwrite, weak) IBOutlet NSSlider *subtitleSizeSlider;
@property (readwrite, weak) IBOutlet NSTextField *subtitleSizeTextField;
@property (readwrite, weak) IBOutlet NSMenu *subtitle_textcolorMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *subtitle_textcolor;
@property (readwrite, weak) IBOutlet NSMenu *subtitle_bgcolorMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *subtitle_bgcolor;
@property (readwrite, weak) IBOutlet NSMenuItem *subtitle_bgopacity;
@property (readwrite, weak) IBOutlet NSView *subtitle_bgopacity_view;
@property (readwrite, weak) IBOutlet NSTextField *subtitle_bgopacityLabel;
@property (readwrite, weak) IBOutlet NSTextField *subtitle_bgopacityLabel_gray;
@property (readwrite, weak) IBOutlet NSSlider *subtitle_bgopacity_sld;
@property (readwrite, weak) IBOutlet NSMenu *subtitle_outlinethicknessMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *subtitle_outlinethickness;
@property (readwrite, weak) IBOutlet NSMenuItem *teletext;
@property (readwrite, weak) IBOutlet NSMenuItem *teletext_transparent;
@property (readwrite, weak) IBOutlet NSMenuItem *teletext_index;
@property (readwrite, weak) IBOutlet NSMenuItem *teletext_red;
@property (readwrite, weak) IBOutlet NSMenuItem *teletext_green;
@property (readwrite, weak) IBOutlet NSMenuItem *teletext_yellow;
@property (readwrite, weak) IBOutlet NSMenuItem *teletext_blue;

@property (readwrite, weak) IBOutlet NSMenu *windowMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *minimize;
@property (readwrite, weak) IBOutlet NSMenuItem *zoom_window;
@property (readwrite, weak) IBOutlet NSMenuItem *player;
@property (readwrite, weak) IBOutlet NSMenuItem *controller;
@property (readwrite, weak) IBOutlet NSMenuItem *audioeffects;
@property (readwrite, weak) IBOutlet NSMenuItem *videoeffects;
@property (readwrite, weak) IBOutlet NSMenuItem *bookmarks;
@property (readwrite, weak) IBOutlet NSMenuItem *playlist;
@property (readwrite, weak) IBOutlet NSMenuItem *info;
@property (readwrite, weak) IBOutlet NSMenuItem *errorsAndWarnings;
@property (readwrite, weak) IBOutlet NSMenuItem *messages;
@property (readwrite, weak) IBOutlet NSMenuItem *bring_atf;

@property (readwrite, weak) IBOutlet NSMenu *helpMenu;
@property (readwrite, weak) IBOutlet NSMenuItem *help;
@property (readwrite, weak) IBOutlet NSMenuItem *documentation;
@property (readwrite, weak) IBOutlet NSMenuItem *license;
@property (readwrite, weak) IBOutlet NSMenuItem *website;
@property (readwrite, weak) IBOutlet NSMenuItem *donation;
@property (readwrite, weak) IBOutlet NSMenuItem *forum;

/* dock menu */
@property (readwrite, weak) IBOutlet NSMenuItem *dockMenuplay;
@property (readwrite, weak) IBOutlet NSMenuItem *dockMenustop;
@property (readwrite, weak) IBOutlet NSMenuItem *dockMenunext;
@property (readwrite, weak) IBOutlet NSMenuItem *dockMenuprevious;
@property (readwrite, weak) IBOutlet NSMenuItem *dockMenumute;

/* vout menu */
@property (readwrite, strong) IBOutlet NSMenu *voutMenu;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenuplay;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenustop;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenuRecord;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenuprev;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenunext;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenuvolup;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenuvoldown;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenumute;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenuAudiotrack;
@property (readwrite, strong) IBOutlet NSMenu *voutMenuAudiotrackMenu;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenuVideotrack;
@property (readwrite, strong) IBOutlet NSMenu *voutMenuVideotrackMenu;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenuOpenSubtitleFile;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenuSubtitlestrack;
@property (readwrite, strong) IBOutlet NSMenu *voutMenuSubtitlestrackMenu;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenufullscreen;
@property (readwrite, strong) IBOutlet NSMenuItem *voutMenusnapshot;

@property (readwrite, strong) IBOutlet NSView *playlistSaveAccessoryView;
@property (readwrite, weak) IBOutlet NSPopUpButton *playlistSaveAccessoryPopup;
@property (readwrite, weak) IBOutlet NSTextField *playlistSaveAccessoryText;

- (void)releaseRepresentedObjects:(NSMenu *)the_menu;

- (IBAction)openAddonManager:(id)sender;

- (IBAction)intfOpenFile:(id)sender;
- (IBAction)intfOpenFileGeneric:(id)sender;
- (IBAction)intfOpenDisc:(id)sender;
- (IBAction)intfOpenNet:(id)sender;
- (IBAction)intfOpenCapture:(id)sender;
- (IBAction)savePlaylist:(id)sender;

- (IBAction)play:(id)sender;
- (IBAction)stop:(id)sender;

- (IBAction)prev:(id)sender;
- (IBAction)next:(id)sender;
- (IBAction)random:(id)sender;
- (IBAction)repeat:(id)sender;

- (IBAction)forward:(id)sender;
- (IBAction)backward:(id)sender;

- (IBAction)volumeUp:(id)sender;
- (IBAction)volumeDown:(id)sender;
- (IBAction)mute:(id)sender;

- (IBAction)goToSpecificTime:(id)sender;

- (IBAction)quitAfterPlayback:(id)sender;
- (IBAction)toggleRecord:(id)sender;
- (IBAction)setPlaybackRate:(id)sender;
- (IBAction)toggleAtoBloop:(id)sender;
- (IBAction)selectRenderer:(id)sender;

- (IBAction)toggleFullscreen:(id)sender;
- (IBAction)resizeVideoWindow:(id)sender;
- (IBAction)floatOnTop:(id)sender;
- (IBAction)createVideoSnapshot:(id)sender;

- (IBAction)addSubtitleFile:(id)sender;
- (IBAction)subtitleSize:(id)sender;
- (IBAction)switchSubtitleBackgroundOpacity:(id)sender;
- (IBAction)telxTransparent:(id)sender;
- (IBAction)telxNavLink:(id)sender;

- (IBAction)showConvertAndSave:(id)sender;
- (IBAction)showVideoEffects:(id)sender;
- (IBAction)showAudioEffects:(id)sender;
- (IBAction)showTrackSynchronization:(id)sender;
- (IBAction)showBookmarks:(id)sender;
- (IBAction)showInformationPanel:(id)sender;

- (IBAction)showAbout:(id)sender;
- (IBAction)showLicense:(id)sender;
- (IBAction)showPreferences:(id)sender;
- (IBAction)showHelp:(id)sender;
- (IBAction)openDocumentation:(id)sender;
- (IBAction)openWebsite:(id)sender;
- (IBAction)openForum:(id)sender;
- (IBAction)openDonate:(id)sender;
- (IBAction)showErrorsAndWarnings:(id)sender;
- (IBAction)showMessagesPanel:(id)showMessagesPanel;
- (IBAction)showMainWindow:(id)sender;
- (IBAction)showPlaylist:(id)sender;

@end
