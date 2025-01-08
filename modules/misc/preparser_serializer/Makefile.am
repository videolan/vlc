
libpreparserserializer_json_plugin_la_SOURCES = \
	misc/preparser_serializer/json/serializer.c \
	misc/preparser_serializer/json/serializer.h \
	misc/preparser_serializer/json/fromjson.c \
	misc/preparser_serializer/json/tojson.c

libpreparserserializer_json_plugin_la_CFLAGS = $(AM_CFLAGS)
libpreparserserializer_json_plugin_la_LIBADD = libvlc_json.la
libpreparserserializer_json_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(miscdir)'
misc_LTLIBRARIES += libpreparserserializer_json_plugin.la
