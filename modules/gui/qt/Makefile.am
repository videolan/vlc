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

SUFFIXES += .ui .h .hpp .moc.cpp .qml

vlc_qt_check_SOURCES = gui/qt/vlc-qt-check.cpp gui/qt/plugins.hpp
vlc_qt_check_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) -fPIC $(CXXFLAGS_qt)
vlc_qt_check_CPPFLAGS = $(AM_CPPFLAGS)
vlc_qt_check_LDADD = $(QT_LIBS) $(LIBS_qt) $(QT5_PLUGINS_LIBS) $(QT5_PLATFORM_PLUGINS_LIBS) $(QT5_XCB_PLUGINS_LIBS) $(QT5_WAYLAND_PLUGINS_LIBS)

libqt_plugin_la_CPPFLAGS = $(AM_CPPFLAGS) \
	-I$(srcdir)/gui/qt -I$(builddir)/gui/qt/ \
	-I$(builddir)/gui/qt/dialogs/extended \
	-I$(builddir)/gui/qt/dialogs/fingerprint \
	-I$(builddir)/gui/qt/dialogs/firstrun \
	-I$(builddir)/gui/qt/dialogs/help \
	-I$(builddir)/gui/qt/dialogs/messages \
	-I$(builddir)/gui/qt/dialogs/open \
	-I$(builddir)/gui/qt/dialogs/podcast \
	-I$(builddir)/gui/qt/dialogs/preferences \
	-I$(builddir)/gui/qt/dialogs/sout \
	-I$(builddir)/gui/qt/dialogs/vlm \
	-DQPNI_HEADER=\<$(QT_VERSION)/QtGui/qpa/qplatformnativeinterface.h\>

libqt_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(QT_CFLAGS) $(CXXFLAGS_qt)
libqt_plugin_la_LIBADD = $(QT_LIBS) $(LIBS_qt) $(LIBM) \
                         $(QT5_PLUGINS_LIBS) $(QT5_PLATFORM_PLUGINS_LIBS)
libqt_plugin_la_LDFLAGS = $(AM_LDFLAGS)
if HAVE_DARWIN
libqt_plugin_la_LDFLAGS += -Wl,-framework,Cocoa
endif
if HAVE_QT5_X11
libqt_plugin_la_CXXFLAGS += $(QT5_X11_CFLAGS) -DQT5_HAS_X11
libqt_plugin_la_LIBADD += $(QT5_X11_LIBS) $(X_LIBS) $(X_PRE_LIB) -lX11
vlc_qt_check_CPPFLAGS += $(QT5_WAYLAND_CFLAGS) -DQT5_HAS_X11
endif

if HAVE_XCB
libqt_plugin_la_CXXFLAGS += -DQT5_HAS_XCB $(XCB_CFLAGS)
libqt_plugin_la_LIBADD += $(XCB_LIBS)
if HAVE_XCB_DAMAGE
if HAVE_XCB_XFIXES
libqt_plugin_la_CXXFLAGS += -DQT5_HAS_X11_COMPOSITOR \
        $(XCB_RENDER_CFLAGS) $(XCB_COMPOSITE_CFLAGS) $(XLCB_DAMAGE_CFLAGS) $(XCB_XFIXES_CFLAGS)
libqt_plugin_la_LIBADD += $(XCB_RENDER_LIBS) $(XCB_COMPOSITE_LIBS) $(XCB_DAMAGE_LIBS) $(XCB_XFIXES_LIBS)
endif
endif
endif

if HAVE_WAYLAND
libqt_plugin_la_CPPFLAGS += -DQT5_HAS_WAYLAND
libqt_plugin_la_CXXFLAGS += $(WAYLAND_CLIENT_CFLAGS)
libqt_plugin_la_LIBADD += $(WAYLAND_CLIENT_LIBS)
endif
if HAVE_QT5_WAYLAND
libqt_plugin_la_CPPFLAGS += $(QT5_WAYLAND_CFLAGS)
libqt_plugin_la_LIBADD += $(QT5_WAYLAND_LIBS)
vlc_qt_check_CPPFLAGS += $(QT5_WAYLAND_CFLAGS) -DQT5_HAS_WAYLAND
vlc_qt_check_LDADD += $(QT5_WAYLAND_LIBS)
endif
if HAVE_WIN32
libqt_plugin_la_CXXFLAGS += $(LIBCOMCXXFLAGS)
libqt_plugin_la_LIBADD += $(LIBCOM) -lcomctl32 -luuid -ld3d11
endif
if UPDATE_CHECK
libqt_plugin_la_CPPFLAGS += -DUPDATE_CHECK
endif
if QT_QML_DEBUG
libqt_plugin_la_CPPFLAGS += -DQT_QML_DEBUG
endif
if HAVE_QT5_GTK
libqt_plugin_la_CPPFLAGS += -DQT5_HAS_GTK
endif
if HAVE_QT5_DECLARATIVE_PRIVATE
libqt_plugin_la_CPPFLAGS += -DQT5_DECLARATIVE_PRIVATE \
	-I$(QT_INCLUDE_DIRECTORY)/QtCore/$(QT_VERSION) \
	-I$(QT_INCLUDE_DIRECTORY)/QtCore/$(QT_VERSION)/QtCore \
	-I$(QT_INCLUDE_DIRECTORY)/QtGui/$(QT_VERSION) \
	-I$(QT_INCLUDE_DIRECTORY)/QtGui/$(QT_VERSION)/QtGui \
	-I$(QT_INCLUDE_DIRECTORY)/QtQml/$(QT_VERSION) \
	-I$(QT_INCLUDE_DIRECTORY)/QtQml/$(QT_VERSION)/QtQml \
	-I$(QT_INCLUDE_DIRECTORY)/QtQuick/$(QT_VERSION) \
	-I$(QT_INCLUDE_DIRECTORY)/QtQuick/$(QT_VERSION)/QtQuick

endif

libqt_plugin_la_SOURCES = \
	gui/qt/qt.cpp gui/qt/qt.hpp gui/qt/plugins.hpp \
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
	gui/qt/dialogs/firstrun/firstrunwizard.cpp gui/qt/dialogs/firstrun/firstrunwizard.hpp \
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
	gui/qt/dialogs/preferences/expert_model.cpp \
	gui/qt/dialogs/preferences/expert_model.hpp \
	gui/qt/dialogs/preferences/expert_view.cpp \
	gui/qt/dialogs/preferences/expert_view.hpp \
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
	gui/qt/dialogs/toolbar/controlbar_profile.hpp \
	gui/qt/dialogs/toolbar/controlbar_profile.cpp \
	gui/qt/dialogs/toolbar/controlbar_profile_model.cpp \
	gui/qt/dialogs/toolbar/controlbar_profile_model.hpp \
	gui/qt/dialogs/vlm/vlm.cpp gui/qt/dialogs/vlm/vlm.hpp \
	gui/qt/dialogs/playlists/playlists.cpp gui/qt/dialogs/playlists/playlists.hpp \
	gui/qt/maininterface/compositor.hpp \
	gui/qt/maininterface/compositor.cpp \
	gui/qt/maininterface/compositor_common.hpp \
	gui/qt/maininterface/compositor_common.cpp \
	gui/qt/maininterface/compositor_dummy.hpp \
	gui/qt/maininterface/compositor_dummy.cpp \
	gui/qt/maininterface/interface_window_handler.cpp \
	gui/qt/maininterface/interface_window_handler.hpp \
	gui/qt/maininterface/mainctx.cpp \
	gui/qt/maininterface/mainctx.hpp \
	gui/qt/maininterface/mainui.cpp \
	gui/qt/maininterface/mainui.hpp \
	gui/qt/maininterface/videosurface.cpp \
	gui/qt/maininterface/videosurface.hpp \
	gui/qt/maininterface/video_window_handler.cpp \
	gui/qt/maininterface/video_window_handler.hpp \
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
	gui/qt/medialibrary/mlevent.hpp \
	gui/qt/medialibrary/mlfolder.cpp \
	gui/qt/medialibrary/mlfolder.hpp \
	gui/qt/medialibrary/mlfoldersmodel.cpp \
	gui/qt/medialibrary/mlfoldersmodel.hpp \
	gui/qt/medialibrary/mlgenre.cpp \
	gui/qt/medialibrary/mlgenre.hpp \
	gui/qt/medialibrary/mlgenremodel.cpp \
	gui/qt/medialibrary/mlgenremodel.hpp \
	gui/qt/medialibrary/mlgroup.cpp \
	gui/qt/medialibrary/mlgroup.hpp \
	gui/qt/medialibrary/mlhelper.cpp \
	gui/qt/medialibrary/mlhelper.hpp \
	gui/qt/medialibrary/mllistcache.cpp \
	gui/qt/medialibrary/mllistcache.hpp \
	gui/qt/medialibrary/mlthreadpool.cpp \
	gui/qt/medialibrary/mlthreadpool.hpp \
	gui/qt/medialibrary/mlqmltypes.hpp \
	gui/qt/medialibrary/mlqueryparams.cpp \
	gui/qt/medialibrary/mlqueryparams.hpp \
	gui/qt/medialibrary/mlrecentsmodel.cpp \
	gui/qt/medialibrary/mlrecentsmodel.hpp \
	gui/qt/medialibrary/mlrecentsvideomodel.cpp \
	gui/qt/medialibrary/mlrecentsvideomodel.hpp \
	gui/qt/medialibrary/mlurlmodel.cpp \
	gui/qt/medialibrary/mlurlmodel.hpp \
	gui/qt/medialibrary/mlvideo.cpp \
	gui/qt/medialibrary/mlvideo.hpp \
	gui/qt/medialibrary/mlvideofoldersmodel.cpp \
	gui/qt/medialibrary/mlvideofoldersmodel.hpp \
	gui/qt/medialibrary/mlvideogroupsmodel.cpp \
	gui/qt/medialibrary/mlvideogroupsmodel.hpp \
	gui/qt/medialibrary/mlvideomodel.cpp \
	gui/qt/medialibrary/mlvideomodel.hpp \
	gui/qt/medialibrary/mlplaylist.cpp \
	gui/qt/medialibrary/mlplaylist.hpp \
	gui/qt/medialibrary/mlplaylistlistmodel.cpp \
	gui/qt/medialibrary/mlplaylistlistmodel.hpp \
	gui/qt/medialibrary/mlplaylistmedia.cpp \
	gui/qt/medialibrary/mlplaylistmedia.hpp \
	gui/qt/medialibrary/mlplaylistmodel.cpp \
	gui/qt/medialibrary/mlplaylistmodel.hpp \
	gui/qt/medialibrary/thumbnailcollector.hpp \
	gui/qt/medialibrary/thumbnailcollector.cpp \
	gui/qt/medialibrary/mlcustomcover.hpp \
	gui/qt/medialibrary/mlcustomcover.cpp \
	gui/qt/menus/custom_menus.cpp \
	gui/qt/menus/custom_menus.hpp \
	gui/qt/menus/qml_menu_wrapper.cpp \
	gui/qt/menus/qml_menu_wrapper.hpp \
	gui/qt/menus/menus.cpp gui/qt/menus/menus.hpp \
	gui/qt/network/mediatreelistener.cpp \
	gui/qt/network/mediatreelistener.hpp \
	gui/qt/network/networkdevicemodel.cpp \
	gui/qt/network/networkdevicemodel.hpp \
	gui/qt/network/networksourcesmodel.cpp \
	gui/qt/network/networksourcesmodel.hpp \
	gui/qt/network/networkmediamodel.cpp \
	gui/qt/network/networkmediamodel.hpp \
	gui/qt/network/servicesdiscoverymodel.cpp \
	gui/qt/network/servicesdiscoverymodel.hpp \
	gui/qt/network/standardpathmodel.cpp \
	gui/qt/network/standardpathmodel.hpp \
	gui/qt/style/qtthemeprovider.hpp \
	gui/qt/style/colorcontext.cpp \
	gui/qt/style/colorcontext.hpp \
	gui/qt/style/systempalette.cpp \
	gui/qt/style/systempalette.hpp \
	gui/qt/style/defaultthemeproviders.hpp \
	gui/qt/style/systempalettethemeprovider.cpp \
	gui/qt/player/input_models.cpp gui/qt/player/input_models.hpp \
	gui/qt/player/player_controller.cpp gui/qt/player/player_controller.hpp gui/qt/player/player_controller_p.hpp \
	gui/qt/player/player_controlbar_model.cpp gui/qt/player/player_controlbar_model.hpp \
	gui/qt/player/control_list_model.cpp gui/qt/player/control_list_model.hpp \
	gui/qt/player/control_list_filter.cpp \
	gui/qt/player/control_list_filter.hpp \
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
	gui/qt/util/asynctask.hpp \
	gui/qt/util/audio_device_model.cpp  \
	gui/qt/util/audio_device_model.hpp \
	gui/qt/util/cliplistmodel.cpp \
	gui/qt/util/cliplistmodel.hpp \
	gui/qt/util/color_scheme_model.cpp gui/qt/util/color_scheme_model.hpp \
	gui/qt/util/color_svg_image_provider.cpp gui/qt/util/color_svg_image_provider.hpp \
	gui/qt/util/covergenerator.cpp \
	gui/qt/util/covergenerator.hpp \
	gui/qt/util/imageluminanceextractor.cpp gui/qt/util/imageluminanceextractor.hpp \
	gui/qt/util/csdbuttonmodel.cpp gui/qt/util/csdbuttonmodel.hpp \
	gui/qt/util/imagehelper.cpp gui/qt/util/imagehelper.hpp \
	gui/qt/util/i18n.cpp gui/qt/util/i18n.hpp \
	gui/qt/util/keyhelper.cpp gui/qt/util/keyhelper.hpp \
	gui/qt/util/listcacheloader.hpp \
	gui/qt/util/navigation_history.cpp gui/qt/util/navigation_history.hpp \
	gui/qt/util/item_key_event_filter.cpp \
	gui/qt/util/item_key_event_filter.hpp \
	gui/qt/util/flickable_scroll_handler.cpp \
	gui/qt/util/flickable_scroll_handler.hpp \
	gui/qt/util/hover_handler_rev11.cpp \
	gui/qt/util/hover_handler_rev11.hpp \
	gui/qt/util/qt_dirs.cpp gui/qt/util/qt_dirs.hpp \
	gui/qt/util/qvlcapp.hpp \
	gui/qt/util/proxycolumnmodel.hpp \
	gui/qt/util/registry.cpp gui/qt/util/registry.hpp \
	gui/qt/util/renderer_manager.cpp gui/qt/util/renderer_manager.hpp \
	gui/qt/util/selectable_list_model.cpp \
	gui/qt/util/selectable_list_model.hpp \
	gui/qt/util/singleton.hpp \
	gui/qt/util/sortfilterproxymodel.cpp \
	gui/qt/util/sortfilterproxymodel.hpp \
	gui/qt/util/soutchain.cpp gui/qt/util/soutchain.hpp \
	gui/qt/util/validators.cpp gui/qt/util/validators.hpp \
	gui/qt/util/varcommon_p.hpp \
	gui/qt/util/varchoicemodel.cpp  gui/qt/util/varchoicemodel.hpp \
	gui/qt/util/variables.cpp gui/qt/util/variables.hpp \
	gui/qt/util/vlctick.cpp \
	gui/qt/util/vlctick.hpp \
	gui/qt/util/qmlinputitem.hpp \
	gui/qt/util/mouse_event_filter.cpp \
	gui/qt/util/mouse_event_filter.hpp \
	gui/qt/util/effects_image_provider.cpp \
	gui/qt/util/effects_image_provider.hpp \
	gui/qt/util/qsgroundedrectangularimagenode.cpp \
	gui/qt/util/qsgroundedrectangularimagenode.hpp \
	gui/qt/widgets/native/animators.cpp \
	gui/qt/widgets/native/animators.hpp \
	gui/qt/widgets/native/customwidgets.cpp gui/qt/widgets/native/customwidgets.hpp \
	gui/qt/widgets/native/csdthemeimage.cpp \
	gui/qt/widgets/native/csdthemeimage.hpp \
	gui/qt/widgets/native/interface_widgets.cpp \
	gui/qt/widgets/native/interface_widgets.hpp \
	gui/qt/widgets/native/navigation_attached.cpp \
	gui/qt/widgets/native/navigation_attached.hpp \
	gui/qt/widgets/native/mlfolderseditor.cpp \
	gui/qt/widgets/native/mlfolderseditor.hpp \
	gui/qt/widgets/native/qvlcframe.cpp \
	gui/qt/widgets/native/qvlcframe.hpp \
	gui/qt/widgets/native/roundimage.cpp gui/qt/widgets/native/roundimage.hpp \
	gui/qt/widgets/native/searchlineedit.cpp gui/qt/widgets/native/searchlineedit.hpp \
	gui/qt/widgets/native/viewblockingrectangle.cpp gui/qt/widgets/native/viewblockingrectangle.hpp
if HAVE_WIN32
libqt_plugin_la_SOURCES += \
	gui/qt/maininterface/mainctx_win32.cpp \
	gui/qt/maininterface/mainctx_win32.hpp \
	gui/qt/maininterface/compositor_win7.cpp \
	gui/qt/maininterface/compositor_win7.hpp \
	gui/qt/style/windowsthemeprovider.cpp

if HAVE_DCOMP
libqt_plugin_la_SOURCES += \
	gui/qt/maininterface/compositor_dcomp.cpp \
	gui/qt/maininterface/compositor_dcomp.hpp \
	gui/qt/maininterface/compositor_dcomp_error.hpp \
	gui/qt/maininterface/compositor_dcomp_acrylicsurface.hpp \
	gui/qt/maininterface/compositor_dcomp_acrylicsurface.cpp \
	gui/qt/maininterface/compositor_dcomp_uisurface.cpp \
	gui/qt/maininterface/compositor_dcomp_uisurface.hpp
endif

endif

if HAVE_XCB
if HAVE_XCB_DAMAGE
if HAVE_XCB_XFIXES
libqt_plugin_la_SOURCES += \
	gui/qt/maininterface/compositor_x11.cpp \
	gui/qt/maininterface/compositor_x11.hpp \
	gui/qt/maininterface/compositor_x11_renderclient.cpp \
	gui/qt/maininterface/compositor_x11_renderclient.hpp \
	gui/qt/maininterface/compositor_x11_renderwindow.cpp \
	gui/qt/maininterface/compositor_x11_renderwindow.hpp \
	gui/qt/maininterface/compositor_x11_uisurface.cpp \
	gui/qt/maininterface/compositor_x11_uisurface.hpp \
	gui/qt/maininterface/compositor_x11_utils.cpp \
	gui/qt/maininterface/compositor_x11_utils.hpp
endif
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
	gui/qt/dialogs/firstrun/firstrunwizard.moc.cpp \
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
	gui/qt/dialogs/preferences/expert_model.moc.cpp \
	gui/qt/dialogs/preferences/expert_view.moc.cpp \
	gui/qt/dialogs/preferences/preferences.moc.cpp \
	gui/qt/dialogs/preferences/preferences_widgets.moc.cpp \
	gui/qt/dialogs/preferences/simple_preferences.moc.cpp \
	gui/qt/dialogs/sout/convert.moc.cpp \
	gui/qt/dialogs/sout/profile_selector.moc.cpp \
	gui/qt/dialogs/sout/sout.moc.cpp \
	gui/qt/dialogs/sout/sout_widgets.moc.cpp \
	gui/qt/dialogs/toolbar/controlbar_profile.moc.cpp \
	gui/qt/dialogs/toolbar/controlbar_profile_model.moc.cpp \
	gui/qt/dialogs/playlists/playlists.moc.cpp \
	gui/qt/maininterface/compositor.moc.cpp \
	gui/qt/maininterface/compositor_common.moc.cpp \
	gui/qt/maininterface/compositor_dummy.moc.cpp \
	gui/qt/maininterface/interface_window_handler.moc.cpp \
	gui/qt/maininterface/mainctx.moc.cpp \
	gui/qt/maininterface/mainui.moc.cpp \
	gui/qt/maininterface/videosurface.moc.cpp \
	gui/qt/maininterface/video_window_handler.moc.cpp \
	gui/qt/medialibrary/medialib.moc.cpp \
	gui/qt/medialibrary/mlalbum.moc.cpp \
	gui/qt/medialibrary/mlalbummodel.moc.cpp \
	gui/qt/medialibrary/mlalbumtrack.moc.cpp \
	gui/qt/medialibrary/mlalbumtrackmodel.moc.cpp \
	gui/qt/medialibrary/mlartist.moc.cpp \
	gui/qt/medialibrary/mlartistmodel.moc.cpp \
        gui/qt/medialibrary/mlbookmarkmodel.moc.cpp \
	gui/qt/medialibrary/mlbasemodel.moc.cpp \
	gui/qt/medialibrary/mlfoldersmodel.moc.cpp \
	gui/qt/medialibrary/mlgenremodel.moc.cpp \
	gui/qt/medialibrary/mllistcache.moc.cpp \
	gui/qt/medialibrary/mlthreadpool.moc.cpp \
	gui/qt/medialibrary/mlqmltypes.moc.cpp \
	gui/qt/medialibrary/mlrecentsmodel.moc.cpp \
	gui/qt/medialibrary/mlrecentsvideomodel.moc.cpp \
	gui/qt/medialibrary/mlurlmodel.moc.cpp \
	gui/qt/medialibrary/mlvideo.moc.cpp \
	gui/qt/medialibrary/mlvideomodel.moc.cpp \
	gui/qt/medialibrary/mlplaylistlistmodel.moc.cpp \
	gui/qt/medialibrary/mlplaylistmodel.moc.cpp \
	gui/qt/medialibrary/mlvideofoldersmodel.moc.cpp \
	gui/qt/medialibrary/mlvideogroupsmodel.moc.cpp \
	gui/qt/medialibrary/thumbnailcollector.moc.cpp \
	gui/qt/menus/custom_menus.moc.cpp \
	gui/qt/menus/qml_menu_wrapper.moc.cpp \
	gui/qt/menus/menus.moc.cpp \
	gui/qt/network/networkdevicemodel.moc.cpp \
	gui/qt/network/networksourcesmodel.moc.cpp \
	gui/qt/network/networkmediamodel.moc.cpp \
	gui/qt/network/servicesdiscoverymodel.moc.cpp \
	gui/qt/network/standardpathmodel.moc.cpp \
	gui/qt/style/colorcontext.moc.cpp \
	gui/qt/style/systempalette.moc.cpp \
	gui/qt/player/input_models.moc.cpp \
	gui/qt/player/player_controller.moc.cpp \
	gui/qt/player/player_controlbar_model.moc.cpp \
	gui/qt/player/control_list_model.moc.cpp \
	gui/qt/player/control_list_filter.moc.cpp \
	gui/qt/playlist/playlist_common.moc.cpp \
	gui/qt/playlist/playlist_controller.moc.cpp \
	gui/qt/playlist/playlist_item.moc.cpp \
	gui/qt/playlist/playlist_model.moc.cpp \
	gui/qt/util/asynctask.moc.cpp \
	gui/qt/util/audio_device_model.moc.cpp \
	gui/qt/util/cliplistmodel.moc.cpp \
	gui/qt/util/color_scheme_model.moc.cpp \
	gui/qt/util/color_svg_image_provider.moc.cpp \
	gui/qt/util/imageluminanceextractor.moc.cpp \
	gui/qt/util/csdbuttonmodel.moc.cpp \
	gui/qt/util/i18n.moc.cpp \
	gui/qt/util/keyhelper.moc.cpp \
	gui/qt/util/navigation_history.moc.cpp \
	gui/qt/util/item_key_event_filter.moc.cpp \
	gui/qt/util/mouse_event_filter.moc.cpp \
	gui/qt/util/effects_image_provider.moc.cpp \
	gui/qt/util/flickable_scroll_handler.moc.cpp \
	gui/qt/util/hover_handler_rev11.moc.cpp \
	gui/qt/util/qvlcapp.moc.cpp \
	gui/qt/util/renderer_manager.moc.cpp \
	gui/qt/util/selectable_list_model.moc.cpp \
	gui/qt/util/sortfilterproxymodel.moc.cpp \
	gui/qt/util/validators.moc.cpp \
	gui/qt/util/varchoicemodel.moc.cpp \
	gui/qt/util/variables.moc.cpp \
	gui/qt/util/vlctick.moc.cpp \
	gui/qt/util/qmlinputitem.moc.cpp \
	gui/qt/util/qsgroundedrectangularimagenode.moc.cpp \
	gui/qt/widgets/native/animators.moc.cpp \
	gui/qt/widgets/native/csdthemeimage.moc.cpp \
	gui/qt/widgets/native/customwidgets.moc.cpp \
	gui/qt/widgets/native/interface_widgets.moc.cpp \
	gui/qt/widgets/native/navigation_attached.moc.cpp \
	gui/qt/widgets/native/mlfolderseditor.moc.cpp \
	gui/qt/widgets/native/roundimage.moc.cpp \
	gui/qt/widgets/native/searchlineedit.moc.cpp \
	gui/qt/widgets/native/viewblockingrectangle.moc.cpp

if HAVE_WIN32
nodist_libqt_plugin_la_SOURCES += gui/qt/maininterface/mainctx_win32.moc.cpp \
	gui/qt/maininterface/compositor_win7.moc.cpp

if HAVE_DCOMP
nodist_libqt_plugin_la_SOURCES += \
	gui/qt/maininterface/compositor_dcomp_acrylicsurface.moc.cpp \
	gui/qt/maininterface/compositor_dcomp.moc.cpp \
	gui/qt/maininterface/compositor_dcomp_uisurface.moc.cpp
endif

endif

if HAVE_XCB
if HAVE_XCB_DAMAGE
if HAVE_XCB_XFIXES
nodist_libqt_plugin_la_SOURCES += \
	gui/qt/maininterface/compositor_x11.moc.cpp \
	gui/qt/maininterface/compositor_x11_renderclient.moc.cpp \
	gui/qt/maininterface/compositor_x11_renderwindow.moc.cpp \
	gui/qt/maininterface/compositor_x11_uisurface.moc.cpp \
	gui/qt/maininterface/compositor_x11_utils.moc.cpp
endif
endif
endif

nodist_libqt_plugin_la_SOURCES += \
	gui/qt/dialogs/extended/ui_equalizer.h \
	gui/qt/dialogs/extended/ui_video_effects.h \
	gui/qt/dialogs/fingerprint/ui_fingerprintdialog.h \
	gui/qt/dialogs/firstrun/ui_firstrunwizard.h \
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
	gui/qt/dialogs/preferences/ui_sprefs_medialibrary.h \
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
	$(QT_CFLAGS) \
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
	gui/qt/dialogs/firstrun/firstrunwizard.ui \
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
	gui/qt/dialogs/preferences/sprefs_medialibrary.ui \
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
	gui/qt/pixmaps/addons/addon_default.svg \
	gui/qt/pixmaps/addons/addon_broken.svg \
	gui/qt/pixmaps/addons/addon_blue.svg \
	gui/qt/pixmaps/addons/addon_cyan.svg \
	gui/qt/pixmaps/addons/addon_green.svg \
	gui/qt/pixmaps/addons/addon_magenta.svg \
	gui/qt/pixmaps/addons/addon_red.svg \
	gui/qt/pixmaps/addons/addon_yellow.svg \
	gui/qt/pixmaps/addons/addon_score.svg \
	gui/qt/pixmaps/icons/active_indicator.svg \
	gui/qt/pixmaps/icons/add.svg \
	gui/qt/pixmaps/icons/album_cover.svg \
	gui/qt/pixmaps/icons/aspect_ratio.svg \
	gui/qt/pixmaps/icons/atob.svg \
	gui/qt/pixmaps/icons/audiosub.svg \
	gui/qt/pixmaps/icons/back.svg \
	gui/qt/pixmaps/icons/bookmark.svg \
	gui/qt/pixmaps/icons/breadcrumb_prev.svg \
	gui/qt/pixmaps/icons/breadcrumb_sep.svg \
	gui/qt/pixmaps/icons/check.svg \
	gui/qt/pixmaps/icons/clear.svg \
	gui/qt/pixmaps/icons/close.svg \
	gui/qt/pixmaps/icons/defullscreen.svg \
	gui/qt/pixmaps/icons/del.svg \
	gui/qt/pixmaps/icons/download.svg \
	gui/qt/pixmaps/icons/dropzone.svg \
	gui/qt/pixmaps/icons/dvd_menu.svg \
	gui/qt/pixmaps/icons/dvd_next.svg \
	gui/qt/pixmaps/icons/dvd_prev.svg \
	gui/qt/pixmaps/icons/eject.svg \
	gui/qt/pixmaps/icons/ellipsis.svg \
	gui/qt/pixmaps/icons/enqueue.svg \
	gui/qt/pixmaps/icons/expand.svg \
	gui/qt/pixmaps/icons/extended.svg \
	gui/qt/pixmaps/icons/faster.svg \
	gui/qt/pixmaps/icons/frame-by-frame.svg \
	gui/qt/pixmaps/icons/fullscreen.svg \
	gui/qt/pixmaps/icons/grid.svg \
	gui/qt/pixmaps/icons/history.svg \
	gui/qt/pixmaps/icons/home.svg \
	gui/qt/pixmaps/icons/info.svg \
	gui/qt/pixmaps/icons/list.svg \
	gui/qt/pixmaps/icons/menu.svg \
	gui/qt/pixmaps/icons/multiselect.svg \
	gui/qt/pixmaps/icons/next.svg \
	gui/qt/pixmaps/icons/pause.svg \
	gui/qt/pixmaps/icons/pip.svg \
	gui/qt/pixmaps/icons/playlist_clear.svg \
	gui/qt/pixmaps/icons/playlist.svg \
	gui/qt/pixmaps/icons/play_outline.svg \
	gui/qt/pixmaps/icons/play_reverse.svg \
	gui/qt/pixmaps/icons/play.svg \
	gui/qt/pixmaps/icons/podcast.svg \
	gui/qt/pixmaps/icons/previous.svg \
	gui/qt/pixmaps/icons/profile_new.svg \
	gui/qt/pixmaps/icons/record.svg \
	gui/qt/pixmaps/icons/remove.svg \
	gui/qt/pixmaps/icons/renderer.svg \
	gui/qt/pixmaps/icons/repeat_all.svg \
	gui/qt/pixmaps/icons/repeat_one.svg \
	gui/qt/pixmaps/icons/search.svg \
	gui/qt/pixmaps/icons/shuffle_on.svg \
	gui/qt/pixmaps/icons/skip_back.svg \
	gui/qt/pixmaps/icons/skip_for.svg \
	gui/qt/pixmaps/icons/slower.svg \
	gui/qt/pixmaps/icons/snapshot.svg \
	gui/qt/pixmaps/icons/space.svg \
	gui/qt/pixmaps/icons/stop.svg \
	gui/qt/pixmaps/icons/stream.svg \
	gui/qt/pixmaps/icons/sync.svg \
	gui/qt/pixmaps/icons/time.svg \
	gui/qt/pixmaps/icons/topbar_discover.svg \
	gui/qt/pixmaps/icons/topbar_filter.svg \
	gui/qt/pixmaps/icons/topbar_music.svg \
	gui/qt/pixmaps/icons/topbar_network.svg \
	gui/qt/pixmaps/icons/topbar_sort.svg \
	gui/qt/pixmaps/icons/topbar_video.svg \
	gui/qt/pixmaps/icons/tv.svg \
	gui/qt/pixmaps/icons/tvtelx.svg \
	gui/qt/pixmaps/icons/visualization.svg \
	gui/qt/pixmaps/icons/volume_high.svg \
	gui/qt/pixmaps/icons/volume_low.svg \
	gui/qt/pixmaps/icons/volume_medium.svg \
	gui/qt/pixmaps/icons/volume_muted.svg \
	gui/qt/pixmaps/icons/volume_zero.svg \
	gui/qt/pixmaps/icons/window_close.svg \
	gui/qt/pixmaps/icons/window_maximize.svg \
	gui/qt/pixmaps/icons/window_minimize.svg \
	gui/qt/pixmaps/icons/window_restore.svg \
	gui/qt/pixmaps/menu/add.svg \
	gui/qt/pixmaps/menu/capture-card.svg \
	gui/qt/pixmaps/menu/clear.svg \
	gui/qt/pixmaps/menu/disc.svg \
	gui/qt/pixmaps/menu/download.svg \
	gui/qt/pixmaps/menu/eject.svg \
	gui/qt/pixmaps/menu/exit.svg \
	gui/qt/pixmaps/menu/faster.svg \
	gui/qt/pixmaps/menu/file.svg \
	gui/qt/pixmaps/menu/folder.svg \
	gui/qt/pixmaps/menu/help.svg \
	gui/qt/pixmaps/menu/info.svg \
	gui/qt/pixmaps/menu/messages.svg \
	gui/qt/pixmaps/menu/movie.svg \
	gui/qt/pixmaps/menu/music.svg \
	gui/qt/pixmaps/menu/network.svg \
	gui/qt/pixmaps/menu/next.svg \
	gui/qt/pixmaps/menu/pause.svg \
	gui/qt/pixmaps/menu/play.svg \
	gui/qt/pixmaps/menu/playlist.svg \
	gui/qt/pixmaps/menu/preferences.svg \
	gui/qt/pixmaps/menu/previous.svg \
	gui/qt/pixmaps/menu/profile_new.svg \
	gui/qt/pixmaps/menu/record.svg \
	gui/qt/pixmaps/menu/remove.svg \
	gui/qt/pixmaps/menu/repeat_all.svg \
	gui/qt/pixmaps/menu/repeat_off.svg \
	gui/qt/pixmaps/menu/repeat_one.svg \
	gui/qt/pixmaps/menu/search_clear.svg \
	gui/qt/pixmaps/menu/settings.svg \
	gui/qt/pixmaps/menu/skip_for.svg \
	gui/qt/pixmaps/menu/skip_back.svg \
	gui/qt/pixmaps/menu/shuffle_on.svg \
	gui/qt/pixmaps/menu/slower.svg \
	gui/qt/pixmaps/menu/stop.svg \
	gui/qt/pixmaps/menu/stream.svg \
	gui/qt/pixmaps/menu/sync.svg \
	gui/qt/pixmaps/menu/update.svg \
	gui/qt/pixmaps/menu/volume-high.svg \
	gui/qt/pixmaps/menu/volume-low.svg \
	gui/qt/pixmaps/menu/volume-muted.svg \
	gui/qt/pixmaps/misc/cone.svg \
	gui/qt/pixmaps/misc/new_indicator.svg \
	gui/qt/pixmaps/misc/play_button.svg \
	gui/qt/pixmaps/misc/play_shadow.png \
	gui/qt/pixmaps/misc/theme_dark.svg \
	gui/qt/pixmaps/misc/theme_daynight.svg \
	gui/qt/pixmaps/misc/theme_light.svg \
	gui/qt/pixmaps/misc/wait1.svg \
	gui/qt/pixmaps/misc/wait2.svg \
	gui/qt/pixmaps/misc/wait3.svg \
	gui/qt/pixmaps/misc/wait4.svg \
	gui/qt/pixmaps/placeholder/add_service.svg \
	gui/qt/pixmaps/placeholder/noart.png \
	gui/qt/pixmaps/placeholder/noart_album.svg \
	gui/qt/pixmaps/placeholder/noart_albumCover.svg \
	gui/qt/pixmaps/placeholder/noart_artist.svg \
	gui/qt/pixmaps/placeholder/noart_artistCover.svg \
	gui/qt/pixmaps/placeholder/noart_artist_small.svg \
	gui/qt/pixmaps/placeholder/noart_videoCover.svg \
	gui/qt/pixmaps/prefs/advprefs_audio.svg \
	gui/qt/pixmaps/prefs/advprefs_codec.svg \
	gui/qt/pixmaps/prefs/advprefs_extended.svg \
	gui/qt/pixmaps/prefs/advprefs_intf.svg \
	gui/qt/pixmaps/prefs/advprefs_playlist.svg \
	gui/qt/pixmaps/prefs/advprefs_sout.svg \
	gui/qt/pixmaps/prefs/advprefs_video.svg \
	gui/qt/pixmaps/prefs/sample_complete.png \
	gui/qt/pixmaps/prefs/sample_minimal.png \
	gui/qt/pixmaps/prefs/sample_skins.png \
	gui/qt/pixmaps/prefs/spref_audio.png \
	gui/qt/pixmaps/prefs/spref_hotkeys.png \
	gui/qt/pixmaps/prefs/spref_input.png \
	gui/qt/pixmaps/prefs/spref_interface.png \
	gui/qt/pixmaps/prefs/spref_medialibrary.png \
	gui/qt/pixmaps/prefs/spref_subtitles.png \
	gui/qt/pixmaps/prefs/spref_video.png \
	gui/qt/pixmaps/sd/appletrailers.svg \
	gui/qt/pixmaps/sd/assembleenationale.svg \
	gui/qt/pixmaps/sd/capture-card.svg \
	gui/qt/pixmaps/sd/directory.svg \
	gui/qt/pixmaps/sd/disc.svg \
	gui/qt/pixmaps/sd/file.svg \
	gui/qt/pixmaps/sd/fmc.svg \
	gui/qt/pixmaps/sd/frenchtv.svg \
	gui/qt/pixmaps/sd/harddisk.svg \
	gui/qt/pixmaps/sd/icecast.svg \
	gui/qt/pixmaps/sd/jamendo.svg \
	gui/qt/pixmaps/sd/katsomo.svg \
	gui/qt/pixmaps/sd/lan.svg \
	gui/qt/pixmaps/sd/library.svg \
	gui/qt/pixmaps/sd/metachannels.svg \
	gui/qt/pixmaps/sd/mtp.svg \
	gui/qt/pixmaps/sd/network.svg \
	gui/qt/pixmaps/sd/node.svg \
	gui/qt/pixmaps/sd/pictures.svg \
	gui/qt/pixmaps/sd/playlist.svg \
	gui/qt/pixmaps/sd/podcast.svg \
	gui/qt/pixmaps/sd/screen.svg \
	gui/qt/pixmaps/sd/stream.svg \
	gui/qt/pixmaps/sd/unknown.svg

if HAVE_WIN32
libqt_plugin_la_RES += gui/qt/pixmaps/windows-theme.conf \
	gui/qt/pixmaps/win7/pause.svg \
	gui/qt/pixmaps/win7/prev.svg \
	gui/qt/pixmaps/win7/next.svg \
	gui/qt/pixmaps/win7/play.svg
endif

#QML and JS resources
libqt_plugin_la_QML = \
	gui/qt/dialogs/dialogs/qml/CustomDialog.qml \
	gui/qt/dialogs/dialogs/qml/Dialogs.qml \
	gui/qt/dialogs/dialogs/qml/ModalDialog.qml \
	gui/qt/dialogs/dialogs/qml/WindowDialog.qml \
	gui/qt/dialogs/toolbar/qml/EditorDNDDelegate.qml \
	gui/qt/dialogs/toolbar/qml/EditorDNDView.qml\
	gui/qt/dialogs/toolbar/qml/EditorDummyButton.qml \
	gui/qt/dialogs/toolbar/qml/EditorTabButton.qml \
	gui/qt/dialogs/toolbar/qml/ToolbarEditor.qml \
	gui/qt/dialogs/toolbar/qml/ToolbarEditorDialog.qml \
	gui/qt/dialogs/toolbar/qml/ToolbarEditorButtonList.qml \
	gui/qt/maininterface/qml/BannerSources.qml \
	gui/qt/maininterface/qml/MainInterface.qml \
	gui/qt/maininterface/qml/MainDisplay.qml \
	gui/qt/maininterface/qml/MainGridView.qml \
	gui/qt/maininterface/qml/MainTableView.qml \
	gui/qt/maininterface/qml/NoMedialibHome.qml \
	gui/qt/medialibrary/qml/ArtistTopBanner.qml \
	gui/qt/medialibrary/qml/AudioGridItem.qml \
	gui/qt/medialibrary/qml/EmptyLabel.qml \
	gui/qt/medialibrary/qml/EmptyLabelButton.qml \
	gui/qt/medialibrary/qml/EmptyLabelHint.qml \
	gui/qt/medialibrary/qml/MediaGroupDisplay.qml \
	gui/qt/medialibrary/qml/MusicAlbums.qml \
	gui/qt/medialibrary/qml/MusicAlbumsDisplay.qml \
	gui/qt/medialibrary/qml/MusicAlbumsGridExpandDelegate.qml \
	gui/qt/medialibrary/qml/MusicArtist.qml \
	gui/qt/medialibrary/qml/MusicArtistDelegate.qml \
	gui/qt/medialibrary/qml/MusicArtistsAlbums.qml \
	gui/qt/medialibrary/qml/MusicAllArtists.qml \
	gui/qt/medialibrary/qml/MusicArtistsDisplay.qml \
	gui/qt/medialibrary/qml/MusicDisplay.qml \
	gui/qt/medialibrary/qml/MusicGenres.qml \
	gui/qt/medialibrary/qml/MusicGenresDisplay.qml \
	gui/qt/medialibrary/qml/MusicPlaylistsDisplay.qml \
	gui/qt/medialibrary/qml/MusicTrackListDisplay.qml \
	gui/qt/medialibrary/qml/MusicTracksDisplay.qml \
	gui/qt/medialibrary/qml/UrlListDisplay.qml \
	gui/qt/medialibrary/qml/VideoDisplay.qml \
	gui/qt/medialibrary/qml/VideoAll.qml \
	gui/qt/medialibrary/qml/VideoAllDisplay.qml \
	gui/qt/medialibrary/qml/VideoAllSubDisplay.qml \
	gui/qt/medialibrary/qml/PlaylistMediaList.qml \
	gui/qt/medialibrary/qml/PlaylistMedia.qml \
	gui/qt/medialibrary/qml/PlaylistMediaDisplay.qml \
	gui/qt/medialibrary/qml/VideoPlaylistsDisplay.qml \
	gui/qt/medialibrary/qml/VideoDisplayRecentVideos.qml \
	gui/qt/medialibrary/qml/VideoGridItem.qml \
	gui/qt/medialibrary/qml/VideoInfoExpandPanel.qml \
	gui/qt/medialibrary/qml/VideoListDisplay.qml \
	gui/qt/menus/qml/GlobalShortcuts.qml \
	gui/qt/menus/qml/GlobalShortcutsMedialib.qml \
	gui/qt/menus/qml/Menubar.qml \
	gui/qt/menus/qml/ShortcutExt.qml \
	gui/qt/network/qml/AddressbarButton.qml \
	gui/qt/network/qml/BrowseDeviceHeader.qml \
	gui/qt/network/qml/BrowseDeviceView.qml \
	gui/qt/network/qml/BrowseDisplay.qml \
	gui/qt/network/qml/BrowseHomeDisplay.qml \
	gui/qt/network/qml/BrowseTreeDisplay.qml \
	gui/qt/network/qml/DiscoverDisplay.qml \
	gui/qt/network/qml/DiscoverUrlDisplay.qml \
	gui/qt/network/qml/NetworkAddressbar.qml \
	gui/qt/network/qml/NetworkCustomCover.qml \
	gui/qt/network/qml/NetworkGridItem.qml \
	gui/qt/network/qml/NetworkThumbnailItem.qml \
	gui/qt/network/qml/ServicesHomeDisplay.qml \
	gui/qt/network/qml/ServicesManage.qml \
	gui/qt/network/qml/ServicesSources.qml \
	gui/qt/player/qml/qmldir \
        gui/qt/player/qml/Bookmarks.qml \
	gui/qt/player/qml/ControlBar.qml \
	gui/qt/player/qml/ControlbarControls.qml \
	gui/qt/player/qml/PlaybackSpeed.qml \
	gui/qt/player/qml/MiniPlayer.qml \
	gui/qt/player/qml/PIPPlayer.qml \
	gui/qt/player/qml/Player.qml \
	gui/qt/player/qml/PlayerBlurredBackground.qml \
	gui/qt/player/qml/PlayerBlurredBackground.frag \
	gui/qt/player/qml/PlayerControlLayout.qml \
	gui/qt/player/qml/PlayerPlaylistVisibilityFSM.qml \
	gui/qt/player/qml/ResumeDialog.qml \
	gui/qt/player/qml/SliderBar.qml \
	gui/qt/player/qml/TopBar.qml \
	gui/qt/player/qml/TracksMenu.qml \
	gui/qt/player/qml/TracksPage.qml \
	gui/qt/player/qml/TracksPageAudio.qml \
	gui/qt/player/qml/TracksPageSpeed.qml \
	gui/qt/player/qml/TracksPageSubtitle.qml \
	gui/qt/player/qml/ControlLayout.qml \
	gui/qt/player/qml/controlbarcontrols/HighResolutionTimeWidget.qml \
	gui/qt/player/qml/controlbarcontrols/ArtworkInfoWidget.qml \
	gui/qt/player/qml/controlbarcontrols/AspectRatioWidget.qml \
	gui/qt/player/qml/controlbarcontrols/AtoBButton.qml \
	gui/qt/player/qml/controlbarcontrols/BackButton.qml \
	gui/qt/player/qml/controlbarcontrols/BookmarkButton.qml \
	gui/qt/player/qml/controlbarcontrols/ChapterNextButton.qml \
	gui/qt/player/qml/controlbarcontrols/ChapterPreviousButton.qml \
	gui/qt/player/qml/controlbarcontrols/ControlButtonPopup.qml \
	gui/qt/player/qml/controlbarcontrols/DvdMenuButton.qml \
	gui/qt/player/qml/controlbarcontrols/ExpandingSpacerWidget.qml \
	gui/qt/player/qml/controlbarcontrols/ExtendedSettingsButton.qml \
	gui/qt/player/qml/controlbarcontrols/FasterButton.qml \
	gui/qt/player/qml/controlbarcontrols/FrameButton.qml \
	gui/qt/player/qml/controlbarcontrols/FullscreenButton.qml \
	gui/qt/player/qml/controlbarcontrols/InfoButton.qml \
	gui/qt/player/qml/controlbarcontrols/LangButton.qml \
	gui/qt/player/qml/controlbarcontrols/LoopButton.qml \
	gui/qt/player/qml/controlbarcontrols/MenuButton.qml \
	gui/qt/player/qml/controlbarcontrols/NavigationWidget.qml \
	gui/qt/player/qml/controlbarcontrols/NextButton.qml \
	gui/qt/player/qml/controlbarcontrols/OpenButton.qml \
	gui/qt/player/qml/controlbarcontrols/PlaybackSpeedButton.qml \
	gui/qt/player/qml/controlbarcontrols/PlayButton.qml \
	gui/qt/player/qml/controlbarcontrols/PlayerSwitchButton.qml \
	gui/qt/player/qml/controlbarcontrols/PlaylistButton.qml \
	gui/qt/player/qml/controlbarcontrols/PreviousButton.qml \
	gui/qt/player/qml/controlbarcontrols/ProgramButton.qml \
	gui/qt/player/qml/controlbarcontrols/QuitButton.qml \
	gui/qt/player/qml/controlbarcontrols/RandomButton.qml \
	gui/qt/player/qml/controlbarcontrols/RendererButton.qml \
	gui/qt/player/qml/controlbarcontrols/RecordButton.qml \
	gui/qt/player/qml/controlbarcontrols/ReverseButton.qml \
	gui/qt/player/qml/controlbarcontrols/SkipBackButton.qml \
	gui/qt/player/qml/controlbarcontrols/SkipForwardButton.qml \
	gui/qt/player/qml/controlbarcontrols/SlowerButton.qml \
	gui/qt/player/qml/controlbarcontrols/SnapshotButton.qml \
	gui/qt/player/qml/controlbarcontrols/SpacerWidget.qml \
	gui/qt/player/qml/controlbarcontrols/StopButton.qml \
	gui/qt/player/qml/controlbarcontrols/TeletextButton.qml \
	gui/qt/player/qml/controlbarcontrols/TeletextColorButton.qml \
	gui/qt/player/qml/controlbarcontrols/TeletextWidget.qml \
	gui/qt/player/qml/controlbarcontrols/VolumeWidget.qml \
	gui/qt/player/qml/controlbarcontrols/Fallback.qml \
	gui/qt/playlist/qml/PlaylistDelegate.qml \
	gui/qt/playlist/qml/PlaylistListView.qml \
	gui/qt/playlist/qml/PlaylistOverlayMenu.qml \
	gui/qt/playlist/qml/PlaylistToolbar.qml \
	gui/qt/playlist/qml/PlaylistDetachedWindow.qml \
	gui/qt/style/AcrylicController.qml \
	gui/qt/style/VLCIcons.qml \
	gui/qt/style/VLCStyle.qml \
	gui/qt/style/qmldir \
	gui/qt/util/qml/Helpers.js \
	gui/qt/util/qml/SelectableDelegateModel.qml \
	gui/qt/util/qml/ModelSortSettingHandler.qml \
	gui/qt/util/qml/MultipleBinding.qml \
	gui/qt/util/qml/FlickableScrollHandler.qml \
	gui/qt/util/qml/ViewDragAutoScrollHandler.qml \
	gui/qt/util/qml/BindingRev8.qml \
	gui/qt/util/qml/BindingRev14.qml \
	gui/qt/util/qml/MouseHoverHandlerRev12.qml \
	gui/qt/util/qml/TouchScreenTapHandlerRev11.qml \
	gui/qt/util/qml/TouchScreenTapHandlerRev12.qml \
	gui/qt/util/qml/VanillaObject.qml \
	gui/qt/util/qml/NativeMenu.qml \
	gui/qt/util/qml/MLContextMenu.qml \
	gui/qt/widgets/qml/ActionButtonOverlay.qml \
	gui/qt/widgets/qml/ActionButtonPrimary.qml \
	gui/qt/widgets/qml/BannerTabButton.qml \
	gui/qt/widgets/qml/BannerCone.qml \
	gui/qt/widgets/qml/BusyIndicatorExt.qml \
	gui/qt/widgets/qml/CaptionLabel.qml \
	gui/qt/widgets/qml/CheckButton.qml \
	gui/qt/widgets/qml/CheckedDelegate.qml \
	gui/qt/widgets/qml/ComboBoxExt.qml \
	gui/qt/widgets/qml/AcrylicBackground.qml \
	gui/qt/widgets/qml/AnimatedBackground.qml \
	gui/qt/widgets/qml/CoverShadow.qml \
	gui/qt/widgets/qml/CSDThemeButton.qml \
	gui/qt/widgets/qml/CSDThemeButtonSet.qml \
	gui/qt/widgets/qml/CSDWindowButton.qml \
	gui/qt/widgets/qml/CSDWindowButtonSet.qml \
	gui/qt/widgets/qml/CSDTitlebarTapNDrapHandler.qml \
	gui/qt/widgets/qml/CSDMouseStealer.qml \
	gui/qt/widgets/qml/CurrentIndicator.qml \
	gui/qt/widgets/qml/DragItem.qml \
	gui/qt/widgets/qml/DrawerExt.qml \
	gui/qt/widgets/qml/ExpandGridView.qml \
	gui/qt/widgets/qml/GridItem.qml \
	gui/qt/widgets/qml/HorizontalResizeHandle.qml \
	gui/qt/widgets/qml/IconLabel.qml \
	gui/qt/widgets/qml/IconButton.qml \
	gui/qt/widgets/qml/IconControlButton.qml \
	gui/qt/widgets/qml/IconToolButton.qml \
	gui/qt/widgets/qml/IconTrackButton.qml \
	gui/qt/widgets/qml/ImageToolButton.qml \
	gui/qt/widgets/qml/KeyNavigableListView.qml \
	gui/qt/widgets/qml/KeyNavigableTableView.qml \
	gui/qt/widgets/qml/TableViewDelegate.qml \
	gui/qt/widgets/qml/ListCoverShadow.qml \
	gui/qt/widgets/qml/ListItem.qml \
	gui/qt/widgets/qml/ListLabel.qml \
	gui/qt/widgets/qml/LoaderFade.qml \
	gui/qt/widgets/qml/LocalTabBar.qml \
	gui/qt/widgets/qml/MediaCover.qml \
	gui/qt/widgets/qml/SubtitleLabel.qml \
	gui/qt/widgets/qml/MenuCaption.qml \
	gui/qt/widgets/qml/MenuLabel.qml \
	gui/qt/widgets/qml/MLDragItem.qml \
	gui/qt/widgets/qml/NavigableCol.qml \
	gui/qt/widgets/qml/NavigableRow.qml \
	gui/qt/widgets/qml/PlayCover.qml \
	gui/qt/widgets/qml/RoundButton.qml \
	gui/qt/widgets/qml/ScaledImage.qml \
	gui/qt/widgets/qml/ScanProgressBar.qml \
	gui/qt/widgets/qml/ScrollingText.qml \
	gui/qt/widgets/qml/SearchBox.qml \
	gui/qt/widgets/qml/Slider.qml \
	gui/qt/widgets/qml/SortControl.qml \
	gui/qt/widgets/qml/SpinBoxExt.qml \
	gui/qt/widgets/qml/StackViewExt.qml \
	gui/qt/widgets/qml/PageLoader.qml \
	gui/qt/widgets/qml/ButtonExt.qml \
	gui/qt/widgets/qml/TableColumns.qml \
	gui/qt/widgets/qml/TextFieldExt.qml \
	gui/qt/widgets/qml/TextToolButton.qml \
	gui/qt/widgets/qml/VideoProgressBar.qml \
	gui/qt/widgets/qml/VideoQualityLabels.qml \
	gui/qt/widgets/qml/ListSubtitleLabel.qml \
	gui/qt/widgets/qml/PointingTooltip.qml \
	gui/qt/widgets/qml/FrostedGlassEffect.qml \
	gui/qt/widgets/qml/OverlayMenu.qml \
	gui/qt/widgets/qml/ToolTipExt.qml \
	gui/qt/widgets/qml/DropShadowImage.qml \
	gui/qt/widgets/qml/DoubleShadow.qml \
	gui/qt/widgets/qml/FadingEdgeListView.qml

lib_qt_plugin_la_QRC = gui/qt/vlc.qrc

if HAVE_WIN32
lib_qt_plugin_la_QRC += gui/qt/windows.qrc
endif

EXTRA_DIST += $(lib_qt_plugin_la_QRC) $(libqt_plugin_la_RES)

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

QRCS_EXCEPT_VLC_QRC = $(filter-out gui/qt/vlc.qrc, $(lib_qt_plugin_la_QRC))
QRCS_EXCEPT_VLC_QRC += qml_cache.qrc

gui/qt/resources.cpp: $(QRCS_EXCEPT_VLC_QRC) $(libqt_plugin_la_RES)
	$(AM_V_GEN)$(RCC) --name qml_cache -o $@ $(filter %.qrc, $^)

#output name matters here
gui/qt/qmlcache_loader.cpp: gui/qt/vlc.qrc qml_cache.qrc $(libqt_plugin_la_QML)
	$(AM_V_GEN)$(QMLCACHE) --resource-file-mapping=$<=qml_cache.qrc $< -o $@

else

EXTRA_DIST += $(libqt_plugin_la_QML)

gui/qt/resources.cpp: $(lib_qt_plugin_la_QRC) $(libqt_plugin_la_RES) $(libqt_plugin_la_QML)
	$(AM_V_GEN)$(RCC) --name vlc -o $@ $(filter %.qrc, $^)

endif

if HAVE_QT5_GTK
libqt_gtktheme_plugin_la_CXXFLAGS = $(AM_CXXFLAGS) $(GTK3_CFLAGS)
libqt_gtktheme_CPPFLAGS = $(AM_CPPFLAGS)
libqt_gtktheme_plugin_la_LDFLAGS = $(AM_LDFLAGS)
libqt_gtktheme_plugin_la_LIBADD = $(GTK3_LIBS)
libqt_gtktheme_plugin_la_SOURCES = \
	gui/qt/style/gtkthemeprovider/gtk_compat.cpp \
	gui/qt/style/gtkthemeprovider/gtk_compat.h \
	gui/qt/style/gtkthemeprovider/gtkthemeprovider.cpp \
	gui/qt/style/gtkthemeprovider/gtk_util.cpp \
	gui/qt/style/gtkthemeprovider/gtk_util.h \
	gui/qt/style/gtkthemeprovider/nav_button_provider_gtk.cpp \
	gui/qt/style/gtkthemeprovider/nav_button_provider_gtk.h \
	gui/qt/style/gtkthemeprovider/scoped_gobject.h
if ENABLE_QT
gui_LTLIBRARIES += libqt_gtktheme_plugin.la
endif
endif

if ENABLE_QT
gui_LTLIBRARIES += libqt_plugin.la
BUILT_SOURCES += $(nodist_libqt_plugin_la_SOURCES)
if !HAVE_WIN32
if !HAVE_OS2
pkglibexec_PROGRAMS += vlc-qt-check
endif
endif
endif
