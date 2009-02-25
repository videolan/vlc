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
	$(NULL)
EXTRA_PROGRAMS = \
	test_libvlc_media_player \
	test_libvlc_meta \
	$(NULL)
#check_DATA = samples/test.sample samples/meta.sample
EXTRA_DIST = samples/empty.voc

check_HEADERS = libvlc/test.h

TESTS = $(check_PROGRAMS)

DISTCLEANFILES = samples/test.sample samples/meta.sample

# Samples server
SAMPLES_SERVER=http://streams.videolan.org/streams-videolan/reference

samples/test.sample:
	mkdir -p `dirname $@`
	curl $(SAMPLES_SERVER)/avi/Hero-Div3.avi > $@

samples/meta.sample:
	mkdir -p `dirname $@`
	curl $(SAMPLES_SERVER)/metadata/id3tag/Wesh-Bonneville.mp3 > $@

CFLAGS_tests = `$(VLC_CONFIG) --cflags libvlc` -DSRCDIR=\"$(srcdir)\"

test_libvlc_core_SOURCES = libvlc/core.c
test_libvlc_core_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_core_CFLAGS = $(CFLAGS_tests)

test_libvlc_events_SOURCES = libvlc/events.c
test_libvlc_events_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_events_CFLAGS = $(CFLAGS_tests)

test_libvlc_media_list_player_SOURCES = libvlc/media_list_player.c
test_libvlc_media_list_player_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_media_list_player_CFLAGS = $(CFLAGS_tests)

test_libvlc_media_list_SOURCES = libvlc/media_list.c
test_libvlc_media_list_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_media_list_CFLAGS = $(CFLAGS_tests)

test_libvlc_media_player_SOURCES = libvlc/media_player.c
test_libvlc_media_player_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_media_player_CFLAGS = $(CFLAGS_tests)

test_libvlc_meta_SOURCES = libvlc/meta.c
test_libvlc_meta_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_meta_CFLAGS = $(CFLAGS_tests)

checkall:
	$(MAKE) check_PROGRAMS="$(check_PROGRAMS) $(EXTRA_PROGRAMS)" check

FORCE:
	@echo "Generated source cannot be phony. Go away." >&2
	@exit 1

.PHONY: FORCE
