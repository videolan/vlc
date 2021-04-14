#include "mainui.hpp"

#include <cassert>

#include "medialibrary/medialib.hpp"
#include "medialibrary/mlqmltypes.hpp"
#include "medialibrary/mlalbummodel.hpp"
#include "medialibrary/mlartistmodel.hpp"
#include "medialibrary/mlalbumtrackmodel.hpp"
#include "medialibrary/mlgenremodel.hpp"
#include "medialibrary/mlurlmodel.hpp"
#include "medialibrary/mlvideomodel.hpp"
#include "medialibrary/mlrecentsmodel.hpp"
#include "medialibrary/mlrecentsvideomodel.hpp"
#include "medialibrary/mlfoldersmodel.hpp"
#include "medialibrary/mlgrouplistmodel.hpp"
#include "medialibrary/mlplaylistlistmodel.hpp"
#include "medialibrary/mlplaylistmodel.hpp"
#include "medialibrary/mlplaylist.hpp"

#include "player/player_controller.hpp"
#include "player/player_controlbar_model.hpp"
#include "player/control_list_model.hpp"

#include "dialogs/toolbar/controlbar_profile_model.hpp"
#include "dialogs/toolbar/controlbar_profile.hpp"

#include "playlist/playlist_model.hpp"
#include "playlist/playlist_controller.hpp"

#include "util/qml_main_context.hpp"
#include "util/qmleventfilter.hpp"
#include "util/imageluminanceextractor.hpp"
#include "util/i18n.hpp"
#include "util/systempalette.hpp"
#include "util/sortfilterproxymodel.hpp"
#include "util/navigation_history.hpp"
#include "util/qmlinputitem.hpp"

#include "dialogs/help/aboutmodel.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "dialogs/dialogs/dialogmodel.hpp"

#include "network/networkmediamodel.hpp"
#include "network/networkdevicemodel.hpp"
#include "network/networksourcesmodel.hpp"
#include "network/servicesdiscoverymodel.hpp"

#include "maininterface/main_interface.hpp"

#include "menus/qml_menu_wrapper.hpp"

#include "widgets/native/roundimage.hpp"

#include "videosurface.hpp"

#include <QQuickWindow>
#include <QQmlContext>
#include <QQmlFileSelector>

using  namespace vlc::playlist;

namespace {

template<class T>
void registerAnonymousType( const char *uri, int versionMajor )
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    qmlRegisterAnonymousType<T>( uri, versionMajor );
#else
    qmlRegisterType<T>();
    VLC_UNUSED( uri );
    VLC_UNUSED( versionMajor );
#endif
}

} // anonymous namespace


MainUI::MainUI(intf_thread_t *p_intf, MainInterface *mainInterface, QWindow* interfaceWindow,  QObject *parent)
    : QObject(parent)
    , m_intf(p_intf)
    , m_mainInterface(mainInterface)
    , m_interfaceWindow(interfaceWindow)
{
    assert(m_intf);
    assert(m_mainInterface);
    assert(m_interfaceWindow);

    registerQMLTypes();
}

MainUI::~MainUI()
{

}

bool MainUI::setup(QQmlEngine* engine)
{
    engine->setOutputWarningsToStandardError(false);
    connect(engine, &QQmlEngine::warnings, this, &MainUI::onQmlWarning);

    QQmlContext *rootCtx = engine->rootContext();

    rootCtx->setContextProperty( "history", new NavigationHistory(this) );
    rootCtx->setContextProperty( "player", m_intf->p_sys->p_mainPlayerController );
    rootCtx->setContextProperty( "i18n", new I18n(this) );
    rootCtx->setContextProperty( "mainctx", new QmlMainContext(m_intf, m_mainInterface, this));
    rootCtx->setContextProperty( "mainInterface", m_mainInterface);
    rootCtx->setContextProperty( "topWindow", m_interfaceWindow);
    rootCtx->setContextProperty( "dialogProvider", DialogsProvider::getInstance());
    rootCtx->setContextProperty( "systemPalette", new SystemPalette(this));
    rootCtx->setContextProperty( "dialogModel", new DialogModel(m_intf, this));

    if (m_mainInterface->hasMediaLibrary())
        rootCtx->setContextProperty( "medialib", m_mainInterface->getMediaLibrary() );
    else
        rootCtx->setContextProperty( "medialib", nullptr );

    m_component  = new QQmlComponent(engine, QStringLiteral("qrc:/main/MainInterface.qml"), QQmlComponent::PreferSynchronous, engine);
    if (m_component->isLoading())
    {
        msg_Warn(m_intf, "component is still loading");
    }

    if (m_component->isError())
    {
        for(auto& error: m_component->errors())
            msg_Err(m_intf, "qml loading %s %s:%u", qtu(error.description()), qtu(error.url().toString()), error.line());
#ifdef QT_STATICPLUGIN
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
    qRegisterMetaType<VLCTick>();
    qmlRegisterUncreatableType<VLCTick>("org.videolan.vlc", 0, 1, "VLCTick", "");

    qRegisterMetaType<QmlInputItem>();

    qmlRegisterType<VideoSurface>("org.videolan.vlc", 0, 1, "VideoSurface");

    if (m_mainInterface->hasMediaLibrary())
    {
        qRegisterMetaType<MLItemId>();
        qmlRegisterType<MLAlbumModel>( "org.videolan.medialib", 0, 1, "MLAlbumModel" );
        qmlRegisterType<MLArtistModel>( "org.videolan.medialib", 0, 1, "MLArtistModel" );
        qmlRegisterType<MLAlbumTrackModel>( "org.videolan.medialib", 0, 1, "MLAlbumTrackModel" );
        qmlRegisterType<MLGenreModel>( "org.videolan.medialib", 0, 1, "MLGenreModel" );
        qmlRegisterType<MLUrlModel>( "org.videolan.medialib", 0, 1, "MLUrlModel" );
        qmlRegisterType<MLVideoModel>( "org.videolan.medialib", 0, 1, "MLVideoModel" );
        qmlRegisterType<MLRecentsVideoModel>( "org.videolan.medialib", 0, 1, "MLRecentsVideoModel" );
        qmlRegisterType<MLGroupListModel>( "org.videolan.medialib", 0, 1, "MLGroupListModel" );
        qmlRegisterType<MLPlaylistListModel>( "org.videolan.medialib", 0, 1, "MLPlaylistListModel" );
        qmlRegisterType<MLPlaylistModel>( "org.videolan.medialib", 0, 1, "MLPlaylistModel" );

        qRegisterMetaType<NetworkTreeItem>();
        qmlRegisterType<NetworkMediaModel>( "org.videolan.medialib", 0, 1, "NetworkMediaModel");
        qmlRegisterType<NetworkDeviceModel>( "org.videolan.medialib", 0, 1, "NetworkDeviceModel");
        qmlRegisterType<NetworkSourcesModel>( "org.videolan.medialib", 0, 1, "NetworkSourcesModel");
        qmlRegisterType<ServicesDiscoveryModel>( "org.videolan.medialib", 0, 1, "ServicesDiscoveryModel");
        qmlRegisterType<MLFoldersModel>( "org.videolan.medialib", 0, 1, "MLFolderModel");
        qmlRegisterType<MLRecentsModel>( "org.videolan.medialib", 0, 1, "MLRecentModel" );

        //expose base object, they aren't instanciable from QML side
        registerAnonymousType<MLAlbum>("org.videolan.medialib", 1);
        registerAnonymousType<MLArtist>("org.videolan.medialib", 1);
        registerAnonymousType<MLAlbumTrack>("org.videolan.medialib", 1);
        registerAnonymousType<MLGenre>("org.videolan.medialib", 1);
        registerAnonymousType<MLPlaylist>("org.videolan.medialib", 1);

        qmlRegisterType<AlbumContextMenu>( "org.videolan.medialib", 0, 1, "AlbumContextMenu" );
        qmlRegisterType<ArtistContextMenu>( "org.videolan.medialib", 0, 1, "ArtistContextMenu" );
        qmlRegisterType<GenreContextMenu>( "org.videolan.medialib", 0, 1, "GenreContextMenu" );
        qmlRegisterType<AlbumTrackContextMenu>( "org.videolan.medialib", 0, 1, "AlbumTrackContextMenu" );
        qmlRegisterType<URLContextMenu>( "org.videolan.medialib", 0, 1, "URLContextMenu" );
        qmlRegisterType<VideoContextMenu>( "org.videolan.medialib", 0, 1, "VideoContextMenu" );
        qmlRegisterType<GroupListContextMenu>( "org.videolan.medialib", 0, 1, "GroupListContextMenu" );
        qmlRegisterType<PlaylistListContextMenu>( "org.videolan.medialib", 0, 1, "PlaylistListContextMenu" );
        qmlRegisterType<PlaylistMediaContextMenu>( "org.videolan.medialib", 0, 1, "PlaylistMediaContextMenu" );
    }

    qRegisterMetaType<NetworkTreeItem>();
    qmlRegisterType<NetworkMediaModel>( "org.videolan.vlc", 0, 1, "NetworkMediaModel");
    qmlRegisterType<NetworkDeviceModel>( "org.videolan.vlc", 0, 1, "NetworkDeviceModel");
    qmlRegisterType<NetworkSourcesModel>( "org.videolan.vlc", 0, 1, "NetworkSourcesModel");
    qmlRegisterType<ServicesDiscoveryModel>( "org.videolan.vlc", 0, 1, "ServicesDiscoveryModel");
    qmlRegisterType<MLFoldersModel>( "org.videolan.vlc", 0, 1, "MLFolderModel");
    qmlRegisterType<ImageLuminanceExtractor>( "org.videolan.vlc", 0, 1, "ImageLuminanceExtractor");

    qmlRegisterUncreatableType<NavigationHistory>("org.videolan.vlc", 0, 1, "History", "Type of global variable history" );

    qmlRegisterUncreatableType<TrackListModel>("org.videolan.vlc", 0, 1, "TrackListModel", "available tracks of a media (audio/video/sub)" );
    qmlRegisterUncreatableType<TitleListModel>("org.videolan.vlc", 0, 1, "TitleListModel", "available titles of a media" );
    qmlRegisterUncreatableType<ChapterListModel>("org.videolan.vlc", 0, 1, "ChapterListModel", "available titles of a media" );
    qmlRegisterUncreatableType<ProgramListModel>("org.videolan.vlc", 0, 1, "ProgramListModel", "available programs of a media" );
    qmlRegisterUncreatableType<VLCVarChoiceModel>("org.videolan.vlc", 0, 1, "VLCVarChoiceModel", "generic variable with choice model" );
    qmlRegisterUncreatableType<PlayerController>("org.videolan.vlc", 0, 1, "PlayerController", "player controller" );

    qRegisterMetaType<PlaylistPtr>();
    qRegisterMetaType<PlaylistItem>();
    qmlRegisterUncreatableType<PlaylistItem>("org.videolan.vlc", 0, 1, "PlaylistItem", "");
    qmlRegisterType<PlaylistListModel>( "org.videolan.vlc", 0, 1, "PlaylistListModel" );
    qmlRegisterType<PlaylistControllerModel>( "org.videolan.vlc", 0, 1, "PlaylistControllerModel" );

    qmlRegisterType<AboutModel>( "org.videolan.vlc", 0, 1, "AboutModel" );

    qmlRegisterUncreatableType<DialogModel>("org.videolan.vlc", 0, 1, "DialogModel", "");
    qmlRegisterUncreatableType<DialogErrorModel>( "org.videolan.vlc", 0, 1, "DialogErrorModel", "");
    qRegisterMetaType<DialogId>();

    qmlRegisterType<QmlEventFilter>( "org.videolan.vlc", 0, 1, "EventFilter" );

    qRegisterMetaType<ControlbarProfile*>();
    qRegisterMetaType<ControlbarProfileModel*>();
    qmlRegisterUncreatableType<ControlbarProfile>("org.videolan.vlc", 0, 1, "ControlbarProfile", "");
    qmlRegisterUncreatableType<PlayerControlbarModel>("org.videolan.vlc", 0, 1, "PlayerControlbarModel", "");
    qmlRegisterUncreatableType<ControlListModel>( "org.videolan.vlc", 0, 1, "ControlListModel", "" );
    qmlRegisterSingletonType("org.videolan.vlc", 0, 1, "PlayerListModel", PlayerControlbarModel::getPlaylistIdentifierListModel);

    qRegisterMetaType<QmlMainContext*>();
    qmlRegisterType<QmlGlobalMenu>( "org.videolan.vlc", 0, 1, "QmlGlobalMenu" );
    qmlRegisterType<QmlMenuBar>( "org.videolan.vlc", 0, 1, "QmlMenuBar" );
    qmlRegisterType<NetworkMediaContextMenu>( "org.videolan.vlc", 0, 1, "NetworkMediaContextMenu" );
    qmlRegisterType<NetworkDeviceContextMenu>( "org.videolan.vlc", 0, 1, "NetworkDeviceContextMenu" );
    qmlRegisterType<PlaylistContextMenu>( "org.videolan.vlc", 0, 1, "PlaylistContextMenu" );
    qmlRegisterType<SortFilterProxyModel>( "org.videolan.vlc", 0, 1, "SortFilterProxyModel" );

    // Custom controls
    qmlRegisterType<RoundImage>( "org.videolan.controls", 0, 1, "RoundImage" );

    qRegisterMetaType<QList<QQmlError>>("QList<QQmlError>");
}

void MainUI::onQmlWarning(const QList<QQmlError>& qmlErrors)
{
    for (auto& error: qmlErrors)
    {
        msg_Warn( m_intf, "qml error %s:%i %s", qtu(error.url().toString()), error.line(), qtu(error.description()) );
    }
}
