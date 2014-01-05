luadir = $(pluginsdir)/lua

liblua_plugin_la_SOURCES = \
	lua/extension.c lua/extension.h \
	lua/extension_thread.c \
	lua/intf.c \
	lua/meta.c \
	lua/demux.c \
	lua/services_discovery.c \
	lua/vlc.c lua/vlc.h \
	lua/libs.h \
	lua/libs/configuration.c \
	lua/libs/equalizer.c \
	lua/libs/gettext.c \
	lua/libs/dialog.c \
	lua/libs/httpd.c \
	lua/libs/input.c lua/libs/input.h \
	lua/libs/messages.c \
	lua/libs/misc.c lua/libs/misc.h \
	lua/libs/net.c \
	lua/libs/objects.c lua/libs/objects.h \
	lua/libs/osd.c \
	lua/libs/playlist.c \
	lua/libs/sd.c \
	lua/libs/stream.c \
	lua/libs/strings.c \
	lua/libs/variables.c lua/libs/variables.h \
	lua/libs/video.c \
	lua/libs/vlm.c \
	lua/libs/volume.c \
	lua/libs/xml.c
if HAVE_WIN32
liblua_plugin_la_SOURCES += lua/libs/win.c
endif
liblua_plugin_la_CFLAGS = $(AM_CFLAGS) $(LUA_CFLAGS) -I$(srcdir)/lua
liblua_plugin_la_LIBADD = $(LUA_LIBS) $(SOCKET_LIBS) $(LIBDL) $(LIBM) $(LIBPTHREAD)
if BUILD_LUA
lua_LTLIBRARIES = liblua_plugin.la
endif
