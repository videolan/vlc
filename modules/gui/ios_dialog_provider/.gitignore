guidir = $(pluginsdir)/gui
gui_LTLIBRARIES =

DIST_SUBDIRS += gui/macosx gui/minimal_macosx gui/qt4 gui/skins2 gui/macosx_dialog_provider gui/ios_dialog_provider

if ENABLE_MACOSX_UI
SUBDIRS += gui/macosx
endif
if ENABLE_MACOSX_DIALOG_PROVIDER
SUBDIRS += gui/macosx_dialog_provider
endif
if HAVE_IOS
SUBDIRS += gui/ios_dialog_provider
endif
if ENABLE_MINIMAL_MACOSX
SUBDIRS += gui/minimal_macosx
endif
if ENABLE_QT4
SUBDIRS += gui/qt4
endif
if BUILD_SKINS
SUBDIRS += gui/skins2
endif

libncurses_plugin_la_SOURCES = gui/ncurses.c
libncurses_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(NCURSES_CFLAGS)
libncurses_plugin_la_LIBADD = $(NCURSES_LIBS) $(LIBPTHREAD) $(LIBM)
if HAVE_NCURSES
gui_LTLIBRARIES += libncurses_plugin.la
endif

EXTRA_DIST += gui/eject.c
