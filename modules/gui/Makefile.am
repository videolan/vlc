guidir = $(pluginsdir)/gui
gui_LTLIBRARIES =

DIST_SUBDIRS += gui/macosx gui/qt4 gui/skins2

if ENABLE_MACOSX_UI
SUBDIRS += gui/macosx
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


libmacosx_dialog_provider_plugin_la_SOURCES = \
	gui/macosx_dialog_provider/VLCLoginPanel.h gui/macosx_dialog_provider/VLCLoginPanel.m \
	gui/macosx_dialog_provider/VLCProgressPanel.h gui/macosx_dialog_provider/VLCProgressPanel.m \
	gui/macosx_dialog_provider/VLCUIWidgets.h gui/macosx_dialog_provider/VLCUIWidgets.m \
	gui/macosx_dialog_provider/dialogProvider.h gui/macosx_dialog_provider/dialogProvider.m
libmacosx_dialog_provider_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(guidir)' -Wl,-framework,Cocoa

EXTRA_LTLIBRARIES += libmacosx_dialog_provider_plugin.la
gui_LTLIBRARIES += $(LTLIBmacosx_dialog_provider)


libminimal_macosx_plugin_la_SOURCES = \
	gui/minimal_macosx/intf.h gui/minimal_macosx/intf.m \
	gui/minimal_macosx/misc.h gui/minimal_macosx/misc.m \
	gui/minimal_macosx/VLCMinimalVoutWindow.h gui/minimal_macosx/VLCMinimalVoutWindow.m \
	gui/minimal_macosx/macosx.c
libminimal_macosx_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS) -fobjc-arc -fobjc-exceptions
libminimal_macosx_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(guidir)' -Wl,-framework,Cocoa
EXTRA_LTLIBRARIES += libminimal_macosx_plugin.la
gui_LTLIBRARIES += $(LTLIBminimal_macosx)


libios_dialog_provider_plugin_la_SOURCES = gui/ios_dialog_provider/dialogProvider.m
libios_dialog_provider_plugin_la_OBJCFLAGS = $(AM_OBJCFLAGS)
libios_dialog_provider_plugin_la_LDFLAGS = $(AM_LDFLAGS) -Wl,-framework,UIKit
if HAVE_IOS
gui_LTLIBRARIES += libios_dialog_provider_plugin.la
endif
