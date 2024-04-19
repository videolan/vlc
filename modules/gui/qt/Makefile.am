## Howto
# For each Q_OBJECT:
#   - Add *.moc.cpp to nodist_..._SOURCES
#   - Add *.cpp to ..._SOURCES
#   - Add *.hpp to ..._SOURCES
# For each UI
#   - Add *.ui to ..._UI
#   - Add *.h to nodist_..._SOURCES
# For each resource (icon, png, ...)
#   - Add it to DEPS_res
# For each qml and js resource
#   - Add it to ..._QML

include ../../common.am
guidir = $(pluginsdir)/gui
gui_LTLIBRARIES =
SUFFIXES += .ui .h .hpp .moc.cpp .qml
TEST_EXTENSIONS = .qml
EXTRA_DIST =

vlc_qt_libs = $(QT_LIBS) $(VLCQTDEPS_LIBS)

vlc_qt_check_SOURCES = vlc-qt-check.cpp plugins.hpp
vlc_qt_check_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) -fPIC $(CXXFLAGS_qt)
vlc_qt_check_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_qt)
vlc_qt_check_LDADD = $(vlc_qt_libs)

libqt_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) $(CPPFLAGS_qt) \
	-DQT_STRICT_ITERATORS \
	-I$(srcdir) -I$(builddir) \
	-I$(builddir)/dialogs/extended \
	-I$(builddir)/dialogs/fingerprint \
	-I$(builddir)/dialogs/firstrun \
	-I$(builddir)/dialogs/help \
	-I$(builddir)/dialogs/messages \
	-I$(builddir)/dialogs/open \
	-I$(builddir)/dialogs/podcast \
	-I$(builddir)/dialogs/preferences \
	-I$(builddir)/dialogs/sout \
	-I$(builddir)/dialogs/vlm \
	-DQPNI_HEADER=\<$(QT_VERSION_GUI)/QtGui/qpa/qplatformnativeinterface.h\>

libqt_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) $(CXXFLAGS_qt)
libqt_plugin_la_LIBADD = $(vlc_qt_libs)
libqt_plugin_la_LDFLAGS = $(AM_LDFLAGS)
if HAVE_DARWIN
libqt_plugin_la_LDFLAGS += -Wl,-framework,Cocoa
endif

if HAVE_X
if HAVE_XCB
libqt_plugin_la_CXXFLAGS += $(XCB_CFLAGS) -DQT_HAS_X11
libqt_plugin_la_LIBADD += $(XCB_LIBS) $(X_LIBS) $(X_PRE_LIB) -lX11
vlc_qt_check_CXXFLAGS += $(XCB_CFLAGS) -DQT_HAS_X11
endif
endif

if HAVE_QT_WAYLAND
libqt_plugin_la_CPPFLAGS += $(QT_WAYLAND_CFLAGS)
libqt_plugin_la_LIBADD += $(QT_WAYLAND_LIBS)
vlc_qt_check_CPPFLAGS += $(QT_WAYLAND_CFLAGS) -DQT_HAS_WAYLAND
vlc_qt_check_LDADD += $(QT_WAYLAND_LIBS)
endif

if HAVE_WIN32
libqt_plugin_la_CXXFLAGS += $(LIBCOMCXXFLAGS)
libqt_plugin_la_LIBADD += $(LIBCOM) -lcomctl32 -luuid -ldwmapi
endif
if UPDATE_CHECK
libqt_plugin_la_CPPFLAGS += -DUPDATE_CHECK
endif
if QT_QML_DEBUG
libqt_plugin_la_CPPFLAGS += -DQT_QML_DEBUG
endif
if HAVE_QT_GTK
libqt_plugin_la_CPPFLAGS += -DQT_HAS_GTK
endif

if HAVE_QT_CORE_PRIVATE
libqt_plugin_la_CPPFLAGS += -DQT_CORE_PRIVATE
endif

if HAVE_QT_GUI_PRIVATE
libqt_plugin_la_CPPFLAGS += -DQT_GUI_PRIVATE \
	-I$(QT_INCLUDE_DIRECTORY)/QtCore/$(QT_VERSION_CORE) \
	-I$(QT_INCLUDE_DIRECTORY)/QtCore/$(QT_VERSION_CORE)/QtCore \
	-I$(QT_INCLUDE_DIRECTORY)/QtGui/$(QT_VERSION_GUI) \
	-I$(QT_INCLUDE_DIRECTORY)/QtGui/$(QT_VERSION_GUI)/QtGui
endif

if HAVE_QT_DECLARATIVE_PRIVATE
libqt_plugin_la_CPPFLAGS += -DQT_DECLARATIVE_PRIVATE \
	-I$(QT_INCLUDE_DIRECTORY)/QtCore/$(QT_VERSION_CORE) \
	-I$(QT_INCLUDE_DIRECTORY)/QtCore/$(QT_VERSION_CORE)/QtCore \
	-I$(QT_INCLUDE_DIRECTORY)/QtGui/$(QT_VERSION_GUI) \
	-I$(QT_INCLUDE_DIRECTORY)/QtGui/$(QT_VERSION_GUI)/QtGui \
	-I$(QT_INCLUDE_DIRECTORY)/QtQml/$(QT_VERSION_QML) \
	-I$(QT_INCLUDE_DIRECTORY)/QtQml/$(QT_VERSION_QML)/QtQml \
	-I$(QT_INCLUDE_DIRECTORY)/QtQuick/$(QT_VERSION_QUICK) \
	-I$(QT_INCLUDE_DIRECTORY)/QtQuick/$(QT_VERSION_QUICK)/QtQuick

endif

libqt_plugin_la_SOURCES = \
	qt.cpp qt.hpp plugins.hpp \
	dialogs/bookmarks/bookmarks.cpp dialogs/bookmarks/bookmarks.hpp \
	dialogs/dialogs/dialogmodel.cpp \
	dialogs/dialogs/dialogmodel.hpp \
	dialogs/dialogs_provider.cpp dialogs/dialogs_provider.hpp \
	dialogs/epg/EPGChannels.cpp \
	dialogs/epg/EPGChannels.hpp \
	dialogs/epg/EPGItem.cpp \
	dialogs/epg/EPGItem.hpp \
	dialogs/epg/EPGProgram.cpp \
	dialogs/epg/EPGProgram.hpp \
	dialogs/epg/EPGRuler.cpp dialogs/epg/EPGRuler.hpp \
	dialogs/epg/EPGView.cpp dialogs/epg/EPGView.hpp \
	dialogs/epg/EPGWidget.cpp \
	dialogs/epg/EPGWidget.hpp \
	dialogs/epg/epg.cpp dialogs/epg/epg.hpp \
	dialogs/errors/errors.cpp dialogs/errors/errors.hpp \
	dialogs/extended/extended.cpp dialogs/extended/extended.hpp \
	dialogs/extended/extended_panels.cpp \
	dialogs/extended/extended_panels.hpp \
	dialogs/extensions/extensions.cpp dialogs/extensions/extensions.hpp \
	dialogs/extensions/extensions_manager.cpp dialogs/extensions/extensions_manager.hpp \
	dialogs/fingerprint/chromaprint.cpp \
	dialogs/fingerprint/chromaprint.hpp \
	dialogs/fingerprint/fingerprintdialog.cpp \
	dialogs/fingerprint/fingerprintdialog.hpp \
	dialogs/firstrun/firstrunwizard.cpp dialogs/firstrun/firstrunwizard.hpp \
	dialogs/gototime/gototime.cpp dialogs/gototime/gototime.hpp \
	dialogs/help/aboutmodel.cpp \
	dialogs/help/aboutmodel.hpp \
	dialogs/help/help.cpp dialogs/help/help.hpp \
	dialogs/mediainfo/info_panels.cpp dialogs/mediainfo/info_panels.hpp \
	dialogs/mediainfo/info_widgets.cpp dialogs/mediainfo/info_widgets.hpp \
	dialogs/mediainfo/mediainfo.cpp dialogs/mediainfo/mediainfo.hpp \
	dialogs/messages/messages.cpp dialogs/messages/messages.hpp \
	dialogs/open/open.cpp dialogs/open/open.hpp \
	dialogs/open/open_panels.cpp dialogs/open/open_panels.hpp \
	dialogs/open/openurl.cpp dialogs/open/openurl.hpp \
	dialogs/plugins/addons_manager.cpp dialogs/plugins/addons_manager.hpp \
	dialogs/plugins/plugins.cpp dialogs/plugins/plugins.hpp \
	dialogs/podcast/podcast_configuration.cpp \
	dialogs/podcast/podcast_configuration.hpp \
	dialogs/preferences/complete_preferences.cpp \
	dialogs/preferences/complete_preferences.hpp \
	dialogs/preferences/expert_model.cpp \
	dialogs/preferences/expert_model.hpp \
	dialogs/preferences/expert_view.cpp \
	dialogs/preferences/expert_view.hpp \
	dialogs/preferences/preferences.cpp \
	dialogs/preferences/preferences.hpp \
	dialogs/preferences/preferences_widgets.cpp \
	dialogs/preferences/preferences_widgets.hpp \
	dialogs/preferences/simple_preferences.cpp \
	dialogs/preferences/simple_preferences.hpp \
	dialogs/sout/convert.cpp dialogs/sout/convert.hpp \
	dialogs/sout/profile_selector.cpp \
	dialogs/sout/profile_selector.hpp \
	dialogs/sout/profiles.hpp \
	dialogs/sout/sout.cpp dialogs/sout/sout.hpp \
	dialogs/sout/sout_widgets.cpp \
	dialogs/sout/sout_widgets.hpp \
	dialogs/toolbar/controlbar_profile.hpp \
	dialogs/toolbar/controlbar_profile.cpp \
	dialogs/toolbar/controlbar_profile_model.cpp \
	dialogs/toolbar/controlbar_profile_model.hpp \
	dialogs/vlm/vlm.cpp dialogs/vlm/vlm.hpp \
	dialogs/playlists/playlists.cpp dialogs/playlists/playlists.hpp \
	maininterface/compositor.hpp \
	maininterface/compositor.cpp \
	maininterface/compositor_common.hpp \
	maininterface/compositor_common.cpp \
	maininterface/compositor_accessibility.cpp \
	maininterface/compositor_accessibility.hpp \
	maininterface/compositor_dummy.hpp \
	maininterface/compositor_dummy.cpp \
	maininterface/interface_window_handler.cpp \
	maininterface/interface_window_handler.hpp \
	maininterface/mainctx.cpp \
	maininterface/mainctx.hpp \
	maininterface/mainctx_submodels.hpp \
	maininterface/mainui.cpp \
	maininterface/mainui.hpp \
	maininterface/videosurface.cpp \
	maininterface/videosurface.hpp \
	maininterface/video_window_handler.cpp \
	maininterface/video_window_handler.hpp \
	medialibrary/medialib.cpp \
	medialibrary/medialib.hpp \
	medialibrary/mlalbum.cpp \
	medialibrary/mlalbum.hpp \
	medialibrary/mlalbummodel.cpp \
	medialibrary/mlalbummodel.hpp \
	medialibrary/mlalbumtrack.cpp \
	medialibrary/mlalbumtrack.hpp \
	medialibrary/mlalbumtrackmodel.cpp \
	medialibrary/mlalbumtrackmodel.hpp \
	medialibrary/mlartist.cpp \
	medialibrary/mlartist.hpp \
	medialibrary/mlartistmodel.cpp \
	medialibrary/mlartistmodel.hpp \
	medialibrary/mlbasemodel.cpp \
	medialibrary/mlbasemodel.hpp \
	medialibrary/mlbookmarkmodel.cpp \
	medialibrary/mlbookmarkmodel.hpp \
	medialibrary/mlevent.hpp \
	medialibrary/mlfolder.cpp \
	medialibrary/mlfolder.hpp \
	medialibrary/mlfoldersmodel.cpp \
	medialibrary/mlfoldersmodel.hpp \
	medialibrary/mlgenre.cpp \
	medialibrary/mlgenre.hpp \
	medialibrary/mlgenremodel.cpp \
	medialibrary/mlgenremodel.hpp \
	medialibrary/mlgroup.cpp \
	medialibrary/mlgroup.hpp \
	medialibrary/mlhelper.cpp \
	medialibrary/mlhelper.hpp \
	medialibrary/mlthreadpool.cpp \
	medialibrary/mlthreadpool.hpp \
	medialibrary/mlqmltypes.hpp \
	medialibrary/mlqueryparams.cpp \
	medialibrary/mlqueryparams.hpp \
	medialibrary/mlrecentsmodel.cpp \
	medialibrary/mlrecentsmodel.hpp \
	medialibrary/mlrecentsvideomodel.cpp \
	medialibrary/mlrecentsvideomodel.hpp \
	medialibrary/mlurlmodel.cpp \
	medialibrary/mlurlmodel.hpp \
	medialibrary/mlvideo.cpp \
	medialibrary/mlvideo.hpp \
	medialibrary/mlvideofoldersmodel.cpp \
	medialibrary/mlvideofoldersmodel.hpp \
	medialibrary/mlvideogroupsmodel.cpp \
	medialibrary/mlvideogroupsmodel.hpp \
	medialibrary/mlvideomodel.cpp \
	medialibrary/mlvideomodel.hpp \
	medialibrary/mlplaylist.cpp \
	medialibrary/mlplaylist.hpp \
	medialibrary/mlplaylistlistmodel.cpp \
	medialibrary/mlplaylistlistmodel.hpp \
	medialibrary/mlplaylistmedia.cpp \
	medialibrary/mlplaylistmedia.hpp \
	medialibrary/mlplaylistmodel.cpp \
	medialibrary/mlplaylistmodel.hpp \
	medialibrary/thumbnailcollector.hpp \
	medialibrary/thumbnailcollector.cpp \
	medialibrary/mlcustomcover.hpp \
	medialibrary/mlcustomcover.cpp \
	menus/custom_menus.cpp \
	menus/custom_menus.hpp \
	menus/qml_menu_wrapper.cpp \
	menus/qml_menu_wrapper.hpp \
	menus/menus.cpp menus/menus.hpp \
	network/mediatreelistener.cpp \
	network/mediatreelistener.hpp \
	network/networkdevicemodel.cpp \
	network/networkdevicemodel.hpp \
	network/networksourcesmodel.cpp \
	network/networksourcesmodel.hpp \
	network/networkmediamodel.cpp \
	network/networkmediamodel.hpp \
	network/servicesdiscoverymodel.cpp \
	network/servicesdiscoverymodel.hpp \
	network/standardpathmodel.cpp \
	network/standardpathmodel.hpp \
	style/qtthemeprovider.hpp \
	style/colorcontext.cpp \
	style/colorcontext.hpp \
	style/systempalette.cpp \
	style/systempalette.hpp \
	style/defaultthemeproviders.hpp \
	style/systempalettethemeprovider.cpp \
	player/input_models.cpp player/input_models.hpp \
	player/delay_estimator.cpp \
	player/delay_estimator.hpp \
	player/player_controller.cpp player/player_controller.hpp player/player_controller_p.hpp \
	player/player_controlbar_model.cpp player/player_controlbar_model.hpp \
	player/control_list_model.cpp player/control_list_model.hpp \
	player/control_list_filter.cpp \
	player/control_list_filter.hpp \
	playlist/media.hpp \
	playlist/playlist_common.hpp \
	playlist/playlist_controller.cpp \
	playlist/playlist_controller.hpp \
	playlist/playlist_controller_p.hpp \
	playlist/playlist_item.cpp \
	playlist/playlist_item.hpp \
	playlist/playlist_model.cpp \
	playlist/playlist_model.hpp \
	playlist/playlist_model_p.hpp \
	util/asynctask.hpp \
	util/audio_device_model.cpp  \
	util/audio_device_model.hpp \
	util/base_model.hpp util/base_model_p.hpp util/base_model.cpp \
	util/color_scheme_model.cpp util/color_scheme_model.hpp \
	util/color_svg_image_provider.cpp util/color_svg_image_provider.hpp \
	util/vlcaccess_image_provider.cpp util/vlcaccess_image_provider.hpp \
	util/covergenerator.cpp \
	util/covergenerator.hpp \
	util/imageluminanceextractor.cpp util/imageluminanceextractor.hpp \
	util/csdbuttonmodel.cpp util/csdbuttonmodel.hpp \
	util/imagehelper.cpp util/imagehelper.hpp \
	util/keyhelper.cpp util/keyhelper.hpp \
	util/listcache.hxx util/listcache.hpp \
	util/listcacheloader.hpp \
	util/locallistcacheloader.hpp \
	util/navigation_history.cpp util/navigation_history.hpp \
	util/item_key_event_filter.cpp \
	util/item_key_event_filter.hpp \
	util/flickable_scroll_handler.cpp \
	util/flickable_scroll_handler.hpp \
	util/qt_dirs.cpp util/qt_dirs.hpp \
	util/proxycolumnmodel.hpp \
	util/registry.cpp util/registry.hpp \
	util/renderer_manager.cpp util/renderer_manager.hpp \
	util/singleton.hpp \
	util/soutchain.cpp util/soutchain.hpp \
	util/validators.cpp util/validators.hpp \
	util/varcommon_p.hpp \
	util/varchoicemodel.cpp  util/varchoicemodel.hpp \
	util/variables.cpp util/variables.hpp \
	util/vlctick.cpp \
	util/vlctick.hpp \
	util/shared_input_item.hpp \
	util/fast_gaussian_blur_template.h \
	util/effects_image_provider.cpp \
	util/effects_image_provider.hpp \
	util/qsgroundedrectangularimagenode.cpp \
	util/qsgroundedrectangularimagenode.hpp \
	util/dismiss_popup_event_filter.cpp \
	util/dismiss_popup_event_filter.hpp \
	util/list_selection_model.cpp \
	util/list_selection_model.hpp \
	widgets/native/animators.cpp \
	widgets/native/animators.hpp \
	widgets/native/customwidgets.cpp widgets/native/customwidgets.hpp \
	widgets/native/csdthemeimage.cpp \
	widgets/native/csdthemeimage.hpp \
	widgets/native/interface_widgets.cpp \
	widgets/native/interface_widgets.hpp \
	widgets/native/navigation_attached.cpp \
	widgets/native/navigation_attached.hpp \
	widgets/native/mlfolderseditor.cpp \
	widgets/native/mlfolderseditor.hpp \
	widgets/native/qvlcframe.cpp \
	widgets/native/qvlcframe.hpp \
	widgets/native/roundimage.cpp widgets/native/roundimage.hpp widgets/native/roundimage_p.hpp \
	widgets/native/searchlineedit.cpp widgets/native/searchlineedit.hpp \
	widgets/native/viewblockingrectangle.cpp widgets/native/viewblockingrectangle.hpp

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
	dialogs/bookmarks/bookmarks.moc.cpp \
	dialogs/dialogs/dialogmodel.moc.cpp \
	dialogs/dialogs_provider.moc.cpp \
	dialogs/epg/EPGChannels.moc.cpp \
	dialogs/epg/EPGProgram.moc.cpp \
	dialogs/epg/EPGRuler.moc.cpp \
	dialogs/epg/EPGView.moc.cpp \
	dialogs/epg/EPGWidget.moc.cpp \
	dialogs/epg/epg.moc.cpp \
	dialogs/errors/errors.moc.cpp \
	dialogs/extended/extended.moc.cpp \
	dialogs/extended/extended_panels.moc.cpp \
	dialogs/extensions/extensions.moc.cpp \
	dialogs/extensions/extensions_manager.moc.cpp \
	dialogs/fingerprint/chromaprint.moc.cpp \
	dialogs/fingerprint/fingerprintdialog.moc.cpp \
	dialogs/firstrun/firstrunwizard.moc.cpp \
	dialogs/gototime/gototime.moc.cpp \
	dialogs/help/aboutmodel.moc.cpp \
	dialogs/help/help.moc.cpp \
	dialogs/mediainfo/info_panels.moc.cpp \
	dialogs/mediainfo/info_widgets.moc.cpp \
	dialogs/mediainfo/mediainfo.moc.cpp \
	dialogs/messages/messages.moc.cpp \
	dialogs/open/open.moc.cpp \
	dialogs/open/open_panels.moc.cpp \
	dialogs/open/openurl.moc.cpp \
	dialogs/plugins/addons_manager.moc.cpp \
	dialogs/plugins/plugins.moc.cpp \
	dialogs/podcast/podcast_configuration.moc.cpp \
	dialogs/preferences/complete_preferences.moc.cpp \
	dialogs/preferences/expert_model.moc.cpp \
	dialogs/preferences/expert_view.moc.cpp \
	dialogs/preferences/preferences.moc.cpp \
	dialogs/preferences/preferences_widgets.moc.cpp \
	dialogs/preferences/simple_preferences.moc.cpp \
	dialogs/sout/convert.moc.cpp \
	dialogs/sout/profile_selector.moc.cpp \
	dialogs/sout/sout.moc.cpp \
	dialogs/sout/sout_widgets.moc.cpp \
	dialogs/toolbar/controlbar_profile.moc.cpp \
	dialogs/toolbar/controlbar_profile_model.moc.cpp \
	dialogs/playlists/playlists.moc.cpp \
	maininterface/compositor.moc.cpp \
	maininterface/compositor_common.moc.cpp \
	maininterface/compositor_dummy.moc.cpp \
	maininterface/interface_window_handler.moc.cpp \
	maininterface/mainctx.moc.cpp \
	maininterface/mainctx_submodels.moc.cpp \
	maininterface/mainui.moc.cpp \
	maininterface/videosurface.moc.cpp \
	maininterface/video_window_handler.moc.cpp \
	medialibrary/medialib.moc.cpp \
	medialibrary/mlalbum.moc.cpp \
	medialibrary/mlalbummodel.moc.cpp \
	medialibrary/mlalbumtrack.moc.cpp \
	medialibrary/mlalbumtrackmodel.moc.cpp \
	medialibrary/mlartist.moc.cpp \
	medialibrary/mlartistmodel.moc.cpp \
        medialibrary/mlbookmarkmodel.moc.cpp \
	medialibrary/mlbasemodel.moc.cpp \
	medialibrary/mlfoldersmodel.moc.cpp \
	medialibrary/mlgenremodel.moc.cpp \
	medialibrary/mlthreadpool.moc.cpp \
	medialibrary/mlqmltypes.moc.cpp \
	medialibrary/mlrecentsmodel.moc.cpp \
	medialibrary/mlrecentsvideomodel.moc.cpp \
	medialibrary/mlurlmodel.moc.cpp \
	medialibrary/mlvideo.moc.cpp \
	medialibrary/mlvideomodel.moc.cpp \
	medialibrary/mlplaylistlistmodel.moc.cpp \
	medialibrary/mlplaylistmodel.moc.cpp \
	medialibrary/mlvideofoldersmodel.moc.cpp \
	medialibrary/mlvideogroupsmodel.moc.cpp \
	medialibrary/thumbnailcollector.moc.cpp \
	menus/custom_menus.moc.cpp \
	menus/qml_menu_wrapper.moc.cpp \
	menus/menus.moc.cpp \
	network/networkdevicemodel.moc.cpp \
	network/networksourcesmodel.moc.cpp \
	network/networkmediamodel.moc.cpp \
	network/servicesdiscoverymodel.moc.cpp \
	network/standardpathmodel.moc.cpp \
	style/colorcontext.moc.cpp \
	style/systempalette.moc.cpp \
	player/delay_estimator.moc.cpp \
	player/input_models.moc.cpp \
	player/player_controller.moc.cpp \
	player/player_controlbar_model.moc.cpp \
	player/control_list_model.moc.cpp \
	player/control_list_filter.moc.cpp \
	playlist/playlist_controller.moc.cpp \
	playlist/playlist_item.moc.cpp \
	playlist/playlist_model.moc.cpp \
	util/asynctask.moc.cpp \
	util/audio_device_model.moc.cpp \
	util/base_model.moc.cpp \
	util/color_scheme_model.moc.cpp \
	util/color_svg_image_provider.moc.cpp \
	util/vlcaccess_image_provider.moc.cpp \
	util/imageluminanceextractor.moc.cpp \
	util/csdbuttonmodel.moc.cpp \
	util/keyhelper.moc.cpp \
	util/listcache.moc.cpp \
	util/locallistcacheloader.moc.cpp \
	util/navigation_history.moc.cpp \
	util/item_key_event_filter.moc.cpp \
	util/effects_image_provider.moc.cpp \
	util/flickable_scroll_handler.moc.cpp \
	util/renderer_manager.moc.cpp \
	util/validators.moc.cpp \
	util/varchoicemodel.moc.cpp \
	util/variables.moc.cpp \
	util/vlctick.moc.cpp \
	util/dismiss_popup_event_filter.moc.cpp \
	util/list_selection_model.moc.cpp \
	widgets/native/animators.moc.cpp \
	widgets/native/csdthemeimage.moc.cpp \
	widgets/native/customwidgets.moc.cpp \
	widgets/native/interface_widgets.moc.cpp \
	widgets/native/navigation_attached.moc.cpp \
	widgets/native/mlfolderseditor.moc.cpp \
	widgets/native/roundimage.moc.cpp \
	widgets/native/roundimage_p.moc.cpp \
	widgets/native/searchlineedit.moc.cpp \
	widgets/native/viewblockingrectangle.moc.cpp

nodist_libqt_plugin_la_SOURCES += \
	dialogs/extended/ui_equalizer.h \
	dialogs/extended/ui_video_effects.h \
	dialogs/fingerprint/ui_fingerprintdialog.h \
	dialogs/firstrun/ui_firstrunwizard.h \
	dialogs/help/ui_about.h \
	dialogs/help/ui_update.h \
	dialogs/messages/ui_messages_panel.h \
	dialogs/open/ui_open.h \
	dialogs/open/ui_open_capture.h \
	dialogs/open/ui_open_disk.h \
	dialogs/open/ui_open_file.h \
	dialogs/open/ui_open_net.h \
	dialogs/podcast/ui_podcast_configuration.h \
	dialogs/preferences/ui_sprefs_audio.h \
	dialogs/preferences/ui_sprefs_input.h \
	dialogs/preferences/ui_sprefs_interface.h \
	dialogs/preferences/ui_sprefs_medialibrary.h \
	dialogs/preferences/ui_sprefs_subtitles.h \
	dialogs/preferences/ui_sprefs_video.h \
	dialogs/sout/ui_profiles.h \
	dialogs/sout/ui_sout.h \
	dialogs/vlm/ui_vlm.h


# windows compositor

if HAVE_WIN32

libqt_plugin_la_SOURCES += \
	maininterface/mainctx_win32.cpp \
	maininterface/mainctx_win32.hpp \
	maininterface/compositor_win7.cpp \
	maininterface/compositor_win7.hpp \
	style/windowsthemeprovider.cpp

nodist_libqt_plugin_la_SOURCES += maininterface/mainctx_win32.moc.cpp \
	maininterface/compositor_win7.moc.cpp

if HAVE_DCOMP
libqt_plugin_la_SOURCES += \
	maininterface/compositor_dcomp.cpp \
	maininterface/compositor_dcomp.hpp \
	maininterface/compositor_dcomp_error.hpp \
	maininterface/compositor_dcomp_acrylicsurface.hpp \
	maininterface/compositor_dcomp_acrylicsurface.cpp

nodist_libqt_plugin_la_SOURCES += \
	maininterface/compositor_dcomp_acrylicsurface.moc.cpp \
	maininterface/compositor_dcomp.moc.cpp

endif
endif

# X11 compositor

if HAVE_X
if HAVE_XCB
if HAVE_XCB_DAMAGE
if HAVE_XCB_XFIXES

libqt_plugin_la_CXXFLAGS += -DQT_HAS_X11_COMPOSITOR \
	$(QT_X11_CFLAGS) $(XCB_RENDER_CFLAGS) $(XCB_COMPOSITE_CFLAGS) $(XLCB_DAMAGE_CFLAGS) $(XCB_XFIXES_CFLAGS)

libqt_plugin_la_LIBADD += $(QT_X11_LIBS) $(XCB_RENDER_LIBS) $(XCB_COMPOSITE_LIBS) $(XCB_DAMAGE_LIBS) $(XCB_XFIXES_LIBS)

libqt_plugin_la_SOURCES += \
	maininterface/compositor_x11.cpp \
	maininterface/compositor_x11.hpp \
	maininterface/compositor_x11_renderclient.cpp \
	maininterface/compositor_x11_renderclient.hpp \
	maininterface/compositor_x11_renderwindow.cpp \
	maininterface/compositor_x11_renderwindow.hpp \
	maininterface/compositor_x11_uisurface.cpp \
	maininterface/compositor_x11_uisurface.hpp \
	maininterface/compositor_x11_utils.cpp \
	maininterface/compositor_x11_utils.hpp

nodist_libqt_plugin_la_SOURCES += \
	maininterface/compositor_x11.moc.cpp \
	maininterface/compositor_x11_renderclient.moc.cpp \
	maininterface/compositor_x11_renderwindow.moc.cpp \
	maininterface/compositor_x11_uisurface.moc.cpp \
	maininterface/compositor_x11_utils.moc.cpp

endif
endif
endif
endif

# wayland compositor

if HAVE_WAYLAND
if HAVE_QT_WAYLAND
if HAVE_QT_GUI_PRIVATE

libqt_plugin_la_CPPFLAGS += -DQT_HAS_WAYLAND_COMPOSITOR

libqt_plugin_la_SOURCES += \
	maininterface/compositor_wayland.cpp \
	maininterface/compositor_wayland.hpp


nodist_libqt_plugin_la_SOURCES += \
	maininterface/compositor_wayland.moc.cpp

libqt_wayland_plugin_la_CFLAGS = $(AM_CFLAGS) $(WAYLAND_CLIENT_CFLAGS)
libqt_wayland_plugin_la_LIBADD =  $(WAYLAND_CLIENT_LIBS)
libqt_wayland_plugin_la_SOURCES = \
	maininterface/compositor_wayland_module.c \
	maininterface/compositor_wayland_module.h

if ENABLE_QT
gui_LTLIBRARIES += libqt_wayland_plugin.la
endif

endif
endif
endif

# User interface compilation

uic_verbose = $(uic_verbose_$(V))
uic_verbose_ = $(uic_verbose__$(AM_DEFAULT_VERBOSITY))
uic_verbose_0 = @echo "  UIC   " $@;
uic_verbose__0 = $(uic_verbose_0)

MOC_CPPFLAGS = $(DEFS) \
	-I$(top_srcdir)/include -I$(top_builddir)/include -I$(top_builddir) \
	$(QT_CFLAGS) \
	$(libqt_plugin_la_CPPFLAGS)
if HAVE_DARWIN
MOC_CPPFLAGS += -DQ_OS_MAC
endif
if HAVE_WIN32
MOC_CPPFLAGS += -D_WIN32
endif

libqt_plugin_la_UI = \
	dialogs/extended/equalizer.ui \
	dialogs/extended/video_effects.ui \
	dialogs/fingerprint/fingerprintdialog.ui \
	dialogs/firstrun/firstrunwizard.ui \
	dialogs/help/about.ui \
	dialogs/help/update.ui \
	dialogs/messages/messages_panel.ui \
	dialogs/open/open.ui \
	dialogs/open/open_capture.ui \
	dialogs/open/open_disk.ui \
	dialogs/open/open_file.ui \
	dialogs/open/open_net.ui \
	dialogs/podcast/podcast_configuration.ui \
	dialogs/preferences/sprefs_audio.ui \
	dialogs/preferences/sprefs_input.ui \
	dialogs/preferences/sprefs_interface.ui \
	dialogs/preferences/sprefs_medialibrary.ui \
	dialogs/preferences/sprefs_subtitles.ui \
	dialogs/preferences/sprefs_video.ui \
	dialogs/sout/profiles.ui \
	dialogs/sout/sout.ui \
	dialogs/vlm/vlm.ui
EXTRA_DIST += $(libqt_plugin_la_UI)

ui_%.h: %.ui
	$(AM_V_at)$(MKDIR_P) "ui"
	$(AM_V_at)rm -f $@ $@.tmp
	$(AM_V_at)echo "#define Q_(a,b) QString::fromUtf8(_(a))" > $@.tmp
	$(uic_verbose)$(UIC) -tr "Q_" $< >> $@.tmp
	$(AM_V_at)sed -e 's/Q_(\"_(\\\"\(.*\)\\\")"/Q_("\1"/' $@.tmp >$@
	$(AM_V_at)rm -f $@.tmp

# Resources

libqt_plugin_la_RES = \
	pixmaps/VLCIcons.ttf \
	pixmaps/addons/addon_default.svg \
	pixmaps/addons/addon_broken.svg \
	pixmaps/addons/addon_blue.svg \
	pixmaps/addons/addon_cyan.svg \
	pixmaps/addons/addon_green.svg \
	pixmaps/addons/addon_magenta.svg \
	pixmaps/addons/addon_red.svg \
	pixmaps/addons/addon_yellow.svg \
	pixmaps/addons/addon_score.svg \
	pixmaps/icons/active_indicator.svg \
	pixmaps/icons/add.svg \
	pixmaps/icons/album_cover.svg \
	pixmaps/icons/aspect_ratio.svg \
	pixmaps/icons/atob.svg \
	pixmaps/icons/audiosub.svg \
	pixmaps/icons/back.svg \
	pixmaps/icons/ic_fluent_bookmark.svg \
	pixmaps/icons/breadcrumb_prev.svg \
	pixmaps/icons/breadcrumb_sep.svg \
	pixmaps/icons/check.svg \
	pixmaps/icons/clear.svg \
	pixmaps/icons/close.svg \
	pixmaps/icons/ic_fluent_full_screen_minimize.svg \
	pixmaps/icons/del.svg \
	pixmaps/icons/download.svg \
	pixmaps/icons/dropzone.svg \
	pixmaps/icons/dvd_menu.svg \
	pixmaps/icons/dvd_next.svg \
	pixmaps/icons/dvd_prev.svg \
	pixmaps/icons/ic_fluent_arrow_eject.svg \
	pixmaps/icons/ellipsis.svg \
	pixmaps/icons/ic_playlist_add.svg \
	pixmaps/icons/expand_inverted.svg \
	pixmaps/icons/expand.svg \
	pixmaps/icons/ic_fluent_options.svg \
	pixmaps/icons/ic_fluent_fast_forward.svg \
	pixmaps/icons/frame-by-frame.svg \
	pixmaps/icons/ic_fluent_full_screen_maximize.svg \
	pixmaps/icons/ic_fluent_grid.svg \
	pixmaps/icons/history.svg \
	pixmaps/icons/home.svg \
	pixmaps/icons/info.svg \
	pixmaps/icons/ic_fluent_apps_list.svg \
	pixmaps/icons/ic_fluent_more_vertical.svg \
	pixmaps/icons/multiselect.svg \
	pixmaps/icons/ok.svg \
	pixmaps/icons/ic_fluent_next.svg \
	pixmaps/icons/ic_pause_filled.svg \
	pixmaps/icons/pip.svg \
	pixmaps/icons/ic_playlist_clear.svg \
	pixmaps/icons/ic_playlist.svg \
	pixmaps/icons/ic_fluent_play.svg \
	pixmaps/icons/play_reverse.svg \
	pixmaps/icons/ic_fluent_arrow_move.svg \
	pixmaps/icons/ic_fluent_chevron_down_24.svg \
	pixmaps/icons/ic_fluent_chevron_left_24.svg \
	pixmaps/icons/ic_fluent_chevron_right_24.svg \
	pixmaps/icons/ic_fluent_chevron_up_24.svg \
	pixmaps/icons/ic_fluent_play_filled.svg \
	pixmaps/icons/podcast.svg \
	pixmaps/icons/ic_fluent_previous.svg \
	pixmaps/icons/profile_new.svg \
	pixmaps/icons/record.svg \
	pixmaps/icons/remove.svg \
	pixmaps/icons/renderer.svg \
	pixmaps/icons/ic_fluent_arrow_repeat_all.svg \
	pixmaps/icons/ic_fluent_arrow_repeat_1.svg \
	pixmaps/icons/ic_fluent_rewind.svg \
	pixmaps/icons/ic_fluent_search.svg \
	pixmaps/icons/ic_fluent_arrow_shuffle.svg \
	pixmaps/icons/ic_fluent_skip_back_10.svg \
	pixmaps/icons/ic_fluent_skip_forward_10.svg \
	pixmaps/icons/ic_fluent_camera.svg \
	pixmaps/icons/space.svg \
	pixmaps/icons/ic_fluent_stop.svg \
	pixmaps/icons/stream.svg \
	pixmaps/icons/sync.svg \
	pixmaps/icons/time.svg \
	pixmaps/icons/ic_fluent_globe.svg \
	pixmaps/icons/topbar_filter.svg \
	pixmaps/icons/ic_fluent_music_note_2.svg \
	pixmaps/icons/ic_fluent_wifi_1.svg \
	pixmaps/icons/ic_fluent_arrow_sort.svg \
	pixmaps/icons/ic_fluent_filmstrip.svg \
	pixmaps/icons/ic_fluent_chevron_up.svg \
	pixmaps/icons/ic_fluent_chevron_down.svg \
	pixmaps/icons/tv.svg \
	pixmaps/icons/tvtelx.svg \
	pixmaps/icons/visualization.svg \
	pixmaps/icons/volume_high.svg \
	pixmaps/icons/volume_low.svg \
	pixmaps/icons/volume_medium.svg \
	pixmaps/icons/volume_muted.svg \
	pixmaps/icons/volume_zero.svg \
	pixmaps/icons/window_close.svg \
	pixmaps/icons/window_maximize.svg \
	pixmaps/icons/window_minimize.svg \
	pixmaps/icons/window_restore.svg \
	pixmaps/menu/add.svg \
	pixmaps/menu/capture-card.svg \
	pixmaps/menu/clear.svg \
	pixmaps/menu/disc.svg \
	pixmaps/menu/download.svg \
	pixmaps/menu/ic_fluent_arrow_eject.svg \
	pixmaps/menu/exit.svg \
	pixmaps/menu/ic_fluent_fast_forward.svg \
	pixmaps/menu/file.svg \
	pixmaps/menu/folder.svg \
	pixmaps/menu/help.svg \
	pixmaps/menu/info.svg \
	pixmaps/menu/media_library.svg \
	pixmaps/menu/messages.svg \
	pixmaps/menu/movie.svg \
	pixmaps/menu/music.svg \
	pixmaps/menu/network.svg \
	pixmaps/menu/ic_playlist.svg \
	pixmaps/menu/ic_fluent_next.svg \
	pixmaps/menu/ic_pause_filled.svg \
	pixmaps/menu/ic_fluent_play_filled.svg \
	pixmaps/menu/preferences.svg \
	pixmaps/menu/ic_fluent_previous.svg \
	pixmaps/menu/profile_new.svg \
	pixmaps/menu/record.svg \
	pixmaps/menu/remove.svg \
	pixmaps/menu/ic_fluent_arrow_repeat_all.svg \
	pixmaps/menu/repeat_off.svg \
	pixmaps/menu/ic_fluent_arrow_repeat_1.svg \
	pixmaps/menu/ic_fluent_rewind.svg \
	pixmaps/menu/search_clear.svg \
	pixmaps/menu/ic_fluent_options.svg \
	pixmaps/menu/ic_fluent_skip_back_10.svg \
	pixmaps/menu/ic_fluent_skip_forward_10.svg \
	pixmaps/menu/ic_fluent_arrow_shuffle.svg \
	pixmaps/menu/ic_fluent_stop.svg \
	pixmaps/menu/stream.svg \
	pixmaps/menu/sync.svg \
	pixmaps/menu/update.svg \
	pixmaps/menu/volume-high.svg \
	pixmaps/menu/volume-low.svg \
	pixmaps/menu/volume-muted.svg \
	pixmaps/misc/cone.svg \
	pixmaps/misc/new_indicator.svg \
	pixmaps/misc/play_shadow.png \
	pixmaps/misc/theme_dark.svg \
	pixmaps/misc/theme_daynight.svg \
	pixmaps/misc/theme_light.svg \
	pixmaps/misc/wait1.svg \
	pixmaps/misc/wait2.svg \
	pixmaps/misc/wait3.svg \
	pixmaps/misc/wait4.svg \
	pixmaps/placeholder/add_service.svg \
	pixmaps/placeholder/noart.png \
	pixmaps/placeholder/noart_album.svg \
	pixmaps/placeholder/noart_albumCover.svg \
	pixmaps/placeholder/noart_artist.svg \
	pixmaps/placeholder/noart_artistCover.svg \
	pixmaps/placeholder/noart_artist_small.svg \
	pixmaps/placeholder/noart_videoCover.svg \
	pixmaps/prefs/advprefs_audio.svg \
	pixmaps/prefs/advprefs_codec.svg \
	pixmaps/prefs/advprefs_extended.svg \
	pixmaps/prefs/advprefs_intf.svg \
	pixmaps/prefs/advprefs_playlist.svg \
	pixmaps/prefs/advprefs_sout.svg \
	pixmaps/prefs/advprefs_video.svg \
	pixmaps/prefs/sample_complete.png \
	pixmaps/prefs/sample_minimal.png \
	pixmaps/prefs/sample_skins.png \
	pixmaps/prefs/spref_audio.png \
	pixmaps/prefs/spref_hotkeys.png \
	pixmaps/prefs/spref_input.png \
	pixmaps/prefs/spref_interface.png \
	pixmaps/prefs/spref_medialibrary.png \
	pixmaps/prefs/spref_subtitles.png \
	pixmaps/prefs/spref_video.png \
	pixmaps/sd/appletrailers.svg \
	pixmaps/sd/assembleenationale.svg \
	pixmaps/sd/capture-card.svg \
	pixmaps/sd/directory.svg \
	pixmaps/sd/disc.svg \
	pixmaps/sd/file.svg \
	pixmaps/sd/fmc.svg \
	pixmaps/sd/frenchtv.svg \
	pixmaps/sd/harddisk.svg \
	pixmaps/sd/icecast.svg \
	pixmaps/sd/jamendo.svg \
	pixmaps/sd/katsomo.svg \
	pixmaps/sd/lan.svg \
	pixmaps/sd/library.svg \
	pixmaps/sd/metachannels.svg \
	pixmaps/sd/mtp.svg \
	pixmaps/sd/network.svg \
	pixmaps/sd/node.svg \
	pixmaps/sd/pictures.svg \
	pixmaps/sd/playlist.svg \
	pixmaps/sd/podcast.svg \
	pixmaps/sd/screen.svg \
	pixmaps/sd/stream.svg \
	pixmaps/sd/unknown.svg

if HAVE_WIN32
libqt_plugin_la_RES += pixmaps/windows-theme.conf \
	pixmaps/win7/pause.svg \
	pixmaps/win7/prev.svg \
	pixmaps/win7/next.svg \
	pixmaps/win7/play.svg
endif

#QML and JS resources
libqt_plugin_la_QML = \
	dialogs/dialogs/qml/CustomDialog.qml \
	dialogs/dialogs/qml/Dialogs.qml \
	dialogs/dialogs/qml/ModalDialog.qml \
	dialogs/dialogs/qml/WindowDialog.qml \
	dialogs/toolbar/qml/EditorDNDDelegate.qml \
	dialogs/toolbar/qml/EditorDNDView.qml\
	dialogs/toolbar/qml/EditorDummyButton.qml \
	dialogs/toolbar/qml/EditorTabButton.qml \
	dialogs/toolbar/qml/ToolbarEditor.qml \
	dialogs/toolbar/qml/ToolbarEditorDialog.qml \
	dialogs/toolbar/qml/ToolbarEditorButtonList.qml \
	maininterface/qml/BannerSources.qml \
	maininterface/qml/MainInterface.qml \
	maininterface/qml/MainViewLoader.qml \
	maininterface/qml/MainDisplay.qml \
	maininterface/qml/MainGridView.qml \
	maininterface/qml/MainTableView.qml \
	maininterface/qml/NoMedialibHome.qml \
	medialibrary/qml/ArtistTopBanner.qml \
	medialibrary/qml/AudioGridItem.qml \
	medialibrary/qml/MediaGroupDisplay.qml \
	medialibrary/qml/MusicAlbums.qml \
	medialibrary/qml/MusicAlbumsDisplay.qml \
	medialibrary/qml/MusicAlbumsGridExpandDelegate.qml \
	medialibrary/qml/MusicArtist.qml \
	medialibrary/qml/MusicArtistDelegate.qml \
	medialibrary/qml/MusicArtistsAlbums.qml \
	medialibrary/qml/MusicAllArtists.qml \
	medialibrary/qml/MusicArtistsDisplay.qml \
	medialibrary/qml/MusicDisplay.qml \
	medialibrary/qml/MusicGenres.qml \
	medialibrary/qml/MusicGenresDisplay.qml \
	medialibrary/qml/MusicPlaylistsDisplay.qml \
	medialibrary/qml/MusicTrackListDisplay.qml \
	medialibrary/qml/MusicTracksDisplay.qml \
	medialibrary/qml/UrlListDisplay.qml \
	medialibrary/qml/VideoDisplay.qml \
	medialibrary/qml/VideoAll.qml \
	medialibrary/qml/VideoAllDisplay.qml \
	medialibrary/qml/VideoAllSubDisplay.qml \
	medialibrary/qml/PlaylistMediaList.qml \
	medialibrary/qml/PlaylistMedia.qml \
	medialibrary/qml/PlaylistMediaDisplay.qml \
	medialibrary/qml/VideoPlaylistsDisplay.qml \
        medialibrary/qml/VideoRecentVideos.qml \
        medialibrary/qml/VideoRecentVideosDisplay.qml \
	medialibrary/qml/VideoGridItem.qml \
	medialibrary/qml/VideoInfoExpandPanel.qml \
	medialibrary/qml/VideoListDisplay.qml \
        medialibrary/qml/VideoGridDisplay.qml \
	menus/qml/GlobalShortcuts.qml \
	menus/qml/GlobalShortcutsMedialib.qml \
	menus/qml/Menubar.qml \
	menus/qml/ShortcutExt.qml \
	network/qml/AddressbarButton.qml \
	network/qml/BrowseDeviceView.qml \
	network/qml/BrowseDisplay.qml \
	network/qml/BrowseHomeDisplay.qml \
	network/qml/BrowseTreeDisplay.qml \
	network/qml/BrowseTreeHeader.qml \
	network/qml/DiscoverDisplay.qml \
	network/qml/DiscoverUrlDisplay.qml \
	network/qml/NetworkAddressbar.qml \
	network/qml/NetworkCustomCover.qml \
	network/qml/NetworkGridItem.qml \
	network/qml/NetworkThumbnailItem.qml \
	network/qml/ServicesHomeDisplay.qml \
	network/qml/ServicesManage.qml \
	network/qml/ServicesSources.qml \
	player/qml/qmldir \
        player/qml/Bookmarks.qml \
	player/qml/ControlBar.qml \
	player/qml/ControlbarControls.qml \
	player/qml/PlaybackSpeed.qml \
	player/qml/MiniPlayer.qml \
	player/qml/PIPPlayer.qml \
	player/qml/Player.qml \
	player/qml/PlayerBlurredBackground.qml \
	player/qml/PlayerControlLayout.qml \
	player/qml/PlayerPlaylistVisibilityFSM.qml \
	player/qml/ResumeDialog.qml \
	player/qml/SliderBar.qml \
	player/qml/TopBar.qml \
	player/qml/TracksMenu.qml \
	player/qml/TracksPage.qml \
	player/qml/TracksPageAudio.qml \
	player/qml/TracksPageSpeed.qml \
	player/qml/TracksPageSubtitle.qml \
	player/qml/TracksListPage.qml \
	player/qml/ControlLayout.qml \
	player/qml/ControlRepeater.qml \
	player/qml/NavigationBox.qml \
	player/qml/controlbarcontrols/HighResolutionTimeWidget.qml \
	player/qml/controlbarcontrols/ArtworkInfoWidget.qml \
	player/qml/controlbarcontrols/AspectRatioWidget.qml \
	player/qml/controlbarcontrols/AtoBButton.qml \
	player/qml/controlbarcontrols/BackButton.qml \
	player/qml/controlbarcontrols/BookmarkButton.qml \
	player/qml/controlbarcontrols/ChapterNextButton.qml \
	player/qml/controlbarcontrols/ChapterPreviousButton.qml \
	player/qml/controlbarcontrols/DvdMenuButton.qml \
	player/qml/controlbarcontrols/ExpandingSpacerWidget.qml \
	player/qml/controlbarcontrols/ExtendedSettingsButton.qml \
	player/qml/controlbarcontrols/FasterButton.qml \
	player/qml/controlbarcontrols/FrameButton.qml \
	player/qml/controlbarcontrols/FullscreenButton.qml \
	player/qml/controlbarcontrols/InfoButton.qml \
	player/qml/controlbarcontrols/LangButton.qml \
	player/qml/controlbarcontrols/LoopButton.qml \
	player/qml/controlbarcontrols/MenuButton.qml \
	player/qml/controlbarcontrols/NavigationBoxButton.qml \
	player/qml/controlbarcontrols/NavigationWidget.qml \
	player/qml/controlbarcontrols/NextButton.qml \
	player/qml/controlbarcontrols/OpenButton.qml \
	player/qml/controlbarcontrols/PlaybackSpeedButton.qml \
	player/qml/controlbarcontrols/PlayButton.qml \
	player/qml/controlbarcontrols/PlayerSwitchButton.qml \
	player/qml/controlbarcontrols/PlaylistButton.qml \
	player/qml/controlbarcontrols/PreviousButton.qml \
	player/qml/controlbarcontrols/ProgramButton.qml \
	player/qml/controlbarcontrols/QuitButton.qml \
	player/qml/controlbarcontrols/RandomButton.qml \
	player/qml/controlbarcontrols/RendererButton.qml \
	player/qml/controlbarcontrols/RecordButton.qml \
	player/qml/controlbarcontrols/ReverseButton.qml \
	player/qml/controlbarcontrols/SkipBackButton.qml \
	player/qml/controlbarcontrols/SkipForwardButton.qml \
	player/qml/controlbarcontrols/SlowerButton.qml \
	player/qml/controlbarcontrols/SnapshotButton.qml \
	player/qml/controlbarcontrols/SpacerWidget.qml \
	player/qml/controlbarcontrols/StopButton.qml \
	player/qml/controlbarcontrols/TeletextButton.qml \
	player/qml/controlbarcontrols/TeletextColorButton.qml \
	player/qml/controlbarcontrols/TeletextWidget.qml \
	player/qml/controlbarcontrols/VolumeWidget.qml \
	player/qml/controlbarcontrols/Fallback.qml \
	playlist/qml/PlaylistDelegate.qml \
	playlist/qml/PlaylistListView.qml \
	playlist/qml/PlaylistToolbar.qml \
	playlist/qml/PlaylistDetachedWindow.qml \
	style/AcrylicController.qml \
	style/VLCIcons.qml \
	style/VLCStyle.qml \
	style/qmldir \
	util/qml/Helpers.js \
	util/qml/ModelSortSettingHandler.qml \
	util/qml/FlickableScrollHandler.qml \
	util/qml/ViewDragAutoScrollHandler.qml \
	util/qml/FSM.qml \
	util/qml/FSMState.qml \
	util/qml/NativeMenu.qml \
	util/qml/MLContextMenu.qml \
	util/qml/FadeControllerStateGroup.qml \
	widgets/qml/LabelExt.qml \
	widgets/qml/ActionButtonOverlay.qml \
	widgets/qml/ActionButtonPrimary.qml \
	widgets/qml/BannerTabButton.qml \
	widgets/qml/BannerCone.qml \
	widgets/qml/BusyIndicatorExt.qml \
	widgets/qml/CaptionLabel.qml \
	widgets/qml/CheckButton.qml \
	widgets/qml/CheckedDelegate.qml \
	widgets/qml/ComboBoxExt.qml \
	widgets/qml/AcrylicBackground.qml \
	widgets/qml/AnimatedBackground.qml \
	widgets/qml/CoverShadow.qml \
	widgets/qml/CSDThemeButton.qml \
	widgets/qml/CSDThemeButtonSet.qml \
	widgets/qml/CSDWindowButton.qml \
	widgets/qml/CSDWindowButtonSet.qml \
	widgets/qml/CSDTitlebarTapNDrapHandler.qml \
	widgets/qml/CSDMouseStealer.qml \
	widgets/qml/CurrentIndicator.qml \
	widgets/qml/DragItem.qml \
	widgets/qml/DrawerExt.qml \
	widgets/qml/ExpandGridView.qml \
	widgets/qml/EmptyLabel.qml \
	widgets/qml/EmptyLabelButton.qml \
	widgets/qml/EmptyLabelHint.qml \
	widgets/qml/GridItem.qml \
	widgets/qml/HorizontalResizeHandle.qml \
	widgets/qml/IconLabel.qml \
	widgets/qml/IconButton.qml \
	widgets/qml/IconToolButton.qml \
	widgets/qml/IconTrackButton.qml \
	widgets/qml/ImageToolButton.qml \
	widgets/qml/KeyNavigableListView.qml \
	widgets/qml/KeyNavigableTableView.qml \
	widgets/qml/TableViewDelegate.qml \
	widgets/qml/DefaultShadow.qml \
	widgets/qml/ListLabel.qml \
	widgets/qml/LocalTabBar.qml \
	widgets/qml/MediaCover.qml \
	widgets/qml/SubtitleLabel.qml \
	widgets/qml/MenuCaption.qml \
	widgets/qml/MenuLabel.qml \
	widgets/qml/MLDragItem.qml \
	widgets/qml/NavigableCol.qml \
	widgets/qml/NavigableRow.qml \
	widgets/qml/PlayCover.qml \
	widgets/qml/RoundButton.qml \
	widgets/qml/ScaledImage.qml \
	widgets/qml/ScanProgressBar.qml \
        widgets/qml/TextAutoScroller.qml \
	widgets/qml/SearchBox.qml \
	widgets/qml/SliderExt.qml \
	widgets/qml/SortControl.qml \
	widgets/qml/SpinBoxExt.qml \
	widgets/qml/StackViewExt.qml \
	widgets/qml/PageLoader.qml \
	widgets/qml/ButtonExt.qml \
	widgets/qml/TableColumns.qml \
	widgets/qml/TextFieldExt.qml \
	widgets/qml/TextToolButton.qml \
	widgets/qml/TrackDelayButton.qml \
	widgets/qml/VideoProgressBar.qml \
	widgets/qml/VideoQualityLabels.qml \
	widgets/qml/ListSubtitleLabel.qml \
	widgets/qml/PointingTooltip.qml \
	widgets/qml/FrostedGlassEffect.qml \
	widgets/qml/OverlayMenu.qml \
	widgets/qml/ToolTipExt.qml \
	widgets/qml/DropShadowImage.qml \
	widgets/qml/DoubleShadow.qml \
	widgets/qml/FadingEdge.qml \
	widgets/qml/FadingEdgeForListView.qml \
	widgets/qml/PopupIconToolButton.qml \
        widgets/qml/PartialEffect.qml \
        widgets/qml/ViewHeader.qml


# Shaders:
libqt_plugin_la_SHADER := shaders/FadingEdge.frag \
                          shaders/FadingEdgeX.vert \
                          shaders/FadingEdgeY.vert \
                          shaders/FrostedGlass.frag \
                          shaders/RectFilter.frag \
                          shaders/SubTexture.vert \
                          shaders/PlayerBlurredBackground.frag
if ENABLE_QT
BUILT_SOURCES += \
    $(libqt_plugin_la_SHADER:.frag=.frag.qsb) \
    $(libqt_plugin_la_SHADER:.vert=.vert.qsb)

QSB_PARAMS = --glsl="100 es,120,150" --batchable -O
if HAVE_WIN32
QSB_PARAMS += --hlsl=50 -c
endif
if HAVE_DARWIN
QSB_PARAMS += --msl=12
endif

QSB_EXTRA_PATH =
if HAVE_FXC
QSB_EXTRA_PATH += $(FXC_PATH):
endif

%.frag.qsb: %.frag
	$(AM_V_GEN) PATH="$(QSB_EXTRA_PATH)$$PATH" $(QSB) $(QSB_PARAMS) -o $@ $<

%.vert.qsb: %.vert
	$(AM_V_GEN) PATH="$(QSB_EXTRA_PATH)$$PATH" $(QSB) $(QSB_PARAMS) -o $@ $<

shaders/shaders.cpp: $(srcdir)/shaders/shaders.qrc $(libqt_plugin_la_SHADER:.frag=.frag.qsb) $(libqt_plugin_la_SHADER:.vert=.vert.qsb)
	$(AM_V_GEN) cp -f $(filter %.qrc, $^) $(builddir)/shaders/shaders.qrc || true
	$(AM_V_GEN) $(RCC) --name shaders -o $@ $(builddir)/shaders/$(notdir $(filter %.qrc, $^))

libqt_plugin_la_SOURCES += shaders/shaders.qrc $(libqt_plugin_la_SHADER)
BUILT_SOURCES += shaders/shaders.qrc

lib_qt_plugin_la_QRC = vlc.qrc

if HAVE_WIN32
lib_qt_plugin_la_QRC += windows.qrc
endif

EXTRA_DIST += $(lib_qt_plugin_la_QRC) $(libqt_plugin_la_RES) $(libqt_plugin_la_SHADER)

nodist_libqt_plugin_la_SOURCES += resources.cpp shaders/shaders.cpp
endif

if ENABLE_VLM
nodist_libqt_plugin_la_SOURCES += dialogs/vlm/vlm.moc.cpp
endif

if HAVE_QMLCACHEGEN

nodist_libqt_plugin_la_SOURCES += qmlcache_loader.cpp
libqt_plugin_la_SOURCES += $(libqt_plugin_la_QML)

.qml.cpp:
	$(AM_V_GEN)$(QMLCACHEGEN) --resource=$(srcdir)/vlc.qrc -o $@ $<

.js.cpp:
	$(AM_V_GEN)$(QMLCACHEGEN) --resource=$(srcdir)/vlc.qrc -o $@ $<

.mjs.cpp:
	$(AM_V_GEN)$(QMLCACHEGEN) --resource=$(srcdir)/vlc.qrc -o $@ $<

qml_cache.qrc: vlc.qrc
	$(AM_V_GEN)$(QMLCACHEGEN) --filter-resource-file -o $@ $<

QRCS_EXCEPT_VLC_QRC = $(filter-out vlc.qrc, $(lib_qt_plugin_la_QRC))
QRCS_EXCEPT_VLC_QRC += qml_cache.qrc

resources.cpp: $(QRCS_EXCEPT_VLC_QRC) $(libqt_plugin_la_RES)
	$(AM_V_GEN)$(RCC) --name qml_cache -o $@ $(filter %.qrc, $^)

#output name matters here
qmlcache_loader.cpp: vlc.qrc qml_cache.qrc $(libqt_plugin_la_QML)
	$(AM_V_GEN)$(QMLCACHEGEN) --resource-file-mapping=$<=qml_cache.qrc $< -o $@

else

EXTRA_DIST += $(libqt_plugin_la_QML)

resources.cpp: $(lib_qt_plugin_la_QRC) $(libqt_plugin_la_RES) $(libqt_plugin_la_QML)
	$(AM_V_GEN)$(RCC) --name vlc -o $@ $(filter %.qrc, $^)

endif

if HAVE_QT_GTK
libqt_gtktheme_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(GTK3_CFLAGS)
libqt_gtktheme_CPPFLAGS = $(AM_CPPFLAGS)
libqt_gtktheme_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libqt_gtktheme_plugin_la_LIBADD = $(GTK3_LIBS)
libqt_gtktheme_plugin_la_SOURCES = \
	style/gtkthemeprovider/gtk_compat.cpp \
	style/gtkthemeprovider/gtk_compat.h \
	style/gtkthemeprovider/gtkthemeprovider.cpp \
	style/gtkthemeprovider/gtk_util.cpp \
	style/gtkthemeprovider/gtk_util.h \
	style/gtkthemeprovider/nav_button_provider_gtk.cpp \
	style/gtkthemeprovider/nav_button_provider_gtk.h \
	style/gtkthemeprovider/scoped_gobject.h
if ENABLE_QT
gui_LTLIBRARIES += libqt_gtktheme_plugin.la
endif
endif

if ENABLE_QT
gui_LTLIBRARIES += libqt_plugin.la
BUILT_SOURCES += $(nodist_libqt_plugin_la_SOURCES)
if !HAVE_WIN32
if !HAVE_OS2
pkglibexec_PROGRAMS = vlc-qt-check
all-local: $(builddir)/../../vlc-qt-check

$(builddir)/../../vlc-qt-check: vlc-qt-check
	$(AM_V_GEN)cd ../../ && $(LN_S) -f gui/qt/vlc-qt-check . && chmod +x vlc-qt-check
endif
endif

base_model_test_SOURCES = tests/test_base_model.cpp \
        util/base_model.cpp util/base_model.hpp util/base_model_p.hpp

nodist_base_model_test_SOURCES = util/base_model.moc.cpp \
        util/listcache.moc.cpp \
        util/locallistcacheloader.moc.cpp

base_model_test_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) -fPIC $(CXXFLAGS_qt)
base_model_test_LDADD = $(QT5_PLUGINS_LIBS) $(QT_LIBS) $(LIBS_qt)
check_PROGRAMS = base_model_test
TESTS = base_model_test


QML_LOG_COMPILER = $(builddir)/qml_test -input

if HAVE_QT_QUICK_TEST
qml_test_SOURCES = tests/qml_test.cpp
nodist_qml_test_SOURCES = resources.cpp
if HAVE_QMLCACHEGEN
nodist_qml_test_SOURCES += qmlcache_loader.cpp $(libqt_plugin_la_QML)
endif
qml_test_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) -fPIC $(CXXFLAGS_qt) ${QT_QUICK_TEST_CFLAGS} -DQUICK_TEST_SOURCE_DIR="\"${srcdir}/tests\""
qml_test_LDADD = $(QT_INDIRECT_LIBS) ${QT_QUICK_TEST_LIBS} $(QT_LIBS) $(LIBS_qt)
check_PROGRAMS += qml_test
EXTRA_DIST += tests/tst_FSM.qml
TESTS += tests/tst_FSM.qml
endif
endif
