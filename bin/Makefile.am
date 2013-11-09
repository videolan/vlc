# Building vlc
#
bin_PROGRAMS = vlc
noinst_PROGRAMS = vlc-static
noinst_DATA =
vlclib_PROGRAMS = vlc-cache-gen
EXTRA_PROGRAMS = vlc-wrapper
EXTRA_DIST = vlc_win32_rc.rc.in

SUFFIXES = .rc.in .rc

AM_CPPFLAGS = $(CPPFLAGS_vlc)
AM_CFLAGS = $(CFLAGS_vlc)
if HAVE_DBUS
AM_CPPFLAGS += -DHAVE_DBUS
endif

if !HAVE_WIN32
bin_PROGRAMS += vlc-wrapper
vlc_SOURCES = vlc.c override.c
endif

EXTRA_vlc_SOURCES = vlc.c winvlc.c darwinvlc.c
if HAVE_WIN32
vlc_SOURCES = winvlc.c
noinst_DATA += vlc_win32_rc.rc
endif
if HAVE_DARWIN
vlc_SOURCES = darwinvlc.c override.c
endif

vlc_wrapper_SOURCES = rootwrap.c
vlc_wrapper_LDADD = $(SOCKET_LIBS)

vlc_LDFLAGS = $(LDFLAGS_vlc)
if HAVE_DARWIN
vlc_LDFLAGS += -Wl,-framework,CoreFoundation
endif
vlc_LDADD = ../lib/libvlc.la $(LIBPTHREAD)

vlc_static_SOURCES = $(vlc_SOURCES)
vlc_static_CFLAGS = $(AM_CFLAGS) \
	-DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\" \
	-DTOP_SRCDIR=\"$$(cd "$(top_srcdir)"; pwd)\" \
	$(NULL)
vlc_static_DEPENDENCIES = $(vlc_DEPENDENCIES)
vlc_static_LDADD = $(vlc_LDADD)
vlc_static_LDFLAGS = $(vlc_LDFLAGS)

if HAVE_WIN32
vlc_LDADD += -lpsapi -lwininet vlc_win32_rc.$(OBJEXT)
vlc_DEPENDENCIES = vlc_win32_rc.$(OBJEXT)
else
vlc_LDADD += $(LIBDL)
vlc_static_LDFLAGS += -no-install -static
endif

vlc_win32_rc.rc: $(top_builddir)/config.status vlc_win32_rc.rc.in
	cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="bin/$@"

vlc_win32_rc.$(OBJEXT): vlc_win32_rc.rc
	$(WINDRES) --include-dir $(top_srcdir)/share/icons --include-dir $(top_srcdir)/extras/package/win32 -i $< -o $@

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
noinst_DATA += ../modules/plugins.dat
MOSTLYCLEANFILES = $(noinst_DATA)

.PHONY: ../modules/plugins.dat

../modules/plugins.dat: vlc-cache-gen$(EXEEXT)
	$(AM_V_at)rm -f ../modules/plugins.dat
	$(AM_V_GEN)if test "$(build)" = "$(host)"; then \
		./vlc-cache-gen$(EXEEXT) ../modules ; \
	else \
		echo "Cross-compilation: cache generation skipped!" ; \
	fi
