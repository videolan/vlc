###############################################################################
# Building the Mozilla plugin
###############################################################################

EXTRA_LIBRARIES = libplugin.a
	                                     
MOSTLYCLEANFILES = $(LIBRARIES_mozilla)
EXTRA_DIST = $(SOURCES_mozilla) $(DIST_rsrc) vlcintf.idl
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

DIST_rsrc = \
	npvlc_rc.rc \
	$(NULL)

# Under Win32, Mozilla plugins need to be named NP******.DLL, but under Unix
# the common naming scheme is lib******plugin.so. Also, we need npwin.cpp
# under Win32 and npunix.c under Unix.
if HAVE_WIN32
LIBRARIES_mozilla = npvlc$(LIBEXT)
SOURCES_win32 = support/npwin.cpp
CPPFLAGS_mozilla_EXTRA = -DXP_WIN -DXP_WIN32
LIBRARIES_libvlc = $(top_builddir)/lib/libvlc.a
else
if HAVE_DARWIN
# We don't define LIBRARIES_mozilla because we'll be using project builder, or not...
BUNDLE_mozilla = VLC\ Plugin.plugin
vlc_moz_FLAGS = `$(VLC_CONFIG) --libs vlc builtin pic mozilla`
moz_CFLAGS = `$(MOZILLA_CONFIG) --cflags plugin java nspr/obsolete nspr oji xpcom xpconnect`
moz_plugin_FLAGS = -I. -I$(top_builddir) -I$(srcdir)/../include -c -F/System/Library/Frameworks/CoreFoundation.framework $(moz_CFLAGS) -I/Developer/Headers/FlatCarbon -arch ppc -fno-common -fpascal-strings -O0 -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -DXP_MACOSX=1 -DNO_X11=1 -DUSE_SYSTEM_CONSOLE=1 -pipe -fmessage-length=0 -g -include mozilla-config.h
pic = pic
LIBRARIES_libvlc = $(top_builddir)/lib/libvlc_pic.a

else
LIBRARIES_mozilla = libvlcplugin$(LIBEXT)
SOURCES_unix = support/npunix.c
pic = pic
LIBRARIES_libvlc = $(top_builddir)/lib/libvlc_pic.a
endif
endif

if BUILD_MOZILLA
libplugin_a_SOURCES = $(SOURCES_mozilla)
libplugin_a_CFLAGS = `$(VLC_CONFIG) --cflags mozilla $(pic)` \
	             $(CPPFLAGS_mozilla_EXTRA) -Imozilla
libplugin_a_CXXFLAGS = `$(VLC_CONFIG) --cxxflags mozilla $(pic)` \
	               $(CPPFLAGS_mozilla_EXTRA) -Imozilla
libplugin_a_DEPENDENCIES = $(DATA_npvlc_rc)

BUILT_SOURCES_mozilla = vlcintf.h
$(SOURCES_mozilla): vlcintf.h

plugin_DATA = $(LIBRARIES_mozilla) $(BUNDLE_mozilla)
plugindir = $(libdir)/mozilla/plugins
$(LIBRARIES_mozilla): $(libplugin_a_OBJECTS) \
	              $(libplugin_a_DEPENDENCIES) stamp-pic
	$(CXXLINK) $(libplugin_a_OBJECTS) $(DATA_npvlc_rc) \
	  $(LIBRARIES_libvlc) -shared \
	  `$(VLC_CONFIG) --libs vlc mozilla builtin $(pic)`

vlcintf_xpt_DATA = vlcintf.xpt
vlcintf_xptdir = $(libdir)/mozilla/components
vlcintf.xpt: vlcintf.idl
	$(XPIDL) $(XPIDL_INCL) \
	  -m typelib -o vlcintf $(srcdir)/vlcintf.idl

vlcintf.h: vlcintf.idl
	$(XPIDL) $(XPIDL_INCL) \
	  -m header -o vlcintf $(srcdir)/vlcintf.idl

if HAVE_WIN32
DATA_npvlc_rc = $(noinst_npvlc_rc_DATA)
noinst_npvlc_rc_DATA = npvlc_rc.$(OBJEXT)
noinst_npvlc_rcdir = $(libdir)
npvlc_rc.$(OBJEXT): npvlc_rc.rc
	$(WINDRES) -DVERSION=$(VERSION) -DVERSION_NUMBER=`echo $(VERSION).0.0.0 | sed 's/\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\).*/\1,\2,\3,\4/'` --include-dir $(srcdir) -i $< -o $@
endif
if HAVE_DARWIN
VLC\ Plugin.plugin:
	rm -rf ./VLC\ Plugin.plugin
	mkdir -p ./VLC\ Plugin.plugin/Contents/MacOS
	mkdir -p ./VLC\ Plugin.plugin/Contents/Resources
	/usr/bin/g++3 $(moz_plugin_FLAGS) $(srcdir)/vlcplugin.cpp -o ./libplugin_a-vlcplugin.o
	/usr/bin/g++3 $(moz_plugin_FLAGS) $(srcdir)/vlcshell.cpp -o ./libplugin_a-vlcshell.o
	/usr/bin/g++3 $(moz_plugin_FLAGS) $(srcdir)/vlcpeer.cpp -o ./libplugin_a-vlcpeer.o
	/usr/bin/g++3 $(moz_plugin_FLAGS) $(srcdir)/support/npmac.cpp -o ./libplugin_a-npmac.o
	/usr/bin/g++3 ./libplugin_a-vlcplugin.o ./libplugin_a-vlcshell.o ./libplugin_a-vlcpeer.o ./libplugin_a-npmac.o -o ./VLC\ Plugin.plugin/Contents/MacOS/VLC\ Plugin -L/usr/lib -F/System/Library/Frameworks/CoreFoundation.framework -framework CoreFoundation -arch ppc -bundle -read_only_relocs suppress $(LIBRARIES_libvlc) -dylib $(vlc_moz_FLAGS)
	cp $(top_srcdir)/extras/MacOSX/plugin/Info.plist ./VLC\ Plugin.plugin/Contents/Info.plist
	cp $(top_srcdir)/extras/MacOSX/plugin/pbdevelopment.plist ./VLC\ Plugin.plugin/Contents/pbdevelopment.plist
	cp -r $(top_srcdir)/extras/MacOSX/plugin/English.lproj ./VLC\ Plugin.plugin/Contents/Resources/
	Rez /Developer/Headers/FlatCarbon/Types.r $(srcdir)/vlc.r -o ./VLC\ Plugin.plugin/Contents/Resources/Vlc\ Plugin.rsrc
	mkdir ./VLC\ Plugin.plugin/Contents/MacOS/modules
	cp -r $(top_builddir)/VLC.app/Contents/MacOS/modules/* ./VLC\ Plugin.plugin/Contents/MacOS/modules/
endif
endif

###############################################################################
# Stamp rules
###############################################################################
clean: clean-stamp clean-bundle
clean-stamp:
	rm -f stamp-pic

stamp-pic: FORCE
	@for dep in "" `$(VLC_CONFIG) --target builtin $(pic)`; do \
	  if test "$${dep}" -nt "$(LIBRARIES_mozilla)"; then \
	    rm -f $@; \
	    break; \
	  fi; \
	done
	@if test ! -f $@; then printf "" > $@; fi

clean-bundle:
	rm -rf ./VLC\ Plugin.plugin

###############################################################################
# Force rule
###############################################################################
FORCE:
