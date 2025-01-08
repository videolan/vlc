AUTOMAKE_OPTIONS = subdir-objects

#
# Preparser executable
#
if BUILD_VLC
if HAVE_DYNAMIC_PLUGINS
pkglibexec_PROGRAMS += vlc-preparser
else
pkglibexec_PROGRAMS = vlc-preparser
endif
noinst_PROGRAMS += vlc-preparser-static
endif

vlc_preparser_SOURCES = \
		../src/config/getopt.c \
		../src/config/jaro_winkler.c \
		preparser/cmdline.c \
		preparser/cmdline.h \
		preparser/main.c

vlc_preparser_CFLAGS = $(vlc_CFLAGS)

vlc_preparser_LDADD = \
		../compat/libcompat.la \
		$(SOCKET_LIBS) \
		$(vlc_LDADD)

vlc_preparser_LDFLAGS = $(vlc_LDFLAGS)
vlc_preparser_DEPENDENCIES = \
		$(vlc_DEPENDENCIES)

if HAVE_OSX
vlc_preparser_LDFLAGS += -Wl,-framework,CoreFoundation,-framework,Cocoa
vlc_preparser_LDFLAGS += -Xlinker -rpath -Xlinker "$(libdir)"
if HAVE_BREAKPAD
vlc_preparser_LDFLAGS += -Wl,-framework,Breakpad
vlc_preparser_OBJCFLAGS = -F$(CONTRIB_DIR)/Frameworks
vlc_preparser_CPPFLAGS = -DHAVE_BREAKPAD
endif
endif

if HAVE_WIN32
vlc_preparser_LDFLAGS += -Wc,-static
endif


vlc_preparser_static_SOURCES = $(vlc_preparser_SOURCES)
vlc_preparser_static_CFLAGS = $(AM_CFLAGS) \
	-DTOP_BUILDDIR=\"$(abs_top_builddir)\" \
	-DTOP_SRCDIR=\"$(abs_top_srcdir)\" \
	$(NULL)

vlc_preparser_static_LDADD = $(vlc_preparser_LDADD)
if HAVE_OSX
vlc_preparser_static_LDFLAGS = $(vlc_preparser_LDFLAGS) -static
vlc_preparser_static_LDFLAGS += -Xlinker -rpath -Xlinker "@executable_path/../lib/.libs/"
vlc_preparser_static_LDFLAGS += -Xlinker -rpath -Xlinker "@executable_path/../src/.libs/"
vlc_preparser_static_LDFLAGS += -Xlinker -rpath -Xlinker "@executable_path/../Frameworks/"
else
vlc_preparser_static_LDFLAGS = $(vlc_preparser_LDFLAGS) -no-install -static
endif
