###############################################################################
# Automake targets and declarations
###############################################################################

# SUBDIRS stores the directories where a "make" is required when building
# something. DIST_SUBDIRS stores the directories where nothing is built but
# which have makefiles with distribution information.
#  - src (libvlc) is nedeed by modules, mozilla and bindings
#  - libs/* are needed by modules
BASE_SUBDIRS = po compat src bin modules share doc test
EXTRA_SUBDIRS = m4 extras/package/ipkg \
	libs/loader libs/srtp libs/unzip \
	projects/mozilla projects/activex
DIST_SUBDIRS = $(BASE_SUBDIRS) $(EXTRA_SUBDIRS)

SUBDIRS = po compat src
if LOADER
SUBDIRS += libs/loader
endif
if HAVE_GCRYPT
SUBDIRS += libs/srtp
endif
if HAVE_ZLIB
if !HAVE_MINIZIP
SUBDIRS += libs/unzip
endif
endif
if BUILD_VLC
SUBDIRS += bin
endif
SUBDIRS += modules share doc test
if BUILD_MOZILLA
SUBDIRS += projects/mozilla
endif
if BUILD_ACTIVEX
SUBDIRS += projects/activex
endif

EXTRA_DIST = \
	HACKING \
	INSTALL.win32 \
	INSTALL.wince \
	extras/package/win32/vlc.exe.manifest \
	extras/package/win32/libvlc.dll.manifest \
	extras/package/macosx/README.MacOSX.rtf \
	vlc-config.in.in \
	extras/package/rpm/vlc.fedora.spec \
	extras/package/rpm/vlc.altlinux.spec \
	extras/package/win32/vlc.win32.nsi.in \
	extras/package/win32/spad.nsi.in \
	extras/package/win32/UAC.nsh \
	extras/package/win32/UAC.dll \
	extras/package/win32/languages/declaration.nsh \
	extras/package/win32/languages/bengali.nsh \
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
	extras/package/win32/languages/occitan.nsh \
	extras/package/win32/languages/polish.nsh \
	extras/package/win32/languages/punjabi.nsh \
	extras/package/win32/languages/romanian.nsh \
	extras/package/win32/languages/schinese.nsh \
	extras/package/win32/languages/slovak.nsh \
	extras/package/win32/languages/slovenian.nsh \
	extras/package/win32/languages/sorani.nsh \
	extras/package/win32/languages/spanish.nsh

dist_noinst_SCRIPTS = bootstrap toolbox
nodist_noinst_SCRIPTS = compile

BUILT_SOURCES_distclean = vlc-config extras/package/win32/vlc.win32.nsi extras/package/win32/spad.nsi
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
	--with-kde-solid='$${datadir}/kde4/apps'

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = \
	1.9 \
	-Wall \
	check-news \
	dist-bzip2 \
	no-dist-gzip
#	std-options

ChangeLog: Makefile.am
	rm -f -- "$@"
	cd doc && $(MAKE) $(AM_MAKEFLAGS) changelogs
	ln -sf doc/ChangeLog-2009 "$@"

###############################################################################
# MacOS X project
###############################################################################

EXTRA_DIST += \
	extras/package/macosx/Resources/English.lproj/MainMenu.nib/designable.nib \
	extras/package/macosx/Resources/English.lproj/MainMenu.nib/keyedobjects.nib \
	extras/package/macosx/Resources/English.lproj/MediaInfo.nib/designable.nib \
	extras/package/macosx/Resources/English.lproj/MediaInfo.nib/keyedobjects.nib \
	extras/package/macosx/Resources/English.lproj/About.nib/designable.nib \
	extras/package/macosx/Resources/English.lproj/About.nib/keyedobjects.nib \
	extras/package/macosx/Resources/English.lproj/Open.nib/designable.nib \
	extras/package/macosx/Resources/English.lproj/Open.nib/keyedobjects.nib \
	extras/package/macosx/Resources/English.lproj/Preferences.nib/designable.nib \
	extras/package/macosx/Resources/English.lproj/Preferences.nib/keyedobjects.nib \
	extras/package/macosx/Resources/English.lproj/Wizard.nib/designable.nib \
	extras/package/macosx/Resources/English.lproj/Wizard.nib/keyedobjects.nib \
	extras/package/macosx/Resources/English.lproj/Bookmarks.nib/designable.nib \
	extras/package/macosx/Resources/English.lproj/Bookmarks.nib/keyedobjects.nib \
	extras/package/macosx/Resources/English.lproj/Extended.nib/designable.nib \
	extras/package/macosx/Resources/English.lproj/Extended.nib/keyedobjects.nib \
	extras/package/macosx/Resources/English.lproj/CoreDialogs.nib/designable.nib \
	extras/package/macosx/Resources/English.lproj/CoreDialogs.nib/keyedobjects.nib \
	extras/package/macosx/Resources/English.lproj/ErrorPanel.nib/designable.nib \
	extras/package/macosx/Resources/English.lproj/ErrorPanel.nib/keyedobjects.nib \
	extras/package/macosx/Resources/English.lproj/InfoPlist.strings.in \
	extras/package/macosx/Resources/dsa_pub.pem \
	extras/package/macosx/Resources/aiff.icns \
	extras/package/macosx/Resources/audio.icns \
	extras/package/macosx/Resources/avi.icns \
	extras/package/macosx/Resources/flv.icns \
	extras/package/macosx/Resources/generic.icns \
	extras/package/macosx/Resources/m4a.icns \
	extras/package/macosx/Resources/m4v.icns \
	extras/package/macosx/Resources/mkv.icns \
	extras/package/macosx/Resources/mov.icns \
	extras/package/macosx/Resources/movie.icns \
	extras/package/macosx/Resources/mp3.icns \
	extras/package/macosx/Resources/mpeg.icns \
	extras/package/macosx/Resources/ogg.icns \
	extras/package/macosx/Resources/playlist.icns \
	extras/package/macosx/Resources/rm.icns \
	extras/package/macosx/Resources/subtitle.icns \
	extras/package/macosx/Resources/vob.icns \
	extras/package/macosx/Resources/wav.icns \
	extras/package/macosx/Resources/vlc.icns \
	extras/package/macosx/Resources/wma.icns \
	extras/package/macosx/Resources/wmv.icns \
	extras/package/macosx/Resources/pause.png \
	extras/package/macosx/Resources/pause_blue.png \
	extras/package/macosx/Resources/play.png \
	extras/package/macosx/Resources/play_blue.png \
	extras/package/macosx/Resources/stop.png \
	extras/package/macosx/Resources/stop_blue.png \
	extras/package/macosx/Resources/display_left.png \
	extras/package/macosx/Resources/display_middle.png \
	extras/package/macosx/Resources/display_right.png \
	extras/package/macosx/Resources/display_slider.png \
	extras/package/macosx/Resources/display_track.png \
	extras/package/macosx/Resources/equalizerdrawer_active.png \
	extras/package/macosx/Resources/equalizerdrawer_blue.png \
	extras/package/macosx/Resources/fullscreen_active.png \
	extras/package/macosx/Resources/fullscreen_blue.png \
	extras/package/macosx/Resources/next_active.png \
	extras/package/macosx/Resources/next_blue.png \
	extras/package/macosx/Resources/playlistdrawer_active.png \
	extras/package/macosx/Resources/playlistdrawer_blue.png \
	extras/package/macosx/Resources/previous_active.png \
	extras/package/macosx/Resources/previous_blue.png \
	extras/package/macosx/Resources/skip_forward_active.png \
	extras/package/macosx/Resources/skip_forward_blue.png \
	extras/package/macosx/Resources/skip_previous_active.png \
	extras/package/macosx/Resources/skip_previous_blue.png \
	extras/package/macosx/Resources/volume_high.png \
	extras/package/macosx/Resources/volume_low.png \
	extras/package/macosx/Resources/volumeslider_blue.png \
	extras/package/macosx/Resources/volumeslider_normal.png \
	extras/package/macosx/Resources/volumetrack.png \
	extras/package/macosx/Resources/about_bg.png \
	extras/package/macosx/Resources/skip_forward_active_embedded.png \
	extras/package/macosx/Resources/play_embedded.png \
	extras/package/macosx/Resources/pause_embedded.png \
	extras/package/macosx/Resources/skip_previous_active_embedded.png \
	extras/package/macosx/Resources/pause_embedded_blue.png \
	extras/package/macosx/Resources/play_embedded_blue.png \
	extras/package/macosx/Resources/skip_forward_embedded_blue.png \
	extras/package/macosx/Resources/skip_previous_embedded_blue.png \
	extras/package/macosx/Resources/vlc.scriptSuite \
	extras/package/macosx/Resources/vlc.scriptTerminology \
	extras/package/macosx/Resources/README \
	extras/package/macosx/Resources/add_embedded.png \
	extras/package/macosx/Resources/add_embedded_blue.png \
	extras/package/macosx/Resources/repeat_embedded.png \
	extras/package/macosx/Resources/repeat_embedded_blue.png \
	extras/package/macosx/Resources/repeat_single_embedded_blue.png \
	extras/package/macosx/Resources/shuffle_embedded.png \
	extras/package/macosx/Resources/shuffle_embedded_blue.png \
	extras/package/macosx/Resources/fs_volume_slider_knob_highlight.png \
	extras/package/macosx/Resources/fs_volume_slider_knob.png \
	extras/package/macosx/Resources/fs_volume_slider_bar.png \
	extras/package/macosx/Resources/fs_volume_mute_highlight.png \
	extras/package/macosx/Resources/fs_volume_mute.png \
	extras/package/macosx/Resources/fs_volume_max_highlight.png \
	extras/package/macosx/Resources/fs_volume_max.png \
	extras/package/macosx/Resources/fs_time_slider_knob_highlight.png \
	extras/package/macosx/Resources/fs_time_slider_knob.png \
	extras/package/macosx/Resources/fs_time_slider.png \
	extras/package/macosx/Resources/fs_stop_highlight.png \
	extras/package/macosx/Resources/fs_stop.png \
	extras/package/macosx/Resources/fs_skip_previous_highlight.png \
	extras/package/macosx/Resources/fs_skip_previous.png \
	extras/package/macosx/Resources/fs_skip_next_highlight.png \
	extras/package/macosx/Resources/fs_skip_next.png \
	extras/package/macosx/Resources/fs_rewind_highlight.png \
	extras/package/macosx/Resources/fs_rewind.png \
	extras/package/macosx/Resources/fs_play_highlight.png \
	extras/package/macosx/Resources/fs_play.png \
	extras/package/macosx/Resources/fs_pause_highlight.png \
	extras/package/macosx/Resources/fs_pause.png \
	extras/package/macosx/Resources/fs_forward_highlight.png \
	extras/package/macosx/Resources/fs_forward.png \
	extras/package/macosx/Resources/fs_exit_fullscreen_highlight.png \
	extras/package/macosx/Resources/fs_exit_fullscreen.png \
	extras/package/macosx/Resources/fs_background.png \
	extras/package/macosx/Resources/add_embedded_graphite.png \
	extras/package/macosx/Resources/equalizerdrawer_graphite.png \
	extras/package/macosx/Resources/fullscreen_graphite.png \
	extras/package/macosx/Resources/next_graphite.png \
	extras/package/macosx/Resources/pause_embedded_graphite.png \
	extras/package/macosx/Resources/pause_graphite.png \
	extras/package/macosx/Resources/play_embedded_graphite.png \
	extras/package/macosx/Resources/play_graphite.png \
	extras/package/macosx/Resources/playlistdrawer_graphite.png \
	extras/package/macosx/Resources/previous_graphite.png \
	extras/package/macosx/Resources/repeat_embedded_graphite.png \
	extras/package/macosx/Resources/repeat_single_embedded_graphite.png \
	extras/package/macosx/Resources/shuffle_embedded_graphite.png \
	extras/package/macosx/Resources/skip_forward_embedded_graphite.png \
	extras/package/macosx/Resources/skip_forward_graphite.png \
	extras/package/macosx/Resources/skip_previous_embedded_graphite.png \
	extras/package/macosx/Resources/skip_previous_graphite.png \
	extras/package/macosx/Resources/stop_graphite.png \
	extras/package/macosx/Resources/volumeslider_graphite.png \
	extras/package/macosx/Resources/backward_big_down.png \
	extras/package/macosx/Resources/backward_big.png \
	extras/package/macosx/Resources/eq_sm.pdf \
	extras/package/macosx/Resources/forward_big_down.png \
	extras/package/macosx/Resources/forward_big.png \
	extras/package/macosx/Resources/fs_sm.pdf \
	extras/package/macosx/Resources/list_sm.pdf \
	extras/package/macosx/Resources/next_sm.pdf \
	extras/package/macosx/Resources/pause_big_down.png \
	extras/package/macosx/Resources/pause_big.png \
	extras/package/macosx/Resources/play_big_down.png \
	extras/package/macosx/Resources/play_big.png \
	extras/package/macosx/Resources/prev_sm.pdf \
	extras/package/macosx/Resources/sidebarAdd.png \
	extras/package/macosx/Resources/sidebarRepeat.png \
	extras/package/macosx/Resources/sidebarRepeatOn.png \
	extras/package/macosx/Resources/sidebarRepeatOneOn.png \
	extras/package/macosx/Resources/sidebarResizeWidget.png \
	extras/package/macosx/Resources/sidebarShuffle.png \
	extras/package/macosx/Resources/sidebarShuffleOn.png \
	extras/package/macosx/Resources/sidebarStatusAreaBackground.png \
	extras/package/macosx/Resources/stop_sm.pdf \
	extras/package/macosx/Resources/noart.png \
	extras/package/macosx/Resources/spref_cone_Audio_64.png \
	extras/package/macosx/Resources/spref_cone_Hotkeys_64.png \
	extras/package/macosx/Resources/spref_cone_Input_64.png \
	extras/package/macosx/Resources/spref_cone_Interface_64.png \
	extras/package/macosx/Resources/spref_cone_Subtitles_64.png \
	extras/package/macosx/Resources/spref_cone_Video_64.png \
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
	extras/package/macosx/plugin/Info.plist.in \
	extras/package/macosx/plugin/InstallerInfo.plist.in \
	extras/package/macosx/plugin/InstallerDescription.plist \
	extras/package/macosx/plugin/pbdevelopment.plist \
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
	extras/misc/mpris.glade

dist-hook:
	distdir=$(distdir) srcdir=$(srcdir) $(SHELL) $(srcdir)/toolbox --dist-contrib

###############################################################################
# Building libvlc
###############################################################################

noinst_SCRIPTS = vlc-config
vlc-config: $(top_builddir)/config.status $(top_builddir)/vlc-config.in
	$(SHELL) ./config.status --file=$@
	chmod 0755 $@
	touch $@

vlc-config.in: vlc-config.in.in
	./config.status --recheck
	touch $@

CLEANFILES = $(BUILT_SOURCES_clean)
DISTCLEANFILES = $(BUILT_SOURCES_distclean) vlc-config.in compile
MAINTAINERCLEANFILES = ChangeLog

# Shortcut for developers to rebuild the core (libvlc + vlc)
# Don't use it if you don't know what it is about.
# Don't complain if it doesn't work. -- Courmisch
libcompat:
	cd compat && $(MAKE) $(AM_MAKEFLAGS)

libvlc: libcompat
	cd src && $(MAKE) $(AM_MAKEFLAGS) libvlc.la

core: libvlc vlc
	cd bin && $(MAKE) $(AM_MAKEFLAGS) vlc$(EXEEXT) vlc-static$(EXEEXT)

doc:
	cd doc && $(MAKE) $(AM_MAKEFLAGS) doc

.PHONY: libvlc core doc

if HAVE_BEOS
noinst_DATA = vlc-bundle
vlc-bundle: vlc
	rm -Rf $(top_builddir)/vlc-bundle ; mkdir -p $(top_builddir)/vlc-bundle
	cp $(top_builddir)/vlc $(top_builddir)/vlc-bundle/
	xres -o $(top_builddir)/vlc-bundle/vlc $(srcdir)/share/vlc_beos.rsrc
	for i in "" `$(VLC_CONFIG) --target plugin` ; do \
	  if test -n "$$i" ; then \
	    mkdir -p $(top_builddir)/vlc-bundle/plugins ; \
	    cp "$$i$(LIBEXT)" $(top_builddir)/vlc-bundle/plugins/ ; \
	  fi ; \
	done
	if test -d $(CONTRIB_DIR)/vlc-lib ; then \
	  mkdir -p $(top_builddir)/vlc-bundle/lib ; \
	  for i in $(CONTRIB_DIR)/vlc-lib/*.so ; do \
	    cp $$i $(top_builddir)/vlc-bundle/lib/ ; \
	  done ; \
	fi
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  mkdir -p "$(top_builddir)/vlc-bundle/share/locale/$$i/LC_MESSAGES" ; \
	  cp "$(top_builddir)/po/$$i.gmo" \
	    "$(top_builddir)/vlc-bundle/share/locale/$$i/LC_MESSAGES/vlc.mo" || true ; \
	done
	find $(top_builddir)/vlc-bundle -type f -exec mimeset -f "{}" \;
endif

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
noinst_SCRIPTS += vlc$(EXEEXT)
endif

vlc$(EXEEXT):
	$(AM_V_GEN)ln -sf bin/vlc-static$(EXEEXT) vlc$(EXEEXT)

if HAVE_DARWIN
if BUILD_MACOSX_VLC_APP
# Create the MacOS X app
noinst_DATA = VLC.app
endif
endif

silentstd = $(silentstd_$(V))
silentstd_ = $(silentstd_$(AM_DEFAULT_VERBOSITY))
silentstd_0 = 2>&1 >/dev/null

# VLC-release.app for packaging and giving it to your friends
# use package-macosx to get a nice dmg
VLC-release.app: vlc
	( cd src && $(MAKE) $(AM_MAKEFLAGS) install $(silentstd) )
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
	    equalizer.h \
	    equalizer.m \
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
	    vout.h \
	    voutgl.m \
	    wizard.h \
	    wizard.m \
	    extended.h \
	    extended.m \
	    bookmarks.h \
	    bookmarks.m \
	    coredialogs.h \
	    coredialogs.m \
	    embeddedwindow.h \
	    embeddedwindow.m \
	    fspanel.h \
	    fspanel.m \
	    vout.m; do \
	  cp "$(srcdir)/modules/gui/macosx/$$i" \
             $(top_builddir)/tmp/modules/gui/macosx; \
	done
	$(AM_V_GEN)cd $(top_builddir)/tmp/extras/package/macosx && \
        xcodebuild -target vlc SYMROOT=../../../build $(silentstd) && \
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
	$(INSTALL) -m 644 $(srcdir)/share/lua/http/requests/readme $(top_builddir)/VLC-release.app/Contents/MacOS/share/lua/http/requests/readme.txt
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/dialogs
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/js
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/images
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/requests
	$(INSTALL) -m 644 $(srcdir)/share/http/.hosts $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/.hosts
	for i in $(srcdir)/share/http/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/http/dialogs/* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/dialogs/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/http/js/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/js/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/http/images/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/images/`basename $${i}` ; \
	done
	for i in $(srcdir)/share/http/requests/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/requests/`basename $${i}` ; \
	done
	$(INSTALL) -m 644 $(srcdir)/share/http/requests/readme $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/requests/readme.txt
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/locale
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  $(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES ; \
	  $(INSTALL) $(srcdir)/po/$${i}.gmo $(top_builddir)/VLC-release.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	  mkdir -p $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/InfoPlist.strings \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/MainMenu.nib \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	done
	printf "APPLVLC#" >| $(top_builddir)/VLC-release.app/Contents/PkgInfo
	find $(top_builddir)/VLC-release.app/Contents/Resources/English.lproj -name designable.nib -delete;
	find $(top_builddir)/VLC-release.app -type d -exec chmod ugo+rx '{}' \;
	find $(top_builddir)/VLC-release.app -type f -exec chmod ugo+r '{}' \;

# This is just for development purposes. 
# The resulting VLC.app will only run in this tree.
VLC.app: vlc $(top_builddir)/src/.libs/libvlccore.dylib $(top_builddir)/src/.libs/libvlc.dylib
	$(AM_V_GEN)(cd src && make install $(silentstd))
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
	    equalizer.h \
	    equalizer.m \
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
	    vout.h \
	    voutgl.m \
	    wizard.h \
	    wizard.m \
	    extended.h \
	    extended.m \
	    bookmarks.h \
	    bookmarks.m \
	    coredialogs.h \
	    coredialogs.m \
	    embeddedwindow.h \
	    embeddedwindow.m \
	    fspanel.h \
	    fspanel.m \
	    vout.m; do \
	  cp "$(srcdir)/modules/gui/macosx/$$i" \
             $(top_builddir)/tmp/modules/gui/macosx; \
	done
	$(AM_V_GEN)cd $(top_builddir)/tmp/extras/package/macosx && \
	xcodebuild -target vlc SYMROOT=../../../build $(silentstd) && \
	cd ../../../../ && \
	cp -R $(top_builddir)/tmp/build/Default/VLC.bundle $(top_builddir)/VLC.app
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS
	touch $(top_builddir)/VLC.app/Contents/MacOS/VLC
	chmod +x $(top_builddir)/VLC.app/Contents/MacOS/VLC
	$(INSTALL) $(top_builddir)/bin/.libs/vlc $(top_builddir)/VLC.app/Contents/MacOS/VLC
	ln -sf ../../../modules $(top_builddir)/VLC.app/Contents/MacOS/plugins
	install -d $(top_builddir)/VLC.app/Contents/MacOS/share
	for i in `ls $(srcdir)/share`; do \
	   ln -sf `pwd`/$(srcdir)/share/$$i $(top_builddir)/VLC.app/Contents/MacOS/share/; \
	done
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS/share/locale
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  mkdir -p $(top_builddir)/VLC.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES ; \
	  ln -sfn `pwd`/$(srcdir)/po/$${i}.gmo $(top_builddir)/VLC.app/Contents/MacOS/share/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	  mkdir -p $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/InfoPlist.strings \
	      $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/MainMenu.nib \
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
win32_destdir=$(top_builddir)/vlc-$(VERSION)
win32_xpi_destdir=$(win32_destdir)/vlc-plugin
win32_debugdir=$(top_builddir)/symbols-$(VERSION)
npvlc=vlc-$(VERSION)/mozilla/npvlc$(LIBEXT)
axvlc=vlc-$(VERSION)/activex/axvlc$(LIBEXT)
win32_lua_destdir=$(win32_destdir)/lua
win32_http_destdir=$(win32_destdir)/http

#Win-common if for win32 and wince
package-win-common:
# Check that tmp isn't in the way
	@if test -e "$(win32_destdir)"; then \
	  echo "Error: please remove $(win32_destdir), it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(win32_destdir)"; \
	fi
# Same for debug symbols dir
	@if test -e "$(win32_debugdir)"; then \
	  echo "Error: please remove $(win32_debugdir), it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(win32_debugdir)"; \
	fi

# Copy relevant files
# Copy executables, major libs+manifests
	cp "$(top_builddir)/bin/.libs/vlc$(EXEEXT)" "$(win32_destdir)/"
	cp "$(top_builddir)/bin/.libs/vlc-cache-gen$(EXEEXT)" "$(win32_destdir)/"
	cp "$(top_srcdir)/extras/package/win32/vlc.exe.manifest" "$(win32_destdir)/"
	cp "$(top_srcdir)/extras/package/win32/libvlc.dll.manifest" "$(win32_destdir)/"
	cp "$(top_builddir)/src/.libs/libvlccore$(LIBEXT)" "$(win32_destdir)/"
	cp "$(top_builddir)/src/.libs/libvlc$(LIBEXT)" "$(win32_destdir)/"

# Copy Text files
	for file in AUTHORS THANKS ; \
	  do sed 's/@/_AT_/' < "$(srcdir)/$$file" > "$(win32_destdir)/$${file}.txt" ; done;
	for file in NEWS COPYING README; \
	  do cp "$(srcdir)/$$file" "$(win32_destdir)/$${file}.txt"; done
	$(U2D) "$(win32_destdir)/"*.txt || echo "WARNING: this shouldn't happen"

# Necessary icons
	cp $(srcdir)/share/icons/vlc.ico $(win32_destdir)/

# Copy the locales
	mkdir -p $(win32_destdir)/locale
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  mkdir -p "$(win32_destdir)/locale/$${i}/LC_MESSAGES" ; \
	  cp "$(srcdir)/po/$${i}.gmo" \
	    "$(win32_destdir)/locale/$${i}/LC_MESSAGES/vlc.mo" || true ; \
	done
	mkdir -p $(win32_destdir)/locale/qt4/
	cp $(QT4LOCALEDIR)/*.qm $(win32_destdir)/locale/qt4/ || true

#  Mozilla plugin
if BUILD_MOZILLA
	mkdir -p "$(win32_destdir)/mozilla"
	cp $(top_builddir)/projects/mozilla/.libs/npvlc$(LIBEXT) $(win32_destdir)/mozilla/
	cp $(top_srcdir)/projects/mozilla/npvlc.dll.manifest $(win32_destdir)/mozilla/
endif

# ActiveX plugin
if BUILD_ACTIVEX
	mkdir -p "$(win32_destdir)/activex"
	cp $(srcdir)/projects/activex/README.TXT  $(win32_destdir)/activex/
	cp $(srcdir)/projects/activex/test.html  $(win32_destdir)/activex/
	$(U2D) $(win32_destdir)/activex/*
	cp $(top_builddir)/projects/activex/.libs/axvlc$(LIBEXT) $(win32_destdir)/activex/
	cp $(top_srcdir)/projects/activex/axvlc.dll.manifest $(win32_destdir)/activex/
endif

# Rebase all those DLLs to speed up loading (need cygwin rebase)
	if rebase -b 0x42 /dev/null >/dev/null 2>&1; then \
		find $(win32_destdir) -type f -name '*.dll' -print | rebase -b 0x70000000 -T -; \
	fi

# Copy the UAC NSIS plugin
	mkdir -p "$(win32_destdir)/NSIS"
	cp "$(top_srcdir)/extras/package/win32/UAC.nsh" "$(win32_destdir)/NSIS"
	cp "$(top_srcdir)/extras/package/win32/UAC.dll" "$(win32_destdir)/NSIS"

prepare-win32-web-plugin: package-win-common-strip
	mkdir -p "$(win32_xpi_destdir)/plugins"
	cp $(win32_destdir)/mozilla/npvlc$(LIBEXT) \
		"$(win32_xpi_destdir)/plugins"
	cp $(top_srcdir)/projects/mozilla/npvlc.dll.manifest \
		"$(win32_xpi_destdir)/plugins"
	cp "$(top_srcdir)/extras/package/win32/libvlc.dll.manifest" \
		"$(win32_xpi_destdir)/plugins"
	cp "$(win32_destdir)/libvlccore$(LIBEXT)" \
		"$(win32_xpi_destdir)/plugins"
	cp "$(win32_destdir)/libvlc$(LIBEXT)" \
		"$(win32_xpi_destdir)/plugins"
	mkdir -p "$(win32_xpi_destdir)/plugins/plugins"
	cp $(win32_destdir)/plugins/*$(LIBEXT) \
		"$(win32_xpi_destdir)/plugins/plugins"
	rm -rf "$(win32_xpi_destdir)/plugins/plugins/*qt*"
	rm -rf "$(win32_xpi_destdir)/plugins/plugins/*skins*"
package-win32-xpi: prepare-win32-web-plugin
	cp $(top_builddir)/projects/mozilla/install.rdf "$(win32_xpi_destdir)"
	cd $(win32_xpi_destdir) && zip -r "../vlc-$(VERSION).xpi" \
					install.rdf plugins

package-win32-crx: prepare-win32-web-plugin
	cp $(top_builddir)/projects/mozilla/manifest.json "$(win32_xpi_destdir)"
	crxmake --pack-extension "$(win32_xpi_destdir)" \
		--extension-output "$(win32_destdir)/vlc-$(VERSION).crx" \
		--ignore-file install.rdf

package-win32-base-debug: package-win-common
# Copy relevant files
# Script installer
	cp "$(top_builddir)/extras/package/win32/vlc.win32.nsi" "$(win32_destdir)/"
	cp "$(top_builddir)/extras/package/win32/spad.nsi" "$(win32_destdir)/"
	mkdir "$(win32_destdir)/languages"
	cp $(srcdir)/extras/package/win32/languages/*.nsh "$(win32_destdir)/languages/"

# Plugins
	mkdir -p "$(win32_destdir)/plugins"
	find modules/ -name '*_plugin$(LIBEXT)' | while read i; do \
	  if test -n "$$i" ; then \
	    $(INSTALL) "$$i" "vlc-$(VERSION)/plugins/" ; \
	  fi ; done

#Enable DEP and ASLR for all the binaries
if USE_PEFLAGS
	find $(win32_destdir) -type f -name '*.dll' -print -o -name '*.exe' -print | while read i; do \
	$(PEFLAGS) --dynamicbase=true --nxcompat=true "$$i" ; \
	done
endif

if BUILD_LUA
# Copy the lua scripts (HTTP)
	mkdir -p "$(win32_lua_destdir)/http/images"
	mkdir -p "$(win32_lua_destdir)/http/requests"
	mkdir -p "$(win32_lua_destdir)/http/js"
	mkdir -p "$(win32_lua_destdir)/http/dialogs"
	cp $(srcdir)/share/lua/http/*.html $(win32_lua_destdir)/http/
	$(U2D) $(win32_lua_destdir)/http/*.html
	cp $(srcdir)/share/lua/http/.hosts $(win32_lua_destdir)/http/
	$(U2D) $(win32_lua_destdir)/http/.hosts
	cp $(srcdir)/share/lua/http/*.css $(win32_lua_destdir)/http/
	$(U2D) $(win32_lua_destdir)/http/*.css
	cp $(srcdir)/share/lua/http/js/*.js $(win32_lua_destdir)/http/js/
	$(U2D) $(win32_lua_destdir)/http/js/*.js
	cp $(srcdir)/share/lua/http/*.lua $(win32_lua_destdir)/http/
	$(U2D) $(win32_lua_destdir)/http/*.lua
	cp $(srcdir)/share/lua/http/dialogs/* $(win32_lua_destdir)/http/dialogs/
	$(U2D) $(win32_lua_destdir)/http/dialogs/*
	cp $(srcdir)/share/lua/http/dialogs/.hosts $(win32_lua_destdir)/http/dialogs/
	$(U2D) $(win32_lua_destdir)/http/dialogs/.hosts
	cp $(srcdir)/share/lua/http/*.ico $(win32_lua_destdir)/http/
	cp $(srcdir)/share/lua/http/images/*.png $(win32_lua_destdir)/http/images/
	cp $(srcdir)/share/lua/http/requests/*.xml $(win32_lua_destdir)/http/requests/
	$(U2D) $(win32_lua_destdir)/http/requests/*.xml
	cp $(srcdir)/share/lua/http/requests/readme $(win32_lua_destdir)/http/requests/readme.txt
	$(U2D) $(win32_lua_destdir)/http/requests/readme.txt

#Lua Scripts
	$(INSTALL) -d $(win32_lua_destdir)
	for i in $(srcdir)/share/lua/*.* ; do \
	  $(INSTALL) -m 644 -- "$${i}" $(win32_lua_destdir)/`basename $${i}` ; \
	done
	$(INSTALL) -d $(win32_lua_destdir)/modules
	for i in $(srcdir)/share/lua/modules/*.* ; do \
	  $(INSTALL) -m 644 -- "$${i}" $(win32_lua_destdir)/modules/`basename $${i}` ; \
	done
	$(INSTALL) -d $(win32_lua_destdir)/extensions
	for i in $(srcdir)/share/lua/extensions/*.* ; do \
	  $(INSTALL) -m 644 -- "$${i}" $(win32_lua_destdir)/extensions/`basename $${i}` ; \
	done
	$(INSTALL) -d $(win32_lua_destdir)/intf
	for i in $(srcdir)/share/lua/intf/*.* ; do \
	  $(INSTALL) -m 644 -- "$${i}" $(win32_lua_destdir)/intf/`basename $${i}` ; \
	done
	$(INSTALL) -d $(win32_lua_destdir)/intf/modules
	for i in $(srcdir)/share/lua/intf/modules/*.* ; do \
	  $(INSTALL) -m 644 -- "$${i}" $(win32_lua_destdir)/intf/modules/`basename $${i}` ; \
	done
	$(INSTALL) -d $(win32_lua_destdir)/meta/art
	for i in $(srcdir)/share/lua/meta/art/*.* ; do \
	  $(INSTALL) -m 644 -- "$${i}" $(win32_lua_destdir)/meta/art/`basename $${i}` ; \
	done
	$(INSTALL) -d $(win32_lua_destdir)/meta/fetcher
	for i in $(srcdir)/share/lua/meta/fetcher/*.* ; do \
	  $(INSTALL) -m 644 -- "$${i}" $(win32_lua_destdir)/meta/fetcher/`basename $${i}` ; \
	done
	$(INSTALL) -d $(win32_lua_destdir)/meta/reader
	for i in $(srcdir)/share/lua/meta/reader/*.* ; do \
	  $(INSTALL) -m 644 -- "$${i}" $(win32_lua_destdir)/meta/reader/`basename $${i}` ; \
	done
	$(INSTALL) -d $(win32_lua_destdir)/playlist
	for i in $(srcdir)/share/lua/playlist/*.* ; do \
	  $(INSTALL) -m 644 -- "$${i}" $(win32_lua_destdir)/playlist/`basename $${i}` ; \
	done
	$(INSTALL) -d $(win32_lua_destdir)/sd
	for i in $(srcdir)/share/lua/sd/*.* ; do \
	  $(INSTALL) -m 644 -- "$${i}" $(win32_lua_destdir)/sd/`basename $${i}` ; \
	done

	$(U2D) $(win32_lua_destdir)/README.txt
	$(U2D) $(win32_lua_destdir)/extensions/README.txt
	$(U2D) $(win32_lua_destdir)/intf/README.txt
	$(U2D) $(win32_lua_destdir)/meta/art/README.txt
	$(U2D) $(win32_lua_destdir)/meta/fetcher/README.txt
	$(U2D) $(win32_lua_destdir)/meta/reader/README.txt
	$(U2D) $(win32_lua_destdir)/playlist/README.txt
	$(U2D) $(win32_lua_destdir)/sd/README.txt
endif

# Copy the http files
if BUILD_HTTPD
	mkdir -p "$(win32_http_destdir)/images"
	mkdir -p "$(win32_http_destdir)/requests"
	mkdir -p "$(win32_http_destdir)/js"
	mkdir -p "$(win32_http_destdir)/dialogs"
	cp $(srcdir)/share/http/*.html $(win32_http_destdir)/
	$(U2D) $(win32_http_destdir)/*.html
	cp $(srcdir)/share/http/.hosts $(win32_http_destdir)/
	$(U2D) $(win32_http_destdir)/.hosts
	cp $(srcdir)/share/http/*.css $(win32_http_destdir)/
	$(U2D) $(win32_http_destdir)/*.css
	cp $(srcdir)/share/http/js/*.js $(win32_http_destdir)/js/
	$(U2D) $(win32_http_destdir)/js/*.js
	cp $(srcdir)/share/http/dialogs/* $(win32_http_destdir)/dialogs/
	$(U2D) $(win32_http_destdir)/dialogs/*
	cp $(srcdir)/share/http/dialogs/.hosts $(win32_http_destdir)/dialogs/
	$(U2D) $(win32_http_destdir)/dialogs/.hosts
	cp $(srcdir)/share/http/*.ico $(win32_http_destdir)/
	cp $(srcdir)/share/http/images/*.png $(win32_http_destdir)/images/
	cp $(srcdir)/share/http/requests/*.xml $(win32_http_destdir)/requests/
	$(U2D) $(win32_http_destdir)/requests/*.xml
	cp $(srcdir)/share/http/requests/readme $(win32_http_destdir)/requests/readme.txt
	$(U2D) $(win32_http_destdir)/requests/readme.txt

endif

if BUILD_SKINS
# Skins
	mkdir -p $(win32_destdir)/skins/fonts
	for i in $(srcdir)/share/skins2/fonts/*.*; do \
	  cp -- "$$i" $(win32_destdir)/skins/fonts/ || true ; \
	done
	for i in $(top_builddir)/share/skins2/*.*; do \
	  cp -- "$$i" $(win32_destdir)/skins/ || true ; \
	done
	cp -- $(srcdir)/share/skins2/skin.catalog $(win32_destdir)/skins/ || true ;
	cp -- $(srcdir)/share/skins2/skin.dtd $(win32_destdir)/skins/ || true ;
endif

if BUILD_OSDMENU
#OSD Menu
	mkdir -p "$(win32_destdir)/osdmenu"
	cp $(srcdir)/share/osdmenu/*.* "$(win32_destdir)/osdmenu"
	for dir in default default/selected default/selection default/volume ;do \
		mkdir -p "$(win32_destdir)/osdmenu/$$dir"; \
		for file in $(srcdir)/share/osdmenu/$${dir}/*.png;do \
			 cp -- "$$file" "$(win32_destdir)/osdmenu/$$dir"; \
		done; \
	done
	$(U2D) $(win32_destdir)/osdmenu/*.cfg;
	for file in $(win32_destdir)/osdmenu/*.cfg; do \
		sed 's%share/osdmenu%osdmenu%g' "$$file" > "$$file.tmp" || exit $$? ; \
		sed 's%/%\\%g' "$$file.tmp" > "$$file" || exit$$? ; \
		rm -f -- "$$file.tmp"; \
	done
endif

# SDK
	mkdir -p "$(win32_destdir)/sdk.tmp"
	mkdir -p "$(win32_destdir)/sdk"
	d=$$(pwd) && \
	cd src && \
	make install DESTDIR="$$d/vlc-$(VERSION)/sdk.tmp"
	cd vlc-$(VERSION) && mv sdk.tmp/$(prefix)/* sdk/
	find vlc-$(VERSION)/sdk.tmp -type d | sort -r | xargs rmdir
	rm -Rf vlc-$(VERSION)/sdk/bin

# Strip DLLs
package-win-common-strip: package-win32-base-debug
	for i in "" $(win32_destdir)/*$(LIBEXT) \
	            $(win32_destdir)/*$(EXEEXT) \
	            $(win32_destdir)/activex/*$(LIBEXT) \
	            $(win32_destdir)/mozilla/*$(LIBEXT) \
	            $(win32_destdir)/plugins/*$(LIBEXT) \
	; do \
	  if test -n "$$i" ; then \
	    $(OBJCOPY) --only-keep-debug "$$i" "$$i.dbg"; \
	    $(OBJCOPY) --strip-all "$$i" ; \
	    $(OBJCOPY) --add-gnu-debuglink="$$i.dbg" "$$i" ; \
	    mv "$$i.dbg" "$(win32_debugdir)"; \
	  fi ; \
	done

package-win32-base: package-win-common-strip

package-win32-base-exe: package-win32-base
# Create package
	rm -Rf $(win32_destdir)/sdk
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

package-win32-base-zip: package-win32-base
# Create package 
	rm -f -- vlc-$(VERSION)-win32.zip
	zip -r -9 vlc-$(VERSION)-win32.zip vlc-$(VERSION)

package-win32-base-debug-zip: package-win32-base-debug
# Create package
	rm -f -- vlc-$(VERSION)-win32-debug.zip
	zip -r -9 vlc-$(VERSION)-win32-debug.zip vlc-$(VERSION)

package-win32-base-7zip: package-win32-base
# Create package 
	7z a -t7z -m0=lzma -mx=9 -mfb=64 -md=32m -ms=on vlc-$(VERSION)-win32.7z vlc-$(VERSION)

package-win32-base-debug-7zip: package-win32-base-debug
# Create package
	7z a -t7z -m0=lzma -mx=9 -mfb=64 -md=32m -ms=on vlc-$(VERSION)-win32-debug.7z vlc-$(VERSION)

package-win32-cleanup:
	rm -Rf $(win32_destdir)
	rm -Rf $(win32_debugdir)

package-win32-exe: package-win32-base package-win32-base-exe package-win32-cleanup

package-win32-zip: package-win32-base-zip package-win32-cleanup

package-win32-debug-zip: package-win32-base-debug-zip package-win32-cleanup

package-win32-7zip: package-win32-base-7zip package-win32-cleanup

package-win32-debug-7zip: package-win32-base-debug-7zip package-win32-cleanup

package-win32-no-clean: package-win32-base-zip package-win32-base-7zip package-win32-base-exe 

package-win32: package-win32-no-clean package-win32-cleanup

package-win32-debug: package-win32-base-debug-zip package-win32-base-debug-7zip package-win32-cleanup

#######
# WinCE
#######
package-wince-base: package-win-common-strip

package-wince-base-zip: package-wince-base
# Create package 
	rm -f -- vlc-$(VERSION)-wince.zip
	zip -r -9 vlc-$(VERSION)-wince.zip vlc-$(VERSION)

package-wince: package-wince-base-zip package-win32-cleanup


#####################################################################
##   BeOS                                                          ##
#####################################################################
package-beos:
# Check that tmp isn't in the way
	@if test -e $(srcdir)/tmp; then \
	  echo "Error: please remove $(srcdir)/tmp, it is in the way"; \
	  false ; \
	else \
	  echo "OK." ; mkdir $(srcdir)/tmp ; \
	fi

# Copy relevant files
	mkdir -p $(srcdir)/tmp/vlc
	cd $(srcdir) && cp -R vlc-bundle/* AUTHORS COPYING ChangeLog README \
	  THANKS NEWS tmp/vlc/

# Create debug package
	xres -o $(srcdir)/tmp/vlc/vlc $(srcdir)/share/vlc_beos.rsrc
	find $(srcdir)/tmp/vlc -exec mimeset -f {} \;
	mv $(srcdir)/tmp/vlc $(srcdir)/tmp/vlc-$(VERSION)
	(cd $(srcdir)/tmp && zip -9 -r vlc-$(VERSION)-BeOS-debug.zip vlc-$(VERSION) )
	mv $(srcdir)/tmp/vlc-$(VERSION)-BeOS-debug.zip $(srcdir)/
	mv $(srcdir)/tmp/vlc-$(VERSION) $(srcdir)/tmp/vlc

# Create normal package
	$(STRIP) --strip-debug --strip-unneeded $(srcdir)/tmp/vlc/vlc
	find $(srcdir)/tmp/vlc -name 'lib*.so' -exec $(STRIP) \
	  --strip-debug --strip-unneeded "{}" \;
	xres -o $(srcdir)/tmp/vlc/vlc $(srcdir)/share/vlc_beos.rsrc
	find $(srcdir)/tmp/vlc -exec mimeset -f {} \;
	mv $(srcdir)/tmp/vlc $(srcdir)/tmp/vlc-$(VERSION)
	(cd $(srcdir)/tmp &&  zip -9 -r vlc-$(VERSION)-BeOS.zip vlc-$(VERSION) )
	mv $(srcdir)/tmp/vlc-$(VERSION)-BeOS.zip $(srcdir)/

# Clean up
	rm -Rf $(srcdir)/tmp

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
	ln -s /Applications $(top_builddir)/vlc-$(VERSION)/Applications

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

package-macosx-plugin:
if BUILD_MOZILLA
# Create Installer
	rm -rf "$(top_builddir)/macosx-plugin-instdata/"; true
	mkdir -p "$(top_builddir)/macosx-plugin-instdata"
	rm -f "$(top_builddir)/projects/mozilla/VLC Plugin.plugin/Contents/MacOS/modules/libmacosx_plugin.dylib" 
	cp -R "$(top_builddir)/projects/mozilla/VLC Plugin.plugin" "$(top_builddir)/macosx-plugin-instdata"
	rm -rf "$(top_builddir)/VLC Internet Plug-In.pkg"
	export PACKAGEMAKER=
	if test -x /Developer/Tools/packagemaker; then \
	   export PACKAGEMAKER=/Developer/Tools/packagemaker; \
	fi; \
	if test -x /Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker; then \
	   export PACKAGEMAKER=/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker; \
	fi; \
	$${PACKAGEMAKER} -build -ds -p "$(top_builddir)/VLC Internet Plug-In.pkg" \
	-f "$(top_builddir)/macosx-plugin-instdata" \
	-i "$(top_builddir)/extras/package/macosx/plugin/InstallerInfo.plist" \
	-d "$(top_srcdir)/extras/package/macosx/plugin/InstallerDescription.plist"; true
	rm -rf "$(top_builddir)/macosx-plugin-instdata/"

# Create disk image 
	rm -f "$(top_builddir)/vlc-plugin-$(VERSION).dmg"; true
	rm -rf "$(top_builddir)/vlc-plugin-$(VERSION)/"; true
	mkdir -p "$(top_builddir)/vlc-plugin-$(VERSION)/"
	mv "$(top_builddir)/VLC Internet Plug-In.pkg" "$(top_builddir)/vlc-plugin-$(VERSION)/"
	hdiutil create -srcfolder "$(top_builddir)/vlc-plugin-$(VERSION)" \
	  "$(top_builddir)/vlc-plugin-$(VERSION).dmg" -format UDBZ \
	  -scrub -imagekey zlib-level=9
	echo "Disk image creation completed:"
	rm -rf "$(top_builddir)/vlc-plugin-$(VERSION)"
endif

package-macosx-xpi:
if BUILD_MOZILLA
	mkdir -p "$(top_builddir)/macosx-xpi/plugins"
	cp $(top_builddir)/projects/mozilla/install.rdf \
		"$(top_builddir)/macosx-xpi/"
	cp -R "$(top_builddir)/projects/mozilla/VLC Plugin.plugin" \
		"$(top_builddir)/macosx-xpi/plugins"
	cd "$(top_builddir)/macosx-xpi" && zip -r vlc-$(VERSION).xpi install.rdf plugins
endif

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
macosx-sdk: Makefile.in $(HEADERS_include) vlc-config
	export MACOSX_DEPLOYMENT_TARGET=$(MACOSX_DEPLOYMENT_TARGET)
