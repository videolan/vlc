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

libmacosx_plugin_la_SOURCES = \
	gui/macosx/helpers.h \
	gui/macosx/VLCAboutWindowController.h gui/macosx/VLCAboutWindowController.m \
	gui/macosx/VLCAddonListItem.h gui/macosx/VLCAddonListItem.m \
	gui/macosx/VLCAddonsWindowController.h gui/macosx/VLCAddonsWindowController.m \
	gui/macosx/applescript.h gui/macosx/applescript.m \
	gui/macosx/AppleRemote.h gui/macosx/AppleRemote.m \
	gui/macosx/CompatibilityFixes.h gui/macosx/CompatibilityFixes.m \
	gui/macosx/VLCControlsBarCommon.m gui/macosx/VLCControlsBarCommon.h \
	gui/macosx/VLCDocumentController.m gui/macosx/VLCDocumentController.h \
	gui/macosx/VLCMainWindowControlsBar.m gui/macosx/VLCMainWindowControlsBar.h \
	gui/macosx/VLCConvertAndSaveWindowController.h gui/macosx/VLCConvertAndSaveWindowController.m \
	gui/macosx/VLCCoreInteraction.h gui/macosx/VLCCoreInteraction.m \
	gui/macosx/VLCErrorWindowController.h gui/macosx/VLCErrorWindowController.m \
	gui/macosx/VLCExtensionsDialogProvider.h gui/macosx/VLCExtensionsDialogProvider.m \
	gui/macosx/VLCExtensionsManager.h gui/macosx/VLCExtensionsManager.m \
	gui/macosx/VLCHelpWindowController.h gui/macosx/VLCHelpWindowController.m \
	gui/macosx/VLCMain+OldPrefs.h gui/macosx/VLCMain+OldPrefs.m \
	gui/macosx/VLCMain.h gui/macosx/VLCMain.m \
	gui/macosx/VLCApplication.h gui/macosx/VLCApplication.m \
	gui/macosx/iTunes.h \
	gui/macosx/VLCKeyboardBacklightControl.h gui/macosx/VLCKeyboardBacklightControl.m \
	gui/macosx/macosx.m \
	gui/macosx/VLCMainMenu.h gui/macosx/VLCMainMenu.m \
	gui/macosx/VLCMainWindowTitleView.h gui/macosx/VLCMainWindowTitleView.m \
	gui/macosx/misc.h gui/macosx/misc.m \
	gui/macosx/NSSound+VLCAdditions.h gui/macosx/NSSound+VLCAdditions.m \
	gui/macosx/NSScreen+VLCAdditions.h gui/macosx/NSScreen+VLCAdditions.m \
	gui/macosx/VLCOpenWindowController.h gui/macosx/VLCOpenWindowController.m \
	gui/macosx/VLCOutput.h gui/macosx/VLCOutput.m \
	gui/macosx/VLCPlaylistView.h gui/macosx/VLCPlaylistView.m \
	gui/macosx/VLCPlaylist.h gui/macosx/VLCPlaylist.m \
	gui/macosx/VLCPlaylistInfo.h gui/macosx/VLCPlaylistInfo.m \
	gui/macosx/VLCPLItem.h gui/macosx/VLCPLItem.m \
	gui/macosx/VLCPLModel.h gui/macosx/VLCPLModel.m \
	gui/macosx/prefs.h gui/macosx/prefs.m \
	gui/macosx/prefs_widgets.h gui/macosx/prefs_widgets.m \
	gui/macosx/PXSourceList.h gui/macosx/PXSourceList.m \
	gui/macosx/PXSourceListDataSource.h gui/macosx/PXSourceListDelegate.h \
	gui/macosx/SideBarItem.h gui/macosx/SideBarItem.m \
	gui/macosx/VLCPopupPanelController.h gui/macosx/VLCPopupPanelController.m \
	gui/macosx/VLCTextfieldPanelController.h gui/macosx/VLCTextfieldPanelController.m \
	gui/macosx/VLCSimplePrefsController.h gui/macosx/VLCSimplePrefsController.m \
	gui/macosx/VLCSimplePrefsWindow.h gui/macosx/VLCSimplePrefsWindow.m \
	gui/macosx/VLCHotkeyChangeWindow.h gui/macosx/VLCHotkeyChangeWindow.m \
	gui/macosx/SPInvocationGrabbing.h gui/macosx/SPInvocationGrabbing.m \
	gui/macosx/SPMediaKeyTap.h gui/macosx/SPMediaKeyTap.m \
	gui/macosx/Spotify.h \
	gui/macosx/VLCStringUtility.h gui/macosx/VLCStringUtility.m \
	gui/macosx/VLCVoutView.h gui/macosx/VLCVoutView.m \
	gui/macosx/VLCTimeSelectionPanelController.h gui/macosx/VLCTimeSelectionPanelController.m \
	gui/macosx/VLCUIWidgets.h gui/macosx/VLCUIWidgets.m \
	gui/macosx/VLCScrollingClipView.h gui/macosx/VLCScrollingClipView.m \
	gui/macosx/VLCStatusBarIcon.h gui/macosx/VLCStatusBarIcon.m \
	gui/macosx/VLCVoutWindowController.h gui/macosx/VLCVoutWindowController.m \
	gui/macosx/Windows.h gui/macosx/Windows.m \
	gui/macosx/VLCAudioEffectsWindowController.h gui/macosx/VLCAudioEffectsWindowController.m \
	gui/macosx/VLCBookmarksWindowController.h gui/macosx/VLCBookmarksWindowController.m \
	gui/macosx/VLCBottomBarView.h gui/macosx/VLCBottomBarView.m \
	gui/macosx/VLCCoreDialogProvider.h gui/macosx/VLCCoreDialogProvider.m \
	gui/macosx/VLCLogMessage.h gui/macosx/VLCLogMessage.m \
	gui/macosx/VLCLogWindowController.h gui/macosx/VLCLogWindowController.m \
	gui/macosx/VLCHUDSliderCell.h gui/macosx/VLCHUDSliderCell.m \
	gui/macosx/VLCHUDButtonCell.h gui/macosx/VLCHUDButtonCell.m \
	gui/macosx/VLCHUDCheckboxCell.h gui/macosx/VLCHUDCheckboxCell.m \
	gui/macosx/VLCHUDTextFieldCell.h gui/macosx/VLCHUDTextFieldCell.m \
	gui/macosx/VLCHUDScroller.h gui/macosx/VLCHUDScroller.m \
	gui/macosx/VLCHUDOutlineView.h gui/macosx/VLCHUDOutlineView.m \
	gui/macosx/VLCHUDTableView.h gui/macosx/VLCHUDTableView.m \
	gui/macosx/VLCHUDTableCornerView.h gui/macosx/VLCHUDTableCornerView.m \
	gui/macosx/VLCHUDSegmentedCell.h gui/macosx/VLCHUDSegmentedCell.m \
	gui/macosx/VLCHUDStepperCell.h gui/macosx/VLCHUDStepperCell.m \
	gui/macosx/VLCHUDPopUpButtonCell.h gui/macosx/VLCHUDPopUpButtonCell.m \
	gui/macosx/VLCInputManager.h gui/macosx/VLCInputManager.m \
	gui/macosx/VLCMainWindow.h gui/macosx/VLCMainWindow.m \
	gui/macosx/VLCRendererDiscovery.h gui/macosx/VLCRendererDiscovery.m \
	gui/macosx/VLCRendererItem.h gui/macosx/VLCRendererItem.m \
	gui/macosx/VLCRendererMenuController.h gui/macosx/VLCRendererMenuController.m \
	gui/macosx/VLCResumeDialogController.h gui/macosx/VLCResumeDialogController.m \
	gui/macosx/VLCTrackSynchronizationWindowController.h gui/macosx/VLCTrackSynchronizationWindowController.m \
	gui/macosx/VLCVideoEffectsWindowController.h gui/macosx/VLCVideoEffectsWindowController.m \
	gui/macosx/VLCFSPanelController.h gui/macosx/VLCFSPanelController.m \
	gui/macosx/VLCFSPanelDraggableView.h gui/macosx/VLCFSPanelDraggableView.m \
	gui/macosx/VLCDefaultValueSlider.h gui/macosx/VLCDefaultValueSlider.m \
	gui/macosx/VLCDefaultValueSliderCell.h gui/macosx/VLCDefaultValueSliderCell.m \
	gui/macosx/VLCImageButton.h gui/macosx/VLCImageButton.m \
	gui/macosx/VLCTintedImageButtonCell.h gui/macosx/VLCTintedImageButtonCell.m \
	gui/macosx/VLCTimeField.h gui/macosx/VLCTimeField.m \
	gui/macosx/VLCHexNumberFormatter.h gui/macosx/VLCHexNumberFormatter.m \
	gui/macosx/VLCSlider.h gui/macosx/VLCSlider.m \
	gui/macosx/VLCSliderCell.h gui/macosx/VLCSliderCell.m \
	gui/macosx/VLCVolumeSlider.h gui/macosx/VLCVolumeSlider.m \
	gui/macosx/VLCVolumeSliderCell.h gui/macosx/VLCVolumeSliderCell.m \
	gui/macosx/VLCWrappableTextField.h gui/macosx/VLCWrappableTextField.m


# User interface compilation

nodist_libmacosx_plugin_la_SOURCES = \
	gui/macosx/UI/About.nib \
	gui/macosx/UI/AddonManager.nib \
	gui/macosx/UI/AudioEffects.nib \
	gui/macosx/UI/Bookmarks.nib \
	gui/macosx/UI/ConvertAndSave.nib \
	gui/macosx/UI/CoreDialogs.nib \
	gui/macosx/UI/DetachedVideoWindow.nib \
	gui/macosx/UI/ErrorPanel.nib \
	gui/macosx/UI/Help.nib \
	gui/macosx/UI/LogMessageWindow.nib \
	gui/macosx/UI/MainMenu.nib \
	gui/macosx/UI/MainWindow.nib \
	gui/macosx/UI/MediaInfo.nib \
	gui/macosx/UI/Open.nib \
	gui/macosx/UI/PlaylistAccessoryView.nib \
	gui/macosx/UI/PlaylistMenu.nib \
	gui/macosx/UI/PopupPanel.nib \
	gui/macosx/UI/Preferences.nib \
	gui/macosx/UI/ResumeDialog.nib \
	gui/macosx/UI/SimplePreferences.nib \
	gui/macosx/UI/StreamOutput.nib \
	gui/macosx/UI/SyncTracks.nib \
	gui/macosx/UI/TextfieldPanel.nib \
	gui/macosx/UI/TimeSelectionPanel.nib \
	gui/macosx/UI/VideoEffects.nib \
	gui/macosx/UI/VLCFullScreenPanel.nib \
	gui/macosx/UI/VLCStatusBarIconMainMenu.nib

EXTRA_DIST += \
	gui/macosx/UI/About.xib \
	gui/macosx/UI/AddonManager.xib \
	gui/macosx/UI/AudioEffects.xib \
	gui/macosx/UI/Bookmarks.xib \
	gui/macosx/UI/ConvertAndSave.xib \
	gui/macosx/UI/CoreDialogs.xib \
	gui/macosx/UI/DetachedVideoWindow.xib \
	gui/macosx/UI/ErrorPanel.xib \
	gui/macosx/UI/Help.xib \
	gui/macosx/UI/LogMessageWindow.xib \
	gui/macosx/UI/MainMenu.xib \
	gui/macosx/UI/MainWindow.xib \
	gui/macosx/UI/MediaInfo.xib \
	gui/macosx/UI/Open.xib \
	gui/macosx/UI/PlaylistAccessoryView.xib \
	gui/macosx/UI/PlaylistMenu.xib \
	gui/macosx/UI/PopupPanel.xib \
	gui/macosx/UI/Preferences.xib \
	gui/macosx/UI/ResumeDialog.xib \
	gui/macosx/UI/SimplePreferences.xib \
	gui/macosx/UI/StreamOutput.xib \
	gui/macosx/UI/SyncTracks.xib \
	gui/macosx/UI/TextfieldPanel.xib \
	gui/macosx/UI/TimeSelectionPanel.xib \
	gui/macosx/UI/VideoEffects.xib \
	gui/macosx/UI/VLCFullScreenPanel.xib \
	gui/macosx/UI/VLCStatusBarIconMainMenu.xib

xib_verbose = $(xib_verbose_$(V))
xib_verbose_ = $(xib_verbose__$(AM_DEFAULT_VERBOSITY))
xib_verbose_0 = @echo "  XIB   " $@;
xib_verbose__0 = $(xib_verbose_0)

.xib.nib:
	$(AM_V_at)mkdir -p -- gui/macosx/UI
	$(xib_verbose)$(XIB) --errors --warnings --notices \
	  --module VLC --auto-activate-custom-fonts --target-device mac \
	  --minimum-deployment-target 10.7 --output-format human-readable-text \
	  --compile $@ $<

if ENABLE_MACOSX_UI
gui_LTLIBRARIES += libmacosx_plugin.la
BUILT_SOURCES += $(nodist_libmacosx_plugin_la_SOURCES)
endif


libmacosx_plugin_la_RES = \
	gui/macosx/Resources/App-Icons/VLC-Xmas.icns \
	gui/macosx/Resources/App-Icons/VLC.icns \
	gui/macosx/Resources/File-Icons/aiff.icns \
	gui/macosx/Resources/File-Icons/audio.icns \
	gui/macosx/Resources/File-Icons/avi.icns \
	gui/macosx/Resources/File-Icons/flv.icns \
	gui/macosx/Resources/File-Icons/generic.icns \
	gui/macosx/Resources/File-Icons/m4a.icns \
	gui/macosx/Resources/File-Icons/m4v.icns \
	gui/macosx/Resources/File-Icons/mkv.icns \
	gui/macosx/Resources/File-Icons/mov.icns \
	gui/macosx/Resources/File-Icons/movie.icns \
	gui/macosx/Resources/File-Icons/mp3.icns \
	gui/macosx/Resources/File-Icons/mpeg.icns \
	gui/macosx/Resources/File-Icons/ogg.icns \
	gui/macosx/Resources/File-Icons/playlist.icns \
	gui/macosx/Resources/File-Icons/rm.icns \
	gui/macosx/Resources/File-Icons/subtitle.icns \
	gui/macosx/Resources/File-Icons/vob.icns \
	gui/macosx/Resources/File-Icons/wav.icns \
	gui/macosx/Resources/File-Icons/wma.icns \
	gui/macosx/Resources/File-Icons/wmv.icns \
	gui/macosx/Resources/mainwindow/backward-3btns-pressed.png \
	gui/macosx/Resources/mainwindow/backward-3btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow/backward-3btns.png \
	gui/macosx/Resources/mainwindow/backward-3btns@2x.png \
	gui/macosx/Resources/mainwindow/backward-6btns-pressed.png \
	gui/macosx/Resources/mainwindow/backward-6btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow/backward-6btns.png \
	gui/macosx/Resources/mainwindow/backward-6btns@2x.png \
	gui/macosx/Resources/mainwindow/dropzone-background.png \
	gui/macosx/Resources/mainwindow/dropzone.png \
	gui/macosx/Resources/mainwindow/dropzone@2x.png \
	gui/macosx/Resources/mainwindow/effects-double-buttons-pressed.png \
	gui/macosx/Resources/mainwindow/effects-double-buttons-pressed@2x.png \
	gui/macosx/Resources/mainwindow/effects-double-buttons.png \
	gui/macosx/Resources/mainwindow/effects-double-buttons@2x.png \
	gui/macosx/Resources/mainwindow/effects-one-button-pressed.png \
	gui/macosx/Resources/mainwindow/effects-one-button-pressed@2x.png \
	gui/macosx/Resources/mainwindow/effects-one-button.png \
	gui/macosx/Resources/mainwindow/effects-one-button@2x.png \
	gui/macosx/Resources/mainwindow/forward-3btns-pressed.png \
	gui/macosx/Resources/mainwindow/forward-3btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow/forward-3btns.png \
	gui/macosx/Resources/mainwindow/forward-3btns@2x.png \
	gui/macosx/Resources/mainwindow/forward-6btns-pressed.png \
	gui/macosx/Resources/mainwindow/forward-6btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow/forward-6btns.png \
	gui/macosx/Resources/mainwindow/forward-6btns@2x.png \
	gui/macosx/Resources/mainwindow/fullscreen-double-buttons-pressed.png \
	gui/macosx/Resources/mainwindow/fullscreen-double-buttons-pressed@2x.png \
	gui/macosx/Resources/mainwindow/fullscreen-double-buttons.png \
	gui/macosx/Resources/mainwindow/fullscreen-double-buttons@2x.png \
	gui/macosx/Resources/mainwindow/fullscreen-one-button-pressed.png \
	gui/macosx/Resources/mainwindow/fullscreen-one-button-pressed@2x.png \
	gui/macosx/Resources/mainwindow/fullscreen-one-button.png \
	gui/macosx/Resources/mainwindow/fullscreen-one-button@2x.png \
	gui/macosx/Resources/mainwindow/next-6btns-pressed.png \
	gui/macosx/Resources/mainwindow/next-6btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow/next-6btns.png \
	gui/macosx/Resources/mainwindow/next-6btns@2x.png \
	gui/macosx/Resources/mainwindow/pause-pressed.png \
	gui/macosx/Resources/mainwindow/pause-pressed@2x.png \
	gui/macosx/Resources/mainwindow/pause.png \
	gui/macosx/Resources/mainwindow/pause@2x.png \
	gui/macosx/Resources/mainwindow/play-pressed.png \
	gui/macosx/Resources/mainwindow/play-pressed@2x.png \
	gui/macosx/Resources/mainwindow/play.png \
	gui/macosx/Resources/mainwindow/play@2x.png \
	gui/macosx/Resources/mainwindow/playlist-1btn-pressed.png \
	gui/macosx/Resources/mainwindow/playlist-1btn-pressed@2x.png \
	gui/macosx/Resources/mainwindow/playlist-1btn.png \
	gui/macosx/Resources/mainwindow/playlist-1btn@2x.png \
	gui/macosx/Resources/mainwindow/playlist-btn-pressed.png \
	gui/macosx/Resources/mainwindow/playlist-btn-pressed@2x.png \
	gui/macosx/Resources/mainwindow/playlist-btn.png \
	gui/macosx/Resources/mainwindow/playlist-btn@2x.png \
	gui/macosx/Resources/mainwindow/previous-6btns-pressed.png \
	gui/macosx/Resources/mainwindow/previous-6btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow/previous-6btns.png \
	gui/macosx/Resources/mainwindow/previous-6btns@2x.png \
	gui/macosx/Resources/mainwindow/repeat-all-pressed.png \
	gui/macosx/Resources/mainwindow/repeat-all-pressed@2x.png \
	gui/macosx/Resources/mainwindow/repeat-all.png \
	gui/macosx/Resources/mainwindow/repeat-all@2x.png \
	gui/macosx/Resources/mainwindow/repeat-one-pressed.png \
	gui/macosx/Resources/mainwindow/repeat-one-pressed@2x.png \
	gui/macosx/Resources/mainwindow/repeat-one.png \
	gui/macosx/Resources/mainwindow/repeat-one@2x.png \
	gui/macosx/Resources/mainwindow/repeat-pressed.png \
	gui/macosx/Resources/mainwindow/repeat-pressed@2x.png \
	gui/macosx/Resources/mainwindow/repeat.png \
	gui/macosx/Resources/mainwindow/repeat@2x.png \
	gui/macosx/Resources/mainwindow/shuffle-blue-pressed.png \
	gui/macosx/Resources/mainwindow/shuffle-blue-pressed@2x.png \
	gui/macosx/Resources/mainwindow/shuffle-blue.png \
	gui/macosx/Resources/mainwindow/shuffle-blue@2x.png \
	gui/macosx/Resources/mainwindow/shuffle-pressed.png \
	gui/macosx/Resources/mainwindow/shuffle-pressed@2x.png \
	gui/macosx/Resources/mainwindow/shuffle.png \
	gui/macosx/Resources/mainwindow/shuffle@2x.png \
	gui/macosx/Resources/mainwindow/stop-pressed.png \
	gui/macosx/Resources/mainwindow/stop-pressed@2x.png \
	gui/macosx/Resources/mainwindow/stop.png \
	gui/macosx/Resources/mainwindow/stop@2x.png \
	gui/macosx/Resources/mainwindow/topbar_background.png \
	gui/macosx/Resources/mainwindow/volume-high.png \
	gui/macosx/Resources/mainwindow/volume-high@2x.png \
	gui/macosx/Resources/mainwindow/volume-low.png \
	gui/macosx/Resources/mainwindow/volume-low@2x.png \
	gui/macosx/Resources/mainwindow_dark/backward-3btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark/backward-3btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark/backward-3btns-dark.png \
	gui/macosx/Resources/mainwindow_dark/backward-3btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/backward-6btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark/backward-6btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark/backward-6btns-dark.png \
	gui/macosx/Resources/mainwindow_dark/backward-6btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/effects-double-buttons-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/effects-double-buttons-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/effects-double-buttons_dark.png \
	gui/macosx/Resources/mainwindow_dark/effects-double-buttons_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/effects-one-button-pressed-dark.png \
	gui/macosx/Resources/mainwindow_dark/effects-one-button-pressed-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/effects-one-button_dark.png \
	gui/macosx/Resources/mainwindow_dark/effects-one-button_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/forward-3btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark/forward-3btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark/forward-3btns-dark.png \
	gui/macosx/Resources/mainwindow_dark/forward-3btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/forward-6btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark/forward-6btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark/forward-6btns-dark.png \
	gui/macosx/Resources/mainwindow_dark/forward-6btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/fullscreen-double-buttons-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/fullscreen-double-buttons-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/fullscreen-double-buttons_dark.png \
	gui/macosx/Resources/mainwindow_dark/fullscreen-double-buttons_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/fullscreen-one-button-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/fullscreen-one-button-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/fullscreen-one-button_dark.png \
	gui/macosx/Resources/mainwindow_dark/fullscreen-one-button_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/next-6btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark/next-6btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark/next-6btns-dark.png \
	gui/macosx/Resources/mainwindow_dark/next-6btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/pause-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/pause-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/pause_dark.png \
	gui/macosx/Resources/mainwindow_dark/pause_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/play-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/play-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/play_dark.png \
	gui/macosx/Resources/mainwindow_dark/play_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/playlist-1btn-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark/playlist-1btn-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark/playlist-1btn-dark.png \
	gui/macosx/Resources/mainwindow_dark/playlist-1btn-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/playlist-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/playlist-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/playlist_dark.png \
	gui/macosx/Resources/mainwindow_dark/playlist_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/previous-6btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark/previous-6btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark/previous-6btns-dark.png \
	gui/macosx/Resources/mainwindow_dark/previous-6btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/repeat-all-blue-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/repeat-all-blue-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/repeat-all-blue_dark.png \
	gui/macosx/Resources/mainwindow_dark/repeat-all-blue_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/repeat-one-blue-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/repeat-one-blue-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/repeat-one-blue_dark.png \
	gui/macosx/Resources/mainwindow_dark/repeat-one-blue_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/repeat-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/repeat-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/repeat_dark.png \
	gui/macosx/Resources/mainwindow_dark/repeat_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/shuffle-blue-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/shuffle-blue-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/shuffle-blue_dark.png \
	gui/macosx/Resources/mainwindow_dark/shuffle-blue_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/shuffle-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/shuffle-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/shuffle_dark.png \
	gui/macosx/Resources/mainwindow_dark/shuffle_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/stop-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark/stop-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/stop_dark.png \
	gui/macosx/Resources/mainwindow_dark/stop_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-on.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-on@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-over.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-over@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over-graphite.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over-graphite@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over@2x.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom.png \
	gui/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom@2x.png \
	gui/macosx/Resources/mainwindow_dark/topbar-dark-center-fill.png \
	gui/macosx/Resources/mainwindow_dark/topbar-dark-center-fill@2x.png \
	gui/macosx/Resources/mainwindow_dark/topbar-dark-left.png \
	gui/macosx/Resources/mainwindow_dark/topbar-dark-left@2x.png \
	gui/macosx/Resources/mainwindow_dark/topbar-dark-right.png \
	gui/macosx/Resources/mainwindow_dark/topbar-dark-right@2x.png \
	gui/macosx/Resources/mainwindow_dark/volume-high_dark.png \
	gui/macosx/Resources/mainwindow_dark/volume-high_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/volume-low_dark.png \
	gui/macosx/Resources/mainwindow_dark/volume-low_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark/window-resize.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button-pressed-dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button-pressed-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-pause-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-pause-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-pause_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-pause_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-play-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-play-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-play_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-play_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-playlist_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-playlist_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark-pressed.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark-pressed@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-repeat_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-stop-pressed_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-stop-pressed_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-stop_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-stop_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-center-fill.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-center-fill@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-left.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-left@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-right.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-right@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-volume-high_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-volume-high_dark@2x.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-volume-low_dark.png \
	gui/macosx/Resources/mainwindow_dark_yosemite/ys-volume-low_dark@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-backward-3btns-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-backward-3btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-backward-3btns.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-backward-3btns@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-backward-6btns-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-backward-6btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-backward-6btns.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-backward-6btns@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-dropzone.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-dropzone@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-effects-one-button-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-effects-one-button-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-effects-one-button.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-effects-one-button@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-forward-3btns-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-forward-3btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-forward-3btns.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-forward-3btns@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-forward-6btns-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-forward-6btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-forward-6btns.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-forward-6btns@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-next-6btns-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-next-6btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-next-6btns.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-next-6btns@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-pause-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-pause-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-pause.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-pause@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-play-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-play-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-play.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-play@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-playlist-btn-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-playlist-btn-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-playlist-btn.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-playlist-btn@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-previous-6btns-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-previous-6btns-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-previous-6btns.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-previous-6btns@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat-all-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat-all-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat-all.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat-all@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat-one-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat-one-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat-one.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat-one@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-repeat@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-shuffle-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-shuffle-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-shuffle.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-shuffle@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-stop-pressed.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-stop-pressed@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-stop.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-stop@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-volume-high.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-volume-high@2x.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-volume-low.png \
	gui/macosx/Resources/mainwindow_yosemite/ys-volume-low@2x.png \
	gui/macosx/Resources/noart.png \
	gui/macosx/Resources/Pref-Icons/VLCAudioCone.png \
	gui/macosx/Resources/Pref-Icons/VLCHotkeysCone.png \
	gui/macosx/Resources/Pref-Icons/VLCInputCone.png \
	gui/macosx/Resources/Pref-Icons/VLCInterfaceCone.png \
	gui/macosx/Resources/Pref-Icons/VLCSubtitleCone.png \
	gui/macosx/Resources/Pref-Icons/VLCVideoCone.png \
	gui/macosx/Resources/sidebar-icons/sidebar-local.png \
	gui/macosx/Resources/sidebar-icons/sidebar-local@2x.png \
	gui/macosx/Resources/sidebar-icons/sidebar-movie.png \
	gui/macosx/Resources/sidebar-icons/sidebar-movie@2x.png \
	gui/macosx/Resources/sidebar-icons/sidebar-music.png \
	gui/macosx/Resources/sidebar-icons/sidebar-music@2x.png \
	gui/macosx/Resources/sidebar-icons/sidebar-pictures.png \
	gui/macosx/Resources/sidebar-icons/sidebar-pictures@2x.png \
	gui/macosx/Resources/sidebar-icons/sidebar-playlist.png \
	gui/macosx/Resources/sidebar-icons/sidebar-playlist@2x.png \
	gui/macosx/Resources/sidebar-icons/sidebar-podcast.png \
	gui/macosx/Resources/sidebar-icons/sidebar-podcast@2x.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-local.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-local@2x.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-movie.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-movie@2x.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-music.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-music@2x.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-pictures.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-pictures@2x.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-playlist.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-playlist@2x.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-podcast.png \
	gui/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-podcast@2x.png \
	gui/macosx/Resources/Button-Icons/VLCNextTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCPauseTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCPlayTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCPreviousTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCShuffleTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCStatusBarIcon.pdf \
	gui/macosx/Resources/Button-Icons/VLCBackwardTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCForwardTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCFullscreenOnTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCFullscreenOffTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCVolumeOnTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCVolumeOffTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCClearLogTemplate.pdf \
	gui/macosx/Resources/Button-Icons/VLCSaveTemplate.pdf

libmacosx_plugin_la_RES += \
	gui/macosx/Resources/dsa_pub.pem \
	gui/macosx/Resources/README \
	gui/macosx/Resources/vlc.scriptSuite \
	gui/macosx/Resources/vlc.scriptTerminology

if ENABLE_MACOSX_UI
dist_libmacosx_plugin_la_DATA = $(libmacosx_plugin_la_RES)
endif
libmacosx_plugin_ladir = $(prefix)/share/macosx/Contents/Resources
