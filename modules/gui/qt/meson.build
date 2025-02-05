#
# Qt-based interface module
#

qt6 = import('qt6')

qt_include_dir = include_directories('.')

qt_extra_deps = []
qt_extra_flags = ['-DQT_STRICT_ITERATORS','-DQT_USE_QSTRINGBUILDER','-DQT_NO_CAST_TO_ASCII','-DQT_NO_CAST_FROM_BYTEARRAY']
qt_cppargs = []
qt_link_args = []

qt6_dep = dependency('qt6',
    # Note that Qt < 6.2.5 does not provide pc files!
    version: '>=6.2',
    modules: [
        'Core', 'Gui', 'Widgets', 'Svg', 'Qml', 'QmlModels',
        'QuickLayouts', 'QuickTemplates2', 'QmlWorkerScript',
        'Quick', 'QuickControls2'
        ],
    private_headers: true,
    required: get_option('qt'))

moc_headers = files(
    'dialogs/bookmarks/bookmarks.hpp',
    'dialogs/dialogs/dialogmodel.hpp',
    'dialogs/dialogs_provider.hpp',
    'dialogs/epg/EPGChannels.hpp',
    'dialogs/epg/EPGProgram.hpp',
    'dialogs/epg/EPGRuler.hpp',
    'dialogs/epg/EPGView.hpp',
    'dialogs/epg/EPGWidget.hpp',
    'dialogs/epg/epg.hpp',
    'dialogs/extended/extended.hpp',
    'dialogs/extended/extended_panels.hpp',
    'dialogs/extensions/extensions.hpp',
    'dialogs/extensions/extensions_manager.hpp',
    'dialogs/fingerprint/chromaprint.hpp',
    'dialogs/fingerprint/fingerprintdialog.hpp',
    'dialogs/firstrun/firstrunwizard.hpp',
    'dialogs/gototime/gototime.hpp',
    'dialogs/help/aboutmodel.hpp',
    'dialogs/help/help.hpp',
    'dialogs/mediainfo/info_panels.hpp',
    'dialogs/mediainfo/info_widgets.hpp',
    'dialogs/mediainfo/mediainfo.hpp',
    'dialogs/messages/messages.hpp',
    'dialogs/open/open.hpp',
    'dialogs/open/open_panels.hpp',
    'dialogs/open/openurl.hpp',
    'dialogs/plugins/plugins.hpp',
    'dialogs/podcast/podcast_configuration.hpp',
    'dialogs/preferences/complete_preferences.hpp',
    'dialogs/preferences/expert_model.hpp',
    'dialogs/preferences/expert_view.hpp',
    'dialogs/preferences/preferences.hpp',
    'dialogs/preferences/preferences_widgets.hpp',
    'dialogs/preferences/simple_preferences.hpp',
    'dialogs/sout/convert.hpp',
    'dialogs/sout/profile_selector.hpp',
    'dialogs/sout/sout.hpp',
    'dialogs/sout/sout_widgets.hpp',
    'dialogs/systray/systray.hpp',
    'dialogs/toolbar/controlbar_profile.hpp',
    'dialogs/toolbar/controlbar_profile_model.hpp',
    'dialogs/playlists/playlists.hpp',
    'dialogs/vlm/vlm.hpp',
    'maininterface/compositor.hpp',
    'maininterface/compositor_common.hpp',
    'maininterface/compositor_dummy.hpp',
    'maininterface/compositor_platform.hpp',
    'maininterface/interface_window_handler.hpp',
    'maininterface/mainctx.hpp',
    'maininterface/mainctx_submodels.hpp',
    'maininterface/mainui.hpp',
    'maininterface/videosurface.hpp',
    'maininterface/video_window_handler.hpp',
    'medialibrary/medialib.hpp',
    'medialibrary/mlmediastore.hpp',
    'medialibrary/mlalbummodel.hpp',
    'medialibrary/mlalbumtrackmodel.hpp',
    'medialibrary/mlartistmodel.hpp',
    'medialibrary/mlbookmarkmodel.hpp',
    'medialibrary/mlbasemodel.hpp',
    'medialibrary/mlfoldersmodel.hpp',
    'medialibrary/mlgenremodel.hpp',
    'medialibrary/mlthreadpool.hpp',
    'medialibrary/mlqmltypes.hpp',
    'medialibrary/mlmedia.hpp',
    'medialibrary/mlrecentsmodel.hpp',
    'medialibrary/mlrecentsvideomodel.hpp',
    'medialibrary/mlurlmodel.hpp',
    'medialibrary/mlvideo.hpp',
    'medialibrary/mlvideomodel.hpp',
    'medialibrary/mlplaylistlistmodel.hpp',
    'medialibrary/mlplaylistmodel.hpp',
    'medialibrary/mlvideofoldersmodel.hpp',
    'medialibrary/mlvideogroupsmodel.hpp',
    'medialibrary/thumbnailcollector.hpp',
    'menus/custom_menus.hpp',
    'menus/qml_menu_wrapper.hpp',
    'menus/menus.hpp',
    'network/addonsmodel.hpp',
    'network/devicesourceprovider.hpp',
    'network/networkbasemodel.hpp',
    'network/networkdevicemodel.hpp',
    'network/networksourcesmodel.hpp',
    'network/networkmediamodel.hpp',
    'network/standardpathmodel.hpp',
    'style/systempalette.hpp',
    'style/colorcontext.hpp',
    'player/input_models.hpp',
    'player/delay_estimator.hpp',
    'player/player_controller.hpp',
    'player/player_controlbar_model.hpp',
    'player/control_list_model.hpp',
    'player/control_list_filter.hpp',
    'playlist/playlist_controller.hpp',
    'playlist/playlist_item.hpp',
    'playlist/playlist_model.hpp',
    'util/asynctask.hpp',
    'util/audio_device_model.hpp',
    'util/workerthreadset.hpp',
    'util/base_model.hpp',
    'util/color_scheme_model.hpp',
    'util/ui_notifier.hpp',
    'util/color_svg_image_provider.hpp',
    'util/vlcaccess_image_provider.hpp',
    'util/csdbuttonmodel.hpp',
    'util/csdmenu.hpp',
    'util/imageluminanceextractor.hpp',
    'util/keyhelper.hpp',
    'util/listcache.hpp',
    'util/locallistcacheloader.hpp',
    'util/navigation_history.hpp',
    'util/item_key_event_filter.hpp',
    'util/effects_image_provider.hpp',
    'util/flickable_scroll_handler.hpp',
    'util/renderer_manager.hpp',
    'util/validators.hpp',
    'util/varchoicemodel.hpp',
    'util/variables.hpp',
    'util/vlchotkeyconverter.hpp',
    'util/vlctick.hpp',
    'util/dismiss_popup_event_filter.hpp',
    'util/list_selection_model.hpp',
    'util/qsgtextureview.hpp',
    'widgets/native/animators.hpp',
    'widgets/native/csdthemeimage.hpp',
    'widgets/native/customwidgets.hpp',
    'widgets/native/interface_widgets.hpp',
    'widgets/native/navigation_attached.hpp',
    'widgets/native/mlfolderseditor.hpp',
    'widgets/native/searchlineedit.hpp',
    'widgets/native/viewblockingrectangle.hpp',
    'widgets/native/textureprovideritem.hpp',
)

if host_system == 'windows'
    moc_headers += files(
        'maininterface/mainctx_win32.hpp',
        'maininterface/compositor_win7.hpp',
        'maininterface/compositor_dcomp.hpp',
        'maininterface/compositor_dcomp_acrylicsurface.hpp',
    )
endif

if (x11_dep.found() and
    xcb_dep.found() and
    xcb_damage_dep.found() and
    xcb_xfixes_dep.found())
    moc_headers += files(
        'maininterface/compositor_x11.hpp',
        'maininterface/compositor_x11_renderclient.hpp',
        'maininterface/compositor_x11_renderwindow.hpp',
        'maininterface/compositor_x11_uisurface.hpp',
        'maininterface/compositor_x11_utils.hpp',
    )
endif

qt_plugin_sources = files(
    'qt.cpp',
    'qt.hpp',
    'plugins.hpp',
    'dialogs/bookmarks/bookmarks.cpp',
    'dialogs/bookmarks/bookmarks.hpp',
    'dialogs/dialogs/dialogmodel.cpp',
    'dialogs/dialogs/dialogmodel.hpp',
    'dialogs/dialogs_provider.cpp',
    'dialogs/dialogs_provider.hpp',
    'dialogs/epg/EPGChannels.cpp',
    'dialogs/epg/EPGChannels.hpp',
    'dialogs/epg/EPGItem.cpp',
    'dialogs/epg/EPGItem.hpp',
    'dialogs/epg/EPGProgram.cpp',
    'dialogs/epg/EPGProgram.hpp',
    'dialogs/epg/EPGRuler.cpp',
    'dialogs/epg/EPGRuler.hpp',
    'dialogs/epg/EPGView.cpp',
    'dialogs/epg/EPGView.hpp',
    'dialogs/epg/EPGWidget.cpp',
    'dialogs/epg/EPGWidget.hpp',
    'dialogs/epg/epg.cpp',
    'dialogs/epg/epg.hpp',
    'dialogs/extended/extended.cpp',
    'dialogs/extended/extended.hpp',
    'dialogs/extended/extended_panels.cpp',
    'dialogs/extended/extended_panels.hpp',
    'dialogs/extensions/extensions.cpp',
    'dialogs/extensions/extensions.hpp',
    'dialogs/extensions/extensions_manager.cpp',
    'dialogs/extensions/extensions_manager.hpp',
    'dialogs/fingerprint/chromaprint.cpp',
    'dialogs/fingerprint/chromaprint.hpp',
    'dialogs/fingerprint/fingerprintdialog.cpp',
    'dialogs/fingerprint/fingerprintdialog.hpp',
    'dialogs/firstrun/firstrunwizard.cpp',
    'dialogs/firstrun/firstrunwizard.hpp',
    'dialogs/gototime/gototime.cpp',
    'dialogs/gototime/gototime.hpp',
    'dialogs/help/aboutmodel.cpp',
    'dialogs/help/aboutmodel.hpp',
    'dialogs/help/help.cpp',
    'dialogs/help/help.hpp',
    'dialogs/mediainfo/info_panels.cpp',
    'dialogs/mediainfo/info_panels.hpp',
    'dialogs/mediainfo/info_widgets.cpp',
    'dialogs/mediainfo/info_widgets.hpp',
    'dialogs/mediainfo/mediainfo.cpp',
    'dialogs/mediainfo/mediainfo.hpp',
    'dialogs/messages/messages.cpp',
    'dialogs/messages/messages.hpp',
    'dialogs/open/open.cpp',
    'dialogs/open/open.hpp',
    'dialogs/open/open_panels.cpp',
    'dialogs/open/open_panels.hpp',
    'dialogs/open/openurl.cpp',
    'dialogs/open/openurl.hpp',
    'dialogs/plugins/plugins.cpp',
    'dialogs/plugins/plugins.hpp',
    'dialogs/podcast/podcast_configuration.cpp',
    'dialogs/podcast/podcast_configuration.hpp',
    'dialogs/preferences/complete_preferences.cpp',
    'dialogs/preferences/complete_preferences.hpp',
    'dialogs/preferences/expert_model.cpp',
    'dialogs/preferences/expert_model.hpp',
    'dialogs/preferences/expert_view.cpp',
    'dialogs/preferences/expert_view.hpp',
    'dialogs/preferences/preferences.cpp',
    'dialogs/preferences/preferences.hpp',
    'dialogs/preferences/preferences_widgets.cpp',
    'dialogs/preferences/preferences_widgets.hpp',
    'dialogs/preferences/simple_preferences.cpp',
    'dialogs/preferences/simple_preferences.hpp',
    'dialogs/sout/convert.cpp',
    'dialogs/sout/convert.hpp',
    'dialogs/sout/profile_selector.cpp',
    'dialogs/sout/profile_selector.hpp',
    'dialogs/sout/profiles.hpp',
    'dialogs/sout/sout.cpp',
    'dialogs/sout/sout.hpp',
    'dialogs/sout/sout_widgets.cpp',
    'dialogs/sout/sout_widgets.hpp',
    'dialogs/systray/systray.cpp',
    'dialogs/systray/systray.hpp',
    'dialogs/toolbar/controlbar_profile.hpp',
    'dialogs/toolbar/controlbar_profile.cpp',
    'dialogs/toolbar/controlbar_profile_model.cpp',
    'dialogs/toolbar/controlbar_profile_model.hpp',
    'dialogs/vlm/vlm.cpp',
    'dialogs/playlists/playlists.cpp',
    'dialogs/playlists/playlists.hpp',
    'maininterface/compositor.hpp',
    'maininterface/compositor.cpp',
    'maininterface/compositor_common.hpp',
    'maininterface/compositor_common.cpp',
    'maininterface/compositor_accessibility.hpp',
    'maininterface/compositor_accessibility.cpp',
    'maininterface/compositor_dummy.hpp',
    'maininterface/compositor_dummy.cpp',
    'maininterface/compositor_platform.hpp',
    'maininterface/compositor_platform.cpp',
    'maininterface/interface_window_handler.cpp',
    'maininterface/interface_window_handler.hpp',
    'maininterface/mainctx.cpp',
    'maininterface/mainctx.hpp',
    'maininterface/mainctx_submodels.hpp',
    'maininterface/mainui.cpp',
    'maininterface/mainui.hpp',
    'maininterface/videosurface.cpp',
    'maininterface/videosurface.hpp',
    'maininterface/video_window_handler.cpp',
    'maininterface/video_window_handler.hpp',
    'maininterface/windoweffects_module.hpp',
    'medialibrary/medialib.cpp',
    'medialibrary/medialib.hpp',
    'medialibrary/mlmediastore.cpp',
    'medialibrary/mlmediastore.hpp',
    'medialibrary/mlalbum.cpp',
    'medialibrary/mlalbum.hpp',
    'medialibrary/mlalbummodel.cpp',
    'medialibrary/mlalbummodel.hpp',
    'medialibrary/mlalbumtrack.cpp',
    'medialibrary/mlalbumtrack.hpp',
    'medialibrary/mlalbumtrackmodel.cpp',
    'medialibrary/mlalbumtrackmodel.hpp',
    'medialibrary/mlartist.cpp',
    'medialibrary/mlartist.hpp',
    'medialibrary/mlartistmodel.cpp',
    'medialibrary/mlartistmodel.hpp',
    'medialibrary/mlbasemodel.cpp',
    'medialibrary/mlbasemodel.hpp',
    'medialibrary/mlbookmarkmodel.cpp',
    'medialibrary/mlbookmarkmodel.hpp',
    'medialibrary/mlevent.hpp',
    'medialibrary/mlfolder.cpp',
    'medialibrary/mlfolder.hpp',
    'medialibrary/mlfoldersmodel.cpp',
    'medialibrary/mlfoldersmodel.hpp',
    'medialibrary/mlgenre.cpp',
    'medialibrary/mlgenre.hpp',
    'medialibrary/mlgenremodel.cpp',
    'medialibrary/mlgenremodel.hpp',
    'medialibrary/mlgroup.cpp',
    'medialibrary/mlgroup.hpp',
    'medialibrary/mlhelper.cpp',
    'medialibrary/mlhelper.hpp',
    'medialibrary/mlthreadpool.cpp',
    'medialibrary/mlthreadpool.hpp',
    'medialibrary/mlqmltypes.hpp',
    'medialibrary/mlmedia.hpp',
    'medialibrary/mlqueryparams.cpp',
    'medialibrary/mlqueryparams.hpp',
    'medialibrary/mlrecentsmodel.cpp',
    'medialibrary/mlrecentsmodel.hpp',
    'medialibrary/mlrecentsvideomodel.cpp',
    'medialibrary/mlrecentsvideomodel.hpp',
    'medialibrary/mlurlmodel.cpp',
    'medialibrary/mlurlmodel.hpp',
    'medialibrary/mlvideo.cpp',
    'medialibrary/mlvideo.hpp',
    'medialibrary/mlvideofoldersmodel.cpp',
    'medialibrary/mlvideofoldersmodel.hpp',
    'medialibrary/mlvideogroupsmodel.cpp',
    'medialibrary/mlvideogroupsmodel.hpp',
    'medialibrary/mlvideomodel.cpp',
    'medialibrary/mlvideomodel.hpp',
    'medialibrary/mlplaylist.cpp',
    'medialibrary/mlplaylist.hpp',
    'medialibrary/mlplaylistlistmodel.cpp',
    'medialibrary/mlplaylistlistmodel.hpp',
    'medialibrary/mlplaylistmedia.cpp',
    'medialibrary/mlplaylistmedia.hpp',
    'medialibrary/mlplaylistmodel.cpp',
    'medialibrary/mlplaylistmodel.hpp',
    'medialibrary/thumbnailcollector.hpp',
    'medialibrary/thumbnailcollector.cpp',
    'medialibrary/mlcustomcover.hpp',
    'medialibrary/mlcustomcover.cpp',
    'menus/custom_menus.cpp',
    'menus/custom_menus.hpp',
    'menus/qml_menu_wrapper.cpp',
    'menus/qml_menu_wrapper.hpp',
    'menus/menus.cpp',
    'menus/menus.hpp',
    'network/addonsmodel.cpp',
    'network/addonsmodel.hpp',
    'network/mediatreelistener.cpp',
    'network/mediatreelistener.hpp',
    'network/devicesourceprovider.cpp',
    'network/devicesourceprovider.hpp',
    'network/networkbasemodel.cpp',
    'network/networkbasemodel.hpp',
    'network/networkdevicemodel.cpp',
    'network/networkdevicemodel.hpp',
    'network/networksourcesmodel.cpp',
    'network/networksourcesmodel.hpp',
    'network/networkmediamodel.cpp',
    'network/networkmediamodel.hpp',
    'network/standardpathmodel.cpp',
    'network/standardpathmodel.hpp',
    'network/vlcmediasourcewrapper.hpp',
    'style/qtthemeprovider.hpp',
    'style/colorcontext.cpp',
    'style/colorcontext.hpp',
    'style/systempalette.cpp',
    'style/systempalette.hpp',
    'style/defaultthemeproviders.hpp',
    'style/systempalettethemeprovider.cpp',
    'player/input_models.cpp',
    'player/delay_estimator.cpp',
    'player/input_models.hpp',
    'player/player_controller.cpp',
    'player/player_controller.hpp',
    'player/player_controller_p.hpp',
    'player/player_controlbar_model.cpp',
    'player/player_controlbar_model.hpp',
    'player/control_list_model.cpp',
    'player/control_list_model.hpp',
    'player/control_list_filter.cpp',
    'player/control_list_filter.hpp',
    'playlist/media.hpp',
    'playlist/playlist_common.hpp',
    'playlist/playlist_controller.cpp',
    'playlist/playlist_controller.hpp',
    'playlist/playlist_controller_p.hpp',
    'playlist/playlist_item.cpp',
    'playlist/playlist_item.hpp',
    'playlist/playlist_model.cpp',
    'playlist/playlist_model.hpp',
    'playlist/playlist_model_p.hpp',
    'util/asynctask.hpp',
    'util/audio_device_model.cpp',
    'util/audio_device_model.hpp',
    'util/workerthreadset.cpp',
    'util/workerthreadset.hpp',
    'util/base_model.cpp',
    'util/base_model.hpp',
    'util/base_model_p.hpp',
    'util/color_scheme_model.cpp',
    'util/color_scheme_model.hpp',
    'util/ui_notifier.cpp',
    'util/ui_notifier.hpp',
    'util/color_svg_image_provider.cpp',
    'util/color_svg_image_provider.hpp',
    'util/vlcaccess_image_provider.cpp',
    'util/vlcaccess_image_provider.hpp',
    'util/covergenerator.cpp',
    'util/covergenerator.hpp',
    'util/csdbuttonmodel.cpp',
    'util/csdbuttonmodel.hpp',
    'util/csdmenu.cpp',
    'util/csdmenu.hpp',
    'util/imageluminanceextractor.cpp',
    'util/imageluminanceextractor.hpp',
    'util/imagehelper.cpp',
    'util/imagehelper.hpp',
    'util/keyhelper.cpp',
    'util/keyhelper.hpp',
    'util/listcache.hpp',
    'util/listcache.hxx',
    'util/listcacheloader.hpp',
    'util/locallistcacheloader.hpp',
    'util/navigation_history.cpp',
    'util/navigation_history.hpp',
    'util/item_key_event_filter.cpp',
    'util/item_key_event_filter.hpp',
    'util/flickable_scroll_handler.cpp',
    'util/flickable_scroll_handler.hpp',
    'util/qt_dirs.cpp',
    'util/qt_dirs.hpp',
    'util/proxycolumnmodel.hpp',
    'util/registry.cpp',
    'util/registry.hpp',
    'util/renderer_manager.cpp',
    'util/renderer_manager.hpp',
    'util/singleton.hpp',
    'util/soutchain.cpp',
    'util/soutchain.hpp',
    'util/validators.cpp',
    'util/validators.hpp',
    'util/varcommon_p.hpp',
    'util/varchoicemodel.cpp',
    'util/varchoicemodel.hpp',
    'util/variables.cpp',
    'util/variables.hpp',
    'util/vlctick.cpp',
    'util/vlctick.hpp',
    'util/vlchotkeyconverter.cpp',
    'util/vlchotkeyconverter.hpp',
    'util/shared_input_item.hpp',
    'util/fast_gaussian_blur_template.h',
    'util/effects_image_provider.cpp',
    'util/effects_image_provider.hpp',
    'util/qsgroundedrectangularimagenode.cpp',
    'util/qsgroundedrectangularimagenode.hpp',
    'util/qsgtextureview.cpp',
    'util/qsgtextureview.hpp',
    'util/dismiss_popup_event_filter.cpp',
    'util/dismiss_popup_event_filter.hpp',
    'util/list_selection_model.cpp',
    'util/list_selection_model.hpp',
    'util/model_recovery_agent.hpp',
    'util/vlcqtmessagehandler.cpp',
    'util/vlcqtmessagehandler.hpp',
    'util/colorizedsvgicon.cpp',
    'util/colorizedsvgicon.hpp',
    'widgets/native/animators.cpp',
    'widgets/native/animators.hpp',
    'widgets/native/customwidgets.cpp',
    'widgets/native/customwidgets.hpp',
    'widgets/native/csdthemeimage.cpp',
    'widgets/native/csdthemeimage.hpp',
    'widgets/native/interface_widgets.cpp',
    'widgets/native/interface_widgets.hpp',
    'widgets/native/navigation_attached.cpp',
    'widgets/native/navigation_attached.hpp',
    'widgets/native/mlfolderseditor.cpp',
    'widgets/native/mlfolderseditor.hpp',
    'widgets/native/qvlcframe.cpp',
    'widgets/native/qvlcframe.hpp',
    'widgets/native/searchlineedit.cpp',
    'widgets/native/searchlineedit.hpp',
    'widgets/native/viewblockingrectangle.cpp',
    'widgets/native/viewblockingrectangle.hpp',
    'widgets/native/doubleclickignoringitem.hpp',
    'widgets/native/textureprovideritem.cpp',
    'widgets/native/textureprovideritem.hpp',
)

if host_system == 'windows'
    qt_plugin_sources += files(
        'maininterface/mainctx_win32.cpp',
        'maininterface/mainctx_win32.hpp',
        'maininterface/compositor_win7.cpp',
        'maininterface/compositor_win7.hpp',
        'style/windowsthemeprovider.cpp',
        'util/csdmenu_win32.c',
        'util/csdmenu_win32.h',
        'maininterface/compositor_dcomp.cpp',
        'maininterface/compositor_dcomp_acrylicsurface.cpp',
    )
endif

if (x11_dep.found() and
    xcb_dep.found() and
    xcb_damage_dep.found() and
    xcb_xfixes_dep.found())
    qt_plugin_sources += files(
        'maininterface/compositor_x11.cpp',
        'maininterface/compositor_x11_renderclient.cpp',
        'maininterface/compositor_x11_renderwindow.cpp',
        'maininterface/compositor_x11_uisurface.cpp',
        'maininterface/compositor_x11_utils.cpp',
    )
endif

qrc_files = files('assets.qrc')
if host_system == 'windows'
  qrc_files += files('windows.qrc')
endif

ui_sources = files(
    'dialogs/extended/equalizer.ui',
    'dialogs/extended/video_effects.ui',
    'dialogs/fingerprint/fingerprintdialog.ui',
    'dialogs/firstrun/firstrunwizard.ui',
    'dialogs/help/about.ui',
    'dialogs/help/update.ui',
    'dialogs/messages/messages_panel.ui',
    'dialogs/open/open.ui',
    'dialogs/open/open_capture.ui',
    'dialogs/open/open_disk.ui',
    'dialogs/open/open_file.ui',
    'dialogs/open/open_net.ui',
    'dialogs/podcast/podcast_configuration.ui',
    'dialogs/preferences/sprefs_audio.ui',
    'dialogs/preferences/sprefs_input.ui',
    'dialogs/preferences/sprefs_interface.ui',
    'dialogs/preferences/sprefs_medialibrary.ui',
    'dialogs/preferences/sprefs_subtitles.ui',
    'dialogs/preferences/sprefs_video.ui',
    'dialogs/sout/profiles.ui',
    'dialogs/sout/sout.ui',
    'dialogs/vlm/vlm.ui',
)

qml_modules = []

qml_modules += {
    'name' : 'VLC.Dialogs',
    'target' : 'dialogs',
    'outdir' : 'VLC/Dialogs',
    'sources': files(
        'dialogs/dialogs/qml/CustomDialog.qml',
        'dialogs/dialogs/qml/Dialogs.qml',
        'dialogs/dialogs/qml/ModalDialog.qml',
        'dialogs/dialogs/qml/WindowDialog.qml',
        'dialogs/toolbar/qml/EditorDNDDelegate.qml',
        'dialogs/toolbar/qml/EditorDNDView.qml',
        'dialogs/toolbar/qml/EditorDummyButton.qml',
        'dialogs/toolbar/qml/EditorTabButton.qml',
        'dialogs/toolbar/qml/ToolbarEditor.qml',
        'dialogs/toolbar/qml/ToolbarEditorDialog.qml',
        'dialogs/toolbar/qml/ToolbarEditorButtonList.qml'
    ),
}

qml_modules += {
    'name' : 'VLC.MainInterface',
    'target' : 'maininterface',
    'outdir' : 'VLC/MainInterface',
    'sources': files(
        'maininterface/qml/BannerSources.qml',
        'maininterface/qml/MainInterface.qml',
        'maininterface/qml/MainViewLoader.qml',
        'maininterface/qml/MainDisplay.qml',
        'maininterface/qml/MainGridView.qml',
        'maininterface/qml/MainTableView.qml',
        'maininterface/qml/NoMedialibHome.qml'
    ),
}
qml_modules += {
    'name' : 'VLC.MediaLibrary',
    'target' : 'medialibrary',
    'outdir' : 'VLC/MediaLibrary',
    'sources': files(
        'medialibrary/qml/ArtistTopBanner.qml',
        'medialibrary/qml/AudioGridItem.qml',
        'medialibrary/qml/VideoGroupDisplay.qml',
        'medialibrary/qml/MusicAlbums.qml',
        'medialibrary/qml/MusicAlbumsDisplay.qml',
        'medialibrary/qml/MusicAlbumsGridExpandDelegate.qml',
        'medialibrary/qml/MusicArtist.qml',
        'medialibrary/qml/MusicArtistDelegate.qml',
        'medialibrary/qml/MusicArtistsAlbums.qml',
        'medialibrary/qml/MusicAllArtists.qml',
        'medialibrary/qml/MusicArtistsDisplay.qml',
        'medialibrary/qml/MusicDisplay.qml',
        'medialibrary/qml/MusicGenres.qml',
        'medialibrary/qml/MusicGenresDisplay.qml',
        'medialibrary/qml/MusicPlaylistsDisplay.qml',
        'medialibrary/qml/MusicTrackListDisplay.qml',
        'medialibrary/qml/MusicTracksDisplay.qml',
        'medialibrary/qml/UrlListDisplay.qml',
        'medialibrary/qml/VideoDisplay.qml',
        'medialibrary/qml/VideoAll.qml',
        'medialibrary/qml/VideoAllDisplay.qml',
        'medialibrary/qml/VideoAllSubDisplay.qml',
        'medialibrary/qml/PlaylistMediaList.qml',
        'medialibrary/qml/PlaylistMedia.qml',
        'medialibrary/qml/PlaylistMediaDisplay.qml',
        'medialibrary/qml/VideoPlaylistsDisplay.qml',
        'medialibrary/qml/VideoRecentVideos.qml',
        'medialibrary/qml/VideoRecentVideosDisplay.qml',
        'medialibrary/qml/VideoGridItem.qml',
        'medialibrary/qml/VideoInfoExpandPanel.qml',
        'medialibrary/qml/VideoListDisplay.qml',
        'medialibrary/qml/VideoGridDisplay.qml'
    ),
}

qml_modules += {
    'name' : 'VLC.Menus',
    'target' : 'menus',
    'outdir' : 'VLC/Menus',
    'sources': files(
        'menus/qml/GlobalShortcuts.qml',
        'menus/qml/GlobalShortcutsMedialib.qml',
        'menus/qml/Menubar.qml',
        'menus/qml/ShortcutExt.qml'
    ),
}

qml_modules += {
    'name' : 'VLC.Network',
    'target' : 'network',
    'outdir' : 'VLC/Network',
    'sources': files(
        'network/qml/AddressbarButton.qml',
        'network/qml/BrowseDeviceView.qml',
        'network/qml/BrowseDisplay.qml',
        'network/qml/BrowseHomeDisplay.qml',
        'network/qml/BrowseTreeDisplay.qml',
        'network/qml/BrowseTreeHeader.qml',
        'network/qml/DiscoverDisplay.qml',
        'network/qml/DiscoverUrlDisplay.qml',
        'network/qml/NetworkAddressbar.qml',
        'network/qml/NetworkCustomCover.qml',
        'network/qml/NetworkGridItem.qml',
        'network/qml/NetworkThumbnailItem.qml',
        'network/qml/ServicesHomeDisplay.qml',
        'network/qml/ServicesManage.qml',
        'network/qml/ServicesSources.qml'
    ),
}

qml_modules += {
   'name' : 'VLC.Player',
   'target' : 'player',
   'outdir' : 'VLC/Player',
   'sources': files(
       'player/qml/Bookmarks.qml',
       'player/qml/ControlBar.qml',
       'player/qml/PlaybackSpeed.qml',
       'player/qml/MiniPlayer.qml',
       'player/qml/PIPPlayer.qml',
       'player/qml/Player.qml',
       'player/qml/PlayerControlLayout.qml',
       'player/qml/PlayerPlaylistVisibilityFSM.qml',
       'player/qml/PlayerToolbarVisibilityFSM.qml',
       'player/qml/MinimalView.qml',
       'player/qml/ResumeDialog.qml',
       'player/qml/SliderBar.qml',
       'player/qml/TopBar.qml',
       'player/qml/TracksMenu.qml',
       'player/qml/TracksPage.qml',
       'player/qml/TracksPageAudio.qml',
       'player/qml/TracksPageSpeed.qml',
       'player/qml/TracksPageSubtitle.qml',
       'player/qml/TracksListPage.qml',
       'player/qml/ControlLayout.qml',
       'player/qml/ControlRepeater.qml',
       'player/qml/NavigationBox.qml',
   ),
   'singletons' : files(
       'player/qml/PlayerControlbarControls.qml',
   )
}

qml_modules += {
   'name' : 'VLC.PlayerControls',
   'target' : 'playercontrols',
   'outdir' : 'VLC/PlayerControls',
   'sources': files(
       'player/qml/controlbarcontrols/HighResolutionTimeWidget.qml',
       'player/qml/controlbarcontrols/ArtworkInfoWidget.qml',
       'player/qml/controlbarcontrols/AspectRatioWidget.qml',
       'player/qml/controlbarcontrols/AtoBButton.qml',
       'player/qml/controlbarcontrols/BackButton.qml',
       'player/qml/controlbarcontrols/BookmarkButton.qml',
       'player/qml/controlbarcontrols/ChapterNextButton.qml',
       'player/qml/controlbarcontrols/ChapterPreviousButton.qml',
       'player/qml/controlbarcontrols/DvdMenuButton.qml',
       'player/qml/controlbarcontrols/ExpandingSpacerWidget.qml',
       'player/qml/controlbarcontrols/ExtendedSettingsButton.qml',
       'player/qml/controlbarcontrols/FasterButton.qml',
       'player/qml/controlbarcontrols/FrameButton.qml',
       'player/qml/controlbarcontrols/FullscreenButton.qml',
       'player/qml/controlbarcontrols/InfoButton.qml',
       'player/qml/controlbarcontrols/LangButton.qml',
       'player/qml/controlbarcontrols/LoopButton.qml',
       'player/qml/controlbarcontrols/MenuButton.qml',
       'player/qml/controlbarcontrols/NavigationBoxButton.qml',
       'player/qml/controlbarcontrols/NavigationWidget.qml',
       'player/qml/controlbarcontrols/NextButton.qml',
       'player/qml/controlbarcontrols/OpenButton.qml',
       'player/qml/controlbarcontrols/PlaybackSpeedButton.qml',
       'player/qml/controlbarcontrols/PlayButton.qml',
       'player/qml/controlbarcontrols/PlayerSwitchButton.qml',
       'player/qml/controlbarcontrols/PlaylistButton.qml',
       'player/qml/controlbarcontrols/PreviousButton.qml',
       'player/qml/controlbarcontrols/ProgramButton.qml',
       'player/qml/controlbarcontrols/QuitButton.qml',
       'player/qml/controlbarcontrols/RandomButton.qml',
       'player/qml/controlbarcontrols/RendererButton.qml',
       'player/qml/controlbarcontrols/RecordButton.qml',
       'player/qml/controlbarcontrols/ReverseButton.qml',
       'player/qml/controlbarcontrols/SkipBackButton.qml',
       'player/qml/controlbarcontrols/SkipForwardButton.qml',
       'player/qml/controlbarcontrols/SlowerButton.qml',
       'player/qml/controlbarcontrols/SnapshotButton.qml',
       'player/qml/controlbarcontrols/SpacerWidget.qml',
       'player/qml/controlbarcontrols/StopButton.qml',
       'player/qml/controlbarcontrols/TeletextButton.qml',
       'player/qml/controlbarcontrols/TeletextColorButton.qml',
       'player/qml/controlbarcontrols/TeletextWidget.qml',
       'player/qml/controlbarcontrols/VolumeWidget.qml',
       'player/qml/controlbarcontrols/Fallback.qml',
   ),
}

qml_modules += {
    'name' : 'VLC.Playlist',
    'target' : 'playlist',
    'outdir' : 'VLC/Playlist',
    'sources': files(
        'playlist/qml/PlaylistDelegate.qml',
        'playlist/qml/PlaylistListView.qml',
        'playlist/qml/PlaylistToolbar.qml',
        'playlist/qml/PlaylistDetachedWindow.qml',
    ),
}

qml_modules += {
    'name' : 'VLC.Style',
    'target' : 'style',
    'outdir' : 'VLC/Style',
    'singletons': files(
        'style/AcrylicController.qml',
        'style/VLCIcons.qml',
        'style/VLCStyle.qml',
    ),
}

qml_modules += {
    'name' : 'VLC.Util',
    'target' : 'util',
    'outdir' : 'VLC/Util',
    'sources': files(
        'util/qml/ModelSortSettingHandler.qml',
        'util/qml/DefaultFlickableScrollHandler.qml',
        'util/qml/ViewDragAutoScrollHandler.qml',
        'util/qml/FSM.qml',
        'util/qml/FSMState.qml',
        'util/qml/GridSizeHelper.qml',
        'util/qml/NativeMenu.qml',
        'util/qml/MLContextMenu.qml',
        'util/qml/FadeControllerStateGroup.qml'
    ),
    'singletons': files(
        'util/qml/Helpers.qml',
    ),
}

if qt6_dep.version().version_compare('>=6.5.0')
    qml_dynamicshadow_file = 'widgets/qml/DynamicShadow.qml'
    qml_blureffect_file = 'widgets/qml/BlurEffect.qml'
else
    qml_dynamicshadow_file = 'widgets/qml/compat/DynamicShadow.qml'
    qml_blureffect_file = 'widgets/qml/compat/BlurEffect.qml'
endif

qml_modules += {
    'name' : 'VLC.Widgets',
    'target' : 'widgets',
    'outdir' : 'VLC/Widgets',
    'sources': files(
        'widgets/qml/LabelExt.qml',
        'widgets/qml/ActionButtonOverlay.qml',
        'widgets/qml/ActionButtonPrimary.qml',
        'widgets/qml/BannerTabButton.qml',
        'widgets/qml/BannerCone.qml',
        'widgets/qml/BusyIndicatorExt.qml',
        'widgets/qml/CaptionLabel.qml',
        'widgets/qml/CheckButton.qml',
        'widgets/qml/CheckedDelegate.qml',
        'widgets/qml/ComboBoxExt.qml',
        'widgets/qml/AcrylicBackground.qml',
        'widgets/qml/FloatingNotification.qml',
        'widgets/qml/AnimatedBackground.qml',
        'widgets/qml/CSDThemeButton.qml',
        'widgets/qml/CSDThemeButtonSet.qml',
        'widgets/qml/CSDWindowButton.qml',
        'widgets/qml/CSDWindowButtonSet.qml',
        'widgets/qml/CSDTitlebarTapNDrapHandler.qml',
        'widgets/qml/CSDMouseStealer.qml',
        'widgets/qml/CurrentIndicator.qml',
        'widgets/qml/DragItem.qml',
        'widgets/qml/DrawerExt.qml',
        'widgets/qml/ExpandGridView.qml',
        'widgets/qml/ExpandGridItemView.qml',
        'widgets/qml/EmptyLabel.qml',
        'widgets/qml/EmptyLabelButton.qml',
        'widgets/qml/EmptyLabelHint.qml',
        'widgets/qml/GridItem.qml',
        'widgets/qml/HorizontalResizeHandle.qml',
        'widgets/qml/IconLabel.qml',
        'widgets/qml/IconButton.qml',
        'widgets/qml/IconToolButton.qml',
        'widgets/qml/IconTrackButton.qml',
        'widgets/qml/ImageToolButton.qml',
        'widgets/qml/ListViewExt.qml',
        'widgets/qml/TableViewExt.qml',
        'widgets/qml/TableViewDelegate.qml',
        'widgets/qml/TableRowDelegate.qml',
        'widgets/qml/TableHeaderDelegate.qml',
        'widgets/qml/DefaultShadow.qml',
        'widgets/qml/ListLabel.qml',
        'widgets/qml/LocalTabBar.qml',
        'widgets/qml/MediaCover.qml',
        'widgets/qml/SubtitleLabel.qml',
        'widgets/qml/MenuCaption.qml',
        'widgets/qml/MenuLabel.qml',
        'widgets/qml/MLDragItem.qml',
        'widgets/qml/MLTableColumns.qml',
        'widgets/qml/NavigableCol.qml',
        'widgets/qml/NavigableRow.qml',
        'widgets/qml/PlayCover.qml',
        'widgets/qml/RoundButton.qml',
        'widgets/qml/ScaledImage.qml',
        'widgets/qml/ScanProgressBar.qml',
        'widgets/qml/TextAutoScroller.qml',
        'widgets/qml/SearchBox.qml',
        'widgets/qml/SliderExt.qml',
        'widgets/qml/SortControl.qml',
        'widgets/qml/SpinBoxExt.qml',
        'widgets/qml/StackViewExt.qml',
        'widgets/qml/PageLoader.qml',
        'widgets/qml/ButtonExt.qml',
        'widgets/qml/TableColumns.qml',
        'widgets/qml/TextFieldExt.qml',
        'widgets/qml/TextToolButton.qml',
        'widgets/qml/TrackDelayButton.qml',
        'widgets/qml/VideoProgressBar.qml',
        'widgets/qml/VideoQualityLabels.qml',
        'widgets/qml/ListSubtitleLabel.qml',
        'widgets/qml/PointingTooltip.qml',
        'widgets/qml/FrostedGlassEffect.qml',
        'widgets/qml/ToolTipExt.qml',
        'widgets/qml/DropShadowImage.qml',
        'widgets/qml/DoubleShadow.qml',
        'widgets/qml/FadingEdge.qml',
        'widgets/qml/FadingEdgeForListView.qml',
        'widgets/qml/PopupIconToolButton.qml',
        'widgets/qml/PartialEffect.qml',
        'widgets/qml/ViewHeader.qml',
        'widgets/qml/ProgressIndicator.qml',
        'widgets/qml/RectangularGlow.qml',
        qml_dynamicshadow_file,
        qml_blureffect_file,
        'widgets/qml/RoundImage.qml',
    ),
}

qml_tests = {
    'tst_FSM': files('tests/tst_FSM.qml'),
}

qmllibs = []

if qt6_dep.found()
    qt_rcc_extra_arguments = []
    qt_zstd_check = '''
    #include <qconfig.h>
    #if QT_FEATURE_zstd != 1
    #error "Qt can not decompress zstd. Resources are going to be compressed using zlib."
    #endif
    '''
    if cpp.compiles(qt_zstd_check, dependencies: qt6_dep, name: 'Qt supports decompressing zstd')
        qt_rcc_extra_arguments += '-compress-algo=zstd'
    else
        qt_rcc_extra_arguments += '-compress-algo=zlib'
    endif

    libatomic = cc.find_library('atomic', required: false)
    if libatomic.found()
        qt_extra_deps += cc.find_library('atomic', required: false)
        qt_extra_flags += '-DQT_HAS_LIBATOMIC'
    endif

    prog_python = find_program('python3')
    modules_qrc = []
    foreach module : qml_modules
        #generates 'res.qrc', 'qmldir.qrc', 'qmldir',
        run_command(
            prog_python,
            meson.current_source_dir() / 'scripts/genqrc.py',
            '-o', meson.current_build_dir() / module['outdir'] / 'res.qrc',
            '--prefix', 'qt/qml' / module['outdir'],
            '--sources', module.get('sources', []) + module.get('singletons', []),
            check: true,
        )
        run_command(
            prog_python,
            meson.current_source_dir() / 'scripts/genqmldir.py',
            '-o', meson.current_build_dir() / module['outdir'] / 'qmldir',
            '--module', module['name'],
            '--prefix', 'qt/qml',
            '--sources', module.get('sources', []),
            '--singletons', module.get('singletons', []),
            check: true,
        )
        run_command(
            prog_python,
            meson.current_source_dir() / 'scripts/genqrc.py',
            '-o', meson.current_build_dir() / module['outdir'] / 'qmldir.qrc',
            '--prefix', 'qt/qml' / module['outdir'],
            '--sources', [meson.current_build_dir() / module['outdir'] / 'qmldir'],
            check: true,
        )
        module_qrc = qt6.preprocess(
            module['target'] + '_assets',
            qresources: files(
                meson.current_build_dir() / module['outdir'] / 'res.qrc',
                meson.current_build_dir() / module['outdir'] / 'qmldir.qrc',
            ),
            include_directories: qt_include_dir,
            dependencies: qt6_dep,
            rcc_extra_arguments: qt_rcc_extra_arguments
        )

        #FIXME copy every files to the outdir module so they can be used by qt tooling

        qmllibs += static_library(
            'qml_module' + module['target'],
            module_qrc,
            dependencies: qt6_dep)
    endforeach

    qt_static_check = '''
    #include <qconfig.h>
    #ifndef QT_STATIC
    # error Qt using shared libraries
    #endif
    '''

    if cpp.compiles(qt_static_check, dependencies: qt6_dep, name: 'Qt6 compiled with static libraries')
        qt_install_bin_directory = qt6_dep.get_variable(pkgconfig: 'bindir', configtool: '-query QT_INSTALL_BINS')
        qmake6 = find_program(qt_install_bin_directory + '/qmake6', required: true)
        if qmake6.found()
            qtcflags = run_command(
                prog_python,
                meson.current_source_dir() / 'scripts/static_dirs.py',
                '--qmake', qmake6.full_path(),
                '--pro', meson.current_source_dir() / 'qt6.pro',
                '--builddir', meson.current_build_dir(),
                '--cflags',
                check: false,
            )
            if qtcflags.returncode() == 0
                qt_extra_flags += qtcflags.stdout().split()
            else
                warning('Failed to get static Qt compilation flags: ' + qtcflags.stderr())
            endif
            qtlflags = run_command(
                prog_python,
                meson.current_source_dir() / 'scripts/static_dirs.py',
                '--qmake', qmake6.full_path(),
                '--pro', meson.current_source_dir() / 'qt6.pro',
                '--builddir', meson.current_build_dir(),
                '--libs', '--ldflags',
                check: false,
            )
            if qtlflags.returncode() == 0
                qt_link_args += qtlflags.stdout().split()
            else
                warning('Failed to get static Qt link flags: ' + qtlflags.stderr())
            endif
        else
            warning('Failed to find qmake6 to get static Qt variables')
        endif
    endif

    qt6pre_qrc = qt6.preprocess(
        qresources: qrc_files,
        include_directories: qt_include_dir,
        dependencies: qt6_dep,
        rcc_extra_arguments: qt_rcc_extra_arguments)

    qt_sources = files('qt.cpp')

    subdir('shaders')

    # Qt plugins
    if host_system == 'windows'
        qt_extra_deps += [
            cc.find_library('dwmapi'),
            cc.find_library('comctl32'),
        ]
    endif

    if x11_dep.found() and xcb_dep.found()
        qt_extra_deps += xcb_dep
        qt_extra_deps += x11_dep
        qt_extra_flags += '-DQT_HAS_XCB'
        if xcb_damage_dep.found() and xcb_xfixes_dep.found()
            qt_extra_deps += [
                xcb_render_dep,
                xcb_composite_dep,
                xcb_damage_dep,
                xcb_xfixes_dep
            ]
            qt_extra_flags += '-DQT_HAS_X11_COMPOSITOR'
        endif
    endif

    if get_option('qt_qml_debug')
        qt_extra_flags += '-DQT_QML_DEBUG'
    endif

    #check private headers
    if cpp.has_header('private/qquickitem_p.h', dependencies: qt6_dep) \
      and cpp.has_header('private/qquickwindow_p.h', dependencies: qt6_dep)
        qt_extra_flags += '-DQT_DECLARATIVE_PRIVATE'
    endif

    if cpp.has_header('qpa/qplatformnativeinterface.h', dependencies: qt6_dep)
        qt_extra_flags += '-DQT_GUI_PRIVATE'
    endif

    if host_system == 'windows'
        qt_cppargs += libcom_cppflags
    endif

    #Qt GTK theme module
    gtk_dep = dependency(
        'gtk+-3.0',
        version: '>=3.20',
        required: get_option('qt_gtk').disable_auto_if(host_system in ['windows', 'darwin'])
    )

    vlc_modules += {
        'name' : 'qt_gtktheme',
        'sources' : files(
            'style/gtkthemeprovider/gtk_compat.cpp',
            'style/gtkthemeprovider/gtk_compat.h',
            'style/gtkthemeprovider/gtkthemeprovider.cpp',
            'style/gtkthemeprovider/gtk_util.cpp',
            'style/gtkthemeprovider/gtk_util.h',
            'style/gtkthemeprovider/nav_button_provider_gtk.cpp',
            'style/gtkthemeprovider/nav_button_provider_gtk.h',
            'style/gtkthemeprovider/scoped_gobject.h'
        ),
        'dependencies' : [gtk_dep],
        'enabled' : gtk_dep.found()
    }

    if gtk_dep.found()
        qt_extra_flags += '-DQT_HAS_GTK'
    endif

    if host_system == 'windows'
        qt_plugin_sources += files('maininterface/win32windoweffects_module.cpp',
                              'maininterface/win32windoweffects_module.hpp')
    else
        kf6WindowSystem_dep = dependency('KF6WindowSystem',
                                         version: '>=6.0.0',
                                         required: false)
        if (kf6WindowSystem_dep.found())
            vlc_modules += {
                'name' : 'qtwindoweffects',
                'sources' : files(
                    'maininterface/windoweffects_module.hpp',
                    'maininterface/kwindowsystem_module.cpp',
                ),
                'dependencies' : [kf6WindowSystem_dep, qt6_dep]
            }
        endif
    endif

    if get_option('update-check').allowed() and gcrypt_dep.found()
        qt_extra_flags += '-DUPDATE_CHECK'
    endif


    vlc_modules += {
        'name' : 'qt_x11',
        'include_directories' : qt_include_dir,
        'sources' : files(
            'maininterface/qt_x11_plugin.c',
            'maininterface/compositor_x11_utils.hpp',
            'maininterface/compositor_x11_utils.cpp',
            'util/csdmenu_module.h',
            'util/csdmenu_x11.cpp',
            'util/csdmenu_x11.h'
        ),
        'dependencies' : [xcb_dep, xcb_render_dep, xcb_xfixes_dep],
        'enabled': xcb_dep.found() and xcb_render_dep.found() and xcb_xfixes_dep.found(),
    }

    qt6pre_files = qt6.preprocess(ui_files: ui_sources,
        moc_headers: moc_headers,
        moc_extra_arguments: qt_extra_flags,
        include_directories: qt_include_dir,
        dependencies: qt6_dep,
        rcc_extra_arguments: qt_rcc_extra_arguments)

    # TODO support qmlcachegen

    vlc_modules += {
        'name' : 'qt',
        'sources' : [qt6pre_files, qt6pre_qrc, shader_targets, shaders_processed, qt_sources, qt_plugin_sources],
        'extra_files' : [shader_files],
        'dependencies' : [qt_extra_deps, qt6_dep, corefoundation_dep],
        'include_directories' : qt_include_dir,
        'c_args' : qt_extra_flags,
        'cpp_args' : [qt_extra_flags, qt_cppargs],
        'link_args' : qt_link_args,
        'link_with' : qmllibs,
    }

    vlc_tests += {
        'name': 'base_model_test',
        'sources': files(
            'tests/test_base_model.cpp',
            'util/base_model.cpp',
            'util/base_model.hpp',
            'util/base_model_p.hpp'),
        'moc_headers': files(
            'util/base_model.hpp',
            'util/listcache.hpp',
            'util/locallistcacheloader.hpp'),
        'suite': ['qt'],
        'link_with': [libvlccore],
        'dependencies': [qt6_dep, qt_extra_deps],
    }

    qtest_qt6_dep = dependency('qt6', version: '=' + qt6_dep.version(), modules: ['Test'], required: false)
    if qtest_qt6_dep.found()
        vlc_tests += {
            'name': 'test_qt_ml_model',
            'sources': files(
                'tests/test_ml_model.cpp',
                'tests/vlc_stub_modules.cpp',
                'util/base_model.cpp',
                'medialibrary/mlbasemodel.cpp',
                'medialibrary/medialib.cpp',
                'medialibrary/mlthreadpool.cpp',
                'playlist/playlist_controller.cpp',
                'playlist/playlist_item.cpp',
            ),
            'moc_sources': files(
                'tests/test_ml_model.cpp'
            ),
            'moc_headers': files(
                'tests/vlc_stub_modules.cpp',
	            'util/base_model.hpp',
	            'util/listcache.hpp',
	            'util/locallistcacheloader.hpp',
	            'medialibrary/mlqmltypes.hpp',
	            'medialibrary/mlbasemodel.hpp',
	            'medialibrary/medialib.hpp',
	            'medialibrary/mlthreadpool.hpp',
	            'playlist/playlist_controller.hpp',
	            'playlist/playlist_item.hpp',
            ),
            'suite': ['qt'],
            'include_directories' : qt_include_dir,
            'link_with': [libvlccore, libvlc],
            'dependencies': [qt6_dep, qt_extra_deps, qtest_qt6_dep],
        }

        vlc_tests += {
            'name': 'test_qt_renderer_manager',
            'sources': files(
                'tests/vlc_stub_modules.cpp',
                'tests/test_renderer_manager_model.cpp',
                'util/renderer_manager.hpp',
                'util/renderer_manager.cpp'
            ),
            'moc_sources': files(
                'tests/test_renderer_manager_model.cpp'
            ),
            'moc_headers': files(
                'util/renderer_manager.hpp'
            ),
            'suite': ['qt'],
            'include_directories' : qt_include_dir,
            'link_with': [libvlccore, libvlc],
            'dependencies': [qt6_dep, qt_extra_deps, qtest_qt6_dep]
        }

        vlc_tests += {
            'name': 'test_qt_thread_runner',
            'sources': files(
                'tests/test_thread_runner.cpp',
                'medialibrary/mlthreadpool.cpp',
                'medialibrary/mlthreadpool.hpp'
            ),
            'moc_sources': files(
                'tests/test_thread_runner.cpp'
            ),
            'moc_headers': files(
                'medialibrary/mlthreadpool.hpp'
            ),
            'suite': ['qt'],
            'include_directories' : qt_include_dir,
            'dependencies': [qt6_dep, qtest_qt6_dep]
        }


        vlc_tests += {
            'name': 'test_qt_dialog_model',
            'sources': files(
                'tests/vlc_stub_modules.cpp',
                'tests/test_vlc_dialog_model.cpp',
                'dialogs/dialogs/dialogmodel.hpp',
                'dialogs/dialogs/dialogmodel.cpp'
            ),
            'moc_sources': files(
                'tests/test_vlc_dialog_model.cpp'
            ),
            'moc_headers': files(
                'dialogs/dialogs/dialogmodel.hpp'
            ),
            'suite': ['qt'],
            'include_directories' : qt_include_dir,
            'link_with': [libvlccore, libvlc],
            'dependencies': [qt6_dep, qt_extra_deps, qtest_qt6_dep],
        }
    endif

    quicktest_qt6_dep = dependency('qt6', version: '=' + qt6_dep.version(), modules: ['QuickTest'], required: false)
    if quicktest_qt6_dep.found() and  qtest_qt6_dep.found()
        qml_test_moc = qt6.compile_moc(
            headers : files('tests/qml_test.hpp'),
            dependencies : [quicktest_qt6_dep, qt6_dep]
        )
        qml_test = executable(
          'qml_test',
          [files('tests/qml_test.cpp', 'tests/qml_test.hpp'), qml_test_moc],
          qt6pre_qrc,
          build_by_default: false,
          dependencies: [quicktest_qt6_dep, qt6_dep, qtest_qt6_dep],
          link_with: qmllibs,
          cpp_args: ['-DQUICK_TEST_SOURCE_DIR="' + meson.current_source_dir() + '/tests"']
        )
        foreach test_name, test_file : qml_tests
              test(test_name, qml_test, args: ['-input', test_file], suite:'qt')
        endforeach
    endif

endif
