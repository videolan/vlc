###############################################################################
# Building the Mozilla plugin
###############################################################################

noinst_LIBRARIES = $(noinst_LIBRARIES_mozilla)
	                                     
MOSTLYCLEANFILES = $(LIBRARIES_mozilla)
EXTRA_DIST = $(SOURCES_mozilla) vlcintf.idl
BUILT_SOURCES = $(BUILT_SOURCES_mozilla)

SOURCES_mozilla = \
	vlcshell.cpp \
	vlcplugin.cpp \
	vlcplugin.h \
	vlcpeer.cpp \
	vlcpeer.h \
	support/classinfo.h \
	$(SOURCES_win32) \
	$(SOURCES_macosx) \
	$(SOURCES_unix) \
	$(NULL)

# Under Win32, Mozilla plugins need to be named NP******.DLL, but under Unix
# the common naming scheme is lib******plugin.so. Also, we need npwin.cpp
# under Win32 and npunix.c under Unix.
if HAVE_WIN32
LIBRARIES_mozilla = npvlc$(LIBEXT)
SOURCES_win32 = support/npwin.cpp
CPPFLAGS_mozilla_EXTRA = -DXP_WIN -DXP_WIN32
else
if HAVE_DARWIN
# We don't define LIBRARIES_mozilla because we'll be using project builder
SOURCES_macosx = support/npmac.cpp
else
LIBRARIES_mozilla = libvlcplugin$(LIBEXT)
SOURCES_unix = support/npunix.c
endif
endif

if BUILD_MOZILLA
if UNTRUE
noinst_LIBRARIES_mozilla = libplugin.a
endif

libplugin_a_SOURCES = $(SOURCES_mozilla)
libplugin_a_CFLAGS = `$(VLC_CONFIG) --cflags mozilla pic` \
	             $(CPPFLAGS_mozilla_EXTRA) -Imozilla
libplugin_a_CXXFLAGS = `$(VLC_CONFIG) --cxxflags mozilla pic` \
	               $(CPPFLAGS_mozilla_EXTRA) -Imozilla
libplugin_a_DEPENDENCIES = $(DATA_npvlc_rc)

BUILT_SOURCES_mozilla = vlcintf.h
$(SOURCES_mozilla): vlcintf.h

plugin_DATA = $(LIBRARIES_mozilla)
plugindir = $(libdir)/mozilla/plugins
$(LIBRARIES_mozilla): $(libplugin_a_OBJECTS) \
	              $(libplugin_a_DEPENDENCIES) $(top_builddir)/stamp-pic
	$(CXXLINK) $(libplugin_a_OBJECTS) $(DATA_npvlc_rc) \
	  $(top_builddir)/lib/libvlc_pic.a -shared \
	  `$(VLC_CONFIG) --libs vlc mozilla builtin pic`

vlcintf_xpt_DATA = vlcintf.xpt
vlcintf_xptdir = $(libdir)/mozilla/components
vlcintf.xpt: vlcintf.idl
	$(XPIDL) -I/usr/share/idl/mozilla -I/usr/lib/mozilla/include/idl \
	  -m typelib -o vlcintf $(srcdir)/vlcintf.idl

vlcintf.h: vlcintf.idl
	$(XPIDL) -I/usr/share/idl/mozilla -I/usr/lib/mozilla/include/idl \
	  -m header -o vlcintf $(srcdir)/vlcintf.idl

if HAVE_WIN32
DATA_npvlc_rc = $(noinst_npvlc_rc_DATA)
noinst_npvlc_rc_DATA = npvlc_rc.$(OBJEXT)
noinst_npvlc_rcdir = $(libdir)
npvlc_rc.$(OBJEXT): npvlc_rc.rc
	$(WINDRES) --include-dir $(srcdir)/mozilla -i $< -o $@
endif
endif

###############################################################################
# Stamp rules
###############################################################################
clean: clean-stamp
clean-stamp:
	rm -f stamp-pic

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
