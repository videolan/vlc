###############################################################################
# Automake targets and declarations
###############################################################################

# SUBDIRS stores the directories where a "make" is required when building
# something. DIST_SUBDIRS stores the directories where nothing is built but
# which have makefiles with distribution information.
SUBDIRS = po intl m4 share
DIST_SUBDIRS = $(SUBDIRS) modules src debian doc ipkg lib

EXTRA_DIST = BUGS FAQ HACKING MAINTAINERS STATUS \
             INSTALL.win32 README.MacOSX.rtf vlc.spec install-win32 \
             Modules.am macosx-dmg \
             configure.ac.in mkinstalldirs bootstrap
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
AUTOMAKE_OPTIONS = foreign dist-bzip2 subdir-objects

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
CPPFLAGS_debug = -DDEBUG -DHAVE_RELEASE
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
	src/misc/modules_plugin.h

pkgincludedir = $(includedir)/vlc

dist_pkginclude_HEADERS = \
	include/vlc/vlc.h \
	include/vlc/aout.h \
	include/vlc/vout.h \
	include/vlc/sout.h \
	include/vlc/decoder.h \
	include/vlc/input.h \
	include/vlc/intf.h

noinst_HEADERS += $(HEADERS_include) $(HEADERS_include_built)

HEADERS_include = \
	include/aout_internal.h \
	include/audio_output.h \
	include/beos_specific.h \
	include/configuration.h \
	include/darwin_specific.h \
	include/input_ext-dec.h \
	include/input_ext-intf.h \
	include/input_ext-plugins.h \
	include/interface.h \
	include/intf_eject.h \
	include/iso_lang.h \
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
	include/win32_specific.h

HEADERS_include_built = \
	include/vlc_symbols.h

include/vlc_symbols.h: Makefile $(HEADERS_include)
	rm -f $@.in
	echo '/* DO NOT EDIT THIS FILE! See Makefile.am */' >> $@.in
	echo 'struct module_symbols_t {' >> $@.in
	cat $(HEADERS_include) | grep '^ *VLC_EXPORT.*;' | sed -e 's/VLC_EXPORT( *\([^,]*\), *\([^,]*\), *\(.*\));.*/    \1 (* \2_inner) \3;/' >> $@.in
	echo '};' >> $@.in
	echo '#ifdef __PLUGIN__' >> $@.in
	cat $(HEADERS_include) | grep '^ *VLC_EXPORT.*;' | sed -e 's/VLC_EXPORT( *\([^,]*\), *\([^,]*\), *\(.*\));.*/#   define \2 p_symbols->\2_inner/' >> $@.in
	echo '#endif /* __PLUGIN__ */' >> $@.in
	mv -f $@.in $@

src/misc/modules_plugin.h: Makefile src/misc/modules_plugin.h.in $(HEADERS_include)
	rm -f $@.tmp && cp $@.in $@.tmp
	sed -e 's#.*\$[I][d]:.*# * Automatically generated from '$@'.in by bootstrap#' < $@.in > $@.tmp
	echo '#define STORE_SYMBOLS( p_symbols ) \' >> $@.tmp
	cat $(HEADERS_include) | grep '^ *VLC_EXPORT.*;' | sed -e 's/VLC_EXPORT( *\([^,]*\), *\([^,]*\), *\(.*\));.*/    (p_symbols)->\2_inner = \2; \\/' >> $@.tmp
	echo '' >> $@.tmp
	mv -f $@.tmp $@

src/misc/modules_builtin.h: Makefile src/misc/modules_builtin.h.in
	rm -f $@.tmp && cp $@.in $@.tmp
if HAVE_BUILTINS
	for i in $(BUILTINS) ; do echo "int vlc_entry__"`echo $$i | sed -e 'y@/@_@' -e 's@\..*@@'`"( module_t* );" >>$@.tmp; done
	echo "" >> $@.tmp
endif
	echo "#define ALLOCATE_ALL_BUILTINS() \\" >> $@.tmp
	echo "    do \\" >> $@.tmp
	echo "    { \\" >> $@.tmp
if HAVE_BUILTINS
	for i in $(BUILTINS) ; do echo "        ALLOCATE_BUILTIN("`echo $$i | sed -e 'y@/@_@' -e 's@\..*@@'`"); \\" >> $@.tmp ; done
endif
	echo "    } while( 0 );" >> $@.tmp
	echo "" >> $@.tmp
	mv -f $@.tmp $@

# These dependencies are mandatory
$(SOURCES): include/vlc_symbols.h
$(SOURCES_libvlc): src/misc/modules_plugin.h src/misc/modules_builtin.h

###############################################################################
# Optional getopt
###############################################################################

EXTRA_DIST += \
	extras/GNUgetopt/COPYING \
	extras/GNUgetopt/getopt.c \
	extras/GNUgetopt/getopt.h \
	extras/GNUgetopt/getopt1.c

if BUILD_GETOPT
SOURCES_libgetopt = extras/GNUgetopt/getopt.c extras/GNUgetopt/getopt1.c
endif

###############################################################################
# MacOS X project
###############################################################################

EXTRA_DIST += \
	extras/MacOSX/Resources/English.lproj/MainMenu.nib/classes.nib \
	extras/MacOSX/Resources/English.lproj/MainMenu.nib/info.nib \
	extras/MacOSX/Resources/English.lproj/MainMenu.nib/objects.nib \
	extras/MacOSX/Resources/English.lproj/InfoPlist.strings \
	extras/MacOSX/Resources/divx.icns \
	extras/MacOSX/Resources/generic.icns \
	extras/MacOSX/Resources/mpeg.icns \
	extras/MacOSX/Resources/mpeg1.icns \
	extras/MacOSX/Resources/mpeg2.icns \
	extras/MacOSX/Resources/mpeg4.icns \
	extras/MacOSX/Resources/vlc.icns \
	extras/MacOSX/Resources/pause.png \
	extras/MacOSX/Resources/play.png \
	extras/MacOSX/Resources/stop.png \
	extras/MacOSX/Resources/stepf.png \
	extras/MacOSX/Resources/stepr.png \
	extras/MacOSX/vlc.pbproj/project.pbxproj

###############################################################################
# Building libvlc
###############################################################################

bin_SCRIPTS = vlc-config

lib_LIBRARIES += lib/libvlc.a $(LIBRARIES_libvlc_pic)

lib_libvlc_a_SOURCES = $(SOURCES_libvlc)
lib_libvlc_a_CFLAGS = $(CPPFLAGS_default) -D__VLC__ $(CFLAGS_default) @CFLAGS_vlc@
lib_libvlc_a_CXXFLAGS = $(CPPFLAGS_default) -D__VLC__ $(CXXFLAGS_default)

lib_libvlc_pic_a_SOURCES = $(SOURCES_libvlc)
lib_libvlc_pic_a_CFLAGS = $(CPPFLAGS_pic) -D__VLC__ $(CFLAGS_pic) @CFLAGS_vlc@
lib_libvlc_pic_a_CXXFLAGS = $(CPPFLAGS_pic) -D__VLC__ $(CXXFLAGS_pic)

if HAVE_BEOS
SOURCES_libvlc_beos = src/misc/beos_specific.cpp
endif
if HAVE_DARWIN
SOURCES_libvlc_darwin = src/misc/darwin_specific.c
endif
if HAVE_WIN32
SOURCES_libvlc_win32 = src/misc/win32_specific.c
endif
if BUILD_MOZILLA
LIBRARIES_libvlc_pic = lib/libvlc_pic.a
endif

EXTRA_DIST += \
	src/misc/beos_specific.cpp \
	src/misc/darwin_specific.c \
	src/misc/win32_specific.c

SOURCES_libvlc = \
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
	src/misc/iso-639.def \
	src/misc/messages.c \
	src/misc/objects.c \
	src/misc/variables.c \
	src/misc/extras.c \
	$(SOURCES_libvlc_win32) \
	$(SOURCES_libvlc_beos) \
	$(SOURCES_libvlc_darwin)

###############################################################################
# Building vlc
###############################################################################

bin_PROGRAMS += vlc

vlc_SOURCES = src/vlc.c $(SOURCES_libgetopt)

# @AUTOMAKE_SUCKS@ gets expanded to $(L_builtin) $(LDFLAGS_builtin)
# but we don't write it directly, otherwise automake will go amok and eat all
# the memory because of its 2^N crap algorithm. So we fool him. Nuahaha.
vlc_LDADD = lib/libvlc.a $(LDFLAGS_vlc) $(DATA_win32_rc) @AUTOMAKE_SUCKS@
vlc_CFLAGS = $(CPPFLAGS_default) $(CFLAGS_default)

# We use DEPENDENCIES_vlc instead of vlc_DEPENDENCIES because of an
# old automake-1.5 bug (automake/279).
DEPENDENCIES_vlc = lib/libvlc.a $(L_builtin) $(DATA_win32_rc)

vlc$(EXEEXT): $(vlc_OBJECTS) $(DEPENDENCIES_vlc)
	@rm -f vlc$(EXEEXT)
	$(LINK) $(vlc_LDFLAGS) $(vlc_OBJECTS) $(vlc_LDADD) $(LIBS)
if HAVE_BEOS
	xres -o $@ ./share/vlc_beos.rsrc
	mimeset -f $@
endif

# Install the symlinks
install-exec-local:
	for i in dummy $(ALIASES) ; do if test "x$$i" != "xdummy" ; then \
		rm -f $(DESTDIR)$(bindir)/$$i && \
		ln -s vlc $(DESTDIR)$(bindir)/$$i ; \
	fi ; done

if HAVE_DARWIN
# Create the MacOS X app
vlc_app_DATA = vlc.app
vlc_appdir = $(bindir)
vlc.app: vlc
	rm -Rf vlc.app
	cd extras/MacOSX ; pbxbuild | grep -v '^[ \t]' | grep -v "^$$"
	cp -r extras/MacOSX/build/vlc.bundle ./vlc.app
	$(INSTALL) -d vlc.app/Contents/MacOS/share
	$(INSTALL) -d vlc.app/Contents/MacOS/modules
	$(INSTALL) vlc vlc.app/Contents/MacOS/
#	FIXME: install modules ! (and add dependency on modules)
	$(INSTALL) -m 644 share/*.psf vlc.app/Contents/MacOS/share
endif

if HAVE_WIN32
DATA_win32_rc = $(noinst_share_vlc_win32_rc_DATA)
noinst_share_vlc_win32_rc_DATA = share/vlc_win32_rc.$(OBJEXT)
noinst_share_vlc_win32_rcdir = $(libdir)
share/vlc_win32_rc.$(OBJEXT): share/vlc_win32_rc.rc
	$(WINDRES) -i $< -o $@
endif

###############################################################################
# Building the Mozilla plugin
###############################################################################

noinst_LIBRARIES += $(noinst_LIBRARIES_mozilla)

MOSTLYCLEANFILES += mozilla/libvlcplugin$(LIBEXT)

EXTRA_DIST += $(SOURCES_mozilla) mozilla/vlcintf.idl

BUILT_SOURCES += $(BUILT_SOURCES_mozilla)

SOURCES_mozilla = \
	mozilla/vlcshell.cpp \
	mozilla/vlcplugin.cpp \
	mozilla/vlcplugin.h \
	mozilla/vlcpeer.cpp \
	mozilla/vlcpeer.h \
	mozilla/classinfo.h \
	mozilla/npunix.c

if BUILD_MOZILLA
noinst_LIBRARIES_mozilla = mozilla/libvlcplugin.a

mozilla_libvlcplugin_a_SOURCES = $(SOURCES_mozilla) $(BUILT_SOURCES_mozilla) \
                                 $(SOURCES_libgetopt)
mozilla_libvlcplugin_a_CFLAGS = $(CPPFLAGS_pic) $(CFLAGS_pic) \
                                $(CPPFLAGS_mozilla) $(CFLAGS_mozilla)
mozilla_libvlcplugin_a_CXXFLAGS = $(CPPFLAGS_pic) $(CXXFLAGS_pic) \
                                  $(CPPFLAGS_mozilla) $(CXXFLAGS_mozilla)
mozilla_libvlcplugin_a_DEPENDENCIES = lib/libvlc_pic.a $(L_builtin_pic)

BUILT_SOURCES_mozilla = mozilla/vlcintf.h
$(SOURCES_mozilla): mozilla/vlcintf.h

mozilla_libvlcplugin_DATA = mozilla/libvlcplugin$(LIBEXT)
mozilla_libvlcplugindir = $(libdir)/mozilla/plugins
mozilla/libvlcplugin$(LIBEXT): mozilla/libvlcplugin.a $(L_builtin_pic)
	$(CC) -o $@ $(mozilla_libvlcplugin_a_OBJECTS) lib/libvlc_pic.a \
		$(L_builtin_pic) -shared $(LDFLAGS) $(LDFLAGS_vlc) \
		$(LDFLAGS_mozilla) $(LDFLAGS_builtin_pic)

mozilla_vlcintf_xpt_DATA = mozilla/vlcintf.xpt
mozilla_vlcintf_xptdir = $(libdir)/mozilla/components
mozilla/vlcintf.xpt: Makefile mozilla/vlcintf.idl
	$(XPIDL) -I/usr/share/idl/mozilla -m typelib \
		-o mozilla/vlcintf mozilla/vlcintf.idl

mozilla/vlcintf.h: Makefile mozilla/vlcintf.idl
	$(XPIDL) -I/usr/share/idl/mozilla -m header \
		-o mozilla/vlcintf mozilla/vlcintf.idl
endif

###############################################################################
# Modules
###############################################################################
libvlcdir = $(libdir)/vlc

include Modules.am

