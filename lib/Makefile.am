AUTOMAKE_OPTIONS = subdir-objects
pkgconfigdir = $(libdir)/pkgconfig
AM_CFLAGS = $(CFLAGS_libvlc)

SUFFIXES = .pc.in .pc .rc.in .rc

BUILT_SOURCES = $(nodist_pkginclude_HEADERS)
CLEANFILES = $(BUILT_SOURCES) $(pkgconfig_DATA)

pkginclude_HEADERS = \
	../include/vlc/deprecated.h \
	../include/vlc/libvlc.h \
	../include/vlc/libvlc_dialog.h \
	../include/vlc/libvlc_events.h \
	../include/vlc/libvlc_media.h \
	../include/vlc/libvlc_media_discoverer.h \
	../include/vlc/libvlc_media_list.h \
	../include/vlc/libvlc_media_list_player.h \
	../include/vlc/libvlc_media_player.h \
	../include/vlc/libvlc_media_track.h \
	../include/vlc/libvlc_renderer_discoverer.h \
	../include/vlc/libvlc_picture.h \
	../include/vlc/libvlc_version.h \
	../include/vlc/vlc.h

lib_LTLIBRARIES = libvlc.la

libvlc_la_SOURCES = \
	libvlc_internal.h \
	media_internal.h \
	media_list_internal.h \
	media_player_internal.h \
	picture_internal.h \
	renderer_discoverer_internal.h \
	core.c \
	dialog.c \
	renderer_discoverer.c \
	error.c \
	log.c \
	playlist.c \
	video.c \
	audio.c \
	event.c \
	media.c \
	media_track.c \
	media_player.c \
	media_list.c \
	media_list_path.h \
	media_list_player.c \
	media_discoverer.c \
	picture.c \
	../src/revision.c
EXTRA_DIST = libvlc.pc.in libvlc.sym

if HAVE_WIN32
noinst_DATA = libvlc_win32_rc.rc
endif
EXTRA_DIST += libvlc_win32_rc.rc.in

libvlc_win32_rc.rc: libvlc_win32_rc.rc.in $(top_builddir)/config.status
	$(AM_V_GEN)cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="lib/$@"

libvlc_la_LIBADD = ../src/libvlccore.la ../compat/libcompat.la $(LIBM)
libvlc_la_LDFLAGS = \
	$(LDFLAGS_libvlc) \
	-no-undefined \
	-version-number @LIBVLC_ABI_MAJOR@:@LIBVLC_ABI_MINOR@:@LIBVLC_ABI_MICRO@ \
	-export-symbols $(srcdir)/libvlc.sym
libvlc_la_DEPENDENCIES = libvlc.sym
libvlc_la_CPPFLAGS = $(AM_CPPFLAGS)
if HAVE_WIN32
libvlc_la_DEPENDENCIES += libvlc_win32_rc.$(OBJEXT)
libvlc_la_LDFLAGS += -Wl,libvlc_win32_rc.$(OBJEXT) -avoid-version -Wc,-static
if HAVE_DYNAMIC_PLUGINS
libvlc_la_CPPFLAGS += -DLIBVLC_DLL_EXPORT
endif
endif
if HAVE_OS2
libvlc_la_LDFLAGS += -avoid-version
endif
if HAVE_DARWIN
libvlc_la_LDFLAGS += -Xlinker -install_name -Xlinker @rpath/libvlc.dylib
endif

# iOS and tvOS applications cannot install global shared libraries and
# dylibs must be in frameworks so there's no need for libtool versioning.
if HAVE_IOS
libvlc_la_LDFLAGS += -avoid-version
endif
if HAVE_TVOS
libvlc_la_LDFLAGS += -avoid-version
endif

pkgconfig_DATA = libvlc.pc

libvlc.pc: libvlc.pc.in $(top_builddir)/config.status
	$(AM_V_GEN)cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="lib/$@"

libvlc_win32_rc.$(OBJEXT): libvlc_win32_rc.rc $(top_srcdir)/extras/package/win32/libvlc.dll.manifest
	$(WINDRES) --include-dir $(top_srcdir)/share --include-dir $(top_srcdir)/extras/package/win32 -i $< -o $@

check-local:
	$(SHELL) $(top_srcdir)/src/check_headers $(pkginclude_HEADERS)
