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
	extras/package/win32/peflags.pl \
	extras/package/win32/change-contribs-directory.sh \
	extras/package/win32/configure.sh \
	extras/package/macosx/README.MacOSX.rtf \
	extras/package/rpm/vlc.fedora.spec \
	extras/package/rpm/vlc.altlinux.spec \
	extras/package/win32/vlc.win32.nsi.in \
	extras/package/win32/spad.nsi.in \
	extras/package/win32/UAC.nsh \
	extras/package/win32/UAC.dll \
	extras/package/win32/languages/declaration.nsh \
	extras/package/win32/languages/bengali.nsh \
	extras/package/win32/languages/basque.nsh \
	extras/package/win32/languages/brazilian_portuguese.nsh \
	extras/package/win32/languages/bulgarian.nsh \
	extras/package/win32/languages/catalan.nsh \
	extras/package/win32/languages/danish.nsh \
	extras/package/win32/languages/dutch.nsh \
	extras/package/win32/languages/english.nsh \
	extras/package/win32/languages/estonian.nsh \
	extras/package/win32/languages/finnish.nsh \
	extras/package/win32/languages/french.nsh \
	extras/package/win32/languages/german.nsh \
	extras/package/win32/languages/hungarian.nsh \
	extras/package/win32/languages/italian.nsh \
	extras/package/win32/languages/japanese.nsh \
	extras/package/win32/languages/lithuanian.nsh \
	extras/package/win32/languages/occitan.nsh \
	extras/package/win32/languages/polish.nsh \
	extras/package/win32/languages/punjabi.nsh \
	extras/package/win32/languages/romanian.nsh \
	extras/package/win32/languages/schinese.nsh \
	extras/package/win32/languages/slovak.nsh \
	extras/package/win32/languages/slovenian.nsh \
	extras/package/win32/languages/sorani.nsh \
	extras/package/win32/languages/spanish.nsh

dist_noinst_SCRIPTS = bootstrap
nodist_noinst_SCRIPTS = compile

BUILT_SOURCES_distclean =

BUILT_SOURCES = $(BUILT_SOURCES_distclean)

SUFFIXES = 

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-fast-install \
	--disable-a52 \
	--disable-avcodec --disable-avformat \
	--disable-postproc --disable-swscale \
	--disable-dbus \
	--disable-mad --disable-libmpeg2 \
	--disable-faad --disable-skins2 \
	--disable-live555 \
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
	$(LN_S) -f doc/ChangeLog-2009 "$@"

###############################################################################
# MacOS X project
###############################################################################

EXTRA_DIST += \
	extras/package/macosx/Resources/about_bg.png \
	extras/package/macosx/Resources/dsa_pub.pem \
	extras/package/macosx/Resources/English.lproj/About.xib \
	extras/package/macosx/Resources/English.lproj/AudioEffects.xib \
	extras/package/macosx/Resources/English.lproj/Bookmarks.xib \
	extras/package/macosx/Resources/English.lproj/CoreDialogs.xib \
	extras/package/macosx/Resources/English.lproj/ErrorPanel.xib \
	extras/package/macosx/Resources/English.lproj/MainMenu.xib \
	extras/package/macosx/Resources/English.lproj/MediaInfo.xib \
	extras/package/macosx/Resources/English.lproj/Open.xib \
	extras/package/macosx/Resources/English.lproj/Preferences.xib \
	extras/package/macosx/Resources/English.lproj/SyncTracks.xib \
	extras/package/macosx/Resources/English.lproj/VideoEffects.xib \
	extras/package/macosx/Resources/English.lproj/Wizard.xib \
	extras/package/macosx/Resources/fspanel/fs_background.png \
	extras/package/macosx/Resources/fspanel/fs_exit_fullscreen.png \
	extras/package/macosx/Resources/fspanel/fs_exit_fullscreen_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_forward.png \
	extras/package/macosx/Resources/fspanel/fs_forward_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_pause.png \
	extras/package/macosx/Resources/fspanel/fs_pause_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_play.png \
	extras/package/macosx/Resources/fspanel/fs_play_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_rewind.png \
	extras/package/macosx/Resources/fspanel/fs_rewind_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_skip_next.png \
	extras/package/macosx/Resources/fspanel/fs_skip_next_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_skip_previous.png \
	extras/package/macosx/Resources/fspanel/fs_skip_previous_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_stop.png \
	extras/package/macosx/Resources/fspanel/fs_stop_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider_knob.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider_knob_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_volume_max.png \
	extras/package/macosx/Resources/fspanel/fs_volume_max_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_volume_mute.png \
	extras/package/macosx/Resources/fspanel/fs_volume_mute_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_bar.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_knob.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_knob_highlight.png \
	extras/package/macosx/Resources/fspanel/fs_background@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_exit_fullscreen@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_exit_fullscreen_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_forward@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_forward_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_pause@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_pause_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_play@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_play_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_rewind@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_rewind_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_skip_next@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_skip_next_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_skip_previous@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_skip_previous_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_stop@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_stop_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider_knob@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_time_slider_knob_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_volume_max@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_volume_max_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_volume_mute@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_volume_mute_highlight@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_bar@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_knob@x1.5.png \
	extras/package/macosx/Resources/fspanel/fs_volume_slider_knob_highlight@x1.5.png \
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
	extras/package/macosx/Resources/icons/vlc.icns \
	extras/package/macosx/Resources/icons/vob.icns \
	extras/package/macosx/Resources/icons/wav.icns \
	extras/package/macosx/Resources/icons/wma.icns \
	extras/package/macosx/Resources/icons/wmv.icns \
	extras/package/macosx/Resources/mainwindow/back-pressed.png \
	extras/package/macosx/Resources/mainwindow/back.png \
	extras/package/macosx/Resources/mainwindow/bottom-background.png \
	extras/package/macosx/Resources/mainwindow/bottombar-mini.png \
	extras/package/macosx/Resources/mainwindow/dropzone-background.png \
	extras/package/macosx/Resources/mainwindow/dropzone.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons-blue.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons-pressed.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons.png \
	extras/package/macosx/Resources/mainwindow/effects-one-button-blue.png \
	extras/package/macosx/Resources/mainwindow/effects-one-button.png \
	extras/package/macosx/Resources/mainwindow/forward-pressed.png \
	extras/package/macosx/Resources/mainwindow/forward.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-double-buttons-pressed.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-double-buttons.png \
	extras/package/macosx/Resources/mainwindow/mini-progressbar-fill-left.png \
	extras/package/macosx/Resources/mainwindow/mini-progressbar-fill-middle.png \
	extras/package/macosx/Resources/mainwindow/mini-progressbar-fill-right.png \
	extras/package/macosx/Resources/mainwindow/mini-progressbar-knob.png \
	extras/package/macosx/Resources/mainwindow/mini-progressbar-wrapper-left.png \
	extras/package/macosx/Resources/mainwindow/mini-progressbar-wrapper-middle.png \
	extras/package/macosx/Resources/mainwindow/mini-progressbar-wrapper-right.png \
	extras/package/macosx/Resources/mainwindow/pause-pressed.png \
	extras/package/macosx/Resources/mainwindow/pause.png \
	extras/package/macosx/Resources/mainwindow/play-pressed.png \
	extras/package/macosx/Resources/mainwindow/play.png \
	extras/package/macosx/Resources/mainwindow/playlist-blue.png \
	extras/package/macosx/Resources/mainwindow/playlist-pressed.png \
	extras/package/macosx/Resources/mainwindow/playlist.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-left.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-middle.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-right.png \
	extras/package/macosx/Resources/mainwindow/progression-knob.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-left.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-middle.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-right.png \
	extras/package/macosx/Resources/mainwindow/repeat-all-pressed.png \
	extras/package/macosx/Resources/mainwindow/repeat-all.png \
	extras/package/macosx/Resources/mainwindow/repeat-one-pressed.png \
	extras/package/macosx/Resources/mainwindow/repeat-one.png \
	extras/package/macosx/Resources/mainwindow/repeat-pressed.png \
	extras/package/macosx/Resources/mainwindow/repeat.png \
	extras/package/macosx/Resources/mainwindow/shuffle-blue-pressed.png \
	extras/package/macosx/Resources/mainwindow/shuffle-blue.png \
	extras/package/macosx/Resources/mainwindow/shuffle-pressed.png \
	extras/package/macosx/Resources/mainwindow/shuffle.png \
	extras/package/macosx/Resources/mainwindow/stop-pressed.png \
	extras/package/macosx/Resources/mainwindow/stop.png \
	extras/package/macosx/Resources/mainwindow/volume-high.png \
	extras/package/macosx/Resources/mainwindow/volume-low.png \
	extras/package/macosx/Resources/mainwindow/volume-slider-knob.png \
	extras/package/macosx/Resources/mainwindow/volume-slider-track.png \
	extras/package/macosx/Resources/mainwindow_dark/back-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/back_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/bottom-background_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/bottombar-mini.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-one-button-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-one-button_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/forward_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-double-buttons-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-double-buttons_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/pause-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/pause_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/play-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/play_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-left_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-middle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-right_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-left_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-middle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-right_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-all-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-all-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-one-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-one-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/stop-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/stop_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-close-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-close-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-close-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-close-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-close-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-close.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-minimize-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-minimize-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-minimize-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-minimize-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-minimize-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-minimize.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-zoom-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-zoom-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-zoom-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-zoom-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-zoom-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/snowleo/snowleo-window-zoom.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-center-fill.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-left.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-right.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-high_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-low_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-track_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/window-fullscreen-on.png \
	extras/package/macosx/Resources/mainwindow_dark/window-fullscreen-over.png \
	extras/package/macosx/Resources/mainwindow_dark/window-fullscreen.png \
	extras/package/macosx/Resources/mainwindow_dark/window-resize.png \
	extras/package/macosx/Resources/noart.png \
	extras/package/macosx/Resources/prefs/spref_cone_Audio_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Hotkeys_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Input_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Interface_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Subtitles_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Video_64.png \
	extras/package/macosx/Resources/README \
	extras/package/macosx/Resources/sidebar-icons/sidebar-local.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-movie.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-music.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-pictures.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-playlist.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-podcast.png \
	extras/package/macosx/Resources/vlc.scriptSuite \
	extras/package/macosx/Resources/vlc.scriptTerminology \
	extras/package/macosx/fullscreen_panel.svg \
	extras/package/macosx/ub.sh \
	extras/package/macosx/vlc.xcodeproj/project.pbxproj \
	extras/package/macosx/Delete_Preferences.app/Contents/Info.plist \
	extras/package/macosx/Delete_Preferences.app/Contents/PkgInfo \
	extras/package/macosx/Delete_Preferences.app/Contents/MacOS/applet \
	extras/package/macosx/Delete_Preferences.app/Contents/Resources/description.rtfd/TXT.rtf \
	extras/package/macosx/Delete_Preferences.app/Contents/Resources/applet.icns \
	extras/package/macosx/Delete_Preferences.app/Contents/Resources/applet.rsrc \
	extras/package/macosx/Delete_Preferences.app/Contents/Resources/Scripts/main.scpt \
	extras/package/macosx/eyetvplugin/EyeTVPluginDefs.h \
	extras/package/macosx/eyetvplugin/Info.plist \
	extras/package/macosx/eyetvplugin/eyetvplugin.c \
	extras/package/macosx/eyetvplugin/eyetvplugin.h \
	extras/package/macosx/eyetvplugin/English.lproj/InfoPlist.strings \
	extras/package/macosx/eyetvplugin/eyetvplugin.xcodeproj/project.pbxproj \
	extras/package/macosx/README.MacOSX.rtf \
	extras/package/macosx/Info.plist.in


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
	contrib/src/

###############################################################################
# Building libvlc
###############################################################################

CLEANFILES =
DISTCLEANFILES = $(BUILT_SOURCES_distclean) compile
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

ALL_ALIASES = cvlc rvlc svlc qvlc nvlc mvlc
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

mvlc: make-alias Makefile
	$(AM_V_GEN)$(MKALIAS) maemo

if BUILD_VLC
noinst_SCRIPTS = vlc$(EXEEXT)
endif

vlc$(EXEEXT):
	$(AM_V_GEN)$(LN_S) -f bin/vlc-static$(EXEEXT) vlc$(EXEEXT)

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

silentstd = $(silentstd_$(V))
silentstd_ = $(silentstd_$(AM_DEFAULT_VERBOSITY))
silentstd_0 = 2>&1 >/dev/null

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

