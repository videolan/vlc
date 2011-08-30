###############################################################################
# Automake targets and declarations
###############################################################################

AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = . test

NULL =
EXTRA_DIST = \
	vlc-plugin.pc.in \
	libvlccore.sym \
	revision.txt

BUILT_SOURCES = $(nodist_pluginsinclude_HEADERS)
CLEANFILES = $(BUILT_SOURCES)

SUFFIXES = .pc.in .pc .rc.in .rc

###############################################################################
# Headers
###############################################################################

pluginsincludedir = $(pkgincludedir)/plugins

pluginsinclude_HEADERS = \
	../include/vlc_access.h \
	../include/vlc_acl.h \
	../include/vlc_aout.h \
	../include/vlc_aout_intf.h \
	../include/vlc_aout_mixer.h \
	../include/vlc_arrays.h \
	../include/vlc_art_finder.h \
	../include/vlc_atomic.h \
	../include/vlc_avcodec.h \
	../include/vlc_bits.h \
	../include/vlc_block.h \
	../include/vlc_block_helper.h \
	../include/vlc_charset.h \
	../include/vlc_codec.h \
	../include/vlc_common.h \
	../include/vlc_config.h \
	../include/vlc_config_cat.h \
	../include/vlc_configuration.h \
	../include/vlc_cpu.h \
	../include/vlc_dialog.h \
	../include/vlc_demux.h \
	../include/vlc_epg.h \
	../include/vlc_es.h \
	../include/vlc_es_out.h \
	../include/vlc_events.h \
	../include/vlc_filter.h \
	../include/vlc_fourcc.h \
	../include/vlc_fs.h \
	../include/vlc_gcrypt.h \
	../include/vlc_opengl.h \
	../include/vlc_http.h \
	../include/vlc_httpd.h \
	../include/vlc_image.h \
	../include/vlc_inhibit.h \
	../include/vlc_input.h \
	../include/vlc_input_item.h \
	../include/vlc_main.h \
	../include/vlc_md5.h \
	../include/vlc_messages.h \
	../include/vlc_meta.h \
	../include/vlc_media_library.h \
	../include/vlc_modules.h \
	../include/vlc_mouse.h \
	../include/vlc_mtime.h \
	../include/vlc_objects.h \
	../include/vlc_picture.h \
	../include/vlc_picture_fifo.h \
	../include/vlc_picture_pool.h \
	../include/vlc_playlist.h \
	../include/vlc_plugin.h \
	../include/vlc_probe.h \
	../include/vlc_rand.h \
	../include/vlc_services_discovery.h \
	../include/vlc_sql.h \
	../include/vlc_sout.h \
	../include/vlc_spu.h \
	../include/vlc_stream.h \
	../include/vlc_strings.h \
	../include/vlc_subpicture.h \
	../include/vlc_text_style.h \
	../include/vlc_threads.h \
	../include/vlc_url.h \
	../include/vlc_variables.h \
	../include/vlc_vlm.h \
	../include/vlc_video_splitter.h \
	../include/vlc_vout.h \
	../include/vlc_vout_display.h \
	../include/vlc_vout_osd.h \
	../include/vlc_vout_window.h \
	../include/vlc_xml.h \
	../include/vlc_xlib.h \
	$(NULL)
nodist_pluginsinclude_HEADERS = ../include/vlc_about.h

noinst_HEADERS = \
	../include/vlc_codec_synchro.h \
	../include/vlc_codecs.h \
	../include/vlc_extensions.h \
	../include/vlc_fixups.h \
	../include/vlc_interface.h \
	../include/vlc_intf_strings.h \
	../include/vlc_iso_lang.h \
	../include/vlc_keys.h \
	../include/vlc_memory.h \
	../include/vlc_network.h \
	../include/vlc_osd.h \
	../include/vlc_pgpkey.h \
	../include/vlc_tls.h \
	../include/vlc_update.h \
	../include/vlc_vod.h \
	../include/vlc_vout_wrapper.h \
	../include/vlc_windows_interfaces.h \
	$(NULL)

../include/vlc_about.h: Makefile.am $(top_srcdir)/COPYING $(top_srcdir)/THANKS $(top_srcdir)/AUTHORS
	$(AM_V_at)rm -f -- "$@.tmp"
	$(AM_V_at)mkdir -p -- ../include
	$(AM_V_GEN)(echo "/* Automatically generated file - DO NOT EDIT */" && \
	echo "static const char psz_license[] =" && \
	sed 's/"/\\"/g;s/^.*$$/\"&\\n\"/' "$(top_srcdir)/COPYING" && \
	echo ";" && \
	echo "static const char psz_thanks[] =" && \
	sed '/\$$Id:/d;s/"/\\"/g;s/<.*.> //;s/^.*$$/\"&\\n\"/' \
		"$(top_srcdir)/THANKS" && \
	echo ";" && \
	echo "static const char psz_authors[] =" && \
	sed '/\$$Id:/d;s/"/\\"/g;s/<.*.> //;s/^.*$$/\"&\\n\"/' \
		"$(top_srcdir)/AUTHORS" && \
	echo ";") >> "$@.tmp"
	$(AM_V_at)mv -f -- "$@.tmp" "$@"

###############################################################################
# pkg-config integration
###############################################################################

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = vlc-plugin.pc
CLEANFILES += $(pkgconfig_DATA)

.pc.in.pc: $(top_builddir)/config.status
	$(AM_V_GEN)cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="src/$@"

##############################################################################
# Windows resource files
##############################################################################

if HAVE_WIN32
noinst_DATA = libvlc_win32_rc.rc
endif
EXTRA_DIST += libvlc_win32_rc.rc.in

.rc.in.rc: $(top_builddir)/config.status
	cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="src/$@"

###############################################################################
# Building libvlc
###############################################################################

lib_LTLIBRARIES = libvlccore.la

AM_CPPFLAGS = $(INCICONV) \
	-DMODULE_STRING=\"main\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DSYSCONFDIR=\"$(sysconfdir)\" \
	-DDATA_PATH=\"$(vlcdatadir)\" \
	-DPKGLIBDIR=\"$(vlclibdir)\"
AM_CFLAGS = $(CFLAGS_libvlccore)

libvlccore_la_SOURCES = $(SOURCES_libvlc)
libvlccore_la_LDFLAGS = \
	-no-undefined \
	$(LDFLAGS_libvlccore) \
	-no-undefined \
	-export-symbols $(srcdir)/libvlccore.sym \
	-version-info 5:0:0
libvlccore_la_LIBADD = $(LIBS_libvlccore) \
	$(LTLIBINTL) $(LTLIBICONV) \
	$(SOCKET_LIBS) $(LIBDL) $(LIBM) \
	../compat/libcompat.la
libvlccore_la_DEPENDENCIES = libvlccore.sym
if HAVE_WIN32
libvlccore_la_DEPENDENCIES += libvlc_win32_rc.$(OBJEXT)
libvlccore_la_LDFLAGS += -Wl,libvlc_win32_rc.$(OBJEXT) -avoid-version
endif

libvlc_win32_rc.$(OBJEXT): libvlc_win32_rc.rc
	$(WINDRES) --include-dir $(top_srcdir)/share -i $< -o $@

EXTRA_libvlccore_la_SOURCES = \
	$(SOURCES_libvlc_darwin) \
	$(SOURCES_libvlc_linux) \
	$(SOURCES_libvlc_win32) \
	$(SOURCES_libvlc_other) \
	$(SOURCES_libvlc_httpd) \
	$(SOURCES_libvlc_sout) \
	$(SOURCES_libvlc_vlm) \
	$(NULL)
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
if HAVE_SYMBIAN
#libvlccore_la_SOURCES += $(SOURCES_libvlc_symbian)
else
libvlccore_la_SOURCES += $(SOURCES_libvlc_other)
endif
endif
endif
endif
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

SOURCES_libvlc_darwin = \
	posix/darwin_dirs.c \
	misc/atomic.c \
	posix/filesystem.c \
	posix/plugin.c \
	network/poll.c \
	posix/thread.c \
	posix/darwin_specific.c \
	$(NULL)

SOURCES_libvlc_linux = \
	posix/dirs.c \
	misc/atomic.c \
	posix/filesystem.c \
	posix/plugin.c \
	network/poll.c \
	posix/thread.c \
	posix/linux_specific.c \
	$(NULL)

SOURCES_libvlc_win32 = \
	win32/dirs.c \
	win32/atomic.c \
	win32/filesystem.c \
	win32/plugin.c \
	win32/poll.c \
	win32/thread.c \
	win32/specific.c \
	win32/winsock.c \
	$(NULL)

SOURCES_libvlc_symbian = \
	symbian/path.cpp \
	symbian/dirs.c \
	misc/atomic.c \
	win32/plugin.c \
	$(NULL)

SOURCES_libvlc_other = \
	posix/dirs.c \
	misc/atomic.c \
	posix/filesystem.c \
	network/poll.c \
	posix/thread.c \
	posix/specific.c \
	$(NULL)

SOURCES_libvlc_common = \
	libvlc.c \
	libvlc.h \
	libvlc-module.c \
	missing.c \
	revision.c \
	version.c \
	interface/dialog.c \
	interface/interface.c \
	interface/intf_eject.c \
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
	input/info.h \
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
	input/vlm_event.h \
	input/resource.h \
	input/resource.c \
	input/stream.c \
	input/stream_demux.c \
	input/stream_filter.c \
	input/stream_memory.c \
	input/subtitles.c \
	input/var.c \
	video_output/chrono.h \
	video_output/control.c \
	video_output/control.h \
	video_output/display.c \
	video_output/display.h \
	video_output/event.h \
	video_output/inhibit.c \
	video_output/inhibit.h \
	video_output/interlacing.c \
	video_output/interlacing.h \
	video_output/snapshot.c \
	video_output/snapshot.h \
	video_output/statistic.h \
	video_output/postprocessing.c \
	video_output/postprocessing.h \
	video_output/video_output.c \
	video_output/video_text.c \
	video_output/video_epg.c \
	video_output/video_widgets.c \
	video_output/vout_subpictures.c \
	video_output/window.c \
	video_output/opengl.c \
	video_output/vout_intf.c \
	video_output/vout_internal.h \
	video_output/vout_control.h \
	video_output/vout_wrapper.c \
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
	network/acl.c \
	network/getaddrinfo.c \
	network/io.c \
	network/tcp.c \
	network/udp.c \
	network/rootbind.c \
	network/tls.c \
	text/charset.c \
	text/strings.c \
	text/unicode.c \
	text/filesystem.c \
	text/iso_lang.c \
	text/iso-639_def.h \
	misc/md5.c \
	misc/probe.c \
	misc/rand.c \
	misc/mtime.c \
	misc/block.c \
	misc/fourcc.c \
	misc/es_format.c \
	misc/picture.c \
	misc/picture_fifo.c \
	misc/picture_pool.c \
	modules/modules.h \
	modules/modules.c \
	modules/bank.c \
	modules/cache.c \
	modules/entry.c \
	modules/textdomain.c \
	misc/threads.c \
	misc/stats.c \
	misc/cpu.c \
	misc/epg.c \
	misc/exit.c \
	config/configuration.h \
	config/core.c \
	config/chain.c \
	config/file.c \
        config/help.c \
	config/intf.c \
	config/keys.c \
	config/cmdline.c \
	config/dirs.c \
	config/getopt.c \
	config/vlc_getopt.h \
	misc/events.c \
	misc/image.c \
	misc/messages.c \
	misc/objects.c \
	misc/variables.h \
	misc/variables.c \
	misc/error.c \
	misc/update.h \
	misc/update.c \
	misc/update_crypto.c \
	misc/xml.c \
	misc/media_library.c \
	extras/libc.c \
	extras/tdestroy.c \
	misc/filter.c \
	misc/filter_chain.c \
	misc/http_auth.c \
	misc/sql.c \
	misc/text_style.c \
	misc/subpicture.c \
	misc/subpicture.h \
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
	input/vlm_event.c \
	input/vlmshell.c \
	$(NULL)

SOURCES_libvlc = \
	$(SOURCES_libvlc_common) \
	$(NULL)

###############################################################################
# libvlc pulse
###############################################################################

if HAVE_PULSE
pkglib_LTLIBRARIES = libvlcpulse.la
endif

libvlcpulse_la_SOURCES = pulse/mainloop.c ../include/vlc_pulse.h
libvlcpulse_la_CPPFLAGS = $(PULSE_CFLAGS)
libvlcpulse_la_LIBADD = $(PULSE_LIBS) libvlccore.la
libvlcpulse_la_LDFLAGS = -export-symbols-regex ^vlc_pa_ -no-undefined

###############################################################################
# GIT revision
###############################################################################

BUILT_SOURCES += stamp-revision

$(srcdir)/revision.c: $(srcdir)/revision.txt
	$(AM_V_at)rm -f -- $@
	$(AM_V_GEN)echo "const char psz_vlc_changeset[] = \"$$(cat $<)\";" \
		> $@

$(srcdir)/revision.txt:
	$(AM_V_at)$(MAKE) stamp-revision
	$(AM_V_GEN)touch $@

stamp-revision:
	$(AM_V_at)rm -f -- revision.tmp
	$(AM_V_GEN)if ! git \
			--git-dir="$(top_srcdir)/.git/" describe \
			--tags --long --match '?.*.*' --always; then \
		cat $(srcdir)/revision.txt ; \
	fi > revision.tmp
	$(AM_V_at)if diff revision.tmp $(srcdir)/revision.txt >/dev/null 2>&1; then \
		rm -f -- revision.tmp; \
	else \
		mv -f -- revision.tmp $(srcdir)/revision.txt; \
	fi
#2>&1

###############################################################################
# Unit/regression test
###############################################################################

dist_check_SCRIPTS = check_headers

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
	$(srcdir)/check_headers $(pluginsinclude_HEADERS)

FORCE:
	@echo "Generated source cannot be phony. Go away." >&2
	@exit 1

.PHONY: FORCE
