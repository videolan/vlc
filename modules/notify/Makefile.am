notifydir = $(pluginsdir)/notify

libosx_notifications_plugin_la_SOURCES = notify/osx_notifications.m
libosx_notifications_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) $(OBJCFLAGS_osx_notifications)
libosx_notifications_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(notifydir)' -Wl,-framework,AppKit
libosx_notifications_plugin_la_LIBADD = $(LIBS_osx_notifications)

libnotify_plugin_la_SOURCES = notify/notify.c
libnotify_plugin_la_CFLAGS = $(AM_CFLAGS) $(NOTIFY_CFLAGS)
libnotify_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(notifydir)'
libnotify_plugin_la_LIBADD = $(NOTIFY_LIBS)

notify_LTLIBRARIES = $(LTLIBosx_notifications) $(LTLIBnotify)
EXTRA_LTLIBRARIES += libosx_notifications_plugin.la libnotify_plugin.la
