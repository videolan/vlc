###############################################################################
# Automake targets and declarations
###############################################################################

AUTOMAKE_OPTIONS = subdir-objects

NULL =
EXTRA_DIST = \
	vlc-plugin.pc.in \
	revision.txt

BUILT_SOURCES = $(nodist_pluginsinclude_HEADERS)
CLEANFILES = $(BUILT_SOURCES)

SUFFIXES = .pc.in .pc .rc.in .rc

###############################################################################
# Headers
###############################################################################

pluginsincludedir = $(pkgincludedir)/plugins

pluginsinclude_HEADERS.h = \
	../include/vlc_access.h \
	../include/vlc_actions.h \
	../include/vlc_addons.h \
	../include/vlc_ancillary.h \
	../include/vlc_aout.h \
	../include/vlc_aout_volume.h \
	../include/vlc_arrays.h \
	../include/vlc_atomic.h \
	../include/vlc_avcodec.h \
	../include/vlc_bits.h \
	../include/vlc_block.h \
	../include/vlc_block_helper.h \
	../include/vlc_boxes.h \
	../include/vlc_charset.h \
	../include/vlc_codec.h \
	../include/vlc_common.h \
	../include/vlc_config.h \
	../include/vlc_config_cat.h \
	../include/vlc_configuration.h \
	../include/vlc_cpu.h \
	../include/vlc_clock.h \
	../include/vlc_decoder.h \
	../include/vlc_demux.h \
	../include/vlc_dialog.h \
	../include/vlc_diffutil.h \
	../include/vlc_epg.h \
	../include/vlc_es.h \
	../include/vlc_es_out.h \
	../include/vlc_executor.h \
	../include/vlc_filter.h \
	../include/vlc_fingerprinter.h \
	../include/vlc_fourcc.h \
	../include/vlc_frame.h \
	../include/vlc_fs.h \
	../include/vlc_gcrypt.h \
	../include/vlc_hash.h \
	../include/vlc_http.h \
	../include/vlc_httpd.h \
	../include/vlc_image.h \
	../include/vlc_inhibit.h \
	../include/vlc_input.h \
	../include/vlc_input_item.h \
	../include/vlc_interface.h \
	../include/vlc_interrupt.h \
	../include/vlc_keystore.h \
	../include/vlc_list.h \
	../include/vlc_media_library.h \
	../include/vlc_media_source.h \
	../include/vlc_memstream.h \
	../include/vlc_messages.h \
	../include/vlc_tracer.h \
	../include/vlc_meta.h \
	../include/vlc_meta_fetcher.h \
	../include/vlc_mime.h \
	../include/vlc_modules.h \
	../include/vlc_mouse.h \
	../include/vlc_network.h \
	../include/vlc_objects.h \
	../include/vlc_opengl.h \
	../include/vlc_opengl_interop.h \
	../include/vlc_opengl_filter.h \
	../include/vlc_picture.h \
	../include/vlc_picture_fifo.h \
	../include/vlc_picture_pool.h \
	../include/vlc_player.h \
	../include/vlc_playlist.h \
	../include/vlc_playlist_export.h \
	../include/vlc_plugin.h \
	../include/vlc_poll.h \
	../include/vlc_probe.h \
	../include/vlc_preparser.h \
	../include/vlc_queue.h \
	../include/vlc_rand.h \
	../include/vlc_renderer_discovery.h \
	../include/vlc_services_discovery.h \
	../include/vlc_sort.h \
	../include/vlc_sout.h \
	../include/vlc_spawn.h \
	../include/vlc_spu.h \
	../include/vlc_stream.h \
	../include/vlc_stream_extractor.h \
	../include/vlc_strings.h \
	../include/vlc_subpicture.h \
	../include/vlc_text_style.h \
	../include/vlc_threads.h \
	../include/vlc_thumbnailer.h \
	../include/vlc_tick.h \
	../include/vlc_timestamp_helper.h \
	../include/vlc_tls.h \
	../include/vlc_url.h \
	../include/vlc_variables.h \
	../include/vlc_vector.h \
	../include/vlc_video_splitter.h \
	../include/vlc_viewpoint.h \
	../include/vlc_vlm.h \
	../include/vlc_vout.h \
	../include/vlc_vout_display.h \
	../include/vlc_vout_osd.h \
	../include/vlc_window.h \
	../include/vlc_xlib.h \
	../include/vlc_xml.h \
	$(NULL)

pluginsinclude_HEADERS.hpp = \
	../include/vlc_cxx_helpers.hpp \
	../include/vlc_list.hpp \
	$(NULL)

pluginsinclude_HEADERS = \
	$(pluginsinclude_HEADERS.h) \
	$(pluginsinclude_HEADERS.hpp)
nodist_pluginsinclude_HEADERS = ../include/vlc_about.h

noinst_HEADERS = \
	../include/vlc_codecs.h \
	../include/vlc_extensions.h \
	../include/vlc_fixups.h \
	../include/vlc_intf_strings.h \
	../include/vlc_iso_lang.h \
	../include/vlc_pgpkey.h \
	../include/vlc_update.h \
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

vlc-plugin.pc: vlc-plugin.pc.in $(top_builddir)/config.status
	$(AM_V_GEN)cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="src/$@"

##############################################################################
# Windows resource files
##############################################################################

if HAVE_WIN32
noinst_DATA = libvlccore_win32_rc.rc
endif
EXTRA_DIST += libvlccore_win32_rc.rc.in

libvlccore_win32_rc.rc: libvlccore_win32_rc.rc.in $(top_builddir)/config.status
	$(AM_V_GEN)cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="src/$@"

###############################################################################
# Building libvlc
###############################################################################

lib_LTLIBRARIES = libvlccore.la

AM_CPPFLAGS = $(INCICONV) $(IDN_CFLAGS) \
	-DMODULE_STRING=\"core\" \
	-DSYSDATADIR=\"$(datadir)\" \
	-DLIBDIR=\"$(libdir)\" \
	-DLIBEXECDIR=\"$(libexecdir)\" \
	-DLOCALEDIR=\"$(localedir)\" \
	-DPKGDATADIR=\"$(pkgdatadir)\" \
	-DPKGLIBDIR=\"$(pkglibdir)\" \
	-DPKGLIBEXECDIR=\"$(pkglibexecdir)\"
AM_CFLAGS = $(CFLAGS_libvlccore)
if HAVE_DYNAMIC_PLUGINS
AM_CPPFLAGS += -DHAVE_DYNAMIC_PLUGINS
if HAVE_WIN32
AM_CPPFLAGS += -DVLC_DLL_EXPORT
endif
endif
if HAVE_DBUS
AM_CPPFLAGS += -DHAVE_DBUS
AM_CFLAGS += $(DBUS_CFLAGS)
endif

libvlccore_la_SOURCES = \
	libvlc.c \
	libvlc.h \
	libvlc-module.c \
	missing.c \
	revision.c \
	version.c \
	config/ansi_term.h \
	config/configuration.h \
	config/cat.c \
	config/core.c \
	config/chain.c \
	config/dirs.c \
	config/file.c \
	config/help.c \
	config/intf.c \
	config/cmdline.c \
	config/getopt.c \
	config/vlc_getopt.h \
	config/jaro_winkler.c \
	config/vlc_jaro_winkler.h \
	extras/libc.c \
	media_source/media_source.c \
	media_source/media_source.h \
	media_source/media_tree.c \
	modules/modules.h \
	modules/modules.c \
	modules/bank.c \
	modules/cache.c \
	modules/entry.c \
	modules/textdomain.c \
	interface/dialog.c \
	interface/interface.c \
	playlist/content.c \
	playlist/content.h \
	playlist/control.c \
	playlist/control.h \
	playlist/export.c \
	playlist/item.c \
	playlist/item.h \
	playlist/notify.c \
	playlist/notify.h \
	playlist/player.c \
	playlist/player.h \
	playlist/playlist.c \
	playlist/playlist.h \
	playlist/preparse.c \
	playlist/preparse.h \
	playlist/randomizer.c \
	playlist/randomizer.h \
	playlist/request.c \
	playlist/shuffle.c \
	playlist/sort.c \
	preparser/art.c \
	preparser/art.h \
	preparser/fetcher.c \
	preparser/fetcher.h \
	preparser/preparser.c \
	input/item.c \
	input/access.c \
	clock/clock_internal.c \
	clock/input_clock.c \
	clock/clock.c \
	input/decoder.c \
	input/decoder_device.c \
	input/decoder_helpers.c \
	input/demux.c \
	input/demux_chained.c \
	input/es_out.c \
	input/es_out_source.c \
	input/es_out_timeshift.c \
	input/input.c \
	input/info.h \
	input/meta.c \
	input/attachment.c \
	input/parse.c \
	player/player.c \
	player/player.h \
	player/input.c \
	player/timer.c \
	player/track.c \
	player/title.c \
	player/aout.c \
	player/vout.c \
	player/osd.c \
	player/medialib.c \
	player/metadata.c \
	clock/input_clock.h \
	clock/clock.h \
	clock/clock_internal.h \
	input/decoder.h \
	input/demux.h \
	input/es_out.h \
	input/event.h \
	input/item.h \
	input/mrl_helpers.h \
	input/stream.h \
	input/input_internal.h \
	input/input_interface.h \
	input/vlm_internal.h \
	input/vlm_event.h \
	input/resource.h \
	input/resource.c \
	input/services_discovery.c \
	input/source.c \
	input/source.h \
	input/stats.c \
	input/stream.c \
	input/stream_fifo.c \
	input/stream_extractor.c \
	input/stream_filter.c \
	input/stream_memory.c \
	input/subtitles.c \
	input/thumbnailer.c \
	input/var.c \
	audio_output/aout_internal.h \
	audio_output/common.c \
	audio_output/dec.c \
	audio_output/filters.c \
	audio_output/meter.c \
	audio_output/output.c \
	audio_output/volume.c \
	video_output/chrono.h \
	video_output/control.c \
	video_output/control.h \
	video_output/display.c \
	video_output/display.h \
	video_output/inhibit.c \
	video_output/inhibit.h \
	video_output/interlacing.c \
	video_output/snapshot.c \
	video_output/snapshot.h \
	video_output/statistic.h \
	video_output/video_output.c \
	video_output/video_text.c \
	video_output/video_epg.c \
	video_output/video_widgets.c \
	video_output/vout_subpictures.c \
	video_output/vout_spuregion_helper.h \
	video_output/vout_wrapper.h \
	video_output/video_window.c \
	video_output/video_window.h \
	video_output/window.c \
	video_output/opengl.c \
	video_output/vout_intf.c \
	video_output/vout_internal.h \
	video_output/vout_private.h \
	video_output/vout_wrapper.c \
	network/getaddrinfo.c \
	network/http_auth.c \
	network/httpd.c \
	network/io.c \
	network/tcp.c \
	network/udp.c \
	network/rootbind.c \
	network/stream.c \
	network/tls.c \
	text/charset.c \
	text/memstream.c \
	text/strings.c \
	text/unicode.c \
	text/url.c \
	text/filesystem.c \
	text/iso_lang.c \
	text/iso-639_def.h \
	misc/actions.c \
	misc/ancillary.h \
	misc/ancillary.c \
	misc/executor.c \
	misc/md5.c \
	misc/probe.c \
	misc/rand.c \
	misc/mtime.c \
	misc/frame.c \
	misc/fifo.c \
	misc/filesystem.c \
	misc/fourcc.c \
	misc/fourcc_list.h \
	misc/es_format.c \
	misc/picture.c \
	misc/picture.h \
	misc/picture_fifo.c \
	misc/picture_pool.c \
	misc/interrupt.h \
	misc/interrupt.c \
	misc/keystore.c \
	misc/rcu.h \
	misc/rcu.c \
	misc/renderer_discovery.c \
	misc/threads.c \
	misc/threads.h \
	misc/cpu.c \
	misc/diffutil.c \
	misc/epg.c \
	misc/exit.c \
	misc/image.c \
	misc/messages.c \
	misc/tracer.c \
	misc/mime.c \
	misc/objects.c \
	misc/objres.c \
	misc/queue.c \
	misc/variables.h \
	misc/variables.c \
	misc/xml.c \
	misc/addons.c \
	misc/filter.c \
	misc/filter_chain.c \
	misc/httpcookies.c \
	misc/fingerprinter.c \
	misc/text_style.c \
	misc/sort.c \
	misc/subpicture.c \
	misc/subpicture.h \
	misc/medialibrary.c \
	misc/viewpoint.c
libvlccore_la_LIBADD = $(LIBS_libvlccore) \
	../compat/libcompat.la \
	$(LTLIBINTL) $(LTLIBICONV) \
	$(IDN_LIBS) $(SOCKET_LIBS) $(LIBRT) $(LIBDL) $(LIBM)

libvlccore_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_libvlccore)

if HAVE_WIN32
libvlccore_la_SOURCES += \
	win32/error.c \
	win32/filesystem.c \
	win32/netconf.c \
	win32/plugin.c \
	win32/rand.c \
	win32/specific.c \
	win32/thread.c
if HAVE_WINSTORE
libvlccore_la_SOURCES += posix/timer.c win32/dirs-uap.c
else
libvlccore_la_SOURCES += win32/timer.c win32/dirs.c win32/spawn.c
libvlccore_la_LIBADD += -lwinmm -lole32 -luuid
endif
libvlccore_la_LIBADD += -lbcrypt
endif

if HAVE_OS2
libvlccore_la_SOURCES += \
	os2/dirs.c \
	darwin/error.c \
	os2/filesystem.c \
	os2/getaddrinfo.c \
	os2/netconf.c \
	os2/plugin.c \
	os2/specific.c \
	os2/rand.c \
	os2/thread.c
endif

if HAVE_EMSCRIPTEN
libvlccore_la_SOURCES += \
	emscripten/netconf.c \
	emscripten/thread.c
endif

if HAVE_DARWIN
libvlccore_la_SOURCES += \
	darwin/error.c \
	darwin/specific.c \
	darwin/thread.c
libvlccore_objc_la_SOURCES = \
	darwin/dirs.m \
	darwin/netconf.m
noinst_LTLIBRARIES = libvlccore_objc.la
endif

if HAVE_FREEBSD
libvlccore_la_SOURCES += \
	freebsd/cpu.c \
	freebsd/thread.c
endif

if HAVE_LINUX
libvlccore_la_SOURCES += \
	linux/cpu.c \
	linux/dirs.c \
	linux/thread.c
if !HAVE_ANDROID
libvlccore_la_SOURCES += \
	linux/filesystem.c
else
libvlccore_la_SOURCES += \
	android/error.c \
	android/specific.c \
	android/thread.c
endif
endif

if !HAVE_WIN32
if !HAVE_OS2
libvlccore_la_SOURCES += \
	posix/filesystem.c \
	posix/plugin.c \
	posix/rand.c \
	posix/timer.c
if !HAVE_LINUX
libvlccore_la_SOURCES += posix/wait.c
endif
if !HAVE_ANDROID
if !HAVE_EMSCRIPTEN
libvlccore_la_SOURCES += posix/sort.c
endif
libvlccore_la_SOURCES += \
	posix/thread.c
libvlccore_la_LIBADD += $(LIBEXECINFO)
if !HAVE_DARWIN
if !HAVE_EMSCRIPTEN
libvlccore_la_SOURCES += posix/netconf.c
endif
libvlccore_la_SOURCES += \
	posix/dirs.c \
	posix/error.c \
	posix/picture.c \
	posix/spawn.c \
	posix/specific.c
if HAVE_LIBANL
libvlccore_la_SOURCES += \
	linux/getaddrinfo.c
libvlccore_la_LIBADD += -lanl
else
libvlccore_la_SOURCES += \
	posix/getaddrinfo.c
endif
endif
endif
endif
endif

libvlccore_la_SOURCES += \
	stream_output/sap.c \
	stream_output/stream_output.c stream_output/stream_output.h
if ENABLE_VLM
libvlccore_la_SOURCES += input/vlm.c input/vlm_event.c input/vlmshell.c
endif

if UPDATE_CHECK
libvlccore_la_SOURCES += \
	misc/update.h misc/update.c \
	misc/update_crypto.c
AM_CPPFLAGS += -DUPDATE_CHECK
AM_CFLAGS += $(GCRYPT_CFLAGS)
libvlccore_la_LIBADD += $(GCRYPT_LIBS)
endif

libvlccore_la_LDFLAGS = \
	$(LDFLAGS_libvlccore) \
	-no-undefined \
	-export-symbols $(srcdir)/libvlccore.sym \
	-version-info 9:0:0
libvlccore_la_SOURCES += libvlccore.sym
EXTRA_libvlccore_la_DEPENDENCIES = libvlccore.sym

if HAVE_WIN32
EXTRA_libvlccore_la_DEPENDENCIES += libvlccore_win32_rc.$(OBJEXT)
libvlccore_la_LDFLAGS += -Wl,libvlccore_win32_rc.$(OBJEXT) -avoid-version -Wc,-static
endif
if HAVE_OS2
libvlccore_la_LDFLAGS += -avoid-version
endif
if HAVE_DBUS
libvlccore_la_LIBADD += $(DBUS_LIBS)
endif
if HAVE_DARWIN
libvlccore_objc_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-arc
libvlccore_objc_la_LDFLAGS = -static
libvlccore_la_LIBADD += libvlccore_objc.la
libvlccore_la_LDFLAGS +=  -Wl,-framework,Foundation -Xlinker -install_name -Xlinker @rpath/libvlccore.dylib
endif

# iOS and tvOS applications cannot install global shared libraries and
# dylibs must be in frameworks so there's no need for libtool versioning.
if HAVE_IOS
libvlccore_la_LDFLAGS += -avoid-version
endif
if HAVE_TVOS
libvlccore_la_LDFLAGS += -avoid-version
endif

libvlccore_win32_rc.$(OBJEXT): libvlccore_win32_rc.rc $(top_srcdir)/extras/package/win32/libvlccore.dll.manifest
	$(WINDRES) --include-dir $(top_srcdir)/share --include-dir $(top_srcdir)/extras/package/win32 -i $< -o $@

# FourCC tables
BUILT_SOURCES += fourcc_tables.h
EXTRA_DIST += misc/fourcc_gen.c
MOSTLYCLEANFILES = fourcc_gen$(BUILDEXEEXT)

fourcc_gen$(BUILDEXEEXT): misc/fourcc_gen.c misc/fourcc_list.h ../include/vlc_fourcc.h
	$(AM_V_at)rm -f -- $@
	$(AM_V_CC)$(BUILDCC) -I$(srcdir) -o $@ $<

fourcc_tables.h: fourcc_gen$(BUILDEXEEXT)
	$(AM_V_at)rm -f -- $@.tmp
	$(AM_V_GEN)$(builddir)/fourcc_gen$(BUILDEXEEXT) > $@.tmp
	$(AM_V_at)mv -f -- $@.tmp $@

# Unit/regression tests
#
check_PROGRAMS = \
	test_block \
	test_dictionary \
	test_executor \
	test_i18n_atof \
	test_interrupt \
	test_jaro_winkler \
	test_list \
	test_md5 \
	test_objects_cxx \
	test_picture_pool \
	test_sort \
	test_timer \
	test_url \
	test_utf8 \
	test_xmlent \
	test_headers \
	test_mrl_helpers \
	test_arrays \
	test_vector \
	test_shared_data_ptr \
	test_playlist \
	test_randomizer \
	test_media_source \
	test_extensions \
	test_thread \
	test_diffutil \
	test_cpp_list

TESTS = $(check_PROGRAMS) check_symbols

test_block_SOURCES = test/block_test.c
test_block_LDADD = $(LDADD) $(LIBS_libvlccore)
test_dictionary_SOURCES = test/dictionary.c
test_executor_SOURCES = test/executor.c
test_i18n_atof_SOURCES = test/i18n_atof.c
test_interrupt_SOURCES = test/interrupt.c
test_interrupt_LDADD = $(LDADD) $(LIBS_libvlccore)
test_jaro_winkler_SOURCES = test/jaro_winkler.c config/jaro_winkler.c
test_list_SOURCES = test/list.c
test_md5_SOURCES = test/md5.c
test_objects_cxx_SOURCES = misc/objects_cxx_test.cpp misc/objects.c
test_picture_pool_SOURCES = test/picture_pool.c
test_sort_SOURCES = test/sort.c
test_timer_SOURCES = test/timer.c
test_url_SOURCES = test/url.c
test_utf8_SOURCES = test/utf8.c
test_xmlent_SOURCES = test/xmlent.c
test_headers_SOURCES = test/headers.c
test_mrl_helpers_SOURCES = test/mrl_helpers.c
test_arrays_SOURCES = test/arrays.c
test_vector_SOURCES = test/vector.c
test_shared_data_ptr_SOURCES = test/shared_data_ptr.cpp
test_extensions_SOURCES = test/extensions.c
test_diffutil_SOURCES = test/diffutil.c
test_playlist_SOURCES = playlist/test.c \
	playlist/content.c \
	playlist/control.c \
	playlist/item.c \
	playlist/notify.c \
	playlist/player.c \
	playlist/playlist.c \
	playlist/preparse.c \
	playlist/randomizer.c \
	playlist/request.c \
	playlist/shuffle.c \
	playlist/sort.c
test_playlist_CFLAGS = -DTEST_PLAYLIST
test_randomizer_SOURCES = playlist/randomizer.c
test_randomizer_CFLAGS = -DTEST_RANDOMIZER
test_media_source_LDADD = $(LDADD) $(LIBS_libvlccore)
test_media_source_CFLAGS = -DTEST_MEDIA_SOURCE
test_media_source_SOURCES = media_source/test.c \
	media_source/media_source.c \
	media_source/media_tree.c
test_thread_SOURCES = test/thread.c
test_cpp_list_SOURCES = test/list_cpp.cpp

LDADD = libvlccore.la \
	../compat/libcompat.la

###############################################################################
# GIT revision
###############################################################################

BUILT_SOURCES += stamp-revision
MAINTAINERCLEANFILES = $(srcdir)/revision.txt $(srcdir)/revision.c

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
			-C "$(top_srcdir)" describe \
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

checkheader_verbose = $(checkheader_verbose_$(V))
checkheader_verbose_ = $(checkheader_verbose_$(AM_DEFAULT_VERBOSITY))
checkheader_verbose__0 = $(checkheader_verbose_0)

checkheaders.h   = $(pluginsinclude_HEADERS.h:../include/%=checkheader_%.check)
checkheaders.hpp = $(pluginsinclude_HEADERS.hpp:../include/%=checkheader_%.check)
checkheaders.all = $(checkheaders.h) $(checkheaders.hpp)

$(checkheaders.all): checkheader_verbose_0 = @echo "  CHECK  $(@:checkheader_%.check=%)";
$(checkheaders.all): skip_checkheader_verbose_0 = @echo "  CHECK  $(@:checkheader_%.check=%) (SKIPPED)";
#$(checkheaders.hpp): checkheader_verbose_0 = @echo "  CHECK  $(@:checkheader_%_hpp=%.hpp)";
#$(checkheaders.hpp): skip_checkheader_verbose_0 = @echo "  CHECK  $(@:checkheader_%_hpp=%.hpp) (SKIPPED)";

nodist_check_SCRIPTS = $(checkheaders.all)
MOSTLYCLEANFILES += \
	$(checkheaders.h:%=%.c) $(checkheaders.h:%=%.lo) \
	$(checkheaders.hpp:%=%.cpp) $(checkheaders.hpp:%=%.lo)

checkheaders: $(checkheaders.all)
checkheader_%.check:
	$(checkheader_verbose) \
	sourcefile="$@.$(CHECKHEADER_SOURCE_EXT)"; exec 3>$${sourcefile} && \
	echo "#include <stdbool.h>" >&3 && \
	echo "#include <vlc_common.h>" >&3 && \
	echo "#include <$*>" >&3 && \
	$(CHECKCOMPILE) $(CHECK_CFLAGS) -c $${sourcefile} -o $@.lo
$(checkheaders.h): CHECKHEADER_SOURCE_EXT=c
$(checkheaders.h): CHECKCOMPILE=$(LTCOMPILE)
$(checkheaders.hpp): CHECKHEADER_SOURCE_EXT=cpp
$(checkheaders.hpp): CHECKCOMPILE=$(LTCXXCOMPILE)

# Some exceptions for headers depending on other libraries
checkheader_vlc_gcrypt.h.check: CHECK_CFLAGS = "-Dgcry_check_version(x)="
checkheader_vlc_xlib.h.check: CHECK_CFLAGS = $(X_CFLAGS)
checkheader_vlc_list.hpp.check: CHECK_CFLAGS = "-Drestrict="

SKIPPED_CHECKHEADERS =
if !HAVE_X
SKIPPED_CHECKHEADERS += vlc_xlib.h
endif

$(SKIPPED_CHECKHEADERS:%=checkheader_%.check): checkheader_verbose_0 = $(skip_checkheader_verbose_0)
$(SKIPPED_CHECKHEADERS:%=checkheader_%.check): CHECKCOMPILE = exit 0;

dist_check_SCRIPTS = check_headers check_symbols

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
	$(SHELL) $(srcdir)/check_headers $(pluginsinclude_HEADERS)

FORCE:
	@echo "Generated source cannot be phony. Go away." >&2
	@exit 1

.PHONY: FORCE
