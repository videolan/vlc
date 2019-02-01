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


libmacosx_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-exceptions -fobjc-arc -I$(srcdir)/gui/macosx
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
	gui/macosx/coreinteraction/VLCClickerManager.h \
	gui/macosx/coreinteraction/VLCClickerManager.m \
	gui/macosx/coreinteraction/VLCCoreInteraction.h \
	gui/macosx/coreinteraction/VLCCoreInteraction.m \
	gui/macosx/coreinteraction/VLCInputManager.h \
	gui/macosx/coreinteraction/VLCInputManager.m \
	gui/macosx/coreinteraction/VLCVideoFilterHelper.h \
	gui/macosx/coreinteraction/VLCVideoFilterHelper.m \
	gui/macosx/extensions/NSScreen+VLCAdditions.h \
	gui/macosx/extensions/NSScreen+VLCAdditions.m \
	gui/macosx/extensions/NSSound+VLCAdditions.h \
	gui/macosx/extensions/NSSound+VLCAdditions.m \
	gui/macosx/extensions/NSString+Helpers.h \
	gui/macosx/extensions/NSString+Helpers.m \
	gui/macosx/extensions/VLCHexNumberFormatter.h \
	gui/macosx/extensions/VLCHexNumberFormatter.m \
	gui/macosx/extensions/helpers.h \
	gui/macosx/extensions/misc.h \
	gui/macosx/extensions/misc.m \
	gui/macosx/imported/AppleRemote/AppleRemote.h \
	gui/macosx/imported/AppleRemote/AppleRemote.m \
	gui/macosx/imported/PXSourceList/Internal/PXSourceListDelegateDataSourceProxy.h \
	gui/macosx/imported/PXSourceList/Internal/PXSourceListDelegateDataSourceProxy.m \
	gui/macosx/imported/PXSourceList/Internal/PXSourceListPrivateConstants.h \
	gui/macosx/imported/PXSourceList/Internal/PXSourceListRuntimeAdditions.h \
	gui/macosx/imported/PXSourceList/Internal/PXSourceListRuntimeAdditions.m \
	gui/macosx/imported/PXSourceList/PXSourceList.h \
	gui/macosx/imported/PXSourceList/PXSourceList.m \
	gui/macosx/imported/PXSourceList/PXSourceListBadgeCell.h \
	gui/macosx/imported/PXSourceList/PXSourceListBadgeCell.m \
	gui/macosx/imported/PXSourceList/PXSourceListBadgeView.h \
	gui/macosx/imported/PXSourceList/PXSourceListBadgeView.m \
	gui/macosx/imported/PXSourceList/PXSourceListDataSource.h \
	gui/macosx/imported/PXSourceList/PXSourceListDelegate.h \
	gui/macosx/imported/PXSourceList/PXSourceListItem.h \
	gui/macosx/imported/PXSourceList/PXSourceListItem.m \
	gui/macosx/imported/PXSourceList/PXSourceListTableCellView.h \
	gui/macosx/imported/PXSourceList/PXSourceListTableCellView.m \
	gui/macosx/imported/SPMediaKeyTap/SPMediaKeyTap.h \
	gui/macosx/imported/SPMediaKeyTap/SPMediaKeyTap.m \
	gui/macosx/library/VLCLibraryCollectionViewItem.h \
	gui/macosx/library/VLCLibraryCollectionViewItem.m \
	gui/macosx/library/VLCLibraryWindow.h \
	gui/macosx/library/VLCLibraryWindow.m \
	gui/macosx/main/CompatibilityFixes.h \
	gui/macosx/main/CompatibilityFixes.m \
	gui/macosx/main/VLCApplication.h \
	gui/macosx/main/VLCApplication.m \
	gui/macosx/main/VLCMain+OldPrefs.h \
	gui/macosx/main/VLCMain+OldPrefs.m \
	gui/macosx/main/VLCMain.h \
	gui/macosx/main/VLCMain.m \
	gui/macosx/main/macosx.m \
	gui/macosx/menus/VLCMainMenu.h \
	gui/macosx/menus/VLCMainMenu.m \
	gui/macosx/menus/VLCStatusBarIcon.h \
	gui/macosx/menus/VLCStatusBarIcon.m \
	gui/macosx/menus/renderers/VLCRendererDiscovery.h \
	gui/macosx/menus/renderers/VLCRendererDiscovery.m \
	gui/macosx/menus/renderers/VLCRendererItem.h \
	gui/macosx/menus/renderers/VLCRendererItem.m \
	gui/macosx/menus/renderers/VLCRendererMenuController.h \
	gui/macosx/menus/renderers/VLCRendererMenuController.m \
	gui/macosx/os-integration/Spotify.h \
	gui/macosx/os-integration/VLCDocumentController.h \
	gui/macosx/os-integration/VLCDocumentController.m \
	gui/macosx/os-integration/VLCKeyboardBacklightControl.h \
	gui/macosx/os-integration/VLCKeyboardBacklightControl.m \
	gui/macosx/os-integration/VLCRemoteControlService.h \
	gui/macosx/os-integration/VLCRemoteControlService.m \
	gui/macosx/os-integration/applescript.h \
	gui/macosx/os-integration/applescript.m \
	gui/macosx/os-integration/iTunes.h \
	gui/macosx/panels/VLCAudioEffectsWindowController.h \
	gui/macosx/panels/VLCAudioEffectsWindowController.m \
	gui/macosx/panels/VLCBookmarksWindowController.h \
	gui/macosx/panels/VLCBookmarksWindowController.m \
	gui/macosx/panels/VLCPlaylistInfo.h \
	gui/macosx/panels/VLCPlaylistInfo.m \
	gui/macosx/panels/VLCTimeSelectionPanelController.h \
	gui/macosx/panels/VLCTimeSelectionPanelController.m \
	gui/macosx/panels/VLCTrackSynchronizationWindowController.h \
	gui/macosx/panels/VLCTrackSynchronizationWindowController.m \
	gui/macosx/panels/VLCVideoEffectsWindowController.h \
	gui/macosx/panels/VLCVideoEffectsWindowController.m \
	gui/macosx/panels/dialogs/VLCCoreDialogProvider.h \
	gui/macosx/panels/dialogs/VLCCoreDialogProvider.m \
	gui/macosx/panels/dialogs/VLCPopupPanelController.h \
	gui/macosx/panels/dialogs/VLCPopupPanelController.m \
	gui/macosx/panels/dialogs/VLCResumeDialogController.h \
	gui/macosx/panels/dialogs/VLCResumeDialogController.m \
	gui/macosx/panels/dialogs/VLCTextfieldPanelController.h \
	gui/macosx/panels/dialogs/VLCTextfieldPanelController.m \
	gui/macosx/playlist/VLCPlaylistController.h \
	gui/macosx/playlist/VLCPlaylistController.m \
	gui/macosx/playlist/VLCPlaylistDataSource.h \
	gui/macosx/playlist/VLCPlaylistDataSource.m \
	gui/macosx/playlist/VLCPlaylistItem.h \
	gui/macosx/playlist/VLCPlaylistItem.m \
	gui/macosx/playlist/VLCPlaylistMenuController.h \
	gui/macosx/playlist/VLCPlaylistMenuController.m \
	gui/macosx/playlist/VLCPlaylistModel.h \
	gui/macosx/playlist/VLCPlaylistModel.m \
	gui/macosx/playlist/VLCPlaylistTableCellView.h \
	gui/macosx/playlist/VLCPlaylistTableCellView.m \
	gui/macosx/playlist/VLCPlaylistTableView.h \
	gui/macosx/playlist/VLCPlaylistTableView.m \
	gui/macosx/preferences/VLCHotkeyChangeWindow.h \
	gui/macosx/preferences/VLCHotkeyChangeWindow.m \
	gui/macosx/preferences/VLCSimplePrefsController.h \
	gui/macosx/preferences/VLCSimplePrefsController.m \
	gui/macosx/preferences/VLCSimplePrefsWindow.h \
	gui/macosx/preferences/VLCSimplePrefsWindow.m \
	gui/macosx/preferences/prefs.h \
	gui/macosx/preferences/prefs.m \
	gui/macosx/preferences/prefs_widgets.h \
	gui/macosx/preferences/prefs_widgets.m \
	gui/macosx/views/VLCBottomBarView.h \
	gui/macosx/views/VLCBottomBarView.m \
	gui/macosx/views/VLCDefaultValueSlider.h \
	gui/macosx/views/VLCDefaultValueSlider.m \
	gui/macosx/views/VLCDefaultValueSliderCell.h \
	gui/macosx/views/VLCDefaultValueSliderCell.m \
	gui/macosx/views/VLCFSPanelDraggableView.h \
	gui/macosx/views/VLCFSPanelDraggableView.m \
	gui/macosx/views/VLCHUDOutlineView.h \
	gui/macosx/views/VLCHUDOutlineView.m \
	gui/macosx/views/VLCHUDTableCornerView.h \
	gui/macosx/views/VLCHUDTableCornerView.m \
	gui/macosx/views/VLCHUDTableView.h \
	gui/macosx/views/VLCHUDTableView.m \
	gui/macosx/views/VLCImageButton.h \
	gui/macosx/views/VLCImageButton.m \
	gui/macosx/views/VLCScrollingClipView.h \
	gui/macosx/views/VLCScrollingClipView.m \
	gui/macosx/views/VLCSlider.h \
	gui/macosx/views/VLCSlider.m \
	gui/macosx/views/VLCSliderCell.h \
	gui/macosx/views/VLCSliderCell.m \
	gui/macosx/views/VLCTimeField.h \
	gui/macosx/views/VLCTimeField.m \
	gui/macosx/views/VLCVolumeSlider.h \
	gui/macosx/views/VLCVolumeSlider.m \
	gui/macosx/views/VLCVolumeSliderCell.h \
	gui/macosx/views/VLCVolumeSliderCell.m \
	gui/macosx/views/VLCWrappableTextField.h \
	gui/macosx/views/VLCWrappableTextField.m \
	gui/macosx/windows/VLCAboutWindowController.h \
	gui/macosx/windows/VLCAboutWindowController.m \
	gui/macosx/windows/VLCErrorWindowController.h \
	gui/macosx/windows/VLCErrorWindowController.m \
	gui/macosx/windows/VLCHelpWindowController.h \
	gui/macosx/windows/VLCHelpWindowController.m \
	gui/macosx/windows/VLCOpenInputMetadata.h \
	gui/macosx/windows/VLCOpenInputMetadata.m \
	gui/macosx/windows/VLCOpenWindowController.h \
	gui/macosx/windows/VLCOpenWindowController.m \
	gui/macosx/windows/addons/VLCAddonListItem.h \
	gui/macosx/windows/addons/VLCAddonListItem.m \
	gui/macosx/windows/addons/VLCAddonsWindowController.h \
	gui/macosx/windows/addons/VLCAddonsWindowController.m \
	gui/macosx/windows/convertandsave/VLCConvertAndSaveWindowController.h \
	gui/macosx/windows/convertandsave/VLCConvertAndSaveWindowController.m \
	gui/macosx/windows/convertandsave/VLCOutput.h \
	gui/macosx/windows/convertandsave/VLCOutput.m \
	gui/macosx/windows/extensions/VLCExtensionsDialogProvider.h \
	gui/macosx/windows/extensions/VLCExtensionsDialogProvider.m \
	gui/macosx/windows/extensions/VLCExtensionsManager.h \
	gui/macosx/windows/extensions/VLCExtensionsManager.m \
	gui/macosx/windows/extensions/VLCUIWidgets.h \
	gui/macosx/windows/extensions/VLCUIWidgets.m \
	gui/macosx/windows/logging/VLCLogMessage.h \
	gui/macosx/windows/logging/VLCLogMessage.m \
	gui/macosx/windows/logging/VLCLogWindowController.h \
	gui/macosx/windows/logging/VLCLogWindowController.m \
	gui/macosx/windows/mainwindow/VLCControlsBarCommon.h \
	gui/macosx/windows/mainwindow/VLCControlsBarCommon.m \
	gui/macosx/windows/mainwindow/VLCMainWindow.h \
	gui/macosx/windows/mainwindow/VLCMainWindow.m \
	gui/macosx/windows/mainwindow/VLCMainWindowControlsBar.h \
	gui/macosx/windows/mainwindow/VLCMainWindowControlsBar.m \
	gui/macosx/windows/mainwindow/VLCSourceListBadgeButton.h \
	gui/macosx/windows/mainwindow/VLCSourceListBadgeButton.m \
	gui/macosx/windows/mainwindow/VLCSourceListItem.h \
	gui/macosx/windows/mainwindow/VLCSourceListItem.m \
	gui/macosx/windows/mainwindow/VLCSourceListTableCellView.h \
	gui/macosx/windows/mainwindow/VLCSourceListTableCellView.m \
	gui/macosx/windows/video/VLCDetachedVideoWindow.h \
	gui/macosx/windows/video/VLCDetachedVideoWindow.m \
	gui/macosx/windows/video/VLCFSPanelController.h \
	gui/macosx/windows/video/VLCFSPanelController.m \
	gui/macosx/windows/video/VLCVideoOutputProvider.h \
	gui/macosx/windows/video/VLCVideoOutputProvider.m \
	gui/macosx/windows/video/VLCVideoWindowCommon.h \
	gui/macosx/windows/video/VLCVideoWindowCommon.m \
	gui/macosx/windows/video/VLCVoutView.h \
	gui/macosx/windows/video/VLCVoutView.m \
	gui/macosx/windows/video/VLCWindow.h \
	gui/macosx/windows/video/VLCWindow.m


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
	gui/macosx/UI/VLCPlaylistMenu.xib \
	gui/macosx/UI/VLCPlaylistTableCellView.xib \
	gui/macosx/UI/VLCLibraryCollectionViewItem.xib \
	gui/macosx/UI/MediaInfo.xib \
	gui/macosx/UI/Open.xib \
	gui/macosx/UI/PlaylistAccessoryView.xib \
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
	gui/macosx/Resources/librarywindow/libraryCellGradient@1x.png \
	gui/macosx/Resources/librarywindow/libraryCellGradient@2x.png \
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
