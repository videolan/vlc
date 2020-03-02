miscdir = $(pluginsdir)/misc

libstats_plugin_la_SOURCES = misc/stats.c

misc_LTLIBRARIES = libstats_plugin.la

libaudioscrobbler_plugin_la_SOURCES = misc/audioscrobbler.c
libaudioscrobbler_plugin_la_LIBADD = $(SOCKET_LIBS)
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
        misc/webservices/json_helper.h \
	misc/fingerprinter.c
libfingerprinter_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir)/misc
libfingerprinter_plugin_la_LIBADD = $(LIBM)
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
libsecuretransport_plugin_la_LDFLAGS += -Wl,-U,_SSLCopyALPNProtocols,-U,_SSLSetALPNProtocols
misc_LTLIBRARIES += libsecuretransport_plugin.la
endif

if HAVE_OSX
libiokit_inhibit_plugin_la_SOURCES = misc/inhibit/iokit-inhibit.c
libiokit_inhibit_plugin_la_LDFLAGS = $(AM_LDFLAGS) -Wl,-framework,CoreFoundation,-framework,IOKit
misc_LTLIBRARIES += libiokit_inhibit_plugin.la
endif

libxdg_screensaver_plugin_la_SOURCES = misc/inhibit/xdg.c
if HAVE_XCB
if !HAVE_WIN32
misc_LTLIBRARIES += libxdg_screensaver_plugin.la
endif
endif

libdbus_screensaver_plugin_la_SOURCES = misc/inhibit/dbus.c
libdbus_screensaver_plugin_la_CFLAGS = $(AM_CFLAGS) $(DBUS_CFLAGS)
libdbus_screensaver_plugin_la_LIBADD = $(DBUS_LIBS)
if HAVE_DBUS
misc_LTLIBRARIES += libdbus_screensaver_plugin.la
endif

libwl_idle_inhibit_plugin_la_SOURCES = misc/inhibit/wl-idle-inhibit.c
nodist_libwl_idle_inhibit_plugin_la_SOURCES = \
	misc/inhibit/idle-inhibit-client-protocol.h \
	misc/inhibit/idle-inhibit-protocol.c
libwl_idle_inhibit_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)/misc/inhibit
libwl_idle_inhibit_plugin_la_CFLAGS = $(WAYLAND_CLIENT_CFLAGS)
libwl_idle_inhibit_plugin_la_LIBADD = $(WAYLAND_CLIENT_LIBS)
if HAVE_WAYLAND
misc_LTLIBRARIES += libwl_idle_inhibit_plugin.la
BUILT_SOURCES += $(nodist_libwl_idle_inhibit_plugin_la_SOURCES)
endif

misc/inhibit/idle-inhibit-client-protocol.h: \
		$(WAYLAND_PROTOCOLS)/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml \
		misc/Makefile.am
	$(AM_V_GEN)$(WAYLAND_SCANNER) client-header $< $@

misc/inhibit/idle-inhibit-protocol.c: \
		$(WAYLAND_PROTOCOLS)/unstable/idle-inhibit/idle-inhibit-unstable-v1.xml \
		misc/Makefile.am
	$(AM_V_GEN)$(WAYLAND_SCANNER) private-code $< $@

libaddonsvorepository_plugin_la_SOURCES = \
	misc/addons/vorepository.c misc/addons/xmlreading.h
libaddonsfsstorage_plugin_la_SOURCES = \
	misc/addons/fsstorage.c misc/addons/xmlreading.h
if ENABLE_ADDONMANAGERMODULES
misc_LTLIBRARIES += \
	libaddonsvorepository_plugin.la \
	libaddonsfsstorage_plugin.la
endif

libxml_plugin_la_SOURCES = misc/xml/libxml.c
libxml_plugin_la_CFLAGS = $(AM_CFLAGS) $(LIBXML2_CFLAGS)
libxml_plugin_la_LIBADD = $(LIBXML2_LIBS)
libxml_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(miscdir)' $(LDFLAGS_xml)
EXTRA_LTLIBRARIES += libxml_plugin.la
misc_LTLIBRARIES += $(LTLIBxml)

libmedialibrary_plugin_la_SOURCES = \
	misc/medialibrary/medialib.cpp \
	misc/medialibrary/MetadataExtractor.cpp \
	misc/medialibrary/entities.cpp \
	misc/medialibrary/Thumbnailer.cpp \
	misc/medialibrary/medialibrary.h \
	misc/medialibrary/fs/device.h \
	misc/medialibrary/fs/device.cpp \
	misc/medialibrary/fs/directory.h \
	misc/medialibrary/fs/directory.cpp \
	misc/medialibrary/fs/file.h \
	misc/medialibrary/fs/file.cpp \
	misc/medialibrary/fs/fs.h \
	misc/medialibrary/fs/fs.cpp \
	misc/medialibrary/fs/devicelister.cpp \
	misc/medialibrary/fs/devicelister.h \
	misc/medialibrary/fs/util.h \
	misc/medialibrary/fs/util.cpp

libmedialibrary_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(MEDIALIBRARY_CFLAGS)
libmedialibrary_plugin_la_LIBADD = $(MEDIALIBRARY_LIBS)
libmedialibrary_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(miscdir)'
EXTRA_LTLIBRARIES += libmedialibrary_plugin.la
misc_LTLIBRARIES += $(LTLIBmedialibrary)

