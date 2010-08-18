# Building vlc
#
bin_PROGRAMS = vlc
noinst_PROGRAMS = vlc-static
vlclib_PROGRAMS = vlc-cache-gen
EXTRA_PROGRAMS = vlc-wrapper
EXTRA_DIST = vlc_win32_rc.rc.in

SUFFIXES = .rc.in .rc

AM_CFLAGS = `$(VLC_CONFIG) --cflags vlc`


if !HAVE_WIN32
if !HAVE_WINCE
bin_PROGRAMS += vlc-wrapper
vlc_SOURCES = vlc.c override.c
endif
endif

EXTRA_vlc_SOURCES = vlc.c winvlc.c
if HAVE_WIN32
vlc_SOURCES = winvlc.c
noinst_DATA = vlc_win32_rc.rc
endif
if HAVE_WINCE
vlc_SOURCES = winvlc.c
endif

vlc_wrapper_SOURCES = rootwrap.c
vlc_wrapper_LDADD = $(SOCKET_LIBS)

vlc_DEPENDENCIES = ../src/libvlc.sym
vlc_LDFLAGS = `$(VLC_CONFIG) --ldflags vlc`
vlc_LDADD = ../src/libvlc.la ../src/libvlccore.la `$(VLC_CONFIG) -libs vlc`

vlc_static_SOURCES = $(vlc_SOURCES)
vlc_static_CFLAGS = $(AM_CFLAGS) \
	-DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\" \
	-DTOP_SRCDIR=\"$$(cd "$(top_srcdir)"; pwd)\" \
	$(NULL)
vlc_static_DEPENDENCIES = $(vlc_DEPENDENCIES)
vlc_static_LDADD = $(vlc_LDADD)
vlc_static_LDFLAGS = $(vlc_LDFLAGS) -no-install -static

if HAVE_WIN32
vlc_LDADD += -lwininet vlc_win32_rc.$(OBJEXT)
vlc_DEPENDENCIES += vlc_win32_rc.$(OBJEXT)
else
vlc_LDADD += $(LIBDL)
endif

vlc_win32_rc.rc: $(top_builddir)/config.status
	cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="bin/$@"

vlc_win32_rc.$(OBJEXT): vlc_win32_rc.rc
	$(WINDRES) --include-dir $(top_srcdir)/share/icons -i $< -o $@

vlc_cache_gen_SOURCES = cachegen.c
vlc_cache_gen_LDADD = \
	$(GNUGETOPT_LIBS) \
	../compat/libcompat.la \
	../src/libvlc.la ../src/libvlccore.la
