noinst_LTLIBRARIES =
noinst_HEADERS =

BASE_SUBDIRS = \
	access/rtp \
	audio_filter \
	audio_mixer \
	control \
	gui \
	misc \
	notify \
	packetizer \
	stream_filter \
	text_renderer \
	video_chroma \
	video_filter \
	video_output \
	video_splitter \
	visualization
EXTRA_SUBDIRS = \
	access_output \
	mux \
	stream_out \
	arm_neon \
	hw/vdpau \
	lua

SUBDIRS = . $(BASE_SUBDIRS)
DIST_SUBDIRS = . $(BASE_SUBDIRS) $(EXTRA_SUBDIRS)
if ENABLE_SOUT
SUBDIRS += access_output mux stream_out
endif
if HAVE_NEON
SUBDIRS += arm_neon
endif
if HAVE_VDPAU
SUBDIRS += hw/vdpau
endif
if BUILD_LUA
SUBDIRS += lua
endif

dist_noinst_SCRIPTS = genmf list.sh module.rc.in
dist_noinst_DATA = MODULES_LIST

libvlc_pulse_la_SOURCES = vlcpulse.c vlcpulse.h
libvlc_pulse_la_CPPFLAGS = -DMODULE_STRING=\"pulse\"
libvlc_pulse_la_CFLAGS = $(AM_CFLAGS) $(PULSE_CFLAGS)
libvlc_pulse_la_LDFLAGS = -static
libvlc_pulse_la_LIBADD = $(PULSE_LIBS)
if HAVE_PULSE
noinst_LTLIBRARIES += libvlc_pulse.la
endif

include common.am
include access/Makefile.am
include audio_output/Makefile.am
include codec/Makefile.am
include demux/Makefile.am
include meta_engine/Makefile.am
include services_discovery/Makefile.am

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
