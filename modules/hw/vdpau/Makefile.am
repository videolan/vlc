basedir = vdpau
include $(top_srcdir)/modules/common.am

AM_CFLAGS += $(VDPAU_CFLAGS)
AM_LIBADD += libvlc_vdpau.la

libvlc_vdpau_la_SOURCES = vlc_vdpau.c vlc_vdpau.h instance.c
libvlc_vdpau_la_CPPFLAGS =
libvlc_vdpau_la_LIBADD = $(X_LIBS) $(X_PRE_LIBS) -lX11 \
	$(LIBDL) $(LIBPTHREAD)
libvlc_vdpau_la_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex ^vdp_ \
	-version-info 0:0:0
pkglib_LTLIBRARIES = libvlc_vdpau.la

libvlc_LTLIBRARIES =

libvdpau_avcodec_plugin_la_SOURCES = avcodec.c picture.c
libvdpau_avcodec_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(X_CFLAGS) $(CFLAGS_avcodec)
libvdpau_avcodec_plugin_la_LIBADD = $(AM_LIBADD) \
	$(X_LIBS) $(X_PRE_LIBS) -lX11 $(LIBS_avcodec)
libvdpau_avcodec_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(LDFLAGS_avcodec)
if HAVE_AVCODEC_VDPAU
libvlc_LTLIBRARIES += libvdpau_avcodec_plugin.la
endif

libvdpau_deinterlace_plugin_la_SOURCES = deinterlace.c picture.c
libvdpau_deinterlace_plugin_la_CFLAGS = $(AM_CFLAGS) # dummy
libvdpau_deinterlace_plugin_la_LIBADD = $(AM_LIBADD)
libvlc_LTLIBRARIES += libvdpau_deinterlace_plugin.la

libvdpau_adjust_plugin_la_SOURCES = adjust.c picture.c
libvdpau_adjust_plugin_la_CFLAGS = $(AM_CFLAGS) # dummy
libvdpau_adjust_plugin_la_LIBADD = $(AM_LIBADD)
libvlc_LTLIBRARIES += libvdpau_adjust_plugin.la

libvdpau_sharpen_plugin_la_SOURCES = sharpen.c picture.c
libvdpau_sharpen_plugin_la_CFLAGS = $(AM_CFLAGS) # dummy
libvdpau_sharpen_plugin_la_LIBADD = $(AM_LIBADD)
libvlc_LTLIBRARIES += libvdpau_sharpen_plugin.la

libvdpau_chroma_plugin_la_SOURCES = chroma.c picture.c
libvdpau_chroma_plugin_la_CFLAGS = $(AM_CFLAGS) # dummy
libvdpau_chroma_plugin_la_LIBADD = $(AM_LIBADD)
libvlc_LTLIBRARIES += libvdpau_chroma_plugin.la

libvdpau_display_plugin_la_SOURCES = \
	../../video_output/xcb/events.c \
	display.c
libvdpau_display_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(srcdir)/../../video_output/xcb
libvdpau_display_plugin_la_CFLAGS = $(AM_CFLAGS) \
	$(XCB_CFLAGS)
libvdpau_display_plugin_la_LIBADD = $(AM_LIBADD) \
	$(X_LIBS) $(X_PRE_LIBS) -lX11 $(XCB_LIBS)
libvlc_LTLIBRARIES += libvdpau_display_plugin.la
