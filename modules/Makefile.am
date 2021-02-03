noinst_LTLIBRARIES =
check_LTLIBRARIES =
pkglib_LTLIBRARIES =
noinst_HEADERS =
check_PROGRAMS =
pkglibexec_PROGRAMS =
EXTRA_DIST =


SUBDIRS = .
TESTS =

dist_noinst_SCRIPTS = module.rc.in
EXTRA_LTLIBRARIES =

include common.am
include access/Makefile.am
include access/http/Makefile.am
include access/rtp/Makefile.am
include arm_neon/Makefile.am
include audio_filter/Makefile.am
include audio_mixer/Makefile.am
include audio_output/Makefile.am
include codec/Makefile.am
include control/Makefile.am
include demux/Makefile.am
include gui/Makefile.am
include hw/nvdec/Makefile.am
include hw/d3d9/Makefile.am
include hw/d3d11/Makefile.am
include hw/vaapi/Makefile.am
include hw/vdpau/Makefile.am
include hw/mmal/Makefile.am
include keystore/Makefile.am
include logger/Makefile.am
include lua/Makefile.am
include meta_engine/Makefile.am
include misc/Makefile.am
include notify/Makefile.am
include packetizer/Makefile.am
include services_discovery/Makefile.am
include spu/Makefile.am
include stream_filter/Makefile.am
include stream_extractor/Makefile.am
include text_renderer/Makefile.am
include video_chroma/Makefile.am
include video_filter/Makefile.am
include video_splitter/Makefile.am
include video_output/Makefile.am
include visualization/Makefile.am
if ENABLE_SOUT
include access_output/Makefile.am
include mux/Makefile.am
include stream_out/Makefile.am
endif

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
endif

# Wayland
SUFFIXES += -client-protocol.h -protocol.c .xml

.xml-client-protocol.h:
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header < "$^" > "$@.tmp"
	$(AM_V_at)mv -f -- "$@.tmp" "$@"

.xml-protocol.c:
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code < "$^" > "$@.tmp"
	$(AM_V_at)mv -f -- "$@.tmp" "$@"

# Extract plugins and convenience libraries from the libtool libraries
# being built, excluding the ones using them.

# String substitution containing a macro expansion is undefined
ALL_TARGETS_1 := $(LTLIBRARIES)
# Remove the special targets
ALL_TARGETS_2 := $(ALL_TARGETS_1:libvlc_static.la=)
ALL_TARGETS := $(ALL_TARGETS_2:libvlc_plugin.la=)

# Remove the non-plugins, it needs intermediate steps because
# we can only replace and not match inverse. Instead we;
#  1/ replace plugins .la targets by plugins names
#  2/ remove every word which is a .la target (ie. non-plugins)
#  3/ rename every plugin name back into the target name
ALL_TARGETS_PLUGIN_RENAMED := $(ALL_TARGETS:%_plugin.la=%)
PLUGINS_NAMES := $(ALL_TARGETS_PLUGIN_RENAMED:%.la=)
PLUGINS := $(PLUGINS_NAMES:%=%_plugin.la)

plugin.c: $(PLUGINS)
	echo "Compile VLC static modules list object"
	## Collect paths of all static libraries needed (plugins and contribs)
	echo "#define VLC_MODULE_LIST(F) \\" > $@.tmp
	## Note: $(NULL) here is to fix syntax interpretation of \\ in the string
	for ltlib in $^; do \
		if ! echo $${ltlib} | grep ytdl ; then \
		if ! echo $${ltlib} | grep wl_shell ; then \
		if ! echo $${ltlib} | grep addon ; then \
		if ! echo $${ltlib} | grep vdpau ; then \
		libpath=`grep old_library= "$${ltlib}" | cut -d"'" -f 2`; \
		entrypoint=`$(NM) ".libs/$${libpath}" | grep vlc_entry__ | cut -d" " -f 3 | head -n1`; \
		module_name=`echo $${entrypoint} | sed s/.*vlc_entry__//`; \
		echo "F($${module_name}) \\$(NULL)" >> $@.tmp; \
		fi \
		fi \
		fi \
		fi \
	done
	echo "" >> $@.tmp
	echo "#include \"plugin_static.c\"" >> $@.tmp
	mv $@.tmp $@

BUILT_SOURCES += plugin.c
CLEANFILES += plugin.c

libvlc_static_la_SOURCES = plugin.c
libvlc_static_la_CFLAGS =
libvlc_static_la_LDFLAGS =
libvlc_static_la_LIBADD = $(PLUGINS) $(LTLIBVLCCORE)
EXTRA_libvlc_static_la_DEPENDENCIES = plugin_static.c

libvlc_plugin_la_SOURCES = plugin.c
libvlc_plugin_la_CFLAGS =
libvlc_plugin_la_LDFLAGS = \
	-avoid-version -module \
	-export-symbols-regex ^vlc_entry \
	-shrext $(LIBEXT) \
	-no-undefined \
	$(top_builddir)/compat/libcompat.la $(LTLIBVLCCORE)
libvlc_plugin_la_LIBADD = $(PLUGINS)
EXTRA_libvlc_plugin_la_DEPENDENCIES = vlc_modules_manifest.h

if HAVE_IOS
libvlc_plugin_la_LDFLAGS += \
	-Wl,-framework,CoreFoundation,-framework,Foundation \
	-Wl,-framework,CoreAudio,-framework,AudioToolbox \
	-Wl,-framework,CoreVideo,-framework,QuartzCore \
	-Wl,-framework,OpenGLES,-framework,UIKit \
	-Wl,-framework,Security,-framework,AVFoundation \
	-Wl,-framework,CoreImage
endif

EXTRA_libvlc_plugin_la_DEPENDENCIES = plugin_static.c

pkglib_LTLIBRARIES += libvlc_plugin.la #libvlc_static.la
