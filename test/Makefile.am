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
	test_libvlc_events \
	test_libvlc_media \
	test_libvlc_media_list \
	test_libvlc_media_player \
	test_src_misc_variables \
        $(NULL)

# Disabled test:
# meta: No suitable test file
EXTRA_PROGRAMS = \
	test_libvlc_meta \
	test_libvlc_media_list_player \
	$(NULL)

#check_DATA = samples/test.sample samples/meta.sample
EXTRA_DIST = samples/empty.voc samples/image.jpg

check_HEADERS = libvlc/test.h libvlc/libvlc_additions.h

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
LDFLAGS_tests = -no-install -static

test_libvlc_core_SOURCES = libvlc/core.c
test_libvlc_core_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_core_CFLAGS = $(CFLAGS_tests)
test_libvlc_core_LDFLAGS = $(LDFLAGS_tests)

test_libvlc_events_SOURCES = libvlc/events.c
test_libvlc_events_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_events_CFLAGS = $(CFLAGS_tests)
test_libvlc_events_LDFLAGS = $(LDFLAGS_tests)

test_libvlc_media_SOURCES = libvlc/media.c
test_libvlc_media_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_media_CFLAGS = $(CFLAGS_tests)
test_libvlc_media_LDFLAGS = $(LDFLAGS_tests)

test_libvlc_media_list_player_SOURCES = libvlc/media_list_player.c
test_libvlc_media_list_player_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_media_list_player_CFLAGS = $(CFLAGS_tests)
test_libvlc_media_list_player_LDFLAGS = $(LDFLAGS_tests)

test_libvlc_media_list_SOURCES = libvlc/media_list.c
test_libvlc_media_list_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_media_list_CFLAGS = $(CFLAGS_tests)
test_libvlc_media_list_LDFLAGS = $(LDFLAGS_tests)

test_libvlc_media_player_SOURCES = libvlc/media_player.c
test_libvlc_media_player_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_media_player_CFLAGS = $(CFLAGS_tests)
test_libvlc_media_player_LDFLAGS = $(LDFLAGS_tests)

test_libvlc_meta_SOURCES = libvlc/meta.c
test_libvlc_meta_LDADD = $(top_builddir)/src/libvlc.la
test_libvlc_meta_CFLAGS = $(CFLAGS_tests)
test_libvlc_meta_LDFLAGS = $(LDFLAGS_tests)

test_src_misc_variables_SOURCES = src/misc/variables.c
test_src_misc_variables_LDADD = $(top_builddir)/src/libvlc.la
test_src_misc_variables_CFLAGS = $(CFLAGS_tests)
test_src_misc_variables_LDFLAGS = $(LDFLAGS_tests)

checkall:
	$(MAKE) check_PROGRAMS="$(check_PROGRAMS) $(EXTRA_PROGRAMS)" check

FORCE:
	@echo "Generated source cannot be phony. Go away." >&2
	@exit 1

.PHONY: FORCE
