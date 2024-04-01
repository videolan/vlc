#
# Qt-based interface module
#

qt6 = import('qt6')

qt_include_dir = include_directories('.')

qt_extra_deps = []
qt_extra_flags = ['-DQT_STRICT_ITERATORS']
qt_cppargs = []
qt_link_args = []

qt6_dep = dependency('qt6',
    # Note that Qt < 6.2.5 does not provide pc files!
    version: '>=6.2',
    modules: [
        'Core', 'Gui', 'Widgets', 'Svg', 'Qml', 'QmlModels',
        'QuickLayouts', 'QuickTemplates2', 'QmlWorkerScript',
        'Quick', 'QuickControls2', 'ShaderTools'
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
    'dialogs/errors/errors.hpp',
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
    'dialogs/plugins/addons_manager.hpp',
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
    'dialogs/toolbar/controlbar_profile.hpp',
    'dialogs/toolbar/controlbar_profile_model.hpp',
    'dialogs/playlists/playlists.hpp',
    'dialogs/vlm/vlm.hpp',
    'maininterface/compositor.hpp',
    'maininterface/compositor_common.hpp',
    'maininterface/compositor_dummy.hpp',
    'maininterface/interface_window_handler.hpp',
    'maininterface/mainctx.hpp',
    'maininterface/mainctx_submodels.hpp',
    'maininterface/mainui.hpp',
    'maininterface/videosurface.hpp',
    'maininterface/video_window_handler.hpp',
    'medialibrary/medialib.hpp',
    'medialibrary/mlalbum.hpp',
    'medialibrary/mlalbummodel.hpp',
    'medialibrary/mlalbumtrack.hpp',
    'medialibrary/mlalbumtrackmodel.hpp',
    'medialibrary/mlartist.hpp',
    'medialibrary/mlartistmodel.hpp',
    'medialibrary/mlbookmarkmodel.hpp',
    'medialibrary/mlbasemodel.hpp',
    'medialibrary/mlfoldersmodel.hpp',
    'medialibrary/mlgenremodel.hpp',
    'medialibrary/mlthreadpool.hpp',
    'medialibrary/mlqmltypes.hpp',
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
    'network/networkdevicemodel.hpp',
    'network/networksourcesmodel.hpp',
    'network/networkmediamodel.hpp',
    'network/servicesdiscoverymodel.hpp',
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
    'util/base_model.hpp',
    'util/color_scheme_model.hpp',
    'util/color_svg_image_provider.hpp',
    'util/vlcaccess_image_provider.hpp',
    'util/csdbuttonmodel.hpp',
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
    'util/vlctick.hpp',
    'util/dismiss_popup_event_filter.hpp',
    'util/list_selection_model.hpp',
    'widgets/native/animators.hpp',
    'widgets/native/csdthemeimage.hpp',
    'widgets/native/customwidgets.hpp',
    'widgets/native/interface_widgets.hpp',
    'widgets/native/navigation_attached.hpp',
    'widgets/native/mlfolderseditor.hpp',
    'widgets/native/roundimage.hpp',
    'widgets/native/roundimage_p.hpp',
    'widgets/native/searchlineedit.hpp',
    'widgets/native/viewblockingrectangle.hpp',
)

if host_system == 'windows'
    moc_headers += files(
        'maininterface/mainctx_win32.hpp',
        'maininterface/compositor_win7.hpp',
    )
    if cdata.has('HAVE_DCOMP_H')
        moc_headers += files(
            'maininterface/compositor_dcomp.hpp',
            'maininterface/compositor_dcomp_acrylicsurface.hpp',
        )
    endif
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

some_sources = files(
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
    'dialogs/errors/errors.cpp',
    'dialogs/errors/errors.hpp',
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
    'dialogs/plugins/addons_manager.cpp',
    'dialogs/plugins/addons_manager.hpp',
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
    'medialibrary/medialib.cpp',
    'medialibrary/medialib.hpp',
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
    'network/mediatreelistener.cpp',
    'network/mediatreelistener.hpp',
    'network/networkdevicemodel.cpp',
    'network/networkdevicemodel.hpp',
    'network/networksourcesmodel.cpp',
    'network/networksourcesmodel.hpp',
    'network/networkmediamodel.cpp',
    'network/networkmediamodel.hpp',
    'network/servicesdiscoverymodel.cpp',
    'network/servicesdiscoverymodel.hpp',
    'network/standardpathmodel.cpp',
    'network/standardpathmodel.hpp',
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
    'util/base_model.cpp',
    'util/base_model.hpp',
    'util/base_model_p.hpp',
    'util/color_scheme_model.cpp',
    'util/color_scheme_model.hpp',
    'util/color_svg_image_provider.cpp',
    'util/color_svg_image_provider.hpp',
    'util/vlcaccess_image_provider.cpp',
    'util/vlcaccess_image_provider.hpp',
    'util/covergenerator.cpp',
    'util/covergenerator.hpp',
    'util/csdbuttonmodel.cpp',
    'util/csdbuttonmodel.hpp',
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
    'util/shared_input_item.hpp',
    'util/fast_gaussian_blur_template.h',
    'util/effects_image_provider.cpp',
    'util/effects_image_provider.hpp',
    'util/qsgroundedrectangularimagenode.cpp',
    'util/qsgroundedrectangularimagenode.hpp',
    'util/dismiss_popup_event_filter.cpp',
    'util/dismiss_popup_event_filter.hpp',
    'util/list_selection_model.cpp',
    'util/list_selection_model.hpp',
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
    'widgets/native/roundimage.cpp',
    'widgets/native/roundimage.hpp',
    'widgets/native/searchlineedit.cpp',
    'widgets/native/searchlineedit.hpp',
    'widgets/native/viewblockingrectangle.cpp',
    'widgets/native/viewblockingrectangle.hpp',
)

if host_system == 'windows'
    some_sources += files(
        'maininterface/mainctx_win32.cpp',
        'maininterface/mainctx_win32.hpp',
        'maininterface/compositor_win7.cpp',
        'maininterface/compositor_win7.hpp',
        'style/windowsthemeprovider.cpp',
    )

    if cdata.has('HAVE_DCOMP_H')
        some_sources += files(
            'maininterface/compositor_dcomp.cpp',
            'maininterface/compositor_dcomp_acrylicsurface.cpp',
        )
    endif
endif

if (x11_dep.found() and
    xcb_dep.found() and
    xcb_damage_dep.found() and
    xcb_xfixes_dep.found())
    some_sources += files(
        'maininterface/compositor_x11.cpp',
        'maininterface/compositor_x11_renderclient.cpp',
        'maininterface/compositor_x11_renderwindow.cpp',
        'maininterface/compositor_x11_uisurface.cpp',
        'maininterface/compositor_x11_utils.cpp',
    )
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

qrc_files = files('vlc.qrc')
if host_system == 'windows'
  qrc_files += files('windows.qrc')
endif

qml_tests = {
    'tst_FSM': files('tests/tst_FSM.qml'),
}

if qt6_dep.found()
    qt6pre_qrc = qt6.preprocess(
        qresources: qrc_files,
        include_directories: qt_include_dir,
        dependencies: qt6_dep)

    qt6pre_files = qt6.preprocess(ui_files: ui_sources,
        moc_headers: moc_headers,
        include_directories: qt_include_dir,
        dependencies: qt6_dep)

    qt_bin_directory = qt6_dep.get_variable(pkgconfig: 'bindir')

    qt_sources = files('qt.cpp')

    subdir('shaders')

    # Qt plugins
    qt_static_check = '''
    #include <qconfig.h>
    #ifndef QT_STATIC
    #error Qt using shared libraries
    #endif
    '''
    if cpp.compiles(qt_static_check, dependencies: qt6_dep, name: 'Qt6 compiled with static libraries')
        qt_extra_deps += dependency('qtvlcdeps', required: true)
    endif

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

    if cpp.has_header('private/qobject_p.h', dependencies: qt6_dep)
        qt_extra_flags += '-DQT_CORE_PRIVATE'
    endif

    if host_system == 'windows'
        qt_cppargs += libcom_cppflags
        qt_extra_flags += '-DQPNI_HEADER=<' + qt6_dep.version() + '/QtGui/qpa/qplatformnativeinterface.h>'
    endif

    if not (host_system in ['windows', 'darwin'])
        #Qt GTK theme module
        gtk_dep = dependency(
            'gtk+-3.0',
            version: '>=3.20',
            required: get_option('qt_gtk')
        )

        if gtk_dep.found()
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
                'dependencies' : [gtk_dep]
            }

            qt_extra_flags += '-DQT_HAS_GTK'
        endif
    endif

    # TODO support qmlcachegen

    vlc_modules += {
        'name' : 'qt',
        'sources' : [qt6pre_files, qt6pre_qrc, shader_targets, shaders_processed, qt_sources, some_sources],
        'extra_files' : [shader_files],
        'dependencies' : [qt_extra_deps, qt6_dep],
        'include_directories' : qt_include_dir,
        'c_args' : qt_extra_flags,
        'cpp_args' : [qt_extra_flags, qt_cppargs],
        'link_args' : qt_link_args
    }

    test_qt6_dep = dependency('qt6', modules: ['QuickTest'], required: false)
    if test_qt6_dep.found()
        qml_test = executable(
          'qml_test',
          files('tests/qml_test.cpp'),
          qt6pre_qrc,
          build_by_default: false,
          dependencies: [test_qt6_dep],
          cpp_args: ['-DQUICK_TEST_SOURCE_DIR="' + meson.current_source_dir() + '/tests"']
        )
        foreach test_name, test_file : qml_tests
              test(test_name, qml_test, args: ['-input', test_file], suite:'qt')
        endforeach
    endif

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
endif

