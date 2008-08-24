# Building vlc
#
bin_PROGRAMS = vlc
EXTRA_PROGRAMS = vlc-wrapper

if !HAVE_WIN32
bin_PROGRAMS += vlc-wrapper
endif

EXTRA_vlc_SOURCES = vlc.c winvlc.c
if !HAVE_WIN32
vlc_SOURCES = vlc.c
else
vlc_SOURCES = winvlc.c
endif
vlc_wrapper_SOURCES = rootwrap.c

vlc_DEPENDENCIES = $(DATA_win32_rc) ../src/libvlc.la

vlc_CFLAGS = `$(VLC_CONFIG) --cflags vlc`
vlc_LDFLAGS = `$(VLC_CONFIG) --ldflags vlc`
# vlc needs libvlccore for locale conversion
vlc_LDADD = ../src/libvlc.la ../src/libvlccore.la $(LTLIBINTL) \
	`$(VLC_CONFIG) -libs vlc`

#vlc$(EXEEXT): $(vlc_OBJECTS) $(vlc_DEPENDENCIES)
#	$(LINK) $(vlc_OBJECTS) $(vlc_LDADD) $(vlc_LDFLAGS)

DATA_win32_rc = vlc_win32_rc.$(OBJEXT)
DATA_win32_rc_lib = libvlc_win32_rc.$(OBJEXT)
EXTRA_DATA = $(DATA_win32_rc) $(DATA_win32_rc_lib)
if HAVE_WIN32
vlc_LDADD += $(DATA_win32_rc)
noinst_DATA = $(DATA_win32_rc) $(DATA_win32_rc_lib)
endif

vlc_win32_rc.$(OBJEXT): $(top_builddir)/share/vlc_win32_rc.rc
	$(WINDRES) --include-dir $(top_srcdir)/share -i $< -o $@

libvlc_win32_rc.$(OBJEXT): $(top_builddir)/share/libvlc_win32_rc.rc
	$(WINDRES) --include-dir $(top_srcdir)/share -i $< -o $@
