vaapidir = $(pluginsdir)/vaapi

libvlc_vaapi_instance_la_SOURCES = hw/vaapi/instance.c hw/vaapi/vlc_vaapi.h
libvlc_vaapi_instance_la_CFLAGS = $(LIBVA_CFLAGS)
libvlc_vaapi_instance_la_LIBADD = $(LIBVA_LIBS) $(LIBPTHREAD)
libvlc_vaapi_instance_la_LDFLAGS = -no-undefined	\
				   -version-info 0:0:0

if HAVE_VAAPI
vaapi_LTLIBRARIES = libvlc_vaapi_instance.la
endif
