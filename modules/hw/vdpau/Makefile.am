basedir = vdpau
include $(top_srcdir)/modules/common.am

AM_CFLAGS += $(VDPAU_CFLAGS)

libvlc_vdpau_la_SOURCES = vlc_vdpau.c vlc_vdpau.h
libvlc_vdpau_la_CPPFLAGS =
libvlc_vdpau_la_LIBADD = $(X_LIBS) $(X_PRE_LIBS) -lX11 \
	$(LIBDL) $(LIBPTHREAD)
libvlc_vdpau_la_LDFLAGS = \
	-no-undefined \
	-export-symbols-regex ^vdp_ \
	-version-info 0:0:0
pkglib_LTLIBRARIES = libvlc_vdpau.la
