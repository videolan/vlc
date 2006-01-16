###############################################################################
# Building the Mozilla plugin
###############################################################################

noinst_LIBRARIES = $(noinst_LIBRARIES_mozilla)
	                                     
MOSTLYCLEANFILES = $(LIBRARIES_mozilla)
EXTRA_DIST = $(SOURCES_mozilla) $(DIST_rsrc) $(DIST_misc)
BUILT_SOURCES = $(BUILT_SOURCES_mozilla)

SOURCES_mozilla = \
	vlcshell.cpp \
	vlcplugin.cpp \
	vlcplugin.h \
	vlcpeer.cpp \
	vlcpeer.h \
	support/classinfo.h \
	$(SOURCES_support) \
	$(NULL)

DIST_rsrc = \
	$(SOURCES_rsrc) \
	$(NULL)

DIST_misc = \
	vlcintf.idl \
	$(NULL)

# Under Win32, Mozilla plugins need to be named NP******.DLL, but under Unix
# the common naming scheme is lib******plugin.so. Also, we need npwin.cpp
# under Win32 and npunix.c under Unix.
if HAVE_WIN32

LIBRARIES_libvlc = $(top_builddir)/lib/libvlc.a

npvlc_DATA = npvlc$(LIBEXT)
npvlcdir = $(libdir)

SOURCES_support = support/npwin.cpp
SOURCES_rsrc = npvlc_rc.rc
CPPFLAGS_mozilla_EXTRA = -DXP_WIN -DXP_WIN32

DATA_npvlc_rc = $(noinst_npvlc_rc_DATA)
noinst_npvlc_rc_DATA = npvlc_rc.$(OBJEXT)
noinst_npvlc_rcdir = $(libdir)
npvlc_rc.$(OBJEXT): npvlc_rc.rc
	$(WINDRES) -DVERSION=$(VERSION) -DVERSION_NUMBER=`echo $(VERSION).0.0.0 | sed 's/\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\).*/\1,\2,\3,\4/'` --include-dir $(srcdir) -i $< -o $@
LDFLAGS_npvlc = -shared $(LIBRARIES_libvlc) `$(VLC_CONFIG) --libs mozilla vlc builtin`

else
if HAVE_DARWIN

LIBRARIES_libvlc = $(top_builddir)/lib/libvlc.a

npvlc_DATA = $(noinst_npvlc_DATA)
noinst_npvlc_DATA = npvlc$(LIBEXT)
noinst_npvlcdir = $(libdir)

BUNDLE_mozilla = VLC\ Plugin.plugin
SOURCES_support = support/npmac.cpp
SOURCES_rsrc = vlc.r

DATA_bundle = $(noinst_bundle_DATA)
noinst_bundle_DATA = VLC\ Plugin.plugin
noinst_bundledir = $(libdir)

VLC\ Plugin.plugin: npvlc.rsrc npvlc.dylib
	rm -rf "$@"
	mkdir -p "./$@/Contents/MacOS"
	cp npvlc.dylib "./$@/Contents/MacOS/$@"
	mkdir -p ./"$@"/Contents/Resources
	cp npvlc.rsrc "./$@/Contents/MacOS/$@.rsrc"
	cp $(top_srcdir)/extras/MacOSX/plugin/Info.plist "./$@/Contents/Info.plist"
	cp $(top_srcdir)/extras/MacOSX/plugin/pbdevelopment.plist "./$@/Contents/pbdevelopment.plist"
	cp -r $(top_srcdir)/extras/MacOSX/plugin/English.lproj "./$@/Contents/Resources/"
	(cd $(top_builddir)/VLC.app/Contents/MacOS/; tar cf - modules)| \
	(cd "./$@/Contents/MacOS"; tar xf -)

CPPFLAGS_mozilla_EXTRA = -I. -I$(top_builddir) -I$(srcdir)/../include -c -F/System/Library/Frameworks/CoreFoundation.framework $(moz_CFLAGS) -I/Developer/Headers/FlatCarbon -arch ppc -fno-common -fpascal-strings -O0 -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -DXP_MACOSX=1 -DNO_X11=1 -DUSE_SYSTEM_CONSOLE=1 -pipe -fmessage-length=0 -g -include mozilla-config.h

DATA_npvlc_rsrc = $(noinst_npvlc_rsrc_DATA)
noinst_npvlc_rsrc_DATA = npvlc.rsrc
noinst_npvlc_rsrcdir = $(libdir)
npvlc.rsrc: $(srcdir)/vlc.r
	/Developer/Tools/Rez /Developer/Headers/FlatCarbon/Types.r $< -o $@
LDFLAGS_npvlc = -arch ppc -bundle -read_only_relocs suppress $(LIBRARIES_libvlc) -dylib `$(VLC_CONFIG) --libs mozilla vlc builtin $(pic)`

else

LIBRARIES_libvlc = $(top_builddir)/lib/libvlc_pic.a
npvlc_DATA = libvlcplugin$(LIBEXT)
npvlcdir = $(libdir)/mozilla/plugins

SOURCES_support = support/npunix.c

LDFLAGS_npvlc = -shared $(LIBRARIES_libvlc) `$(VLC_CONFIG) --libs mozilla vlc builtin $(pic)`

endif
endif

if BUILD_MOZILLA

noinst_LIBRARIES_mozilla = libnpvlc.a

libnpvlc_a_SOURCES = $(SOURCES_mozilla)
libnpvlc_a_CFLAGS = `$(VLC_CONFIG) --cflags mozilla $(pic)` \
	             $(CPPFLAGS_mozilla_EXTRA)
libnpvlc_a_CXXFLAGS = `$(VLC_CONFIG) --cxxflags mozilla $(pic)` \
	               $(CPPFLAGS_mozilla_EXTRA)
libnpvlc_a_DEPENDENCIES = $(DATA_npvlc_rc)

BUILT_SOURCES_mozilla = vlcintf.h
$(SOURCES_mozilla): vlcintf.h

npvlc$(LIBEXT): $(libnpvlc_a_OBJECTS) \
	              $(libnpvlc_a_DEPENDENCIES) stamp-pic
	$(CXXLINK) $(libnpvlc_a_OBJECTS) $(DATA_npvlc_rc) \
	 $(LDFLAGS_npvlc)

vlcintf_xpt_DATA = vlcintf.xpt
vlcintf_xptdir = $(libdir)/mozilla/components
vlcintf.xpt: vlcintf.idl
	$(XPIDL) $(XPIDL_INCL) \
	  -m typelib -o vlcintf $(srcdir)/vlcintf.idl

vlcintf.h: vlcintf.idl
	$(XPIDL) $(XPIDL_INCL) \
	  -m header -o vlcintf $(srcdir)/vlcintf.idl


endif

###############################################################################
# Stamp rules
###############################################################################
clean-local: clean-stamp clean-bundle

clean-bundle::
	rm -rf ./VLC\ Plugin.plugin

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

###############################################################################
# Force rule
###############################################################################
FORCE:
