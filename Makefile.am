###############################################################################
# Automake targets and declarations
###############################################################################

# SUBDIRS stores the directories where a "make" is required when building
# something. DIST_SUBDIRS stores the directories where nothing is built but
# which have makefiles with distribution information.
#  - intl (gettext) is needed by src and modules
#  - src (libvlc) is nedeed by modules, mozilla and bindings
#  - libs/* are needed by modules
SUBDIRS = intl po libs/loader libs/srtp src modules \
		mozilla bindings activex share doc
DIST_SUBDIRS = $(SUBDIRS) m4 ipkg

EXTRA_DIST = \
	HACKING \
	INSTALL.win32 \
	INSTALL.wince \
	vlc.exe.manifest \
	MAINTAINERS \
	README.MacOSX.rtf \
	vlc-config.in.in \
	vlc.fedora.spec \
	vlc.mandriva.spec \
	vlc.altlinux.spec \
	vlc.win32.nsi.in

dist_noinst_SCRIPTS = bootstrap toolbox

BUILT_SOURCES_distclean = vlc-config compile vlc.win32.nsi
if HAVE_DARWIN
BUILT_SOURCES_clean = macosx-sdk
else
BUILT_SOURCES_clean =
endif

BUILT_SOURCES = $(BUILT_SOURCES_distclean) $(BUILT_SOURCES_clean)

SUFFIXES = 

DISTCHECK_CONFIGURE_FLAGS = --disable-dvd --disable-mad --disable-libmpeg2 \
	--disable-ffmpeg --disable-faad --disable-wxwidgets --disable-skins2

# Tell aclocal to use -I m4. Wonder if it really works.
ACLOCAL_AMFLAGS = -I m4

###############################################################################
# MacOS X project
###############################################################################

EXTRA_DIST += \
	extras/MacOSX/Resources/English.lproj/MainMenu.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/MainMenu.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/MainMenu.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/About.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/About.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/About.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/Open.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/Open.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/Open.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/Preferences.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/Preferences.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/Preferences.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/Wizard.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/Wizard.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/Wizard.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/Bookmarks.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/Bookmarks.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/Bookmarks.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/Extended.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/Extended.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/Extended.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/SFilters.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/SFilters.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/SFilters.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/Update.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/Update.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/Update.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/Interaction.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/Interaction.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/Interaction.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/InteractionErrorPanel.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/InteractionErrorPanel.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/InteractionErrorPanel.nib/keyedobjects.nib \
	extras/MacOSX/Resources/English.lproj/InfoPlist.strings \
	extras/MacOSX/Resources/a52.icns \
	extras/MacOSX/Resources/aac.icns \
	extras/MacOSX/Resources/asf.icns \
	extras/MacOSX/Resources/asx.icns \
	extras/MacOSX/Resources/avi.icns \
	extras/MacOSX/Resources/bin.icns \
	extras/MacOSX/Resources/cue.icns \
	extras/MacOSX/Resources/dat.icns \
	extras/MacOSX/Resources/divx.icns \
	extras/MacOSX/Resources/dv.icns \
	extras/MacOSX/Resources/generic.icns \
	extras/MacOSX/Resources/m3u.icns \
	extras/MacOSX/Resources/mov.icns \
	extras/MacOSX/Resources/mp3.icns \
	extras/MacOSX/Resources/mp4.icns \
	extras/MacOSX/Resources/mpeg.icns \
	extras/MacOSX/Resources/mpeg1.icns \
	extras/MacOSX/Resources/mpeg2.icns \
	extras/MacOSX/Resources/mpeg4.icns \
	extras/MacOSX/Resources/mpg.icns \
	extras/MacOSX/Resources/ogg.icns \
	extras/MacOSX/Resources/ogm.icns \
	extras/MacOSX/Resources/pls.icns \
	extras/MacOSX/Resources/srt.icns \
	extras/MacOSX/Resources/sub.icns \
	extras/MacOSX/Resources/vlc.icns \
	extras/MacOSX/Resources/vob.icns \
	extras/MacOSX/Resources/wma.icns \
	extras/MacOSX/Resources/wmv.icns \
	extras/MacOSX/Resources/pause.png \
	extras/MacOSX/Resources/pause_blue.png \
	extras/MacOSX/Resources/play.png \
	extras/MacOSX/Resources/play_blue.png \
	extras/MacOSX/Resources/stop.png \
	extras/MacOSX/Resources/stop_blue.png \
	extras/MacOSX/Resources/display.png \
	extras/MacOSX/Resources/display_slider.png \
	extras/MacOSX/Resources/display_track.png \
	extras/MacOSX/Resources/equalizerdrawer_active.png \
	extras/MacOSX/Resources/equalizerdrawer_blue.png \
	extras/MacOSX/Resources/fullscreen_active.png \
	extras/MacOSX/Resources/fullscreen_blue.png \
	extras/MacOSX/Resources/next_active.png \
	extras/MacOSX/Resources/next_blue.png \
	extras/MacOSX/Resources/playlistdrawer_active.png \
	extras/MacOSX/Resources/playlistdrawer_blue.png \
	extras/MacOSX/Resources/previous_active.png \
	extras/MacOSX/Resources/previous_blue.png \
	extras/MacOSX/Resources/skip_forward_active.png \
	extras/MacOSX/Resources/skip_forward_blue.png \
	extras/MacOSX/Resources/skip_previous_active.png \
	extras/MacOSX/Resources/skip_previous_blue.png \
	extras/MacOSX/Resources/volume_high.png \
	extras/MacOSX/Resources/volume_low.png \
	extras/MacOSX/Resources/volumeslider_blue.png \
	extras/MacOSX/Resources/volumeslider_normal.png \
	extras/MacOSX/Resources/volumetrack.png \
	extras/MacOSX/Resources/about_bg.png \
	extras/MacOSX/Resources/skip_forward_active_embedded.png \
	extras/MacOSX/Resources/play_embedded.png \
	extras/MacOSX/Resources/pause_embedded.png \
	extras/MacOSX/Resources/skip_previous_active_embedded.png \
	extras/MacOSX/Resources/pause_embedded_blue.png \
	extras/MacOSX/Resources/play_embedded_blue.png \
	extras/MacOSX/Resources/skip_forward_embedded_blue.png \
	extras/MacOSX/Resources/skip_previous_embedded_blue.png \
	extras/MacOSX/Resources/vlc.scriptSuite \
	extras/MacOSX/Resources/vlc.scriptTerminology \
	extras/MacOSX/Resources/README \
	extras/MacOSX/Resources/add_embedded.png \
	extras/MacOSX/Resources/add_embedded_blue.png \
	extras/MacOSX/Resources/repeat_embedded.png \
	extras/MacOSX/Resources/repeat_embedded_blue.png \
	extras/MacOSX/Resources/repeat_single_embedded_blue.png \
	extras/MacOSX/Resources/shuffle_embedded.png \
	extras/MacOSX/Resources/shuffle_embedded_blue.png \
	extras/MacOSX/Resources/fs_volume_slider_knob_highlight.png \
	extras/MacOSX/Resources/fs_volume_slider_knob.png \
	extras/MacOSX/Resources/fs_volume_slider_bar.png \
	extras/MacOSX/Resources/fs_volume_mute_highlight.png \
	extras/MacOSX/Resources/fs_volume_mute.png \
	extras/MacOSX/Resources/fs_volume_max_highlight.png \
	extras/MacOSX/Resources/fs_volume_max.png \
	extras/MacOSX/Resources/fs_time_slider_knob_highlight.png \
	extras/MacOSX/Resources/fs_time_slider_knob.png \
	extras/MacOSX/Resources/fs_time_slider.png \
	extras/MacOSX/Resources/fs_stop_highlight.png \
	extras/MacOSX/Resources/fs_stop.png \
	extras/MacOSX/Resources/fs_skip_previous_highlight.png \
	extras/MacOSX/Resources/fs_skip_previous.png \
	extras/MacOSX/Resources/fs_skip_next_highlight.png \
	extras/MacOSX/Resources/fs_skip_next.png \
	extras/MacOSX/Resources/fs_rewind_highlight.png \
	extras/MacOSX/Resources/fs_rewind.png \
	extras/MacOSX/Resources/fs_play_highlight.png \
	extras/MacOSX/Resources/fs_play.png \
	extras/MacOSX/Resources/fs_pause_highlight.png \
	extras/MacOSX/Resources/fs_pause.png \
	extras/MacOSX/Resources/fs_forward_highlight.png \
	extras/MacOSX/Resources/fs_forward.png \
	extras/MacOSX/Resources/fs_exit_fullscreen_highlight.png \
	extras/MacOSX/Resources/fs_exit_fullscreen.png \
	extras/MacOSX/Resources/fs_background.png \
	extras/MacOSX/fullscreen_panel.svg \
	extras/MacOSX/ub.sh \
	extras/MacOSX/vlc.xcodeproj/project.pbxproj \
	extras/MacOSX/Delete_Preferences.app/Contents/Info.plist \
	extras/MacOSX/Delete_Preferences.app/Contents/PkgInfo \
	extras/MacOSX/Delete_Preferences.app/Contents/MacOS/applet \
	extras/MacOSX/Delete_Preferences.app/Contents/Resources/description.rtfd/TXT.rtf \
	extras/MacOSX/Delete_Preferences.app/Contents/Resources/applet.icns \
	extras/MacOSX/Delete_Preferences.app/Contents/Resources/applet.rsrc \
	extras/MacOSX/Delete_Preferences.app/Contents/Resources/Scripts/main.scpt \
	extras/MacOSX/plugin/Info.plist \
	extras/MacOSX/plugin/InstallerInfo.plist \
	extras/MacOSX/plugin/InstallerDescription.plist \
	extras/MacOSX/plugin/pbdevelopment.plist \
	extras/MacOSX/plugin/English.lproj/InfoPlist.strings

###############################################################################
# Various utilities ( editor syntax files, D-Bus controller ... )
##############################################################################
EXTRA_DIST += \
	extras/Makefile \
	extras/zsh.cpp \
	extras/emacs.init \
	extras/vlc.vim \
	extras/make.pl \
	extras/valgrind.suppressions \
	extras/m4/autoconf260.m4 \
	extras/mpris.py \
	extras/mpris.glade

dist-hook:
	distdir=$(distdir) srcdir=$(srcdir) $(SHELL) $(srcdir)/toolbox --dist-contrib

###############################################################################
# Building libvlc
###############################################################################

bin_SCRIPTS = vlc-config
vlc-config: $(top_builddir)/config.status $(top_builddir)/vlc-config.in
	$(SHELL) ./config.status --file=$@
	chmod 0755 $@
	touch $@

vlc-config.in: vlc-config.in.in
	./config.status --recheck
	touch $@

CLEANFILES = $(BUILT_SOURCES_clean) stamp-builtin
DISTCLEANFILES = $(BUILT_SOURCES_distclean) vlc-config.in compile

# Shortcut for developpers to rebuild the core (libvlc + vlc)
# Don't use it if you don't know what it is about.
# Don't complain if it doesn't work. -- Courmisch
libvlc:
	cd src && $(MAKE) $(AM_MAKEFLAGS) $(LIB_libvlc)

.PHONY: libvlc

###############################################################################
# Building vlc
###############################################################################

bin_PROGRAMS = vlc

vlc_SOURCES = src/vlc.c
# Ugly kludge so that automake defines $(CXXLINK)
nodist_EXTRA_vlc_SOURCES = extras/zsh.cpp src/misc/darwin_specific.m

vlc_DEPENDENCIES = $(DATA_win32_rc)

vlc_LDADD = $(DATA_win32_rc) $(LIBVLC) $(LTLIBINTL) `$(VLC_CONFIG) -libs vlc`
vlc_LDFLAGS = `$(VLC_CONFIG) --ldflags vlc`
if !HAVE_PLUGINS
vlc_LDFLAGS += `$(VLC_CONFIG) --target plugin | sed -e 's/\_plugin /_plugin.la /g'`
endif

vlc_CFLAGS = `$(VLC_CONFIG) --cflags vlc`
LIB_libvlc = libvlc.la
LIBVLC = src/$(LIB_libvlc)

#To get some nice output
nice:
	$(top_builddir)/compile

core: libvlc vlc$(EXEEXT)

.PHONY: core

vlc$(EXEEXT): $(vlc_OBJECTS) $(vlc_DEPENDENCIES)
	@rm -f vlc$(EXEEXT)
	@case `$(VLC_CONFIG) --linkage vlc builtin` in \
	  c++) cmd="$(CXXLINK)" ;; \
	  objc) cmd="$(OBJCLINK)" ;; \
	  c|*) cmd="$(LINK)" ;; \
	esac ; \
	cmd="$$cmd $(vlc_OBJECTS) $(vlc_LDADD) $(vlc_LDFLAGS)" ; \
	echo $$cmd ; \
	eval $$cmd

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
	if test -d $(top_builddir)/extras/contrib/vlc-lib ; then \
	  mkdir -p $(top_builddir)/vlc-bundle/lib ; \
	  for i in $(top_builddir)/extras/contrib/vlc-lib/*.so ; do \
	    cp $$i $(top_builddir)/vlc-bundle/lib/ ; \
	  done ; \
	fi
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  mkdir -p "$(top_builddir)/vlc-bundle/locale/$$i/LC_MESSAGES" ; \
	  cp "$(top_builddir)/po/$$i.gmo" \
	    "$(top_builddir)/vlc-bundle/locale/$$i/LC_MESSAGES/vlc.mo" || true ; \
	done
	find $(top_builddir)/vlc-bundle -type f -exec mimeset -f "{}" \;
endif

###############################################################################
# Building aliases
###############################################################################

ALL_ALIASES = cvlc rvlc svlc wxvlc qvlc nvlc
bin_SCRIPTS += $(ALIASES)
EXTRA_SCRIPTS = $(ALL_ALIASES)

dist_noinst_SCRIPTS += make-alias

MKALIAS = bindir="$(bindir)" transform="$(transform)" $(top_srcdir)/make-alias $@

cvlc: make-alias
	$(MKALIAS) dummy

rvlc: make-alias
	$(MKALIAS) rc

svlc: make-alias
	$(MKALIAS) skins2

wxvlc: make-alias
	$(MKALIAS) wx

qvlc: make-alias
	$(MKALIAS) qt4

nvlc: make-alias
	$(MKALIAS) ncurses

if HAVE_DARWIN
# Create the MacOS X app
vlc_app_DATA = VLC.app
vlc_appdir = $(bindir)
# VLC-release.app is the old VLC.app target
VLC-release.app: vlc
	@if test -e "$(top_builddir)/tmp"; then \
	  echo "Error: please remove $(top_builddir)/tmp, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(top_builddir)/tmp/extras/MacOSX"; \
	fi
	rm -Rf $(top_builddir)/VLC-release.app
	cp -R $(srcdir)/extras/MacOSX/vlc.xcodeproj $(top_builddir)/tmp/extras/MacOSX/
	cp -R $(srcdir)/extras/MacOSX/Resources $(top_builddir)/tmp/extras/MacOSX
	for i in AUTHORS COPYING README.MacOSX.rtf THANKS; do \
	  cp "$(srcdir)/$$i" $(top_builddir)/tmp; \
	done
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
	    vout.h \
	    voutqt.m \
	    voutgl.m \
	    wizard.h \
	    wizard.m \
	    extended.h \
	    extended.m \
	    bookmarks.h \
	    bookmarks.m \
	    sfilters.h \
	    sfilters.m \
	    update.h \
	    update.m \
	    interaction.h \
	    interaction.m \
	    embeddedwindow.h \
	    embeddedwindow.m \
	    fspanel.h \
	    fspanel.m \
	    vout.m; do \
	  cp "$(srcdir)/modules/gui/macosx/$$i" \
             $(top_builddir)/tmp/modules/gui/macosx; \
	done
	cd $(top_builddir)/tmp/extras/MacOSX && xcodebuild -target vlc | grep -v '^\([ \t]\|$$\)' && \
	    cd ../../../ && \
	    cp -R $(top_builddir)/tmp/extras/MacOSX/build/Default/VLC.bundle \
	          $(top_builddir)/VLC-release.app; \
	rm -Rf $(top_builddir)/tmp
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/lib
	$(INSTALL) $(top_builddir)/.libs/vlc \
		   $(top_builddir)/VLC-release.app/Contents/MacOS/VLC
	$(INSTALL) $(top_builddir)/src/.libs/libvlc.1.dylib \
		   $(top_builddir)/VLC-release.app/Contents/MacOS/lib/libvlc.dylib
	$(INSTALL) $(top_builddir)/src/.libs/libvlc-control.1.dylib \
		   $(top_builddir)/VLC-release.app/Contents/MacOS/lib/libvlc-control.dylib
	install_name_tool -change /usr/local/lib/libvlc.1.dylib \
		   @executable_path/lib/libvlc.dylib \
		   $(top_builddir)/VLC-release.app/Contents/MacOS/lib/libvlc-control.dylib
	install_name_tool -change /usr/local/lib/libvlc.1.dylib \
		   @executable_path/lib/libvlc.dylib \
	       $(top_builddir)/VLC-release.app/Contents/MacOS/VLC
	ln -sf ./VLC $(top_builddir)/VLC-release.app/Contents/MacOS/clivlc
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/modules
	for i in `$(VLC_CONFIG) --target plugin` ; do \
	  if test -n "$$i" ; \
	    then $(INSTALL) "`pwd`/`dirname $$i`/.libs/`basename $$i`$(LIBEXT)" \
			   "$(top_builddir)/VLC-release.app/Contents/MacOS/modules" ; \
	  fi ; done
	if test -d $(srcdir)/extras/contrib/vlc-lib; then \
	  mkdir -p $(top_builddir)/VLC-release.app/Contents/MacOS/lib ; \
	  for i in $(srcdir)/extras/contrib/vlc-lib/*.dylib ; do \
	    $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/lib/`basename $${i}` ; \
	  done ; \
	fi
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/luaplaylist
	for i in $(srcdir)/share/luaplaylist/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/luaplaylist/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/luameta
	for i in $(srcdir)/share/luameta/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/luameta/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/luaintf
	for i in $(srcdir)/share/luaintf/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/luaintf/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/luaintf/modules
	for i in $(srcdir)/share/luaintf/modules/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/luaintf/modules/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/dialogs
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/js
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/old
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/old/admin
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/old/vlm
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/images
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/requests
	$(INSTALL) -m 644 $(srcdir)/share/http-lua/.hosts $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/.hosts
	for i in $(srcdir)/share/http-lua/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http-lua/dialogs/* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/dialogs/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http-lua/js/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/js/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http-lua/old/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/old/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http-lua/old/admin/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/old/admin/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http-lua/old/vlm/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/old/vlm/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http-lua/images/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/images/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http-lua/requests/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/requests/`basename $${i}` ; \
	done ; \
	$(INSTALL) -m 644 $(srcdir)/share/http-lua/requests/readme $(top_builddir)/VLC-release.app/Contents/MacOS/share/http-lua/requests/readme.txt
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/dialogs
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/js
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/old
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/old/admin
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/old/vlm
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/images
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/requests
	$(INSTALL) -m 644 $(srcdir)/share/http/.hosts $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/.hosts
	$(INSTALL) -m 644 $(srcdir)/share/http/old/.hosts $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/old/.hosts
	for i in $(srcdir)/share/http/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http/dialogs/* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/dialogs/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http/js/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/js/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http/old/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/old/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http/old/admin/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/old/admin/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http/old/vlm/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/old/vlm/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http/images/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/images/`basename $${i}` ; \
	done ; \
	for i in $(srcdir)/share/http/requests/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/requests/`basename $${i}` ; \
	done ; \
	$(INSTALL) -m 644 $(srcdir)/share/http/requests/readme $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/requests/readme.txt
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/locale
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  mkdir -p $(top_builddir)/VLC-release.app/Contents/MacOS/locale/$${i}/LC_MESSAGES ; \
	  cp $(srcdir)/po/$${i}.gmo $(top_builddir)/VLC-release.app/Contents/MacOS/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	  mkdir -p $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/InfoPlist.strings \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/MainMenu.nib \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	done
	printf "APPLVLC#" >| $(top_builddir)/VLC-release.app/Contents/PkgInfo
	find $(top_builddir)/VLC-release.app -type d -exec chmod ugo+rx '{}' \;
	find $(top_builddir)/VLC-release.app -type f -exec chmod ugo+r '{}' \;

VLC.app: vlc
	@if test -e "$(top_builddir)/tmp"; then \
	  echo "Error: please remove $(top_builddir)/tmp, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(top_builddir)/tmp/extras/MacOSX"; \
	fi
	rm -Rf $(top_builddir)/VLC.app
	cp -R $(srcdir)/extras/MacOSX/vlc.xcodeproj $(top_builddir)/tmp/extras/MacOSX/
	cp -R $(srcdir)/extras/MacOSX/Resources $(top_builddir)/tmp/extras/MacOSX
	for i in AUTHORS COPYING README.MacOSX.rtf THANKS; do \
	  cp "$(srcdir)/$$i" $(top_builddir)/tmp; \
	done
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
	    vout.h \
	    voutqt.m \
	    voutgl.m \
	    wizard.h \
	    wizard.m \
	    extended.h \
	    extended.m \
	    bookmarks.h \
	    bookmarks.m \
	    sfilters.h \
	    sfilters.m \
	    update.h \
	    update.m \
	    interaction.h \
	    interaction.m \
	    embeddedwindow.h \
	    embeddedwindow.m \
	    fspanel.h \
	    fspanel.m \
	    vout.m; do \
	  cp "$(srcdir)/modules/gui/macosx/$$i" \
             $(top_builddir)/tmp/modules/gui/macosx; \
	done
	cd $(top_builddir)/tmp/extras/MacOSX && xcodebuild -target vlc | grep -v '^\([ \t]\|$$\)' && \
	    cd ../../../ && \
	    cp -R $(top_builddir)/tmp/extras/MacOSX/build/Default/VLC.bundle \
	          $(top_builddir)/VLC.app; \
	rm -Rf $(top_builddir)/tmp
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS/lib
	$(INSTALL) $(top_builddir)/.libs/vlc \
		   $(top_builddir)/VLC.app/Contents/MacOS/VLC
	$(INSTALL) $(top_builddir)/src/.libs/libvlc.1.dylib \
		   $(top_builddir)/VLC.app/Contents/MacOS/lib/libvlc.dylib
	$(INSTALL) $(top_builddir)/src/.libs/libvlc-control.1.dylib \
		   $(top_builddir)/VLC.app/Contents/MacOS/lib/libvlc-control.dylib
	install_name_tool -change /usr/local/lib/libvlc.1.dylib \
		   @executable_path/lib/libvlc.dylib \
		   $(top_builddir)/VLC.app/Contents/MacOS/lib/libvlc-control.dylib
	install_name_tool -change /usr/local/lib/libvlc.1.dylib \
		   @executable_path/lib/libvlc.dylib \
	       $(top_builddir)/VLC.app/Contents/MacOS/VLC
	ln -sf ./VLC $(top_builddir)/VLC.app/Contents/MacOS/clivlc
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS/modules
	for i in `$(VLC_CONFIG) --target plugin` ; do \
	  if test -n "$$i" ; \
        then install_name_tool -change /usr/local/lib/libvlc.1.dylib \
                                  @executable_path/lib/libvlc.dylib \
                          "`pwd`/`dirname $$i`/.libs/`basename $$i`$(LIBEXT)"; \
	    ln -sfn "`pwd`/`dirname $$i`/.libs/`basename $$i`$(LIBEXT)" \
			   "$(top_builddir)/VLC.app/Contents/MacOS/modules" ; \
	  fi ; done
	if test -d $(srcdir)/extras/contrib/vlc-lib; then \
	  mkdir -p $(top_builddir)/VLC.app/Contents/MacOS/lib ; \
	  for i in $(srcdir)/extras/contrib/vlc-lib/*.dylib ; do \
		install_name_tool -change /usr/local/lib/libvlc.1.dylib @executable_path/lib/libvlc.dylib $${i}; \
	    ln -sfn `pwd`/$${i} $(top_builddir)/VLC.app/Contents/MacOS/lib/`basename $${i}` ; \
	  done ; \
	fi
	ln -sfn `pwd`/$(srcdir)/share $(top_builddir)/VLC.app/Contents/MacOS/
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS/locale
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  mkdir -p $(top_builddir)/VLC.app/Contents/MacOS/locale/$${i}/LC_MESSAGES ; \
	  ln -sfn `pwd`/po/$${i}.gmo $(top_builddir)/VLC.app/Contents/MacOS/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	  mkdir -p $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/InfoPlist.strings \
	      $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/MainMenu.nib \
	      $(top_builddir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	done
	printf "APPLVLC#" >| $(top_builddir)/VLC.app/Contents/PkgInfo
endif

if HAVE_WIN32
DATA_win32_rc = $(noinst_share_vlc_win32_rc_DATA)
noinst_share_vlc_win32_rc_DATA = share/vlc_win32_rc.$(OBJEXT)
noinst_share_vlc_win32_rcdir = $(libdir)
share/vlc_win32_rc.$(OBJEXT): share/vlc_win32_rc.rc
	$(WINDRES) -DVERSION=$(VERSION) -DVERSION_NUMBER=`echo $(VERSION).0.0.0 | sed 's/\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\).*/\1,\2,\3,\4/'` --include-dir $(srcdir)/share -i $< -o $@
endif


###############################################################################
# Building architecture-specific binary packages
###############################################################################

# XXX: this rule is probably only useful to you if you have exactly
# the same setup as the maintaner(s).
#

package-win-common:
# Check that tmp isn't in the way
	@if test -e "$(top_builddir)/vlc-${VERSION}"; then \
	  echo "Error: please remove $(top_builddir)/vlc-${VERSION}, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(top_builddir)/vlc-${VERSION}"; \
	fi

# Copy relevant files
	cp "$(top_builddir)/.libs/vlc$(EXEEXT)" "$(top_builddir)/vlc-${VERSION}/"
	cp "$(top_srcdir)/vlc.exe.manifest" "$(top_builddir)/vlc-${VERSION}/"
	cp "$(top_builddir)/src/.libs/libvlc$(LIBEXT)" "vlc-${VERSION}/"
	cp "$(top_builddir)/src/.libs/libvlc-control$(LIBEXT)" "vlc-${VERSION}/"

	for file in AUTHORS MAINTAINERS THANKS ; \
	  do sed 's/@/_AT_/' < "$(srcdir)/$$file" > "$(top_builddir)/vlc-${VERSION}/$${file}.txt" ; done;
	for file in NEWS COPYING README; \
	   do cp "$(srcdir)/$$file" "$(top_builddir)/vlc-${VERSION}/$${file}.txt"; done
	  unix2dos "$(top_builddir)/vlc-${VERSION}/"*.txt

	mkdir $(top_builddir)/vlc-${VERSION}/locale
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  mkdir -p "$(top_builddir)/vlc-${VERSION}/locale/$${i}/LC_MESSAGES" ; \
	  cp "$(srcdir)/po/$${i}.gmo" \
	    "$(top_builddir)/vlc-${VERSION}/locale/$${i}/LC_MESSAGES/vlc.mo" \
            || true ; \
	done

	mkdir -p "$(top_builddir)/vlc-${VERSION}/http-lua/images"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http-lua/requests"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http-lua/js"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http-lua/dialogs"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http-lua/old"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http-lua/old/vlm"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http-lua/old/admin"
	cp $(srcdir)/share/http-lua/*.html $(top_builddir)/vlc-${VERSION}/http-lua/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/*.html ;
	cp $(srcdir)/share/http-lua/.hosts $(top_builddir)/vlc-${VERSION}/http-lua/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/.hosts ;
	cp $(srcdir)/share/http-lua/*.css $(top_builddir)/vlc-${VERSION}/http-lua/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/*.css ;
	cp $(srcdir)/share/http-lua/js/*.js $(top_builddir)/vlc-${VERSION}/http-lua/js/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/js/*.js ;
	cp $(srcdir)/share/http-lua/dialogs/* $(top_builddir)/vlc-${VERSION}/http-lua/dialogs/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/dialogs/* ;
	cp $(srcdir)/share/http-lua/dialogs/.hosts $(top_builddir)/vlc-${VERSION}/http-lua/dialogs/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/dialogs/.hosts ;
	cp $(srcdir)/share/http-lua/*.ico $(top_builddir)/vlc-${VERSION}/http-lua/ ;
	cp $(srcdir)/share/http-lua/images/*.png $(top_builddir)/vlc-${VERSION}/http-lua/images/
	cp $(srcdir)/share/http-lua/requests/*.xml $(top_builddir)/vlc-${VERSION}/http-lua/requests/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/requests/*.xml ;
	cp $(srcdir)/share/http-lua/requests/readme $(top_builddir)/vlc-${VERSION}/http-lua/requests/readme.txt ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/requests/readme.txt ;

	cp $(srcdir)/share/http-lua/old/*.html $(top_builddir)/vlc-${VERSION}/http-lua/old/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/old/*.html ;
	cp $(srcdir)/share/http-lua/old/*.css $(top_builddir)/vlc-${VERSION}/http-lua/old/ ;
	cp $(srcdir)/share/http-lua/old/.hosts $(top_builddir)/vlc-${VERSION}/http-lua/old/ ;
	cp $(srcdir)/share/http-lua/old/*.png $(top_builddir)/vlc-${VERSION}/http-lua/old/ ;
	cp $(srcdir)/share/http-lua/old/vlm/*.html $(top_builddir)/vlc-${VERSION}/http-lua/old/vlm/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/old/vlm/*.html ;
	cp $(srcdir)/share/http-lua/old/admin/*.html $(top_builddir)/vlc-${VERSION}/http-lua/old/admin/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http-lua/old/admin/*.html ;
	cp $(srcdir)/share/http-lua/old/admin/.access $(top_builddir)/vlc-${VERSION}/http-lua/old/admin/ ;

	mkdir -p "$(top_builddir)/vlc-${VERSION}/http/images"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http/requests"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http/js"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http/dialogs"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http/old"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http/old/vlm"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http/old/admin"
	cp $(srcdir)/share/http/*.html $(top_builddir)/vlc-${VERSION}/http/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/*.html ;
	cp $(srcdir)/share/http/.hosts $(top_builddir)/vlc-${VERSION}/http/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/.hosts ;
	cp $(srcdir)/share/http/*.css $(top_builddir)/vlc-${VERSION}/http/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/*.css ;
	cp $(srcdir)/share/http/js/*.js $(top_builddir)/vlc-${VERSION}/http/js/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/js/*.js ;
	cp $(srcdir)/share/http/dialogs/* $(top_builddir)/vlc-${VERSION}/http/dialogs/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/dialogs/* ;
	cp $(srcdir)/share/http/dialogs/.hosts $(top_builddir)/vlc-${VERSION}/http/dialogs/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/dialogs/.hosts ;
	cp $(srcdir)/share/http/*.ico $(top_builddir)/vlc-${VERSION}/http/ ;
	cp $(srcdir)/share/http/images/*.png $(top_builddir)/vlc-${VERSION}/http/images/
	cp $(srcdir)/share/http/requests/*.xml $(top_builddir)/vlc-${VERSION}/http/requests/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/requests/*.xml ;
	cp $(srcdir)/share/http/requests/readme $(top_builddir)/vlc-${VERSION}/http/requests/readme.txt ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/requests/readme.txt ;

	cp $(srcdir)/share/http/old/*.html $(top_builddir)/vlc-${VERSION}/http/old/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/old/*.html ;
	cp $(srcdir)/share/http/old/*.css $(top_builddir)/vlc-${VERSION}/http/old/ ;
	cp $(srcdir)/share/http/old/.hosts $(top_builddir)/vlc-${VERSION}/http/old/ ;
	cp $(srcdir)/share/http/old/*.png $(top_builddir)/vlc-${VERSION}/http/old/ ;
	cp $(srcdir)/share/http/old/vlm/*.html $(top_builddir)/vlc-${VERSION}/http/old/vlm/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/old/vlm/*.html ;
	cp $(srcdir)/share/http/old/admin/*.html $(top_builddir)/vlc-${VERSION}/http/old/admin/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/old/admin/*.html ;
	cp $(srcdir)/share/http/old/admin/.access $(top_builddir)/vlc-${VERSION}/http/old/admin/ ;

	cp $(srcdir)/share/vlc48x48.ico $(top_builddir)/vlc-${VERSION}/ ;

if BUILD_MOZILLA
	mkdir -p "$(top_builddir)/vlc-${VERSION}/mozilla"
	cp $(top_builddir)/mozilla/.libs/npvlc$(LIBEXT) $(top_builddir)/vlc-${VERSION}/mozilla/ ;
endif

if BUILD_ACTIVEX
	mkdir -p "$(top_builddir)/vlc-${VERSION}/activex"
	cp $(srcdir)/activex/README.TXT  $(top_builddir)/vlc-${VERSION}/activex/ ;
	cp $(srcdir)/activex/test.html  $(top_builddir)/vlc-${VERSION}/activex/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/activex/* ;

	cp $(top_builddir)/activex/.libs/axvlc$(LIBEXT) $(top_builddir)/vlc-${VERSION}/activex/ ;
endif
# Rebase all those DLLs to speed up loading (need cygwin rebase)
	if rebase -b 0x42 /dev/null >/dev/null 2>&1; then \
		find $(top_builddir)/vlc-${VERSION} -type f -name '*.dll' -print | rebase -b 0x70000000 -T -; \
	fi

package-win-common-strip:
	$(STRIP) "$(top_builddir)/vlc-${VERSION}/vlc$(EXEEXT)"
	$(STRIP) "vlc-${VERSION}/libvlc$(LIBEXT)"
	$(STRIP) "vlc-${VERSION}/libvlc-control$(LIBEXT)"
if BUILD_MOZILLA
	$(STRIP) $(top_builddir)/vlc-${VERSION}/mozilla/npvlc$(LIBEXT);
endif
if BUILD_ACTIVEX
	$(STRIP) $(top_builddir)/vlc-${VERSION}/activex/axvlc$(LIBEXT);
endif

package-win32-base-debug: package-win-common
# Copy relevant files
	cp "$(top_builddir)/vlc.win32.nsi" "$(top_builddir)/vlc-${VERSION}/"

	mkdir -p "$(top_builddir)/vlc-${VERSION}/plugins"
	for i in "" `$(VLC_CONFIG) --target plugin` ; do \
	  if test -n "$$i" ; then \
	    $(INSTALL) "$(top_builddir)/`dirname $$i`/.libs/`basename $$i`$(LIBEXT)" \
            "$(top_builddir)/vlc-${VERSION}/plugins/" ; \
	  fi ; done

	mkdir -p $(top_builddir)/vlc-${VERSION}/skins/fonts
	for i in $(srcdir)/share/skins2/fonts/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/skins/fonts/ || true ; \
	done
	for i in $(srcdir)/share/skins2/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/skins/ || true ; \
	done

	$(INSTALL) -d $(top_builddir)/vlc-${VERSION}/share/luaplaylist
	for i in $(srcdir)/share/luaplaylist/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/vlc-${VERSION}/share/luaplaylist/`basename $${i}` ; \
	done ;
	$(INSTALL) -d $(top_builddir)/vlc-${VERSION}/share/luameta
	for i in $(srcdir)/share/luameta/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/vlc-${VERSION}/share/luameta/`basename $${i}` ; \
	done ;
	$(INSTALL) -d $(top_builddir)/vlc-${VERSION}/share/luaintf
	for i in $(srcdir)/share/luaintf/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/vlc-${VERSION}/share/luaintf/`basename $${i}` ; \
	done ;
	$(INSTALL) -d $(top_builddir)/vlc-${VERSION}/share/luaintf/modules
	for i in $(srcdir)/share/luaintf/modules/*.* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/vlc-${VERSION}/share/luaintf/modules/`basename $${i}` ; \
	done ;

	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu"
	cp $(srcdir)/share/osdmenu/*.* "$(top_builddir)/vlc-${VERSION}/osdmenu"
	for dir in dvd dvd/selected dvd/unselect dvd/selection dvd/volume default default/selected default/selection default/volume minimal;do \
		mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu/$$dir"; \
		for file in $(srcdir)/share/osdmenu/$${dir}/*.*;do \
			 cp $$file "$(top_builddir)/vlc-${VERSION}/osdmenu/$$dir" || true; \
		done; \
	done
	unix2dos $(top_builddir)/vlc-${VERSION}/osdmenu/*.cfg;
	for file in $(top_builddir)/vlc-${VERSION}/osdmenu/*.cfg; do \
		if sed 's%share/osdmenu%osdmenu%g' $$file >$$file.tmp; then :; else exit 1; fi; \
		if sed 's%/%\\%g' $$file.tmp >$$file; then :; else exit 1; fi; \
		rm -f $$file.tmp; \
	done


package-win32-base: package-win32-base-debug package-win-common-strip
	for i in "" $(top_builddir)/vlc-${VERSION}/plugins/*$(LIBEXT) ; \
	  do if test -n "$$i" ; then $(STRIP) "$$i" ; fi ; done

package-win32-base-exe: package-win32-base
# Create package
	if makensis -VERSION >/dev/null 2>&1; then \
	    MAKENSIS="makensis -DVERSION=${VERSION}"; \
	elif [ -x "/cygdrive/c/Program Files/NSIS/makensis" ]; then \
	    MAKENSIS="/cygdrive/c/Program\ Files/NSIS/makensis /DVERSION=${VERSION}"; \
	elif [ -x "${PROGRAMFILES}/NSIS/makensis" ]; then \
	    MAKENSIS="${PROGRAMFILES}/NSIS/makensis -DVERSION=${VERSION}"; \
	elif wine --version >/dev/null 2>&1; then \
	    MAKENSIS="wine C:/Program\ Files/NSIS/makensis.exe /DVERSION=${VERSION}"; \
	else \
	    echo 'Error: cannot locate makensis tool'; exit 1; \
	fi; \
	eval "$$MAKENSIS $(top_builddir)/vlc-${VERSION}/vlc.win32.nsi"

package-win32-base-exe-cygwin: package-win32-base-exe
# Create package

package-win32-base-exe-linux:
# Create package
	makensis -DVERSION=${VERSION} $(top_builddir)/vlc-${VERSION}/vlc.win32.nsi

package-win32-base-zip: package-win32-base
# Create package 
	zip -r -9 vlc-${VERSION}-win32.zip vlc-${VERSION}

package-win32-base-7zip: package-win32-base
# Create package 
	7z a -t7z -m0=lzma -mx=9 -mfb=64 -md=32m -ms=on vlc-${VERSION}-win32.7z vlc-${VERSION}

package-win32-exe: package-win32-base package-win32-base-exe
# Clean up
	rm -Rf $(top_builddir)/vlc-${VERSION}

package-win32-exe-linux: package-win32-base package-win32-base-exe-linux
# Clean up
	rm -Rf $(top_builddir)/vlc-${VERSION}

package-win32-zip: package-win32-base-zip
# Clean up
	rm -Rf $(top_builddir)/vlc-${VERSION}

package-win32-7zip: package-win32-base-7zip
# Clean up
	rm -Rf $(top_builddir)/vlc-${VERSION}

package-win32-no-clean: package-win32-base-exe package-win32-base-zip package-win32-base-7zip

package-win32: package-win32-no-clean
# Clean up
	rm -Rf $(top_builddir)/vlc-${VERSION}

package-wince-base: package-win-common package-win-common-strip

package-wince-base-zip:
# Create package 
	zip -r vlc-${VERSION}-wince.zip vlc-${VERSION}

package-wince: package-wince-base  package-wince-base-zip
# Clean up
	rm -Rf $(top_builddir)/vlc-${VERSION}


package-beos:
# Check that tmp isn't in the way
	@if test -e $(srcdir)/tmp; then \
	  echo "Error: please remove $(srcdir)/tmp, it is in the way"; \
	  false ; \
	else \
	  echo "OK." ; mkdir $(srcdir)/tmp ; \
	fi

# Copy relevant files
	mkdir -p $(srcdir)/tmp/vlc ;
	cd $(srcdir) && cp -R vlc-bundle/* AUTHORS COPYING ChangeLog README \
	  THANKS NEWS tmp/vlc/ ;

# Create debug package
	xres -o $(srcdir)/tmp/vlc/vlc $(srcdir)/share/vlc_beos.rsrc ;
	find $(srcdir)/tmp/vlc -exec mimeset -f {} \; ;
	mv $(srcdir)/tmp/vlc $(srcdir)/tmp/vlc-${VERSION} ;
	(cd $(srcdir)/tmp && zip -9 -r vlc-${VERSION}-BeOS-debug.zip vlc-${VERSION} )
	mv $(srcdir)/tmp/vlc-${VERSION}-BeOS-debug.zip $(srcdir)/ ;
	mv $(srcdir)/tmp/vlc-${VERSION} $(srcdir)/tmp/vlc ;

# Create normal package
	$(STRIP) --strip-debug --strip-unneeded $(srcdir)/tmp/vlc/vlc ;
	find $(srcdir)/tmp/vlc -name 'lib*.so' -exec $(STRIP) \
	  --strip-debug --strip-unneeded "{}" \; ;
	xres -o $(srcdir)/tmp/vlc/vlc $(srcdir)/share/vlc_beos.rsrc ;
	find $(srcdir)/tmp/vlc -exec mimeset -f {} \; ;
	mv $(srcdir)/tmp/vlc $(srcdir)/tmp/vlc-${VERSION} ;
	(cd $(srcdir)/tmp &&  zip -9 -r vlc-${VERSION}-BeOS.zip vlc-${VERSION} )
	mv $(srcdir)/tmp/vlc-${VERSION}-BeOS.zip $(srcdir)/ ;

# Clean up
	rm -Rf $(srcdir)/tmp ;

package-macosx:
# Check that the temporary location isn't in the way
	@if test -e "$(top_builddir)/vlc-${VERSION}/"; then \
	  rm -Rf "$(top_builddir)/vlc-${VERSION}/" ; \
	fi

	echo "Create package directory: vlc-${VERSION}/";
	mkdir -p "$(top_builddir)/vlc-${VERSION}/";

# Copy relevant files 
	@if test -e "$(top_builddir)/VLC-release.app/"; then \
	  cp -R "$(top_builddir)/VLC-release.app" "$(top_builddir)/vlc-${VERSION}/VLC.app"; \
	else \
	  cp -R "$(top_builddir)/VLC.app" "$(top_builddir)/vlc-${VERSION}/VLC.app"; \
	fi
	cd "$(srcdir)" && mkdir -p $(top_builddir)/vlc-${VERSION}/Goodies/ && \
          mkdir -p $(top_builddir)/vlc-${VERSION}/.background/ && \
          cp AUTHORS COPYING ChangeLog README THANKS NEWS $(top_builddir)/vlc-${VERSION}/Goodies/ && \
          cp -R extras/MacOSX/Delete_Preferences.app $(top_builddir)/vlc-${VERSION}/Goodies/Delete\ VLC\ Preferences.app && \
	  cp README.MacOSX.rtf $(top_builddir)/vlc-${VERSION}/Read\ Me.rtf && \
	  cp extras/MacOSX/Resources/about_bg.png $(top_builddir)/vlc-${VERSION}/.background/background.png ;

# Place a link to the application folder
	ln -s /Applications $(top_builddir)/vlc-${VERSION}/Applications ;

# Create disk image 
	echo "Creating disk image" ;
	rm -f "$(top_builddir)/vlc-${VERSION}.dmg" ;
	hdiutil create -verbose -srcfolder "$(top_builddir)/vlc-${VERSION}" \
	  "$(top_builddir)/vlc-${VERSION}.dmg" -format UDRW \
	  -scrub -imagekey zlib-level=9 ;

# Make sure the root window of the dmg will pop up when the dmg is mounted.
# Note: We dont mount in /Volumes to be sure we won't collide with an other
# finder mounted dmg with the same name.
	echo "Make sure the root window of the dmg will pop up when the dmg is mounted" ;
	mkdir -p $(top_builddir)/vlcmnt ;
	hdiutil attach -kernel -readwrite -noverify -noautoopen "$(top_builddir)/vlc-${VERSION}.dmg" -mountpoint "$(top_builddir)/vlcmnt/vlc-${VERSION}" ;
	bless --folder "$(top_builddir)/vlcmnt/vlc-${VERSION}/" --openfolder "$(top_builddir)/vlcmnt/vlc-${VERSION}" ;
	sleep 1 # Make sure operation completes
	cd "$(srcdir)"

# Unmount the image now
	hdiutil detach "$(top_builddir)/vlcmnt/vlc-${VERSION}" ;
	rm -R $(top_builddir)/vlcmnt ;

# Make sure the image is not writable
# Note: We can't directly create a read only dmg as we do the bless stuff
	echo "Make the disk image read-only" ;
	mv "$(top_builddir)/vlc-${VERSION}.dmg" "$(top_builddir)/vlc-${VERSION}-rw.dmg" ;
	hdiutil convert "$(top_builddir)/vlc-${VERSION}-rw.dmg" -format UDZO -o "$(top_builddir)/vlc-${VERSION}.dmg" ;
	rm "$(top_builddir)/vlc-${VERSION}-rw.dmg" ;

# We are done
	echo "Disk image creation completed:" ;
	ls -la "$(top_builddir)/vlc-${VERSION}.dmg" ; echo ;

# Clean up
	rm -Rf "$(top_builddir)/vlc-${VERSION}" ;

package-macosx-plugin:
if BUILD_MOZILLA
# Create Installer
	rm -rf "$(top_builddir)/macosx-plugin-instdata/"; true
	mkdir -p "$(top_builddir)/macosx-plugin-instdata"
	cp -R "$(top_builddir)/mozilla/VLC Plugin.plugin" "$(top_builddir)/macosx-plugin-instdata"
	rm -rf "$(top_builddir)/VLC Internet Plug-In.pkg"
	PATH=/Developer/usr/bin:/Developer/Tools:$$PATH packagemaker -build -ds -p "$(top_builddir)/VLC Internet Plug-In.pkg" \
	-f "$(top_builddir)/macosx-plugin-instdata" \
	-i "$(srcdir)/extras/MacOSX/plugin/InstallerInfo.plist" \
	-d "$(srcdir)/extras/MacOSX/plugin/InstallerDescription.plist"; true
	rm -rf "$(top_builddir)/macosx-plugin-instdata/"

# Create disk image 
	rm -f "$(top_builddir)/vlc-plugin-${VERSION}.dmg"; true
	rm -rf "$(top_builddir)/vlc-plugin-${VERSION}/"; true
	mkdir -p "$(top_builddir)/vlc-plugin-${VERSION}/"
	mv "$(top_builddir)/VLC Internet Plug-In.pkg" "$(top_builddir)/vlc-plugin-${VERSION}/"
	hdiutil create -srcfolder "$(top_builddir)/vlc-plugin-${VERSION}" \
	  "$(top_builddir)/vlc-plugin-${VERSION}.dmg" -format UDZO \
	  -scrub -imagekey zlib-level=9 ;
	echo "Disk image creation completed:"
	rm -rf "$(top_builddir)/vlc-plugin-${VERSION}"
endif

package-translations:
	@if test -e "$(srcdir)/vlc-translations-${VERSION}"; then \
	  echo "Error: please remove $(srcdir)/vlc-translations-${VERSION}, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(srcdir)/vlc-translations-${VERSION}"; \
	fi
# Copy translations
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  cp "$(srcdir)/po/$${i}.po" \
	    "$(srcdir)/vlc-translations-${VERSION}/$${i}.po" \
	    || true ; \
	done
	cp "$(srcdir)/doc/translations.txt" \
	  "$(srcdir)/vlc-translations-${VERSION}/README.txt"

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
# Generate po/POTFILES.in which lists all the files containing translatable
# strings. Find all source files and remove the generated files
###############################################################################
.PHONY: update-po

update-po:
	rm -f $(top_srcdir)/po/POTFILES.in
	{ \
	  cd $(top_srcdir) && \
	  echo "# automatically created by make update-po" ; \
	  echo "" ; \
	  echo "# main sources" ; \
	  find include src -name '*.[chm]' -o -name '*.[ch]pp' \
	    | grep -v '\(misc/modules_\|src/misc/revision.c\|modules/builtin.h\|include/vlc_about.h\)' \
	    | sort ; \
	  echo "" ; \
	  echo "# modules" ; \
	  find modules -name '*.[chm]' -o -name '*.[ch]pp' -o -name '*.ui' \
	    | grep -v '\(\.moc\.\|gui/gtk2/\)' \
	    | grep -vE 'qt4/ui/.*.h' \
	    | grep -v 'qt4/resources.cpp\|qt4/dialogs/about.hpp' \
	    | grep -v 'gui/opie' \
	    | sort ; \
	} > $(top_srcdir)/po/POTFILES.in
	rm -f $(top_srcdir)/po/vlc.pot
	cd po && $(MAKE) POTFILES vlc.pot update-po

#cd po && $(MAKE) update-po

###############################################################################
# Stamp rules
###############################################################################
stamp-builtin:
	@for dep in "" `$(VLC_CONFIG) --target plugin`; do \
	  if test "$${dep}.la" -nt "$(top_builddir)/vlc$(EXEEXT)"; then \
	    touch "$@"; \
	    break; \
	  fi; \
	done

.PHONY: stamp-builtin

###############################################################################
# Enforce Mac OS X deployment target environment variable
###############################################################################
macosx-sdk: Makefile.in $(HEADERS_include) vlc-config
	export MACOSX_DEPLOYMENT_TARGET=$(MACOSX_DEPLOYMENT_TARGET)
