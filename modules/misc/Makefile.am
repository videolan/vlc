miscdir = $(pluginsdir)/misc

liblogger_plugin_la_SOURCES = misc/logger.c
libstats_plugin_la_SOURCES = misc/stats.c

misc_LTLIBRARIES = liblogger_plugin.la libstats_plugin.la

libaudioscrobbler_plugin_la_SOURCES = misc/audioscrobbler.c
libaudioscrobbler_plugin_la_LIBADD = $(SOCKET_LIBS) $(LIBPTHREAD)
misc_LTLIBRARIES += libaudioscrobbler_plugin.la

libexport_plugin_la_SOURCES = \
	misc/playlist/html.c \
	misc/playlist/m3u.c \
	misc/playlist/xspf.c \
	misc/playlist/export.c
misc_LTLIBRARIES += libexport_plugin.la

libfingerprinter_plugin_la_SOURCES =  \
	misc/webservices/acoustid.c misc/webservices/acoustid.h \
	misc/webservices/json.c misc/webservices/json.h \
	misc/fingerprinter.c
libfingerprinter_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/misc
libfingerprinter_plugin_la_LIBADD = $(LIBM) $(LIBPTHREAD)
misc_LTLIBRARIES += libfingerprinter_plugin.la

libgnutls_plugin_la_SOURCES = misc/gnutls.c
libgnutls_plugin_la_CFLAGS = $(AM_CFLAGS) $(GNUTLS_CFLAGS)
libgnutls_plugin_la_LIBADD = $(GNUTLS_LIBS)
if HAVE_WIN32
libgnutls_plugin_la_LIBADD += $(SOCKET_LIBS)
endif
libgnutls_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(miscdir)'
if HAVE_DARWIN
libgnutls_plugin_la_LDFLAGS += -Wl,-framework,Security,-framework,CoreFoundation
endif
EXTRA_LTLIBRARIES += libgnutls_plugin.la
misc_LTLIBRARIES += $(LTLIBgnutls)

if HAVE_DARWIN
libsecuretransport_plugin_la_SOURCES = misc/securetransport.c
libsecuretransport_plugin_la_CFLAGS = $(AM_CFLAGS) $(SECURETRANSPORT_CFLAGS)
libsecuretransport_plugin_la_LIBADD = $(SECURETRANSPORT_LIBS)
libsecuretransport_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(miscdir)' -Wl,-framework,Security,-framework,CoreFoundation
misc_LTLIBRARIES += libsecuretransport_plugin.la
endif

libxdg_screensaver_plugin_la_SOURCES = misc/inhibit/xdg.c
if HAVE_XCB
misc_LTLIBRARIES += libxdg_screensaver_plugin.la
endif

libdbus_screensaver_plugin_la_SOURCES = misc/inhibit/dbus.c
libdbus_screensaver_plugin_la_CFLAGS = $(AM_CFLAGS) $(DBUS_CFLAGS)
libdbus_screensaver_plugin_la_LIBADD = $(DBUS_LIBS)
if HAVE_DBUS
misc_LTLIBRARIES += libdbus_screensaver_plugin.la
endif

libaddonsvorepository_plugin_la_SOURCES = \
	misc/addons/vorepository.c misc/addons/xmlreading.h
libaddonsfsstorage_plugin_la_SOURCES = \
	misc/addons/fsstorage.c misc/addons/xmlreading.h
if ENABLE_ADDONMANAGERMODULES
misc_LTLIBRARIES += \
	libaddonsvorepository_plugin.la \
	libaddonsfsstorage_plugin.la
endif

libvod_rtsp_plugin_la_SOURCES = misc/rtsp.c
libvod_rtsp_plugin_la_LIBADD = $(SOCKET_LIBS)
if ENABLE_SOUT
misc_LTLIBRARIES += libvod_rtsp_plugin.la
endif

libxml_plugin_la_SOURCES = misc/xml/libxml.c
libxml_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBXML2_CFLAGS)
libxml_plugin_la_LIBADD = $(LIBXML2_LIBS)
libxml_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(miscdir)' $(LDFLAGS_xml)
EXTRA_LTLIBRARIES += libxml_plugin.la
misc_LTLIBRARIES += $(LTLIBxml)
