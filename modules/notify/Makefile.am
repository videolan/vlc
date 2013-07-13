basedir = notify

include $(top_srcdir)/modules/common.am

libgrowl_plugin_la_SOURCES = growl.m
libgrowl_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) $(OBJCFLAGS_growl)
libgrowl_plugin_la_LIBADD = $(AM_LIBADD) $(LIBS_growl)
libgrowl_plugin_la_LIBTOOLFLAGS=--tag=CC

libnotify_plugin_la_SOURCES = notify.c
libnotify_plugin_la_CFLAGS = $(AM_CFLAGS) $(NOTIFY_CFLAGS)
libnotify_plugin_la_LIBADD = $(AM_LIBADD) $(NOTIFY_LIBS)

libvlc_LTLIBRARIES =  $(LTLIBgrowl) $(LTLIBnotify)
EXTRA_LTLIBRARIES = libgrowl_plugin.la libnotify_plugin.la
