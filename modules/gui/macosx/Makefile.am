include $(top_srcdir)/modules/common.am

guidir = $(pluginsdir)/gui

SUFFIXES += .xib

libmacosx_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-exceptions -fobjc-arc 
libmacosx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(guidir)' \
	-Wl,-framework,Cocoa -Wl,-framework,Carbon -Wl,-framework,CoreServices \
	-Wl,-framework,AVFoundation -Wl,-framework,CoreMedia -Wl,-framework,IOKit \
	-Wl,-framework,AddressBook -Wl,-framework,WebKit -Wl,-framework,CoreAudio \
	-Wl,-framework,SystemConfiguration -Wl,-framework,ScriptingBridge \
	-Wl,-framework,QuartzCore

if HAVE_SPARKLE
libmacosx_plugin_la_LDFLAGS += -Wl,-framework,Sparkle
endif

EXTRA_LTLIBRARIES = libmacosx_plugin.la
gui_LTLIBRARIES = $(LTLIBmacosx)

libmacosx_plugin_la_SOURCES = \
	helpers.h \
	VLCAboutWindowController.h VLCAboutWindowController.m \
	VLCAddonListItem.h VLCAddonListItem.m \
	VLCAddonsWindowController.h VLCAddonsWindowController.m \
	applescript.h applescript.m \
	AppleRemote.h AppleRemote.m \
	BWQuincyManager.h BWQuincyManager.m \
	BWQuincyUI.h BWQuincyUI.m \
	CompatibilityFixes.h \
	VLCControlsBarCommon.m VLCControlsBarCommon.h \
	VLCDocumentController.m VLCDocumentController.h \
	VLCMainWindowControlsBar.m VLCMainWindowControlsBar.h \
	VLCConvertAndSaveWindowController.h VLCConvertAndSaveWindowController.m \
	VLCCoreInteraction.h VLCCoreInteraction.m \
	VLCExtensionsDialogProvider.h VLCExtensionsDialogProvider.m \
	VLCExtensionsManager.h VLCExtensionsManager.m \
	VLCHelpWindowController.h VLCHelpWindowController.m \
	VLCMain+OldPrefs.h VLCMain+OldPrefs.m \
	VLCMain.h VLCMain.m \
	VLCApplication.h VLCApplication.m \
	iTunes.h \
	VLCKeyboardBacklightControl.h VLCKeyboardBacklightControl.m \
	macosx.m \
	VLCMainMenu.h VLCMainMenu.m \
	VLCMainWindowTitleView.h VLCMainWindowTitleView.m \
	misc.h misc.m \
	VLCOpenWindowController.h VLCOpenWindowController.m \
	VLCOutput.h VLCOutput.m \
	VLCPlaylistView.h VLCPlaylistView.m \
	VLCPlaylist.h VLCPlaylist.m \
	VLCPlaylistInfo.h VLCPlaylistInfo.m \
	VLCPLItem.h VLCPLItem.m \
	VLCPLModel.h VLCPLModel.m \
	prefs.h prefs.m \
	prefs_widgets.h prefs_widgets.m \
	PXSourceList.h PXSourceList.m \
	PXSourceListDataSource.h PXSourceListDelegate.h \
	SideBarItem.h SideBarItem.m \
	VLCPopupPanelController.h VLCPopupPanelController.m \
	VLCTextfieldPanelController.h VLCTextfieldPanelController.m \
	simple_prefs.h simple_prefs.m \
	SPInvocationGrabbing.h SPInvocationGrabbing.m \
	SPMediaKeyTap.h SPMediaKeyTap.m \
	Spotify.h \
	VLCStringUtility.h VLCStringUtility.m \
	VLCVoutView.h VLCVoutView.m \
	VLCTimeSelectionPanelController.h VLCTimeSelectionPanelController.m \
	VLCUIWidgets.h VLCUIWidgets.m \
	VLCScrollingClipView.h VLCScrollingClipView.m \
	VLCStatusBarIcon.h VLCStatusBarIcon.m \
	VLCVoutWindowController.h VLCVoutWindowController.m \
	Windows.h Windows.m \
	VLCAudioEffectsWindowController.h VLCAudioEffectsWindowController.m \
	VLCBookmarksWindowController.h VLCBookmarksWindowController.m \
	VLCCoreDialogProvider.h VLCCoreDialogProvider.m \
	VLCDebugMessageWindowController.h VLCDebugMessageWindowController.m \
	VLCHUDSliderCell.h VLCHUDSliderCell.m \
	VLCHUDButtonCell.h VLCHUDButtonCell.m \
	VLCHUDCheckboxCell.h VLCHUDCheckboxCell.m \
	VLCHUDRadiobuttonCell.h VLCHUDRadiobuttonCell.m \
	VLCHUDTextFieldCell.h VLCHUDTextFieldCell.m \
	VLCInputManager.h VLCInputManager.m \
	VLCMainWindow.h VLCMainWindow.m \
	VLCRendererDiscovery.h VLCRendererDiscovery.m \
	VLCRendererItem.h VLCRendererItem.m \
	VLCRendererDialog.h VLCRendererDialog.m \
	VLCResumeDialogController.h VLCResumeDialogController.m \
	VLCTrackSynchronizationWindowController.h VLCTrackSynchronizationWindowController.m \
	VLCVideoEffectsWindowController.h VLCVideoEffectsWindowController.m \
	VLCFSPanelController.h VLCFSPanelController.m \
	VLCFSPanelDraggableView.h VLCFSPanelDraggableView.m \
	VLCDefaultValueSlider.h VLCDefaultValueSlider.m \
	VLCDefaultValueSliderCell.h VLCDefaultValueSliderCell.m

if HAVE_DARWIN
nobase_libmacosx_plugin_la_DATA = \
	UI/About.nib \
	UI/AddonManager.nib \
	UI/AudioEffects.nib \
	UI/Bookmarks.nib \
	UI/BWQuincyMain.nib \
	UI/ConvertAndSave.nib \
	UI/CoreDialogs.nib \
	UI/DetachedVideoWindow.nib \
	UI/FSPanel.nib \
	UI/Help.nib \
	UI/LogMessageWindow.nib \
	UI/MainMenu.nib \
	UI/MainWindow.nib \
	UI/MediaInfo.nib \
	UI/Open.nib \
	UI/PlaylistAccessoryView.nib \
	UI/PlaylistMenu.nib \
	UI/PopupPanel.nib \
	UI/Preferences.nib \
	UI/ResumeDialog.nib \
	UI/SimplePreferences.nib \
	UI/StreamOutput.nib \
	UI/SyncTracks.nib \
	UI/TextfieldPanel.nib \
	UI/TimeSelectionPanel.nib \
	UI/VideoEffects.nib \
	UI/VLCFullScreenPanel.nib \
	UI/VLCRendererDialog.nib \
	UI/VLCStatusBarIconMainMenu.nib
endif

libmacosx_plugin_la_SOURCES += \
	UI/About.xib \
	UI/AddonManager.xib \
	UI/AudioEffects.xib \
	UI/Bookmarks.xib \
	UI/BWQuincyMain.xib \
	UI/ConvertAndSave.xib \
	UI/CoreDialogs.xib \
	UI/DetachedVideoWindow.xib \
	UI/FSPanel.xib \
	UI/Help.xib \
	UI/LogMessageWindow.xib \
	UI/MainMenu.xib \
	UI/MainWindow.xib \
	UI/MediaInfo.xib \
	UI/Open.xib \
	UI/PlaylistAccessoryView.xib \
	UI/PlaylistMenu.xib \
	UI/PopupPanel.xib \
	UI/Preferences.xib \
	UI/ResumeDialog.xib \
	UI/SimplePreferences.xib \
	UI/StreamOutput.xib \
	UI/SyncTracks.xib \
	UI/TextfieldPanel.xib \
	UI/TimeSelectionPanel.xib \
	UI/VideoEffects.xib \
	UI/VLCFullScreenPanel.xib \
	UI/VLCRendererDialog.xib \
	UI/VLCStatusBarIconMainMenu.xib

# User interface compilation

xib_verbose = $(xib_verbose_$(V))
xib_verbose_ = $(xib_verbose__$(AM_DEFAULT_VERBOSITY))
xib_verbose_0 = @echo "  XIB   " $@;
xib_verbose__0 = $(xib_verbose_0)

.xib.nib:
	$(AM_V_at)mkdir -p -- UI
	$(xib_verbose)$(XIB) --errors --warnings --notices \
	  --module VLC --auto-activate-custom-fonts --target-device mac \
	  --minimum-deployment-target 10.7 --output-format human-readable-text \
	  --compile $@ $<

DEPS_res = \
	Resources/App-Icons/VLC-Xmas.icns \
	Resources/App-Icons/VLC.icns \
	Resources/File-Icons/aiff.icns \
	Resources/File-Icons/audio.icns \
	Resources/File-Icons/avi.icns \
	Resources/File-Icons/flv.icns \
	Resources/File-Icons/generic.icns \
	Resources/File-Icons/m4a.icns \
	Resources/File-Icons/m4v.icns \
	Resources/File-Icons/mkv.icns \
	Resources/File-Icons/mov.icns \
	Resources/File-Icons/movie.icns \
	Resources/File-Icons/mp3.icns \
	Resources/File-Icons/mpeg.icns \
	Resources/File-Icons/ogg.icns \
	Resources/File-Icons/playlist.icns \
	Resources/File-Icons/rm.icns \
	Resources/File-Icons/subtitle.icns \
	Resources/File-Icons/vob.icns \
	Resources/File-Icons/wav.icns \
	Resources/File-Icons/wma.icns \
	Resources/File-Icons/wmv.icns \
	Resources/mainwindow/backward-3btns-pressed.png \
	Resources/mainwindow/backward-3btns-pressed@2x.png \
	Resources/mainwindow/backward-3btns.png \
	Resources/mainwindow/backward-3btns@2x.png \
	Resources/mainwindow/backward-6btns-pressed.png \
	Resources/mainwindow/backward-6btns-pressed@2x.png \
	Resources/mainwindow/backward-6btns.png \
	Resources/mainwindow/backward-6btns@2x.png \
	Resources/mainwindow/bottom-background.png \
	Resources/mainwindow/bottom-background@2x.png \
	Resources/mainwindow/dropzone-background.png \
	Resources/mainwindow/dropzone.png \
	Resources/mainwindow/dropzone@2x.png \
	Resources/mainwindow/effects-double-buttons-pressed.png \
	Resources/mainwindow/effects-double-buttons-pressed@2x.png \
	Resources/mainwindow/effects-double-buttons.png \
	Resources/mainwindow/effects-double-buttons@2x.png \
	Resources/mainwindow/effects-one-button-pressed.png \
	Resources/mainwindow/effects-one-button-pressed@2x.png \
	Resources/mainwindow/effects-one-button.png \
	Resources/mainwindow/effects-one-button@2x.png \
	Resources/mainwindow/forward-3btns-pressed.png \
	Resources/mainwindow/forward-3btns-pressed@2x.png \
	Resources/mainwindow/forward-3btns.png \
	Resources/mainwindow/forward-3btns@2x.png \
	Resources/mainwindow/forward-6btns-pressed.png \
	Resources/mainwindow/forward-6btns-pressed@2x.png \
	Resources/mainwindow/forward-6btns.png \
	Resources/mainwindow/forward-6btns@2x.png \
	Resources/mainwindow/fullscreen-double-buttons-pressed.png \
	Resources/mainwindow/fullscreen-double-buttons-pressed@2x.png \
	Resources/mainwindow/fullscreen-double-buttons.png \
	Resources/mainwindow/fullscreen-double-buttons@2x.png \
	Resources/mainwindow/fullscreen-one-button-pressed.png \
	Resources/mainwindow/fullscreen-one-button-pressed@2x.png \
	Resources/mainwindow/fullscreen-one-button.png \
	Resources/mainwindow/fullscreen-one-button@2x.png \
	Resources/mainwindow/next-6btns-pressed.png \
	Resources/mainwindow/next-6btns-pressed@2x.png \
	Resources/mainwindow/next-6btns.png \
	Resources/mainwindow/next-6btns@2x.png \
	Resources/mainwindow/pause-pressed.png \
	Resources/mainwindow/pause-pressed@2x.png \
	Resources/mainwindow/pause.png \
	Resources/mainwindow/pause@2x.png \
	Resources/mainwindow/play-pressed.png \
	Resources/mainwindow/play-pressed@2x.png \
	Resources/mainwindow/play.png \
	Resources/mainwindow/play@2x.png \
	Resources/mainwindow/playlist-1btn-pressed.png \
	Resources/mainwindow/playlist-1btn-pressed@2x.png \
	Resources/mainwindow/playlist-1btn.png \
	Resources/mainwindow/playlist-1btn@2x.png \
	Resources/mainwindow/playlist-btn-pressed.png \
	Resources/mainwindow/playlist-btn-pressed@2x.png \
	Resources/mainwindow/playlist-btn.png \
	Resources/mainwindow/playlist-btn@2x.png \
	Resources/mainwindow/previous-6btns-pressed.png \
	Resources/mainwindow/previous-6btns-pressed@2x.png \
	Resources/mainwindow/previous-6btns.png \
	Resources/mainwindow/previous-6btns@2x.png \
	Resources/mainwindow/progression-fill-left.png \
	Resources/mainwindow/progression-fill-left@2x.png \
	Resources/mainwindow/progression-fill-middle.png \
	Resources/mainwindow/progression-fill-middle@2x.png \
	Resources/mainwindow/progression-fill-right.png \
	Resources/mainwindow/progression-fill-right@2x.png \
	Resources/mainwindow/progression-knob.png \
	Resources/mainwindow/progression-knob@2x.png \
	Resources/mainwindow/progression-track-wrapper-left.png \
	Resources/mainwindow/progression-track-wrapper-left@2x.png \
	Resources/mainwindow/progression-track-wrapper-middle.png \
	Resources/mainwindow/progression-track-wrapper-middle@2x.png \
	Resources/mainwindow/progression-track-wrapper-right.png \
	Resources/mainwindow/progression-track-wrapper-right@2x.png \
	Resources/mainwindow/repeat-all-pressed.png \
	Resources/mainwindow/repeat-all-pressed@2x.png \
	Resources/mainwindow/repeat-all.png \
	Resources/mainwindow/repeat-all@2x.png \
	Resources/mainwindow/repeat-one-pressed.png \
	Resources/mainwindow/repeat-one-pressed@2x.png \
	Resources/mainwindow/repeat-one.png \
	Resources/mainwindow/repeat-one@2x.png \
	Resources/mainwindow/repeat-pressed.png \
	Resources/mainwindow/repeat-pressed@2x.png \
	Resources/mainwindow/repeat.png \
	Resources/mainwindow/repeat@2x.png \
	Resources/mainwindow/shuffle-blue-pressed.png \
	Resources/mainwindow/shuffle-blue-pressed@2x.png \
	Resources/mainwindow/shuffle-blue.png \
	Resources/mainwindow/shuffle-blue@2x.png \
	Resources/mainwindow/shuffle-pressed.png \
	Resources/mainwindow/shuffle-pressed@2x.png \
	Resources/mainwindow/shuffle.png \
	Resources/mainwindow/shuffle@2x.png \
	Resources/mainwindow/stop-pressed.png \
	Resources/mainwindow/stop-pressed@2x.png \
	Resources/mainwindow/stop.png \
	Resources/mainwindow/stop@2x.png \
	Resources/mainwindow/topbar_background.png \
	Resources/mainwindow/volume-high.png \
	Resources/mainwindow/volume-high@2x.png \
	Resources/mainwindow/volume-low.png \
	Resources/mainwindow/volume-low@2x.png \
	Resources/mainwindow/volume-slider-knob.png \
	Resources/mainwindow/volume-slider-knob@2x.png \
	Resources/mainwindow/volume-slider-track.png \
	Resources/mainwindow/volume-slider-track@2x.png \
	Resources/mainwindow_dark/backward-3btns-dark-pressed.png \
	Resources/mainwindow_dark/backward-3btns-dark-pressed@2x.png \
	Resources/mainwindow_dark/backward-3btns-dark.png \
	Resources/mainwindow_dark/backward-3btns-dark@2x.png \
	Resources/mainwindow_dark/backward-6btns-dark-pressed.png \
	Resources/mainwindow_dark/backward-6btns-dark-pressed@2x.png \
	Resources/mainwindow_dark/backward-6btns-dark.png \
	Resources/mainwindow_dark/backward-6btns-dark@2x.png \
	Resources/mainwindow_dark/bottom-background_dark.png \
	Resources/mainwindow_dark/bottom-background_dark@2x.png \
	Resources/mainwindow_dark/bottomdark-left.png \
	Resources/mainwindow_dark/bottomdark-left@2x.png \
	Resources/mainwindow_dark/bottomdark-right.png \
	Resources/mainwindow_dark/bottomdark-right@2x.png \
	Resources/mainwindow_dark/effects-double-buttons-pressed_dark.png \
	Resources/mainwindow_dark/effects-double-buttons-pressed_dark@2x.png \
	Resources/mainwindow_dark/effects-double-buttons_dark.png \
	Resources/mainwindow_dark/effects-double-buttons_dark@2x.png \
	Resources/mainwindow_dark/effects-one-button-pressed-dark.png \
	Resources/mainwindow_dark/effects-one-button-pressed-dark@2x.png \
	Resources/mainwindow_dark/effects-one-button_dark.png \
	Resources/mainwindow_dark/effects-one-button_dark@2x.png \
	Resources/mainwindow_dark/forward-3btns-dark-pressed.png \
	Resources/mainwindow_dark/forward-3btns-dark-pressed@2x.png \
	Resources/mainwindow_dark/forward-3btns-dark.png \
	Resources/mainwindow_dark/forward-3btns-dark@2x.png \
	Resources/mainwindow_dark/forward-6btns-dark-pressed.png \
	Resources/mainwindow_dark/forward-6btns-dark-pressed@2x.png \
	Resources/mainwindow_dark/forward-6btns-dark.png \
	Resources/mainwindow_dark/forward-6btns-dark@2x.png \
	Resources/mainwindow_dark/fullscreen-double-buttons-pressed_dark.png \
	Resources/mainwindow_dark/fullscreen-double-buttons-pressed_dark@2x.png \
	Resources/mainwindow_dark/fullscreen-double-buttons_dark.png \
	Resources/mainwindow_dark/fullscreen-double-buttons_dark@2x.png \
	Resources/mainwindow_dark/fullscreen-one-button-pressed_dark.png \
	Resources/mainwindow_dark/fullscreen-one-button-pressed_dark@2x.png \
	Resources/mainwindow_dark/fullscreen-one-button_dark.png \
	Resources/mainwindow_dark/fullscreen-one-button_dark@2x.png \
	Resources/mainwindow_dark/next-6btns-dark-pressed.png \
	Resources/mainwindow_dark/next-6btns-dark-pressed@2x.png \
	Resources/mainwindow_dark/next-6btns-dark.png \
	Resources/mainwindow_dark/next-6btns-dark@2x.png \
	Resources/mainwindow_dark/pause-pressed_dark.png \
	Resources/mainwindow_dark/pause-pressed_dark@2x.png \
	Resources/mainwindow_dark/pause_dark.png \
	Resources/mainwindow_dark/pause_dark@2x.png \
	Resources/mainwindow_dark/play-pressed_dark.png \
	Resources/mainwindow_dark/play-pressed_dark@2x.png \
	Resources/mainwindow_dark/play_dark.png \
	Resources/mainwindow_dark/play_dark@2x.png \
	Resources/mainwindow_dark/playlist-1btn-dark-pressed.png \
	Resources/mainwindow_dark/playlist-1btn-dark-pressed@2x.png \
	Resources/mainwindow_dark/playlist-1btn-dark.png \
	Resources/mainwindow_dark/playlist-1btn-dark@2x.png \
	Resources/mainwindow_dark/playlist-pressed_dark.png \
	Resources/mainwindow_dark/playlist-pressed_dark@2x.png \
	Resources/mainwindow_dark/playlist_dark.png \
	Resources/mainwindow_dark/playlist_dark@2x.png \
	Resources/mainwindow_dark/previous-6btns-dark-pressed.png \
	Resources/mainwindow_dark/previous-6btns-dark-pressed@2x.png \
	Resources/mainwindow_dark/previous-6btns-dark.png \
	Resources/mainwindow_dark/previous-6btns-dark@2x.png \
	Resources/mainwindow_dark/progressbar-fill-left_dark.png \
	Resources/mainwindow_dark/progressbar-fill-left_dark@2x.png \
	Resources/mainwindow_dark/progressbar-fill-middle_dark.png \
	Resources/mainwindow_dark/progressbar-fill-middle_dark@2x.png \
	Resources/mainwindow_dark/progressbar-fill-right_dark.png \
	Resources/mainwindow_dark/progressbar-fill-right_dark@2x.png \
	Resources/mainwindow_dark/progression-knob_dark.png \
	Resources/mainwindow_dark/progression-knob_dark@2x.png \
	Resources/mainwindow_dark/progression-track-wrapper-left_dark.png \
	Resources/mainwindow_dark/progression-track-wrapper-left_dark@2x.png \
	Resources/mainwindow_dark/progression-track-wrapper-middle_dark.png \
	Resources/mainwindow_dark/progression-track-wrapper-middle_dark@2x.png \
	Resources/mainwindow_dark/progression-track-wrapper-right_dark.png \
	Resources/mainwindow_dark/progression-track-wrapper-right_dark@2x.png \
	Resources/mainwindow_dark/repeat-all-blue-pressed_dark.png \
	Resources/mainwindow_dark/repeat-all-blue-pressed_dark@2x.png \
	Resources/mainwindow_dark/repeat-all-blue_dark.png \
	Resources/mainwindow_dark/repeat-all-blue_dark@2x.png \
	Resources/mainwindow_dark/repeat-one-blue-pressed_dark.png \
	Resources/mainwindow_dark/repeat-one-blue-pressed_dark@2x.png \
	Resources/mainwindow_dark/repeat-one-blue_dark.png \
	Resources/mainwindow_dark/repeat-one-blue_dark@2x.png \
	Resources/mainwindow_dark/repeat-pressed_dark.png \
	Resources/mainwindow_dark/repeat-pressed_dark@2x.png \
	Resources/mainwindow_dark/repeat_dark.png \
	Resources/mainwindow_dark/repeat_dark@2x.png \
	Resources/mainwindow_dark/shuffle-blue-pressed_dark.png \
	Resources/mainwindow_dark/shuffle-blue-pressed_dark@2x.png \
	Resources/mainwindow_dark/shuffle-blue_dark.png \
	Resources/mainwindow_dark/shuffle-blue_dark@2x.png \
	Resources/mainwindow_dark/shuffle-pressed_dark.png \
	Resources/mainwindow_dark/shuffle-pressed_dark@2x.png \
	Resources/mainwindow_dark/shuffle_dark.png \
	Resources/mainwindow_dark/shuffle_dark@2x.png \
	Resources/mainwindow_dark/stop-pressed_dark.png \
	Resources/mainwindow_dark/stop-pressed_dark@2x.png \
	Resources/mainwindow_dark/stop_dark.png \
	Resources/mainwindow_dark/stop_dark@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close-graphite.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close-on-graphite.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close-on-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close-on.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close-on@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close-over-graphite.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close-over-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close-over.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close-over@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-close@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-on.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-on@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-over.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-over@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-graphite.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on-graphite.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over-graphite.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-minimize@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-graphite.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on-graphite.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over-graphite.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over@2x.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom.png \
	Resources/mainwindow_dark/titlebar/lion/lion-window-zoom@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over-graphite.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over-graphite@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over@2x.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom.png \
	Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom@2x.png \
	Resources/mainwindow_dark/topbar-dark-center-fill.png \
	Resources/mainwindow_dark/topbar-dark-center-fill@2x.png \
	Resources/mainwindow_dark/topbar-dark-left.png \
	Resources/mainwindow_dark/topbar-dark-left@2x.png \
	Resources/mainwindow_dark/topbar-dark-right.png \
	Resources/mainwindow_dark/topbar-dark-right@2x.png \
	Resources/mainwindow_dark/volume-high_dark.png \
	Resources/mainwindow_dark/volume-high_dark@2x.png \
	Resources/mainwindow_dark/volume-low_dark.png \
	Resources/mainwindow_dark/volume-low_dark@2x.png \
	Resources/mainwindow_dark/volume-slider-knob_dark.png \
	Resources/mainwindow_dark/volume-slider-knob_dark@2x.png \
	Resources/mainwindow_dark/volume-slider-track_dark.png \
	Resources/mainwindow_dark/volume-slider-track_dark@2x.png \
	Resources/mainwindow_dark/window-resize.png \
	Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark-pressed.png \
	Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark-pressed@2x.png \
	Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark.png \
	Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark-pressed.png \
	Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark-pressed@2x.png \
	Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark.png \
	Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-bottom-background_dark.png \
	Resources/mainwindow_dark_yosemite/ys-bottom-background_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-bottomdark-left.png \
	Resources/mainwindow_dark_yosemite/ys-bottomdark-left@2x.png \
	Resources/mainwindow_dark_yosemite/ys-bottomdark-right.png \
	Resources/mainwindow_dark_yosemite/ys-bottomdark-right@2x.png \
	Resources/mainwindow_dark_yosemite/ys-effects-double-buttons-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-effects-double-buttons-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-effects-double-buttons_dark.png \
	Resources/mainwindow_dark_yosemite/ys-effects-double-buttons_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-effects-one-button-pressed-dark.png \
	Resources/mainwindow_dark_yosemite/ys-effects-one-button-pressed-dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-effects-one-button_dark.png \
	Resources/mainwindow_dark_yosemite/ys-effects-one-button_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark-pressed.png \
	Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark-pressed@2x.png \
	Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark.png \
	Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark-pressed.png \
	Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark-pressed@2x.png \
	Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark.png \
	Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons_dark.png \
	Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button_dark.png \
	Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-next-6btns-dark-pressed.png \
	Resources/mainwindow_dark_yosemite/ys-next-6btns-dark-pressed@2x.png \
	Resources/mainwindow_dark_yosemite/ys-next-6btns-dark.png \
	Resources/mainwindow_dark_yosemite/ys-next-6btns-dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-pause-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-pause-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-pause_dark.png \
	Resources/mainwindow_dark_yosemite/ys-pause_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-play-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-play-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-play_dark.png \
	Resources/mainwindow_dark_yosemite/ys-play_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark-pressed.png \
	Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark-pressed@2x.png \
	Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark.png \
	Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-playlist-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-playlist-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-playlist_dark.png \
	Resources/mainwindow_dark_yosemite/ys-playlist_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark-pressed.png \
	Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark-pressed@2x.png \
	Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark.png \
	Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-progressbar-fill-left_dark.png \
	Resources/mainwindow_dark_yosemite/ys-progressbar-fill-left_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-progressbar-fill-middle_dark.png \
	Resources/mainwindow_dark_yosemite/ys-progressbar-fill-middle_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-progressbar-fill-right_dark.png \
	Resources/mainwindow_dark_yosemite/ys-progressbar-fill-right_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-progression-knob_dark.png \
	Resources/mainwindow_dark_yosemite/ys-progression-knob_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-left_dark.png \
	Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-left_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-middle_dark.png \
	Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-middle_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-right_dark.png \
	Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-right_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-repeat-all-blue-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-repeat-all-blue-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-repeat-all-blue_dark.png \
	Resources/mainwindow_dark_yosemite/ys-repeat-all-blue_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-repeat-one-blue-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-repeat-one-blue-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-repeat-one-blue_dark.png \
	Resources/mainwindow_dark_yosemite/ys-repeat-one-blue_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-repeat-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-repeat-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-repeat_dark.png \
	Resources/mainwindow_dark_yosemite/ys-repeat_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-shuffle-blue-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-shuffle-blue-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-shuffle-blue_dark.png \
	Resources/mainwindow_dark_yosemite/ys-shuffle-blue_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-shuffle-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-shuffle-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-shuffle_dark.png \
	Resources/mainwindow_dark_yosemite/ys-shuffle_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-stop-pressed_dark.png \
	Resources/mainwindow_dark_yosemite/ys-stop-pressed_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-stop_dark.png \
	Resources/mainwindow_dark_yosemite/ys-stop_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-topbar-dark-center-fill.png \
	Resources/mainwindow_dark_yosemite/ys-topbar-dark-center-fill@2x.png \
	Resources/mainwindow_dark_yosemite/ys-topbar-dark-left.png \
	Resources/mainwindow_dark_yosemite/ys-topbar-dark-left@2x.png \
	Resources/mainwindow_dark_yosemite/ys-topbar-dark-right.png \
	Resources/mainwindow_dark_yosemite/ys-topbar-dark-right@2x.png \
	Resources/mainwindow_dark_yosemite/ys-volume-high_dark.png \
	Resources/mainwindow_dark_yosemite/ys-volume-high_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-volume-low_dark.png \
	Resources/mainwindow_dark_yosemite/ys-volume-low_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-volume-slider-knob_dark.png \
	Resources/mainwindow_dark_yosemite/ys-volume-slider-knob_dark@2x.png \
	Resources/mainwindow_dark_yosemite/ys-volume-slider-track_dark.png \
	Resources/mainwindow_dark_yosemite/ys-volume-slider-track_dark@2x.png \
	Resources/mainwindow_yosemite/ys-backward-3btns-pressed.png \
	Resources/mainwindow_yosemite/ys-backward-3btns-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-backward-3btns.png \
	Resources/mainwindow_yosemite/ys-backward-3btns@2x.png \
	Resources/mainwindow_yosemite/ys-backward-6btns-pressed.png \
	Resources/mainwindow_yosemite/ys-backward-6btns-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-backward-6btns.png \
	Resources/mainwindow_yosemite/ys-backward-6btns@2x.png \
	Resources/mainwindow_yosemite/ys-bottom-background.png \
	Resources/mainwindow_yosemite/ys-bottom-background@2x.png \
	Resources/mainwindow_yosemite/ys-dropzone.png \
	Resources/mainwindow_yosemite/ys-dropzone@2x.png \
	Resources/mainwindow_yosemite/ys-effects-double-buttons-pressed.png \
	Resources/mainwindow_yosemite/ys-effects-double-buttons-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-effects-double-buttons.png \
	Resources/mainwindow_yosemite/ys-effects-double-buttons@2x.png \
	Resources/mainwindow_yosemite/ys-effects-one-button-pressed.png \
	Resources/mainwindow_yosemite/ys-effects-one-button-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-effects-one-button.png \
	Resources/mainwindow_yosemite/ys-effects-one-button@2x.png \
	Resources/mainwindow_yosemite/ys-forward-3btns-pressed.png \
	Resources/mainwindow_yosemite/ys-forward-3btns-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-forward-3btns.png \
	Resources/mainwindow_yosemite/ys-forward-3btns@2x.png \
	Resources/mainwindow_yosemite/ys-forward-6btns-pressed.png \
	Resources/mainwindow_yosemite/ys-forward-6btns-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-forward-6btns.png \
	Resources/mainwindow_yosemite/ys-forward-6btns@2x.png \
	Resources/mainwindow_yosemite/ys-fullscreen-double-buttons-pressed.png \
	Resources/mainwindow_yosemite/ys-fullscreen-double-buttons-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-fullscreen-double-buttons.png \
	Resources/mainwindow_yosemite/ys-fullscreen-double-buttons@2x.png \
	Resources/mainwindow_yosemite/ys-fullscreen-one-button-pressed.png \
	Resources/mainwindow_yosemite/ys-fullscreen-one-button-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-fullscreen-one-button.png \
	Resources/mainwindow_yosemite/ys-fullscreen-one-button@2x.png \
	Resources/mainwindow_yosemite/ys-next-6btns-pressed.png \
	Resources/mainwindow_yosemite/ys-next-6btns-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-next-6btns.png \
	Resources/mainwindow_yosemite/ys-next-6btns@2x.png \
	Resources/mainwindow_yosemite/ys-pause-pressed.png \
	Resources/mainwindow_yosemite/ys-pause-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-pause.png \
	Resources/mainwindow_yosemite/ys-pause@2x.png \
	Resources/mainwindow_yosemite/ys-play-pressed.png \
	Resources/mainwindow_yosemite/ys-play-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-play.png \
	Resources/mainwindow_yosemite/ys-play@2x.png \
	Resources/mainwindow_yosemite/ys-playlist-1btn-pressed.png \
	Resources/mainwindow_yosemite/ys-playlist-1btn-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-playlist-1btn.png \
	Resources/mainwindow_yosemite/ys-playlist-1btn@2x.png \
	Resources/mainwindow_yosemite/ys-playlist-btn-pressed.png \
	Resources/mainwindow_yosemite/ys-playlist-btn-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-playlist-btn.png \
	Resources/mainwindow_yosemite/ys-playlist-btn@2x.png \
	Resources/mainwindow_yosemite/ys-previous-6btns-pressed.png \
	Resources/mainwindow_yosemite/ys-previous-6btns-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-previous-6btns.png \
	Resources/mainwindow_yosemite/ys-previous-6btns@2x.png \
	Resources/mainwindow_yosemite/ys-progression-fill-left.png \
	Resources/mainwindow_yosemite/ys-progression-fill-left@2x.png \
	Resources/mainwindow_yosemite/ys-progression-fill-middle.png \
	Resources/mainwindow_yosemite/ys-progression-fill-middle@2x.png \
	Resources/mainwindow_yosemite/ys-progression-fill-right.png \
	Resources/mainwindow_yosemite/ys-progression-fill-right@2x.png \
	Resources/mainwindow_yosemite/ys-progression-knob.png \
	Resources/mainwindow_yosemite/ys-progression-knob@2x.png \
	Resources/mainwindow_yosemite/ys-progression-track-wrapper-left.png \
	Resources/mainwindow_yosemite/ys-progression-track-wrapper-left@2x.png \
	Resources/mainwindow_yosemite/ys-progression-track-wrapper-middle.png \
	Resources/mainwindow_yosemite/ys-progression-track-wrapper-middle@2x.png \
	Resources/mainwindow_yosemite/ys-progression-track-wrapper-right.png \
	Resources/mainwindow_yosemite/ys-progression-track-wrapper-right@2x.png \
	Resources/mainwindow_yosemite/ys-repeat-all-pressed.png \
	Resources/mainwindow_yosemite/ys-repeat-all-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-repeat-all.png \
	Resources/mainwindow_yosemite/ys-repeat-all@2x.png \
	Resources/mainwindow_yosemite/ys-repeat-one-pressed.png \
	Resources/mainwindow_yosemite/ys-repeat-one-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-repeat-one.png \
	Resources/mainwindow_yosemite/ys-repeat-one@2x.png \
	Resources/mainwindow_yosemite/ys-repeat-pressed.png \
	Resources/mainwindow_yosemite/ys-repeat-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-repeat.png \
	Resources/mainwindow_yosemite/ys-repeat@2x.png \
	Resources/mainwindow_yosemite/ys-shuffle-blue-pressed.png \
	Resources/mainwindow_yosemite/ys-shuffle-blue-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-shuffle-blue.png \
	Resources/mainwindow_yosemite/ys-shuffle-blue@2x.png \
	Resources/mainwindow_yosemite/ys-shuffle-pressed.png \
	Resources/mainwindow_yosemite/ys-shuffle-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-shuffle.png \
	Resources/mainwindow_yosemite/ys-shuffle@2x.png \
	Resources/mainwindow_yosemite/ys-stop-pressed.png \
	Resources/mainwindow_yosemite/ys-stop-pressed@2x.png \
	Resources/mainwindow_yosemite/ys-stop.png \
	Resources/mainwindow_yosemite/ys-stop@2x.png \
	Resources/mainwindow_yosemite/ys-volume-high.png \
	Resources/mainwindow_yosemite/ys-volume-high@2x.png \
	Resources/mainwindow_yosemite/ys-volume-low.png \
	Resources/mainwindow_yosemite/ys-volume-low@2x.png \
	Resources/mainwindow_yosemite/ys-volume-slider-knob.png \
	Resources/mainwindow_yosemite/ys-volume-slider-knob@2x.png \
	Resources/mainwindow_yosemite/ys-volume-slider-track.png \
	Resources/mainwindow_yosemite/ys-volume-slider-track@2x.png \
	Resources/noart.png \
	Resources/Pref-Icons/VLCAudioCone.png \
	Resources/Pref-Icons/VLCHotkeysCone.png \
	Resources/Pref-Icons/VLCInputCone.png \
	Resources/Pref-Icons/VLCInterfaceCone.png \
	Resources/Pref-Icons/VLCSubtitleCone.png \
	Resources/Pref-Icons/VLCVideoCone.png \
	Resources/sidebar-icons/sidebar-local.png \
	Resources/sidebar-icons/sidebar-local@2x.png \
	Resources/sidebar-icons/sidebar-movie.png \
	Resources/sidebar-icons/sidebar-movie@2x.png \
	Resources/sidebar-icons/sidebar-music.png \
	Resources/sidebar-icons/sidebar-music@2x.png \
	Resources/sidebar-icons/sidebar-pictures.png \
	Resources/sidebar-icons/sidebar-pictures@2x.png \
	Resources/sidebar-icons/sidebar-playlist.png \
	Resources/sidebar-icons/sidebar-playlist@2x.png \
	Resources/sidebar-icons/sidebar-podcast.png \
	Resources/sidebar-icons/sidebar-podcast@2x.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-local.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-local@2x.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-movie.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-movie@2x.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-music.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-music@2x.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-pictures.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-pictures@2x.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-playlist.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-playlist@2x.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-podcast.png \
	Resources/sidebar-icons_yosemite/ys-sidebar-podcast@2x.png \
	Resources/Button-Icons/VLCNextTemplate.pdf \
	Resources/Button-Icons/VLCPauseTemplate.pdf \
	Resources/Button-Icons/VLCPlayTemplate.pdf \
	Resources/Button-Icons/VLCPreviousTemplate.pdf \
	Resources/Button-Icons/VLCShuffleTemplate.pdf \
	Resources/Button-Icons/VLCStatusBarIcon.pdf \
	Resources/Button-Icons/VLCBackwardTemplate.pdf \
	Resources/Button-Icons/VLCForwardTemplate.pdf \
	Resources/Button-Icons/VLCFullscreenOnTemplate.pdf \
	Resources/Button-Icons/VLCFullscreenOffTemplate.pdf \
	Resources/Button-Icons/VLCVolumeOnTemplate.pdf \
	Resources/Button-Icons/VLCVolumeOffTemplate.pdf \
	Resources/Button-Icons/VLCClearLogTemplate.pdf \
	Resources/Button-Icons/VLCSaveTemplate.pdf

DEPS_res += \
	Resources/dsa_pub.pem \
	Resources/README \
	Resources/vlc.scriptSuite \
	Resources/vlc.scriptTerminology

libmacosx_plugin_ladir = $(prefix)/share/macosx/Contents/Resources

if HAVE_DARWIN
dist_libmacosx_plugin_la_DATA = $(DEPS_res)
endif
