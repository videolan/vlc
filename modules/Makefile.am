noinst_LTLIBRARIES =
noinst_HEADERS =
check_PROGRAMS =
EXTRA_DIST =

BASE_SUBDIRS = \
	audio_filter \
	control \
	gui \
	packetizer \
	text_renderer \
	video_filter \
	visualization
EXTRA_SUBDIRS = \
	access_output \
	mux \
	stream_out \
	hw/mmal

SUBDIRS = . $(BASE_SUBDIRS)
DIST_SUBDIRS = . $(BASE_SUBDIRS) $(EXTRA_SUBDIRS)
if ENABLE_SOUT
SUBDIRS += access_output mux stream_out
endif
if HAVE_MMAL
SUBDIRS += hw/mmal
endif

TESTS =

dist_noinst_SCRIPTS = genmf list.sh module.rc.in
dist_noinst_DATA = MODULES_LIST
EXTRA_LTLIBRARIES =

include common.am
include access/Makefile.am
include access/rtp/Makefile.am
include arm_neon/Makefile.am
include audio_mixer/Makefile.am
include audio_output/Makefile.am
include codec/Makefile.am
include demux/Makefile.am
include hw/vdpau/Makefile.am
include logger/Makefile.am
include lua/Makefile.am
include meta_engine/Makefile.am
include misc/Makefile.am
include notify/Makefile.am
include services_discovery/Makefile.am
include stream_filter/Makefile.am
include video_chroma/Makefile.am
include video_splitter/Makefile.am
include video_output/Makefile.am

BUILT_SOURCES += dummy.cpp

dummy.cpp:
	touch dummy.cpp

if HAVE_WIN32
BUILT_SOURCES += module.rc.lo
CLEANFILES += module.rc
#noinst_DATA = module.rc.lo

module.rc: module.rc.in $(top_builddir)/config.status
	$(AM_V_GEN) cd "$(top_builddir)" && $(SHELL) ./config.status --file="modules/$@"

module.rc.lo: module.rc
	$(AM_V_GEN) $(LIBTOOL) --tag=RC --mode=compile $(WINDRES) \
		--include-dir $(top_srcdir)/share \
		--include-dir $(top_srcdir)/extras/package/win32 \
		-i $< -o $@
	echo HELLO
endif

# Wayland
SUFFIXES += -client-protocol.h -protocol.c .xml

.xml-client-protocol.h:
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < "$^" > "$@.tmp"
	$(AM_V_at)mv -f -- "$@.tmp" "$@"

.xml-protocol.c:
	$(AM_V_GEN)$(WAYLAND_SCANNER) code < "$^" > "$@.tmp"
	$(AM_V_at)mv -f -- "$@.tmp" "$@"
