###############################################################################
# Automake targets and declarations
###############################################################################

NULL =

# SUBDIRS stores the directories where a "make" is required when building
# something. DIST_SUBDIRS stores the directories where nothing is built but
# which have makefiles with distribution information.
#  - intl should come before modules and . because all the code uses gettext
#  - modules should come before . because vlc needs the builtins
#  - . should come before mozilla because the plugin needs libvlc_pic.a
#  - po should come before . because VLC.app needs the pofiles
#  - loader should come before modules because some plugins need it
SUBDIRS = intl loader modules po . mozilla activex share m4 doc
DIST_SUBDIRS = $(SUBDIRS) debian ipkg lib

EXTRA_DIST = \
	HACKING \
	INSTALL.win32 \
	vlc.exe.manifest \
	MAINTAINERS \
	README.MacOSX.rtf \
	bootstrap \
	src/extras/COPYING \
	toolbox \
	vlc-api.pl \
	vlc-config.in.in \
	vlc.spec \
	vlc.spec.mdk \
	vlc.win32.nsi \
	src/misc/modules_builtin.h.in \
	$(NULL)

BUILT_SOURCES_distclean = vlc-config
BUILT_SOURCES_clean = \
	stamp-api \
	src/misc/modules_builtin.h \
	src/misc/version.c \
	$(NULL)

BUILT_SOURCES = $(BUILT_SOURCES_distclean) $(BUILT_SOURCES_clean)

SUFFIXES = 

# Tell aclocal to use -I m4. Wonder if it really works.
ACLOCAL_AMFLAGS = -I m4

# XXX: these flags could be set in configure.ac, but we set them here
# because old versions of automake don't support them in configure.ac.
AUTOMAKE_OPTIONS = dist-bzip2 subdir-objects

###############################################################################
# Headers
###############################################################################

pkgincludedir = $(includedir)/vlc

dist_pkginclude_HEADERS = \
	include/vlc/vlc.h \
	include/vlc/aout.h \
	include/vlc/vout.h \
	include/vlc/sout.h \
	include/vlc/decoder.h \
	include/vlc/input.h \
	include/vlc/intf.h \
	include/vlc/control.h \
	$(NULL)

noinst_HEADERS = $(HEADERS_include)
noinst_DATA = $(DATA_noinst_beos) $(DATA_noinst_libvlc)

HEADERS_include = \
	include/aout_internal.h \
	include/audio_output.h \
	include/beos_specific.h \
	include/charset.h \
	include/codecs.h \
	include/configuration.h \
	include/darwin_specific.h \
	include/intf_eject.h \
	include/iso_lang.h \
	include/main.h \
	include/mmx.h \
	include/modules.h \
	include/modules_inner.h \
	include/mtime.h \
	include/network.h \
	include/osd.h \
	include/os_specific.h \
	include/snapshot.h \
	include/stream_output.h \
	include/variables.h \
	include/video_output.h \
	include/vlc_access.h \
	include/vlc_acl.h \
	include/vlc_bits.h \
	include/vlc_block.h \
	include/vlc_block_helper.h \
	include/vlc_codec.h \
	include/vlc_common.h \
	include/vlc_config.h \
	include/vlc_cpu.h \
	include/vlc_demux.h \
	include/vlc_error.h \
	include/vlc_es.h \
	include/vlc_es_out.h \
	include/vlc_filter.h \
	include/vlc_config_cat.h \
	include/vlc_httpd.h \
	include/vlc_tls.h \
	include/vlc_md5.h \
	include/vlc_image.h \
	include/vlc_input.h \
	include/vlc_interface.h \
	include/vlc_keys.h \
	include/vlc_messages.h \
	include/vlc_meta.h \
	include/vlc_objects.h \
	include/vlc_osd.h \
	include/vlc_playlist.h \
	include/vlc_spu.h \
	include/vlc_stream.h \
	include/vlc_symbols.h \
	include/vlc_threads_funcs.h \
	include/vlc_threads.h \
	include/vlc_video.h \
	include/vlc_vlm.h \
	include/vlc_vod.h \
	include/vlc_xml.h \
	include/vout_synchro.h \
	include/win32_specific.h \
	$(NULL)

src/misc/modules_builtin.h: Makefile src/misc/modules_builtin.h.in vlc-config
	srcdir=$(srcdir) $(srcdir)/toolbox --update-includes
	touch src/misc/modules_builtin.h

src/misc/modules.c: src/misc/modules_builtin.h

src/misc/version.c: FORCE
	srcdir=$(srcdir) $(srcdir)/toolbox --update-version

# These dependencies are mandatory
$(SOURCES_libvlc): $(LIB_intl)

###############################################################################
# Optional libintl - FIXME, bad dependencies
###############################################################################

intl/libintl.a: FORCE
	cd $(top_builddir)/intl && $(MAKE) $(AM_MAKEFLAGS)

if BUILD_INTL
LIB_intl = intl/libintl.a
endif

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
	extras/MacOSX/Resources/English.lproj/Update.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/Update.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/Update.nib/keyedobjects.nib \
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
	extras/MacOSX/Resources/vlc.scriptSuite \
	extras/MacOSX/Resources/vlc.scriptTerminology \
	extras/MacOSX/Resources/README \
	extras/MacOSX/vlc.pbproj/project.pbxproj \
	extras/MacOSX/macosx-dmg \
	\
	extras/Makefile \
	extras/zsh.cpp \
	$(NULL)

###############################################################################
# MS Visual Studio and eMbedded Visual Studio projects
###############################################################################

EXTRA_DIST += \
	msvc/vlc.dsw \
	msvc/libvlc.dsp.in \
	msvc/plugins.dsp.in \
	msvc/vlc.dsp.in \
	msvc/config.h.in \
	msvc/modules_builtin_msvc.h \
	evc/vlc.vcw \
	evc/libvlc.vcp.in \
	evc/vlc.vcp.in \
	evc/plugins.vcp.in \
	evc/errno.h \
	evc/config.h.in \
	evc/modules_builtin_evc.h \
	$(NULL)

dist-hook:
	cd $(distdir) && srcdir=. $(SHELL) ./toolbox --update-vc
	distdir=$(distdir) srcdir=$(srcdir) $(SHELL) $(srcdir)/toolbox --dist-contrib

###############################################################################
# Building libvlc
###############################################################################

bin_SCRIPTS = vlc-config
vlc-config: $(top_builddir)/config.status $(top_builddir)/vlc-config.in
	$(SHELL) ./config.status --file=vlc-config
	chmod 0755 vlc-config

vlc-config.in: vlc-config.in.in
	./config.status --recheck

MOSTLYCLEANFILES = $(DATA_noinst_libvlc)
CLEANFILES = $(BUILT_SOURCES_clean) stamp-builtin
DISTCLEANFILES = $(BUILT_SOURCES_distclean) vlc-config.in

if HAVE_WIN32
lib_LIBRARIES = lib/libvlc.a
else
if BUILD_SHARED
lib_LIBRARIES = lib/libvlc_pic.a
else
lib_LIBRARIES = lib/libvlc.a
if BUILD_MOZILLA
lib_LIBRARIES += lib/libvlc_pic.a
endif
endif
endif

lib_libvlc_a_SOURCES = $(SOURCES_libvlc)
lib_libvlc_a_CFLAGS = `$(VLC_CONFIG) --cflags vlc`
lib_libvlc_a_CXXFLAGS = `$(VLC_CONFIG) --cxxflags vlc`
lib_libvlc_a_OBJCFLAGS = `$(VLC_CONFIG) --objcflags vlc`

lib_libvlc_pic_a_SOURCES = $(SOURCES_libvlc)
lib_libvlc_pic_a_CFLAGS = `$(VLC_CONFIG) --cflags vlc pic`
lib_libvlc_pic_a_CXXFLAGS = `$(VLC_CONFIG) --cxxflags vlc pic`
lib_libvlc_pic_a_OBJCFLAGS = `$(VLC_CONFIG) --objcflags vlc pic`

if HAVE_BEOS
OPT_SOURCES_libvlc_beos = $(SOURCES_libvlc_beos)
endif
if HAVE_DARWIN
OPT_SOURCES_libvlc_darwin = $(SOURCES_libvlc_darwin)
endif
if HAVE_WIN32
OPT_SOURCES_libvlc_win32 = $(SOURCES_libvlc_win32)
endif
if HAVE_WINCE
OPT_SOURCES_libvlc_win32 = $(SOURCES_libvlc_win32)
endif
if BUILD_DIRENT
OPT_SOURCES_libvlc_dirent = $(SOURCES_libvlc_dirent)
endif
if BUILD_GETOPT
OPT_SOURCES_libvlc_getopt = $(SOURCES_libvlc_getopt)
endif

# Build libvlc as a shared library
if BUILD_SHARED
DATA_noinst_libvlc = libvlc$(LIBEXT)
if HAVE_WIN32
OBJECTS_libvlc_so = $(lib_libvlc_a_OBJECTS)
else
OBJECTS_libvlc_so = $(lib_libvlc_pic_a_OBJECTS)
endif
endif

libvlc$(LIBEXT): $(OBJECTS_libvlc_so)
	@ldfl="`$(VLC_CONFIG) --libs plugin vlc $(pic) builtin`" ; \
	case `$(VLC_CONFIG) --linkage vlc` in \
	  c++)  ld="$(CXXLINK)" ;; \
	  objc) ld="$(OBJCLINK)" ;; \
	  c|*)  ld="$(LINK)" ;; \
	esac ; \
	echo $$ld $(OBJECTS_libvlc_so) $$ldfl ; \
	$$ld $(OBJECTS_libvlc_so) $$ldfl

EXTRA_DIST += \
	$(SOURCES_libvlc_beos) \
	$(SOURCES_libvlc_darwin) \
	$(SOURCES_libvlc_win32) \
	$(SOURCES_libvlc_dirent) \
	$(SOURCES_libvlc_getopt) \
	$(NULL)

SOURCES_libvlc_beos = \
	src/misc/beos_specific.cpp \
	$(NULL)

SOURCES_libvlc_darwin = \
	src/misc/darwin_specific.m \
	$(NULL)

SOURCES_libvlc_win32 = \
	src/misc/win32_specific.c \
	$(NULL)

SOURCES_libvlc_dirent = \
	src/extras/dirent.c \
	$(NULL)

SOURCES_libvlc_getopt = \
	src/extras/getopt.c \
	src/extras/getopt.h \
	src/extras/getopt1.c \
	$(NULL)

SOURCES_libvlc_common = \
	src/libvlc.c \
	src/libvlc.h \
	src/interface/interface.c \
	src/interface/intf_eject.c \
	src/playlist/playlist.c \
	src/playlist/sort.c \
	src/playlist/loadsave.c \
	src/playlist/view.c \
	src/playlist/item.c \
	src/playlist/item-ext.c \
	src/playlist/services_discovery.c \
	src/input/access.c \
	src/input/clock.c \
	src/input/control.c \
	src/input/decoder.c \
	src/input/demux.c \
	src/input/es_out.c \
	src/input/input.c \
	src/input/input_internal.h \
	src/input/stream.c \
	src/input/mem_stream.c \
	src/input/subtitles.c \
	src/input/var.c \
	src/video_output/video_output.c \
	src/video_output/vout_pictures.c \
	src/video_output/vout_pictures.h \
	src/video_output/video_text.c \
	src/video_output/video_widgets.c \
	src/video_output/vout_subpictures.c \
	src/video_output/vout_synchro.c \
	src/video_output/vout_intf.c \
	src/audio_output/common.c \
	src/audio_output/dec.c \
	src/audio_output/filters.c \
	src/audio_output/input.c \
	src/audio_output/mixer.c \
	src/audio_output/output.c \
	src/audio_output/intf.c \
	src/stream_output/stream_output.c \
	src/stream_output/announce.c \
	src/stream_output/sap.c \
	src/stream_output/acl.c \
	src/osd/osd.c \
	src/osd/osd_widgets.c \
	src/osd/osd_parser.c \
	src/misc/charset.c \
	src/misc/httpd.c \
	src/misc/tls.c \
	src/misc/md5.c \
	src/misc/mtime.c \
	src/misc/block.c \
	src/misc/modules.c \
	src/misc/threads.c \
	src/misc/unicode.c \
	src/misc/cpu.c \
	src/misc/configuration.c \
	src/misc/image.c \
	src/misc/iso_lang.c \
	src/misc/iso-639_def.h \
	src/misc/messages.c \
	src/misc/objects.c \
	src/misc/variables.c \
	src/misc/error.c \
	src/misc/net.c \
	src/misc/getaddrinfo.c \
	src/misc/vlm.c \
	src/misc/xml.c \
	src/misc/version.c \
	src/extras/libc.c \
	src/control/core.c \
	src/control/util.c \
	src/control/audio_video.c \
	$(NULL)

SOURCES_libvlc = \
	$(SOURCES_libvlc_common) \
	$(OPT_SOURCES_libvlc_beos) \
	$(OPT_SOURCES_libvlc_darwin) \
	$(OPT_SOURCES_libvlc_win32) \
	$(OPT_SOURCES_libvlc_dirent) \
	$(OPT_SOURCES_libvlc_getopt) \
	$(NULL)

###############################################################################
# Building vlc
###############################################################################

bin_PROGRAMS = vlc

vlc_SOURCES = src/vlc.c

# Work around a bug in the arm-wince-pe linker
if HAVE_WINCE
vlc_WORKAROUNDLDFLAGS = $(LIB_libvlc)
endif

if BUILD_SHARED
### libvlc.so cannot be created into lib/
LIB_libvlc = libvlc$(LIBEXT)
LDFLAGS_libvlc =
#-Wl,-rpath $(libdir)
else
LIB_libvlc = lib/libvlc.a
LDFLAGS_libvlc = `$(VLC_CONFIG) --libs vlc builtin` 
endif

vlc_LDFLAGS = $(LDFLAGS_libvlc) $(vlc_WORKAROUNDLDFLAGS)
vlc_LDADD = $(LIB_libvlc) $(DATA_win32_rc) $(LIB_intl)
vlc_CFLAGS = `$(VLC_CONFIG) --cflags vlc`

# We use DEPENDENCIES_vlc instead of vlc_DEPENDENCIES because of an
# old automake-1.5 bug (automake/279).
DEPENDENCIES_vlc = $(LIB_libvlc) $(DATA_win32_rc) $(LIB_intl)

vlc$(EXEEXT): $(vlc_OBJECTS) $(DEPENDENCIES_vlc) stamp-builtin
	@rm -f vlc$(EXEEXT)
	@case `$(VLC_CONFIG) --linkage vlc builtin` in \
	  c++) cmd="$(CXXLINK)" ;; \
	  objc) cmd="$(OBJCLINK)" ;; \
	  c|*) cmd="$(LINK)" ;; \
	esac ; \
	cmd="$$cmd $(vlc_OBJECTS) $(vlc_LDADD) $(vlc_LDFLAGS) $(LIBS)" ; \
	echo $$cmd ; \
	eval $$cmd

if HAVE_BEOS
DATA_noinst_beos = vlc-bundle
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
	for i in $(ALL_LINGUAS); do \
	  mkdir -p "$(top_builddir)/vlc-bundle/locale/$$i/LC_MESSAGES" ; \
	  cp "$(top_builddir)/po/$$i.gmo" \
	    "$(top_builddir)/vlc-bundle/locale/$$i/LC_MESSAGES/vlc.mo" || true ; \
	done
	find $(top_builddir)/vlc-bundle -type f -exec mimeset -f "{}" \;
endif

# Install the symlinks and shared libvlc
install-exec-local:
	for i in "" $(ALIASES) ; do if test -n "$$i" ; then \
	  rm -f "$(DESTDIR)$(bindir)/$$i" && \
	  ln -s vlc "$(DESTDIR)$(bindir)/$$i" ; \
	fi ; done
	test -z "$(DATA_noinst_libvlc)" || $(INSTALL_PROGRAM) "$(DATA_noinst_libvlc)" "$(DESTDIR)$(libdir)"

# the opposite of install-{data,exec}-local
uninstall-local:
	for i in "" $(ALIASES) ; do if test -n "$$i" ; then \
	  rm -f "$(DESTDIR)$(bindir)/$$i" ; \
	fi ; done
	test -z "$(DATA_noinst_libvlc)" || rm -f "$(DESTDIR)$(libdir)/$(DATA_noinst_libvlc)"

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
	  echo "OK."; mkdir -p "$(top_builddir)/tmp/extras"; \
	fi
	rm -Rf $(top_builddir)/VLC-release.app
	cp -r $(srcdir)/extras/MacOSX $(top_builddir)/tmp/extras
	for i in AUTHORS COPYING README.MacOSX.rtf THANKS; do \
	  cp "$(srcdir)/$$i" $(top_builddir)/tmp; \
	done
	mkdir -p $(top_builddir)/tmp/modules/audio_output
	cp $(srcdir)/modules/audio_output/coreaudio.c \
	   $(top_builddir)/tmp/modules/audio_output/coreaudio.c
	mkdir -p $(top_builddir)/tmp/modules/gui/macosx
	for i in \
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
	    update.h \
	    update.m \
	    vout.m; do \
	  cp "$(srcdir)/modules/gui/macosx/$$i" \
             $(top_builddir)/tmp/modules/gui/macosx; \
	done
	case $(target_triplet) in \
	  *darwin6*) cd $(top_builddir)/tmp/extras/MacOSX && pbxbuild -target vlc | grep -v '^\([ \t]\|$$\)'; \
	    cd ../../../; \
	    cp -r $(top_builddir)/tmp/extras/MacOSX/build/VLC.bundle \
	          $(top_builddir)/VLC-release.app;; \
	  *darwin7*) cd $(top_builddir)/tmp/extras/MacOSX && xcodebuild -target vlc | grep -v '^\([ \t]\|$$\)'; \
	    cd ../../../; \
	    cp -r $(top_builddir)/tmp/extras/MacOSX/build/VLC.bundle \
	          $(top_builddir)/VLC-release.app;; \
	  *darwin8*) cd $(top_builddir)/tmp/extras/MacOSX && xcodebuild -target vlc | grep -v '^\([ \t]\|$$\)'; \
	    cd ../../../; \
	    cp -r $(top_builddir)/tmp/extras/MacOSX/build/Default/VLC.bundle \
	          $(top_builddir)/VLC-release.app;; \
	esac
	rm -Rf $(top_builddir)/tmp
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS
	$(INSTALL) $(top_builddir)/vlc \
		   $(top_builddir)/VLC-release.app/Contents/MacOS/VLC
	ln -sf ./VLC $(top_builddir)/VLC-release.app/Contents/MacOS/clivlc
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/modules
	for i in "" `$(VLC_CONFIG) --target plugin` ; do \
	  if test -n "$$i" ; \
	    then $(INSTALL) "$$i$(LIBEXT)" \
			   "$(top_builddir)/VLC-release.app/Contents/MacOS/modules" ; \
	  fi ; done
	if test -d $(srcdir)/extras/contrib/vlc-lib; then \
	  mkdir -p $(top_builddir)/VLC-release.app/Contents/MacOS/lib ; \
	  for i in $(srcdir)/extras/contrib/vlc-lib/*.dylib ; do \
	    $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/lib/vlc_`basename $${i}` ; \
	  done ; \
	fi
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http
	for i in $(srcdir)/share/http/* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/vlm
	for i in $(srcdir)/share/http/vlm/* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/vlm/`basename $${i}` ; \
	done ; \
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/admin
	for i in $(srcdir)/share/http/admin/* ; do \
	  $(INSTALL) -m 644 $${i} $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/admin/`basename $${i}` ; \
	done ; \
	$(INSTALL) -m 644 $(srcdir)/share/http/admin/.access $(top_builddir)/VLC-release.app/Contents/MacOS/share/http/admin/.access
	$(INSTALL) -d $(top_builddir)/VLC-release.app/Contents/MacOS/locale
	for i in $(ALL_LINGUAS); do \
	  mkdir -p $(top_builddir)/VLC-release.app/Contents/MacOS/locale/$${i}/LC_MESSAGES ; \
	  cp $(srcdir)/po/$${i}.gmo $(top_builddir)/VLC-release.app/Contents/MacOS/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	  mkdir -p $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/InfoPlist.strings \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/MainMenu.nib \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/vlc.scriptTerminology \
	      $(top_builddir)/VLC-release.app/Contents/Resources/$${i}.lproj ; \
	done
	printf "APPLVLC#" >| $(top_builddir)/VLC-release.app/Contents/PkgInfo

VLC.app: vlc
	@if test -e "$(top_builddir)/tmp"; then \
	  echo "Error: please remove $(top_builddir)/tmp, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(top_builddir)/tmp/extras"; \
	fi
	rm -Rf $(top_builddir)/VLC.app
	cp -r $(srcdir)/extras/MacOSX $(top_builddir)/tmp/extras
	for i in AUTHORS COPYING README.MacOSX.rtf THANKS; do \
	  cp "$(srcdir)/$$i" $(top_builddir)/tmp; \
	done
	mkdir -p $(top_builddir)/tmp/modules/audio_output
	cp $(srcdir)/modules/audio_output/coreaudio.c \
	   $(top_builddir)/tmp/modules/audio_output/coreaudio.c
	mkdir -p $(top_builddir)/tmp/modules/gui/macosx
	for i in \
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
	    update.h \
	    update.m \
	    vout.m; do \
	  cp "$(srcdir)/modules/gui/macosx/$$i" \
             $(top_builddir)/tmp/modules/gui/macosx; \
	done
	case $(target_triplet) in \
	  *darwin6*) cd $(top_builddir)/tmp/extras/MacOSX && pbxbuild -target vlc | grep -v '^\([ \t]\|$$\)'; \
	    cd ../../../; \
	    cp -r $(top_builddir)/tmp/extras/MacOSX/build/VLC.bundle \
	          $(top_builddir)/VLC.app;; \
	  *darwin7*) cd $(top_builddir)/tmp/extras/MacOSX && xcodebuild -target vlc | grep -v '^\([ \t]\|$$\)'; \
	    cd ../../../; \
	    cp -r $(top_builddir)/tmp/extras/MacOSX/build/VLC.bundle \
	          $(top_builddir)/VLC.app;; \
	  *darwin8*) cd $(top_builddir)/tmp/extras/MacOSX && xcodebuild -target vlc | grep -v '^\([ \t]\|$$\)'; \
	    cd ../../../; \
	    cp -r $(top_builddir)/tmp/extras/MacOSX/build/Default/VLC.bundle \
	          $(top_builddir)/VLC.app;; \
	esac
	rm -Rf $(top_builddir)/tmp
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS
	$(INSTALL) $(top_builddir)/vlc \
		   $(top_builddir)/VLC.app/Contents/MacOS/VLC
	ln -sf ./VLC $(top_builddir)/VLC.app/Contents/MacOS/clivlc
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS/modules
	for i in "" `$(VLC_CONFIG) --target plugin` ; do \
	  if test -n "$$i" ; \
	    then ln -sfn "`pwd`/$$i$(LIBEXT)" \
			   "$(top_builddir)/VLC.app/Contents/MacOS/modules" ; \
	  fi ; done
	if test -d $(srcdir)/extras/contrib/vlc-lib; then \
	  mkdir -p $(top_builddir)/VLC.app/Contents/MacOS/lib ; \
	  for i in $(srcdir)/extras/contrib/vlc-lib/*.dylib ; do \
	    ln -sfn `pwd`/$${i} $(top_builddir)/VLC.app/Contents/MacOS/lib/vlc_`basename $${i}` ; \
	  done ; \
	fi
	ln -sfn `pwd`/share $(top_builddir)/VLC.app/Contents/MacOS/
	$(INSTALL) -d $(top_builddir)/VLC.app/Contents/MacOS/locale
	for i in $(ALL_LINGUAS); do \
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
# Building specific source packages
###############################################################################

dist-woody: distdir
	$(remove_distdir_woody)
	mv $(distdir) $(PACKAGE)-woody-$(VERSION)
	$(srcdir)/toolbox --make-woody $(PACKAGE)-woody-$(VERSION)
	$(AMTAR) chof - $(PACKAGE)-woody-$(VERSION) \
	  | GZIP=$(GZIP_ENV) gzip -c >$(PACKAGE)-woody-$(VERSION).tar.gz
	$(remove_distdir_woody)

remove_distdir_woody = \
  { test ! -d $(PACKAGE)-woody-$(VERSION) \
    || { find $(PACKAGE)-woody-$(VERSION) -type d ! -perm -200 \
           -exec chmod u+w {} ';' \
         && rm -fr $(PACKAGE)-woody-$(VERSION); }; }

###############################################################################
# Building architecture-specific binary packages
###############################################################################

# XXX: this rule is probably only useful to you if you have exactly
# the same setup as me. Contact sam@zoy.org if you need to use it.
#
package-win32-base:
# Check that tmp isn't in the way
	@if test -e "$(top_builddir)/vlc-${VERSION}"; then \
	  echo "Error: please remove $(top_builddir)/vlc-${VERSION}, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(top_builddir)/vlc-${VERSION}"; \
	fi

# Copy relevant files
	cp "$(srcdir)/vlc.win32.nsi" "$(top_builddir)/vlc-${VERSION}/"
	cp "$(top_builddir)/vlc$(EXEEXT)" "$(top_builddir)/vlc-${VERSION}/"
	cp "$(top_srcdir)/vlc.exe.manifest" "$(top_builddir)/vlc-${VERSION}/"
	$(STRIP) "$(top_builddir)/vlc-${VERSION}/vlc$(EXEEXT)"

	for file in AUTHORS MAINTAINERS THANKS NEWS COPYING README ; \
	  do sed 's/@/_AT_/' < "$(srcdir)/$$file" > "$(top_builddir)/vlc-${VERSION}/$${file}.txt" ; \
	  unix2dos "$(top_builddir)/vlc-${VERSION}/$${file}.txt" ; done

	mkdir -p "$(top_builddir)/vlc-${VERSION}/plugins"
	for i in "" `$(VLC_CONFIG) --target plugin` ; do \
	  if test -n "$$i" ; then \
	    $(INSTALL) "$(top_builddir)/$$i$(LIBEXT)" \
            "$(top_builddir)/vlc-${VERSION}/plugins/" ; \
	  fi ; done

	for i in "" $(top_builddir)/vlc-${VERSION}/plugins/*$(LIBEXT) ; \
	  do if test -n "$$i" ; then $(STRIP) "$$i" ; fi ; done

	mkdir $(top_builddir)/vlc-${VERSION}/locale
	for i in $(ALL_LINGUAS); do \
	  mkdir -p "$(top_builddir)/vlc-${VERSION}/locale/$${i}/LC_MESSAGES" ; \
	  cp "$(srcdir)/po/$${i}.gmo" \
	    "$(top_builddir)/vlc-${VERSION}/locale/$${i}/LC_MESSAGES/vlc.mo" \
            || true ; \
	done

	mkdir -p $(top_builddir)/vlc-${VERSION}/skins/fonts
	for i in $(srcdir)/share/skins2/fonts/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/skins/fonts/ || true ; \
	done
	for i in $(srcdir)/share/skins2/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/skins/ || true ; \
	done

	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu/dvd"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu/dvd/selected"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu/dvd/unselect"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu/dvd/selection"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu/dvd/volume"
	for i in $(srcdir)/share/osdmenu/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/osdmenu/ || true ; \
	  unix2dos $(top_builddir)/vlc-${VERSION}/osdmenu/`basename $$i` ; \
	done
	for i in $(srcdir)/share/osdmenu/dvd/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/osdmenu/dvd || true ; \
	done
	for i in $(srcdir)/share/osdmenu/dvd/unselect/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/osdmenu/dvd/unselect || true ; \
	done
	for i in $(srcdir)/share/osdmenu/dvd/selected/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/osdmenu/dvd/selected/ || true ; \
	done
	for i in $(srcdir)/share/osdmenu/dvd/selection/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/osdmenu/dvd/selection/ || true ; \
	done
	for i in $(srcdir)/share/osdmenu/dvd/volume/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/osdmenu/dvd/volume/ || true ; \
	done

	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu/default"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu/default/selected"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu/default/selection"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/osdmenu/default/volume"
	for i in $(srcdir)/share/osdmenu/default/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/osdmenu/default || true ; \
	done
	for i in $(srcdir)/share/osdmenu/default/selected/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/osdmenu/default/selected/ || true ; \
	done
	for i in $(srcdir)/share/osdmenu/default/selection/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/osdmenu/default/selection/ || true ; \
	done
	for i in $(srcdir)/share/osdmenu/default/volume/*.*; do \
	  cp $$i $(top_builddir)/vlc-${VERSION}/osdmenu/default/volume/ || true ; \
	done

	mkdir -p "$(top_builddir)/vlc-${VERSION}/http/admin"
	mkdir -p "$(top_builddir)/vlc-${VERSION}/http/vlm"
	cp $(srcdir)/share/http/*.html $(top_builddir)/vlc-${VERSION}/http/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/*.html ;
	cp $(srcdir)/share/http/*.css $(top_builddir)/vlc-${VERSION}/http/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/*.css ;
	cp $(srcdir)/share/http/*.png $(top_builddir)/vlc-${VERSION}/http/
	cp $(srcdir)/share/http/*.ico $(top_builddir)/vlc-${VERSION}/http/ ;
	cp $(srcdir)/share/http/admin/*.html \
           $(top_builddir)/vlc-${VERSION}/http/admin/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/admin/*.html ;
	cp $(srcdir)/share/http/admin/dboxfiles.html \
           $(top_builddir)/vlc-${VERSION}/http/admin/ ;
	cp $(srcdir)/share/http/admin/.access \
           $(top_builddir)/vlc-${VERSION}/http/admin/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/admin/.access ;
	cp $(srcdir)/share/http/vlm/*.html \
           $(top_builddir)/vlc-${VERSION}/http/vlm/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/http/vlm/*.html ;

	cp $(srcdir)/share/vlc48x48new.ico $(top_builddir)/vlc-${VERSION}/ ;

	mkdir -p "$(top_builddir)/vlc-${VERSION}/mozilla"
if BUILD_MOZILLA
	cp $(top_builddir)/mozilla/*$(LIBEXT) $(top_builddir)/vlc-${VERSION}/mozilla/ ;
	$(STRIP) $(top_builddir)/vlc-${VERSION}/mozilla/*$(LIBEXT);
	cp $(top_builddir)/mozilla/vlcintf.xpt  $(top_builddir)/vlc-${VERSION}/mozilla/ ;
endif

	mkdir -p "$(top_builddir)/vlc-${VERSION}/activex"
if BUILD_ACTIVEX
	cp $(srcdir)/activex/README.TXT  $(top_builddir)/vlc-${VERSION}/activex/ ;
	cp $(srcdir)/activex/test.html  $(top_builddir)/vlc-${VERSION}/activex/ ;
	unix2dos $(top_builddir)/vlc-${VERSION}/activex/* ;

	cp $(top_builddir)/activex/*$(LIBEXT) $(top_builddir)/vlc-${VERSION}/activex/ ;
	$(STRIP) $(top_builddir)/vlc-${VERSION}/activex/*$(LIBEXT);
endif
# Rebase all those DLLs to speed up loading (need cygwin rebase)
	if [ -x rebase ]; then \
		find $(top_builddir)/vlc-${VERSION} -type f -name '*.dll' -print | rebase -b 0x70000000 -T -; \
	fi

package-win32-base-exe:
# Create package
	wine C:/Program\ Files/NSIS/makensis.exe \
             /DVERSION=${VERSION} $(top_builddir)/vlc-${VERSION}/vlc.win32.nsi

package-win32-base-exe-cygwin:
# Create package
	C:/Program\ Files/NSIS/makensis.exe \
             /DVERSION=${VERSION} $(top_builddir)/vlc-${VERSION}/vlc.win32.nsi

package-win32-base-exe-linux:
# Create package
	makensis -DVERSION=${VERSION} $(top_builddir)/vlc-${VERSION}/vlc.win32.nsi

package-win32-base-zip:
# Create package 
	zip -r vlc-${VERSION}-win32.zip vlc-${VERSION}

package-win32-exe: package-win32-base package-win32-base-exe
# Clean up
	rm -Rf $(top_builddir)/vlc-${VERSION}

package-win32-exe-linux: package-win32-base package-win32-base-exe-linux
# Clean up
	rm -Rf $(top_builddir)/vlc-${VERSION}

package-win32-zip: package-win32-base package-win32-base-zip
# Clean up
	rm -Rf $(top_builddir)/vlc-${VERSION}

package-win32: package-win32-base package-win32-base-exe package-win32-base-zip
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
	(cd $(srcdir)/tmp ; zip -9 -r vlc-${VERSION}-BeOS-debug.zip vlc-${VERSION} )
	mv $(srcdir)/tmp/vlc-${VERSION}-BeOS-debug.zip $(srcdir)/ ;
	mv $(srcdir)/tmp/vlc-${VERSION} $(srcdir)/tmp/vlc ;

# Create normal package
	$(STRIP) --strip-debug --strip-unneeded $(srcdir)/tmp/vlc/vlc ;
	find $(srcdir)/tmp/vlc -name 'lib*.so' -exec $(STRIP) \
	  --strip-debug --strip-unneeded "{}" \; ;
	xres -o $(srcdir)/tmp/vlc/vlc $(srcdir)/share/vlc_beos.rsrc ;
	find $(srcdir)/tmp/vlc -exec mimeset -f {} \; ;
	mv $(srcdir)/tmp/vlc $(srcdir)/tmp/vlc-${VERSION} ;
	(cd $(srcdir)/tmp ; zip -9 -r vlc-${VERSION}-BeOS.zip vlc-${VERSION} )
	mv $(srcdir)/tmp/vlc-${VERSION}-BeOS.zip $(srcdir)/ ;

# Clean up
	rm -Rf $(srcdir)/tmp ;

package-macosx:
# Check that tmp isn't in the way
	@if test -e "$(top_builddir)/tmp"; then \
	  echo "Error: please remove $(top_builddir)/tmp, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(top_builddir)/tmp"; \
	fi

# Copy relevant files 
	cp -R "$(top_builddir)/VLC-release.app" "$(top_builddir)/tmp/VLC.app"
	cd "$(srcdir)" && cp AUTHORS COPYING ChangeLog README README.MacOSX.rtf THANKS NEWS $(top_builddir)/tmp/

# Create disk image 
	$(srcdir)/extras/MacOSX/macosx-dmg "vlc-${VERSION}" $(top_builddir)/tmp

# Clean up
	rm -Rf $(top_builddir)/tmp

package-translations:
	@if test -e "$(srcdir)/vlc-translations-${VERSION}"; then \
	  echo "Error: please remove $(srcdir)/vlc-translations-${VERSION}, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(srcdir)/vlc-translations-${VERSION}"; \
	fi
# Copy translations
	for i in $(ALL_LINGUAS); do \
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
###############################################################################
.PHONY: update-po

update-po:
	rm -f $(top_srcdir)/po/POTFILES.in
	{ \
	  cd $(top_srcdir) ; \
	  echo "# automatically created by make update-po" ; \
	  echo "" ; \
	  echo "# main sources" ; \
	  find include src -name '*.[chm]' -o -name '*.[ch]pp' \
	    | grep -v '\(vlc_symbols\|misc/modules_\)' \
	    | sort ; \
	  echo "" ; \
	  echo "# modules" ; \
	  find modules -name '*.[chm]' -o -name '*.[ch]pp' \
	    | grep -v '\(\.moc\.\|gui/gtk2/\)' \
	    | sort ; \
	} > $(top_srcdir)/po/POTFILES.in
	rm -f $(top_srcdir)/po/vlc.pot
	cd po && $(MAKE) POTFILES vlc.pot update-po

#cd po && $(MAKE) update-po

###############################################################################
# Stamp rules
###############################################################################
stamp-builtin: FORCE
	@for dep in "" `$(VLC_CONFIG) --target builtin`; do \
	  if test "$${dep}" -nt "$(top_builddir)/vlc$(EXEEXT)"; then \
	    rm -f $@; \
	    break; \
	  fi; \
	done
	@if test ! -f $@; then printf "" > $@; fi

stamp-api: Makefile.in $(HEADERS_include) vlc-config vlc-api.pl
	( cd $(srcdir) && cat $(HEADERS_include) ) | \
	  top_srcdir="$(top_srcdir)" perl $(top_srcdir)/vlc-api.pl
	touch stamp-api

###############################################################################
# Force rule
###############################################################################
FORCE:
