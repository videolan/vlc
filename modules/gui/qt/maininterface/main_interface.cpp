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

#include "maininterface/main_interface.hpp"
#include "player/player_controller.hpp"                    // Creation
#include "util/renderer_manager.hpp"

#include "widgets/native/customwidgets.hpp"               // qtEventToVLCKey, QVLCStackedWidget
#include "util/qt_dirs.hpp"                     // toNativeSeparators
#include "util/imagehelper.hpp"
#include "util/color_scheme_model.hpp"

#include "widgets/native/interface_widgets.hpp"     // bgWidget, videoWidget
#include "dialogs/firstrun/firstrun.hpp"                 // First Run

#include "playlist/playlist_model.hpp"
#include "playlist/playlist_controller.hpp"
#include <vlc_playlist.h>

#include "videosurface.hpp"

#include "menus/menus.hpp"                            // Menu creation

#include "vlc_media_library.h"

#include "dialogs/toolbar/controlbar_profile_model.hpp"

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


#include <QtGlobal>
#include <QTimer>

#include <vlc_actions.h>                    /* Wheel event */
#include <vlc_vout_window.h>                /* VOUT_ events */

#define VLC_REFERENCE_SCALE_FACTOR 96.

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

MainInterface::MainInterface(qt_intf_t *_p_intf , QWidget* parent, Qt::WindowFlags flags)
    : QVLCMW( _p_intf, parent, flags )
{
    /* Variables initialisation */
    lastWinScreen        = NULL;
    sysTray              = NULL;
    cryptedLabel         = NULL;

    b_hideAfterCreation  = false; // --qt-start-minimized
    playlistVisible      = false;
    playlistWidthFactor  = 4.0;
    b_interfaceFullScreen= false;
    i_kc_offset          = false;

    /**
     *  Configuration and settings
     *  Pre-building of interface
     **/

    /* Are we in the enhanced always-video mode or not ? */
    b_minimalView = var_InheritBool( p_intf, "qt-minimal-view" );

    /* Do we want anoying popups or not */
    i_notificationSetting = var_InheritInteger( p_intf, "qt-notification" );

    /* */
    m_intfUserScaleFactor = var_InheritFloat(p_intf, "qt-interface-scale");
    if (m_intfUserScaleFactor == -1)
        m_intfUserScaleFactor = getSettings()->value( "MainWindow/interface-scale", 1.0).toFloat();
    winId(); //force window creation
    QWindow* window = windowHandle();
    if (window)
        connect(window, &QWindow::screenChanged, this, &MainInterface::updateIntfScaleFactor);
    updateIntfScaleFactor();

    /* Get the available interfaces */
    m_extraInterfaces = new VLCVarChoiceModel(p_intf, "intf-add", this);

    vlc_medialibrary_t* ml = vlc_ml_instance_get( p_intf );
    b_hasMedialibrary = (ml != NULL);
    if (b_hasMedialibrary) {
        m_medialib = new MediaLib(p_intf, this);
    }

    /* Set the other interface settings */
    settings = getSettings();

    /* playlist settings */
    b_playlistDocked = getSettings()->value( "MainWindow/pl-dock-status", true ).toBool();
    playlistVisible  = getSettings()->value( "MainWindow/playlist-visible", false ).toBool();
    playlistWidthFactor = getSettings()->value( "MainWindow/playlist-width-factor", 4.0 ).toDouble();
    m_gridView = getSettings()->value( "MainWindow/grid-view", true).toBool();
    QString currentColorScheme = getSettings()->value( "MainWindow/color-scheme", "system").toString();
    m_showRemainingTime = getSettings()->value( "MainWindow/ShowRemainingTime", false ).toBool();
    m_pinVideoControls = getSettings()->value("MainWindow/pin-video-controls", false ).toBool();

    m_colorScheme = new ColorSchemeModel(this);
    m_colorScheme->setCurrent(currentColorScheme);

    /* Controlbar Profile Model Creation */
    m_controlbarProfileModel = new ControlbarProfileModel(p_intf, this);

    /* Should the UI stays on top of other windows */
    b_interfaceOnTop = var_InheritBool( p_intf, "video-on-top" );

#if QT_CLIENT_SIDE_DECORATION_AVAILABLE
    m_clientSideDecoration = ! var_InheritBool( p_intf, "qt-titlebar" );
#endif
    m_hasToolbarMenu = var_InheritBool( p_intf, "qt-menubar" );

    QString platformName = QGuiApplication::platformName();

#ifdef QT5_HAS_WAYLAND
    b_hasWayland = platformName.startsWith(QLatin1String("wayland"), Qt::CaseInsensitive);
#endif

    /**************************
     *  UI and Widgets design
     **************************/

    /* Main settings */
    setFocusPolicy( Qt::StrongFocus );
    setAcceptDrops( true );

    /*********************************
     * Create the Systray Management *
     *********************************/
    //postpone systray initialisation to speedup starting time
    QMetaObject::invokeMethod(this, &MainInterface::initSystray, Qt::QueuedConnection);

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
    connect( THEMIM, &PlayerController::inputChanged, this, &MainInterface::onInputChanged );

    /* END CONNECTS ON IM */

    /* VideoWidget connects for asynchronous calls */
    connect( this, &MainInterface::askToQuit, THEDP, &DialogsProvider::quit, Qt::QueuedConnection  );

    connect(this, &MainInterface::interfaceFullScreenChanged, this, &MainInterface::useClientSideDecorationChanged);

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

    //add a dummy transparent widget
    QWidget* widget = new QWidget(this);
    widget->setStyleSheet("background-color: transparent");
    setCentralWidget(widget);

    computeMinimumSize();
}

MainInterface::~MainInterface()
{
    RendererManager::killInstance();

    /* Save states */

    settings->beginGroup("MainWindow");
    settings->setValue( "pl-dock-status", b_playlistDocked );
    settings->setValue( "ShowRemainingTime", m_showRemainingTime );
    settings->setValue( "interface-scale", m_intfUserScaleFactor );
    settings->setValue( "pin-video-controls", m_pinVideoControls );

    /* Save playlist state */
    settings->setValue( "playlist-visible", playlistVisible );
    settings->setValue( "playlist-width-factor", playlistWidthFactor);

    settings->setValue( "grid-view", m_gridView );
    settings->setValue( "color-scheme", m_colorScheme->getCurrent() );
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

    p_intf->p_mi = NULL;
}

bool MainInterface::hasVLM() const {
#ifdef ENABLE_VLM
    return true;
#else
    return false;
#endif
}

bool MainInterface::useClientSideDecoration() const
{
    //don't show CSD when interface is fullscreen
    return m_clientSideDecoration && !b_interfaceFullScreen;
}

void MainInterface::computeMinimumSize()
{
    int minWidth = 450;
    int minHeight = 300;
    setMinimumWidth( minWidth );
    setMinimumHeight( minHeight );
}


/*****************************
 *   Main UI handling        *
 *****************************/

void MainInterface::reloadPrefs()
{
    i_notificationSetting = var_InheritInteger( p_intf, "qt-notification" );
    
    if ( m_hasToolbarMenu != var_InheritBool( p_intf, "qt-menubar" ) )
    {
        m_hasToolbarMenu = !m_hasToolbarMenu;
        emit hasToolbarMenuChanged();
    }

#if QT_CLIENT_SIDE_DECORATION_AVAILABLE
    if (m_clientSideDecoration != (! var_InheritBool( p_intf, "qt-titlebar" )))
    {
        m_clientSideDecoration = !m_clientSideDecoration;
        emit useClientSideDecorationChanged();
    }
#endif
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

void MainInterface::updateIntfScaleFactor()
{
    QWindow* window = windowHandle();
    m_intfScaleFactor = m_intfUserScaleFactor;
    if (window)
    {
        QScreen* screen = window->screen();
        if (screen)
        {
            qreal dpi = screen->logicalDotsPerInch();
            m_intfScaleFactor = m_intfUserScaleFactor * dpi / VLC_REFERENCE_SCALE_FACTOR;
        }
    }
    emit intfScaleFactorChanged();
}

void MainInterface::incrementIntfUserScaleFactor(bool increment)
{
    if (increment)
        setIntfUserScaleFactor(m_intfScaleFactor + .1f);
    else
        setIntfUserScaleFactor(m_intfScaleFactor - .1f);
}

void MainInterface::setIntfUserScaleFactor(float newValue)
{
    m_intfUserScaleFactor = std::max(std::min(newValue, getMaxIntfUserScaleFactor()), getMinIntfUserScaleFactor());
    updateIntfScaleFactor();
}

void MainInterface::setPinVideoControls(bool pinVideoControls)
{
    if (m_pinVideoControls == pinVideoControls)
        return;

    m_pinVideoControls = pinVideoControls;
    emit pinVideoControlsChanged(m_pinVideoControls);
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

void MainInterface::setPlaylistWidthFactor( double factor )
{
    if (factor > 0.0)
    {
        playlistWidthFactor = factor;
        emit playlistWidthFactorChanged(factor);
    }
}

void MainInterface::setShowRemainingTime( bool show )
{
    m_showRemainingTime = show;
    emit showRemainingTimeChanged(show);
}

void MainInterface::setGridView(bool asGrid)
{
    m_gridView = asGrid;
    emit gridViewChanged( asGrid );
}

void MainInterface::setInterfaceAlwaysOnTop( bool on_top )
{
    b_interfaceOnTop = on_top;
    emit interfaceAlwaysOnTopChanged(on_top);
}

bool MainInterface::hasEmbededVideo() const
{
    return m_videoSurfaceProvider && m_videoSurfaceProvider->hasVideoEmbed();
}

void MainInterface::setVideoSurfaceProvider(VideoSurfaceProvider* videoSurfaceProvider)
{
    if (m_videoSurfaceProvider)
        disconnect(m_videoSurfaceProvider, &VideoSurfaceProvider::hasVideoEmbedChanged, this, &MainInterface::hasEmbededVideoChanged);
    m_videoSurfaceProvider = videoSurfaceProvider;
    if (m_videoSurfaceProvider)
        connect(m_videoSurfaceProvider, &VideoSurfaceProvider::hasVideoEmbedChanged,
                this, &MainInterface::hasEmbededVideoChanged,
                Qt::QueuedConnection);
    emit hasEmbededVideoChanged(m_videoSurfaceProvider && m_videoSurfaceProvider->hasVideoEmbed());
}

VideoSurfaceProvider* MainInterface::getVideoSurfaceProvider() const
{
    return m_videoSurfaceProvider;
}

const Qt::Key MainInterface::kc[10] =
{
    Qt::Key_Up, Qt::Key_Up,
    Qt::Key_Down, Qt::Key_Down,
    Qt::Key_Left, Qt::Key_Right, Qt::Key_Left, Qt::Key_Right,
    Qt::Key_B, Qt::Key_A
};


void MainInterface::showBuffering( float f_cache )
{
    QString amount = QString("Buffering: %1%").arg( (int)(100*f_cache) );
    statusBar()->showMessage( amount, 1000 );
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

/**
 * Updates the Systray Icon's menu and toggle the main interface
 */
void MainInterface::toggleUpdateSystrayMenu()
{
    emit toggleWindowVisibility();
    if( sysTray )
        VLCMenuBar::updateSystrayMenu( this, p_intf );
}

/* First Item of the systray menu */
void MainInterface::showUpdateSystrayMenu()
{
    emit setInterfaceVisibible(true);
    VLCMenuBar::updateSystrayMenu( this, p_intf );
}

/* First Item of the systray menu */
void MainInterface::hideUpdateSystrayMenu()
{
    emit setInterfaceVisibible(false);
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

    QVector<vlc::playlist::Media> medias;
    for( const QUrl &url: mimeData->urls() )
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
                medias.push_back( vlc::playlist::Media{ mrl, nullptr, nullptr });
        }
    }

    /* Browsers give content as text if you dnd the addressbar,
       so check if mimedata has valid url in text and use it
       if we didn't get any normal Urls()*/
    if( !mimeData->hasUrls() && mimeData->hasText() &&
        QUrl(mimeData->text()).isValid() )
    {
        QString mrl = toURI( mimeData->text() );
        medias.push_back( vlc::playlist::Media{ mrl, nullptr, nullptr });
    }
    if (!medias.empty())
        THEMPL->append(medias, b_play);
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
    PlaylistControllerModel* playlistController = p_intf->p_mainPlaylistController;
    PlayerController* playerController = p_intf->p_mainPlayerController;

    if (m_videoSurfaceProvider)
        m_videoSurfaceProvider->onWindowClosed();
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

void MainInterface::setInterfaceFullScreen( bool fs )
{
    b_interfaceFullScreen = fs;
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

void MainInterface::emitShow()
{
    emit askShow();
}

void MainInterface::emitRaise()
{
    emit askRaise();
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
    qt_intf_t *p_intf = (qt_intf_t *)param;

    if( p_intf->pf_show_dialog )
    {
        p_intf->pf_show_dialog( p_intf->intf, INTF_DIALOG_POPUPMENU,
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
    qt_intf_t *p_intf = (qt_intf_t *)param;
    p_intf->p_mi->emitShow();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfRaiseMainCB: callback triggered by the intf-show-main libvlc variable.
 *****************************************************************************/
static int IntfRaiseMainCB( vlc_object_t *, const char *,
                            vlc_value_t, vlc_value_t, void *param )
{
    qt_intf_t *p_intf = (qt_intf_t *)param;
    p_intf->p_mi->emitRaise();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfBossCB: callback triggered by the intf-boss libvlc variable.
 *****************************************************************************/
static int IntfBossCB( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void *param )
{
    qt_intf_t *p_intf = (qt_intf_t *)param;
    p_intf->p_mi->emitBoss();

    return VLC_SUCCESS;
}
