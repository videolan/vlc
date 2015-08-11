###############################################################################
# Automake targets and declarations
###############################################################################

# SUBDIRS stores the directories where a "make" is required when building
# something. DIST_SUBDIRS stores the directories where nothing is built but
# which have makefiles with distribution information.
#  - src (libvlccore) is nedeed by modules
DIST_SUBDIRS = m4 compat doc po share src modules lib bin test
SUBDIRS = compat doc po share src modules lib
if BUILD_VLC
SUBDIRS += bin
endif
SUBDIRS += test

EXTRA_DIST = \
	extras/package/win32/vlc.exe.manifest \
	extras/package/win32/libvlc.dll.manifest \
	extras/package/win32/change-contribs-directory.sh \
	extras/package/win32/configure.sh \
	extras/package/npapi.am \
	extras/package/win32/NSIS/vlc.win32.nsi.in \
	extras/package/win32/NSIS/spad.nsi.in \
	extras/package/win32/NSIS/UAC/examples \
	extras/package/win32/NSIS/UAC/examples/UAC_AllowLUA.nsi \
	extras/package/win32/NSIS/UAC/examples/UAC_AdminOnly.nsi \
	extras/package/win32/NSIS/UAC/examples/UAC_Uninstaller.nsi \
	extras/package/win32/NSIS/UAC/examples/UAC.nsh \
	extras/package/win32/NSIS/UAC/examples/UAC_GetUserShellFolderPath.nsi \
	extras/package/win32/NSIS/UAC/examples/UAC_RealWorldFullyLoadedDualModeExample.nsi \
	extras/package/win32/NSIS/UAC/History.txt \
	extras/package/win32/NSIS/UAC/License.txt \
	extras/package/win32/NSIS/UAC/nsisutil.h \
	extras/package/win32/NSIS/UAC/resource.h \
	extras/package/win32/NSIS/UAC/resource.rc \
	extras/package/win32/NSIS/UAC/runas.cpp \
	extras/package/win32/NSIS/UAC/uac.cpp \
	extras/package/win32/NSIS/UAC/uac.h \
	extras/package/win32/NSIS/UAC/Readme.html \
	extras/package/win32/NSIS/UAC.nsh \
	extras/package/win32/NSIS/languages/declaration.nsh \
	extras/package/win32/NSIS/languages/bengali.nsh \
	extras/package/win32/NSIS/languages/basque.nsh \
	extras/package/win32/NSIS/languages/brazilian_portuguese.nsh \
	extras/package/win32/NSIS/languages/bulgarian.nsh \
	extras/package/win32/NSIS/languages/catalan.nsh \
	extras/package/win32/NSIS/languages/danish.nsh \
	extras/package/win32/NSIS/languages/dutch.nsh \
	extras/package/win32/NSIS/languages/english.nsh \
	extras/package/win32/NSIS/languages/estonian.nsh \
	extras/package/win32/NSIS/languages/finnish.nsh \
	extras/package/win32/NSIS/languages/french.nsh \
	extras/package/win32/NSIS/languages/galician.nsh \
	extras/package/win32/NSIS/languages/german.nsh \
	extras/package/win32/NSIS/languages/hebrew.nsh \
	extras/package/win32/NSIS/languages/hungarian.nsh \
	extras/package/win32/NSIS/languages/italian.nsh \
	extras/package/win32/NSIS/languages/japanese.nsh \
	extras/package/win32/NSIS/languages/lithuanian.nsh \
	extras/package/win32/NSIS/languages/occitan.nsh \
	extras/package/win32/NSIS/languages/polish.nsh \
	extras/package/win32/NSIS/languages/punjabi.nsh \
	extras/package/win32/NSIS/languages/russian.nsh
	extras/package/win32/NSIS/languages/romanian.nsh \
	extras/package/win32/NSIS/languages/schinese.nsh \
	extras/package/win32/NSIS/languages/slovak.nsh \
	extras/package/win32/NSIS/languages/slovenian.nsh \
	extras/package/win32/NSIS/languages/sorani.nsh \
	extras/package/win32/NSIS/languages/spanish.nsh \
	extras/package/win32/NSIS/languages/swedish.nsh

dist_noinst_SCRIPTS = bootstrap
nodist_noinst_SCRIPTS = compile

BUILT_SOURCES_distclean =

BUILT_SOURCES = $(BUILT_SOURCES_distclean)

SUFFIXES = 

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-fast-install \
	--disable-a52 \
	--disable-alsa \
	--disable-avcodec --disable-avformat \
	--disable-postproc --disable-swscale \
	--disable-dbus \
	--disable-mad --disable-libmpeg2 \
	--disable-faad --disable-skins2 \
	--disable-live555 \
	--disable-lua \
	--disable-fribidi --disable-glx \
	--disable-mkv \
	--with-kde-solid='$${datadir}/kde4/apps'

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = \
	1.11 \
	-Wall \
	check-news \
	dist-xz \
	no-dist-gzip
#	std-options

ChangeLog: Makefile.am
	rm -f -- "$@"
	cd doc && $(MAKE) $(AM_MAKEFLAGS) changelogs
	$(LN_S) -f doc/ChangeLog-2015 "$@"

###############################################################################
# Mac OS X project
###############################################################################

EXTRA_DIST += \
	extras/package/macosx/build-package.sh \
	extras/package/macosx/build.sh \
	extras/package/macosx/codesign-dp.sh \
	extras/package/macosx/codesign.sh \
	extras/package/macosx/configure.sh \
	extras/package/macosx/dmg_setup.scpt \
	extras/package/macosx/eyetvplugin/English.lproj/InfoPlist.strings \
	extras/package/macosx/eyetvplugin/EyeTV_Plugin_Installer.pmdoc/01vlc-contents.xml \
	extras/package/macosx/eyetvplugin/EyeTV_Plugin_Installer.pmdoc/01vlc.xml \
	extras/package/macosx/eyetvplugin/EyeTV_Plugin_Installer.pmdoc/index.xml \
	extras/package/macosx/eyetvplugin/eyetvplugin.c \
	extras/package/macosx/eyetvplugin/eyetvplugin.h \
	extras/package/macosx/eyetvplugin/eyetvplugin.xcodeproj/project.pbxproj \
	extras/package/macosx/eyetvplugin/EyeTVPluginDefs.h \
	extras/package/macosx/eyetvplugin/Info.plist \
	extras/package/macosx/fullscreen_panel.svg \
	extras/package/macosx/Info.plist.in \
	extras/package/macosx/package.mak \
	extras/package/macosx/README.MacOSX.rtf \
	extras/package/macosx/Resources/dsa_pub.pem \
	extras/package/macosx/Resources/English.lproj/About.xib \
	extras/package/macosx/Resources/English.lproj/AddonManager.xib \
	extras/package/macosx/Resources/English.lproj/AudioEffects.xib \
	extras/package/macosx/Resources/English.lproj/Bookmarks.xib \
	extras/package/macosx/Resources/English.lproj/BWQuincyMain.xib \
	extras/package/macosx/Resources/English.lproj/ConvertAndSave.xib \
	extras/package/macosx/Resources/English.lproj/CoreDialogs.xib \
	extras/package/macosx/Resources/English.lproj/DebugMessageVisualizer.xib \
	extras/package/macosx/Resources/English.lproj/DetachedVideoWindow.xib \
	extras/package/macosx/Resources/English.lproj/ErrorPanel.xib \
	extras/package/macosx/Resources/English.lproj/FSPanel.xib \
	extras/package/macosx/Resources/English.lproj/InfoPlist.strings.in \
	extras/package/macosx/Resources/English.lproj/Help.xib \
	extras/package/macosx/Resources/English.lproj/MainMenu.xib \
	extras/package/macosx/Resources/English.lproj/MainWindow.xib \
	extras/package/macosx/Resources/English.lproj/MediaInfo.xib \
	extras/package/macosx/Resources/English.lproj/Open.xib \
	extras/package/macosx/Resources/English.lproj/PlaylistAccessoryView.xib \
	extras/package/macosx/Resources/English.lproj/PlaylistMenu.xib \
	extras/package/macosx/Resources/English.lproj/PopupPanel.xib \
	extras/package/macosx/Resources/English.lproj/Preferences.xib \
	extras/package/macosx/Resources/English.lproj/ResumeDialog.xib \
	extras/package/macosx/Resources/English.lproj/SimplePreferences.xib \
	extras/package/macosx/Resources/English.lproj/StreamOutput.xib \
	extras/package/macosx/Resources/English.lproj/SyncTracks.xib \
	extras/package/macosx/Resources/English.lproj/TextfieldPanel.xib \
	extras/package/macosx/Resources/English.lproj/TimeSelectionPanel.xib \
	extras/package/macosx/Resources/English.lproj/VideoEffects.xib \
	extras/package/macosx/Resources/fspanel/fs_background.png \
	extras/package/macosx/Resources/fspanel/fs_background@2x.png \
	extras/package/macosx/Resources/fspanel/fs_exit_fullscreen.png \
	extras/package/macosx/Resources/fspanel/fs_exit_fullscreen@2x.png \
	extras/package/macosx/Resources/fspanel/fs_exit_fullscreen_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_exit_fullscreen_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_forward.png \
	extras/package/macosx/Resources/fspanel/fs_forward@2x.png \
	extras/package/macosx/Resources/fspanel/fs_forward_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_forward_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_pause.png \
	extras/package/macosx/Resources/fspanel/fs_pause@2x.png \
	extras/package/macosx/Resources/fspanel/fs_pause_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_pause_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_play.png \
	extras/package/macosx/Resources/fspanel/fs_play@2x.png \
	extras/package/macosx/Resources/fspanel/fs_play_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_play_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_rewind.png \
	extras/package/macosx/Resources/fspanel/fs_rewind@2x.png \
	extras/package/macosx/Resources/fspanel/fs_rewind_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_rewind_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_skip_next.png \
	extras/package/macosx/Resources/fspanel/fs_skip_next@2x.png \
	extras/package/macosx/Resources/fspanel/fs_skip_next_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_skip_next_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_skip_previous.png \
	extras/package/macosx/Resources/fspanel/fs_skip_previous@2x.png \
	extras/package/macosx/Resources/fspanel/fs_skip_previous_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_skip_previous_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_stop.png \
	extras/package/macosx/Resources/fspanel/fs_stop@2x.png \
	extras/package/macosx/Resources/fspanel/fs_stop_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_stop_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider@2x.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider_knob.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider_knob@2x.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider_knob_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider_knob_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_volume_max.png \
	extras/package/macosx/Resources/fspanel/fs_volume_max@2x.png \
	extras/package/macosx/Resources/fspanel/fs_volume_max_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_volume_max_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_volume_mute.png \
	extras/package/macosx/Resources/fspanel/fs_volume_mute@2x.png \
	extras/package/macosx/Resources/fspanel/fs_volume_mute_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_volume_mute_highlight@2x.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_bar.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_bar@2x.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_knob.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_knob@2x.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_knob_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_knob_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_background.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_background@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_exit_fullscreen.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_exit_fullscreen@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_exit_fullscreen_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_exit_fullscreen_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_forward.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_forward@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_forward_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_forward_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_pause.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_pause@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_pause_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_pause_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_play.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_play@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_play_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_play_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_rewind.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_rewind@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_rewind_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_rewind_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_skip_next.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_skip_next@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_skip_next_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_skip_next_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_skip_previous.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_skip_previous@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_skip_previous_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_skip_previous_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_stop.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_stop@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_stop_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_stop_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_time_slider.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_time_slider@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_time_slider_knob.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_time_slider_knob@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_time_slider_knob_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_time_slider_knob_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_max.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_max@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_max_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_max_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_mute.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_mute@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_mute_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_mute_highlight@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_slider_bar.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_slider_bar@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_slider_knob.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_slider_knob@2x.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_slider_knob_highlight.png \
	extras/package/macosx/Resources/fspanel-yosemite/ys-fs_volume_slider_knob_highlight@2x.png \
	extras/package/macosx/Resources/icons/aiff.icns \
	extras/package/macosx/Resources/icons/audio.icns \
	extras/package/macosx/Resources/icons/avi.icns \
	extras/package/macosx/Resources/icons/flv.icns \
	extras/package/macosx/Resources/icons/generic.icns \
	extras/package/macosx/Resources/icons/m4a.icns \
	extras/package/macosx/Resources/icons/m4v.icns \
	extras/package/macosx/Resources/icons/mkv.icns \
	extras/package/macosx/Resources/icons/mov.icns \
	extras/package/macosx/Resources/icons/movie.icns \
	extras/package/macosx/Resources/icons/mp3.icns \
	extras/package/macosx/Resources/icons/mpeg.icns \
	extras/package/macosx/Resources/icons/ogg.icns \
	extras/package/macosx/Resources/icons/playlist.icns \
	extras/package/macosx/Resources/icons/rm.icns \
	extras/package/macosx/Resources/icons/subtitle.icns \
	extras/package/macosx/Resources/icons/vlc-xmas.png \
	extras/package/macosx/Resources/icons/vlc.icns \
	extras/package/macosx/Resources/icons/vob.icns \
	extras/package/macosx/Resources/icons/wav.icns \
	extras/package/macosx/Resources/icons/wma.icns \
	extras/package/macosx/Resources/icons/wmv.icns \
	extras/package/macosx/Resources/mainwindow/backward-3btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/backward-3btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/backward-3btns.png \
	extras/package/macosx/Resources/mainwindow/backward-3btns@2x.png \
	extras/package/macosx/Resources/mainwindow/backward-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/backward-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/backward-6btns.png \
	extras/package/macosx/Resources/mainwindow/backward-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow/bottom-background.png \
	extras/package/macosx/Resources/mainwindow/bottom-background@2x.png \
	extras/package/macosx/Resources/mainwindow/dropzone-background.png \
	extras/package/macosx/Resources/mainwindow/dropzone.png \
	extras/package/macosx/Resources/mainwindow/dropzone@2x.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons-pressed.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons@2x.png \
	extras/package/macosx/Resources/mainwindow/effects-one-button-pressed.png \
	extras/package/macosx/Resources/mainwindow/effects-one-button-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/effects-one-button.png \
	extras/package/macosx/Resources/mainwindow/effects-one-button@2x.png \
	extras/package/macosx/Resources/mainwindow/forward-3btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/forward-3btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/forward-3btns.png \
	extras/package/macosx/Resources/mainwindow/forward-3btns@2x.png \
	extras/package/macosx/Resources/mainwindow/forward-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/forward-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/forward-6btns.png \
	extras/package/macosx/Resources/mainwindow/forward-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-double-buttons-pressed.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-double-buttons-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-double-buttons.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-double-buttons@2x.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-one-button-pressed.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-one-button-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-one-button.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-one-button@2x.png \
	extras/package/macosx/Resources/mainwindow/next-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/next-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/next-6btns.png \
	extras/package/macosx/Resources/mainwindow/next-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow/pause-pressed.png \
	extras/package/macosx/Resources/mainwindow/pause-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/pause.png \
	extras/package/macosx/Resources/mainwindow/pause@2x.png \
	extras/package/macosx/Resources/mainwindow/play-pressed.png \
	extras/package/macosx/Resources/mainwindow/play-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/play.png \
	extras/package/macosx/Resources/mainwindow/play@2x.png \
	extras/package/macosx/Resources/mainwindow/playlist-1btn-pressed.png \
	extras/package/macosx/Resources/mainwindow/playlist-1btn-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/playlist-1btn.png \
	extras/package/macosx/Resources/mainwindow/playlist-1btn@2x.png \
	extras/package/macosx/Resources/mainwindow/playlist-btn-pressed.png \
	extras/package/macosx/Resources/mainwindow/playlist-btn-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/playlist-btn.png \
	extras/package/macosx/Resources/mainwindow/playlist-btn@2x.png \
	extras/package/macosx/Resources/mainwindow/previous-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/previous-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/previous-6btns.png \
	extras/package/macosx/Resources/mainwindow/previous-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-left.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-left@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-middle.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-middle@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-right.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-right@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-knob.png \
	extras/package/macosx/Resources/mainwindow/progression-knob@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-left.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-left@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-middle.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-middle@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-right.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-right@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat-all-pressed.png \
	extras/package/macosx/Resources/mainwindow/repeat-all-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat-all.png \
	extras/package/macosx/Resources/mainwindow/repeat-all@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat-one-pressed.png \
	extras/package/macosx/Resources/mainwindow/repeat-one-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat-one.png \
	extras/package/macosx/Resources/mainwindow/repeat-one@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat-pressed.png \
	extras/package/macosx/Resources/mainwindow/repeat-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat.png \
	extras/package/macosx/Resources/mainwindow/repeat@2x.png \
	extras/package/macosx/Resources/mainwindow/shuffle-blue-pressed.png \
	extras/package/macosx/Resources/mainwindow/shuffle-blue-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/shuffle-blue.png \
	extras/package/macosx/Resources/mainwindow/shuffle-blue@2x.png \
	extras/package/macosx/Resources/mainwindow/shuffle-pressed.png \
	extras/package/macosx/Resources/mainwindow/shuffle-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/shuffle.png \
	extras/package/macosx/Resources/mainwindow/shuffle@2x.png \
	extras/package/macosx/Resources/mainwindow/stop-pressed.png \
	extras/package/macosx/Resources/mainwindow/stop-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/stop.png \
	extras/package/macosx/Resources/mainwindow/stop@2x.png \
	extras/package/macosx/Resources/mainwindow/topbar_background.png \
	extras/package/macosx/Resources/mainwindow/volume-high.png \
	extras/package/macosx/Resources/mainwindow/volume-high@2x.png \
	extras/package/macosx/Resources/mainwindow/volume-low.png \
	extras/package/macosx/Resources/mainwindow/volume-low@2x.png \
	extras/package/macosx/Resources/mainwindow/volume-slider-knob.png \
	extras/package/macosx/Resources/mainwindow/volume-slider-knob@2x.png \
	extras/package/macosx/Resources/mainwindow/volume-slider-track.png \
	extras/package/macosx/Resources/mainwindow/volume-slider-track@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-3btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-3btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-3btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-3btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/bottom-background_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/bottom-background_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/bottomdark-left.png \
	extras/package/macosx/Resources/mainwindow_dark/bottomdark-left@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/bottomdark-right.png \
	extras/package/macosx/Resources/mainwindow_dark/bottomdark-right@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-one-button-pressed-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-one-button-pressed-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-one-button_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-one-button_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-3btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-3btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-3btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-3btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-double-buttons-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-double-buttons-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-double-buttons_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-double-buttons_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-one-button-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-one-button-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-one-button_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-one-button_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/next-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/next-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/next-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/next-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/pause-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/pause-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/pause_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/pause_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/play-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/play-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/play_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/play_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-1btn-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-1btn-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-1btn-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-1btn-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/previous-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/previous-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/previous-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/previous-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-left_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-left_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-middle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-middle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-right_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-right_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-knob_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-left_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-left_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-middle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-middle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-right_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-right_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-all-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-all-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-all-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-all-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-one-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-one-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-one-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-one-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/stop-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/stop-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/stop_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/stop_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-center-fill.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-center-fill@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-left.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-left@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-right.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-right@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-high_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-high_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-low_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-low_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-knob_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-track_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-track_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/window-resize.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottom-background_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottom-background_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottomdark-left.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottomdark-left@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottomdark-right.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottomdark-right@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button-pressed-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button-pressed-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-pause-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-pause-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-pause_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-pause_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-play-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-play-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-play_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-play_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-left_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-left_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-middle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-middle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-right_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-right_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-knob_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-left_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-left_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-middle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-middle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-right_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-right_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-stop-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-stop-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-stop_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-stop_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-center-fill.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-center-fill@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-left.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-left@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-right.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-right@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-high_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-high_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-low_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-low_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-slider-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-slider-knob_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-slider-track_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-slider-track_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-3btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-3btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-3btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-3btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-6btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-bottom-background.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-bottom-background@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-dropzone.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-dropzone@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-one-button-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-one-button-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-one-button.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-one-button@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-3btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-3btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-3btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-3btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-6btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-next-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-next-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-next-6btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-next-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-pause-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-pause-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-pause.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-pause@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-play-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-play-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-play.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-play@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-btn-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-btn-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-btn.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-btn@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-previous-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-previous-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-previous-6btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-previous-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-left.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-left@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-middle.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-middle@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-right.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-right@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-knob.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-knob@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-left.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-left@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-middle.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-middle@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-right.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-right@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-all-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-all-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-all.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-all@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-one-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-one-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-one.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-one@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-stop-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-stop-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-stop.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-stop@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-high.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-high@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-low.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-low@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-slider-knob.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-slider-knob@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-slider-track.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-slider-track@2x.png \
	extras/package/macosx/Resources/noart.png \
	extras/package/macosx/Resources/prefs/spref_cone_Audio_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Hotkeys_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Input_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Interface_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Subtitles_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Video_64.png \
	extras/package/macosx/Resources/README \
	extras/package/macosx/Resources/sidebar-icons/sidebar-local.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-local@2x.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-movie.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-movie@2x.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-music.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-music@2x.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-pictures.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-pictures@2x.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-playlist.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-playlist@2x.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-podcast.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-podcast@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-local.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-local@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-movie.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-movie@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-music.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-music@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-pictures.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-pictures@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-playlist.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-playlist@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-podcast.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-podcast@2x.png \
	extras/package/macosx/Resources/vlc.scriptSuite \
	extras/package/macosx/Resources/vlc.scriptTerminology \
	extras/package/macosx/ub.sh \
	extras/package/macosx/VLC.entitlements \
	extras/package/macosx/vlc.xcodeproj/project.pbxproj


###############################################################################
# iOS project
##############################################################################
EXTRA_DIST += \
	extras/package/ios/build.sh

###############################################################################
# tools (needed for contrib)
##############################################################################
EXTRA_DIST += \
	extras/tools/bootstrap \
	extras/tools/packages.mak \
	extras/tools/tools.mak \
	extras/tools/ragel-6.8-javacodegen.patch

###############################################################################
# Various utilities ( editor syntax files, D-Bus controller ... )
##############################################################################
EXTRA_DIST += \
	extras/analyser/zsh_completion.sh \
	extras/analyser/zsh.cpp \
	extras/analyser/emacs.init \
	extras/analyser/vlc.vim \
	extras/analyser/valgrind.suppressions \
	extras/buildsystem/make.pl \
	extras/misc/mpris.py \
	extras/misc/mpris.xml

###############################################################################
# Scripts for building dependencies.
##############################################################################
EXTRA_DIST += \
	contrib/bootstrap \
	contrib/src

###############################################################################
# Building libvlc
###############################################################################

CLEANFILES =
DISTCLEANFILES = $(BUILT_SOURCES_distclean) compile doltcompile doltlibtool
MAINTAINERCLEANFILES = ChangeLog

# Shortcut for developers to rebuild the core (libvlc + vlc)
# Don't use it if you don't know what it is about.
# Don't complain if it doesn't work. -- Courmisch
libcompat:
	cd compat && $(MAKE) $(AM_MAKEFLAGS)

libvlccore: libcompat
	cd src && $(MAKE) $(AM_MAKEFLAGS) libvlccore.la

libvlc: libvlccore
	cd lib && $(MAKE) $(AM_MAKEFLAGS) libvlc.la

core: libvlc vlc$(EXEEXT)
	cd bin && $(MAKE) $(AM_MAKEFLAGS) vlc$(EXEEXT) vlc-static$(EXEEXT)

doc:
	cd doc && $(MAKE) $(AM_MAKEFLAGS) doc

.PHONY: libvlc core doc

###############################################################################
# Building aliases
###############################################################################

ALL_ALIASES = cvlc rvlc svlc qvlc nvlc
bin_SCRIPTS = $(ALIASES)
CLEANFILES += $(ALIASES) $(noinst_SCRIPTS)
EXTRA_SCRIPTS = $(ALL_ALIASES)

dist_noinst_SCRIPTS += make-alias

MKALIAS = bindir="$(bindir)" transform="$(transform)" program_prefix="$(program_prefix)" program_suffix="$(program_suffix)" $(top_srcdir)/make-alias $@

cvlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) dummy

rvlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) rc

svlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) skins2

qvlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) qt4

nvlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) ncurses

if BUILD_VLC
noinst_SCRIPTS = vlc$(EXEEXT)
endif

vlc$(EXEEXT):
if HAVE_DARWIN
	$(AM_V_GEN)$(LN_S) -f bin/vlc-osx-static vlc
else
	$(AM_V_GEN)$(LN_S) -f bin/vlc-static$(EXEEXT) vlc$(EXEEXT)
endif

TESTS = test/run_vlc.sh
dist_noinst_SCRIPTS += test/run_vlc.sh

###############################################################################
# Installing plugins cache
###############################################################################
install-exec-hook:
	if test "$(build)" = "$(host)"; then \
		PATH="$(DESTDIR)$(bindir):$$PATH" \
		LD_LIBRARY_PATH="$(DESTDIR)$(libdir):$$LD_LIBRARY_PATH" \
		"$(DESTDIR)$(vlclibdir)/vlc-cache-gen$(EXEEXT)" \
			 "$(DESTDIR)$(vlclibdir)/plugins" ; \
	else \
		echo "Cross-compilation: cache generation skipped!" ; \
	fi

uninstall-hook:
	rm -f -- "$(DESTDIR)$(vlclibdir)/plugins/plugins.dat"

###############################################################################
# PO translation files update
###############################################################################
.PHONY: update-po

update-po:
	cd po && $(MAKE) POTFILES vlc.pot update-po

###############################################################################
# OS Packaging rules
###############################################################################
include extras/package/macosx/package.mak
include extras/package/win32/package.mak
include extras/package/win32/msi.mak

