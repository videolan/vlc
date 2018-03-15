
object_loaderdir = $(pluginsdir)/object_loader

# 3d object loader
libassimp_plugin_la_SOURCES = object_loader/assimp.cpp misc/webservices/json.c
libassimp_plugin_la_LIBADD = $(LIBM) $(ASSIMP_LIBS) -lIrrXML -lz
libassimp_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(object_loaderdir)'

object_loader_LTLIBRARIES = $(LTLIBassimp)
EXTRA_LTLIBRARIES += libassimp_plugin.la
