include $(top_srcdir)/modules/common.am

guidir = $(pluginsdir)/gui


libmacosx_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-exceptions -fobjc-arc -F${CONTRIB_DIR}
libmacosx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(guidir)' \
	-Wl,-framework,Cocoa -Wl,-framework,Carbon -Wl,-framework,CoreServices \
	-Wl,-framework,AVFoundation -Wl,-framework,CoreMedia -Wl,-framework,IOKit \
	-Wl,-framework,AddressBook -Wl,-framework,WebKit -Wl,-framework,CoreAudio \
	-Wl,-framework,SystemConfiguration -Wl,-framework,ScriptingBridge \
	-F${CONTRIB_DIR} -Wl,-framework,BGHUDAppKit -Wl,-framework,QuartzCore

if HAVE_SPARKLE
libmacosx_plugin_la_LDFLAGS += -Wl,-framework,Sparkle
endif

EXTRA_LTLIBRARIES = libmacosx_plugin.la
gui_LTLIBRARIES = $(LTLIBmacosx)

libmacosx_plugin_la_SOURCES = \
	helpers.h \
	about.h about.m \
	AddonListDataSource.h AddonListDataSource.m \
	AddonsWindowController.h AddonsWindowController.m \
	applescript.h applescript.m \
	AppleRemote.h AppleRemote.m \
	AudioEffects.h AudioEffects.m \
	bookmarks.h bookmarks.m \
	BWQuincyManager.h BWQuincyManager.m \
	BWQuincyUI.h BWQuincyUI.m \
	CompatibilityFixes.h \
	ControlsBar.m ControlsBar.h \
	ConvertAndSave.h ConvertAndSave.m \
	coredialogs.h coredialogs.m \
	CoreInteraction.h CoreInteraction.m \
	DebugMessageVisualizer.h DebugMessageVisualizer.m \
	ExtensionsDialogProvider.h ExtensionsDialogProvider.m \
	ExtensionsManager.h ExtensionsManager.m \
	eyetv.h eyetv.m \
	fspanel.h fspanel.m \
	intf-prefs.h intf-prefs.m \
	intf.h intf.m \
	InputManager.h InputManager.m \
	iTunes.h \
	KeyboardBacklight.h KeyboardBacklight.m \
	macosx.m \
	MainMenu.h MainMenu.m \
	MainWindow.h MainWindow.m \
	MainWindowTitle.h MainWindowTitle.m \
	misc.h misc.m \
	open.h open.m \
	output.h output.m \
	VLCPlaylistView.h VLCPlaylistView.m \
	VLCPlaylist.h VLCPlaylist.m \
	VLCPlaylistInfo.h VLCPlaylistInfo.m \
	PLItem.h PLItem.m \
	PLModel.h PLModel.m \
	prefs.h prefs.m \
	prefs_widgets.h prefs_widgets.m \
	PXSourceList.h PXSourceList.m \
	PXSourceListDataSource.h PXSourceListDelegate.h \
	ResumeDialogController.h ResumeDialogController.m \
	SideBarItem.h SideBarItem.m \
	SharedDialogs.h SharedDialogs.m \
	simple_prefs.h simple_prefs.m \
	SPInvocationGrabbing.h SPInvocationGrabbing.m \
	SPMediaKeyTap.h SPMediaKeyTap.m \
	Spotify.h \
	StringUtility.h StringUtility.m \
	TrackSynchronization.h TrackSynchronization.m \
	VideoEffects.h VideoEffects.m \
	VideoView.h VideoView.m \
	VLCTimeSelectionPanelController.h VLCTimeSelectionPanelController.m \
	VLCUIWidgets.h VLCUIWidgets.m \
	VLCScrollingClipView.h VLCScrollingClipView.m \
	VLCStatusBarIcon.h VLCStatusBarIcon.m \
	VLCVoutWindowController.h VLCVoutWindowController.m \
	Windows.h Windows.m \
	VLCHUDSliderCell.h VLCHUDSliderCell.m \
	VLCHUDButtonCell.h VLCHUDButtonCell.m \
	VLCHUDCheckboxCell.h VLCHUDCheckboxCell.m \
	VLCHUDRadiobuttonCell.h VLCHUDRadiobuttonCell.m \
	VLCHUDTextFieldCell.h VLCHUDTextFieldCell.m \
	VLCRendererDiscovery.h VLCRendererDiscovery.m \
	VLCRendererItem.h VLCRendererItem.m \
	VLCRendererDialog.h VLCRendererDialog.m
