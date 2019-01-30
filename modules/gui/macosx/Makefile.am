SUFFIXES += .xib .nib

xib_verbose = $(xib_verbose_$(V))
xib_verbose_ = $(xib_verbose__$(AM_DEFAULT_VERBOSITY))
xib_verbose_0 = @echo "  XIB     " $@;
xib_verbose__0 = $(xib_verbose_0)

.xib.nib:
	$(AM_V_at)mkdir -p -- gui/macosx/UI
	$(xib_verbose)$(XIB) --errors --warnings --notices \
	  --module VLC --auto-activate-custom-fonts --target-device mac \
	  --minimum-deployment-target 10.11 --output-format human-readable-text \
	  --compile $@ $<

libmacosx_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-exceptions -fobjc-arc
libmacosx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(guidir)' \
	-Wl,-framework,Cocoa -Wl,-framework,CoreServices \
	-Wl,-framework,AVFoundation -Wl,-framework,CoreMedia -Wl,-framework,IOKit \
	-Wl,-framework,AddressBook -Wl,-framework,WebKit -Wl,-framework,CoreAudio \
	-Wl,-framework,SystemConfiguration -Wl,-framework,ScriptingBridge \
	-Wl,-framework,QuartzCore -Wl,-weak_framework,MediaPlayer

if HAVE_SPARKLE
libmacosx_plugin_la_LDFLAGS += -Wl,-framework,Sparkle
endif

libmacosx_plugin_la_SOURCES = \
	gui/macosx/helpers.h \
	gui/macosx/NSString+Helpers.h gui/macosx/NSString+Helpers.m \
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
	gui/macosx/VLCClickerManager.h gui/macosx/VLCClickerManager.m \
	gui/macosx/VLCVideoFilterHelper.h gui/macosx/VLCVideoFilterHelper.m \
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
	gui/macosx/VLCPopupPanelController.h gui/macosx/VLCPopupPanelController.m \
	gui/macosx/VLCTextfieldPanelController.h gui/macosx/VLCTextfieldPanelController.m \
	gui/macosx/VLCSimplePrefsController.h gui/macosx/VLCSimplePrefsController.m \
	gui/macosx/VLCSimplePrefsWindow.h gui/macosx/VLCSimplePrefsWindow.m \
	gui/macosx/VLCHotkeyChangeWindow.h gui/macosx/VLCHotkeyChangeWindow.m \
	gui/macosx/SPMediaKeyTap.h gui/macosx/SPMediaKeyTap.m \
	gui/macosx/Spotify.h \
	gui/macosx/VLCVoutView.h gui/macosx/VLCVoutView.m \
	gui/macosx/VLCTimeSelectionPanelController.h gui/macosx/VLCTimeSelectionPanelController.m \
	gui/macosx/VLCUIWidgets.h gui/macosx/VLCUIWidgets.m \
	gui/macosx/VLCScrollingClipView.h gui/macosx/VLCScrollingClipView.m \
	gui/macosx/VLCStatusBarIcon.h gui/macosx/VLCStatusBarIcon.m \
	gui/macosx/VLCVideoOutputProvider.h gui/macosx/VLCVideoOutputProvider.m \
	gui/macosx/VLCWindow.h gui/macosx/VLCWindow.m \
	gui/macosx/VLCVideoWindowCommon.h gui/macosx/VLCVideoWindowCommon.m \
	gui/macosx/VLCAudioEffectsWindowController.h gui/macosx/VLCAudioEffectsWindowController.m \
	gui/macosx/VLCBookmarksWindowController.h gui/macosx/VLCBookmarksWindowController.m \
	gui/macosx/VLCBottomBarView.h gui/macosx/VLCBottomBarView.m \
	gui/macosx/VLCCoreDialogProvider.h gui/macosx/VLCCoreDialogProvider.m \
	gui/macosx/VLCLogMessage.h gui/macosx/VLCLogMessage.m \
	gui/macosx/VLCLogWindowController.h gui/macosx/VLCLogWindowController.m \
	gui/macosx/VLCHUDOutlineView.h gui/macosx/VLCHUDOutlineView.m \
	gui/macosx/VLCHUDTableView.h gui/macosx/VLCHUDTableView.m \
	gui/macosx/VLCHUDTableCornerView.h gui/macosx/VLCHUDTableCornerView.m \
	gui/macosx/VLCInputManager.h gui/macosx/VLCInputManager.m \
	gui/macosx/VLCRemoteControlService.h gui/macosx/VLCRemoteControlService.m \
	gui/macosx/VLCMainWindow.h gui/macosx/VLCMainWindow.m \
	gui/macosx/VLCLibraryWindow.h gui/macosx/VLCLibraryWindow.m \
	gui/macosx/VLCDetachedVideoWindow.h gui/macosx/VLCDetachedVideoWindow.m \
	gui/macosx/VLCRendererDiscovery.h gui/macosx/VLCRendererDiscovery.m \
	gui/macosx/VLCRendererItem.h gui/macosx/VLCRendererItem.m \
	gui/macosx/VLCRendererMenuController.h gui/macosx/VLCRendererMenuController.m \
	gui/macosx/VLCResumeDialogController.h gui/macosx/VLCResumeDialogController.m \
	gui/macosx/VLCSourceListItem.h gui/macosx/VLCSourceListItem.m \
	gui/macosx/VLCSourceListTableCellView.h gui/macosx/VLCSourceListTableCellView.m \
	gui/macosx/VLCSourceListBadgeButton.h gui/macosx/VLCSourceListBadgeButton.m \
	gui/macosx/VLCTrackSynchronizationWindowController.h gui/macosx/VLCTrackSynchronizationWindowController.m \
	gui/macosx/VLCVideoEffectsWindowController.h gui/macosx/VLCVideoEffectsWindowController.m \
	gui/macosx/VLCFSPanelController.h gui/macosx/VLCFSPanelController.m \
	gui/macosx/VLCFSPanelDraggableView.h gui/macosx/VLCFSPanelDraggableView.m \
	gui/macosx/VLCDefaultValueSlider.h gui/macosx/VLCDefaultValueSlider.m \
	gui/macosx/VLCDefaultValueSliderCell.h gui/macosx/VLCDefaultValueSliderCell.m \
	gui/macosx/VLCImageButton.h gui/macosx/VLCImageButton.m \
	gui/macosx/VLCTimeField.h gui/macosx/VLCTimeField.m \
	gui/macosx/VLCHexNumberFormatter.h gui/macosx/VLCHexNumberFormatter.m \
	gui/macosx/VLCSlider.h gui/macosx/VLCSlider.m \
	gui/macosx/VLCSliderCell.h gui/macosx/VLCSliderCell.m \
	gui/macosx/VLCVolumeSlider.h gui/macosx/VLCVolumeSlider.m \
	gui/macosx/VLCVolumeSliderCell.h gui/macosx/VLCVolumeSliderCell.m \
	gui/macosx/VLCWrappableTextField.h gui/macosx/VLCWrappableTextField.m

# PXSourceList sources
libmacosx_plugin_la_SOURCES += \
	gui/macosx/PXSourceList/PXSourceList.h \
	gui/macosx/PXSourceList/PXSourceList.m \
	gui/macosx/PXSourceList/PXSourceListBadgeCell.h \
	gui/macosx/PXSourceList/PXSourceListBadgeCell.m \
	gui/macosx/PXSourceList/PXSourceListBadgeView.h \
	gui/macosx/PXSourceList/PXSourceListBadgeView.m \
	gui/macosx/PXSourceList/PXSourceListItem.h \
	gui/macosx/PXSourceList/PXSourceListItem.m \
	gui/macosx/PXSourceList/PXSourceListTableCellView.h \
	gui/macosx/PXSourceList/PXSourceListTableCellView.m \
	gui/macosx/PXSourceList/Internal/PXSourceListDelegateDataSourceProxy.h \
	gui/macosx/PXSourceList/Internal/PXSourceListDelegateDataSourceProxy.m \
	gui/macosx/PXSourceList/Internal/PXSourceListRuntimeAdditions.h \
	gui/macosx/PXSourceList/Internal/PXSourceListRuntimeAdditions.m \
	gui/macosx/PXSourceList/Internal/PXSourceListPrivateConstants.h \
	gui/macosx/PXSourceList/PXSourceListDataSource.h \
	gui/macosx/PXSourceList/PXSourceListDelegate.h

# User interface compilation

libmacosx_plugin_la_XIB_SOURCES = \
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
	gui/macosx/UI/VLCLibraryWindow.xib \
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

libmacosx_plugin_la_SOURCES += $(libmacosx_plugin_la_XIB_SOURCES)

if ENABLE_MACOSX_UI
gui_LTLIBRARIES += libmacosx_plugin.la
BUILT_SOURCES += $(libmacosx_plugin_la_XIB_SOURCES:.xib=.nib)
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
