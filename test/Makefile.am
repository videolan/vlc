###############################################################################
# Automake targets and declarations
###############################################################################

AUTOMAKE_OPTIONS = subdir-objects

###############################################################################
# Unit/regression test
###############################################################################
check_PROGRAMS = \
    test_libvlc_core \
	test_libvlc_events \
	test_libvlc_media_list \
	test_libvlc_media_list_player \
	test_libvlc_media_player \
	$(NULL)

TESTS = samples $(check_PROGRAMS)

# Samples server
SAMPLES_SERVER=http://streams.videolan.org/streams-videolan/reference

samples/test.sample:
	mkdir -p `dirname $@`
	curl $(SAMPLES_SERVER)/avi/Hero-Div3.avi > $@

samples: samples/test.sample

CFLAGS_tests = `$(VLC_CONFIG) --cflags libvlc`

test_libvlc_core_SOURCES = libvlc/core.c
test_libvlc_core_LDADD = $(top_builddir)/src/libvlc-control.la
test_libvlc_core_CFLAGS = $(CFLAGS_tests)

test_libvlc_events_SOURCES = libvlc/events.c
test_libvlc_events_LDADD = $(top_builddir)/src/libvlc-control.la
test_libvlc_events_CFLAGS = $(CFLAGS_tests)

test_libvlc_media_list_player_SOURCES = libvlc/media_list_player.c
test_libvlc_media_list_player_LDADD = $(top_builddir)/src/libvlc-control.la
test_libvlc_media_list_player_CFLAGS = $(CFLAGS_tests)

test_libvlc_media_list_SOURCES = libvlc/media_list.c
test_libvlc_media_list_LDADD = $(top_builddir)/src/libvlc-control.la
test_libvlc_media_list_CFLAGS = $(CFLAGS_tests)

test_libvlc_media_player_SOURCES = libvlc/media_player.c
test_libvlc_media_player_LDADD = $(top_builddir)/src/libvlc-control.la
test_libvlc_media_player_CFLAGS = $(CFLAGS_tests)

FORCE:
	@echo "Generated source cannot be phony. Go away." >&2
	@exit 1

.PHONY: FORCE
