# Building vlc
#
if BUILD_VLC
if HAVE_OSX
bin_PROGRAMS = vlc-osx
noinst_PROGRAMS = vlc-osx-static
else
bin_PROGRAMS = vlc
noinst_PROGRAMS = vlc-static
endif
endif
EXTRA_DIST = vlc_win32_rc.rc.in
CLEANFILES = vlc_win32_rc.rc

SUFFIXES = .rc.in .rc

AM_CPPFLAGS = $(CPPFLAGS_vlc)
AM_CFLAGS = $(CFLAGS_vlc)
if HAVE_DBUS
AM_CPPFLAGS += -DHAVE_DBUS
endif

#
# Main VLC executable
#
vlc_LDADD = ../lib/libvlc.la $(LIBPTHREAD)
vlc_CPPFLAGS = $(AM_CPPFLAGS)
if !HAVE_WIN32
vlc_SOURCES = vlc.c override.c
vlc_LDADD += $(LIBDL)
else
vlc_SOURCES = winvlc.c
vlc_DEPENDENCIES = vlc_win32_rc.$(OBJEXT)
vlc_LDFLAGS = -mwindows
vlc_LDADD += vlc_win32_rc.$(OBJEXT)
libbreakpad_wrapper_la_SOURCES = breakpad.cpp
libbreakpad_wrapper_la_LIBADD = $(BREAKPAD_LIBS)
libbreakpad_wrapper_la_LDFLAGS = -static
libbreakpad_wrapper_la_CXXFLAGS = $(AM_CXXFLAGS) $(BREAKPAD_CFLAGS) -DBREAKPAD_URL=\"@BREAKPAD_URL@\"
if HAVE_BREAKPAD
noinst_LTLIBRARIES = libbreakpad_wrapper.la
vlc_LDADD += libbreakpad_wrapper.la -lstdc++ -lwininet
vlc_DEPENDENCIES += libbreakpad_wrapper.la
vlc_CPPFLAGS += -DHAVE_BREAKPAD
endif
endif

vlc_osx_SOURCES = darwinvlc.m
vlc_osx_LDFLAGS = $(LDFLAGS_vlc) -Wl,-framework,CoreFoundation,-framework,Cocoa
vlc_osx_LDFLAGS += -Xlinker -rpath -Xlinker "@executable_path/lib/"
vlc_osx_LDFLAGS += -Xlinker -rpath -Xlinker "@executable_path/plugins/"
if HAVE_BREAKPAD
vlc_osx_LDFLAGS += -Wl,-framework,Breakpad
vlc_osx_OBJCFLAGS = -F$(CONTRIB_DIR)/Frameworks
vlc_osx_CPPFLAGS = -DHAVE_BREAKPAD
endif
vlc_osx_LDADD = ../lib/libvlc.la

vlc_osx_static_SOURCES = $(vlc_osx_SOURCES)
vlc_osx_static_OBJCFLAGS = $(AM_OBJCFLAGS) \
	-DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\" \
	-DTOP_SRCDIR=\"$$(cd "$(top_srcdir)"; pwd)\" \
	$(NULL)
vlc_osx_static_LDFLAGS = $(vlc_osx_LDFLAGS) -static
vlc_osx_static_LDFLAGS += -Xlinker -rpath -Xlinker "@executable_path/../lib/.libs/"
vlc_osx_static_LDFLAGS += -Xlinker -rpath -Xlinker "@executable_path/../src/.libs/"
vlc_osx_static_LDADD = $(vlc_osx_LDADD)
if HAVE_BREAKPAD
vlc_osx_static_LDFLAGS += -Wl,-framework,Breakpad
vlc_osx_static_OBJCFLAGS += -F$(CONTRIB_DIR)/Frameworks
vlc_osx_static_CPPFLAGS = -DHAVE_BREAKPAD
endif


#
# Static (debug) VLC executable
#
vlc_static_SOURCES = $(vlc_SOURCES)
vlc_static_CFLAGS = $(AM_CFLAGS) \
	-DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\" \
	-DTOP_SRCDIR=\"$$(cd "$(top_srcdir)"; pwd)\" \
	$(NULL)
vlc_static_CPPFLAGS = $(vlc_CPPFLAGS)
vlc_static_DEPENDENCIES = $(vlc_DEPENDENCIES)
vlc_static_LDADD = $(vlc_LDADD)
vlc_static_LDFLAGS = $(vlc_LDFLAGS) -no-install -static

#
# Root wrapper
#
if BUILD_VLC
if !HAVE_WIN32
if !HAVE_OS2
bin_PROGRAMS += vlc-wrapper
endif
endif
endif
vlc_wrapper_SOURCES = rootwrap.c
vlc_wrapper_LDADD = $(SOCKET_LIBS)

vlc_win32_rc.rc: $(top_builddir)/config.status vlc_win32_rc.rc.in
	cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="bin/$@"

vlc_win32_rc.$(OBJEXT): vlc_win32_rc.rc $(top_srcdir)/extras/package/win32/vlc.exe.manifest
	$(WINDRES) --include-dir $(top_srcdir)/share/icons --include-dir $(top_srcdir)/extras/package/win32 -i $< -o $@

#
# Plug-ins cache generator
#
if HAVE_DYNAMIC_PLUGINS
vlclib_PROGRAMS = vlc-cache-gen
endif
vlc_cache_gen_SOURCES = cachegen.c
vlc_cache_gen_LDADD = \
	$(GNUGETOPT_LIBS) \
	../compat/libcompat.la \
	../lib/libvlc.la
if HAVE_WIN32
vlc_cache_gen_LDADD += vlc_win32_rc.$(OBJEXT)
vlc_cache_gen_DEPENDENCIES = vlc_win32_rc.$(OBJEXT)
endif

#
# Plug-ins cache
#
if HAVE_DYNAMIC_PLUGINS
noinst_DATA = ../modules/plugins.dat
endif
MOSTLYCLEANFILES = $(noinst_DATA)

if HAVE_OSX
if BUILD_VLC
install-data-local:
	cd $(bindir); mv vlc-osx vlc

endif
endif

.PHONY: ../modules/plugins.dat

../modules/plugins.dat: vlc-cache-gen$(EXEEXT)
	$(AM_V_at)rm -f ../modules/plugins.dat
	$(AM_V_GEN)if test "$(build)" = "$(host)"; then \
		./vlc-cache-gen$(EXEEXT) `realpath ../modules` ; \
	else \
		echo "Cross-compilation: cache generation skipped!" ; \
	fi
