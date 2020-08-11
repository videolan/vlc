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
# For each qml and js ressource
#   - Add it to ..._QML

SUFFIXES += .ui .h .hpp .moc.cpp .qml

libqt_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(srcdir)/gui/qt -I$(builddir)/gui/qt/ \
	-I$(builddir)/gui/qt/dialogs/extended \
	-I$(builddir)/gui/qt/dialogs/fingerprint \
	-I$(builddir)/gui/qt/dialogs/help \
	-I$(builddir)/gui/qt/dialogs/messages \
	-I$(builddir)/gui/qt/dialogs/open \
	-I$(builddir)/gui/qt/dialogs/podcast \
	-I$(builddir)/gui/qt/dialogs/preferences \
	-I$(builddir)/gui/qt/dialogs/sout \
	-I$(builddir)/gui/qt/dialogs/vlm

libqt_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) $(CXXFLAGS_qt)
libqt_plugin_la_LIBADD = $(QT_LIBS) $(LIBS_qt) $(LIBM)
libqt_plugin_la_LDFLAGS = $(AM_LDFLAGS) -rpath '$(guidir)'
if HAVE_DARWIN
libqt_plugin_la_LDFLAGS += -Wl,-framework,Cocoa
endif
if HAVE_QT5_X11
libqt_plugin_la_CXXFLAGS += $(QT5_X11_CFLAGS) -DQT5_HAS_X11
libqt_plugin_la_LIBADD += $(QT5_X11_LIBS) $(X_LIBS) $(X_PRE_LIB) -lX11
endif
if HAVE_XCB
libqt_plugin_la_CXXFLAGS += -DQT5_HAS_XCB
endif
if HAVE_WAYLAND
libqt_plugin_la_CPPFLAGS += -DQT5_HAS_WAYLAND \
	-DQPNI_HEADER=\<$(QT_VERSION)/QtGui/qpa/qplatformnativeinterface.h\>
libqt_plugin_la_CXXFLAGS += $(WAYLAND_CLIENT_CFLAGS)
libqt_plugin_la_LIBADD += $(WAYLAND_CLIENT_LIBS)
endif
if HAVE_WIN32
libqt_plugin_la_LIBADD += $(LIBCOM) -lcomctl32 -luuid -ld3d11
endif
if UPDATE_CHECK
libqt_plugin_la_CPPFLAGS += -DUPDATE_CHECK
endif
if QT_QML_DEBUG
libqt_plugin_la_CPPFLAGS += -DQT_QML_DEBUG
endif

libqt_plugin_la_SOURCES = \
	gui/qt/qt.cpp gui/qt/qt.hpp \
	gui/qt/dialogs/bookmarks/bookmarks.cpp gui/qt/dialogs/bookmarks/bookmarks.hpp \
	gui/qt/dialogs/dialogs/dialogmodel.cpp \
	gui/qt/dialogs/dialogs/dialogmodel.hpp \
	gui/qt/dialogs/dialogs_provider.cpp gui/qt/dialogs/dialogs_provider.hpp \
	gui/qt/dialogs/epg/EPGChannels.cpp \
	gui/qt/dialogs/epg/EPGChannels.hpp \
	gui/qt/dialogs/epg/EPGItem.cpp \
	gui/qt/dialogs/epg/EPGItem.hpp \
	gui/qt/dialogs/epg/EPGProgram.cpp \
	gui/qt/dialogs/epg/EPGProgram.hpp \
	gui/qt/dialogs/epg/EPGRuler.cpp gui/qt/dialogs/epg/EPGRuler.hpp \
	gui/qt/dialogs/epg/EPGView.cpp gui/qt/dialogs/epg/EPGView.hpp \
	gui/qt/dialogs/epg/EPGWidget.cpp \
	gui/qt/dialogs/epg/EPGWidget.hpp \
	gui/qt/dialogs/epg/epg.cpp gui/qt/dialogs/epg/epg.hpp \
	gui/qt/dialogs/errors/errors.cpp gui/qt/dialogs/errors/errors.hpp \
	gui/qt/dialogs/extended/extended.cpp gui/qt/dialogs/extended/extended.hpp \
	gui/qt/dialogs/extended/extended_panels.cpp \
	gui/qt/dialogs/extended/extended_panels.hpp \
	gui/qt/dialogs/extensions/extensions.cpp gui/qt/dialogs/extensions/extensions.hpp \
	gui/qt/dialogs/extensions/extensions_manager.cpp gui/qt/dialogs/extensions/extensions_manager.hpp \
	gui/qt/dialogs/fingerprint/chromaprint.cpp \
	gui/qt/dialogs/fingerprint/chromaprint.hpp \
	gui/qt/dialogs/fingerprint/fingerprintdialog.cpp \
	gui/qt/dialogs/fingerprint/fingerprintdialog.hpp \
	gui/qt/dialogs/firstrun/firstrun.cpp gui/qt/dialogs/firstrun/firstrun.hpp \
	gui/qt/dialogs/gototime/gototime.cpp gui/qt/dialogs/gototime/gototime.hpp \
	gui/qt/dialogs/help/aboutmodel.cpp \
	gui/qt/dialogs/help/aboutmodel.hpp \
	gui/qt/dialogs/help/help.cpp gui/qt/dialogs/help/help.hpp \
	gui/qt/dialogs/mediainfo/info_panels.cpp gui/qt/dialogs/mediainfo/info_panels.hpp \
	gui/qt/dialogs/mediainfo/info_widgets.cpp gui/qt/dialogs/mediainfo/info_widgets.hpp \
	gui/qt/dialogs/mediainfo/mediainfo.cpp gui/qt/dialogs/mediainfo/mediainfo.hpp \
	gui/qt/dialogs/messages/messages.cpp gui/qt/dialogs/messages/messages.hpp \
	gui/qt/dialogs/open/open.cpp gui/qt/dialogs/open/open.hpp \
	gui/qt/dialogs/open/open_panels.cpp gui/qt/dialogs/open/open_panels.hpp \
	gui/qt/dialogs/open/openurl.cpp gui/qt/dialogs/open/openurl.hpp \
	gui/qt/dialogs/plugins/addons_manager.cpp gui/qt/dialogs/plugins/addons_manager.hpp \
	gui/qt/dialogs/plugins/plugins.cpp gui/qt/dialogs/plugins/plugins.hpp \
	gui/qt/dialogs/podcast/podcast_configuration.cpp \
	gui/qt/dialogs/podcast/podcast_configuration.hpp \
	gui/qt/dialogs/preferences/complete_preferences.cpp \
	gui/qt/dialogs/preferences/complete_preferences.hpp \
	gui/qt/dialogs/preferences/preferences.cpp \
	gui/qt/dialogs/preferences/preferences.hpp \
	gui/qt/dialogs/preferences/preferences_widgets.cpp \
	gui/qt/dialogs/preferences/preferences_widgets.hpp \
	gui/qt/dialogs/preferences/simple_preferences.cpp \
	gui/qt/dialogs/preferences/simple_preferences.hpp \
	gui/qt/dialogs/sout/convert.cpp gui/qt/dialogs/sout/convert.hpp \
	gui/qt/dialogs/sout/profile_selector.cpp \
	gui/qt/dialogs/sout/profile_selector.hpp \
	gui/qt/dialogs/sout/profiles.hpp \
	gui/qt/dialogs/sout/sout.cpp gui/qt/dialogs/sout/sout.hpp \
	gui/qt/dialogs/sout/sout_widgets.cpp \
	gui/qt/dialogs/sout/sout_widgets.hpp \
	gui/qt/dialogs/toolbar/toolbareditor.cpp gui/qt/dialogs/toolbar/toolbareditor.hpp \
	gui/qt/dialogs/vlm/vlm.cpp gui/qt/dialogs/vlm/vlm.hpp \
	gui/qt/maininterface/compositor.hpp \
	gui/qt/maininterface/compositor.cpp \
	gui/qt/maininterface/compositor_dummy.hpp \
	gui/qt/maininterface/compositor_dummy.cpp \
	gui/qt/maininterface/main_interface.cpp \
	gui/qt/maininterface/main_interface.hpp \
	gui/qt/maininterface/mainui.cpp \
	gui/qt/maininterface/mainui.hpp \
	gui/qt/maininterface/videosurface.cpp \
	gui/qt/maininterface/videosurface.hpp \
	gui/qt/medialibrary/medialib.cpp \
	gui/qt/medialibrary/medialib.hpp \
	gui/qt/medialibrary/mlalbum.cpp \
	gui/qt/medialibrary/mlalbum.hpp \
	gui/qt/medialibrary/mlalbummodel.cpp \
	gui/qt/medialibrary/mlalbummodel.hpp \
	gui/qt/medialibrary/mlalbumtrack.cpp \
	gui/qt/medialibrary/mlalbumtrack.hpp \
	gui/qt/medialibrary/mlalbumtrackmodel.cpp \
	gui/qt/medialibrary/mlalbumtrackmodel.hpp \
	gui/qt/medialibrary/mlartist.cpp \
	gui/qt/medialibrary/mlartist.hpp \
	gui/qt/medialibrary/mlartistmodel.cpp \
	gui/qt/medialibrary/mlartistmodel.hpp \
	gui/qt/medialibrary/mlbasemodel.cpp \
	gui/qt/medialibrary/mlbasemodel.hpp \
	gui/qt/medialibrary/mlbookmarkmodel.cpp \
	gui/qt/medialibrary/mlbookmarkmodel.hpp \
	gui/qt/medialibrary/mlfoldersmodel.cpp \
	gui/qt/medialibrary/mlfoldersmodel.hpp \
	gui/qt/medialibrary/mlgenre.cpp \
	gui/qt/medialibrary/mlgenre.hpp \
	gui/qt/medialibrary/mlgenremodel.cpp \
	gui/qt/medialibrary/mlgenremodel.hpp \
	gui/qt/medialibrary/mlhelper.cpp \
	gui/qt/medialibrary/mlhelper.hpp \
	gui/qt/medialibrary/mlqmltypes.hpp \
	gui/qt/medialibrary/mlrecentsvideomodel.cpp \
	gui/qt/medialibrary/mlrecentsvideomodel.hpp \
	gui/qt/medialibrary/mlvideo.cpp \
	gui/qt/medialibrary/mlvideo.hpp \
	gui/qt/medialibrary/mlvideomodel.cpp \
	gui/qt/medialibrary/mlvideomodel.hpp \
	gui/qt/menus/custom_menus.cpp \
	gui/qt/menus/custom_menus.hpp \
	gui/qt/menus/menus.cpp gui/qt/menus/menus.hpp \
	gui/qt/network/networkdevicemodel.cpp \
	gui/qt/network/networkdevicemodel.hpp \
	gui/qt/network/networkmediamodel.cpp \
	gui/qt/network/networkmediamodel.hpp \
	gui/qt/network/networksourcelistener.cpp \
	gui/qt/network/networksourcelistener.hpp \
	gui/qt/player/input_models.cpp gui/qt/player/input_models.hpp \
	gui/qt/player/player_controller.cpp gui/qt/player/player_controller.hpp gui/qt/player/player_controller_p.hpp \
	gui/qt/player/playercontrolbarmodel.cpp gui/qt/player/playercontrolbarmodel.hpp \
	gui/qt/playlist/media.hpp \
	gui/qt/playlist/playlist_common.cpp \
	gui/qt/playlist/playlist_common.hpp \
	gui/qt/playlist/playlist_controller.cpp \
	gui/qt/playlist/playlist_controller.hpp \
	gui/qt/playlist/playlist_controller_p.hpp \
	gui/qt/playlist/playlist_item.cpp \
	gui/qt/playlist/playlist_item.hpp \
	gui/qt/playlist/playlist_model.cpp \
	gui/qt/playlist/playlist_model.hpp \
	gui/qt/playlist/playlist_model_p.hpp \
	gui/qt/util/audio_device_model.cpp  \
	gui/qt/util/audio_device_model.hpp \
	gui/qt/util/imagehelper.cpp gui/qt/util/imagehelper.hpp \
	gui/qt/util/i18n.cpp gui/qt/util/i18n.hpp \
	gui/qt/util/navigation_history.cpp gui/qt/util/navigation_history.hpp \
	gui/qt/util/qml_main_context.cpp \
	gui/qt/util/qml_main_context.hpp \
	gui/qt/util/qmleventfilter.cpp \
	gui/qt/util/qmleventfilter.hpp \
	gui/qt/util/qt_dirs.cpp gui/qt/util/qt_dirs.hpp \
	gui/qt/util/qvlcapp.hpp \
	gui/qt/util/recent_media_model.cpp  \
	gui/qt/util/recent_media_model.hpp \
	gui/qt/util/recents.cpp gui/qt/util/recents.hpp \
	gui/qt/util/registry.cpp gui/qt/util/registry.hpp \
	gui/qt/util/renderer_manager.cpp gui/qt/util/renderer_manager.hpp \
	gui/qt/util/selectable_list_model.cpp \
	gui/qt/util/selectable_list_model.hpp \
	gui/qt/util/settings.cpp \
	gui/qt/util/settings.hpp \
	gui/qt/util/singleton.hpp \
	gui/qt/util/soutchain.cpp gui/qt/util/soutchain.hpp \
	gui/qt/util/systempalette.cpp gui/qt/util/systempalette.hpp \
	gui/qt/util/validators.cpp gui/qt/util/validators.hpp \
	gui/qt/util/varcommon_p.hpp \
	gui/qt/util/varchoicemodel.cpp  gui/qt/util/varchoicemodel.hpp \
	gui/qt/util/variables.cpp gui/qt/util/variables.hpp \
	gui/qt/util/vlctick.cpp \
	gui/qt/util/vlctick.hpp \
	gui/qt/widgets/native/animators.cpp \
	gui/qt/widgets/native/animators.hpp \
	gui/qt/widgets/native/customwidgets.cpp gui/qt/widgets/native/customwidgets.hpp \
	gui/qt/widgets/native/interface_widgets.cpp \
	gui/qt/widgets/native/interface_widgets.hpp \
	gui/qt/widgets/native/qvlcframe.cpp \
	gui/qt/widgets/native/qvlcframe.hpp \
	gui/qt/widgets/native/searchlineedit.cpp gui/qt/widgets/native/searchlineedit.hpp
if HAVE_WIN32
libqt_plugin_la_SOURCES += \
	gui/qt/maininterface/main_interface_win32.cpp \
	gui/qt/maininterface/main_interface_win32.hpp

if HAVE_DCOMP
libqt_plugin_la_SOURCES += \
	gui/qt/maininterface/compositor_dcomp.cpp \
	gui/qt/maininterface/compositor_dcomp.hpp \
	gui/qt/maininterface/compositor_dcomp_error.hpp \
	gui/qt/maininterface/compositor_dcomp_uisurface.cpp \
	gui/qt/maininterface/compositor_dcomp_uisurface.hpp
endif
endif

# Meta-object compilation

moc_verbose = $(moc_verbose_$(V))
moc_verbose_ = $(moc_verbose__$(AM_DEFAULT_VERBOSITY))
moc_verbose_0 = @echo "  MOC   " $@;
moc_verbose__0 = $(moc_verbose_0)

.hpp.moc.cpp:
	$(moc_verbose)$(MOC) $(MOC_CPPFLAGS) -o $@ $<

# These MOC files depend on the configure settings:
dialogs/dialogs_provider.moc.cpp dialogs/vlm/vlm.moc.cpp: $(top_builddir)/config.h

nodist_libqt_plugin_la_SOURCES = \
	gui/qt/dialogs/bookmarks/bookmarks.moc.cpp \
	gui/qt/dialogs/dialogs/dialogmodel.moc.cpp \
	gui/qt/dialogs/dialogs_provider.moc.cpp \
	gui/qt/dialogs/epg/EPGChannels.moc.cpp \
	gui/qt/dialogs/epg/EPGProgram.moc.cpp \
	gui/qt/dialogs/epg/EPGRuler.moc.cpp \
	gui/qt/dialogs/epg/EPGView.moc.cpp \
	gui/qt/dialogs/epg/EPGWidget.moc.cpp \
	gui/qt/dialogs/epg/epg.moc.cpp \
	gui/qt/dialogs/errors/errors.moc.cpp \
	gui/qt/dialogs/extended/extended.moc.cpp \
	gui/qt/dialogs/extended/extended_panels.moc.cpp \
	gui/qt/dialogs/extensions/extensions.moc.cpp \
	gui/qt/dialogs/extensions/extensions_manager.moc.cpp \
	gui/qt/dialogs/fingerprint/chromaprint.moc.cpp \
	gui/qt/dialogs/fingerprint/fingerprintdialog.moc.cpp \
	gui/qt/dialogs/firstrun/firstrun.moc.cpp \
	gui/qt/dialogs/gototime/gototime.moc.cpp \
	gui/qt/dialogs/help/aboutmodel.moc.cpp \
	gui/qt/dialogs/help/help.moc.cpp \
	gui/qt/dialogs/mediainfo/info_panels.moc.cpp \
	gui/qt/dialogs/mediainfo/info_widgets.moc.cpp \
	gui/qt/dialogs/mediainfo/mediainfo.moc.cpp \
	gui/qt/dialogs/messages/messages.moc.cpp \
	gui/qt/dialogs/open/open.moc.cpp \
	gui/qt/dialogs/open/open_panels.moc.cpp \
	gui/qt/dialogs/open/openurl.moc.cpp \
	gui/qt/dialogs/plugins/addons_manager.moc.cpp \
	gui/qt/dialogs/plugins/plugins.moc.cpp \
	gui/qt/dialogs/podcast/podcast_configuration.moc.cpp \
	gui/qt/dialogs/preferences/complete_preferences.moc.cpp \
	gui/qt/dialogs/preferences/preferences.moc.cpp \
	gui/qt/dialogs/preferences/preferences_widgets.moc.cpp \
	gui/qt/dialogs/preferences/simple_preferences.moc.cpp \
	gui/qt/dialogs/sout/convert.moc.cpp \
	gui/qt/dialogs/sout/profile_selector.moc.cpp \
	gui/qt/dialogs/sout/sout.moc.cpp \
	gui/qt/dialogs/sout/sout_widgets.moc.cpp \
	gui/qt/dialogs/toolbar/toolbareditor.moc.cpp \
	gui/qt/maininterface/compositor_dummy.moc.cpp \
	gui/qt/maininterface/main_interface.moc.cpp \
	gui/qt/maininterface/mainui.moc.cpp \
	gui/qt/maininterface/videosurface.moc.cpp \
	gui/qt/medialibrary/medialib.moc.cpp \
	gui/qt/medialibrary/mlalbum.moc.cpp \
	gui/qt/medialibrary/mlalbummodel.moc.cpp \
	gui/qt/medialibrary/mlalbumtrack.moc.cpp \
	gui/qt/medialibrary/mlalbumtrackmodel.moc.cpp \
	gui/qt/medialibrary/mlartist.moc.cpp \
	gui/qt/medialibrary/mlartistmodel.moc.cpp \
	gui/qt/medialibrary/mlbasemodel.moc.cpp \
	gui/qt/medialibrary/mlfoldersmodel.moc.cpp \
	gui/qt/medialibrary/mlgenre.moc.cpp \
	gui/qt/medialibrary/mlgenremodel.moc.cpp \
	gui/qt/medialibrary/mlqmltypes.moc.cpp \
	gui/qt/medialibrary/mlrecentsvideomodel.moc.cpp \
	gui/qt/medialibrary/mlvideo.moc.cpp \
	gui/qt/medialibrary/mlvideomodel.moc.cpp \
	gui/qt/menus/custom_menus.moc.cpp \
	gui/qt/menus/menus.moc.cpp \
	gui/qt/network/networkdevicemodel.moc.cpp \
	gui/qt/network/networkmediamodel.moc.cpp \
	gui/qt/player/input_models.moc.cpp \
	gui/qt/player/player_controller.moc.cpp \
	gui/qt/player/playercontrolbarmodel.moc.cpp \
	gui/qt/playlist/playlist_common.moc.cpp \
	gui/qt/playlist/playlist_controller.moc.cpp \
	gui/qt/playlist/playlist_item.moc.cpp \
	gui/qt/playlist/playlist_model.moc.cpp \
	gui/qt/util/audio_device_model.moc.cpp \
	gui/qt/util/i18n.moc.cpp \
	gui/qt/util/navigation_history.moc.cpp \
	gui/qt/util/qml_main_context.moc.cpp \
	gui/qt/util/qmleventfilter.moc.cpp \
	gui/qt/util/qvlcapp.moc.cpp \
	gui/qt/util/recent_media_model.moc.cpp \
	gui/qt/util/recents.moc.cpp \
	gui/qt/util/renderer_manager.moc.cpp \
	gui/qt/util/selectable_list_model.moc.cpp \
	gui/qt/util/settings.moc.cpp \
	gui/qt/util/systempalette.moc.cpp \
	gui/qt/util/validators.moc.cpp \
	gui/qt/util/varchoicemodel.moc.cpp \
	gui/qt/util/variables.moc.cpp \
	gui/qt/util/vlctick.moc.cpp \
	gui/qt/widgets/native/animators.moc.cpp \
	gui/qt/widgets/native/customwidgets.moc.cpp \
	gui/qt/widgets/native/interface_widgets.moc.cpp \
	gui/qt/widgets/native/searchlineedit.moc.cpp

if HAVE_WIN32
nodist_libqt_plugin_la_SOURCES += gui/qt/maininterface/main_interface_win32.moc.cpp

if HAVE_DCOMP
nodist_libqt_plugin_la_SOURCES += \
	gui/qt/maininterface/compositor_dcomp.moc.cpp \
	gui/qt/maininterface/compositor_dcomp_uisurface.moc.cpp
endif

endif

nodist_libqt_plugin_la_SOURCES += \
	gui/qt/dialogs/extended/ui_equalizer.h \
	gui/qt/dialogs/extended/ui_video_effects.h \
	gui/qt/dialogs/fingerprint/ui_fingerprintdialog.h \
	gui/qt/dialogs/help/ui_about.h \
	gui/qt/dialogs/help/ui_update.h \
	gui/qt/dialogs/messages/ui_messages_panel.h \
	gui/qt/dialogs/open/ui_open.h \
	gui/qt/dialogs/open/ui_open_capture.h \
	gui/qt/dialogs/open/ui_open_disk.h \
	gui/qt/dialogs/open/ui_open_file.h \
	gui/qt/dialogs/open/ui_open_net.h \
	gui/qt/dialogs/podcast/ui_podcast_configuration.h \
	gui/qt/dialogs/preferences/ui_sprefs_audio.h \
	gui/qt/dialogs/preferences/ui_sprefs_input.h \
	gui/qt/dialogs/preferences/ui_sprefs_interface.h \
	gui/qt/dialogs/preferences/ui_sprefs_subtitles.h \
	gui/qt/dialogs/preferences/ui_sprefs_video.h \
	gui/qt/dialogs/sout/ui_profiles.h \
	gui/qt/dialogs/sout/ui_sout.h \
	gui/qt/dialogs/vlm/ui_vlm.h

# User interface compilation

uic_verbose = $(uic_verbose_$(V))
uic_verbose_ = $(uic_verbose__$(AM_DEFAULT_VERBOSITY))
uic_verbose_0 = @echo "  UIC   " $@;
uic_verbose__0 = $(uic_verbose_0)

MOC_CPPFLAGS = $(DEFS) \
	-I$(top_srcdir)/include -I$(top_builddir)/include -I$(top_builddir) \
	$(libqt_plugin_la_CPPFLAGS)
if HAVE_DARWIN
MOC_CPPFLAGS += -DQ_OS_MAC
endif
if HAVE_WIN32
MOC_CPPFLAGS += -D_WIN32
endif

libqt_plugin_la_UI = \
	gui/qt/dialogs/extended/equalizer.ui \
	gui/qt/dialogs/extended/video_effects.ui \
	gui/qt/dialogs/fingerprint/fingerprintdialog.ui \
	gui/qt/dialogs/help/about.ui \
	gui/qt/dialogs/help/update.ui \
	gui/qt/dialogs/messages/messages_panel.ui \
	gui/qt/dialogs/open/open.ui \
	gui/qt/dialogs/open/open_capture.ui \
	gui/qt/dialogs/open/open_disk.ui \
	gui/qt/dialogs/open/open_file.ui \
	gui/qt/dialogs/open/open_net.ui \
	gui/qt/dialogs/podcast/podcast_configuration.ui \
	gui/qt/dialogs/preferences/sprefs_audio.ui \
	gui/qt/dialogs/preferences/sprefs_input.ui \
	gui/qt/dialogs/preferences/sprefs_interface.ui \
	gui/qt/dialogs/preferences/sprefs_subtitles.ui \
	gui/qt/dialogs/preferences/sprefs_video.ui \
	gui/qt/dialogs/sout/profiles.ui \
	gui/qt/dialogs/sout/sout.ui \
	gui/qt/dialogs/vlm/vlm.ui
EXTRA_DIST += $(libqt_plugin_la_UI)

ui_%.h: %.ui
	$(AM_V_at)$(MKDIR_P) "gui/qt/ui"
	$(AM_V_at)rm -f $@ $@.tmp
	$(AM_V_at)echo "#define Q_(a,b) QString::fromUtf8(_(a))" > $@.tmp
	$(uic_verbose)$(UIC) -tr "Q_" $< >> $@.tmp
	$(AM_V_at)sed -e 's/Q_(\"_(\\\"\(.*\)\\\")"/Q_("\1"/' $@.tmp >$@
	$(AM_V_at)rm -f $@.tmp

# Resources

libqt_plugin_la_RES = \
	gui/qt/pixmaps/VLCIcons.ttf \
	gui/qt/pixmaps/clear.svg \
	gui/qt/pixmaps/eject.svg \
	gui/qt/pixmaps/faster.svg \
	gui/qt/pixmaps/faster2.svg \
	gui/qt/pixmaps/go-next.svg \
	gui/qt/pixmaps/addons/addon.svg \
	gui/qt/pixmaps/addons/addon_broken.svg \
	gui/qt/pixmaps/addons/addon_blue.svg \
	gui/qt/pixmaps/addons/addon_cyan.svg \
	gui/qt/pixmaps/addons/addon_green.svg \
	gui/qt/pixmaps/addons/addon_magenta.svg \
	gui/qt/pixmaps/addons/addon_red.svg \
	gui/qt/pixmaps/addons/addon_yellow.svg \
	gui/qt/pixmaps/addons/score.svg \
	gui/qt/pixmaps/menu.svg \
	gui/qt/pixmaps/menus/exit_16px.svg \
	gui/qt/pixmaps/menus/help_16px.svg \
	gui/qt/pixmaps/menus/info_16px.svg \
	gui/qt/pixmaps/menus/messages_16px.svg \
	gui/qt/pixmaps/menus/playlist_16px.svg \
	gui/qt/pixmaps/menus/preferences_16px.svg \
	gui/qt/pixmaps/menus/settings_16px.svg \
	gui/qt/pixmaps/menus/stream_16px.svg \
	gui/qt/pixmaps/next.svg \
	gui/qt/pixmaps/noart-64.png \
	gui/qt/pixmaps/noart.png \
	gui/qt/pixmaps/noart_album.svg \
	gui/qt/pixmaps/noart_artist.svg \
	gui/qt/pixmaps/noart_artist_small.svg \
	gui/qt/pixmaps/pause.svg \
	gui/qt/pixmaps/play.svg \
	gui/qt/pixmaps/play_button.svg \
	gui/qt/pixmaps/playlist/add.svg \
	gui/qt/pixmaps/playlist/playlist.svg \
	gui/qt/pixmaps/playlist/remove.svg \
	gui/qt/pixmaps/playlist/repeat_all.svg \
	gui/qt/pixmaps/playlist/repeat_off.svg \
	gui/qt/pixmaps/playlist/repeat_one.svg \
	gui/qt/pixmaps/playlist/active_indicator.svg \
	gui/qt/pixmaps/playlist/shuffle_on.svg \
	gui/qt/pixmaps/playlist/dropzone.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/capture.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/pictures.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/disc.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/playlist.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/lan.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/podcast.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/library.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/screen.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/movie.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/mtp.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/music.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/network.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/appletrailers.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/icecast.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/katsomo.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/fmc.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/frenchtv.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/jamendo.svg \
	gui/qt/pixmaps/playlist/sidebar-icons/sd/assembleenationale.svg \
	gui/qt/pixmaps/prefs/advprefs_audio.svg \
	gui/qt/pixmaps/prefs/advprefs_codec.svg \
	gui/qt/pixmaps/prefs/advprefs_extended.svg \
	gui/qt/pixmaps/prefs/advprefs_intf.svg \
	gui/qt/pixmaps/prefs/advprefs_playlist.svg \
	gui/qt/pixmaps/prefs/advprefs_sout.svg \
	gui/qt/pixmaps/prefs/advprefs_video.svg \
	gui/qt/pixmaps/prefs/spref_cone_Audio_64.png \
	gui/qt/pixmaps/prefs/spref_cone_Hotkeys_64.png \
	gui/qt/pixmaps/prefs/spref_cone_Input_64.png \
	gui/qt/pixmaps/prefs/spref_cone_Interface_64.png \
	gui/qt/pixmaps/prefs/spref_cone_Subtitles_64.png \
	gui/qt/pixmaps/prefs/spref_cone_Video_64.png \
	gui/qt/pixmaps/previous.svg \
	gui/qt/pixmaps/profile_new.svg \
	gui/qt/pixmaps/sample_complete.png \
	gui/qt/pixmaps/sample_minimal.png \
	gui/qt/pixmaps/sample_skins.png \
	gui/qt/pixmaps/slower.svg \
	gui/qt/pixmaps/slower2.svg \
	gui/qt/pixmaps/space.svg \
	gui/qt/pixmaps/stop.svg \
	gui/qt/pixmaps/toolbar/audiosub.svg \
	gui/qt/pixmaps/toolbar/arrows.svg \
	gui/qt/pixmaps/toolbar/aspect-ratio.svg \
	gui/qt/pixmaps/toolbar/atob.svg \
	gui/qt/pixmaps/toolbar/atob_noa.svg \
	gui/qt/pixmaps/toolbar/atob_nob.svg \
	gui/qt/pixmaps/toolbar/atob_bg_a.svg \
	gui/qt/pixmaps/toolbar/atob_bg_ab.svg \
	gui/qt/pixmaps/toolbar/atob_bg_b.svg \
	gui/qt/pixmaps/toolbar/atob_bg_none.svg \
	gui/qt/pixmaps/toolbar/atob_fg_a.svg \
	gui/qt/pixmaps/toolbar/atob_fg_ab.svg \
	gui/qt/pixmaps/toolbar/atob_fg_b.svg \
	gui/qt/pixmaps/toolbar/renderer.svg \
	gui/qt/pixmaps/toolbar/defullscreen.svg \
	gui/qt/pixmaps/toolbar/dvd_menu.svg \
	gui/qt/pixmaps/toolbar/dvd_next.svg \
	gui/qt/pixmaps/toolbar/dvd_prev.svg \
	gui/qt/pixmaps/toolbar/extended_16px.svg \
	gui/qt/pixmaps/toolbar/frame-by-frame.svg \
	gui/qt/pixmaps/toolbar/fullscreen.svg \
	gui/qt/pixmaps/toolbar/play_reverse.svg \
	gui/qt/pixmaps/toolbar/record_16px.svg \
	gui/qt/pixmaps/toolbar/snapshot.svg \
	gui/qt/pixmaps/toolbar/skip_for.svg \
	gui/qt/pixmaps/toolbar/skip_back.svg \
	gui/qt/pixmaps/toolbar/tv.svg \
	gui/qt/pixmaps/toolbar/tvtelx.svg \
	gui/qt/pixmaps/toolbar/visu.svg \
	gui/qt/pixmaps/toolbar/volume-high.svg \
	gui/qt/pixmaps/toolbar/volume-low.svg \
	gui/qt/pixmaps/toolbar/volume-medium.svg \
	gui/qt/pixmaps/toolbar/volume-muted.svg \
	gui/qt/pixmaps/toolbar/volume-slider-inside.svg \
	gui/qt/pixmaps/toolbar/volume-slider-outside.svg \
	gui/qt/pixmaps/types/capture-card_16px.svg \
	gui/qt/pixmaps/types/disc_16px.svg \
	gui/qt/pixmaps/types/file-asym_16px.svg \
	gui/qt/pixmaps/types/file-wide_16px.svg \
	gui/qt/pixmaps/types/file_black.svg \
	gui/qt/pixmaps/types/folder-blue_16px.svg \
	gui/qt/pixmaps/types/folder-grey_16px.svg \
	gui/qt/pixmaps/types/harddisk_16px.svg \
	gui/qt/pixmaps/types/network_16px.svg \
	gui/qt/pixmaps/types/tape_16px.svg \
	gui/qt/pixmaps/types/type_directory.svg \
	gui/qt/pixmaps/types/type_directory_black.svg \
	gui/qt/pixmaps/types/type_file.svg \
	gui/qt/pixmaps/types/type_stream.svg \
	gui/qt/pixmaps/types/type_node.svg \
	gui/qt/pixmaps/types/type_playlist.svg \
	gui/qt/pixmaps/types/type_unknown.xpm \
	gui/qt/pixmaps/util/wait1.svg \
	gui/qt/pixmaps/util/wait2.svg \
	gui/qt/pixmaps/util/wait3.svg \
	gui/qt/pixmaps/util/wait4.svg \
	gui/qt/pixmaps/win7/win7thumbnail_pause.svg \
	gui/qt/pixmaps/win7/win7thumbnail_prev.svg \
	gui/qt/pixmaps/win7/win7thumbnail_next.svg \
	gui/qt/pixmaps/win7/win7thumbnail_play.svg \
	gui/qt/pixmaps/update.svg \
	gui/qt/pixmaps/valid.svg \
	gui/qt/pixmaps/search_clear.svg \
	gui/qt/pixmaps/lock.svg \
	gui/qt/pixmaps/close.svg \
	gui/qt/pixmaps/rename.svg \
	gui/qt/pixmaps/del.svg \
	gui/qt/pixmaps/ellipsis.svg \
	gui/qt/pixmaps/topbar/grid.svg \
	gui/qt/pixmaps/topbar/list.svg \
	gui/qt/pixmaps/cone.svg \
	gui/qt/pixmaps/album_cover.svg \
	gui/qt/pixmaps/time.svg

#QML and JS resources
libqt_plugin_la_QML = \
	gui/qt/dialogs/dialogs/qml/CustomDialog.qml \
	gui/qt/dialogs/dialogs/qml/Dialogs.qml \
	gui/qt/dialogs/dialogs/qml/ModalDialog.qml \
	gui/qt/dialogs/help/qml/About.qml \
	gui/qt/dialogs/toolbar/qml/EditorDNDDelegate.qml \
	gui/qt/dialogs/toolbar/qml/EditorDNDView.qml\
	gui/qt/dialogs/toolbar/qml/EditorDummyButton.qml \
	gui/qt/dialogs/toolbar/qml/EditorTabButton.qml \
	gui/qt/dialogs/toolbar/qml/ToolbarEditor.qml \
	gui/qt/dialogs/toolbar/qml/ToolbarEditorButtonList.qml \
	gui/qt/maininterface/qml/BannerSources.qml \
	gui/qt/maininterface/qml/MainInterface.qml \
	gui/qt/medialibrary/qml/ArtistTopBanner.qml \
	gui/qt/medialibrary/qml/AudioGridItem.qml \
	gui/qt/medialibrary/qml/EmptyLabel.qml \
	gui/qt/medialibrary/qml/MainDisplay.qml \
	gui/qt/medialibrary/qml/MusicAlbums.qml \
	gui/qt/medialibrary/qml/MusicAlbumsDisplay.qml \
	gui/qt/medialibrary/qml/MusicAlbumsGridExpandDelegate.qml \
	gui/qt/medialibrary/qml/MusicArtistsDisplay.qml \
	gui/qt/medialibrary/qml/MusicDisplay.qml \
	gui/qt/medialibrary/qml/MusicGenres.qml \
	gui/qt/medialibrary/qml/MusicGenresDisplay.qml \
	gui/qt/medialibrary/qml/MusicTrackListDisplay.qml \
	gui/qt/medialibrary/qml/MusicTracksDisplay.qml \
	gui/qt/medialibrary/qml/VideoDisplay.qml \
	gui/qt/medialibrary/qml/VideoGridItem.qml \
	gui/qt/medialibrary/qml/VideoInfoExpandPanel.qml \
	gui/qt/medialibrary/qml/VideoListDisplay.qml \
	gui/qt/menus/qml/AudioMenu.qml \
	gui/qt/menus/qml/CheckableModelSubMenu.qml \
	gui/qt/menus/qml/HelpMenu.qml \
	gui/qt/menus/qml/MainDropdownMenu.qml \
	gui/qt/menus/qml/MainMenubar.qml \
	gui/qt/menus/qml/MediaMenu.qml \
	gui/qt/menus/qml/PlaybackMenu.qml \
	gui/qt/menus/qml/PopupMenu.qml \
	gui/qt/menus/qml/SubtitleMenu.qml \
	gui/qt/menus/qml/ToolsMenu.qml \
	gui/qt/menus/qml/VideoMenu.qml \
	gui/qt/menus/qml/ViewMenu.qml \
	gui/qt/network/qml/NetworkBrowseDisplay.qml \
	gui/qt/network/qml/NetworkDisplay.qml \
	gui/qt/network/qml/NetworkGridItem.qml \
	gui/qt/network/qml/NetworkHomeDisplay.qml \
	gui/qt/network/qml/NetworkHomeDeviceListView.qml \
	gui/qt/network/qml/NetworkListItem.qml \
	gui/qt/player/qml/ControlBar.qml \
	gui/qt/player/qml/ControlButtons.qml \
	gui/qt/player/qml/MiniPlayer.qml \
	gui/qt/player/qml/Player.qml \
	gui/qt/player/qml/PlayerButtonsLayout.qml \
	gui/qt/player/qml/PlayerMenu.qml \
	gui/qt/player/qml/PlayerMenuItem.qml \
	gui/qt/player/qml/ResumeDialog.qml \
	gui/qt/player/qml/SliderBar.qml \
	gui/qt/player/qml/TeletextWidget.qml \
	gui/qt/player/qml/TopBar.qml \
	gui/qt/player/qml/TrackInfo.qml \
	gui/qt/player/qml/VolumeWidget.qml \
	gui/qt/playlist/qml/PLItem.qml \
	gui/qt/playlist/qml/PLItemFooter.qml \
	gui/qt/playlist/qml/PlaylistListView.qml \
	gui/qt/playlist/qml/PlaylistMainView.qml \
	gui/qt/playlist/qml/PlaylistMenu.qml \
	gui/qt/playlist/qml/PlaylistToolbar.qml \
	gui/qt/style/VLCColors.qml \
	gui/qt/style/VLCIcons.qml \
	gui/qt/style/VLCStyle.qml \
	gui/qt/style/qmldir \
	gui/qt/util/qml/KeyHelper.js \
	gui/qt/util/qml/SelectableDelegateModel.qml \
	gui/qt/widgets/qml/BusyIndicatorExt.qml \
	gui/qt/widgets/qml/CaptionLabel.qml \
	gui/qt/widgets/qml/ComboBoxExt.qml \
	gui/qt/widgets/qml/ContextButton.qml \
	gui/qt/widgets/qml/DNDLabel.qml \
	gui/qt/widgets/qml/DrawerExt.qml \
	gui/qt/widgets/qml/ExpandGridView.qml \
	gui/qt/widgets/qml/FocusBackground.qml \
	gui/qt/widgets/qml/GridItem.qml \
	gui/qt/widgets/qml/IconLabel.qml \
	gui/qt/widgets/qml/IconToolButton.qml \
	gui/qt/widgets/qml/ImageToolButton.qml \
	gui/qt/widgets/qml/KeyNavigableGridView.qml \
	gui/qt/widgets/qml/KeyNavigableListView.qml \
	gui/qt/widgets/qml/KeyNavigableTableView.qml \
	gui/qt/widgets/qml/LabelSeparator.qml \
	gui/qt/widgets/qml/ListItem.qml \
	gui/qt/widgets/qml/ListLabel.qml \
	gui/qt/widgets/qml/MediaCover.qml \
	gui/qt/widgets/qml/SubtitleLabel.qml \
	gui/qt/widgets/qml/MenuCaption.qml \
	gui/qt/widgets/qml/MenuExt.qml \
	gui/qt/widgets/qml/MenuItemExt.qml \
	gui/qt/widgets/qml/MenuLabel.qml \
	gui/qt/widgets/qml/NavigableCol.qml \
	gui/qt/widgets/qml/NavigableFocusScope.qml \
	gui/qt/widgets/qml/NavigableRow.qml \
	gui/qt/widgets/qml/PlayCover.qml \
	gui/qt/widgets/qml/RoundButton.qml \
	gui/qt/widgets/qml/RoundImage.qml \
	gui/qt/widgets/qml/ScanProgressBar.qml \
	gui/qt/widgets/qml/ScrollingText.qml \
	gui/qt/widgets/qml/SearchBox.qml \
	gui/qt/widgets/qml/SortControl.qml \
	gui/qt/widgets/qml/SpinBoxExt.qml \
	gui/qt/widgets/qml/StackViewExt.qml \
	gui/qt/widgets/qml/TabButtonExt.qml \
	gui/qt/widgets/qml/TableColumns.qml \
	gui/qt/widgets/qml/TextToolButton.qml \
	gui/qt/widgets/qml/ToolTipArea.qml \
	gui/qt/widgets/qml/VideoProgressBar.qml \
	gui/qt/widgets/qml/VideoQualityLabel.qml \
	gui/qt/widgets/qml/ListSubtitleLabel.qml

EXTRA_DIST += gui/qt/vlc.qrc $(libqt_plugin_la_RES)

nodist_libqt_plugin_la_SOURCES += gui/qt/resources.cpp

if ENABLE_VLM
nodist_libqt_plugin_la_SOURCES += gui/qt/dialogs/vlm/vlm.moc.cpp
endif

if HAVE_QMLCACHE

nodist_libqt_plugin_la_SOURCES += gui/qt/qmlcache_loader.cpp
libqt_plugin_la_SOURCES += $(libqt_plugin_la_QML)

.qml.cpp:
	$(AM_V_GEN)$(QMLCACHE) --resource=$(srcdir)/gui/qt/vlc.qrc -o $@ $<

.js.cpp:
	$(AM_V_GEN)$(QMLCACHE) --resource=$(srcdir)/gui/qt/vlc.qrc -o $@ $<

.mjs.cpp:
	$(AM_V_GEN)$(QMLCACHE) --resource=$(srcdir)/gui/qt/vlc.qrc -o $@ $<

qml_cache.qrc: gui/qt/vlc.qrc
	$(AM_V_GEN)$(QMLCACHE) --filter-resource-file -o $@ $<

gui/qt/resources.cpp: qml_cache.qrc $(libqt_plugin_la_RES)
	$(AM_V_GEN)$(RCC) --name qml_cache -o $@ $<

#output name matters here
gui/qt/qmlcache_loader.cpp: gui/qt/vlc.qrc qml_cache.qrc $(libqt_plugin_la_QML)
	$(AM_V_GEN)$(QMLCACHE) --resource-file-mapping=$<=qml_cache.qrc $< -o $@

else

EXTRA_DIST += $(libqt_plugin_la_QML)

gui/qt/resources.cpp: gui/qt/vlc.qrc $(libqt_plugin_la_RES) $(libqt_plugin_la_QML)
	$(AM_V_GEN)$(RCC) --name vlc -o $@ $<

endif

vlc_qt_check_SOURCES = gui/qt/vlc-qt-check.cpp
vlc_qt_check_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) -fPIC
vlc_qt_check_LDADD = $(QT_LIBS)

if ENABLE_QT
gui_LTLIBRARIES += libqt_plugin.la
BUILT_SOURCES += $(nodist_libqt_plugin_la_SOURCES)
if !HAVE_WIN32
if !HAVE_OS2
pkglibexec_PROGRAMS += vlc-qt-check
endif
endif
endif
