###############################################################################
# Automake targets and declarations
###############################################################################

NULL =

# SUBDIRS stores the directories where a "make" is required when building
# something. DIST_SUBDIRS stores the directories where nothing is built but
# which have makefiles with distribution information.
SUBDIRS = intl modules . share po m4
DIST_SUBDIRS = $(SUBDIRS) src debian doc ipkg lib autotools

EXTRA_DIST = \
	HACKING \
	INSTALL.win32 \
	MAINTAINERS \
	README.MacOSX.rtf \
	bootstrap \
	install-win32 \
	src/extras/COPYING \
	toolbox \
	vlc-config.in.in \
	vlc.ebuild \
	vlc.spec \
	vlc.spec.redhat \
	$(NULL)

BUILT_SOURCES =
SUFFIXES = 

# Tell aclocal to use -I m4. Wonder if it really works.
ACLOCAL_AMFLAGS = -I m4

# XXX: these flags could be set in configure.ac, but we set them here
# because old versions of automake don't support them in configure.ac.
AUTOMAKE_OPTIONS = dist-bzip2 subdir-objects

###############################################################################
# Headers
###############################################################################

BUILT_SOURCES += \
	include/vlc_symbols.h \
	src/misc/modules_builtin.h \
	src/misc/modules_plugin.h \
	$(NULL)

pkgincludedir = $(includedir)/vlc

dist_pkginclude_HEADERS = \
	include/vlc/vlc.h \
	include/vlc/aout.h \
	include/vlc/vout.h \
	include/vlc/sout.h \
	include/vlc/decoder.h \
	include/vlc/input.h \
	include/vlc/intf.h \
	$(NULL)

noinst_HEADERS = $(HEADERS_include) $(HEADERS_include_built)

HEADERS_include = \
	include/aout_internal.h \
	include/audio_output.h \
	include/beos_specific.h \
	include/configuration.h \
	include/darwin_specific.h \
	include/codecs.h \
	include/encoder.h \
	include/input_ext-dec.h \
	include/input_ext-intf.h \
	include/input_ext-plugins.h \
	include/intf_eject.h \
	include/iso_lang.h \
	include/httpd.h \
	include/main.h \
	include/mmx.h \
	include/modules.h \
	include/modules_inner.h \
	include/mtime.h \
	include/network.h \
	include/os_specific.h \
	include/stream_control.h \
	include/stream_output.h \
	include/announce.h \
	include/variables.h \
	include/video_output.h \
	include/vlc_common.h \
	include/vlc_config.h \
	include/vlc_cpu.h \
	include/vlc_error.h \
	include/vlc_interface.h \
	include/vlc_messages.h \
	include/vlc_objects.h \
	include/vlc_playlist.h \
	include/vlc_threads.h \
	include/vlc_threads_funcs.h \
	include/vlc_video.h \
	include/vout_synchro.h \
	include/win32_specific.h \
	include/osd.h \
	$(NULL)

HEADERS_include_built = \
	include/vlc_symbols.h \
	$(NULL)

include/vlc_symbols.h: Makefile $(HEADERS_include)
	$(srcdir)/toolbox --update-includes

src/misc/modules_plugin.h: Makefile src/misc/modules_plugin.h.in $(HEADERS_include)
	$(srcdir)/toolbox --update-includes

src/misc/modules_builtin.h: Makefile src/misc/modules_builtin.h.in
	$(srcdir)/toolbox --update-includes

# These dependencies are mandatory
$(SOURCES): include/vlc_symbols.h
$(SOURCES_libvlc): src/misc/modules_plugin.h src/misc/modules_builtin.h $(LIB_intl)

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
	extras/MacOSX/Resources/English.lproj/MainMenu.nib/objects.nib \
	extras/MacOSX/Resources/English.lproj/InfoPlist.strings \
	extras/MacOSX/Resources/English.lproj/vlc.scriptTerminology \
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
	extras/MacOSX/Resources/back.png \
	extras/MacOSX/Resources/begin.png \
	extras/MacOSX/Resources/end.png \
	extras/MacOSX/Resources/forw.png \
	extras/MacOSX/Resources/list.png \
	extras/MacOSX/Resources/pause.png \
	extras/MacOSX/Resources/play.png \
	extras/MacOSX/Resources/prefs.png \
	extras/MacOSX/Resources/slow.png \
	extras/MacOSX/Resources/stop.png \
	extras/MacOSX/Resources/vlc_hg.png \
	extras/MacOSX/Resources/about_bg.png \
	extras/MacOSX/Resources/vlc.scriptSuite \
	extras/MacOSX/Resources/README \
	extras/MacOSX/vlc.pbproj/project.pbxproj \
	extras/MacOSX/macosx-dmg \
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
	evc/vlc.c \
	evc/config.h.in \
	evc/modules_builtin_evc.h \
	$(NULL)

dist-hook:
	mv $(distdir)/vlc.ebuild $(distdir)/vlc-${VERSION}.ebuild
	cd $(distdir) && $(srcdir)/toolbox --update-vc

###############################################################################
# Building libvlc
###############################################################################

bin_SCRIPTS = vlc-config

lib_LIBRARIES = lib/libvlc.a $(LIBRARIES_libvlc_pic)

lib_libvlc_a_SOURCES = $(SOURCES_libvlc)
dist_lib_libvlc_a_SOURCES = src/misc/modules_plugin.h
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
if BUILD_DIRENT
OPT_SOURCES_libvlc_dirent = $(SOURCES_libvlc_dirent)
endif
if BUILD_GETOPT
OPT_SOURCES_libvlc_getopt = $(SOURCES_libvlc_getopt)
endif
if BUILD_MOZILLA
LIBRARIES_libvlc_pic = lib/libvlc_pic.a
endif

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
	src/extras/dirent.h \
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
	src/input/input.c \
	src/input/input_ext-plugins.c \
	src/input/input_ext-dec.c \
	src/input/input_ext-intf.c \
	src/input/input_dec.c \
	src/input/input_programs.c \
	src/input/input_clock.c \
	src/input/input_info.c \
	src/video_output/video_output.c \
	src/video_output/vout_pictures.c \
	src/video_output/vout_pictures.h \
	src/video_output/video_text.c \
	src/video_output/video_text.h \
	src/video_output/vout_subpictures.c \
	src/video_output/vout_synchro.c \
	src/audio_output/common.c \
	src/audio_output/dec.c \
	src/audio_output/filters.c \
	src/audio_output/input.c \
	src/audio_output/mixer.c \
	src/audio_output/output.c \
	src/audio_output/intf.c \
	src/stream_output/stream_output.c \
	src/stream_output/announce.c \
	src/misc/mtime.c \
	src/misc/modules.c \
	src/misc/threads.c \
	src/misc/cpu.c \
	src/misc/configuration.c \
	src/misc/iso_lang.c \
	src/misc/iso-639_def.h \
	src/misc/messages.c \
	src/misc/objects.c \
	src/misc/variables.c \
	src/misc/error.c \
	src/extras/libc.c \
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

vlc_LDFLAGS = `$(VLC_CONFIG) --libs vlc builtin`
vlc_LDADD = lib/libvlc.a $(DATA_win32_rc) $(LIB_intl)
vlc_CFLAGS = `$(VLC_CONFIG) --cflags vlc`

# We use DEPENDENCIES_vlc instead of vlc_DEPENDENCIES because of an
# old automake-1.5 bug (automake/279).
DEPENDENCIES_vlc = lib/libvlc.a $(DATA_win32_rc) $(LIB_intl)

vlc$(EXEEXT): $(vlc_OBJECTS) $(DEPENDENCIES_vlc) stamp-builtin
	@rm -f $(top_builddir)/vlc$(EXEEXT)
	case `$(VLC_CONFIG) --linkage vlc builtin` in \
	  c++) $(CXXLINK) $(vlc_OBJECTS) $(vlc_LDADD) $(vlc_LDFLAGS) $(LIBS) ;; \
	  objc) $(OBJCLINK) $(vlc_OBJECTS) $(vlc_LDADD) $(vlc_LDFLAGS) $(LIBS) ;; \
	  c|*) $(LINK) $(vlc_OBJECTS) $(vlc_LDADD) $(vlc_LDFLAGS) $(LIBS) ;; \
	esac

if HAVE_BEOS
	xres -o $@ $(srcdir)/share/vlc_beos.rsrc
	mimeset -f $@
	rm -Rf $(srcdir)/locale ; mkdir -p "$(srcdir)/locale"
	for i in $(ALL_LINGUAS); do \
	  mkdir -p "$(srcdir)/locale/$${i}/LC_MESSAGES" ; \
	  cp "$(srcdir)/po/$${i}.gmo" \
	    "$(srcdir)/locale/$${i}/LC_MESSAGES/vlc.mo" || true ; \
	done
endif

# Install the modules and the symlinks
install-exec-local:
	for i in "" `$(VLC_CONFIG) --target plugin` ; do if test -n "$$i" ; then \
	  $(INSTALL) -m 755 "$$i$(LIBEXT)" "$(DESTDIR)$(libdir)/vlc/`echo $$i | cut -f1 -d/`" ; \
	fi ; done
	for i in "" `$(VLC_CONFIG) --target builtin` `$(VLC_CONFIG) --target builtin pic` ; do if test -n "$$i" ; then \
	  $(INSTALL) -m 644 "$$i" "$(DESTDIR)$(libdir)/vlc" ; \
	fi ; done
	for i in "" $(ALIASES) ; do if test -n "$$i" ; then \
	  rm -f "$(DESTDIR)$(bindir)/$$i" && \
	  ln -s vlc "$(DESTDIR)$(bindir)/$$i" ; \
	fi ; done

if HAVE_DARWIN
# Create the MacOS X app
vlc_app_DATA = VLC.app
vlc_appdir = $(bindir)
VLC.app: vlc
	rm -Rf VLC.app
	cd $(srcdir)/extras/MacOSX && pbxbuild -target vlc | grep -v '^\([ \t]\|$$\)'
	cp -r $(srcdir)/extras/MacOSX/build/vlc.bundle $(srcdir)/VLC.app
	$(INSTALL) -d $(srcdir)/VLC.app/Contents/MacOS
	$(INSTALL) vlc $(srcdir)/VLC.app/Contents/MacOS/VLC
	$(INSTALL) -d $(srcdir)/VLC.app/Contents/MacOS/modules
	for i in "" `$(VLC_CONFIG) --target plugin` ; do \
	  if test -n "$$i" ; then $(INSTALL) "$(srcdir)/$$i$(LIBEXT)" \
	    "$(srcdir)/VLC.app/Contents/MacOS/modules" ; \
	  fi ; done
	$(INSTALL) -d $(srcdir)/VLC.app/Contents/MacOS/share
	$(INSTALL) -m 644 $(srcdir)/share/*.psf $(srcdir)/share/*.rle \
	  $(srcdir)/VLC.app/Contents/MacOS/share
	$(INSTALL) -d $(srcdir)/VLC.app/Contents/MacOS/locale
	for i in $(ALL_LINGUAS); do \
	  mkdir -p $(srcdir)/VLC.app/Contents/MacOS/locale/$${i}/LC_MESSAGES ; \
	  cp $(srcdir)/po/$${i}.gmo $(srcdir)/VLC.app/Contents/MacOS/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	  mkdir -p $(srcdir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/InfoPlist.strings $(srcdir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/MainMenu.nib $(srcdir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	  ln -sf ../English.lproj/vlc.scriptTerminology $(srcdir)/VLC.app/Contents/Resources/$${i}.lproj ; \
	done
	printf "APPLVLC#" >| $(srcdir)/VLC.app/Contents/PkgInfo
endif

if HAVE_WIN32
DATA_win32_rc = $(noinst_share_vlc_win32_rc_DATA)
noinst_share_vlc_win32_rc_DATA = share/vlc_win32_rc.$(OBJEXT)
noinst_share_vlc_win32_rcdir = $(libdir)
share/vlc_win32_rc.$(OBJEXT): share/vlc_win32_rc.rc
	$(WINDRES) --include-dir $(srcdir)/share -i $< -o $@
endif

###############################################################################
# Building architecture-specific binary packages
###############################################################################

# XXX: this rule is probably only useful to you if you have exactly
# the same setup as me. Contact sam@zoy.org if you need to use it.
#
# Check that tmp isn't in the way
package-win32:
	@if test -e "$(srcdir)/tmp"; then \
	  echo "Error: please remove $(srcdir)/tmp, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(srcdir)/tmp"; \
	fi
# Create installation script
	cp "$(srcdir)/install-win32" "$(srcdir)/tmp/nsi"
# Copy relevant files
	cp "$(top_builddir)/vlc$(EXEEXT)" "$(srcdir)/tmp/"
	$(STRIP) "$(srcdir)/tmp/vlc$(EXEEXT)"
	cp "$(srcdir)/INSTALL.win32" "$(srcdir)/tmp/INSTALL.txt"
	unix2dos "$(srcdir)/tmp/INSTALL.txt"
	for file in AUTHORS MAINTAINERS THANKS NEWS COPYING README ; \
	  do cp "$(srcdir)/$$file" "$(srcdir)/tmp/$${file}.txt" ; \
	  unix2dos "$(srcdir)/tmp/$${file}.txt" ; done

	mkdir -p "$(srcdir)/tmp/plugins"
	for i in "" `$(VLC_CONFIG) --target plugin` ; do \
	  if test -n "$$i" ; then \
	    $(INSTALL) "$(srcdir)/$$i$(LIBEXT)" "$(srcdir)/tmp/plugins/" ; \
	  fi ; done

	for i in "" $(srcdir)/tmp/plugins/*$(LIBEXT) ; \
	  do if test -n "$$i" ; then $(STRIP) "$$i" ; fi ; done

	mkdir -p "$(srcdir)/tmp/share"
	cp $(srcdir)/share/*.rle $(srcdir)/tmp/share/ ;

	mkdir $(srcdir)/tmp/locale
	for i in $(ALL_LINGUAS); do \
	  mkdir -p "$(srcdir)/tmp/locale/$${i}/LC_MESSAGES" ; \
	  cp "$(srcdir)/po/$${i}.gmo" \
	    "$(srcdir)/tmp/locale/$${i}/LC_MESSAGES/vlc.mo" || true ; \
	done

	mkdir -p $(srcdir)/tmp/skins/default
	for i in $(srcdir)/share/skins/default/*.*; do \
	  cp $$i $(srcdir)/tmp/skins/default/ || true ; \
	done
	for i in $(srcdir)/doc/skins/*.txt; do \
	  cp $$i $(srcdir)/tmp/skins/ || true ; \
	done

# Create package 
	wine ~/.wine/fake_windows/Program\ Files/NSIS/makensis.exe -- /DVERSION=${VERSION} $(srcdir)/tmp/nsi
# Clean up
	rm -Rf $(srcdir)/tmp

package-beos:
# Check that tmp isn't in the way
	@if test -e $(srcdir)/tmp; then \
	  echo "Error: please remove $(srcdir)/tmp, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir $(srcdir)/tmp; \
	fi

# Copy relevant files
	mkdir -p $(srcdir)/tmp/vlc
	cd $(srcdir) && \
	  cp vlc AUTHORS COPYING ChangeLog README THANKS NEWS tmp/vlc/
# We cannot use ${PLUGIN_FILES}, it looks like it is too long for the BeOS shell...
	if [ `find $(srcdir)/modules -name "lib*_plugin.so" | wc -l` -gt 0 ]; then \
	  mkdir -p $(srcdir)/tmp/vlc/plugins; \
	  find $(srcdir)/modules -name "lib*_plugin.so" | xargs -i{} cp {} $(srcdir)/tmp/vlc/plugins/; \
	fi
	for i in $(ALL_LINGUAS); do \
	  mkdir -p $(srcdir)/tmp/vlc/locale/$${i}/LC_MESSAGES ; \
	  cp $(srcdir)/po/$${i}.gmo $(srcdir)/tmp/vlc/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	done
	mkdir -p $(srcdir)/tmp/vlc/share
	cp $(srcdir)/share/*.rle $(srcdir)/tmp/vlc/share/

# Create package with debug symbols
	mimeset -f $(srcdir)/tmp/vlc/* $(srcdir)/tmp/vlc/*/*
	mv $(srcdir)/tmp/vlc $(srcdir)/tmp/vlc-${VERSION}
	(cd $(srcdir)/tmp ; zip -9 -r vlc-${VERSION}-BeOS-debug.zip vlc-${VERSION} )
	mv $(srcdir)/tmp/vlc-${VERSION}-BeOS-debug.zip $(srcdir)/

# Create package without debug symbols
	strip $(srcdir)/tmp/vlc-${VERSION}/vlc
	if [ -d $(srcdir)/tmp/vlc-${VERSION}/plugins ]; then \
	  strip $(srcdir)/tmp/vlc-${VERSION}/plugins/lib*_plugin.so; \
	fi
	xres -o $(srcdir)/tmp/vlc-${VERSION}/vlc $(srcdir)/share/vlc_beos.rsrc
	mimeset -f $(srcdir)/tmp/vlc-${VERSION}/* $(srcdir)/tmp/vlc-${VERSION}/*/*
	(cd $(srcdir)/tmp ; zip -9 -r vlc-${VERSION}-BeOS.zip vlc-${VERSION} )
	mv $(srcdir)/tmp/vlc-${VERSION}-BeOS.zip $(srcdir)/

# Clean up
	rm -Rf $(srcdir)/tmp

package-macosx:
# Check that tmp isn't in the way
	@if test -e "$(srcdir)/tmp"; then \
	  echo "Error: please remove $(srcdir)/tmp, it is in the way"; \
	  false; \
	else \
	  echo "OK."; mkdir -p "$(srcdir)/tmp"; \
	fi

# Copy relevant files 
	cp -R "$(srcdir)/VLC.app" "$(srcdir)/tmp/"
	cd "$(srcdir)" && cp AUTHORS COPYING ChangeLog README README.MacOSX.rtf THANKS NEWS tmp/

# Create disk image 
	$(srcdir)/extras/MacOSX/macosx-dmg 18 "vlc-${VERSION}" $(srcdir)/tmp/* 

# Clean up
	rm -Rf $(srcdir)/tmp

###############################################################################
# Building the Mozilla plugin
###############################################################################

noinst_LIBRARIES = $(noinst_LIBRARIES_mozilla)

MOSTLYCLEANFILES = $(LIBRARIES_mozilla)

EXTRA_DIST += $(SOURCES_mozilla) mozilla/vlcintf.idl

BUILT_SOURCES += $(BUILT_SOURCES_mozilla)

SOURCES_mozilla = \
	mozilla/vlcshell.cpp \
	mozilla/vlcplugin.cpp \
	mozilla/vlcplugin.h \
	mozilla/vlcpeer.cpp \
	mozilla/vlcpeer.h \
	mozilla/support/classinfo.h \
	$(SOURCES_mozilla_win32) \
	$(SOURCES_mozilla_macosx) \
	$(SOURCES_mozilla_unix) \
	$(NULL)

# Under Win32, Mozilla plugins need to be named NP******.DLL, but under Unix
# the common naming scheme is lib******plugin.so. Also, we need npwin.cpp
# under Win32 and npunix.c under Unix.
if HAVE_WIN32
LIBRARIES_mozilla = mozilla/npvlc$(LIBEXT)
SOURCES_mozilla_win32 = mozilla/support/npwin.cpp
CPPFLAGS_mozilla_EXTRA = -DXP_WIN -DXP_WIN32
else
if HAVE_DARWIN
# We don't define LIBRARIES_mozilla because we'll be using project builder
SOURCES_mozilla_macosx = mozilla/support/npmac.cpp
else
LIBRARIES_mozilla = mozilla/libvlcplugin$(LIBEXT)
SOURCES_mozilla_unix = mozilla/support/npunix.c
endif
endif

if BUILD_MOZILLA
if UNTRUE
noinst_LIBRARIES_mozilla = mozilla/libplugin.a
endif

mozilla_libplugin_a_SOURCES = $(SOURCES_mozilla)
mozilla_libplugin_a_CFLAGS = `$(VLC_CONFIG) --cflags mozilla pic` \
                             $(CPPFLAGS_mozilla_EXTRA) -Imozilla
mozilla_libplugin_a_CXXFLAGS = `$(VLC_CONFIG) --cxxflags mozilla pic` \
                               $(CPPFLAGS_mozilla_EXTRA) -Imozilla
mozilla_libplugin_a_DEPENDENCIES = lib/libvlc_pic.a $(DATA_npvlc_rc)

BUILT_SOURCES_mozilla = mozilla/vlcintf.h
$(SOURCES_mozilla): mozilla/vlcintf.h

mozilla_plugin_DATA = $(LIBRARIES_mozilla)
mozilla_plugindir = $(libdir)/mozilla/plugins
$(LIBRARIES_mozilla): $(mozilla_libplugin_a_OBJECTS) \
                      $(mozilla_libplugin_a_DEPENDENCIES) stamp-pic
	$(CXXLINK) $(mozilla_libplugin_a_OBJECTS) $(DATA_npvlc_rc) \
	  lib/libvlc_pic.a -shared \
	  `$(VLC_CONFIG) --libs vlc mozilla builtin pic`

mozilla_vlcintf_xpt_DATA = mozilla/vlcintf.xpt
mozilla_vlcintf_xptdir = $(libdir)/mozilla/components
mozilla/vlcintf.xpt: mozilla/vlcintf.idl
	$(XPIDL) -I/usr/share/idl/mozilla -m typelib \
	  -o mozilla/vlcintf $(srcdir)/mozilla/vlcintf.idl

mozilla/vlcintf.h: mozilla/vlcintf.idl
	$(XPIDL) -I/usr/share/idl/mozilla -m header \
	  -o mozilla/vlcintf $(srcdir)/mozilla/vlcintf.idl

if HAVE_WIN32
DATA_npvlc_rc = $(noinst_mozilla_npvlc_rc_DATA)
noinst_mozilla_npvlc_rc_DATA = mozilla/npvlc_rc.$(OBJEXT)
noinst_mozilla_npvlc_rcdir = $(libdir)
mozilla/npvlc_rc.$(OBJEXT): mozilla/npvlc_rc.rc
	$(WINDRES) --include-dir $(srcdir)/mozilla -i $< -o $@
endif
endif

###############################################################################
# Stamp rules
###############################################################################
clean: clean-stamps
clean-stamp:
	rm -f stamp-builtin stamp-pic

stamp-builtin: FORCE
	@for dep in "" `$(VLC_CONFIG) --target builtin`; do \
	  if test "$${dep}" -nt "$(top_builddir)/vlc$(EXEEXT)"; then \
	    rm -f $@; \
	    break; \
	  fi; \
	done
	@if test ! -f $@; then printf "" > $@; fi

stamp-pic: FORCE
	@for dep in "" `$(VLC_CONFIG) --target builtin pic`; do \
	  if test "$${dep}" -nt "$(LIBRARIES_mozilla)"; then \
	    rm -f $@; \
	    break; \
	  fi; \
	done
	@if test ! -f $@; then printf "" > $@; fi

###############################################################################
# Force rule
###############################################################################
FORCE:
