# Building vlc
#
bin_PROGRAMS = vlc
noinst_PROGRAMS = vlc-static
EXTRA_PROGRAMS = vlc-wrapper

AM_CFLAGS = `$(VLC_CONFIG) --cflags vlc`


if !HAVE_WIN32
if !HAVE_WINCE
bin_PROGRAMS += vlc-wrapper
vlc_SOURCES = vlc.c
endif
endif

EXTRA_vlc_SOURCES = vlc.c winvlc.c
if HAVE_WIN32
vlc_SOURCES = winvlc.c
endif
if HAVE_WINCE
vlc_SOURCES = winvlc.c
endif

vlc_wrapper_SOURCES = rootwrap.c
vlc_wrapper_LDADD = $(SOCKET_LIBS)

vlc_DEPENDENCIES = ../src/libvlc.sym
vlc_LDFLAGS = `$(VLC_CONFIG) --ldflags vlc`
vlc_LDADD = ../src/libvlc.la `$(VLC_CONFIG) -libs vlc`

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
endif

vlc_win32_rc.$(OBJEXT): $(top_builddir)/share/vlc_win32_rc.rc
	$(WINDRES) --include-dir $(top_srcdir)/share -i $< -o $@

