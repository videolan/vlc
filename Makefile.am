###############################################################################
# Automake targets and declarations
###############################################################################

NULL =

# SUBDIRS stores the directories where a "make" is required when building
# something. DIST_SUBDIRS stores the directories where nothing is built but
# which have makefiles with distribution information.
SUBDIRS = po intl m4 share
DIST_SUBDIRS = $(SUBDIRS) modules src debian doc ipkg lib autotools

EXTRA_DIST = \
	HACKING \
	INSTALL.win32 \
	MAINTAINERS \
	Modules.am \
	README.MacOSX.rtf \
	bootstrap \
	configure.ac.in \
	install-win32 \
	src/extras/COPYING \
	toolbox \
	vlc.spec \
	$(NULL)

MOSTLYCLEANFILES =
BUILT_SOURCES =
SUFFIXES = 

# List of programs, libraries and headers that need to be built and/or
# distributed. Initialized to empty because we'll use += later.
bin_PROGRAMS =

lib_LIBRARIES = 
libvlc_LIBRARIES =
noinst_LIBRARIES =

noinst_HEADERS =

# Tell aclocal to use -I m4. Wonder if it really works.
ACLOCAL_AMFLAGS = -I m4

# XXX: these flags could be set in configure.ac.in, but we set them here
# because old versions of automake don't support them in configure.ac.
AUTOMAKE_OPTIONS = dist-bzip2 subdir-objects

###############################################################################
# Compilation flags for debug mode, profiling, and others
###############################################################################

# Standard flags used everywhere to build things in the distribution. Some
# of them are empty but might be extended later in the Makefile. *_default
# is the default flag list for all files in the distribution, for instance
# the vlc code. *_pic is for PIC code, such as the Mozilla plugin. *_plugin
# is for plugin code, *_builtin is for builtin code, and *_builtin_pic is
# for PIC builtin code, for instance builtin modules in the mozilla plugin.

CPPFLAGS_default = -I$(top_srcdir)/include
CFLAGS_default = 
CXXFLAGS_default = 
OBJCFLAGS_default = 
LDFLAGS_default =

CPPFLAGS_pic = $(CPPFLAGS_default)
CFLAGS_pic = $(CFLAGS_default) @CFLAGS_pics@
CXXFLAGS_pic = $(CXXFLAGS_default) @CFLAGS_pics@
OBJCFLAGS_pic = $(OBJCFLAGS_default) @CFLAGS_pics@
LDFLAGS_pic = $(LDFLAGS_default)

CPPFLAGS_plugin = $(CPPFLAGS_default) -D__VLC__ -D__PLUGIN__
CFLAGS_plugin = $(CFLAGS_default) @CFLAGS_plugins@
CXXFLAGS_plugin = $(CXXFLAGS_default) @CFLAGS_plugins@
OBJCFLAGS_plugin = $(OBJCFLAGS_default) @CFLAGS_plugins@
LDFLAGS_plugin = @LDFLAGS_plugins@

CPPFLAGS_builtin = $(CPPFLAGS_default) -D__VLC__ -D__BUILTIN__
CFLAGS_builtin = $(CFLAGS_default) @CFLAGS_builtins@
CXXFLAGS_builtin = $(CXXFLAGS_default) @CFLAGS_builtins@
OBJCFLAGS_builtin = $(OBJCFLAGS_default) @CFLAGS_builtins@
LDFLAGS_builtin =
L_builtin =

CPPFLAGS_builtin_pic = $(CPPFLAGS_builtin) $(CPPFLAGS_pic)
CFLAGS_builtin_pic = $(CFLAGS_builtin) $(CFLAGS_pic)
CXXFLAGS_builtin_pic = $(CXXFLAGS_builtin) $(CXXFLAGS_pic)
OBJCFLAGS_builtin_pic = $(OBJCFLAGS_builtin) $(OBJCFLAGS_pic)
LDFLAGS_builtin_pic = $(LDFLAGS_builtin) $(LDFLAGS_pic)
L_builtin_pic = $(L_builtin) $(L_pic)

# On Linux and Solaris, activate 64-bit off_t (by default under BSD)
CPPFLAGS_default += -D_FILE_OFFSET_BITS=64 -D__USE_UNIX98 -D_LARGEFILE64_SOURCE
CPPFLAGS_default += -D_REENTRANT -D_THREAD_SAFE
CPPFLAGS_default += -D_GNU_SOURCE

# Gettext support
CPPFLAGS_default += -DLOCALEDIR=\"$(datadir)/locale\"

# Data and plugin location
CPPFLAGS_default += -DDATA_PATH=\"@prefix@/share/vlc\"
CPPFLAGS_default += -DPLUGIN_PATH=\"@prefix@/lib/vlc\"

# Conditional flags that get added to *FLAGS_default
if RELEASE
CPPFLAGS_release = -DHAVE_RELEASE
endif
if DEBUG
CPPFLAGS_debug = -DDEBUG
CFLAGS_debug = -g
endif
if GPROF
CPPFLAGS_gprof = -DGPROF
CFLAGS_gprof = -finstrument-functions
endif
if CPROF
CPPFLAGS_cprof = -DCPROF
CFLAGS_cprof = -pg
endif
if OPTIM
CFLAGS_optim = @CFLAGS_OPTIM@ @CFLAGS_TUNING@
if DEBUG
else
if GPROF
else
if CPROF
else
CFLAGS_nodebug = @CFLAGS_OPTIM_NODEBUG@
endif
endif
endif
else
# !OPTIM
CFLAGS_optim = @CFLAGS_NOOPTIM@
endif

CPPFLAGS_default += $(CPPFLAGS_release) \
		$(CPPFLAGS_debug) $(CPPFLAGS_gprof) $(CPPFLAGS_cprof)
CFLAGS_default += $(CFLAGS_optim) $(CFLAGS_nodebug) \
		$(CFLAGS_debug) $(CFLAGS_gprof) $(CFLAGS_cprof)
CXXFLAGS_default += $(CFLAGS_optim) $(CFLAGS_nodebug) \
		$(CFLAGS_debug) $(CFLAGS_gprof) $(CFLAGS_cprof)
OBJCFLAGS_default += $(CFLAGS_optim) $(CFLAGS_nodebug) \
		$(CFLAGS_debug) $(CFLAGS_gprof) $(CFLAGS_cprof)

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

noinst_HEADERS += $(HEADERS_include) $(HEADERS_include_built)

HEADERS_include = \
	include/aout_internal.h \
	include/audio_output.h \
	include/beos_specific.h \
	include/configuration.h \
	include/darwin_specific.h \
	include/codecs.h \
	include/error.h \
	include/encoder.h \
	include/input_ext-dec.h \
	include/input_ext-intf.h \
	include/input_ext-plugins.h \
	include/interface.h \
	include/intf_eject.h \
	include/iso_lang.h \
	include/httpd.h \
	include/main.h \
	include/mmx.h \
	include/modules.h \
	include/modules_inner.h \
	include/mtime.h \
	include/netutils.h \
	include/network.h \
	include/os_specific.h \
	include/stream_control.h \
	include/stream_output.h \
	include/variables.h \
	include/video.h \
	include/video_output.h \
	include/vlc_common.h \
	include/vlc_config.h \
	include/vlc_cpu.h \
	include/vlc_messages.h \
	include/vlc_objects.h \
	include/vlc_playlist.h \
	include/vlc_threads.h \
	include/vlc_threads_funcs.h \
	include/vout_synchro.h \
	include/win32_specific.h \
	include/osd.h \
	$(NULL)

HEADERS_include_built = \
	include/vlc_symbols.h \
	$(NULL)

include/vlc_symbols.h: Makefile $(HEADERS_include)
	./toolbox --update-includes $(BUILTINS)

src/misc/modules_plugin.h: Makefile src/misc/modules_plugin.h.in $(HEADERS_include)
	./toolbox --update-includes $(BUILTINS)

src/misc/modules_builtin.h: Makefile src/misc/modules_builtin.h.in
	./toolbox --update-includes $(BUILTINS)

# These dependencies are mandatory
$(SOURCES): include/vlc_symbols.h
$(SOURCES_libvlc): src/misc/modules_plugin.h src/misc/modules_builtin.h $(LIB_intl)

###############################################################################
# Optional libintl - FIXME, bad dependencies
###############################################################################

intl/libintl.a: FORCE
	cd intl && $(MAKE)

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
	extras/MacOSX/Resources/divx.icns \
	extras/MacOSX/Resources/generic.icns \
	extras/MacOSX/Resources/mpeg.icns \
	extras/MacOSX/Resources/mpeg1.icns \
	extras/MacOSX/Resources/mpeg2.icns \
	extras/MacOSX/Resources/mpeg4.icns \
	extras/MacOSX/Resources/vlc.icns \
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

show-libvlc-sources: FORCE
	@echo X: $(SOURCES_libvlc_common) $(SOURCES_libvlc_win32) $(SOURCES_libvlc_dirent) $(SOURCES_libvlc_getopt)

show-libvlc-headers: FORCE
	@echo X: $(HEADERS_include) $(HEADERS_include_built)

show-libvlc-pkg-headers: FORCE
	@echo X: $(dist_pkginclude_HEADERS)

dist-hook:
	cd $(distdir); ./toolbox --update-vc

###############################################################################
# Building libvlc
###############################################################################

bin_SCRIPTS = vlc-config

lib_LIBRARIES += lib/libvlc.a $(LIBRARIES_libvlc_pic)

lib_libvlc_a_SOURCES = $(SOURCES_libvlc)
dist_lib_libvlc_a_SOURCES = src/misc/modules_plugin.h
lib_libvlc_a_CFLAGS = $(CPPFLAGS_default) -D__VLC__ $(CFLAGS_default) @CFLAGS_vlc@
lib_libvlc_a_CXXFLAGS = $(CPPFLAGS_default) -D__VLC__ $(CXXFLAGS_default)
lib_libvlc_a_OBJCFLAGS = $(CPPFLAGS_default) -D__VLC__ $(OBJCFLAGS_default)

lib_libvlc_pic_a_SOURCES = $(SOURCES_libvlc)
lib_libvlc_pic_a_CFLAGS = $(CPPFLAGS_pic) -D__VLC__ $(CFLAGS_pic) @CFLAGS_vlc@
lib_libvlc_pic_a_CXXFLAGS = $(CPPFLAGS_pic) -D__VLC__ $(CXXFLAGS_pic)
lib_libvlc_pic_a_OBJCFLAGS = $(CPPFLAGS_pic) -D__VLC__ $(OBJCFLAGS_pic)

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
	src/misc/mtime.c \
	src/misc/modules.c \
	src/misc/threads.c \
	src/misc/cpu.c \
	src/misc/configuration.c \
	src/misc/netutils.c \
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

bin_PROGRAMS += vlc

vlc_SOURCES = src/vlc.c

# @AUTOMAKE_SUCKS@ gets expanded to $(L_builtin) $(LDFLAGS_builtin)
# but we don't write it directly, otherwise automake will go amok and eat all
# the memory because of its 2^N crap algorithm. So we fool him. Nuahaha.
vlc_LDADD = lib/libvlc.a $(LDFLAGS_vlc) \
            $(DATA_win32_rc) $(LIB_intl) @AUTOMAKE_SUCKS@
vlc_CFLAGS = $(CPPFLAGS_default) $(CFLAGS_default)

# We use DEPENDENCIES_vlc instead of vlc_DEPENDENCIES because of an
# old automake-1.5 bug (automake/279).
DEPENDENCIES_vlc = lib/libvlc.a $(L_builtin) $(DATA_win32_rc) $(LIB_intl)

vlc$(EXEEXT): $(vlc_OBJECTS) $(DEPENDENCIES_vlc)
	@rm -f vlc$(EXEEXT)
	$(LINK) $(vlc_LDFLAGS) $(vlc_OBJECTS) $(vlc_LDADD) $(LIBS)

if HAVE_BEOS
	xres -o $@ ./share/vlc_beos.rsrc
	mimeset -f $@
	rm -Rf locale ; mkdir locale
	for i in $(ALL_LINGUAS); do \
		mkdir -p locale/$${i}/LC_MESSAGES ; \
		cp po/$${i}.gmo locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	done
endif

# Install the symlinks
install-exec-local:
	for i in dummy $(ALIASES) ; do if test "x$$i" != "xdummy" ; then \
		rm -f $(DESTDIR)$(bindir)/$$i && \
		ln -s vlc $(DESTDIR)$(bindir)/$$i ; \
	fi ; done

if HAVE_DARWIN
# Create the MacOS X app
vlc_app_DATA = VLC.app
vlc_appdir = $(bindir)
VLC.app: vlc $(PLUGIN_FILES)
	rm -Rf VLC.app
	cd extras/MacOSX ; pbxbuild -target vlc | grep -v '^\([ \t]\|$$\)'
	cp -r extras/MacOSX/build/vlc.bundle ./VLC.app
	$(INSTALL) -d VLC.app/Contents/MacOS
	$(INSTALL) vlc VLC.app/Contents/MacOS/VLC
	$(INSTALL) -d VLC.app/Contents/MacOS/modules
	for i in dummy $(PLUGIN_FILES) ; do if test "x$$i" != "xdummy" ; then \
		$(INSTALL) $$i VLC.app/Contents/MacOS/modules/ ; \
	fi ; done
	$(INSTALL) -d VLC.app/Contents/MacOS/share
	$(INSTALL) -m 644 share/*.psf share/*.rle VLC.app/Contents/MacOS/share
	$(INSTALL) -d VLC.app/Contents/MacOS/locale
	for i in $(ALL_LINGUAS); do \
		mkdir -p VLC.app/Contents/MacOS/locale/$${i}/LC_MESSAGES ; \
		cp po/$${i}.gmo VLC.app/Contents/MacOS/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
		mkdir -p VLC.app/Contents/Resources/$${i}.lproj ; \
		ln -sf ../English.lproj/InfoPlist.strings VLC.app/Contents/Resources/$${i}.lproj ; \
		ln -sf ../English.lproj/MainMenu.nib VLC.app/Contents/Resources/$${i}.lproj ; \
		ln -sf ../English.lproj/vlc.scriptTerminology VLC.app/Contents/Resources/$${i}.lproj ; \
	done
	echo -n "APPLVLC#" >| VLC.app/Contents/PkgInfo
endif

if HAVE_WIN32
DATA_win32_rc = $(noinst_share_vlc_win32_rc_DATA)
noinst_share_vlc_win32_rc_DATA = share/vlc_win32_rc.$(OBJEXT)
noinst_share_vlc_win32_rcdir = $(libdir)
share/vlc_win32_rc.$(OBJEXT): share/vlc_win32_rc.rc
	$(WINDRES) --include-dir share -i $< -o $@
endif

###############################################################################
# Building architecture-specific binary packages
###############################################################################

# XXX: this rule is probably only useful to you if you have exactly
# the same setup as me. Contact sam@zoy.org if you need to use it.
#
# Check that tmp isn't in the way
package-win32:
	@if test -e tmp; then \
		echo "Error: please remove ./tmp, it is in the way"; false; \
	else \
		echo "OK."; mkdir tmp; \
	fi
# Create installation script
	cp install-win32 tmp/nsi
# Copy relevant files
	cp vlc.exe tmp/ 
	$(STRIP) tmp/vlc.exe
	cp INSTALL.win32 tmp/INSTALL.txt ; unix2dos tmp/INSTALL.txt
	for file in AUTHORS MAINTAINERS THANKS NEWS COPYING README ; \
			do cp $$file tmp/$${file}.txt ; \
			unix2dos tmp/$${file}.txt ; done

	mkdir tmp/plugins
	for i in dummy $(PLUGIN_FILES) ; do if test "x$$i" != "xdummy" ; then \
		$(INSTALL) $$i tmp/plugins/ ; \
	fi ; done

	for i in dummy tmp/plugins/*$(LIBEXT) ; \
		do if test $$i != tmp/plugins/libwin32_plugin$(LIBEXT) \
		-a $$i != dummy ; then $(STRIP) $$i ; fi ; done

	mkdir tmp/share
	cp share/*.rle tmp/share/ ;

	mkdir tmp/locale
	for i in $(ALL_LINGUAS); do \
		mkdir -p tmp/locale/$${i}/LC_MESSAGES ; \
		cp po/$${i}.gmo tmp/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	done

	mkdir -p tmp/skins/default
	for i in share/skins/default/*.*; do \
		cp $$i tmp/skins/default/ || true ; \
	done
	for i in doc/skins/*.txt; do \
		cp $$i tmp/skins/ || true ; \
	done

	cd doc/faq ; $(MAKE) html ; cd ../.. ;
	cp doc/faq/index.html tmp/FAQ.htm ;

# Create package 
	wine ~/.wine/fake_windows/Program\ Files/NSIS/makensis.exe -- /DVERSION=${VERSION} tmp/nsi
# Clean up
	rm -Rf tmp

package-beos:
# Check that tmp isn't in the way
	@if test -e tmp; then \
		echo "Error: please remove ./tmp, it is in the way"; false; \
	else \
		echo "OK."; mkdir tmp; \
	fi

# Copy relevant files
	mkdir -p tmp/vlc
	cp vlc AUTHORS COPYING ChangeLog README THANKS NEWS tmp/vlc/
# We cannot use ${PLUGIN_FILES}, it looks like it is too long for the BeOS shell...
	if [ `find modules -name "lib*_plugin.so" | wc -l` -gt 0 ]; then \
		mkdir -p tmp/vlc/plugins; \
		find modules -name "lib*_plugin.so" | xargs -i{} cp {} tmp/vlc/plugins/; \
	fi
	for i in $(ALL_LINGUAS); do \
		mkdir -p tmp/vlc/locale/$${i}/LC_MESSAGES ; \
		cp po/$${i}.gmo tmp/vlc/locale/$${i}/LC_MESSAGES/vlc.mo || true ; \
	done
	mkdir -p tmp/vlc/share
	cp share/*.rle tmp/vlc/share/

# Create package with debug symbols
	mimeset -f tmp/vlc/* tmp/vlc/*/*
	mv tmp/vlc tmp/vlc-${VERSION}
	(cd tmp ; zip -9 -r vlc-${VERSION}-BeOS.zip vlc-${VERSION} )
	mv tmp/vlc-${VERSION}-BeOS.zip .

# Create package without debug symbols
	strip tmp/vlc-${VERSION}/vlc tmp/vlc-${VERSION}/plugins/lib*_plugin.so
	xres -o tmp/vlc-${VERSION}/vlc share/vlc_beos.rsrc
	mimeset -f tmp/vlc-${VERSION}/* tmp/vlc-${VERSION}/*/*
	(cd tmp ; zip -9 -r vlc-${VERSION}-BeOS-stripped.zip vlc-${VERSION} )
	mv tmp/vlc-${VERSION}-BeOS-stripped.zip .

# Clean up
	rm -Rf tmp

package-macosx:
# Check that tmp isn't in the way
	@if test -e tmp; then \
		echo "Error: please remove ./tmp, it is in the way"; false; \
	else \
		echo "OK."; mkdir tmp; \
	fi

# Copy relevant files 
	cp -R VLC.app tmp/
	cp AUTHORS COPYING ChangeLog README README.MacOSX.rtf THANKS NEWS tmp/

# Create disk image 
	./extras/MacOSX/macosx-dmg 18 "vlc-${VERSION}" tmp/* 

# Clean up
	rm -Rf tmp

###############################################################################
# Building the Mozilla plugin
###############################################################################

noinst_LIBRARIES += $(noinst_LIBRARIES_mozilla)

MOSTLYCLEANFILES += $(LIBRARIES_mozilla)

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
mozilla_libplugin_a_CFLAGS = $(CPPFLAGS_pic) $(CFLAGS_pic) \
                             $(CPPFLAGS_mozilla) $(CFLAGS_mozilla) \
                             $(CPPFLAGS_mozilla_EXTRA)
mozilla_libplugin_a_CXXFLAGS = $(CPPFLAGS_pic) $(CXXFLAGS_pic) \
                               $(CPPFLAGS_mozilla) $(CXXFLAGS_mozilla) \
                               $(CPPFLAGS_mozilla_EXTRA)
mozilla_libplugin_a_DEPENDENCIES = lib/libvlc_pic.a $(L_builtin_pic) $(DATA_npvlc_rc)

BUILT_SOURCES_mozilla = mozilla/vlcintf.h
$(SOURCES_mozilla): mozilla/vlcintf.h

mozilla_plugin_DATA = $(LIBRARIES_mozilla)
mozilla_plugindir = $(libdir)/mozilla/plugins
$(LIBRARIES_mozilla): $(mozilla_libplugin_a_OBJECTS) \
                      $(mozilla_libplugin_a_DEPENDENCIES) \
                      $(L_builtin_pic)
	$(CXXLINK) $(mozilla_libplugin_a_OBJECTS) $(DATA_npvlc_rc) \
		lib/libvlc_pic.a $(L_builtin_pic) -shared $(LDFLAGS) \
		$(LDFLAGS_vlc) $(LDFLAGS_mozilla) $(LDFLAGS_builtin_pic)

mozilla_vlcintf_xpt_DATA = mozilla/vlcintf.xpt
mozilla_vlcintf_xptdir = $(libdir)/mozilla/components
mozilla/vlcintf.xpt: Makefile mozilla/vlcintf.idl
	$(XPIDL) -I/usr/share/idl/mozilla -m typelib \
		-o mozilla/vlcintf mozilla/vlcintf.idl

mozilla/vlcintf.h: Makefile mozilla/vlcintf.idl
	$(XPIDL) -I/usr/share/idl/mozilla -m header \
		-o mozilla/vlcintf mozilla/vlcintf.idl

if HAVE_WIN32
DATA_npvlc_rc = $(noinst_mozilla_npvlc_rc_DATA)
noinst_mozilla_npvlc_rc_DATA = mozilla/npvlc_rc.$(OBJEXT)
noinst_mozilla_npvlc_rcdir = $(libdir)
mozilla/npvlc_rc.$(OBJEXT): mozilla/npvlc_rc.rc
	$(WINDRES) --include-dir mozilla -i $< -o $@
endif
endif

###############################################################################
# Modules
###############################################################################
MOSTLYCLEANFILES += $(PLUGIN_FILES)
PLUGIN_FILES =

libvlcdir = $(libdir)/vlc

include Modules.am

###############################################################################
# Force rule
###############################################################################
FORCE:
