###############################################################################
# Automake targets and declarations
###############################################################################

AUTOMAKE_OPTIONS = subdir-objects

extra_check_verbose = $(extra_check_verbose_$(V))
extra_check_verbose_ = $(extra_check_flags__$(AM_DEFAULT_VERBOSITY))
extra_check_verbose_0 = @echo TEST $@
extra_check_verbose__0 = $(extra_check_verbose_0)

###############################################################################
# Unit/regression test
###############################################################################
noinst_PROGRAMS =
check_PROGRAMS = \
	test_libvlc_core \
	test_libvlc_equalizer \
	test_libvlc_media \
	test_libvlc_media_list \
	test_libvlc_media_player \
	test_libvlc_media_discoverer \
	test_libvlc_renderer_discoverer \
	test_libvlc_slaves \
	test_src_config_chain \
	test_src_misc_variables \
	test_src_input_stream \
	test_src_input_stream_fifo \
	test_src_interface_dialog \
	test_src_misc_bits \
	test_src_misc_epg \
	test_src_misc_keystore \
	test_modules_packetizer_hxxx \
	test_modules_keystore
if ENABLE_SOUT
check_PROGRAMS += test_modules_tls
endif
if UPDATE_CHECK
check_PROGRAMS += test_src_crypto_update
endif

check_SCRIPTS = \
	modules/lua/telnet.sh \
	check_POTFILES.sh

# Disabled test:
# meta: No suitable test file
EXTRA_PROGRAMS = \
	test_libvlc_meta \
	test_libvlc_media_list_player \
	test_src_input_stream_net \
	$(NULL)

#check_DATA = samples/test.sample samples/meta.sample
EXTRA_DIST = \
	samples/certs/certkey.pem \
	samples/empty.voc \
	samples/image.jpg \
	samples/subitems \
	samples/slaves \
	$(check_SCRIPTS)

check_HEADERS = libvlc/test.h libvlc/libvlc_additions.h

TESTS = $(check_PROGRAMS) check_POTFILES.sh

DISTCLEANFILES = samples/test.sample samples/meta.sample

# Samples server
SAMPLES_SERVER=http://streams.videolan.org/streams-videolan/reference

samples/test.sample:
	mkdir -p `dirname $@`
	curl $(SAMPLES_SERVER)/avi/Hero-Div3.avi > $@

samples/meta.sample:
	mkdir -p `dirname $@`
	curl $(SAMPLES_SERVER)/metadata/id3tag/Wesh-Bonneville.mp3 > $@

AM_CFLAGS = -DSRCDIR=\"$(srcdir)\"
AM_LDFLAGS = -no-install
LIBVLCCORE = -L../src/ -lvlccore
LIBVLC = -L../lib -lvlc

test_libvlc_core_SOURCES = libvlc/core.c
test_libvlc_core_LDADD = $(LIBVLC)
test_libvlc_equalizer_SOURCES = libvlc/equalizer.c
test_libvlc_equalizer_LDADD = $(LIBVLC)
test_libvlc_media_SOURCES = libvlc/media.c
test_libvlc_media_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_libvlc_media_list_player_SOURCES = libvlc/media_list_player.c
test_libvlc_media_list_player_LDADD = $(LIBVLC)
test_libvlc_media_list_SOURCES = libvlc/media_list.c
test_libvlc_media_list_LDADD = $(LIBVLC)
test_libvlc_media_player_SOURCES = libvlc/media_player.c
test_libvlc_media_player_LDADD = $(LIBVLC)
test_libvlc_media_discoverer_SOURCES = libvlc/media_discoverer.c
test_libvlc_media_discoverer_LDADD = $(LIBVLC)
test_libvlc_renderer_discoverer_SOURCES = libvlc/renderer_discoverer.c
test_libvlc_renderer_discoverer_LDADD = $(LIBVLC)
test_libvlc_slaves_SOURCES = libvlc/slaves.c
test_libvlc_slaves_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_libvlc_meta_SOURCES = libvlc/meta.c
test_libvlc_meta_LDADD = $(LIBVLC)
test_src_misc_variables_SOURCES = src/misc/variables.c
test_src_misc_variables_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_src_config_chain_SOURCES = src/config/chain.c
test_src_config_chain_LDADD = $(LIBVLCCORE)
test_src_crypto_update_SOURCES = src/crypto/update.c
test_src_crypto_update_LDADD = $(LIBVLCCORE) $(GCRYPT_LIBS)
test_src_input_stream_SOURCES = src/input/stream.c
test_src_input_stream_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_src_input_stream_net_SOURCES = src/input/stream.c
test_src_input_stream_net_CFLAGS = $(AM_CFLAGS) -DTEST_NET
test_src_input_stream_net_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_src_input_stream_fifo_SOURCES = src/input/stream_fifo.c
test_src_input_stream_fifo_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_src_misc_bits_SOURCES = src/misc/bits.c
test_src_misc_bits_LDADD = $(LIBVLC)
test_src_misc_epg_SOURCES = src/misc/epg.c
test_src_misc_epg_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_src_misc_keystore_SOURCES = src/misc/keystore.c
test_src_misc_keystore_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_src_interface_dialog_SOURCES = src/interface/dialog.c
test_src_interface_dialog_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_modules_packetizer_hxxx_SOURCES = modules/packetizer/hxxx.c
test_modules_packetizer_hxxx_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_modules_keystore_SOURCES = modules/keystore/test.c
test_modules_keystore_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_modules_tls_SOURCES = modules/misc/tls.c
test_modules_tls_LDADD = $(LIBVLCCORE) $(LIBVLC)

checkall:
	$(MAKE) check_PROGRAMS="$(check_PROGRAMS) $(EXTRA_PROGRAMS)" check

FORCE:
	@echo "Generated source cannot be phony. Go away." >&2
	@exit 1

.PHONY: FORCE

libvlc_demux_run_la_SOURCES = src/input/demux-run.c src/input/demux-run.h \
	src/input/common.c src/input/common.h
libvlc_demux_run_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-DTOP_BUILDDIR=\"$$(cd "$(top_builddir)"; pwd)\" \
	-DTOP_SRCDIR=\"$$(cd "$(top_srcdir)"; pwd)\"
libvlc_demux_run_la_LDFLAGS = -no-install -static
libvlc_demux_run_la_LIBADD = \
	../lib/libvlc.la ../src/libvlccore.la ../compat/libcompat.la
if !HAVE_DYNAMIC_PLUGINS
libvlc_demux_run_la_CPPFLAGS += -DHAVE_STATIC_MODULES
libvlc_demux_run_la_LIBADD += \
	../modules/libxml_plugin.la \
	../modules/libconsole_logger_plugin.la \
	../modules/libaiff_plugin.la \
	../modules/libasf_plugin.la \
	../modules/libau_plugin.la \
	../modules/libavi_plugin.la \
	../modules/libcaf_plugin.la \
	../modules/libdiracsys_plugin.la \
	../modules/libes_plugin.la \
	../modules/libflacsys_plugin.la \
	../modules/libh26x_plugin.la \
	../modules/libmjpeg_plugin.la \
	../modules/libmkv_plugin.la \
	../modules/libmp4_plugin.la \
	../modules/libnsc_plugin.la \
	../modules/libnsv_plugin.la \
	../modules/libnuv_plugin.la \
	../modules/libps_plugin.la \
	../modules/libpva_plugin.la \
	../modules/libsap_plugin.la \
	../modules/libsmf_plugin.la \
	../modules/libsubtitle_plugin.la \
	../modules/libtta_plugin.la \
	../modules/libttml_plugin.la \
	../modules/libty_plugin.la \
	../modules/libvoc_plugin.la \
	../modules/libwav_plugin.la \
	../modules/libwebvtt_plugin.la \
	../modules/libxa_plugin.la \
	../modules/libpacketizer_a52_plugin.la \
	../modules/libpacketizer_copy_plugin.la \
	../modules/libpacketizer_dts_plugin.la \
	../modules/libpacketizer_dirac_plugin.la \
	../modules/libpacketizer_flac_plugin.la \
	../modules/libpacketizer_h264_plugin.la \
	../modules/libpacketizer_hevc_plugin.la \
	../modules/libpacketizer_mlp_plugin.la \
	../modules/libpacketizer_mpeg4audio_plugin.la \
	../modules/libpacketizer_mpeg4video_plugin.la \
	../modules/libpacketizer_mpegaudio_plugin.la \
	../modules/libpacketizer_mpegvideo_plugin.la \
	../modules/libpacketizer_vc1_plugin.la \
	../modules/librawaud_plugin.la \
	../modules/librawvid_plugin.la \
	../modules/libfilesystem_plugin.la \
	../modules/libxml_plugin.la \
	-lstdc++
if HAVE_DVBPSI
libvlc_demux_run_la_CPPFLAGS += -DHAVE_DVBPSI
libvlc_demux_run_la_LIBADD += ../modules/libts_plugin.la
endif
endif
EXTRA_LTLIBRARIES = libvlc_demux_run.la

libvlc_demux_dec_run_la_SOURCES = $(libvlc_demux_run_la_SOURCES) \
	src/input/decoder.c src/input/decoder.h
libvlc_demux_dec_run_la_CPPFLAGS = $(libvlc_demux_run_la_CPPFLAGS) -DHAVE_DECODERS
libvlc_demux_dec_run_la_LDFLAGS = $(libvlc_demux_run_la_LDFLAGS)
libvlc_demux_dec_run_la_LIBADD = $(libvlc_demux_run_la_LIBADD)
if !HAVE_DYNAMIC_PLUGINS
libvlc_demux_dec_run_la_LIBADD += \
	../modules/libadpcm_plugin.la \
	../modules/libaes3_plugin.la \
	../modules/libaraw_plugin.la \
	../modules/libg711_plugin.la \
	../modules/liblpcm_plugin.la \
	../modules/libuleaddvaudio_plugin.la \
	../modules/librawvideo_plugin.la \
	../modules/libcc_plugin.la \
	../modules/libcvdsub_plugin.la \
	../modules/libdvbsub_plugin.la \
	../modules/libscte18_plugin.la \
	../modules/libscte27_plugin.la \
	../modules/libspudec_plugin.la \
	../modules/libstl_plugin.la \
	../modules/libsubsdec_plugin.la \
	../modules/libsubsusf_plugin.la \
	../modules/libsvcdsub_plugin.la \
	../modules/libtextst_plugin.la \
	../modules/libsubstx3g_plugin.la
endif
EXTRA_LTLIBRARIES += libvlc_demux_dec_run.la

#
# Fuzzers
#
vlc_demux_run_LDFLAGS = -no-install -static
vlc_demux_run_LDADD = libvlc_demux_run.la
vlc_demux_dec_run_SOURCES = vlc-demux-run.c
vlc_demux_dec_run_LDFLAGS = -no-install -static
vlc_demux_dec_run_LDADD = libvlc_demux_dec_run.la
EXTRA_PROGRAMS += vlc-demux-run vlc-demux-dec-run

vlc_demux_libfuzzer_LDADD = libvlc_demux_run.la
vlc_demux_dec_libfuzzer_SOURCES = vlc-demux-libfuzzer.c
vlc_demux_dec_libfuzzer_LDADD = libvlc_demux_dec_run.la
if HAVE_LIBFUZZER
noinst_PROGRAMS += vlc-demux-libfuzzer vlc-demux-dec-libfuzzer vlc-demux-run vlc-demux-dec-run
endif
