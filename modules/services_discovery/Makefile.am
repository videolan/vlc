sddir = $(pluginsdir)/services_discovery
sd_LTLIBRARIES =

libmediadirs_plugin_la_SOURCES = services_discovery/mediadirs.c
sd_LTLIBRARIES += libmediadirs_plugin.la

libpodcast_plugin_la_SOURCES = services_discovery/podcast.c
libpodcast_plugin_la_LIBADD = $(LIBPTHREAD)
sd_LTLIBRARIES += libpodcast_plugin.la

libsap_plugin_la_SOURCES = services_discovery/sap.c
libsap_plugin_la_CFLAGS = $(AM_CFLAGS) $(CFLAGS_sap)
libsap_plugin_la_LIBADD = $(LIBS_sap) $(SOCKET_LIBS)
sd_LTLIBRARIES += libsap_plugin.la

libbonjour_plugin_la_SOURCES = services_discovery/bonjour.c
libbonjour_plugin_la_CFLAGS = $(AM_CFLAGS) $(BONJOUR_CFLAGS)
libbonjour_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(sddir)'
libbonjour_plugin_la_LIBADD = $(BONJOUR_LIBS)
EXTRA_LTLIBRARIES += libbonjour_plugin.la
sd_LTLIBRARIES += $(LTLIBbonjour)

libmtp_plugin_la_SOURCES = services_discovery/mtp.c
libmtp_plugin_la_CFLAGS = $(AM_CFLAGS) $(MTP_CFLAGS)
libmtp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(sddir)'
libmtp_plugin_la_LIBADD = $(MTP_LIBS)
EXTRA_LTLIBRARIES += libmtp_plugin.la
sd_LTLIBRARIES += $(LTLIBmtp)

libupnp_plugin_la_SOURCES = services_discovery/upnp.cpp services_discovery/upnp.hpp
libupnp_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(UPNP_CFLAGS)
libupnp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(sddir)'
libupnp_plugin_la_LIBADD = $(UPNP_LIBS)
EXTRA_LTLIBRARIES += libupnp_plugin.la
sd_LTLIBRARIES += $(LTLIBupnp)

libpulselist_plugin_la_SOURCES = services_discovery/pulse.c
libpulselist_plugin_la_CFLAGS = $(AM_CFLAGS) $(PULSE_CFLAGS)
libpulselist_plugin_la_LIBADD = libvlc_pulse.la $(PULSE_LIBS)
if HAVE_PULSE
sd_LTLIBRARIES += libpulselist_plugin.la
endif

libudev_plugin_la_SOURCES = services_discovery/udev.c
libudev_plugin_la_CFLAGS = $(AM_CFLAGS) $(UDEV_CFLAGS)
libudev_plugin_la_LIBADD = $(UDEV_LIBS)
if HAVE_ALSA
libudev_plugin_la_CFLAGS += $(ALSA_CFLAGS) -DHAVE_ALSA
libudev_plugin_la_LIBADD += $(ALSA_LIBS)
endif
libudev_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(sddir)'
EXTRA_LTLIBRARIES += libudev_plugin.la
sd_LTLIBRARIES += $(LTLIBudev)

libxcb_apps_plugin_la_SOURCES = services_discovery/xcb_apps.c
libxcb_apps_plugin_la_CFLAGS = $(AM_CFLAGS) $(XCB_CFLAGS)
libxcb_apps_plugin_la_LIBADD = $(XCB_LIBS)
if HAVE_XCB
sd_LTLIBRARIES += libxcb_apps_plugin.la
endif

libwindrive_plugin_la_SOURCES = services_discovery/windrive.c
if HAVE_WIN32
sd_LTLIBRARIES += libwindrive_plugin.la
endif

libos2drive_plugin_la_SOURCES = services_discovery/os2drive.c
if HAVE_OS2
sd_LTLIBRARIES += libos2drive_plugin.la
endif
