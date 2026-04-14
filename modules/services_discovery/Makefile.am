sddir = $(pluginsdir)/services_discovery
sd_PLUGINS =
sd_LTLIBRARIES =
sd_RPATH =
if HAVE_PARTIAL_LINKING
noinst_LTLIBRARIES += $(sd_PLUGINS)
else
sd_LTLIBRARIES += $(sd_PLUGINS)
sd_RPATH += -rpath '$(sddir)'
endif

libpodcast_plugin_la_SOURCES = services_discovery/podcast.c
sd_PLUGINS += libpodcast_plugin.la

libradio_plugin_la_SOURCES = services_discovery/radio.c
sd_PLUGINS += libradio_plugin.la

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
if ENABLE_UPNP_SERVER
libupnp_plugin_la_SOURCES += \
	control/upnp_server/cds/Object.hpp \
	control/upnp_server/cds/Container.hpp \
	control/upnp_server/cds/FixedContainer.cpp \
	control/upnp_server/cds/FixedContainer.hpp \
	control/upnp_server/cds/Item.cpp \
	control/upnp_server/cds/Item.hpp \
	control/upnp_server/cds/MLContainer.hpp \
	control/upnp_server/cds/MLFolderContainer.hpp \
	control/upnp_server/cds/cds.cpp \
	control/upnp_server/cds/cds.hpp \
	control/upnp_server/ml.hpp \
	control/upnp_server/xml_wrapper.hpp \
	control/upnp_server/upnp_server.hpp \
	control/upnp_server/utils.cpp \
	control/upnp_server/utils.hpp \
	control/upnp_server/FileHandler.cpp \
	control/upnp_server/FileHandler.hpp \
	control/upnp_server/upnp_server.cpp
endif
libupnp_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(UPNP_CFLAGS)
libupnp_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(sd_RPATH)
libupnp_plugin_la_LIBADD = $(UPNP_LIBS)
if ENABLE_UPNP_SERVER
libupnp_plugin_la_RES = ../share/upnp_server/ContentDirectory.xml \
	../share/upnp_server/ConnectionManager.xml \
	../share/upnp_server/X_MS_MediaReceiverRegistrar.xml \
	../share/upnp_server/vlc.png \
	../share/upnp_server/vlc512x512.png
dist_libupnp_plugin_la_DATA = $(libupnp_plugin_la_RES)
libupnp_plugin_ladir = $(prefix)/share/vlc/upnp_server
endif
EXTRA_LTLIBRARIES += libupnp_plugin.la
sd_PLUGINS += $(LTLIBupnp)
if HAVE_OSX
libupnp_plugin_la_LDFLAGS += -Wl,-framework,CoreFoundation,-framework,SystemConfiguration
endif

libpipewirelist_plugin_la_SOURCES = services_discovery/pipewire.c
libpipewirelist_plugin_la_CFLAGS = $(AM_CFLAGS) $(PIPEWIRE_CFLAGS)
libpipewirelist_plugin_la_LIBADD = libvlc_pipewire.la $(PIPEWIRE_LIBS)
if HAVE_PIPEWIRE
sd_PLUGINS += libpipewirelist_plugin.la
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

libwinvolumes_plugin_la_SOURCES = services_discovery/winvolumes.c
if HAVE_WIN32_DESKTOP
sd_PLUGINS += libwinvolumes_plugin.la
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
if !HAVE_WATCHOS
sd_PLUGINS += libbonjour_plugin.la
endif
endif

libudisks_plugin_la_SOURCES = services_discovery/udisks.c
libudisks_plugin_la_CFLAGS = $(AM_CFLAGS) $(SDBUS_CFLAGS)
libudisks_plugin_la_LIBADD = $(SDBUS_LIBS)
if HAVE_SDBUS
sd_PLUGINS += libudisks_plugin.la
endif
