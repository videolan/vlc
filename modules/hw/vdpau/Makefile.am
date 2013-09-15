include $(top_srcdir)/modules/common.am
vdpaudir = $(pluginsdir)/vdpau

AM_CFLAGS += $(VDPAU_CFLAGS)

libvlc_vdpau_la_SOURCES = vlc_vdpau.c vlc_vdpau.h instance.c
libvlc_vdpau_la_CPPFLAGS =
libvlc_vdpau_la_LIBADD = $(X_LIBS) $(X_PRE_LIBS) -lX11 \
	$(LIBDL) $(LIBPTHREAD)
libvlc_vdpau_la_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex ^vdp_ \
	-version-info 0:0:0
pkglib_LTLIBRARIES = libvlc_vdpau.la

vdpau_LTLIBRARIES =

libvdpau_avcodec_plugin_la_SOURCES = avcodec.c picture.c
libvdpau_avcodec_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(X_CFLAGS) $(AVCODEC_CFLAGS)
libvdpau_avcodec_plugin_la_LIBADD = libvlc_vdpau.la \
	$(X_LIBS) $(X_PRE_LIBS) -lX11 $(AVCODEC_LIBS)
# FIXME: hwdummy only needs libavutil, and only if avcodec < 55.13.0
if HAVE_AVCODEC_VDPAU
vdpau_LTLIBRARIES += libvdpau_avcodec_plugin.la
endif

libvdpau_deinterlace_plugin_la_SOURCES = deinterlace.c picture.c
libvdpau_deinterlace_plugin_la_CFLAGS = $(AM_CFLAGS) # dummy
libvdpau_deinterlace_plugin_la_LIBADD = libvlc_vdpau.la
vdpau_LTLIBRARIES += libvdpau_deinterlace_plugin.la

libvdpau_adjust_plugin_la_SOURCES = adjust.c picture.c
libvdpau_adjust_plugin_la_CFLAGS = $(AM_CFLAGS) # dummy
libvdpau_adjust_plugin_la_LIBADD = libvlc_vdpau.la
vdpau_LTLIBRARIES += libvdpau_adjust_plugin.la

libvdpau_sharpen_plugin_la_SOURCES = sharpen.c picture.c
libvdpau_sharpen_plugin_la_CFLAGS = $(AM_CFLAGS) # dummy
libvdpau_sharpen_plugin_la_LIBADD = libvlc_vdpau.la
vdpau_LTLIBRARIES += libvdpau_sharpen_plugin.la

libvdpau_chroma_plugin_la_SOURCES = chroma.c picture.c
libvdpau_chroma_plugin_la_CFLAGS = $(AM_CFLAGS) # dummy
libvdpau_chroma_plugin_la_LIBADD = libvlc_vdpau.la
vdpau_LTLIBRARIES += libvdpau_chroma_plugin.la

libvdpau_display_plugin_la_SOURCES = display.c
libvdpau_display_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/../../video_output/xcb
libvdpau_display_plugin_la_CFLAGS = $(AM_CFLAGS) $(XCB_CFLAGS)
libvdpau_display_plugin_la_LIBADD = libvlc_vdpau.la \
	../../video_output/libvlc_xcb_events.la \
	$(AM_LIBADD) $(X_LIBS) $(X_PRE_LIBS) -lX11 $(XCB_LIBS)
vdpau_LTLIBRARIES += libvdpau_display_plugin.la
