gbmdir = $(pluginsdir)/gbm

libgbm_plugin_la_SOURCES = hw/gbm/gbm.c
libgbm_plugin_la_CFLAGS = $(GBM_CFLAGS)
libgbm_plugin_la_LDFLAGS = $(AM_LDFLAGS) -lEGL -lGL $(GBM_LIBS)

if HAVE_LINUX
if !HAVE_ANDROID
gbm_LTLIBRARIES = libgbm_plugin.la
endif
endif
