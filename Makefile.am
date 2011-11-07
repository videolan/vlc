###############################################################################
# Automake targets and declarations
###############################################################################

# SUBDIRS stores the directories where a "make" is required when building
# something. DIST_SUBDIRS stores the directories where nothing is built but
# which have makefiles with distribution information.
#  - src (libvlc) is nedeed by modules
#  - libs/* are needed by modules
DIST_SUBDIRS = m4 compat doc po share src modules lib bin test
SUBDIRS = compat doc po share src modules lib
if BUILD_VLC
SUBDIRS += bin
endif
SUBDIRS += test

EXTRA_DIST = \
	extras/package/win32/vlc.exe.manifest \
	extras/package/win32/libvlc.dll.manifest \
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
if HAVE_WIN32
BUILT_SOURCES_distclean += \
	extras/package/win32/vlc.win32.nsi extras/package/win32/spad.nsi
endif
if HAVE_DARWIN
BUILT_SOURCES_clean = macosx-sdk
else
BUILT_SOURCES_clean =
endif

BUILT_SOURCES = $(BUILT_SOURCES_distclean) $(BUILT_SOURCES_clean)

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
	extras/package/macosx/Resources/mainwindow_dark/volume-high_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-low_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-track_dark.png \
	extras/package/macosx/Resources/noart.png \
	extras/package/macosx/Resources/prefs/spref_cone_Audio_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Hotkeys_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Input_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Interface_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Subtitles_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Video_64.png \
	extras/package/macosx/Resources/README \
	extras/package/macosx/Resources/sidebar-icons/document-music-playlist.png \
	extras/package/macosx/Resources/sidebar-icons/film-cast.png \
	extras/package/macosx/Resources/sidebar-icons/film.png \
	extras/package/macosx/Resources/sidebar-icons/music-beam.png \
	extras/package/macosx/Resources/sidebar-icons/network-cloud.png \
	extras/package/macosx/Resources/sidebar-icons/picture.png \
	extras/package/macosx/Resources/sidebar-icons/README \
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

CLEANFILES = $(BUILT_SOURCES_clean)
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

if HAVE_DARWIN
if BUILD_MACOSX_VLC_APP
# Create the MacOS X app
noinst_DATA = VLC.app
endif
endif

###############################################################################
# Installing plugins cache
###############################################################################
install-exec-hook:
	if test "$(build)" = "$(host)"; then \
		LD_LIBRARY_PATH="$(DESTDIR)$(libdir):$$LD_LIBRARY_PATH" \
		"$(DESTDIR)$(vlclibdir)/vlc-cache-gen$(EXEEXT)" \
			 "$(DESTDIR)$(vlclibdir)/plugins" ; \
	else \
		echo "Cross-compilation: cache generation skipped!" ; \
	fi

silentstd = $(silentstd_$(V))
silentstd_ = $(silentstd_$(AM_DEFAULT_VERBOSITY))
silentstd_0 = 2>&1 >/dev/null

# VLC-release.app for packaging and giving it to your friends
# use package-macosx to get a nice dmg
VLC-release.app: vlc
	( cd src && $(MAKE) $(AM_MAKEFLAGS) install $(silentstd) )
	( cd lib && $(MAKE) $(AM_MAKEFLAGS) install $(silentstd) )
	rm -Rf "$(top_builddir)/tmp"
	mkdir -p "$(top_builddir)/tmp/extras/package/macosx"
	rm -Rf $(top_builddir)/VLC-release.app
	for i in vlc.xcodeproj Resources README.MacOSX.rtf ; do \
	  cp -R $(srcdir)/extras/package/macosx/$$i $(top_builddir)/tmp/extras/package/macosx/; \
	done
	REVISION=`(git --git-dir=$(srcdir)/.git describe --always || echo exported)` && \
	cat $(top_builddir)/extras/package/macosx/Info.plist | \
	sed "s/#REVISION#/$$REVISION/g" > $(top_builddir)/tmp/extras/package/macosx/Info.plist
	cp -R $(top_builddir)/extras/package/macosx/Resources $(top_builddir)/tmp/extras/package/macosx/
	for i in AUTHORS COPYING THANKS; do \
	  cp "$(srcdir)/$$i" $(top_builddir)/tmp; \
	done
	mkdir -p $(top_builddir)/tmp/extras/contrib/Sparkle
	cp -R $(CONTRIB_DIR)/Sparkle/Sparkle.framework $(top_builddir)/tmp/extras/contrib/Sparkle
	mkdir -p $(top_builddir)/tmp/extras/contrib/BGHUDAppKit
	cp -R $(CONTRIB_DIR)/BGHUDAppKit/BGHUDAppKit.framework $(top_builddir)/tmp/extras/contrib/BGHUDAppKit
	mkdir -p $(top_builddir)/tmp/extras/contrib/Growl
	cp -R $(CONTRIB_DIR)/Growl/Growl.framework $(top_builddir)/tmp/extras/contrib/Growl
	mkdir -p $(top_builddir)/tmp/modules/audio_output
	mkdir -p $(top_builddir)/tmp/modules/gui/macosx
	for i in \
	    AppleRemote.h \
	    AppleRemote.m \
	    about.h \
	    about.m \
	    applescript.h \
	    applescript.m \
	    controls.h \
	    controls.m \
	    intf.h \
	    intf.m \
	    macosx.m \
	    misc.h \
	    misc.m \
	    open.h \
	    open.m \
	    output.h \
	    output.m \
	    playlist.h \
	    playlist.m \
	    playlistinfo.h \
	    playlistinfo.m \
	    prefs_widgets.h \
	    prefs_widgets.m \
	    prefs.h \
	    prefs.m \
	    simple_prefs.h \
	    simple_prefs.m \
	    wizard.h \
	    wizard.m \
	    bookmarks.h \
	    bookmarks.m \
	    coredialogs.h \
	    coredialogs.m \
	    fspanel.h \
	    fspanel.m; do \
	  cp "$(srcdir)/modules/gui/macosx/$$i" \
             $(top_builddir)/tmp/modules/gui/macosx; \
	done
	$(AM_V_GEN)cd $(top_builddir)/tmp/extras/package/macosx && \
	xcodebuild -target vlc SYMROOT=../../../build DSTROOT=../../../build $(silentstd) && \
	cd ../../../../ && \
	cp -R $(top_builddir)/tmp/build/Default/VLC.bundle $(top_builddir)/VLC-release.app; \
	rm -Rf $(top_builddir)/tmp
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS
	PRODUCT="VLC-release.app" ACTION="release-makefile" src_dir=$(srcdir) build_dir=$(top_builddir) sh $(srcdir)/projects/macosx/framework/Pre-Compile.sh
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua
	for i in $(srcdir)/share/lua/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/playlist
	for i in $(srcdir)/share/lua/playlist/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/playlist/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/meta
	for i in $(srcdir)/share/lua/meta/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/meta/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/intf
	for i in $(srcdir)/share/lua/intf/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/intf/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/intf/modules
	for i in $(srcdir)/share/lua/intf/modules/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/intf/modules/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/dialogs
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/js
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/images
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/requests
	$(INSTALL) -m 644 $(srcdir)/share/lua/http/.hosts $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/.hosts
	for i in $(srcdir)/share/lua/http/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/lua/http/dialogs/* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/dialogs/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/lua/http/js/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/js/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/lua/http/images/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/images/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/lua/http/requests/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/requests/`basename $${i}` ; \
	done
	$(INSTALL) -m 644 $(srcdir)/share/lua/http/requests/README.txt $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/requests/README.txt
	$(INSTALL) -m 644 $(srcdir)/share/vlc512x512.png $(top_builddir)/VLC-release.app/Contents/MacOS/share/vlc512x512.png
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/locale
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  $(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES ; \
	  $(INSTALL) $(srcdir)/po/$${i}.gmo $(top_builddir)/VLC-release.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	  mkdir -p $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  $(LN_S) -f ../English.lproj/InfoPlist.strings \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  $(LN_S) -f ../English.lproj/MainMenu.xib \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	done
	printf "APPLVLC#" >| $(top_builddir)/VLC-release.app/Contents/PkgInfo
	rm -Rf $(top_builddir)/VLC-release.app/Contents/Frameworks/BGHUDAppKit.framework/Resources/
	find $(top_builddir)/VLC-release.app -type d -exec chmod ugo+rx '{}' \;
	find $(top_builddir)/VLC-release.app -type f -exec chmod ugo+r '{}' \;

# This is just for development purposes. 
# The resulting VLC.app will only run in this tree.
VLC.app: vlc $(top_builddir)/src/.libs/libvlccore.dylib $(top_builddir)/lib/.libs/libvlc.dylib
	$(AM_V_GEN)(cd src && make install $(silentstd))
	$(AM_V_GEN)(cd lib && make install $(silentstd))
	rm -Rf $(top_builddir)/tmp
	mkdir -p "$(top_builddir)/tmp/extras/package/macosx"
	rm -Rf $(top_builddir)/VLC.app
	for i in vlc.xcodeproj Resources README.MacOSX.rtf; do \
	  cp -R $(srcdir)/extras/package/macosx/$$i $(top_builddir)/tmp/extras/package/macosx/; \
	done
	REVISION=`(git --git-dir=$(srcdir)/.git describe --always || echo exported)` && \
	cat $(top_builddir)/extras/package/macosx/Info.plist | \
	sed "s/#REVISION#/$$REVISION/g" > $(top_builddir)/tmp/extras/package/macosx/Info.plist
	cp -R $(top_builddir)/extras/package/macosx/Resources $(top_builddir)/tmp/extras/package/macosx/
	for i in AUTHORS COPYING THANKS; do \
	  cp "$(srcdir)/$$i" $(top_builddir)/tmp; \
	done
	mkdir -p $(top_builddir)/tmp/extras/contrib/Sparkle
	cp -R $(CONTRIB_DIR)/Sparkle/Sparkle.framework $(top_builddir)/tmp/extras/contrib/Sparkle
	mkdir -p $(top_builddir)/tmp/extras/contrib/BGHUDAppKit
	cp -R $(CONTRIB_DIR)/BGHUDAppKit/BGHUDAppKit.framework $(top_builddir)/tmp/extras/contrib/BGHUDAppKit
	mkdir -p $(top_builddir)/tmp/extras/contrib/Growl
	cp -R $(CONTRIB_DIR)/Growl/Growl.framework $(top_builddir)/tmp/extras/contrib/Growl
	mkdir -p $(top_builddir)/tmp/modules/audio_output
	mkdir -p $(top_builddir)/tmp/modules/gui/macosx
	for i in \
	    AppleRemote.h \
	    AppleRemote.m \
	    about.h \
	    about.m \
	    applescript.h \
	    applescript.m \
	    controls.h \
	    controls.m \
	    intf.h \
	    intf.m \
	    macosx.m \
	    misc.h \
	    misc.m \
	    open.h \
	    open.m \
	    output.h \
	    output.m \
	    playlist.h \
	    playlist.m \
	    playlistinfo.h \
	    playlistinfo.m \
	    prefs_widgets.h \
	    prefs_widgets.m \
	    prefs.h \
	    prefs.m \
	    simple_prefs.h \
	    simple_prefs.m \
	    wizard.h \
	    wizard.m \
	    bookmarks.h \
	    bookmarks.m \
	    coredialogs.h \
	    coredialogs.m \
	    fspanel.h \
	    fspanel.m; do \
	  cp "$(srcdir)/modules/gui/macosx/$$i" \
             $(top_builddir)/tmp/modules/gui/macosx; \
	done
	$(AM_V_GEN)cd $(top_builddir)/tmp/extras/package/macosx && \
	xcodebuild -target vlc SYMROOT=../../../build DSTROOT=../../../build $(silentstd) && \
	cd ../../../../ && \
	cp -R -L $(top_builddir)/tmp/build/Default/VLC.bundle $(top_builddir)/VLC.app
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS
	touch $(top_builddir)/VLC.app/Contents/MacOS/VLC
	chmod +x $(top_builddir)/VLC.app/Contents/MacOS/VLC
	$(INSTALL) $(top_builddir)/bin/.libs/vlc $(top_builddir)/VLC.app/Contents/MacOS/VLC
	$(LN_S) -f ../../../modules $(top_builddir)/VLC.app/Contents/MacOS/plugins
	install -d $(top_builddir)/VLC.app/Contents/MacOS/share
	for i in `ls $(srcdir)/share`; do \
	   $(LN_S) -f `pwd`/$(srcdir)/share/$$i $(top_builddir)/VLC.app/Contents/MacOS/share/; \
	done
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS/share/locale
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  mkdir -p $(top_builddir)/VLC.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES ; \
	  $(LN_S) -f `pwd`/$(srcdir)/po/$${i}.gmo $(top_builddir)/VLC.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	  mkdir -p $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  $(LN_S) -f ../English.lproj/InfoPlist.strings \
	      $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  $(LN_S) -f ../English.lproj/MainMenu.xib \
	      $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	done
	printf "APPLVLC#" >| $(top_builddir)/VLC.app/Contents/PkgInfo

###############################################################################
# Building architecture-specific binary packages
###############################################################################

# XXX: this rule is probably only useful to you if you have exactly
# the same setup as the maintaner(s).
#

############################################################################
## Win                                                                    ##
############################################################################
destdir=$(prefix)
win32_destdir=$(top_builddir)/vlc-$(VERSION)
win32_debugdir=$(top_builddir)/symbols-$(VERSION)
win32_xpi_destdir=$(win32_destdir)/vlc-plugin

if HAVE_WIN64
include extras/package/npapi.am

build-activex:
	touch $@
else
if HAVE_WINCE
build-npapi:
	touch $@

build-activex:
	touch $@
else
if HAVE_WIN32
include extras/package/npapi.am
include extras/package/activex.am
endif
endif
endif

#Win-common is for win32 and wince
package-win-common: install build-npapi build-activex
# Check that tmp isn't in the way
	@if test -e "$(win32_destdir)"; then \
	    echo "Error: please remove $(win32_destdir), it is in the way"; \
	    false; \
	elif test -e "$(win32_debugdir)"; then \
	    echo "Error: please remove $(win32_debugdir), it is in the way"; \
	    false; \
	else \
	    echo "Debug dir OK."; mkdir -p "$(win32_debugdir)"; \
	    echo "Dest dir OK."; mkdir -p "$(win32_destdir)"; \
	fi

# Executables, major libs+manifests
	find $(destdir) -maxdepth 4 \( -name "*$(LIBEXT)" -o -name "*$(EXEEXT)" \) -exec cp {} "$(win32_destdir)/" \;
	for file in $(top_srcdir)/extras/package/win32/vlc$(EXEEXT).manifest \
                $(top_srcdir)/extras/package/win32/libvlc$(LIBEXT).manifest; \
	    do cp $$file "$(win32_destdir)/" ; done;

# Text files and clean them
	for file in AUTHORS THANKS ; \
	  do sed 's/@/_AT_/' < "$(srcdir)/$$file" > "$(win32_destdir)/$${file}.txt" ; done;
	for file in NEWS COPYING README; \
	  do cp "$(srcdir)/$$file" "$(win32_destdir)/$${file}.txt"; done

# Necessary icon
	cp $(srcdir)/share/icons/vlc.ico $(win32_destdir)

# Locales
	-cp -r $(destdir)/share/locale $(win32_destdir)

# Plugins
	cp -r $(destdir)/lib/vlc/plugins $(win32_destdir)

if BUILD_LUA
	mkdir -p $(win32_destdir)/lua
	cp -r $(destdir)/lib/vlc/lua/* $(win32_destdir)/lua
	cp -r $(destdir)/share/vlc/lua/* $(win32_destdir)/lua
endif

if BUILD_SKINS
	cp -r $(destdir)/share/vlc/skins2 $(win32_destdir)/skins
endif
if BUILD_OSDMENU
	cp -r $(destdir)/share/vlc/osdmenu "$(win32_destdir)/osdmenu"
	for file in $(win32_destdir)/osdmenu/*.cfg; do \
		sed 's%share/osdmenu%osdmenu%g' "$$file" > "$$file.tmp" || exit $$? ; \
		sed 's%/%\\%g' "$$file.tmp" > "$$file" || exit$$? ; \
		rm -f -- "$$file.tmp"; \
	done
endif
if !HAVE_WIN64
if !HAVE_WINCE
	cp "$(top_builddir)/activex-vlc/src/axvlc.dll.manifest" "$(win32_destdir)/"
	cp "$(top_builddir)/activex-vlc/installed/lib/axvlc.dll" "$(win32_destdir)/"
	cp "$(top_builddir)/npapi-vlc/npapi/npvlc.dll.manifest" "$(win32_destdir)/"
	cp "$(top_builddir)/npapi-vlc/installed/lib/npvlc.dll" "$(win32_destdir)/"
endif
endif

# Compiler shared DLLs
# For win64 only, Debian mingw32 compilers are built with --disable-shared
if HAVE_WIN64
#   if gcc_s_sjlj/stdc++ DLLs exist, our C++ modules were linked to them
	gcc_lib_dir=`x86_64-w64-mingw32-gcc -v /dev/null 2>&1 | grep ^LIBRARY_PATH|cut -d= -f2|cut -d: -f1` ; \
	cp "$${gcc_lib_dir}/libstdc++-6.dll" "$${gcc_lib_dir}/libgcc_s_sjlj-1.dll" "$(win32_destdir)/" ; true
endif

# SDK
	mkdir -p "$(win32_destdir)/sdk/lib"
	cp -r $(destdir)/include "$(win32_destdir)/sdk"
	cp -r $(destdir)/lib/pkgconfig "$(win32_destdir)/sdk/lib"
	for file in libvlc.dll.a libvlc.la libvlccore.dll.a libvlccore.la; do \
        cp -rv $(destdir)/lib/$$file "$(win32_destdir)/sdk/lib"; done
if !HAVE_WIN64
if !HAVE_WINCE
	mkdir -p "$(win32_destdir)/sdk/activex"
	cp $(top_builddir)/activex-vlc/README $(win32_destdir)/sdk/activex/README.TXT
	cp $(top_builddir)/activex-vlc/src/test.html $(win32_destdir)/sdk/activex/
endif
endif

	find $(win32_destdir) -type f \( -name "*xml" -or -name "*html" -or -name '*js' -or -name '*css' -or -name '*hosts' -or -iname '*txt' -or -name '*.cfg' -or -name '*.lua' \) -exec $(U2D) {} \;

#Enable DEP and ASLR for all the binaries
	find $(win32_destdir) -type f \( -name '*$(LIBEXT)' -print -o -name '*$(EXEEXT)' -print \) -exec $(top_srcdir)/extras/package/win32/peflags.py {} \;
	find $(win32_destdir)/plugins/ -type f \( -name '*.a' -or -name '*.la' \) -exec rm -rvf {} \;

package-win-base: package-win-common
	find $(win32_destdir) -type f \( -name '*$(LIBEXT)' -or -name '*$(EXEEXT)' \) | while read i; \
	do if test -n "$$i" ; then \
	    $(OBJCOPY) --only-keep-debug "$$i" "$$i.dbg"; \
	    $(OBJCOPY) --strip-all "$$i" ; \
	    $(OBJCOPY) --add-gnu-debuglink="$$i.dbg" "$$i" ; \
	    mv "$$i.dbg" "$(win32_debugdir)"; \
	  fi ; \
	done

package-win32-webplugin-common: package-win-base
	mkdir -p "$(win32_xpi_destdir)/plugins"
	find $(destdir) -maxdepth 4 -name "*$(LIBEXT)" -exec cp {} "$(win32_xpi_destdir)/" \;
if !HAVE_WIN64
	cp $(top_builddir)/npapi-vlc/npapi/npvlc.dll.manifest "$(win32_xpi_destdir)/plugins"
endif
	cp "$(top_srcdir)/extras/package/win32/libvlc.dll.manifest" "$(win32_xpi_destdir)/plugins"
	cp -r $(win32_destdir)/plugins/ "$(win32_xpi_destdir)/plugins"
	rm -rf "$(win32_xpi_destdir)/plugins/plugins/*qt*"
	rm -rf "$(win32_xpi_destdir)/plugins/plugins/*skins*"

package-win32-xpi: package-win32-webplugin-common
if !HAVE_WIN64
	cp $(top_builddir)/npapi-vlc/npapi/install.rdf "$(win32_xpi_destdir)"
	cd $(win32_xpi_destdir) && zip -r "../vlc-$(VERSION).xpi" install.rdf plugins
endif

package-win32-crx: package-win32-webplugin-common
if !HAVE_WIN64
	cp $(top_builddir)/npapi-vlc/npapi/manifest.json "$(win32_xpi_destdir)"
	crxmake --pack-extension "$(win32_xpi_destdir)" \
		--extension-output "$(win32_destdir)/vlc-$(VERSION).crx" --ignore-file install.rdf
endif

package-win32-base-exe: package-win-base
# Script installer
	cp "$(top_builddir)/extras/package/win32/vlc.win32.nsi" "$(win32_destdir)/"
	cp "$(top_builddir)/extras/package/win32/spad.nsi" "$(win32_destdir)/"
	mkdir "$(win32_destdir)/languages"
	cp $(srcdir)/extras/package/win32/languages/*.nsh "$(win32_destdir)/languages/"
# Copy the UAC NSIS plugin
	mkdir -p "$(win32_destdir)/NSIS"
	cp "$(top_srcdir)/extras/package/win32/UAC.nsh" "$(win32_destdir)/NSIS"
	cp "$(top_srcdir)/extras/package/win32/UAC.dll" "$(win32_destdir)/NSIS"

# Create package
	if makensis -VERSION >/dev/null 2>&1; then \
	    MAKENSIS="makensis"; \
	elif [ -x "/cygdrive/c/Program Files/NSIS/makensis" ]; then \
	    MAKENSIS="/cygdrive/c/Program\ Files/NSIS/makensis"; \
	elif [ -x "$(PROGRAMFILES)/NSIS/makensis" ]; then \
	    MAKENSIS="$(PROGRAMFILES)/NSIS/makensis"; \
	elif wine --version >/dev/null 2>&1; then \
	    MAKENSIS="wine C:/Program\ Files/NSIS/makensis.exe"; \
	else \
	    echo 'Error: cannot locate makensis tool'; exit 1; \
	fi; \
	eval "$$MAKENSIS $(win32_destdir)/spad.nsi"; \
	eval "$$MAKENSIS $(win32_destdir)/vlc.win32.nsi"

package-win32-base-zip: package-win-base
# Create package
	rm -f -- vlc-$(VERSION)-win32.zip
	zip -r -9 vlc-$(VERSION)-win32.zip vlc-$(VERSION)

package-win32-base-debug-zip: package-win-common
# Create package
	rm -f -- vlc-$(VERSION)-win32-debug.zip
	zip -r -9 vlc-$(VERSION)-win32-debug.zip vlc-$(VERSION)

package-win32-base-7zip: package-win-base
# Create package
	7z a -t7z -m0=lzma -mx=9 -mfb=64 -md=32m -ms=on vlc-$(VERSION)-win32.7z vlc-$(VERSION)

package-win32-base-debug-7zip: package-win-common
# Create package
	7z a -t7z -m0=lzma -mx=9 -mfb=64 -md=32m -ms=on vlc-$(VERSION)-win32-debug.7z vlc-$(VERSION)

package-win32-cleanup:
	rm -Rf $(win32_destdir) $(win32_debugdir)

package-win32-exe: package-win-base package-win32-base-exe package-win32-cleanup

package-win32-zip: package-win32-base-zip package-win32-cleanup

package-win32-debug-zip: package-win32-base-debug-zip package-win32-cleanup

package-win32-7zip: package-win32-base-7zip package-win32-cleanup

package-win32-debug-7zip: package-win32-base-debug-7zip package-win32-cleanup

package-win32-no-clean: package-win32-base-zip package-win32-base-7zip package-win32-base-exe package-win32-xpi

package-win32: package-win32-no-clean package-win32-cleanup

package-win32-debug: package-win32-base-debug-zip package-win32-base-debug-7zip package-win32-cleanup


#######
# WinCE
#######
package-wince-base: package-win-base

package-wince-base-zip: package-wince-base
# Create package
	rm -f -- vlc-$(VERSION)-wince.zip
	zip -r -9 vlc-$(VERSION)-wince.zip vlc-$(VERSION)

package-wince: package-wince-base-zip package-win32-cleanup


#########################################################################
## MacOS X                                                             ##
#########################################################################
package-macosx: VLC-release.app ChangeLog
# Check that the temporary location isn't in the way
	@if test -e "$(top_builddir)/vlc-$(VERSION)/"; then \
	  rm -Rf "$(top_builddir)/vlc-$(VERSION)/" ; \
	fi

	echo "Create package directory: vlc-$(VERSION)/";
	mkdir -p "$(top_builddir)/vlc-$(VERSION)/";

# Copy relevant files 
	@if test -e "$(top_builddir)/VLC-release.app/"; then \
	  cp -R "$(top_builddir)/VLC-release.app" "$(top_builddir)/vlc-$(VERSION)/VLC.app"; \
	else \
	  cp -R "$(top_builddir)/VLC.app" "$(top_builddir)/vlc-$(VERSION)/VLC.app"; \
	fi
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies/ && \
          mkdir -p $(top_builddir)/vlc-$(VERSION)/.background/ && \
          cp $(srcdir)/AUTHORS $(srcdir)/COPYING $(srcdir)/README $(srcdir)/THANKS $(srcdir)/NEWS $(top_builddir)/vlc-$(VERSION)/Goodies/ && \
          cp -R  $(srcdir)/extras/package/macosx/Delete_Preferences.app $(top_builddir)/vlc-$(VERSION)/Goodies/Delete\ VLC\ Preferences.app && \
	  cp $(srcdir)/extras/package/macosx/README.MacOSX.rtf $(top_builddir)/vlc-$(VERSION)/Read\ Me.rtf && \
	  cp $(srcdir)/extras/package/macosx/Resources/about_bg.png $(top_builddir)/vlc-$(VERSION)/.background/background.png
	cp -L $(top_builddir)/ChangeLog $(top_builddir)/vlc-$(VERSION)/Goodies/

# Place a link to the application folder
	$(LN_S) /Applications $(top_builddir)/vlc-$(VERSION)/Applications

# Create disk image (temporarily taken from the 0.8.6-bugfix branch to provide reliable NBs)
	echo "Creating disk image"
	rm -f "$(top_builddir)/vlc-$(VERSION).dmg"
	hdiutil create -verbose -srcfolder "$(top_builddir)/vlc-$(VERSION)" \
	  "$(top_builddir)/vlc-$(VERSION).dmg" -scrub
	echo "Disk image creation completed:"
	ls -la "$(top_builddir)/vlc-$(VERSION).dmg" ; echo

# Create disk image 
#	echo "Creating disk image"
#	rm -f "$(top_builddir)/vlc-$(VERSION).dmg"
#	hdiutil create -verbose -srcfolder "$(top_builddir)/vlc-$(VERSION)" \
#	  "$(top_builddir)/vlc-$(VERSION).dmg" -format UDRW \
#	  -scrub -imagekey zlib-level=9 -attach

# Make sure the root window of the dmg will pop up when the dmg is mounted.
# Note: We dont mount in /Volumes to be sure we won't collide with an other
# finder mounted dmg with the same name.
#	echo "Make sure the root window of the dmg will pop up when the dmg is mounted"
#	mkdir -p $(top_builddir)/vlcmnt
#	hdiutil attach -nokernel -readwrite -noverify -noautoopen -private "$(top_builddir)/vlc-$(VERSION).dmg" -mountpoint "$(top_builddir)/vlcmnt/vlc-$(VERSION)"
#	bless --folder "$(top_builddir)/vlcmnt/vlc-$(VERSION)/" --openfolder "$(top_builddir)/vlcmnt/vlc-$(VERSION)"
#	sleep 1 # Make sure operation completes
#	cd "$(srcdir)"

# Unmount the image now
#	hdiutil detach "$(top_builddir)/vlcmnt/vlc-$(VERSION)"
#	rm -R $(top_builddir)/vlcmnt

# Make sure the image is not writable
# Note: We can't directly create a read only dmg as we do the bless stuff
	echo "Make the disk image read-only"
	mv "$(top_builddir)/vlc-$(VERSION).dmg" "$(top_builddir)/vlc-$(VERSION)-rw.dmg"
	hdiutil convert "$(top_builddir)/vlc-$(VERSION)-rw.dmg" -format UDBZ -o "$(top_builddir)/vlc-$(VERSION).dmg"
	rm "$(top_builddir)/vlc-$(VERSION)-rw.dmg"

# We are done
	echo "Disk image creation completed:"
	ls -la "$(top_builddir)/vlc-$(VERSION).dmg" ; echo

# Clean up
	rm -Rf "$(top_builddir)/vlc-$(VERSION)"

package-macosx-zip: VLC-release.app
	rm -Rf $(top_builddir)/vlc-$(VERSION)
	mkdir -p $(top_builddir)/vlc-$(VERSION)
	cp -R $(top_builddir)/VLC-release.app $(top_builddir)/vlc-$(VERSION)/VLC.app
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies
	for i in AUTHORS COPYING ChangeLog README THANKS NEWS; do \
	  cp $(srcdir)/$$i $(top_builddir)/vlc-$(VERSION)/Goodies; \
	done
	cp -R  $(srcdir)/extras/package/macosx/Delete_Preferences.app \
	     $(top_builddir)/vlc-$(VERSION)/Goodies
	cp $(srcdir)/extras/package/macosx/README.MacOSX.rtf \
	   $(top_builddir)/vlc-$(VERSION)/Read\ Me.rtf
	zip -r -y -9 $(top_builddir)/vlc-$(VERSION).zip $(top_builddir)/vlc-$(VERSION)
	rm -Rf $(top_builddir)/vlc-$(VERSION)

package-macosx-framework-zip:
	rm -Rf $(top_builddir)/vlckit-$(VERSION)
	mkdir -p $(top_builddir)/vlckit-$(VERSION)
	cp -R $(srcdir)/projects/macosx/framework/build/Debug/VLCKit.framework \
	  $(top_builddir)/vlckit-$(VERSION)/VLCKit.framework
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies
	for i in AUTHORS COPYING ChangeLog README THANKS NEWS; do \
	  cp $(srcdir)/$$i $(top_builddir)/vlckit-$(VERSION)/Goodies; \
	done
	zip -r -y -9 $(top_builddir)/vlckit-$(VERSION).zip $(top_builddir)/vlckit-$(VERSION)
	rm -Rf $(top_builddir)/vlc-$(VERSION)

package-translations:
	@if test -e "$(srcdir)/vlc-translations-$(VERSION)"; then \
	  echo "Error: please remove $(srcdir)/vlc-translations-$(VERSION), it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(srcdir)/vlc-translations-$(VERSION)"; \
	fi
# Copy translations
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  cp "$(srcdir)/po/$${i}.po" \
	    "$(srcdir)/vlc-translations-$(VERSION)/$${i}.po" \
	    || true ; \
	done
	cp "$(srcdir)/doc/translations.txt" \
	  "$(srcdir)/vlc-translations-$(VERSION)/README.txt"

	echo "#!/bin/sh" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo 'if test $$# != 1; then' >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "	echo \"Usage: convert-po.sh <.po file>\"" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "	exit 1" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "fi" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo 'msgfmt --statistics -o vlc.mo $$1' >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"

	$(AMTAR) chof - $(srcdir)/vlc-translations-$(VERSION) \
	  | GZIP=$(GZIP_ENV) gzip -c >$(srcdir)/vlc-translations-$(VERSION).tar.gz

###############################################################################
# PO translation files update
###############################################################################
.PHONY: update-po

update-po:
	cd po && $(MAKE) POTFILES vlc.pot update-po


###############################################################################
# Enforce Mac OS X deployment target environment variable
###############################################################################
macosx-sdk: Makefile.in $(HEADERS_include)
	export MACOSX_DEPLOYMENT_TARGET=$(MACOSX_DEPLOYMENT_TARGET)
