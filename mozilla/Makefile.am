###############################################################################
# Building the Mozilla plugin
###############################################################################

noinst_LIBRARIES = $(noinst_LIBRARIES_mozilla)

MOSTLYCLEANFILES = $(npvlc_DATA) $(vlcintf_xpt_DATA)
CLEANFILES = stamp-pic $(BUILT_SOURCES)
EXTRA_DIST = $(DIST_sources) vlcintf.idl npvlc_rc.rc vlc.r

SOURCES_mozilla_common = \
	vlcshell.cpp \
	vlcplugin.cpp \
	vlcplugin.h \
	vlcpeer.cpp \
	vlcpeer.h \
	support/classinfo.h

DIST_sources = $(SOURCES_mozilla_common) \
	support/npwin.cpp support/npmac.cpp support/npunix.c

if BUILD_SHARED
LIBRARIES_libvlc_pic = -Wl,-rpath '$(libdir)' -L$(top_builddir)/src -lvlc
LIBRARIES_libvlc_nopic = $(LIBRARIES_libvlc_pic)
else
LIBRARIES_libvlc_pic = $(top_builddir)/src/libvlc_pic.a
LIBRARIES_libvlc_nopic = $(top_builddir)/src/libvlc.a
endif

if BUILD_MOZILLA

# Under Win32, Mozilla plugins need to be named NP******.DLL, but under Unix
# the common naming scheme is lib******plugin.so. Also, we need npwin.cpp
# under Win32 and npunix.c under Unix.
if HAVE_WIN32

LIBRARIES_libvlc = $(LIBRARIES_libvlc_nopic)
npvlc = npvlc$(LIBEXT)
npvlcdir = $(libdir)
noinst_DATA = npvlc_rc.$(OBJEXT)

SOURCES_support = support/npwin.cpp
CPPFLAGS_mozilla_EXTRA = -DXP_WIN -DXP_WIN32
LDFLAGS_npvlc = -shared $(LIBRARIES_libvlc)

DATA_npvlc_rc = $(noinst_npvlc_rc_DATA)
npvlc_rc.$(OBJEXT): npvlc_rc.rc
	$(WINDRES) -DVERSION=$(VERSION) \
		-DVERSION_NUMBER=`echo $(VERSION).0.0.0 | sed 's/\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\).*/\1,\2,\3,\4/'` \
		--include-dir $(srcdir) -i $< -o $@

else
if HAVE_DARWIN

LIBRARIES_libvlc = $(LIBRARIES_libvlc_nopic)
npvlc = npvlc$(LIBEXT)
npvlcdir = $(libdir)
noinst_DATA = npvlc.rsrc VLC\ Plugin.plugin
MOSTLYCLEANFILES += npvlc.rsrc
CLEANFILES += VLC\ Plugin.plugin

SOURCES_support = support/npmac.cpp
CPPFLAGS_mozilla_EXTRA = -I. -I$(top_builddir) -I$(srcdir)/../include -c \
	-F/System/Library/Frameworks/CoreFoundation.framework $(moz_CFLAGS) \
	-I/Developer/Headers/FlatCarbon -arch ppc -fno-common -fpascal-strings \
	-O0 -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -DXP_MACOSX=1 \
	-DNO_X11=1 -DUSE_SYSTEM_CONSOLE=1 -pipe -fmessage-length=0 -g \
	-include mozilla-config.h
LDFLAGS_npvlc = -arch ppc -bundle -read_only_relocs suppress \
	$(LIBRARIES_libvlc) -dylib

npvlc.rsrc: $(srcdir)/vlc.r
	/Developer/Tools/Rez -useDF /Developer/Headers/FlatCarbon/Types.r $< -o $@

VLC\ Plugin.plugin: npvlc.rsrc npvlc.dylib
	rm -rf "$@"
	mkdir -p "./$@/Contents/MacOS"
	cp npvlc.dylib "./$@/Contents/MacOS/VLC Plugin"
	mkdir -p ./"$@"/Contents/Resources
	cp npvlc.rsrc "./$@/Contents/Resources/VLC Plugin.rsrc"
	cp -r $(top_srcdir)/extras/MacOSX/plugin/English.lproj "./$@/Contents/Resources/"
	cp $(top_srcdir)/extras/MacOSX/plugin/Info.plist "./$@/Contents/Info.plist"
	(cd $(top_builddir)/VLC.app/Contents/MacOS/; tar cf - modules)| \
	(cd "./$@/Contents/MacOS"; tar xf -)

else

LIBRARIES_libvlc = $(LIBRARIES_libvlc_pic)
npvlc = libvlcplugin$(LIBEXT)
npvlcdir = $(libdir)/mozilla/plugins
SOURCES_support = support/npunix.c
noinst_DATA =

LDFLAGS_npvlc = -shared $(LIBRARIES_libvlc)
endif
endif

noinst_LIBRARIES_mozilla = libnpvlc.a

$(SOURCES_mozilla): vlcintf.h

BUILT_SOURCES = vlcintf.h
vlcintf_xpt_DATA = vlcintf.xpt

if USE_LIBTOOL
# FIXME: name is incorrect on Win32 & Darwin
npvlc_LTLIBRARIES = libvlcplugin.la
else
npvlc_DATA = $(npvlc)
EXTRA_LIBRARIES = libnpvlc.a
endif
endif

libvlcplugin_la_SOURCES = $(SOURCES_mozilla_common) $(SOURCES_support)
libvlcplugin_la_CFLAGS = `$(VLC_CONFIG) --cflags mozilla`
libvlcplugin_la_CXXFLAGS = `$(VLC_CONFIG) --cxxflags mozilla`
libvlcplugin_la_LDFLAGS = `$(VLC_CONFIG) --libs mozilla` -module -shrext $(LIBEXT)
libvlcplugin_la_LIBADD = ../src/libvlc.la

libnpvlc_a_SOURCES = $(SOURCES_mozilla_common) $(SOURCES_support)
libnpvlc_a_CFLAGS = `$(VLC_CONFIG) --cflags mozilla $(pic)` \
	             $(CPPFLAGS_mozilla_EXTRA)
libnpvlc_a_CXXFLAGS = `$(VLC_CONFIG) --cxxflags mozilla $(pic)` \
	               $(CPPFLAGS_mozilla_EXTRA)
LDFLAGS_libnpvlc = $(LDFLAGS_npvlc) `$(VLC_CONFIG) --libs mozilla vlc builtin $(pic)`
libnpvlc_a_DEPENDENCIES = $(DATA_npvlc_rc)

$(npvlc): $(libnpvlc_a_OBJECTS) $(libnpvlc_a_DEPENDENCIES) stamp-pic
	$(CXXLINK) $(libnpvlc_a_OBJECTS) $(DATA_npvlc_rc) \
	 $(LDFLAGS_libnpvlc)

vlcintf_xptdir = $(libdir)/mozilla/components
vlcintf.xpt: vlcintf.idl
	$(XPIDL) $(XPIDL_INCL) \
	  -m typelib -o vlcintf $(srcdir)/vlcintf.idl

vlcintf.h: vlcintf.idl
	$(XPIDL) $(XPIDL_INCL) \
	  -m header -o vlcintf $(srcdir)/vlcintf.idl

###############################################################################
# Stamp rules
###############################################################################
stamp-pic:
	@for dep in "" `$(VLC_CONFIG) --target builtin $(pic)`; do \
	  if test "$${dep}" -nt "$(LIBRARIES_mozilla)"; then \
	    rm -f $@; \
	    break; \
	  fi; \
	done
	@if test ! -f $@; then printf "" > $@; fi

###############################################################################
# Force rule
###############################################################################
.PHONY: stamp-pic
