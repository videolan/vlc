sddir = $(pluginsdir)/services_discovery
sd_PLUGINS =
sd_LTLIBRARIES =
sd_RPATH =
if HAVE_MERGE_PLUGINS
noinst_LTLIBRARIES += $(sd_PLUGINS)
else
sd_LTLIBRARIES += $(sd_PLUGINS)
sd_RPATH += -rpath '$(sddir)'
endif

libpodcast_plugin_la_SOURCES = services_discovery/podcast.c
sd_PLUGINS += libpodcast_plugin.la

libsap_plugin_la_SOURCES = services_discovery/sap.c access/rtp/sdp.c
libsap_plugin_la_CPPFLAGS = $(AM_CPPFLAGS)
libsap_plugin_la_LIBADD = $(SOCKET_LIBS) $(LIBZ)
sd_PLUGINS += libsap_plugin.la

libavahi_plugin_la_SOURCES = services_discovery/avahi.c
libavahi_plugin_la_CFLAGS = $(AM_CFLAGS) $(AVAHI_CFLAGS)
libavahi_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(sd_RPATH)
libavahi_plugin_la_LIBADD = $(AVAHI_LIBS)
EXTRA_LTLIBRARIES += libavahi_plugin.la
sd_PLUGINS += $(LTLIBavahi)

libmtp_plugin_la_SOURCES = services_discovery/mtp.c
libmtp_plugin_la_CFLAGS = $(AM_CFLAGS) $(MTP_CFLAGS)
libmtp_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(sd_RPATH)
libmtp_plugin_la_LIBADD = $(MTP_LIBS)
EXTRA_LTLIBRARIES += libmtp_plugin.la
sd_PLUGINS += $(LTLIBmtp)

libupnp_plugin_la_SOURCES = services_discovery/upnp.cpp services_discovery/upnp.hpp \
			    services_discovery/upnp-wrapper.hpp \
			    services_discovery/upnp-wrapper.cpp \
			    stream_out/renderer_common.hpp \
			    stream_out/renderer_common.cpp \
			    stream_out/dlna/profile_names.hpp \
			    stream_out/dlna/dlna_common.hpp \
			    stream_out/dlna/dlna.hpp \
			    stream_out/dlna/dlna.cpp
libupnp_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(UPNP_CFLAGS)
libupnp_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(sd_RPATH)
libupnp_plugin_la_LIBADD = $(UPNP_LIBS)
EXTRA_LTLIBRARIES += libupnp_plugin.la
sd_PLUGINS += $(LTLIBupnp)
if HAVE_OSX
libupnp_plugin_la_LDFLAGS += -Wl,-framework,CoreFoundation,-framework,SystemConfiguration
endif

libpulselist_plugin_la_SOURCES = services_discovery/pulse.c
libpulselist_plugin_la_CFLAGS = $(AM_CFLAGS) $(PULSE_CFLAGS)
libpulselist_plugin_la_LIBADD = libvlc_pulse.la $(PULSE_LIBS)
if HAVE_PULSE
sd_PLUGINS += libpulselist_plugin.la
endif

libudev_plugin_la_SOURCES = services_discovery/udev.c
libudev_plugin_la_CFLAGS = $(AM_CFLAGS) $(UDEV_CFLAGS)
libudev_plugin_la_LIBADD = $(UDEV_LIBS)
if HAVE_ALSA
libudev_plugin_la_CFLAGS += $(ALSA_CFLAGS) -DHAVE_ALSA
libudev_plugin_la_LIBADD += $(ALSA_LIBS)
endif
libudev_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(sd_RPATH)
EXTRA_LTLIBRARIES += libudev_plugin.la
sd_PLUGINS += $(LTLIBudev)

libxcb_apps_plugin_la_SOURCES = services_discovery/xcb_apps.c
libxcb_apps_plugin_la_CFLAGS = $(AM_CFLAGS) $(XCB_CFLAGS)
libxcb_apps_plugin_la_LIBADD = $(XCB_LIBS)
if HAVE_XCB
sd_PLUGINS += libxcb_apps_plugin.la
endif

libwindrive_plugin_la_SOURCES = services_discovery/windrive.c
if HAVE_WIN32_DESKTOP
sd_PLUGINS += libwindrive_plugin.la
endif

libos2drive_plugin_la_SOURCES = services_discovery/os2drive.c
if HAVE_OS2
sd_PLUGINS += libos2drive_plugin.la
endif

libmicrodns_plugin_la_SOURCES = services_discovery/microdns.c
libmicrodns_plugin_la_CFLAGS = $(AM_CFLAGS) $(MICRODNS_CFLAGS)
libmicrodns_plugin_la_LIBADD = $(SOCKET_LIBS) $(MICRODNS_LIBS)
libmicrodns_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(sd_RPATH)
sd_PLUGINS += $(LTLIBmicrodns)
EXTRA_LTLIBRARIES += libmicrodns_plugin.la

libbonjour_plugin_la_SOURCES = services_discovery/bonjour.m
libbonjour_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-arc
libbonjour_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(sd_RPATH) -Wl,-framework,Foundation
if HAVE_DARWIN
sd_PLUGINS += libbonjour_plugin.la
endif

libudisks_plugin_la_SOURCES = services_discovery/udisks.c
libudisks_plugin_la_CFLAGS = $(AM_CFLAGS) $(SDBUS_CFLAGS)
libudisks_plugin_la_LIBADD = $(SDBUS_LIBS)
if HAVE_SDBUS
sd_PLUGINS += libudisks_plugin.la
endif
