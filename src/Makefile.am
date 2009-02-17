###############################################################################
# Automake targets and declarations
###############################################################################

AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = . test

NULL =
EXTRA_DIST = extras/COPYING \
	misc/beos_specific.cpp \
	libvlc.pc.in \
	vlc-plugin.pc.in \
	libvlc.sym \
	libvlccore.sym

BUILT_SOURCES = ../include/vlc_about.h
CLEANFILES = $(BUILT_SOURCES)

SUFFIXES = .pc.in .pc

###############################################################################
# Headers
###############################################################################

pluginsincludedir = $(pkgincludedir)/plugins

pkginclude_HEADERS = \
	../include/vlc/vlc.h \
	../include/vlc/deprecated.h \
	../include/vlc/libvlc.h \
	../include/vlc/libvlc_structures.h \
	../include/vlc/libvlc_media_list.h \
	../include/vlc/libvlc_events.h \
	../include/vlc/libvlc_vlm.h \
	../include/vlc/mediacontrol.h \
	../include/vlc/mediacontrol_structures.h \
	$(NULL)

pluginsinclude_HEADERS = \
	../include/vlc_access.h \
	../include/vlc_acl.h \
	../include/vlc_aout.h \
	../include/vlc_arrays.h \
	../include/vlc_bits.h \
	../include/vlc_block.h \
	../include/vlc_block_helper.h \
	../include/vlc_charset.h \
	../include/vlc_codec.h \
	../include/vlc_common.h \
	../include/vlc_config.h \
	../include/vlc_config_cat.h \
	../include/vlc_configuration.h \
	../include/vlc_demux.h \
	../include/vlc_epg.h \
	../include/vlc_es.h \
	../include/vlc_es_out.h \
	../include/vlc_events.h \
	../include/vlc_filter.h \
	../include/vlc_gcrypt.h \
	../include/vlc_httpd.h \
	../include/vlc_image.h \
	../include/vlc_input.h \
	../include/vlc_input_item.h \
	../include/vlc_main.h \
	../include/vlc_md5.h \
	../include/vlc_messages.h \
	../include/vlc_meta.h \
	../include/vlc_modules.h \
	../include/vlc_mtime.h \
	../include/vlc_objects.h \
	../include/vlc_playlist.h \
	../include/vlc_plugin.h \
	../include/vlc_rand.h \
	../include/vlc_services_discovery.h \
	../include/vlc_sout.h \
	../include/vlc_stream.h \
	../include/vlc_strings.h \
	../include/vlc_threads.h \
	../include/vlc_url.h \
	../include/vlc_variables.h \
	../include/vlc_vlm.h \
	../include/vlc_vout.h \
	../include/vlc_window.h \
	../include/vlc_xml.h \
	$(NULL)

noinst_HEADERS = \
	../include/mmx.h \
	../include/vlc_codec_synchro.h \
	../include/vlc_codecs.h \
	../include/vlc_devices.h \
	../include/vlc_fixups.h \
	../include/vlc_interface.h \
	../include/vlc_intf_strings.h \
	../include/vlc_iso_lang.h \
	../include/vlc_keys.h \
	../include/vlc_network.h \
	../include/vlc_osd.h \
	../include/vlc_pgpkey.h \
	../include/vlc_tls.h \
	../include/vlc_update.h \
	../include/vlc_vod.h \
	$(NULL)

../include/vlc_about.h: Makefile.am $(top_srcdir)/COPYING $(top_srcdir)/THANKS $(top_srcdir)/AUTHORS
	rm -f -- "$@.tmp"
	mkdir -p -- ../include
	echo "/* Automatically generated file - DO NOT EDIT */" > "$@.tmp"
	echo "static const char psz_license[] =" >> "$@.tmp"
	cat $(top_srcdir)/COPYING | sed s/'"'/'\\"'/g | $(AWK) '{ print "\""$$0"\\n\"" }' >> "$@.tmp"
	echo ";" >> "$@.tmp"
	echo "static const char psz_thanks[] =" >> "$@.tmp"
	grep -v '$$Id:'  $(top_srcdir)/THANKS | sed s/'"'/'\\"'/g | $(AWK) '{ print "\""$$0"\\n\"" }'|sed s/"<.*.> "// >> "$@.tmp"
	echo ";" >> "$@.tmp"
	echo "static const char psz_authors[] =" >> "$@.tmp"
	grep N: $(top_srcdir)/AUTHORS | cut -d" " -f 2- | sed s/'"'/'\\"'/g | $(AWK) '{ print "\""$$0"\\n\"" }' >> "$@.tmp"
	echo ";" >> "$@.tmp"
	mv -f -- "$@.tmp" "$@"

###############################################################################
# pkg-config integration
###############################################################################

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libvlc.pc vlc-plugin.pc
CLEANFILES += $(pkgconfig_DATA)

.pc.in.pc: $(top_builddir)/config.status
	cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="src/$@"

###############################################################################
# Building libvlc
###############################################################################

nice:
	$(top_builddir)/compile

lib_LTLIBRARIES = libvlccore.la libvlc.la

AM_LDFLAGS = -no-undefined
if HAVE_WIN32
AM_LDFLAGS += -avoid-version
endif
if HAVE_WINCE
AM_LDFLAGS += -avoid-version
endif

libvlccore_la_SOURCES = $(SOURCES_libvlc)
libvlccore_la_CFLAGS = `$(VLC_CONFIG) --cflags libvlc` \
	-DMODULE_STRING=\"main\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DDATA_PATH=\"$(vlcdatadir)\" \
	-DLIBDIR=\"$(libdir)\" \
	-DPLUGIN_PATH=\"$(vlclibdir)\"
libvlccore_la_LDFLAGS = `$(VLC_CONFIG) --ldflags libvlc` $(AM_LDFLAGS) \
	-export-symbols $(srcdir)/libvlccore.sym \
	-version-info 2:0:0
libvlccore_la_LIBADD = `$(VLC_CONFIG) -libs libvlc` $(AM_LIBADD) $(LTLIBINTL)
libvlccore_la_DEPENDENCIES = libvlccore.sym
if HAVE_WIN32
libvlccore_la_DEPENDENCIES += libvlc_win32_rc.$(OBJEXT)
libvlccore_la_LDFLAGS += -Wl,libvlc_win32_rc.$(OBJEXT)
endif

libvlc_win32_rc.$(OBJEXT): $(top_builddir)/share/libvlc_win32_rc.rc
	$(WINDRES) --include-dir $(top_srcdir)/share -i $< -o $@

libvlc_la_SOURCES = $(SOURCES_libvlc_control)
libvlc_la_LIBADD = libvlccore.la
libvlc_la_CFLAGS = `$(VLC_CONFIG) --cflags libvlc` \
	-DMODULE_STRING=\"control\"
libvlc_la_LDFLAGS = $(AM_LDFLAGS) \
	-version-info 4:0:2 \
	-export-symbols $(srcdir)/libvlc.sym
libvlc_la_DEPENDENCIES = libvlc.sym libvlccore.la
if HAVE_WIN32
libvlc_la_DEPENDENCIES += $(DATA_win32_rc_lib)
libvlc_la_LDFLAGS += -Wl,$(DATA_win32_rc_lib)
endif


EXTRA_libvlccore_la_SOURCES = \
	$(SOURCES_libvlc_beos) \
	$(SOURCES_libvlc_darwin) \
	$(SOURCES_libvlc_linux) \
	$(SOURCES_libvlc_win32) \
	$(SOURCES_libvlc_other) \
	$(SOURCES_libvlc_dirent) \
	$(SOURCES_libvlc_getopt) \
	$(SOURCES_libvlc_httpd) \
	$(SOURCES_libvlc_sout) \
	$(SOURCES_libvlc_vlm) \
	$(NULL)
if HAVE_BEOS
libvlccore_la_SOURCES += $(SOURCES_libvlc_beos)
else
if HAVE_DARWIN
libvlccore_la_SOURCES += $(SOURCES_libvlc_darwin)
else
if HAVE_LINUX
libvlccore_la_SOURCES += $(SOURCES_libvlc_linux)
else
if HAVE_WIN32
libvlccore_la_SOURCES += $(SOURCES_libvlc_win32)
else
if HAVE_WINCE
libvlccore_la_SOURCES += $(SOURCES_libvlc_win32)
else
libvlccore_la_SOURCES += $(SOURCES_libvlc_other)
endif
endif
endif
endif
endif
if BUILD_DIRENT
libvlccore_la_SOURCES += $(SOURCES_libvlc_dirent)
endif
if BUILD_GETOPT
libvlccore_la_SOURCES += $(SOURCES_libvlc_getopt)
endif
if BUILD_HTTPD
libvlccore_la_SOURCES += $(SOURCES_libvlc_httpd)
endif
if ENABLE_SOUT
libvlccore_la_SOURCES += $(SOURCES_libvlc_sout)
if ENABLE_VLM
libvlccore_la_SOURCES += $(SOURCES_libvlc_vlm)
endif
endif

SOURCES_libvlc_beos = \
	$(NULL)

SOURCES_libvlc_darwin = \
	misc/darwin_specific.c \
	$(NULL)

SOURCES_libvlc_linux = \
	misc/linux_specific.c \
	$(NULL)

SOURCES_libvlc_win32 = \
	misc/win32_specific.c \
	network/winsock.c \
	$(NULL)

SOURCES_libvlc_other = misc/not_specific.c

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
	libvlc-module.c \
	missing.c \
	version.c \
	interface/interface.h \
	interface/interface.c \
	interface/intf_eject.c \
	interface/interaction.c \
	playlist/playlist_internal.h \
	playlist/art.c \
	playlist/art.h \
	playlist/thread.c \
	playlist/control.c \
	playlist/engine.c \
	playlist/fetcher.c \
	playlist/fetcher.h \
	playlist/sort.c \
	playlist/loadsave.c \
	playlist/preparser.c \
	playlist/preparser.h \
	playlist/tree.c \
	playlist/item.c \
	playlist/search.c \
	playlist/services_discovery.c \
	input/item.c \
	input/access.c \
	input/clock.c \
	input/control.c \
	input/decoder.c \
	input/decoder_synchro.c \
	input/demux.c \
	input/es_out.c \
	input/es_out_timeshift.c \
	input/event.c \
	input/input.c \
	input/meta.c \
	input/access.h \
	input/clock.h \
	input/decoder.h \
	input/demux.h \
	input/es_out.h \
	input/es_out_timeshift.h \
	input/event.h \
	input/item.h \
	input/stream.h \
	input/input_internal.h \
	input/input_interface.h \
	input/vlm_internal.h \
	input/ressource.h \
	input/ressource.c \
	input/stream.c \
	input/stream_demux.c \
	input/stream_filter.c \
	input/stream_memory.c \
	input/subtitles.c \
	input/var.c \
	video_output/video_output.c \
	video_output/vout_pictures.c \
	video_output/vout_pictures.h \
	video_output/video_text.c \
	video_output/video_widgets.c \
	video_output/vout_subpictures.c \
	video_output/vout_intf.c \
	video_output/vout_internal.h \
	video_output/vout_control.h \
	audio_output/aout_internal.h \
	audio_output/common.c \
	audio_output/dec.c \
	audio_output/filters.c \
	audio_output/input.c \
	audio_output/mixer.c \
	audio_output/output.c \
	audio_output/intf.c \
	osd/osd.c \
	osd/osd_text.c \
	osd/osd_widgets.c \
	network/acl.c \
	network/getaddrinfo.c \
	network/io.c \
	network/tcp.c \
	network/udp.c \
	network/rootbind.c \
	network/tls.c \
	network/poll.c \
	text/charset.c \
	text/strings.c \
	text/unicode.c \
	text/filesystem.c \
	text/wincp.c \
	text/iso_lang.c \
	text/iso-639_def.h \
	misc/md5.c \
	misc/rand.c \
	misc/mtime.c \
	misc/block.c \
	misc/es_format.c \
	modules/modules.h \
	modules/modules.c \
	modules/cache.c \
	modules/entry.c \
	modules/os.c \
	misc/threads.c \
	misc/stats.c \
	misc/cpu.c \
	misc/action.c \
	config/configuration.h \
	config/core.c \
	config/dirs.c \
	config/chain.c \
	config/file.c \
	config/intf.c \
	config/cmdline.c \
	misc/events.c \
	misc/image.c \
	misc/messages.c \
	misc/objects.c \
	misc/variables.h \
	misc/variables.c \
	misc/error.c \
	misc/update.h \
	misc/update.c \
	misc/xml.c \
	misc/devices.c \
	extras/libc.c \
	misc/filter_chain.c \
	$(NULL)

SOURCES_libvlc_httpd = \
	network/httpd.c \
	$(NULL)

SOURCES_libvlc_sout = \
	stream_output/stream_output.c \
	stream_output/stream_output.h \
	stream_output/announce.c \
	stream_output/sap.c \
	stream_output/sdp.c \
	$(NULL)

SOURCES_libvlc_vlm = \
	input/vlm.c \
	input/vlmshell.c \
	$(NULL)

SOURCES_libvlc = \
	$(SOURCES_libvlc_common) \
	$(OPT_SOURCES_libvlc_beos) \
	$(OPT_SOURCES_libvlc_darwin) \
	$(OPT_SOURCES_libvlc_win32) \
	$(OPT_SOURCES_libvlc_dirent) \
	$(OPT_SOURCES_libvlc_getopt) \
	$(NULL)

SOURCES_libvlc_control = \
	control/libvlc_internal.h \
	control/core.c \
	control/log.c \
	control/playlist.c \
	control/vlm.c \
	control/video.c \
	control/audio.c \
	control/event.c \
	control/flat_media_list_view.c \
	control/hierarchical_media_list_view.c \
	control/hierarchical_node_media_list_view.c \
	control/media.c \
	control/media_player.c \
	control/media_list.c \
	control/media_list_path.h \
	control/media_list_player.c \
	control/media_list_view.c \
	control/media_library.c \
	control/mediacontrol_internal.h \
	control/mediacontrol_core.c \
	control/mediacontrol_util.c \
	control/mediacontrol_audio_video.c \
	control/media_discoverer.c

###############################################################################
# Unit/regression test
###############################################################################

dist_check_SCRIPTS = check_symbols check_headers
TESTS = check_symbols

check-local:
	for h in `echo $(pkginclude_HEADERS) | sed -e s,\.\./include/,,g`; \
	do \
		echo grep - "#include <$$h>" $(srcdir)/test/headers.c ; \
		if ! grep -- "#include <$$h>" $(srcdir)/test/headers.c ; \
		then \
			echo "Header $$h not included in test/headers.c!"; \
			exit 1; \
		fi ; \
	done
	$(srcdir)/check_headers $(pkginclude_HEADERS)
	$(srcdir)/check_headers $(pluginsinclude_HEADERS)

FORCE:
	@echo "Generated source cannot be phony. Go away." >&2
	@exit 1

.PHONY: FORCE
