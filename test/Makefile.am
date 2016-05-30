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
check_PROGRAMS = \
	test_libvlc_core \
	test_libvlc_equalizer \
	test_libvlc_media \
	test_libvlc_media_list \
	test_libvlc_media_player \
	test_libvlc_slaves \
	test_src_config_chain \
	test_src_misc_variables \
	test_src_crypto_update \
	test_src_input_stream \
	test_src_interface_dialog \
	test_src_misc_bits \
	test_src_misc_epg \
	test_src_misc_keystore \
	test_modules_packetizer_hxxx \
	test_modules_keystore \
	test_modules_tls \
	$(NULL)

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
EXTRA_DIST = samples/empty.voc samples/image.jpg samples/subitems $(check_SCRIPTS)

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
LIBVLCCORE = ../src/libvlccore.la
LIBVLC = ../lib/libvlc.la

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
test_src_misc_bits_SOURCES = src/misc/bits.c
test_src_misc_bits_LDADD = $(LIBVLC)
test_src_misc_epg_SOURCES = src/misc/epg.c
test_src_misc_epg_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_src_misc_keystore_SOURCES = src/misc/keystore.c
test_src_misc_keystore_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_src_interface_dialog_SOURCES = src/interface/dialog.c
test_src_interface_dialog_LDADD = $(LIBVLCCORE) $(LIBVLC)
test_modules_packetizer_hxxx_SOURCES = modules/packetizer/hxxx.c
test_modules_packetizer_hxxx_LDADD = $(LIBVLC)
test_modules_packetizer_hxxx_LDFLAGS = -no-install -static # WTF
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
