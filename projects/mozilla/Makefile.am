###############################################################################
# Building the Mozilla plugin
###############################################################################

MOSTLYCLEANFILES = 
CLEANFILES = $(BUILT_SOURCES)
EXTRA_DIST = $(DIST_sources) npvlc_rc.rc.in COPYING npvlc.dll.manifest

SOURCES_mozilla_common = \
	vlcshell.cpp \
	vlcshell.h \
	vlcplugin.cpp \
	vlcplugin.h \
	control/npolibvlc.cpp \
	control/npolibvlc.h \
	control/nporuntime.cpp \
	control/nporuntime.h \
	support/classinfo.h

DIST_sources = $(SOURCES_mozilla_common) \
	support/npwin.cpp support/npmac.cpp support/npunix.c

if BUILD_MOZILLA

LIBRARIES_libvlc = $(top_builddir)/src/libvlc.la \
				   $(top_builddir)/src/libvlccore.la

if HAVE_WIN32

# Under Win32, Mozilla plugins need to be named NP******.DLL, but under Unix
# the common naming scheme is lib******plugin.so. Also, we need npwin.cpp
# under Win32 and npunix.c under Unix.
#
lib_LTLIBRARIES = npvlc.la

SOURCES_support = support/npwin.cpp
CPPFLAGS_mozilla_EXTRA = -DXP_WIN -DXP_WIN32
LDFLAGS_mozilla_EXTRA = -no-undefined -Wl,--kill-at -Wl,$(DATA_npvlc_rc)

npvlc_la_SOURCES = $(SOURCES_mozilla_common) $(SOURCES_support)
npvlc_la_CFLAGS = `$(VLC_CONFIG) --cflags mozilla` $(CPPFLAGS_mozilla_extra)
npvlc_la_CXXFLAGS = `$(VLC_CONFIG) --cxxflags mozilla` $(CPPFLAGS_mozilla_extra)
npvlc_la_DEPENDENCIES = $(DATA_npvlc_rc) $(LIBRARIES_libvlc)
npvlc_la_LDFLAGS = `$(VLC_CONFIG) --ldflags mozilla` -module -avoid-version \
				   $(LDFLAGS_mozilla_EXTRA)
npvlc_la_LIBADD = `$(VLC_CONFIG) -libs mozilla` $(LIBRARIES_libvlc)

DATA_npvlc_rc = $(noinst_npvlc_rc_DATA)
noinst_npvlc_rc_DATA = npvlc_rc.$(OBJEXT)
noinst_npvlc_rcdir =

npvlc_rc.$(OBJEXT): npvlc_rc.rc
	$(WINDRES) --include-dir $(srcdir) -i $< -o $@

else
if HAVE_DARWIN

#
# MacOS X
#

lib_LTLIBRARIES = npvlc.la

SOURCES_support = support/npmac.cpp
CPPFLAGS_mozilla_EXTRA = -I. -I$(top_builddir) -I$(srcdir)/../include -c \
	-F/System/Library/Frameworks/CoreFoundation.framework $(moz_CFLAGS) \
	-I/Developer/Headers/FlatCarbon -fno-common -fpascal-strings \
	-Wmost -Wno-four-char-constants -Wno-unknown-pragmas -DXP_MACOSX=1 \
	-DNO_X11=1 -DUSE_SYSTEM_CONSOLE=1 -pipe -fmessage-length=0 \
	-include mozilla-config.h
LDFLAGS_mozilla_EXTRA = -no-undefined -bundle -Wl,-read_only_relocs -Wl,suppress \
	-Wl,-headerpad_max_install_names -shrext $(LIBEXT) -Wl,-framework,Carbon -Wl,-framework,System

npvlc_la_SOURCES = $(SOURCES_mozilla_common) $(SOURCES_support)
npvlc_la_CFLAGS = `$(VLC_CONFIG) --cflags mozilla` $(CPPFLAGS_mozilla_EXTRA)
npvlc_la_CXXFLAGS = `$(VLC_CONFIG) --cxxflags mozilla` $(CPPFLAGS_mozilla_EXTRA) 
npvlc_la_DEPENDENCIES = $(LIBRARIES_libvlc)
npvlc_la_LDFLAGS = `$(VLC_CONFIG) --ldflags mozilla` -module -avoid-version \
				   $(LDFLAGS_mozilla_EXTRA)
npvlc_la_LIBADD = `$(VLC_CONFIG) -libs mozilla` $(LIBRARIES_libvlc)


noinst_DATA = npvlc.rsrc VLC\ Plugin.plugin
MOSTLYCLEANFILES += npvlc.rsrc
CLEANFILES += VLC\ Plugin.plugin

npvlc.rsrc: vlc.r
	/Developer/Tools/Rez -useDF /Developer/Headers/FlatCarbon/Types.r $< -o $@

#
# Plugin uses shared libraries that are located relatively through @executable_path,
# which unfortunately references the path of the App using the Plugin, rather than the
# Plugin itself. Since this Plugin should always be installed in '/Library/Internet Plug-Ins',
# it is safer to force dylibs to locate dependents through a fixed path
#
define FIXEXECPATH
	otool -L "$$dylib" | \
	awk -v libdylib="$$dylib" ' \
	/@executable_path/ { \
	    newpath=$$1 ; \
	    sub("@executable_path","/Library/Internet Plug-Ins/VLC Plugin.plugin/Contents/MacOS",newpath) ; \
	    print "install_name_tool -change \""$$1"\" \""newpath"\" \""libdylib"\"" ; \
	}' | sh -x
endef

VLC\ Plugin.plugin: npvlc.rsrc $(lib_LTLIBRARIES)
	rm -Rf "$@"
	$(INSTALL) -d "VLC-Plugin.plugin/Contents/MacOS/lib"
	$(INSTALL) -d "VLC-Plugin.plugin/Contents/Resources"
	ACTION="release-makefile" PRODUCT="VLC-Plugin.plugin" src_dir=$(srcdir) build_dir=$(top_builddir) sh "$(top_srcdir)/projects/macosx/framework/Pre-Compile.sh"
	$(INSTALL) npvlc.rsrc "VLC-Plugin.plugin/Contents/Resources/VLC Plugin.rsrc"
	$(INSTALL) "$(top_builddir)/extras/package/macosx/plugin/Info.plist" "VLC-Plugin.plugin/Contents/Info.plist"
	mv "VLC-Plugin.plugin" "VLC Plugin.plugin"
	find "VLC Plugin.plugin" -type d -exec chmod ugo+rx '{}' \;
	find "VLC Plugin.plugin" -type f -exec chmod ugo+r '{}' \;

# uncomment if dependencies on XPCOM libs is sought
#	if test -d "$(MOZILLA_SDK_PATH)/lib"; then \
#	  for i in "$(MOZILLA_SDK_PATH)"/lib/*.dylib ; do \
#	    dylib="$(srcdir)/$@/Contents/MacOS/`basename $${i}`" ; \
#	    $(INSTALL) -m 644 "$${i}" "$$dylib" ; \
#	    $(FIXEXECPATH); \
#	  done ; \
#	fi

else

#
# UNIX/Others platforms
#

CPPFLAGS_mozilla_EXTRA = -DDATA_PATH=\"$(pkgdatadir)\"

npvlc_LTLIBRARIES = libvlcplugin.la

npvlc = libvlcplugin$(LIBEXT)
npvlcdir = $(libdir)/mozilla/plugins
SOURCES_support = support/npunix.c

libvlcplugin_la_SOURCES = $(SOURCES_mozilla_common) $(SOURCES_support)
libvlcplugin_la_CFLAGS = `$(VLC_CONFIG) --cflags mozilla` $(CPPFLAGS_mozilla_EXTRA)
libvlcplugin_la_CXXFLAGS = `$(VLC_CONFIG) --cxxflags mozilla` $(CPPFLAGS_mozilla_EXTRA)
libvlcplugin_la_LDFLAGS = `$(VLC_CONFIG) --ldflags mozilla` -module -avoid-version -shrext $(LIBEXT)
libvlcplugin_la_LIBADD = $(LIBRARIES_libvlc) `$(VLC_CONFIG) -libs mozilla`
# automake gets confused by the ../..
libvlcplugin_la_DEPENDENCIES = $(AM_LIBADD) 
endif
endif
endif

