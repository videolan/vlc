###############################################################################
# Automake targets and declarations
###############################################################################

AUTOMAKE_OPTIONS = subdir-objects

NULL =
SUBDIRS =

EXTRA_DIST = extras/COPYING misc/modules_builtin.h.in

BUILT_SOURCES = $(DISTCLEANFILES) $(CLEANFILES)
DISTCLEANFILES = stamp-api
CLEANFILES = misc/modules_builtin.h stamp-version misc/version.c
MOSTLYCLEANFILES = $(DATA_noinst_libvlc) stamp-builtins

TOOLBOX = srcdir=$(top_srcdir) builddir=$(top_builddir) $(top_srcdir)/toolbox

###############################################################################
# Headers
###############################################################################

pkgincludedir = $(includedir)/vlc

dist_pkginclude_HEADERS = \
	../include/vlc/vlc.h \
	../include/vlc/libvlc.h \
	../include/vlc/aout.h \
	../include/vlc/vout.h \
	../include/vlc/sout.h \
	../include/vlc/decoder.h \
	../include/vlc/input.h \
	../include/vlc/intf.h \
	../include/vlc/mediacontrol.h \
	../include/vlc/mediacontrol_structures.h \
	$(NULL)

noinst_HEADERS = $(HEADERS_include)
noinst_DATA = $(DATA_noinst_libvlc)

HEADERS_include = \
	../include/aout_internal.h \
	../include/audio_output.h \
	../include/beos_specific.h \
	../include/charset.h \
	../include/codecs.h \
	../include/configuration.h \
	../include/darwin_specific.h \
	../include/intf_eject.h \
	../include/iso_lang.h \
	../include/main.h \
	../include/mmx.h \
	../include/modules.h \
	../include/modules_inner.h \
	../include/mtime.h \
	../include/network.h \
	../include/os_specific.h \
	../include/snapshot.h \
	../include/stream_output.h \
	../include/variables.h \
	../include/video_output.h \
	../include/vlc_access.h \
	../include/vlc_acl.h \
	../include/vlc_bits.h \
	../include/vlc_block.h \
	../include/vlc_block_helper.h \
	../include/vlc_codec.h \
	../include/vlc_common.h \
	../include/vlc_config.h \
	../include/vlc_cpu.h \
	../include/vlc_demux.h \
	../include/vlc_error.h \
	../include/vlc_es.h \
	../include/vlc_es_out.h \
	../include/vlc_filter.h \
	../include/vlc_config_cat.h \
	../include/vlc_httpd.h \
	../include/vlc_tls.h \
	../include/vlc_md5.h \
	../include/vlc_image.h \
	../include/vlc_input.h \
	../include/vlc_interaction.h \
	../include/vlc_interface.h \
	../include/vlc_keys.h \
	../include/vlc_messages.h \
	../include/vlc_meta.h \
	../include/vlc_objects.h \
	../include/vlc_osd.h \
	../include/vlc_playlist.h \
	../include/vlc_spu.h \
	../include/vlc_stream.h \
	../include/vlc_symbols.h \
	../include/vlc_threads_funcs.h \
	../include/vlc_threads.h \
	../include/vlc_update.h \
	../include/vlc_url.h \
	../include/vlc_video.h \
	../include/vlc_vlm.h \
	../include/vlc_vod.h \
	../include/vlc_xml.h \
	../include/vout_synchro.h \
	../include/win32_specific.h \
	../include/libvlc_internal.h \
	../include/mediacontrol_internal.h
	$(NULL)

misc/modules_builtin.h: Makefile misc/modules_builtin.h.in ../vlc-config
	$(TOOLBOX) --update-includes
	touch $@

misc/modules.c: misc/modules_builtin.h

misc/version.c:
	$(TOOLBOX) --update-version

stamp-version: misc/version.c
	$(TOOLBOX) --update-version
	touch $@

.PHONY: stamp-version

###############################################################################
# Building builtin modules
###############################################################################
#
# As long as we use builtins with a shared libvlc, we must build them before
# we build libvlc. Maybe one day, libvlc will handle multiple modules per
# shared object, which will make builtins fairly redumdant. Until then, we
# need this workaround.

stamp-builtins: Makefile ../vlc-config ../config.status
if USE_LIBTOOL
	@for c in `$(VLC_CONFIG) --libs builtin`; do \
		case $$c in \
        		../modules/*.a) echo $$c ;; \
		esac ; \
	done | \
	sed -e 's/^\(.*\)\/\([^\/]*\)\.a$$/cd \1 \&\& $(MAKE) \2_builtin.la/g' | \
	while read cmd; do echo $$cmd; eval "($$cmd)" || exit $$? ; done
endif
if BUILD_SHARED
	@if test "$(pic)" = "pic"; then ext="_pic.a"; else ext=".a"; fi ; \
	for c in `$(VLC_CONFIG) --libs builtin`; do \
		case $$c in \
			../modules/*.a) echo $$c ;; \
		esac ; \
	done | \
	sed -e 's/^\(.*\)\/\([^\/]*\)\.a$$/cd \1 \&\& $(MAKE) \2/g' | \
	while read cmd; do echo $$cmd$$ext; eval "($$cmd$$ext)" || exit $$? ; done
endif
	touch $@

###############################################################################
# Building libvlc
###############################################################################

libvlc_a_SOURCES = $(SOURCES_libvlc)
libvlc_a_CFLAGS = `$(VLC_CONFIG) --cflags vlc`
libvlc_a_CXXFLAGS = `$(VLC_CONFIG) --cxxflags vlc`
libvlc_a_OBJCFLAGS = `$(VLC_CONFIG) --objcflags vlc`

libvlc_pic_a_SOURCES = $(SOURCES_libvlc)
libvlc_pic_a_CFLAGS = `$(VLC_CONFIG) --cflags vlc pic`
libvlc_pic_a_CXXFLAGS = `$(VLC_CONFIG) --cxxflags vlc pic`
libvlc_pic_a_OBJCFLAGS = `$(VLC_CONFIG) --objcflags vlc pic`

libvlc_la_SOURCES = $(SOURCES_libvlc)
libvlc_la_CFLAGS = `$(VLC_CONFIG) --cflags vlc`
libvlc_la_CXXFLAGS = `$(VLC_CONFIG) --cxxflags vlc`
libvlc_la_OBJCFLAGS = `$(VLC_CONFIG) --objcflags vlc`
libvlc_la_LDFLAGS = `$(VLC_CONFIG) --libs vlc builtin|sed -e 's/\(modules\/[^ ]*\)\.a /\1_builtin.la /g'` \
		-avoid-version -no-undefined
libvlc_la_DEPENDENCIES = stamp-builtins


if HAVE_BEOS
OPT_SOURCES_libvlc_beos = $(SOURCES_libvlc_beos)
endif
if HAVE_DARWIN
OPT_SOURCES_libvlc_darwin = $(SOURCES_libvlc_darwin)
endif
if HAVE_WIN32
OPT_SOURCES_libvlc_win32 = $(SOURCES_libvlc_win32)
endif
if HAVE_WINCE
OPT_SOURCES_libvlc_win32 = $(SOURCES_libvlc_win32)
endif
if BUILD_DIRENT
OPT_SOURCES_libvlc_dirent = $(SOURCES_libvlc_dirent)
endif
if BUILD_GETOPT
OPT_SOURCES_libvlc_getopt = $(SOURCES_libvlc_getopt)
endif

# Build libvlc as a shared library
if USE_LIBTOOL
lib_LTLIBRARIES = libvlc.la
else
lib_LIBRARIES = libvlc.a
if BUILD_PIC
lib_LIBRARIES += libvlc_pic.a
endif
endif

if HAVE_WIN32
if BUILD_SHARED
DATA_noinst_libvlc = libvlc$(LIBEXT)
endif

libvlc.dll.a: libvlc$(LIBEXT)

libvlc$(LIBEXT): $(OBJECTS_libvlc_so) stamp-builtins
	@ldfl="`$(VLC_CONFIG) --libs plugin vlc builtin $(pic)` $(INCLUDED_LIBINTL)" ; \
	case `$(VLC_CONFIG) --linkage vlc builtin` in \
	  c++)  ld="$(CXXLINK)" ;; \
	  objc) ld="$(OBJCLINK)" ;; \
	  c|*)  ld="$(LINK)" ;; \
	esac ; \
	echo $$ld $(OBJECTS_libvlc_so) $(LDFLAGS_libvlc_so) $$ldfl; \
	$$ld $(libvlc_a_OBJECTS) \
		-Wl,--out-implib,$(top_builddir)/src/libvlc.dll.a $$ldfl
# It sucks big time, but we have to do that to link vlc properly
# on non-PIC OSes, otherwise ld will favor builtins-less libvlc.a over
# libvlc$(LIBEXT)
	rm -f libvlc.a
# Cygwin work-around
	@if test -f "$@.exe"; then mv -f "$@.exe" "$@"; fi
endif

EXTRA_DIST += \
	$(SOURCES_libvlc_beos) \
	$(SOURCES_libvlc_darwin) \
	$(SOURCES_libvlc_win32) \
	$(SOURCES_libvlc_dirent) \
	$(SOURCES_libvlc_getopt) \
	$(NULL)

SOURCES_libvlc_beos = \
	misc/beos_specific.cpp \
	$(NULL)

SOURCES_libvlc_darwin = \
	misc/darwin_specific.m \
	$(NULL)

SOURCES_libvlc_win32 = \
	misc/win32_specific.c \
	$(NULL)

SOURCES_libvlc_dirent = \
	extras/dirent.c \
	$(NULL)

SOURCES_libvlc_getopt = \
	extras/getopt.c \
	extras/getopt.h \
	extras/getopt1.c \
	$(NULL)

SOURCES_libvlc_common = \
	libvlc.c \
	libvlc.h \
	interface/interface.c \
	interface/intf_eject.c \
	interface/interaction.c \
	playlist/playlist.c \
	playlist/sort.c \
	playlist/loadsave.c \
	playlist/view.c \
	playlist/item.c \
	playlist/item-ext.c \
	playlist/services_discovery.c \
	input/access.c \
	input/clock.c \
	input/control.c \
	input/decoder.c \
	input/demux.c \
	input/es_out.c \
	input/input.c \
	input/input_internal.h \
	input/stream.c \
	input/mem_stream.c \
	input/subtitles.c \
	input/var.c \
	video_output/video_output.c \
	video_output/vout_pictures.c \
	video_output/vout_pictures.h \
	video_output/video_text.c \
	video_output/video_widgets.c \
	video_output/vout_subpictures.c \
	video_output/vout_synchro.c \
	video_output/vout_intf.c \
	audio_output/common.c \
	audio_output/dec.c \
	audio_output/filters.c \
	audio_output/input.c \
	audio_output/mixer.c \
	audio_output/output.c \
	audio_output/intf.c \
	stream_output/stream_output.c \
	stream_output/announce.c \
	stream_output/sap.c \
	osd/osd.c \
	osd/osd_parser.c \
	osd/osd_text.c \
	osd/osd_widgets.c \
	network/acl.c \
	network/getaddrinfo.c \
	network/io.c \
	network/error.c \
	network/tcp.c \
	network/udp.c \
	network/httpd.c \
	network/rootwrap.c \
	network/tls.c \
	misc/charset.c \
	misc/md5.c \
	misc/mtime.c \
	misc/block.c \
	misc/modules.c \
	misc/threads.c \
	misc/stats.c \
	misc/unicode.c \
	misc/cpu.c \
	misc/configuration.c \
	misc/image.c \
	misc/iso_lang.c \
	misc/iso-639_def.h \
	misc/messages.c \
	misc/objects.c \
	misc/variables.c \
	misc/error.c \
	misc/update.c \
	misc/vlm.c \
	misc/xml.c \
	misc/hashtables.c \
	misc/version.c \
	extras/libc.c \
	control/core.c \
	control/playlist.c \
	control/vlm.c \
	control/input.c \
	control/video.c \
	control/audio.c \
	control/mediacontrol_core.c \
	control/mediacontrol_util.c \
	control/mediacontrol_audio_video.c \
	$(NULL)

# These should be distributed, but not compiled
EXTRA_DIST += control/mediacontrol_init.c control/mediacontrol_plugin.c

SOURCES_libvlc = \
	$(SOURCES_libvlc_common) \
	$(OPT_SOURCES_libvlc_beos) \
	$(OPT_SOURCES_libvlc_darwin) \
	$(OPT_SOURCES_libvlc_win32) \
	$(OPT_SOURCES_libvlc_dirent) \
	$(OPT_SOURCES_libvlc_getopt) \
	$(NULL)

if !USE_LIBTOOL
# Install shared libvlc
install-exec-local:
	test -z "$(DATA_noinst_libvlc)" || $(INSTALL_PROGRAM) "$(DATA_noinst_libvlc)" "$(DESTDIR)$(libdir)"

# the opposite of install-{data,exec}-local
uninstall-local:
	test -z "$(DATA_noinst_libvlc)" || rm -f "$(DESTDIR)$(libdir)/$(DATA_noinst_libvlc)"
endif

###############################################################################
# Stamp rules
###############################################################################
stamp-api: Makefile.in $(HEADERS_include) ../vlc-api.pl
	( cd $(srcdir) && cat $(HEADERS_include) ) | \
	  top_srcdir="$(top_srcdir)" perl $(top_srcdir)/vlc-api.pl
	touch stamp-api

###############################################################################
# Unit/regression test
###############################################################################
if USE_LIBTOOL
check_PROGRAMS = test_i18n_atof
TESTS = $(check_PROGRAMS)

CFLAGS_tests = `$(VLC_CONFIG) --cflags vlc`

test_i18n_atof_SOURCES = test/i18n_atof.c
test_i18n_atof_LDADD = libvlc.la
test_i18n_atof_CFLAGS = $(CFLAGS_tests)
endif

