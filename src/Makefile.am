###############################################################################
# Automake targets and declarations
###############################################################################

AUTOMAKE_OPTIONS = subdir-objects

NULL =
EXTRA_DIST = extras/COPYING modules/builtin.h.in \
	vlc-control.pc.in \
	vlc-plugin.pc.in \
	libvlc.sym

BUILT_SOURCES = modules/builtin.h misc/revision.c ../include/vlc_about.h
CLEANFILES = $(BUILT_SOURCES)

SUFFIXES = .pc.in .pc

###############################################################################
# Headers
###############################################################################

pkgincludedir = $(includedir)/vlc

dist_pkginclude_HEADERS = \
	../include/vlc/vlc.h \
	../include/vlc/common.h \
	../include/vlc/deprecated.h \
	../include/vlc/libvlc.h \
	../include/vlc/libvlc_structures.h \
	../include/vlc/libvlc_media_list.h \
	../include/vlc/libvlc_events.h \
	../include/vlc/libvlc_vlm.h \
	../include/vlc/mediacontrol.h \
	../include/vlc/mediacontrol_structures.h \
	$(NULL)

noinst_HEADERS = $(HEADERS_include)

HEADERS_include = \
	../include/iso_lang.h \
	../include/main.h \
	../include/mmx.h \
	../include/vlc_access.h \
	../include/vlc_acl.h \
	../include/vlc_aout.h \
	../include/vlc_arrays.h \
	../include/vlc_bits.h \
	../include/vlc_block.h \
	../include/vlc_block_helper.h \
	../include/vlc_charset.h \
	../include/vlc_codec.h \
	../include/vlc_codec_synchro.h \
	../include/vlc_codecs.h \
	../include/vlc_common.h \
	../include/vlc_config.h \
	../include/vlc_config_cat.h \
	../include/vlc_configuration.h \
	../include/vlc_demux.h \
	../include/vlc_devices.h \
	../include/vlc_epg.h \
	../include/vlc_es.h \
	../include/vlc_es_out.h \
	../include/vlc_events.h \
	../include/vlc_filter.h \
	../include/vlc_fixups.h \
	../include/vlc_httpd.h \
	../include/vlc_image.h \
	../include/vlc_input.h \
	../include/vlc_interface.h \
	../include/vlc_intf_strings.h \
	../include/vlc_keys.h \
	../include/vlc_md5.h \
	../include/vlc_messages.h \
	../include/vlc_meta.h \
	../include/vlc_modules.h \
	../include/vlc_modules_macros.h \
	../include/vlc_mtime.h \
	../include/vlc_network.h \
	../include/vlc_objects.h \
	../include/vlc_osd.h \
	../include/vlc_pgpkey.h \
	../include/vlc_playlist.h \
	../include/vlc_plugin.h \
	../include/vlc_rand.h \
	../include/vlc_services_discovery.h \
	../include/vlc_sout.h \
	../include/vlc_stream.h \
	../include/vlc_streaming.h \
	../include/vlc_strings.h \
	../include/vlc_threads.h \
	../include/vlc_threads_funcs.h \
	../include/vlc_tls.h \
	../include/vlc_update.h \
	../include/vlc_url.h \
	../include/vlc_variables.h \
	../include/vlc_vlm.h \
	../include/vlc_vod.h \
	../include/vlc_vout.h \
	../include/vlc_xml.h \
	$(NULL)

modules/builtin.h: modules/builtin.h.in ../vlc-config Makefile.am
	rm -f -- "$@.tmp"
	cat "$(srcdir)/modules/builtin.h.in" > "$@.tmp"
if !HAVE_PLUGINS
	plugins="$$($(VLC_CONFIG) --list plugin)" ; \
	test -n "$${plugins}" && \
	for p in $${plugins}; do \
		echo "int vlc_entry__$$p (module_t *);" ; \
	done >> "$@.tmp"
endif
	echo "#define ALLOCATE_ALL_BUILTINS() \\" >> "$@.tmp"
	echo "    do \\" >> "$@.tmp"
	echo "    { \\" >> "$@.tmp"
if !HAVE_PLUGINS
	plugins="$$($(VLC_CONFIG) --list plugin)" ; \
	test -n "$${plugins}" && \
	for p in $${plugins}; do \
		echo "        ALLOCATE_BUILTIN($$p); \\" ; \
	done >> "$@.tmp"
endif
	echo '    } while( 0 );' >> "$@.tmp"
	mv -f -- "$@.tmp" "$@"

modules/modules.c: modules/builtin.h

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
pkgconfig_DATA = vlc-control.pc vlc-plugin.pc
CLEANFILES += $(pkgconfig_DATA)

.pc.in.pc: $(top_builddir)/config.status
	cd "$(top_builddir)" && \
	$(SHELL) ./config.status --file="src/$@"

###############################################################################
# Building libvlc
###############################################################################

nice:
	$(top_builddir)/compile

lib_LTLIBRARIES = libvlc.la libvlc-control.la

AM_LDFLAGS = -no-undefined -no-install
if HAVE_WIN32
AM_LDFLAGS += -avoid-version
endif

libvlc_la_SOURCES = $(SOURCES_libvlc)
nodist_libvlc_la_SOURCES = misc/revision.c
libvlc_la_CFLAGS = `$(VLC_CONFIG) --cflags libvlc` \
	-DLOCALEDIR=\"$(localedir)\" \
	-DDATA_PATH=\"$(pkgdatadir)\" \
	-DPLUGIN_PATH=\"$(pkglibdir)\"
libvlc_la_CXXFLAGS = `$(VLC_CONFIG) --cxxflags libvlc`
libvlc_la_OBJCFLAGS = `$(VLC_CONFIG) --objcflags libvlc`
libvlc_la_LDFLAGS = `$(VLC_CONFIG) --ldflags libvlc` $(AM_LDFLAGS) \
	-version-info 1:0:0
if HAVE_COMPILER_EXPORT
libvlc_la_LDFLAGS += -export-dynamic
else
libvlc_la_LDFLAGS += -export-symbols $(srcdir)/libvlc.sym 
endif
libvlc_la_LIBADD = `$(VLC_CONFIG) -libs libvlc` $(AM_LIBADD) $(LTLIBINTL)

libvlc_la_DEPENDENCIES = libvlc.sym

libvlc_control_la_SOURCES = $(SOURCES_libvlc_control)
libvlc_control_la_LIBADD = libvlc.la
libvlc_control_la_CFLAGS = `$(VLC_CONFIG) --cflags libvlc`
libvlc_control_la_LDFLAGS = $(AM_LDFLAGS) -version-info 1:0:0
if HAVE_COMPILER_EXPORT
libvlc_control_la_LDFLAGS += -export-dynamic
else
libvlc_control_la_LDFLAGS += -export-symbols-regex '^_?(libvlc|mediacontrol)_.*'
endif

EXTRA_libvlc_la_SOURCES = \
	$(SOURCES_libvlc_beos) \
	$(SOURCES_libvlc_darwin) \
	$(SOURCES_libvlc_win32) \
	$(SOURCES_libvlc_dirent) \
	$(SOURCES_libvlc_getopt) \
	$(NULL)
if HAVE_BEOS
libvlc_la_SOURCES += $(SOURCES_libvlc_beos)
endif
if HAVE_DARWIN
libvlc_la_SOURCES += $(SOURCES_libvlc_darwin)
endif
if HAVE_WIN32
libvlc_la_SOURCES += $(SOURCES_libvlc_win32)
endif
if HAVE_WINCE
libvlc_la_SOURCES += $(SOURCES_libvlc_win32)
endif
if BUILD_DIRENT
libvlc_la_SOURCES += $(SOURCES_libvlc_dirent)
endif
if BUILD_GETOPT
libvlc_la_SOURCES += $(SOURCES_libvlc_getopt)
endif

SOURCES_libvlc_beos = \
	misc/beos_specific.cpp \
	$(NULL)

SOURCES_libvlc_darwin = \
	misc/darwin_specific.c \
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
	libvlc-common.c \
	libvlc.h \
	libvlc-module.c \
	interface/interface.h \
	interface/interface.c \
	interface/intf_eject.c \
	interface/interaction.c \
	playlist/playlist_internal.h \
	playlist/thread.c \
	playlist/control.c \
	playlist/engine.c \
	playlist/sort.c \
	playlist/loadsave.c \
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
	input/input.c \
	input/meta.c \
	input/input_internal.h \
	input/vlm_internal.h \
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
	video_output/vout_intf.c \
	audio_output/aout_internal.h \
	audio_output/common.c \
	audio_output/dec.c \
	audio_output/filters.c \
	audio_output/input.c \
	audio_output/mixer.c \
	audio_output/output.c \
	audio_output/intf.c \
	stream_output/stream_output.c \
	stream_output/stream_output.h \
	stream_output/announce.c \
	stream_output/profiles.c \
	stream_output/sap.c \
	stream_output/sdp.c \
	osd/osd.c \
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
	network/poll.c \
	text/charset.c \
	text/strings.c \
	text/unicode.c \
	text/wincp.c \
	text/iso_lang.c \
	text/iso-639_def.h \
	misc/md5.c \
	misc/rand.c \
	misc/mtime.c \
	misc/block.c \
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
	misc/update.c \
	input/vlm.c \
	misc/xml.c \
	misc/devices.c \
	extras/libc.c \
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
# Stamp rules
###############################################################################

misc/revision.c:
	rm -f $@ $@.tmp
	echo "/* AUTOGENERATED FILE - DO NOT EDIT */" > $@.tmp
	REVISION="$$(LANG=C git --git-dir=$(top_srcdir)/.git show-ref -s HEAD 2>/dev/null || echo exported)"; \
	REVISION="$$(echo $$REVISION|cut -b -10)"; \
	echo "const char psz_vlc_changeset[] = \"$$REVISION\";" >> $@.tmp
	mv -f $@.tmp $@


###############################################################################
# Building vlc
###############################################################################

if BUILD_VLC
bin_PROGRAMS = vlc
endif

vlc_SOURCES = vlc.c
vlc_DEPENDENCIES = $(DATA_win32_rc) libvlc.la

vlc_CFLAGS = `$(VLC_CONFIG) --cflags vlc`
vlc_LDFLAGS = `$(VLC_CONFIG) --ldflags vlc`
vlc_LDADD = $(DATA_win32_rc) libvlc.la $(LTLIBINTL) `$(VLC_CONFIG) -libs vlc`

if BUILD_VLC
vlc$(EXEEXT): $(vlc_OBJECTS) $(vlc_DEPENDENCIES)
	@rm -f vlc$(EXEEXT)
	@case `$(VLC_CONFIG) --linkage vlc builtin` in \
	  c++) cmd="$(CXXLINK)" ;; \
	  objc) cmd="$(OBJCLINK)" ;; \
	  c|*) cmd="$(LINK)" ;; \
	esac ; \
	cmd="$$cmd $(vlc_OBJECTS) $(vlc_LDADD) $(vlc_LDFLAGS)" ; \
	echo $$cmd ; \
	eval $$cmd

if HAVE_WIN32
DATA_win32_rc = $(noinst_DATA)
noinst_DATA = vlc_win32_rc.$(OBJEXT)

vlc_win32_rc.$(OBJEXT): $(top_srcdir)/share/vlc_win32_rc.rc
	$(WINDRES) -DVERSION=$(VERSION) -DVERSION_NUMBER=`echo $(VERSION).0.0.0 | sed 's/\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\)[^.]*\.*\([0-9]*\).*/\1,\2,\3,\4/'` --include-dir $(top_srcdir)/share -i $< -o $@
endif
endif

###############################################################################
# Unit/regression test
###############################################################################
check_PROGRAMS = \
	test_dictionary \
	test_i18n_atof \
	test_url \
	test_utf8 \
	test_headers \
	$(NULL)

dist_check_SCRIPTS = check_symbols check_headers
TESTS = $(check_PROGRAMS) check_symbols

CFLAGS_tests = `$(VLC_CONFIG) --cflags libvlc`

test_dictionary_SOURCES = test/dictionary.c
test_dictionary_LDADD = libvlc.la
test_dictionary_CFLAGS = $(CFLAGS_tests)

test_i18n_atof_SOURCES = test/i18n_atof.c text/charset.c
test_i18n_atof_LDADD = libvlc.la
test_i18n_atof_CFLAGS = $(CFLAGS_tests)

test_url_SOURCES = test/url.c
test_url_LDADD = libvlc.la
test_url_CFLAGS = $(CFLAGS_tests)

test_utf8_SOURCES = test/utf8.c
test_utf8_LDADD = libvlc.la
test_utf8_CFLAGS = $(CFLAGS_tests)

test_headers_SOURCES = test/headers.c
test_headers_CFLAGS = $(CFLAGS_tests)

check-local:
	for h in `echo $(dist_pkginclude_HEADERS) $(pkginclude_HEADERS) | sed -e s,\.\./include/,,g`; \
	do \
		echo grep - "#include <$$h>" $(srcdir)/test/headers.c ; \
		if ! grep -- "#include <$$h>" $(srcdir)/test/headers.c ; \
		then \
			echo "Header $$h not included in test/headers.c!"; \
			exit 1; \
		fi ; \
	done
	$(srcdir)/check_headers $(dist_pkginclude_HEADERS) $(pkginclude_HEADERS)

FORCE:
	@echo "Generated source cannot be phony. Go away." >&2
	@exit 1

.PHONY: FORCE
