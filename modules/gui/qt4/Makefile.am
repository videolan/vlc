include $(top_srcdir)/modules/common.am
guidir = $(pluginsdir)/gui

## Howto
# For each Q_OBJECT:
#   - Add *.moc.cpp to nodist_..._SOURCES
#   - Add *.cpp to ..._SOURCES
#   - Add *.hpp to ..._SOURCES
# For each UI
#   - Add *.ui to ..._UI
#   - Add *.h to nodist_..._SOURCES
# For each ressource (icon, png, ...)
#   - Add it to DEPS_res

SUFFIXES += .ui .h .hpp .moc.cpp

libqt4_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) $(CXXFLAGS_qt4)
libqt4_plugin_la_LIBADD = $(QT_LIBS) $(LIBS_qt4)
libqt4_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(guidir)'
if HAVE_OS2
else
if HAVE_WIN32
libqt4_plugin_la_LIBADD += -lole32 -lcomctl32 -luuid
else
libqt4_plugin_la_LIBADD += $(X_LIBS) $(X_PRE_LIB) -lX11
endif
endif
if HAVE_DARWIN
libqt4_plugin_la_LDFLAGS += -Wl,-framework,Cocoa
endif

gui_LTLIBRARIES =  $(LTLIBqt4)
EXTRA_LTLIBRARIES = libqt4_plugin.la

libqt4_plugin_la_SOURCES = \
	qt4.cpp qt4.hpp \
	menus.cpp menus.hpp \
	main_interface.cpp main_interface.hpp \
	dialogs_provider.cpp dialogs_provider.hpp \
	input_manager.cpp input_manager.hpp \
	actions_manager.cpp actions_manager.hpp \
	extensions_manager.cpp extensions_manager.hpp \
	managers/addons_manager.cpp managers/addons_manager.hpp \
	recents.cpp recents.hpp \
	adapters/seekpoints.cpp adapters/seekpoints.hpp \
	adapters/chromaprint.cpp adapters/chromaprint.hpp \
	adapters/variables.cpp adapters/variables.hpp \
	dialogs/playlist.cpp dialogs/playlist.hpp \
	dialogs/bookmarks.cpp dialogs/bookmarks.hpp \
	dialogs/preferences.cpp dialogs/preferences.hpp \
	dialogs/mediainfo.cpp dialogs/mediainfo.hpp \
	dialogs/epg.cpp dialogs/epg.hpp \
	dialogs/extended.cpp dialogs/extended.hpp \
	dialogs/messages.cpp dialogs/messages.hpp \
	dialogs/errors.cpp dialogs/errors.hpp \
	dialogs/external.cpp dialogs/external.hpp \
	dialogs/plugins.cpp dialogs/plugins.hpp \
	dialogs/sout.cpp dialogs/sout.hpp \
	dialogs/convert.cpp dialogs/convert.hpp \
	dialogs/help.cpp dialogs/help.hpp \
	dialogs/gototime.cpp dialogs/gototime.hpp \
	dialogs/toolbar.cpp dialogs/toolbar.hpp \
	dialogs/open.cpp dialogs/open.hpp \
	dialogs/openurl.cpp dialogs/openurl.hpp \
	dialogs/vlm.cpp dialogs/vlm.hpp \
	dialogs/firstrun.cpp dialogs/firstrun.hpp \
	dialogs/podcast_configuration.cpp dialogs/podcast_configuration.hpp \
	dialogs/extensions.cpp dialogs/extensions.hpp \
	dialogs/fingerprintdialog.cpp dialogs/fingerprintdialog.hpp \
	components/extended_panels.cpp components/extended_panels.hpp \
	components/info_panels.cpp components/info_panels.hpp \
	components/info_widgets.cpp components/info_widgets.hpp \
	components/preferences_widgets.cpp components/preferences_widgets.hpp \
	components/complete_preferences.cpp \
	components/complete_preferences.hpp \
	components/simple_preferences.cpp components/simple_preferences.hpp \
	components/open_panels.cpp components/open_panels.hpp \
	components/interface_widgets.cpp components/interface_widgets.hpp \
	components/controller.cpp components/controller.hpp \
	components/controller_widget.cpp components/controller_widget.hpp \
	components/epg/EPGChannels.cpp components/epg/EPGChannels.hpp \
	components/epg/EPGItem.cpp components/epg/EPGItem.hpp \
	components/epg/EPGRuler.cpp components/epg/EPGRuler.hpp \
	components/epg/EPGView.cpp components/epg/EPGView.hpp \
	components/epg/EPGWidget.cpp components/epg/EPGWidget.hpp \
	components/playlist/views.cpp components/playlist/views.hpp \
	components/playlist/vlc_model.cpp components/playlist/vlc_model.hpp \
	components/playlist/playlist_model.cpp \
	components/playlist/playlist_model.hpp \
	components/playlist/playlist_item.cpp \
	components/playlist/playlist_item.hpp \
	components/playlist/standardpanel.cpp \
	components/playlist/standardpanel.hpp \
	components/playlist/playlist.cpp components/playlist/playlist.hpp \
	components/playlist/selector.cpp components/playlist/selector.hpp \
	components/playlist/sorting.h \
	components/sout/profile_selector.cpp \
	components/sout/profile_selector.hpp \
	components/sout/sout_widgets.cpp components/sout/sout_widgets.hpp \
	components/sout/profiles.hpp \
	util/animators.cpp util/animators.hpp \
	util/input_slider.cpp util/input_slider.hpp \
	util/timetooltip.cpp util/timetooltip.hpp \
	util/customwidgets.cpp util/customwidgets.hpp \
	util/searchlineedit.cpp util/searchlineedit.hpp \
	util/registry.cpp util/registry.hpp \
	util/qmenuview.cpp util/qmenuview.hpp \
	util/qt_dirs.cpp util/qt_dirs.hpp \
	util/pictureflow.cpp util/pictureflow.hpp \
	util/validators.cpp util/validators.hpp \
	util/buttons/BrowseButton.cpp util/buttons/BrowseButton.hpp \
	util/buttons/DeckButtonsLayout.cpp util/buttons/DeckButtonsLayout.hpp \
	util/buttons/RoundButton.cpp util/buttons/RoundButton.hpp \
	util/qvlcframe.hpp \
	util/qvlcapp.hpp \
	util/singleton.hpp \
	styles/seekstyle.cpp styles/seekstyle.hpp
if HAVE_WIN32
libqt4_plugin_la_SOURCES += main_interface_win32.cpp
endif

# Meta-object compilation

moc_verbose = $(moc_verbose_$(V))
moc_verbose_ = $(moc_verbose__$(AM_DEFAULT_VERBOSITY))
moc_verbose_0 = @echo "  MOC   " $@;
moc_verbose__0 = $(moc_verbose_0)

.hpp.moc.cpp:
	$(moc_verbose)$(MOC) $(MOC_CPPFLAGS) -o $@ $<

# These MOC files depend on the configure settings:
dialogs_provider.moc.cpp dialogs/vlm.moc.cpp: $(top_builddir)/config.h

nodist_libqt4_plugin_la_SOURCES = \
	main_interface.moc.cpp \
	menus.moc.cpp \
	dialogs_provider.moc.cpp \
	input_manager.moc.cpp \
	actions_manager.moc.cpp \
	extensions_manager.moc.cpp \
	managers/addons_manager.moc.cpp \
	recents.moc.cpp \
	adapters/seekpoints.moc.cpp \
	adapters/chromaprint.moc.cpp \
	adapters/variables.moc.cpp \
	dialogs/playlist.moc.cpp \
	dialogs/bookmarks.moc.cpp \
	dialogs/mediainfo.moc.cpp \
	dialogs/extended.moc.cpp \
	dialogs/messages.moc.cpp \
	dialogs/epg.moc.cpp \
	dialogs/errors.moc.cpp \
	dialogs/external.moc.cpp \
	dialogs/plugins.moc.cpp \
	dialogs/preferences.moc.cpp \
	dialogs/sout.moc.cpp \
	dialogs/convert.moc.cpp \
	dialogs/help.moc.cpp \
	dialogs/gototime.moc.cpp \
	dialogs/toolbar.moc.cpp \
	dialogs/open.moc.cpp \
	dialogs/openurl.moc.cpp \
	dialogs/podcast_configuration.moc.cpp \
	dialogs/vlm.moc.cpp \
	dialogs/firstrun.moc.cpp \
	dialogs/extensions.moc.cpp \
	dialogs/fingerprintdialog.moc.cpp \
	components/extended_panels.moc.cpp \
	components/info_panels.moc.cpp \
	components/info_widgets.moc.cpp \
	components/preferences_widgets.moc.cpp \
	components/complete_preferences.moc.cpp \
	components/simple_preferences.moc.cpp \
	components/open_panels.moc.cpp \
	components/interface_widgets.moc.cpp \
	components/controller.moc.cpp \
	components/controller_widget.moc.cpp \
	components/epg/EPGChannels.moc.cpp \
	components/epg/EPGRuler.moc.cpp \
	components/epg/EPGView.moc.cpp \
	components/epg/EPGWidget.moc.cpp \
	components/playlist/views.moc.cpp \
	components/playlist/vlc_model.moc.cpp \
	components/playlist/playlist_model.moc.cpp \
	components/playlist/playlist.moc.cpp \
	components/playlist/standardpanel.moc.cpp \
	components/playlist/selector.moc.cpp \
	components/sout/profile_selector.moc.cpp \
	components/sout/sout_widgets.moc.cpp \
	util/animators.moc.cpp \
	util/input_slider.moc.cpp \
	util/timetooltip.moc.cpp \
	util/customwidgets.moc.cpp \
	util/searchlineedit.moc.cpp \
	util/qmenuview.moc.cpp \
	util/qvlcapp.moc.cpp \
	util/pictureflow.moc.cpp \
	util/validators.moc.cpp \
	util/buttons/RoundButton.moc.cpp \
	util/buttons/DeckButtonsLayout.moc.cpp \
	util/buttons/BrowseButton.moc.cpp \
	styles/seekstyle.moc.cpp

nodist_libqt4_plugin_la_SOURCES += \
	ui/equalizer.h \
	ui/video_effects.h \
	ui/open_file.h \
	ui/open_disk.h \
	ui/open_net.h \
	ui/open_capture.h \
	ui/open.h \
	ui/vlm.h \
	ui/podcast_configuration.h \
	ui/profiles.h \
	ui/sprefs_audio.h \
	ui/sprefs_input.h \
	ui/sprefs_interface.h \
	ui/sprefs_subtitles.h \
	ui/sprefs_video.h \
	ui/streampanel.h \
	ui/messages_panel.h \
	ui/about.h \
	ui/update.h \
	ui/fingerprintdialog.h \
	ui/sout.h

# User interface compilation

uic_verbose = $(uic_verbose_$(V))
uic_verbose_ = $(uic_verbose__$(AM_DEFAULT_VERBOSITY))
uic_verbose_0 = @echo "  UIC   " $@;
uic_verbose__0 = $(uic_verbose_0)

MOC_CPPFLAGS = $(DEFS) \
	-I$(top_srcdir)/include -I$(top_builddir)/include -I$(top_builddir) \
	$(libqt4_plugin_la_CPPFLAGS)
if HAVE_DARWIN
MOC_CPPFLAGS += -DQ_OS_MAC
endif
if HAVE_WIN32
MOC_CPPFLAGS += -D_WIN32
endif

libqt4_plugin_la_UI = \
	ui/equalizer.ui \
	ui/video_effects.ui \
	ui/open_file.ui \
	ui/open_disk.ui \
	ui/open_net.ui \
	ui/open_capture.ui \
	ui/open.ui \
	ui/podcast_configuration.ui \
	ui/profiles.ui \
	ui/sprefs_audio.ui \
	ui/sprefs_input.ui \
	ui/sprefs_interface.ui \
	ui/sprefs_subtitles.ui \
	ui/sprefs_video.ui \
	ui/streampanel.ui \
	ui/messages_panel.ui \
	ui/about.ui \
	ui/update.ui \
	ui/sout.ui \
	ui/vlm.ui \
	ui/fingerprintdialog.ui
EXTRA_DIST = $(libqt4_plugin_la_UI)

.ui.h:
	$(AM_V_at)mkdir -p -- ui
	$(AM_V_at)rm -f $@ $@.tmp
	$(AM_V_at)echo "#define Q_(a,b) QString::fromUtf8(_(a))" > $@.tmp
	$(uic_verbose)$(UIC) -tr "Q_" $< >> $@.tmp
	$(AM_V_at)sed -e 's/Q_(\"_(\\\"\(.*\)\\\")"/Q_("\1"/' $@.tmp >$@
	$(AM_V_at)rm -f $@.tmp

# Resources

DEPS_res = \
	pixmaps/arrow_down_dark.png \
	pixmaps/clear.png \
	pixmaps/eject.png \
	pixmaps/faster.png \
	pixmaps/faster2.png \
	pixmaps/go-next.png \
	pixmaps/addons/addon.png \
	pixmaps/addons/addon_broken.png \
	pixmaps/addons/score.png \
	pixmaps/menus/exit_16px.png \
	pixmaps/menus/help_16px.png \
	pixmaps/menus/info_16px.png \
	pixmaps/menus/messages_16px.png \
	pixmaps/menus/playlist_16px.png \
	pixmaps/menus/preferences_16px.png \
	pixmaps/menus/quit_16px.png \
	pixmaps/menus/settings_16px.png \
	pixmaps/menus/stream_16px.png \
	pixmaps/next.png \
	pixmaps/next_16px.png \
	pixmaps/noart-64.png \
	pixmaps/noart.png \
	pixmaps/pause.png \
	pixmaps/pause_16px.png \
	pixmaps/play.png \
	pixmaps/play_16px.png \
	pixmaps/playlist/add.png \
	pixmaps/playlist/jumpto.png \
	pixmaps/playlist/playlist.png \
	pixmaps/playlist/remove.png \
	pixmaps/playlist/repeat_all.png \
	pixmaps/playlist/repeat_off.png \
	pixmaps/playlist/repeat_one.png \
	pixmaps/playlist/shuffle_off.png \
	pixmaps/playlist/shuffle_on.png \
	pixmaps/playlist/dropzone.png \
	pixmaps/playlist/sidebar-icons/capture.png \
	pixmaps/playlist/sidebar-icons/pictures.png \
	pixmaps/playlist/sidebar-icons/disc.png \
	pixmaps/playlist/sidebar-icons/playlist.png \
	pixmaps/playlist/sidebar-icons/lan.png \
	pixmaps/playlist/sidebar-icons/podcast.png \
	pixmaps/playlist/sidebar-icons/library.png \
	pixmaps/playlist/sidebar-icons/screen.png \
	pixmaps/playlist/sidebar-icons/movie.png \
	pixmaps/playlist/sidebar-icons/mtp.png \
	pixmaps/playlist/sidebar-icons/music.png \
	pixmaps/playlist/sidebar-icons/network.png \
	pixmaps/playlist/sidebar-icons/sd/appletrailers.png \
	pixmaps/playlist/sidebar-icons/sd/icecast.png \
	pixmaps/playlist/sidebar-icons/sd/metachannels.png \
	pixmaps/playlist/sidebar-icons/sd/katsomo.png \
	pixmaps/playlist/sidebar-icons/sd/fmc.png \
	pixmaps/playlist/sidebar-icons/sd/frenchtv.png \
	pixmaps/playlist/sidebar-icons/sd/jamendo.png \
	pixmaps/playlist/sidebar-icons/sd/assembleenationale.png \
	pixmaps/prefs/advprefs_audio.png \
	pixmaps/prefs/advprefs_codec.png \
	pixmaps/prefs/advprefs_extended.png \
	pixmaps/prefs/advprefs_intf.png \
	pixmaps/prefs/advprefs_playlist.png \
	pixmaps/prefs/advprefs_sout.png \
	pixmaps/prefs/advprefs_video.png \
	pixmaps/prefs/spref_cone_Audio_64.png \
	pixmaps/prefs/spref_cone_Hotkeys_64.png \
	pixmaps/prefs/spref_cone_Input_64.png \
	pixmaps/prefs/spref_cone_Interface_64.png \
	pixmaps/prefs/spref_cone_Subtitles_64.png \
	pixmaps/prefs/spref_cone_Video_64.png \
	pixmaps/previous.png \
	pixmaps/previous_16px.png \
	pixmaps/profile_new.png \
	pixmaps/sample_complete.png \
	pixmaps/sample_minimal.png \
	pixmaps/sample_skins.png \
	pixmaps/slower.png \
	pixmaps/slower2.png \
	pixmaps/space.png \
	pixmaps/stop.png \
	pixmaps/stop_16px.png \
	pixmaps/toolbar/arrows.png \
	pixmaps/toolbar/aspect-ratio.png \
	pixmaps/toolbar/atob.png \
	pixmaps/toolbar/atob_noa.png \
	pixmaps/toolbar/atob_nob.png \
	pixmaps/toolbar/defullscreen.png \
	pixmaps/toolbar/dvd_menu.png \
	pixmaps/toolbar/dvd_next.png \
	pixmaps/toolbar/dvd_prev.png \
	pixmaps/toolbar/extended_16px.png \
	pixmaps/toolbar/frame-by-frame.png \
	pixmaps/toolbar/fullscreen.png \
	pixmaps/toolbar/play_reverse.png \
	pixmaps/toolbar/record_16px.png \
	pixmaps/toolbar/snapshot.png \
	pixmaps/toolbar/skip_for.png \
	pixmaps/toolbar/skip_back.png \
	pixmaps/toolbar/tv.png \
	pixmaps/toolbar/tvtelx.png \
	pixmaps/toolbar/visu.png \
	pixmaps/toolbar/volume-high.png \
	pixmaps/toolbar/volume-low.png \
	pixmaps/toolbar/volume-medium.png \
	pixmaps/toolbar/volume-muted.png \
	pixmaps/toolbar/volume-slider-inside.png \
	pixmaps/toolbar/volume-slider-outside.png \
	pixmaps/types/capture-card_16px.png \
	pixmaps/types/cdda_16px.png \
	pixmaps/types/disc_16px.png \
	pixmaps/types/file-asym_16px.png \
	pixmaps/types/file-wide_16px.png \
	pixmaps/types/folder-blue_16px.png \
	pixmaps/types/folder-grey_16px.png \
	pixmaps/types/harddisk_16px.png \
	pixmaps/types/network_16px.png \
	pixmaps/types/tape_16px.png \
	pixmaps/types/type_directory.png \
	pixmaps/types/type_file.png \
	pixmaps/types/type_net.png \
	pixmaps/types/type_node.png \
	pixmaps/types/type_playlist.png \
	pixmaps/types/type_unknown.xpm \
	pixmaps/util/wait0.png \
	pixmaps/util/wait1.png \
	pixmaps/util/wait2.png \
	pixmaps/util/wait3.png \
	pixmaps/util/wait4.png \
	pixmaps/win7/win7thumbnail_pause.png \
	pixmaps/win7/win7thumbnail_prev.png \
	pixmaps/win7/win7thumbnail_next.png \
	pixmaps/win7/win7thumbnail_play.png \
	pixmaps/update.png \
	pixmaps/valid.png \
	pixmaps/search_clear.png \
	pixmaps/lock.png
EXTRA_DIST += vlc.qrc $(DEPS_res)

nodist_libqt4_plugin_la_SOURCES += resources.cpp

resources.cpp: vlc.qrc $(DEPS_res)
	$(AM_V_GEN)$(RCC) -name vlc -o $@ $<


BUILT_SOURCES += $(nodist_libqt4_plugin_la_SOURCES)
