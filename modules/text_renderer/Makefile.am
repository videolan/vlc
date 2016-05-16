textdir = $(pluginsdir)/text_renderer

libtdummy_plugin_la_SOURCES = text_renderer/tdummy.c
text_LTLIBRARIES = libtdummy_plugin.la

libfreetype_plugin_la_SOURCES = \
	text_renderer/freetype/platform_fonts.c text_renderer/freetype/platform_fonts.h \
	text_renderer/freetype/freetype.c text_renderer/freetype/freetype.h \
	text_renderer/freetype/text_layout.c text_renderer/freetype/text_layout.h

libfreetype_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(FREETYPE_CFLAGS)
libfreetype_plugin_la_LIBADD = $(LIBM) $(FREETYPE_LIBS)
libfreetype_plugin_la_LDFLAGS = $(AM_LDFLAGS) $(FREETYPE_LDFLAGS) -rpath '$(textdir)'

if HAVE_WIN32_DESKTOP
libfreetype_plugin_la_SOURCES += text_renderer/freetype/fonts/win32.c
libfreetype_plugin_la_LIBADD += -liconv -lz -lgdi32 -lusp10
endif
if HAVE_FONTCONFIG
libfreetype_plugin_la_SOURCES += text_renderer/freetype/fonts/fontconfig.c
libfreetype_plugin_la_CPPFLAGS += -DHAVE_FONTCONFIG
libfreetype_plugin_la_LIBADD += -lfontconfig
endif
if HAVE_ANDROID
libfreetype_plugin_la_SOURCES += text_renderer/freetype/fonts/android.c
endif
if HAVE_DARWIN
libfreetype_plugin_la_SOURCES += text_renderer/freetype/fonts/darwin.c
libfreetype_plugin_la_LDFLAGS += -Wl,-framework,CoreFoundation -Wl,-framework,CoreText
endif
if HAVE_FRIBIDI
libfreetype_plugin_la_CPPFLAGS += $(FRIBIDI_CFLAGS) -DHAVE_FRIBIDI
libfreetype_plugin_la_LIBADD += $(FRIBIDI_LIBS)
endif
if HAVE_HARFBUZZ
libfreetype_plugin_la_CPPFLAGS += $(HARFBUZZ_CFLAGS) -DHAVE_HARFBUZZ
libfreetype_plugin_la_LIBADD += $(HARFBUZZ_LIBS)
endif
if HAVE_FREETYPE
text_LTLIBRARIES += libfreetype_plugin.la
endif

# SVG plugin
libsvg_plugin_la_SOURCES = text_renderer/svg.c
libsvg_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(SVG_CFLAGS)
libsvg_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(textdir)'
libsvg_plugin_la_LIBADD = $(SVG_LIBS)
EXTRA_LTLIBRARIES += libsvg_plugin.la
text_LTLIBRARIES += $(LTLIBsvg)

# OS X TTS
libnsspeechsynthesizer_plugin_la_SOURCES = text_renderer/nsspeechsynthesizer.m
libnsspeechsynthesizer_plugin_la_LDFLAGS = -Wl,-framework,Cocoa
if HAVE_OSX
text_LTLIBRARIES += libnsspeechsynthesizer_plugin.la
endif

# SAPI TTS (win32)
libsapi_plugin_la_SOURCES = text_renderer/sapi.cpp
libsapi_plugin_la_LIBADD = -lole32
if HAVE_SAPI
text_LTLIBRARIES += libsapi_plugin.la
endif
