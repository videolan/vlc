#include "mainui.hpp"

#include <cassert>

#include "medialibrary/medialib.hpp"
#include "medialibrary/mlqmltypes.hpp"
#include "medialibrary/mlalbummodel.hpp"
#include "medialibrary/mlartistmodel.hpp"
#include "medialibrary/mlalbumtrackmodel.hpp"
#include "medialibrary/mlgenremodel.hpp"
#include "medialibrary/mlvideomodel.hpp"
#include "medialibrary/mlrecentsvideomodel.hpp"
#include "medialibrary/mlfoldersmodel.hpp"

#include "player/player_controller.hpp"
#include "player/playercontrolbarmodel.hpp"

#include "playlist/playlist_model.hpp"
#include "playlist/playlist_controller.hpp"

#include "util/qml_main_context.hpp"
#include "util/qmleventfilter.hpp"
#include "util/i18n.hpp"
#include "util/systempalette.hpp"
#include "util/recent_media_model.hpp"
#include "util/settings.hpp"
#include "util/navigation_history.hpp"

#include "dialogs/help/aboutmodel.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "dialogs/dialogs/dialogmodel.hpp"

#include "network/networkmediamodel.hpp"
#include "network/networkdevicemodel.hpp"

#include "maininterface/main_interface.hpp"

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


MainUI::MainUI(intf_thread_t *p_intf, MainInterface *mainInterface,  QObject *parent)
    : QObject(parent)
    , m_intf(p_intf)
    , m_mainInterface(mainInterface)
{
    assert(m_intf);
    assert(m_mainInterface);

    m_settings = getSettings();

    m_hasMedialibrary = (vlc_ml_instance_get( p_intf ) != NULL);

    /* Get the available interfaces */
    m_extraInterfaces = new VLCVarChoiceModel(p_intf, "intf-add", this);

    /* */
    m_playlistDocked = m_settings->value( "MainWindow/pl-dock-status", true ).toBool();
    m_showRemainingTime = m_settings->value( "MainWindow/ShowRemainingTime", false ).toBool();

    registerQMLTypes();
}

MainUI::~MainUI()
{
    /* Save states */

    m_settings->beginGroup("MainWindow");
    m_settings->setValue( "pl-dock-status", m_playlistDocked );
    m_settings->setValue( "ShowRemainingTime", m_showRemainingTime );

    /* Save playlist state */
    m_settings->setValue( "playlist-visible", m_playlistVisible );

    /* Save the stackCentralW sizes */
    m_settings->endGroup();
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
    rootCtx->setContextProperty( "topWindow", m_mainInterface->windowHandle());
    rootCtx->setContextProperty( "dialogProvider", DialogsProvider::getInstance());
    rootCtx->setContextProperty( "recentsMedias",  new VLCRecentMediaModel( m_intf, this ));
    rootCtx->setContextProperty( "settings",  new Settings( m_intf, this ));
    rootCtx->setContextProperty( "systemPalette", new SystemPalette(this));

    if (m_hasMedialibrary)
        rootCtx->setContextProperty( "medialib", new MediaLib(m_intf, this) );
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

    qmlRegisterType<VideoSurface>("org.videolan.vlc", 0, 1, "VideoSurface");

    if (m_hasMedialibrary)
    {
        qRegisterMetaType<MLParentId>();
        qmlRegisterType<MLAlbumModel>( "org.videolan.medialib", 0, 1, "MLAlbumModel" );
        qmlRegisterType<MLArtistModel>( "org.videolan.medialib", 0, 1, "MLArtistModel" );
        qmlRegisterType<MLAlbumTrackModel>( "org.videolan.medialib", 0, 1, "MLAlbumTrackModel" );
        qmlRegisterType<MLGenreModel>( "org.videolan.medialib", 0, 1, "MLGenreModel" );
        qmlRegisterType<MLVideoModel>( "org.videolan.medialib", 0, 1, "MLVideoModel" );
        qmlRegisterType<MLRecentsVideoModel>( "org.videolan.medialib", 0, 1, "MLRecentsVideoModel" );
        qRegisterMetaType<NetworkTreeItem>();
        qmlRegisterType<NetworkMediaModel>( "org.videolan.medialib", 0, 1, "NetworkMediaModel");
        qmlRegisterType<NetworkDeviceModel>( "org.videolan.medialib", 0, 1, "NetworkDeviceModel");
        qmlRegisterType<MlFoldersModel>( "org.videolan.medialib", 0, 1, "MLFolderModel");

        //expose base object, they aren't instanciable from QML side
        registerAnonymousType<MLAlbum>("org.videolan.medialib", 1);
        registerAnonymousType<MLArtist>("org.videolan.medialib", 1);
        registerAnonymousType<MLAlbumTrack>("org.videolan.medialib", 1);
        registerAnonymousType<MLGenre>("org.videolan.medialib", 1);
        registerAnonymousType<MLVideo>("org.videolan.medialib", 1);
    }

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
    qRegisterMetaType<DialogId>();
    qmlRegisterType<DialogModel>("org.videolan.vlc", 0, 1, "DialogModel");

    qmlRegisterType<QmlEventFilter>( "org.videolan.vlc", 0, 1, "EventFilter" );

    qmlRegisterType<PlayerControlBarModel>( "org.videolan.vlc", 0, 1, "PlayerControlBarModel");
}

void MainUI::onQmlWarning(const QList<QQmlError>& qmlErrors)
{
    for (auto& error: qmlErrors)
    {
        msg_Warn( m_intf, "qml error %s:%i %s", qtu(error.url().toString()), error.line(), qtu(error.description()) );
    }
}

void MainUI::setPlaylistDocked( bool docked )
{
    m_playlistDocked = docked;

    emit playlistDockedChanged(docked);
}

void MainUI::setPlaylistVisible( bool visible )
{
    m_playlistVisible = visible;

    emit playlistVisibleChanged(visible);
}

void MainUI::setShowRemainingTime( bool show )
{
    m_showRemainingTime = show;
    emit showRemainingTimeChanged(show);
}
