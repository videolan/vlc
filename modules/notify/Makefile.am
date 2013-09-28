notifydir = $(pluginsdir)/notify

libgrowl_plugin_la_SOURCES = notify/growl.m
libgrowl_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) $(OBJCFLAGS_growl)
libgrowl_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(notifydir)'
libgrowl_plugin_la_LIBADD = $(LIBS_growl)
libgrowl_plugin_la_LIBTOOLFLAGS = --tag=CC

libnotify_plugin_la_SOURCES = notify/notify.c
libnotify_plugin_la_CFLAGS = $(AM_CFLAGS) $(NOTIFY_CFLAGS)
libnotify_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(notifydir)'
libnotify_plugin_la_LIBADD = $(NOTIFY_LIBS)

notify_LTLIBRARIES = $(LTLIBgrowl) $(LTLIBnotify)
EXTRA_LTLIBRARIES += libgrowl_plugin.la libnotify_plugin.la
