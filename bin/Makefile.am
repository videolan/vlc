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

vlc_DEPENDENCIES = ../src/libvlc.la

vlc_CFLAGS = `$(VLC_CONFIG) --cflags vlc`
vlc_LDFLAGS = `$(VLC_CONFIG) --ldflags vlc`
# vlc needs libvlccore for locale conversion
vlc_LDADD = ../src/libvlc.la ../src/libvlccore.la $(LTLIBINTL) \
	`$(VLC_CONFIG) -libs vlc`

#vlc$(EXEEXT): $(vlc_OBJECTS) $(vlc_DEPENDENCIES)
#	$(LINK) $(vlc_OBJECTS) $(vlc_LDADD) $(vlc_LDFLAGS)

if HAVE_WIN32
vlc_LDADD += vlc_win32_rc.$(OBJEXT)
vlc_DEPENDENCIES += vlc_win32_rc.$(OBJEXT)
endif

vlc_win32_rc.$(OBJEXT): $(top_builddir)/share/vlc_win32_rc.rc
	$(WINDRES) --include-dir $(top_srcdir)/share -i $< -o $@

