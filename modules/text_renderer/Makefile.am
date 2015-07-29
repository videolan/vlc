textdir = $(pluginsdir)/text_renderer

libtdummy_plugin_la_SOURCES = text_renderer/tdummy.c
text_LTLIBRARIES = libtdummy_plugin.la

libfreetype_plugin_la_SOURCES = \
	text_renderer/platform_fonts.c text_renderer/platform_fonts.h \
	text_renderer/freetype.c text_renderer/freetype.h \
	text_renderer/text_layout.c text_renderer/text_layout.h

libfreetype_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(FREETYPE_CFLAGS)
libfreetype_plugin_la_LIBADD = $(LIBM) $(FREETYPE_LIBS)
if HAVE_FREETYPE
text_LTLIBRARIES += libfreetype_plugin.la
endif
if HAVE_FONTCONFIG
libfreetype_plugin_la_CPPFLAGS += -DHAVE_FONTCONFIG
libfreetype_plugin_la_LIBADD += -lfontconfig
endif
if HAVE_FRIBIDI
libfreetype_plugin_la_CPPFLAGS += $(FRIBIDI_CFLAGS) -DHAVE_FRIBIDI
libfreetype_plugin_la_LIBADD += $(FRIBIDI_LIBS)
endif
if HAVE_HARFBUZZ
libfreetype_plugin_la_CPPFLAGS += $(HARFBUZZ_CFLAGS) -DHAVE_HARFBUZZ
libfreetype_plugin_la_LIBADD += $(HARFBUZZ_LIBS)
endif
libfreetype_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(FREETYPE_LDFLAGS) -rpath '$(textdir)'
if HAVE_DARWIN
libfreetype_plugin_la_LDFLAGS += -Wl,-framework,Carbon
endif

libquartztext_plugin_la_SOURCES = text_renderer/quartztext.c
libquartztext_plugin_la_LDFLAGS = $(AM_LDLFAGS) -rpath '$(textdir)' \
	-Wl,-framework,ApplicationServices
EXTRA_LTLIBRARIES += libquartztext_plugin.la
text_LTLIBRARIES += $(LTLIBquartztext)

if HAVE_OSX
libnsspeechsynthesizer_plugin_la_SOURCES = text_renderer/nsspeechsynthesizer.m
libnsspeechsynthesizer_plugin_la_LDFLAGS = -Wl,-framework,Cocoa
text_LTLIBRARIES += libnsspeechsynthesizer_plugin.la
endif

libsvg_plugin_la_SOURCES = text_renderer/svg.c
libsvg_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SVG_CFLAGS)
libsvg_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(textdir)'
libsvg_plugin_la_LIBADD = $(SVG_LIBS)
EXTRA_LTLIBRARIES += libsvg_plugin.la
text_LTLIBRARIES += $(LTLIBsvg)

libwin32text_plugin_la_SOURCES = text_renderer/win32text.c
libwin32text_plugin_la_LIBADD = -lgdi32
if HAVE_WIN32
libfreetype_plugin_la_LIBADD += -liconv -lz
if !HAVE_WINSTORE
libfreetype_plugin_la_LIBADD += -lgdi32
text_LTLIBRARIES += libwin32text_plugin.la
endif
endif
