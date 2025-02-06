#include "mainui.hpp"

#include <cassert>

#include "medialibrary/medialib.hpp"
#include "medialibrary/mlqmltypes.hpp"
#include "medialibrary/mlcustomcover.hpp"
#include "medialibrary/mlalbummodel.hpp"
#include "medialibrary/mlartistmodel.hpp"
#include "medialibrary/mlalbumtrackmodel.hpp"
#include "medialibrary/mlgenremodel.hpp"
#include "medialibrary/mlurlmodel.hpp"
#include "medialibrary/mlvideomodel.hpp"
#include "medialibrary/mlrecentsmodel.hpp"
#include "medialibrary/mlrecentsvideomodel.hpp"
#include "medialibrary/mlfoldersmodel.hpp"
#include "medialibrary/mlvideogroupsmodel.hpp"
#include "medialibrary/mlvideofoldersmodel.hpp"
#include "medialibrary/mlplaylistlistmodel.hpp"
#include "medialibrary/mlplaylistmodel.hpp"
#include "medialibrary/mlplaylist.hpp"
#include "medialibrary/mlbookmarkmodel.hpp"

#include "player/player_controller.hpp"
#include "player/player_controlbar_model.hpp"
#include "player/control_list_model.hpp"
#include "player/control_list_filter.hpp"
#include "player/delay_estimator.hpp"

#include "dialogs/toolbar/controlbar_profile_model.hpp"
#include "dialogs/toolbar/controlbar_profile.hpp"

#include "playlist/playlist_model.hpp"
#include "playlist/playlist_controller.hpp"

#include "util/csdmenu.hpp"
#include "util/item_key_event_filter.hpp"
#include "util/imageluminanceextractor.hpp"
#include "util/keyhelper.hpp"
#include "style/systempalette.hpp"
#include "util/navigation_history.hpp"
#include "util/flickable_scroll_handler.hpp"
#include "util/color_svg_image_provider.hpp"
#include "util/effects_image_provider.hpp"
#include "util/vlcaccess_image_provider.hpp"
#include "util/csdbuttonmodel.hpp"
#include "util/vlctick.hpp"
#include "util/list_selection_model.hpp"
#include "util/ui_notifier.hpp"

#include "dialogs/help/aboutmodel.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "dialogs/dialogs/dialogmodel.hpp"
#include "util/vlchotkeyconverter.hpp"

#include "network/networkmediamodel.hpp"
#include "network/networkdevicemodel.hpp"
#include "network/networksourcesmodel.hpp"
#include "network/addonsmodel.hpp"
#include "network/standardpathmodel.hpp"

#include "menus/qml_menu_wrapper.hpp"

#include "widgets/native/textureprovideritem.hpp"
#include "widgets/native/csdthemeimage.hpp"
#include "widgets/native/navigation_attached.hpp"
#include "widgets/native/viewblockingrectangle.hpp"
#if QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
#include "widgets/native/doubleclickignoringitem.hpp"
#else
// QQuickItem already ignores double click, starting
// with Qt 6.4.0:
#define DoubleClickIgnoringItem QQuickItem
#endif

#include "videosurface.hpp"
#include "mainctx.hpp"
#include "mainctx_submodels.hpp"

#include <QScreen>

using  namespace vlc::playlist;

MainUI::MainUI(qt_intf_t *p_intf, MainCtx *mainCtx, QWindow* interfaceWindow,  QObject *parent)
    : QObject(parent)
    , m_intf(p_intf)
    , m_mainCtx(mainCtx)
    , m_interfaceWindow(interfaceWindow)
{
    assert(m_intf);
    assert(m_mainCtx);
    assert(m_interfaceWindow);

    registerQMLTypes();
}

MainUI::~MainUI()
{
    qmlClearTypeRegistrations();
}

bool MainUI::setup(QQmlEngine* engine)
{
    if (m_mainCtx->hasMediaLibrary())
    {
        engine->addImageProvider(MLCustomCover::providerId, new MLCustomCover(m_mainCtx->getMediaLibrary()));
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    engine->addImportPath(":/qt/qml");
#endif

    if (!EffectsImageProvider::instance(engine, engine))
        new EffectsImageProvider(engine);
    engine->addImageProvider(QStringLiteral("svgcolor"), new SVGColorImageImageProvider());
    engine->addImageProvider(QStringLiteral("vlcaccess"), new VLCAccessImageProvider());

    m_component  = new QQmlComponent(engine, QStringLiteral("qrc:/qt/qml/VLC/MainInterface/MainInterface.qml"), QQmlComponent::PreferSynchronous, engine);
    if (m_component->isLoading())
    {
        msg_Warn(m_intf, "component is still loading");
    }

    if (m_component->isError())
    {
        for(auto& error: m_component->errors())
            msg_Err(m_intf, "qml loading %s %s:%u", qtu(error.description()), qtu(error.url().toString()), error.line());
#ifdef QT_STATIC
            assert( !"Missing qml modules from qt contribs." );
#else
            msg_Err( m_intf, "Install missing modules using your packaging tool" );
#endif
        return false;
    }
    return true;
}

QQuickItem* MainUI::createRootItem()
{
    QObject* rootObject = m_component->create();

    if (m_component->isError())
    {
        for(auto& error: m_component->errors())
            msg_Err(m_intf, "qml loading %s %s:%u", qtu(error.description()), qtu(error.url().toString()), error.line());
        return nullptr;
    }

    if (rootObject == nullptr)
    {
        msg_Err(m_intf, "unable to create main interface, no root item");
        return nullptr;
    }
    m_rootItem = qobject_cast<QQuickItem*>(rootObject);
    if (!m_rootItem)
    {
        msg_Err(m_intf, "unexpected type of qml root item");
        return nullptr;
    }

    return m_rootItem;
}

void MainUI::registerQMLTypes()
{
    {
        const char* uri = "VLC.MainInterface";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.MainInterface
        qmlRegisterSingletonInstance<MainCtx>(uri, versionMajor, versionMinor, "MainCtx", m_mainCtx);
        qmlRegisterUncreatableType<SearchCtx>(uri, versionMajor, versionMinor, "SearchCtx", "");
        qmlRegisterUncreatableType<SortCtx>(uri, versionMajor, versionMinor, "SortCtx", "");
        qmlRegisterUncreatableType<UINotifier>(uri, versionMajor, versionMinor, "UINotifier", "");
        qmlRegisterSingletonInstance<UINotifier>(uri, versionMajor, versionMinor, "UINotifier", new UINotifier(m_mainCtx, m_mainCtx));
        qmlRegisterSingletonInstance<NavigationHistory>(uri, versionMajor, versionMinor, "History", new NavigationHistory(this));
        qmlRegisterUncreatableType<QAbstractItemModel>(uri, versionMajor, versionMinor, "QtAbstractItemModel", "");
        qmlRegisterUncreatableType<QWindow>(uri, versionMajor, versionMinor, "QtWindow", "");
        qmlRegisterUncreatableType<QScreen>(uri, versionMajor, versionMinor, "QtScreen", "");
        qmlRegisterUncreatableType<VLCTick>(uri, versionMajor, versionMinor, "vlcTick", "");
        qmlRegisterType<VideoSurface>(uri, versionMajor, versionMinor, "VideoSurface");
        qmlRegisterUncreatableType<BaseModel>( uri, versionMajor, versionMinor, "BaseModel", "Base Model is uncreatable." );
        qmlRegisterUncreatableType<VLCVarChoiceModel>(uri, versionMajor, versionMinor, "VLCVarChoiceModel", "generic variable with choice model" );
        qmlRegisterUncreatableType<CSDButton>(uri, versionMajor, versionMinor, "CSDButton", "");
        qmlRegisterUncreatableType<CSDButtonModel>(uri, versionMajor, versionMinor, "CSDButtonModel", "has CSD buttons and provides for communicating CSD events between UI and backend");
        qmlRegisterTypesAndRevisions<CSDMenu>( uri, versionMajor);
        qmlRegisterUncreatableType<NavigationAttached>( uri, versionMajor, versionMinor, "Navigation", "Navigation is only available via attached properties");
        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Dialogs";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Dialogs
        qmlRegisterType<AboutModel>( uri, versionMajor, versionMinor, "AboutModel" );
        qmlRegisterType<VLCDialog>( uri, versionMajor, versionMinor, "VLCDialog" );
        assert(VLCDialogModel::getInstance<false>());
        qmlRegisterSingletonInstance<VLCDialogModel>(uri, versionMajor, versionMinor, "VLCDialogModel", VLCDialogModel::getInstance<false>());
        qmlRegisterUncreatableType<DialogId>( uri, versionMajor, versionMinor, "dialogId", "");
        assert(DialogsProvider::getInstance());
        qmlRegisterSingletonInstance<DialogsProvider>(uri, versionMajor, versionMinor, "DialogsProvider", DialogsProvider::getInstance());
        assert(DialogErrorModel::getInstance<false>());
        qmlRegisterSingletonInstance<DialogErrorModel>(uri, versionMajor, versionMinor, "DialogErrorModel", DialogErrorModel::getInstance<false>());

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Menus";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Menus
        qmlRegisterType<StringListMenu>( uri, versionMajor, versionMinor, "StringListMenu" );
        qmlRegisterType<SortMenu>( uri, versionMajor, versionMinor, "SortMenu" );
        qmlRegisterType<SortMenuVideo>( uri, versionMajor, versionMinor, "SortMenuVideo" );
        qmlRegisterType<QmlGlobalMenu>( uri, versionMajor, versionMinor, "QmlGlobalMenu" );
        qmlRegisterType<QmlMenuBar>( uri, versionMajor, versionMinor, "QmlMenuBar" );

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Player";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Player
        qmlRegisterUncreatableType<TrackListModel>(uri, versionMajor, versionMinor, "TrackListModel", "available tracks of a media (audio/video/sub)" );
        qmlRegisterUncreatableType<TitleListModel>(uri, versionMajor, versionMinor, "TitleListModel", "available titles of a media" );
        qmlRegisterUncreatableType<ChapterListModel>(uri, versionMajor, versionMinor, "ChapterListModel", "available chapters of a media" );
        qmlRegisterUncreatableType<ProgramListModel>(uri, versionMajor, versionMinor, "ProgramListModel", "available programs of a media" );
        assert(m_intf->p_mainPlayerController);
        qmlRegisterSingletonInstance<PlayerController>(uri, versionMajor, versionMinor, "Player", m_intf->p_mainPlayerController);

        qmlRegisterType<QmlBookmarkMenu>( uri, versionMajor, versionMinor, "QmlBookmarkMenu" );
        qmlRegisterType<QmlProgramMenu>( uri, versionMajor, versionMinor, "QmlProgramMenu" );
        qmlRegisterType<QmlRendererMenu>( uri, versionMajor, versionMinor, "QmlRendererMenu" );
        qmlRegisterType<QmlSubtitleMenu>( uri, versionMajor, versionMinor, "QmlSubtitleMenu" );
        qmlRegisterType<QmlAudioMenu>( uri, versionMajor, versionMinor, "QmlAudioMenu" );
        qmlRegisterType<QmlAudioContextMenu>( uri, versionMajor, versionMinor, "QmlAudioContextMenu" );

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.PlayerControls";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.PlayerControls
        qmlRegisterUncreatableType<ControlbarProfileModel>(uri, versionMajor, versionMinor, "ControlbarProfileModel", "");
        qmlRegisterUncreatableType<ControlbarProfile>(uri, versionMajor, versionMinor, "ControlbarProfile", "");
        qmlRegisterUncreatableType<PlayerControlbarModel>(uri, versionMajor, versionMinor, "PlayerControlbarModel", "");
        qmlRegisterUncreatableType<ControlListModel>( uri, versionMajor, versionMinor, "ControlListModel", "" );
        qmlRegisterType<ControlListFilter>(uri, versionMajor, versionMinor, "ControlListFilter");
        qmlRegisterSingletonType(uri, versionMajor, versionMinor, "PlayerListModel", PlayerControlbarModel::getPlaylistIdentifierListModel);


        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Playlist";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Playlist
        qmlRegisterUncreatableType<PlaylistItem>(uri, versionMajor, versionMinor, "playlistItem", "");
        qmlRegisterType<PlaylistListModel>( uri, versionMajor, versionMinor, "PlaylistListModel" );
        qmlRegisterType<PlaylistController>( uri, versionMajor, versionMinor, "PlaylistController" );
        qmlRegisterType<PlaylistContextMenu>( uri, versionMajor, versionMinor, "PlaylistContextMenu" );
        assert(m_intf->p_mainPlaylistController);
        qmlRegisterSingletonInstance<PlaylistController>(uri, versionMajor, versionMinor, "MainPlaylistController", m_intf->p_mainPlaylistController);

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Network";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Network
        qmlRegisterType<NetworkMediaModel>( uri, versionMajor, versionMinor, "NetworkMediaModel");
        qmlRegisterType<NetworkDeviceModel>( uri, versionMajor, versionMinor, "NetworkDeviceModel");
        qmlRegisterType<NetworkSourcesModel>( uri, versionMajor, versionMinor, "NetworkSourcesModel");
        qmlRegisterType<AddonsModel>( uri, versionMajor, versionMinor, "ServicesDiscoveryModel");
        qmlRegisterType<StandardPathModel>( uri, versionMajor, versionMinor, "StandardPathModel");
        qmlRegisterType<MLFoldersModel>( uri, versionMajor, versionMinor, "MLFolderModel");

        qmlRegisterType<NetworkMediaContextMenu>( uri, versionMajor, versionMinor, "NetworkMediaContextMenu" );
        qmlRegisterType<NetworkDeviceContextMenu>( uri, versionMajor, versionMinor, "NetworkDeviceContextMenu" );

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Style";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Style
        qmlRegisterUncreatableType<ColorSchemeModel>(uri, versionMajor, versionMinor, "ColorSchemeModel", "");
        qmlRegisterType<ColorContext>(uri, versionMajor, versionMinor, "ColorContext");
        qmlRegisterUncreatableType<ColorProperty>(uri, versionMajor, versionMinor, "colorProperty", "");
        qmlRegisterType<SystemPalette>(uri, versionMajor, versionMinor, "SystemPalette");

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Util";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Util
        qmlRegisterSingletonInstance<QmlKeyHelper>(uri, versionMajor, versionMinor, "KeyHelper", new QmlKeyHelper(this));
        qmlRegisterSingletonType<EffectsImageProvider>(uri, versionMajor, versionMinor, "Effects", EffectsImageProvider::instance);
        qmlRegisterUncreatableType<SVGColorImageBuilder>(uri, versionMajor, versionMinor, "SVGColorImageBuilder", "");
        qmlRegisterSingletonInstance<SVGColorImage>(uri, versionMajor, versionMinor, "SVGColorImage", new SVGColorImage(this));
        qmlRegisterSingletonInstance<VLCAccessImage>(uri, versionMajor, versionMinor, "VLCAccessImage", new VLCAccessImage(this));
        qmlRegisterType<DelayEstimator>( uri, versionMajor, versionMinor, "DelayEstimator" );
        qmlRegisterTypesAndRevisions<WheelToVLCConverter>( uri, versionMajor );

        qmlRegisterType<ImageLuminanceExtractor>( uri, versionMajor, versionMinor, "ImageLuminanceExtractor");

        qmlRegisterType<ItemKeyEventFilter>( uri, versionMajor, versionMinor, "KeyEventFilter" );
        qmlRegisterType<FlickableScrollHandler>( uri, versionMajor, versionMinor, "FlickableScrollHandler" );
        qmlRegisterType<ListSelectionModel>( uri, versionMajor, versionMinor, "ListSelectionModel" );
        qmlRegisterType<DoubleClickIgnoringItem>( uri, versionMajor, versionMinor, "DoubleClickIgnoringItem" );

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    {
        const char* uri = "VLC.Widgets";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.Widgets
        qmlRegisterType<CSDThemeImage>(uri, versionMajor, versionMinor, "CSDThemeImage");
        qmlRegisterType<ViewBlockingRectangle>( uri, versionMajor, versionMinor, "ViewBlockingRectangle" );
        qmlRegisterType<TextureProviderItem>( uri, versionMajor, versionMinor, "TextureProviderItem" );

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

    if (m_mainCtx->hasMediaLibrary())
    {
        const char* uri = "VLC.MediaLibrary";
        const int versionMajor = 1;
        const int versionMinor = 0;

        // @uri VLC.MediaLibrary
        assert(m_mainCtx->getMediaLibrary());
        qmlRegisterSingletonInstance<MediaLib>(uri, versionMajor, versionMinor, "MediaLib", m_mainCtx->getMediaLibrary());

        qmlRegisterUncreatableType<MLItemId>( uri, versionMajor, versionMinor, "mediaId", "");
        qmlRegisterUncreatableType<MLBaseModel>( uri, versionMajor, versionMinor, "MLBaseModel", "ML Base Model is uncreatable." );
        qmlRegisterType<MLAlbumModel>( uri, versionMajor, versionMinor, "MLAlbumModel" );
        qmlRegisterType<MLArtistModel>( uri, versionMajor, versionMinor, "MLArtistModel" );
        qmlRegisterType<MLAlbumTrackModel>( uri, versionMajor, versionMinor, "MLAlbumTrackModel" );
        qmlRegisterType<MLGenreModel>( uri, versionMajor, versionMinor, "MLGenreModel" );
        qmlRegisterType<MLUrlModel>( uri, versionMajor, versionMinor, "MLUrlModel" );
        qmlRegisterType<MLVideoModel>( uri, versionMajor, versionMinor, "MLVideoModel" );
        qmlRegisterType<MLRecentsVideoModel>( uri, versionMajor, versionMinor, "MLRecentsVideoModel" );
        qmlRegisterType<MLVideoGroupsModel>( uri, versionMajor, versionMinor, "MLVideoGroupsModel" );
        qmlRegisterType<MLVideoFoldersModel>( uri, versionMajor, versionMinor, "MLVideoFoldersModel" );
        qmlRegisterType<MLPlaylistListModel>( uri, versionMajor, versionMinor, "MLPlaylistListModel" );
        qmlRegisterType<MLPlaylistModel>( uri, versionMajor, versionMinor, "MLPlaylistModel" );
        qmlRegisterType<MLBookmarkModel>( uri, versionMajor, versionMinor, "MLBookmarkModel" );

        qmlRegisterType<NetworkMediaModel>( uri, versionMajor, versionMinor, "NetworkMediaModel");
        qmlRegisterType<NetworkDeviceModel>( uri, versionMajor, versionMinor, "NetworkDeviceModel");
        qmlRegisterType<NetworkSourcesModel>( uri, versionMajor, versionMinor, "NetworkSourcesModel");
        qmlRegisterType<AddonsModel>( uri, versionMajor, versionMinor, "ServicesDiscoveryModel");
        qmlRegisterType<MLFoldersModel>( uri, versionMajor, versionMinor, "MLFolderModel");
        qmlRegisterType<MLRecentsModel>( uri, versionMajor, versionMinor, "MLRecentModel" );

        qmlRegisterType<PlaylistListContextMenu>( uri, versionMajor, versionMinor, "PlaylistListContextMenu" );
        qmlRegisterType<PlaylistMediaContextMenu>( uri, versionMajor, versionMinor, "PlaylistMediaContextMenu" );

        qmlRegisterModule(uri, versionMajor, versionMinor);
        qmlProtectModule(uri, versionMajor);
    }

#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    // Dummy QtQuick.Effects module
    qmlRegisterModule("QtQuick.Effects", 0, 0);
    // Do not protect, types can still be registered.
#else
    // Dummy Qt5Compat.GraphicalEffects module
    qmlRegisterModule("Qt5Compat.GraphicalEffects", 0, 0);
    // Do not protect, types can still be registered.
#endif
}
