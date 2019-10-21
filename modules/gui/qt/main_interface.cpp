/*****************************************************************************
 * main_interface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2011 VideoLAN and AUTHORS
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"

#include "main_interface.hpp"
#include "components/player_controller.hpp"                    // Creation
#include "managers/renderer_manager.hpp"

#include "util/customwidgets.hpp"               // qtEventToVLCKey, QVLCStackedWidget
#include "util/qt_dirs.hpp"                     // toNativeSeparators
#include "util/imagehelper.hpp"

#include "components/interface_widgets.hpp"     // bgWidget, videoWidget
#include "components/controller.hpp"            // controllers
#include "dialogs/firstrun.hpp"                 // First Run

#include "components/playlist/playlist_model.hpp"
#include <vlc_playlist.h>

#include "components/mediacenter/mcmedialib.hpp"
#include "components/mediacenter/mlqmltypes.hpp"
#include "components/mediacenter/mlalbummodel.hpp"
#include "components/mediacenter/mlartistmodel.hpp"
#include "components/mediacenter/mlalbumtrackmodel.hpp"
#include "components/mediacenter/mlgenremodel.hpp"
#include "components/mediacenter/mlvideomodel.hpp"
#include "components/mediacenter/mlrecentsvideomodel.hpp"
#include "components/mediacenter/mlnetworkmediamodel.hpp"
#include "components/mediacenter/mlnetworkdevicemodel.hpp"
#include "components/recent_media_model.hpp"
#include "components/settings.hpp"

#include "components/navigation_history.hpp"
#include "components/aboutmodel.hpp"
#include "components/dialogmodel.hpp"
#include "components/playercontrolbarmodel.hpp"

#include "components/voutwindow/qvoutwindowdummy.hpp"

#include "components/qml_main_context.hpp"

#include "util/qmleventfilter.hpp"

#include "menus.hpp"                            // Menu creation

#include <QCloseEvent>
#include <QKeyEvent>

#include <QUrl>
#include <QSize>
#include <QDate>
#include <QMimeData>

#include <QWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QStackedWidget>
#include <QScreen>
#include <QStackedLayout>
#ifdef _WIN32
#include <QFileInfo>
#endif

#if ! HAS_QT510 && defined(QT5_HAS_X11)
# include <QX11Info>
# include <X11/Xlib.h>
#endif

#include <QTimer>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>


#include <vlc_actions.h>                    /* Wheel event */
#include <vlc_vout_window.h>                /* VOUT_ events */

using  namespace vlc::playlist;

// #define DEBUG_INTF

/* Callback prototypes */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfBossCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfRaiseMainCB( vlc_object_t *p_this, const char *psz_variable,
                           vlc_value_t old_val, vlc_value_t new_val,
                           void *param );

const QEvent::Type MainInterface::ToolbarsNeedRebuild =
        (QEvent::Type)QEvent::registerEventType();

MainInterface::MainInterface( intf_thread_t *_p_intf ) : QVLCMW( _p_intf ),
    videoActive( ATOMIC_FLAG_INIT )
{
    /* Variables initialisation */
    lastWinScreen        = NULL;
    sysTray              = NULL;
    cryptedLabel         = NULL;

    b_hideAfterCreation  = false; // --qt-start-minimized
    playlistVisible      = false;
    b_interfaceFullScreen= false;
    b_hasPausedWhenMinimized = false;
    i_kc_offset          = false;
    b_maximizedView      = false;
    b_isWindowTiled      = false;

    /**
     *  Configuration and settings
     *  Pre-building of interface
     **/
    /* Main settings */
    setFocusPolicy( Qt::StrongFocus );
    setAcceptDrops( true );
    setWindowRole( "vlc-main" );
    setWindowIcon( QApplication::windowIcon() );
    setWindowOpacity( var_InheritFloat( p_intf, "qt-opacity" ) );

    /* Does the interface resize to video size or the opposite */
    b_autoresize = var_InheritBool( p_intf, "qt-video-autoresize" );

    /* Are we in the enhanced always-video mode or not ? */
    b_minimalView = var_InheritBool( p_intf, "qt-minimal-view" );

    /* Do we want anoying popups or not */
    i_notificationSetting = var_InheritInteger( p_intf, "qt-notification" );

    /* */
    b_pauseOnMinimize = var_InheritBool( p_intf, "qt-pause-minimized" );

    /* Get the available interfaces */
    m_extraInterfaces = new VLCVarChoiceModel(p_intf, "intf-add", this);

    /* Set the other interface settings */
    settings = getSettings();

    /* */
    b_playlistDocked = getSettings()->value( "MainWindow/pl-dock-status", true ).toBool();
    m_showRemainingTime = getSettings()->value( "MainWindow/ShowRemainingTime", false ).toBool();

    /* Should the UI stays on top of other windows */
    b_interfaceOnTop = var_InheritBool( p_intf, "video-on-top" );

    b_hasMedialibrary = (vlc_ml_instance_get( p_intf ) != NULL);

    QString platformName = QGuiApplication::platformName();

#ifdef QT5_HAS_WAYLAND
    b_hasWayland = platformName.startsWith(QLatin1String("wayland"), Qt::CaseInsensitive);
#endif

    // TODO: handle Wayland/X11/Win32 windows
    m_videoRenderer.reset(new QVoutWindowDummy(this));

    /**************************
     *  UI and Widgets design
     **************************/
    setVLCWindowsTitle();

    createMainWidget( settings );

    /*********************************
     * Create the Systray Management *
     *********************************/
    initSystray();

    /*************************************************************
     * Connect the input manager to the GUI elements it manages  *
     * Beware initSystray did some connects on input manager too *
     *************************************************************/
    /**
     * Connects on nameChanged()
     * Those connects are different because options can impeach them to trigger.
     **/
    /* Main Interface statusbar */
    /* and title of the Main Interface*/
    if( var_InheritBool( p_intf, "qt-name-in-title" ) )
    {
        connect( THEMIM, &PlayerController::nameChanged, this, &MainInterface::setVLCWindowsTitle );
    }
    connect( THEMIM, &PlayerController::inputChanged, this, &MainInterface::onInputChanged );

    /* END CONNECTS ON IM */

    /* VideoWidget connects for asynchronous calls */
    b_videoFullScreen = false;
    connect( this, &MainInterface::askGetVideo, this, &MainInterface::getVideoSlot, Qt::BlockingQueuedConnection );
    connect( this, &MainInterface::askReleaseVideo, this, &MainInterface::releaseVideoSlot, Qt::BlockingQueuedConnection );
    connect( this, &MainInterface::askVideoToResize, this, &MainInterface::setVideoSize, Qt::QueuedConnection );

    connect( THEDP, &DialogsProvider::toolBarConfUpdated, this, &MainInterface::toolBarConfUpdated );

    connect( this, &MainInterface::askToQuit, THEDP, &DialogsProvider::quit );
    connect( this, &MainInterface::askBoss, this, &MainInterface::setBoss );
    connect( this, &MainInterface::askRaise, this, &MainInterface::setRaise );

    connect( this, &MainInterface::askVideoSetFullScreen, this, &MainInterface::setVideoFullScreen);

    /** END of CONNECTS**/


    /************
     * Callbacks
     ************/
    libvlc_int_t* libvlc = vlc_object_instance(p_intf);
    var_AddCallback( libvlc, "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_AddCallback( libvlc, "intf-boss", IntfBossCB, p_intf );
    var_AddCallback( libvlc, "intf-show", IntfRaiseMainCB, p_intf );

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( libvlc, "intf-popupmenu", PopupMenuCB, p_intf );


    QVLCTools::restoreWidgetPosition( settings, this, QSize(600, 420) );

    b_interfaceFullScreen = isFullScreen();

    setVisible( !b_hideAfterCreation );

    computeMinimumSize();
}

MainInterface::~MainInterface()
{
    RendererManager::killInstance();

    /* Save states */

    settings->beginGroup("MainWindow");
    settings->setValue( "pl-dock-status", b_playlistDocked );
    settings->setValue( "ShowRemainingTime", m_showRemainingTime );

    /* Save playlist state */
    settings->setValue( "playlist-visible", playlistVisible );

    /* Save the stackCentralW sizes */
    settings->endGroup();

    /* Save this size */
    QVLCTools::saveWidgetPosition(settings, this);

    /* Unregister callbacks */
    libvlc_int_t* libvlc = vlc_object_instance(p_intf);
    var_DelCallback( libvlc, "intf-boss", IntfBossCB, p_intf );
    var_DelCallback( libvlc, "intf-show", IntfRaiseMainCB, p_intf );
    var_DelCallback( libvlc, "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_DelCallback( libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    p_intf->p_sys->p_mi = NULL;
}

void MainInterface::computeMinimumSize()
{
    int minWidth = 450;
    int minHeight = 300;
    setMinimumWidth( minWidth );
    setMinimumHeight( minHeight );
}

QList<QQmlError> MainInterface::qmlErrors() const
{
    return mediacenterView->errors();
}

/*****************************
 *   Main UI handling        *
 *****************************/

void MainInterface::reloadPrefs()
{
    i_notificationSetting = var_InheritInteger( p_intf, "qt-notification" );
    b_pauseOnMinimize = var_InheritBool( p_intf, "qt-pause-minimized" );
}


void MainInterface::onInputChanged( bool hasInput )
{
    if( hasInput == false )
        return;
    int autoRaise = var_InheritInteger( p_intf, "qt-auto-raise" );
    if ( autoRaise == MainInterface::RAISE_NEVER )
        return;
    if( THEMIM->hasVideoOutput() == true )
    {
        if( ( autoRaise & MainInterface::RAISE_VIDEO ) == 0 )
            return;
    }
    else if ( ( autoRaise & MainInterface::RAISE_AUDIO ) == 0 )
        return;
    emit askRaise();
}

#ifdef KeyPress
#undef KeyPress
#endif
void MainInterface::sendHotkey(Qt::Key key , Qt::KeyboardModifiers modifiers)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers );
    int vlckey = qtEventToVLCKey(&event);
    var_SetInteger(vlc_object_instance(p_intf), "key-pressed", vlckey);
}

void MainInterface::createMainWidget( QSettings * )
{
    qRegisterMetaType<VLCTick>();
    qmlRegisterUncreatableType<VLCTick>("org.videolan.vlc", 0, 1, "VLCTick", "");

    qmlRegisterType<VideoSurface>("org.videolan.vlc", 0, 1, "VideoSurface");

    if (b_hasMedialibrary)
    {
        qRegisterMetaType<MLParentId>();
        qmlRegisterType<MLAlbumModel>( "org.videolan.medialib", 0, 1, "MLAlbumModel" );
        qmlRegisterType<MLArtistModel>( "org.videolan.medialib", 0, 1, "MLArtistModel" );
        qmlRegisterType<MLAlbumTrackModel>( "org.videolan.medialib", 0, 1, "MLAlbumTrackModel" );
        qmlRegisterType<MLGenreModel>( "org.videolan.medialib", 0, 1, "MLGenreModel" );
        qmlRegisterType<MLVideoModel>( "org.videolan.medialib", 0, 1, "MLVideoModel" );
        qmlRegisterType<MLRecentsVideoModel>( "org.videolan.medialib", 0, 1, "MLRecentsVideoModel" );
        qRegisterMetaType<NetworkTreeItem>();
        qmlRegisterType<MLNetworkMediaModel>( "org.videolan.medialib", 0, 1, "MLNetworkMediaModel");
        qmlRegisterType<MLNetworkDeviceModel>( "org.videolan.medialib", 0, 1, "MLNetworkDeviceModel");

        //expose base object, they aren't instanciable from QML side
        qmlRegisterType<MLAlbum>();
        qmlRegisterType<MLArtist>();
        qmlRegisterType<MLAlbumTrack>();
        qmlRegisterType<MLGenre>();
        qmlRegisterType<MLVideo>();
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

    mediacenterView = new QQuickWidget(this);
    mediacenterView->setClearColor(Qt::transparent);

    NavigationHistory* navigation_history = new NavigationHistory(mediacenterView);

    QmlMainContext* mainCtx = new QmlMainContext(p_intf, this, mediacenterView);


    QQmlContext *rootCtx = mediacenterView->rootContext();

    rootCtx->setContextProperty( "history", navigation_history );
    rootCtx->setContextProperty( "player", p_intf->p_sys->p_mainPlayerController );
    rootCtx->setContextProperty( "mainctx", mainCtx);
    rootCtx->setContextProperty( "rootQMLView", mediacenterView);
    rootCtx->setContextProperty( "rootWindow", this);
    rootCtx->setContextProperty( "dialogProvider", DialogsProvider::getInstance());
    rootCtx->setContextProperty( "recentsMedias",  new VLCRecentMediaModel( p_intf, this ));
    rootCtx->setContextProperty( "settings",  new Settings( p_intf, this ));

    if (b_hasMedialibrary)
    {
        MCMediaLib *medialib = new MCMediaLib(p_intf, mediacenterView);
        rootCtx->setContextProperty( "medialib", medialib );
    }
    else
    {
        rootCtx->setContextProperty( "medialib", nullptr );
    }

    mediacenterView->setSource( QUrl ( QStringLiteral("qrc:/qml/MainInterface.qml") ) );
    mediacenterView->setResizeMode( QQuickWidget::SizeRootObjectToView );

    setCentralWidget( mediacenterView );

    if ( b_interfaceOnTop )
        setWindowFlags( windowFlags() | Qt::WindowStaysOnTopHint );
}

inline void MainInterface::initSystray()
{
    bool b_systrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    bool b_systrayWanted = var_InheritBool( p_intf, "qt-system-tray" );

    if( var_InheritBool( p_intf, "qt-start-minimized") )
    {
        if( b_systrayAvailable )
        {
            b_systrayWanted = true;
            b_hideAfterCreation = true;
        }
        else
            msg_Err( p_intf, "cannot start minimized without system tray bar" );
    }

    if( b_systrayAvailable && b_systrayWanted )
        createSystray();
}


/****************************************************************************
 * Video Handling
 ****************************************************************************/

/**
 * NOTE:
 * You must not change the state of this object or other Qt UI objects,
 * from the video output thread - only from the Qt UI main loop thread.
 * All window provider queries must be handled through signals or events.
 * That's why we have all those emit statements...
 */
bool MainInterface::getVideo( struct vout_window_t *p_wnd )
{
    static const struct vout_window_operations ops = {
        MainInterface::enableVideo,
        MainInterface::disableVideo,
        MainInterface::resizeVideo,
        MainInterface::releaseVideo,
        MainInterface::requestVideoState,
        MainInterface::requestVideoWindowed,
        MainInterface::requestVideoFullScreen,
        NULL,
    };

    if( videoActive.test_and_set() )
        return false;

    p_wnd->ops = &ops;
    p_wnd->info.has_double_click = true;
    p_wnd->sys = this;
    if (!m_videoRenderer->setupVoutWindow(p_wnd))
        return false;

    m_hasEmbededVideo = true;
    emit hasEmbededVideoChanged(true);

    return true;
}

void MainInterface::setVideoFullScreen( bool fs )
{
    b_videoFullScreen = fs;
    if( fs )
    {
        int numscreen = var_InheritInteger( p_intf, "qt-fullscreen-screennumber" );

        if ( numscreen >= 0 && numscreen < QApplication::desktop()->screenCount() )
        {
            QRect screenres = QApplication::desktop()->screenGeometry( numscreen );
            lastWinScreen = windowHandle()->screen();
#ifdef QT5_HAS_WAYLAND
            if( !b_hasWayland )
                windowHandle()->setScreen(QGuiApplication::screens()[numscreen]);
#else
            windowHandle()->setScreen(QGuiApplication::screens()[numscreen]);
#endif

            /* To be sure window is on proper-screen in xinerama */
            if( !screenres.contains( pos() ) )
            {
                lastWinPosition = pos();
                lastWinSize = size();
                msg_Dbg( p_intf, "Moving video to correct position");
                move( QPoint( screenres.x(), screenres.y() ) );
            }
        }

        setFullScreen( true );
    }
    else
    {
        setFullScreen( b_interfaceFullScreen );
#ifdef QT5_HAS_WAYLAND
        if( lastWinScreen != NULL && !b_hasWayland )
            windowHandle()->setScreen(lastWinScreen);
#else
        if( lastWinScreen != NULL )
            windowHandle()->setScreen(lastWinScreen);
#endif
        if( lastWinPosition.isNull() == false )
        {
            move( lastWinPosition );
            lastWinPosition = QPoint();
            if( !pendingResize.isValid() )
            {
                resizeWindow( lastWinSize.width(), lastWinSize.height() );
                lastWinSize = QSize();
            }
        }

    }
}


/* Slot to change the video always-on-top flag.
 * Emit askVideoOnTop() to invoke this from other thread. */
void MainInterface::setVideoOnTop( bool on_top )
{
    //don't apply changes if user has already sets its interface on top
    if ( b_interfaceOnTop )
        return;

    Qt::WindowFlags oldflags = windowFlags(), newflags;

    if( on_top )
        newflags = oldflags | Qt::WindowStaysOnTopHint;
    else
        newflags = oldflags & ~Qt::WindowStaysOnTopHint;
    if( newflags != oldflags && !b_videoFullScreen )
    {
        setWindowFlags( newflags );
        show(); /* necessary to apply window flags */
    }
}

void MainInterface::setPlaylistDocked( bool docked )
{
    b_playlistDocked = docked;

    emit playlistDockedChanged(docked);
}

void MainInterface::setPlaylistVisible( bool visible )
{
    playlistVisible = visible;

    emit playlistVisibleChanged(visible);
}

void MainInterface::setShowRemainingTime( bool show )
{
    m_showRemainingTime = show;
    emit showRemainingTimeChanged(show);
}

void MainInterface::setInterfaceAlwaysOnTop( bool on_top )
{
    b_interfaceOnTop = on_top;
    Qt::WindowFlags oldflags = windowFlags(), newflags;

    if( on_top )
        newflags = oldflags | Qt::WindowStaysOnTopHint;
    else
        newflags = oldflags & ~Qt::WindowStaysOnTopHint;
    if( newflags != oldflags && !b_videoFullScreen )
    {
        setWindowFlags( newflags );
        show(); /* necessary to apply window flags */
    }
    emit interfaceAlwaysOnTopChanged(on_top);
}

/* Asynchronous calls for video window contrlos */
int MainInterface::enableVideo( vout_window_t *p_wnd,
                                 const vout_window_cfg_t *cfg )
{
    MainInterface *p_mi = (MainInterface *)p_wnd->sys;

    msg_Dbg( p_wnd, "requesting video window..." );
    p_mi->m_videoRenderer->enableVideo(cfg->width, cfg->height, cfg->is_fullscreen);
    /* This is a blocking call signal. Results are stored directly in the
     * vout_window_t and boolean pointers. Beware of deadlocks! */
    emit p_mi->askGetVideo( cfg->is_fullscreen );
    return VLC_SUCCESS;
}

void MainInterface::disableVideo( vout_window_t *p_wnd )
{
    MainInterface *p_mi = (MainInterface *)p_wnd->sys;
    p_mi->m_videoRenderer->disableVideo();
    msg_Dbg( p_wnd, "releasing video..." );
    emit p_mi->askReleaseVideo();
}

void MainInterface::resizeVideo( vout_window_t *p_wnd,
                                 unsigned i_width, unsigned i_height )
{
    MainInterface *p_mi = (MainInterface *)p_wnd->sys;

    emit p_mi->askVideoToResize( i_width, i_height );
}

void MainInterface::requestVideoWindowed( struct vout_window_t *wnd )
{
   MainInterface *p_mi = (MainInterface *)wnd->sys;
   msg_Warn( wnd, "requestVideoWindowed..." );

   emit p_mi->askVideoSetFullScreen( false );
}

void MainInterface::requestVideoFullScreen( vout_window_t *wnd, const char * )
{
    MainInterface *p_mi = (MainInterface *)wnd->sys;
    msg_Warn( wnd, "requestVideoFullScreen..." );

    emit p_mi->askVideoSetFullScreen( true );
}

void MainInterface::requestVideoState( vout_window_t *p_wnd, unsigned i_arg )
{
    MainInterface *p_mi = (MainInterface *)p_wnd->sys;
    bool on_top = (i_arg & VOUT_WINDOW_STATE_ABOVE) != 0;

    emit p_mi->askVideoOnTop( on_top );
}

void MainInterface::releaseVideo( vout_window_t *p_wnd )
{
    MainInterface *p_mi = (MainInterface *)p_wnd->sys;

    /* Releasing video (in disableVideo()) was a blocking call.
     * The video is no longer active by this point.
     */
    p_mi->videoActive.clear();
    p_mi->m_videoRenderer->setupVoutWindow(nullptr);
    p_mi->m_hasEmbededVideo = false;
    emit p_mi->hasEmbededVideoChanged(false);
}

QQuickWindow*MainInterface::getRootQuickWindow()
{
    //FIXME, thread safety
    QQuickItem* rootObject = mediacenterView->rootObject();
    if (!rootObject)
        return nullptr;
    return rootObject->window();
}

VideoSurfaceProvider* MainInterface::getVideoSurfaceProvider() const
{
    return m_videoRenderer->getVideoSurfaceProvider();
}

const Qt::Key MainInterface::kc[10] =
{
    Qt::Key_Up, Qt::Key_Up,
    Qt::Key_Down, Qt::Key_Down,
    Qt::Key_Left, Qt::Key_Right, Qt::Key_Left, Qt::Key_Right,
    Qt::Key_B, Qt::Key_A
};

/**
 * Give the decorations of the Main Window a correct Name.
 * If nothing is given, set it to VLC...
 **/
void MainInterface::setVLCWindowsTitle( const QString& aTitle )
{
    setWindowTitle( aTitle );
}

void MainInterface::showBuffering( float f_cache )
{
    QString amount = QString("Buffering: %1%").arg( (int)(100*f_cache) );
    statusBar()->showMessage( amount, 1000 );
}

void MainInterface::getVideoSlot(bool fullscreen)
{
    setVideoFullScreen(fullscreen);
}


void MainInterface::releaseVideoSlot( void )
{
    setVideoOnTop( false );
    setVideoFullScreen( false );
}

void MainInterface::setVideoSize(unsigned int w, unsigned int h)
{
    if (!isFullScreen() && !isMaximized() )
    {
        /* Resize video widget to video size, or keep it at the same
         * size. Call setSize() either way so that vout_window_ReportSize
         * will always get called.
         * If the video size is too large for the screen, resize it
         * to the screen size.
         */
        if (b_autoresize)
        {
            QRect screen = QApplication::desktop()->availableGeometry();
            float factor = devicePixelRatioF();
            if( (float)h / factor > screen.height() )
            {
                w = screen.width();
                h = screen.height();
            }
            else
            {
                // Convert the size in logical pixels
                w = qRound( (float)w / factor );
                h = qRound( (float)h / factor );
                msg_Dbg( p_intf, "Logical video size: %ux%u", w, h );
            }
            resize(w, h);
        }
    }
}

/*****************************************************************************
 * Systray Icon and Systray Menu
 *****************************************************************************/
/**
 * Create a SystemTray icon and a menu that would go with it.
 * Connects to a click handler on the icon.
 **/
void MainInterface::createSystray()
{
    QIcon iconVLC;
    if( QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY && var_InheritBool( p_intf, "qt-icon-change" ) )
        iconVLC = QIcon::fromTheme( "vlc-xmas", QIcon( ":/logo/vlc128-xmas.png" ) );
    else
        iconVLC = QIcon::fromTheme( "vlc", QIcon( ":/logo/vlc256.png" ) );
    sysTray = new QSystemTrayIcon( iconVLC, this );
    sysTray->setToolTip( qtr( "VLC media player" ));

    systrayMenu = new QMenu( qtr( "VLC media player" ), this );
    systrayMenu->setIcon( iconVLC );

    VLCMenuBar::updateSystrayMenu( this, p_intf, true );
    sysTray->show();

    connect( sysTray, &QSystemTrayIcon::activated,
             this, &MainInterface::handleSystrayClick );

    /* Connects on nameChanged() */
    connect( THEMIM, &PlayerController::nameChanged,
             this, &MainInterface::updateSystrayTooltipName );
    /* Connect PLAY_STATUS on the systray */
    connect( THEMIM, &PlayerController::playingStateChanged,
             this, &MainInterface::updateSystrayTooltipStatus );
}

void MainInterface::toggleUpdateSystrayMenuWhenVisible()
{
    hide();
}

void MainInterface::resizeWindow(int w, int h)
{
#if ! HAS_QT510 && defined(QT5_HAS_X11)
    if( QX11Info::isPlatformX11() )
    {
#if HAS_QT56
        qreal dpr = devicePixelRatioF();
#else
        qreal dpr = devicePixelRatio();
#endif
        QSize size(w, h);
        size = size.boundedTo(maximumSize()).expandedTo(minimumSize());
        /* X11 window managers are not required to accept geometry changes on
         * the top-level window.  Unfortunately, Qt < 5.10 assumes that the
         * change will succeed, and resizes all sub-windows unconditionally.
         * By calling XMoveResizeWindow directly, Qt will not see our change
         * request until the ConfigureNotify event on success
         * and not at all if it is rejected. */
        XResizeWindow( QX11Info::display(), winId(),
                       (unsigned int)size.width() * dpr, (unsigned int)size.height() * dpr);
        return;
    }
#endif
    resize(w, h);
}

/**
 * Updates the Systray Icon's menu and toggle the main interface
 */
void MainInterface::toggleUpdateSystrayMenu()
{
    /* If hidden, show it */
    if( isHidden() )
    {
        show();
        activateWindow();
    }
    else if( isMinimized() )
    {
        /* Minimized */
        showNormal();
        activateWindow();
    }
    else
    {
        /* Visible (possibly under other windows) */
        toggleUpdateSystrayMenuWhenVisible();
    }
    if( sysTray )
        VLCMenuBar::updateSystrayMenu( this, p_intf );
}

/* First Item of the systray menu */
void MainInterface::showUpdateSystrayMenu()
{
    if( isHidden() )
        show();
    if( isMinimized() )
        showNormal();
    activateWindow();

    VLCMenuBar::updateSystrayMenu( this, p_intf );
}

/* First Item of the systray menu */
void MainInterface::hideUpdateSystrayMenu()
{
    hide();
    VLCMenuBar::updateSystrayMenu( this, p_intf );
}

/* Click on systray Icon */
void MainInterface::handleSystrayClick(
                                    QSystemTrayIcon::ActivationReason reason )
{
    switch( reason )
    {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
#ifdef Q_OS_MAC
            VLCMenuBar::updateSystrayMenu( this, p_intf );
#else
            toggleUpdateSystrayMenu();
#endif
            break;
        case QSystemTrayIcon::MiddleClick:
            sysTray->showMessage( qtr( "VLC media player" ),
                    qtr( "Control menu for the player" ),
                    QSystemTrayIcon::Information, 3000 );
            break;
        default:
            break;
    }
}

/**
 * Updates the name of the systray Icon tooltip.
 * Doesn't check if the systray exists, check before you call it.
 **/
void MainInterface::updateSystrayTooltipName( const QString& name )
{
    if( name.isEmpty() )
    {
        sysTray->setToolTip( qtr( "VLC media player" ) );
    }
    else
    {
        sysTray->setToolTip( name );
        if( ( i_notificationSetting == NOTIFICATION_ALWAYS ) ||
            ( i_notificationSetting == NOTIFICATION_MINIMIZED && (isMinimized() || isHidden()) ) )
        {
            sysTray->showMessage( qtr( "VLC media player" ), name,
                    QSystemTrayIcon::NoIcon, 3000 );
        }
    }

    VLCMenuBar::updateSystrayMenu( this, p_intf );
}

/**
 * Updates the status of the systray Icon tooltip.
 * Doesn't check if the systray exists, check before you call it.
 **/
void MainInterface::updateSystrayTooltipStatus( PlayerController::PlayingState )
{
    VLCMenuBar::updateSystrayMenu( this, p_intf );
}

void MainInterface::changeEvent(QEvent *event)
{
    if( event->type() == QEvent::WindowStateChange )
    {
        QWindowStateChangeEvent *windowStateChangeEvent = static_cast<QWindowStateChangeEvent*>(event);
        Qt::WindowStates newState = windowState();
        Qt::WindowStates oldState = windowStateChangeEvent->oldState();

        /* b_maximizedView stores if the window was maximized before entering fullscreen.
         * It is set when entering maximized mode, unset when leaving it to normal mode.
         * Upon leaving full screen, if b_maximizedView is set,
         * the window should be maximized again. */
        if( newState & Qt::WindowMaximized &&
            !( oldState & Qt::WindowMaximized ) )
            b_maximizedView = true;

        if( !( newState & Qt::WindowMaximized ) &&
            oldState & Qt::WindowMaximized &&
            !b_videoFullScreen )
            b_maximizedView = false;

        if( !( newState & Qt::WindowFullScreen ) &&
            oldState & Qt::WindowFullScreen &&
            b_maximizedView )
        {
            showMaximized();
            return;
        }

        if( newState & Qt::WindowMinimized )
        {
            b_hasPausedWhenMinimized = false;

            if( THEMIM->getPlayingState() == PlayerController::PLAYING_STATE_PLAYING &&
                THEMIM->hasVideoOutput() && !THEMIM->hasAudioVisualization() &&
                b_pauseOnMinimize )
            {
                b_hasPausedWhenMinimized = true;
                THEMPL->pause();
            }
        }
        else if( oldState & Qt::WindowMinimized && !( newState & Qt::WindowMinimized ) )
        {
            if( b_hasPausedWhenMinimized )
            {
                THEMPL->play();
            }
        }
    }

    QWidget::changeEvent(event);
}

/************************************************************************
 * D&D Events
 ************************************************************************/
void MainInterface::dropEvent(QDropEvent *event)
{
    dropEventPlay( event, true );
}

/**
 * dropEventPlay
 *
 * Event called if something is dropped onto a VLC window
 * \param event the event in question
 * \param b_play whether to play the file immediately
 * \return nothing
 */
void MainInterface::dropEventPlay( QDropEvent *event, bool b_play )
{
    if( event->possibleActions() & ( Qt::CopyAction | Qt::MoveAction | Qt::LinkAction ) )
       event->setDropAction( Qt::CopyAction );
    else
        return;

    const QMimeData *mimeData = event->mimeData();

    /* D&D of a subtitles file, add it on the fly */
    if( mimeData->urls().count() == 1 && THEMIM->hasInput() )
    {
        if( !THEMIM->AddAssociatedMedia(SPU_ES, mimeData->urls()[0].toString(), true, true, true) )
        {
            event->accept();
            return;
        }
    }

    bool first = b_play;
    foreach( const QUrl &url, mimeData->urls() )
    {
        if( url.isValid() )
        {
            QString mrl = toURI( url.toEncoded().constData() );
#ifdef _WIN32
            QFileInfo info( url.toLocalFile() );
            if( info.exists() && info.isSymLink() )
            {
                QString target = info.symLinkTarget();
                QUrl url;
                if( QFile::exists( target ) )
                {
                    url = QUrl::fromLocalFile( target );
                }
                else
                {
                    url.setUrl( target );
                }
                mrl = toURI( url.toEncoded().constData() );
            }
#endif
            if( mrl.length() > 0 )
            {
                Open::openMRL( p_intf, mrl, first );
                first = false;
            }
        }
    }

    /* Browsers give content as text if you dnd the addressbar,
       so check if mimedata has valid url in text and use it
       if we didn't get any normal Urls()*/
    if( !mimeData->hasUrls() && mimeData->hasText() &&
        QUrl(mimeData->text()).isValid() )
    {
        QString mrl = toURI( mimeData->text() );
        Open::openMRL( p_intf, mrl, first );
    }
    event->accept();
}
void MainInterface::dragEnterEvent(QDragEnterEvent *event)
{
     event->acceptProposedAction();
}
void MainInterface::dragMoveEvent(QDragMoveEvent *event)
{
     event->acceptProposedAction();
}
void MainInterface::dragLeaveEvent(QDragLeaveEvent *event)
{
     event->accept();
}

/************************************************************************
 * Events stuff
 ************************************************************************/

void MainInterface::closeEvent( QCloseEvent *e )
{
    PlaylistControllerModel* playlistController = p_intf->p_sys->p_mainPlaylistController;
    PlayerController* playerController = p_intf->p_sys->p_mainPlayerController;

    if (m_videoRenderer)
        m_videoRenderer->windowClosed();
    //We need to make sure that noting is playing anymore otherwise the vout will be closed
    //after the main interface, and it requires (at least with OpenGL) that the OpenGL context
    //from the main window is still valid.
    //vout_window_ReportClose is currently stubbed
    if (playerController->hasVideoOutput()) {

        connect(playerController, &PlayerController::playingStateChanged, [this](PlayerController::PlayingState state){
            if (state == PlayerController::PLAYING_STATE_STOPPED) {
                QMetaObject::invokeMethod(this, &MainInterface::close, Qt::QueuedConnection, nullptr);
            }
        });
        playlistController->stop();

        e->ignore();
    }
    else
    {
        emit askToQuit(); /* ask THEDP to quit, so we have a unique method */
        /* Accept session quit. Otherwise we break the desktop mamager. */
        e->accept();
    }
}

void MainInterface::setFullScreen( bool fs )
{
    if( fs )
        setWindowState( windowState() | Qt::WindowFullScreen );
    else
        setWindowState( windowState() & ~Qt::WindowFullScreen );
}

void MainInterface::setInterfaceFullScreen( bool fs )
{
    b_interfaceFullScreen = fs;
    setFullScreen(fs);
    emit interfaceFullScreenChanged( fs );
}

void MainInterface::toggleInterfaceFullScreen()
{
    setInterfaceFullScreen( !b_interfaceFullScreen );
    emit fullscreenInterfaceToggled( b_interfaceFullScreen );
}

void MainInterface::emitBoss()
{
    emit askBoss();
}
void MainInterface::setBoss()
{
    THEMPL->pause();
    if( sysTray )
    {
        hide();
    }
    else
    {
        showMinimized();
    }
}

void MainInterface::emitShow()
{
    emit askShow();
}

void MainInterface::popupMenu(bool show)
{
    emit askPopupMenu( show );
}

void MainInterface::emitRaise()
{
    emit askRaise();
}
void MainInterface::setRaise()
{
    activateWindow();
    raise();
}

VLCVarChoiceModel* MainInterface::getExtraInterfaces()
{
    return m_extraInterfaces;
}

/*****************************************************************************
 * PopupMenuCB: callback triggered by the intf-popupmenu playlist variable.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PopupMenuCB( vlc_object_t *, const char *,
                        vlc_value_t, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;

    if( p_intf->pf_show_dialog )
    {
        p_intf->pf_show_dialog( p_intf, INTF_DIALOG_POPUPMENU,
                                new_val.b_bool, NULL );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfShowCB: callback triggered by the intf-toggle-fscontrol libvlc variable.
 *****************************************************************************/
static int IntfShowCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->emitShow();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfRaiseMainCB: callback triggered by the intf-show-main libvlc variable.
 *****************************************************************************/
static int IntfRaiseMainCB( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->emitRaise();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfBossCB: callback triggered by the intf-boss libvlc variable.
 *****************************************************************************/
static int IntfBossCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->emitBoss();

    return VLC_SUCCESS;
}
