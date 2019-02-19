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
#include "input_manager.hpp"                    // Creation
#include "actions_manager.hpp"                  // killInstance
#include "managers/renderer_manager.hpp"

#include "util/customwidgets.hpp"               // qtEventToVLCKey, QVLCStackedWidget
#include "util/qt_dirs.hpp"                     // toNativeSeparators
#include "util/imagehelper.hpp"

#include "components/interface_widgets.hpp"     // bgWidget, videoWidget
#include "components/controller.hpp"            // controllers
#include "components/playlist/playlist.hpp"     // plWidget
#include "dialogs/firstrun.hpp"                 // First Run
#include "dialogs/playlist.hpp"                 // PlaylistDialog

#include "menus.hpp"                            // Menu creation
#include "recents.hpp"                          // RecentItems when DnD

#include <QCloseEvent>
#include <QKeyEvent>

#include <QUrl>
#include <QSize>
#include <QDate>
#include <QMimeData>

#include <QWindow>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QStackedWidget>
#include <QScreen>
#ifdef _WIN32
#include <QFileInfo>
#endif

#if ! HAS_QT510 && defined(QT5_HAS_X11)
# include <QX11Info>
# include <X11/Xlib.h>
#endif

#include <QTimer>

#include <vlc_actions.h>                    /* Wheel event */
#include <vlc_vout_window.h>                /* VOUT_ events */

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
    bgWidget             = NULL;
    videoWidget          = NULL;
    playlistWidget       = NULL;
    stackCentralOldWidget= NULL;
    lastWinScreen        = NULL;
    sysTray              = NULL;
    fullscreenControls   = NULL;
    cryptedLabel         = NULL;
    controls             = NULL;
    inputC               = NULL;

    b_hideAfterCreation  = false; // --qt-start-minimized
    playlistVisible      = false;
    input_name           = "";
    b_interfaceFullScreen= false;
    b_hasPausedWhenMinimized = false;
    i_kc_offset          = false;
    b_maximizedView      = false;
    b_isWindowTiled      = false;

    /* Ask for Privacy */
    FirstRun::CheckAndRun( this, p_intf );

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

    /* Set the other interface settings */
    settings = getSettings();

    /* */
    b_plDocked = getSettings()->value( "MainWindow/pl-dock-status", true ).toBool();

    /* Should the UI stays on top of other windows */
    b_interfaceOnTop = var_InheritBool( p_intf, "video-on-top" );

#ifdef QT5_HAS_WAYLAND
    b_hasWayland = QGuiApplication::platformName()
        .startsWith(QLatin1String("wayland"), Qt::CaseInsensitive);
#endif

    /**************************
     *  UI and Widgets design
     **************************/
    setVLCWindowsTitle();

    /************
     * Menu Bar *
     ************/
    VLCMenuBar::createMenuBar( this, p_intf );
    CONNECT( THEMIM->getIM(), voutListChanged( vout_thread_t **, int ),
             THEDP, destroyPopupMenu() );

    createMainWidget( settings );

    /**************
     * Status Bar *
     **************/
    createStatusBar();
    setStatusBarVisibility( getSettings()->value( "MainWindow/status-bar-visible", false ).toBool() );

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
    CONNECT( THEMIM->getIM(), nameChanged( const QString& ),
             this, setName( const QString& ) );
    /* and title of the Main Interface*/
    if( var_InheritBool( p_intf, "qt-name-in-title" ) )
    {
        CONNECT( THEMIM->getIM(), nameChanged( const QString& ),
                 this, setVLCWindowsTitle( const QString& ) );
    }
    CONNECT( THEMIM, inputChanged( bool ), this, onInputChanged( bool ) );

    /* END CONNECTS ON IM */

    /* VideoWidget connects for asynchronous calls */
    b_videoFullScreen = false;
    connect( this, SIGNAL(askGetVideo(struct vout_window_t*, unsigned, unsigned, bool)),
             this, SLOT(getVideoSlot(struct vout_window_t*, unsigned, unsigned, bool)),
             Qt::BlockingQueuedConnection );
    connect( this, SIGNAL(askReleaseVideo( void )),
             this, SLOT(releaseVideoSlot( void )),
             Qt::BlockingQueuedConnection );
    CONNECT( this, askVideoOnTop(bool), this, setVideoOnTop(bool));

    if( videoWidget )
    {
        if( b_autoresize )
        {
            CONNECT( videoWidget, sizeChanged( int, int ),
                     this, videoSizeChanged( int,  int ) );
        }
        CONNECT( this, askVideoToResize( unsigned int, unsigned int ),
                 this, setVideoSize( unsigned int, unsigned int ) );

        CONNECT( this, askVideoSetFullScreen( bool ),
                 this, setVideoFullScreen( bool ) );
    }

    CONNECT( THEDP, toolBarConfUpdated(), this, toolBarConfUpdated() );
    installEventFilter( this );

    CONNECT( this, askToQuit(), THEDP, quit() );

    CONNECT( this, askBoss(), this, setBoss() );
    CONNECT( this, askRaise(), this, setRaise() );


    connect( THEDP, &DialogsProvider::releaseMouseEvents, this, &MainInterface::voutReleaseMouseEvents ) ;
    /** END of CONNECTS**/


    /************
     * Callbacks
     ************/
    var_AddCallback( pl_Get(p_intf), "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_AddCallback( pl_Get(p_intf), "intf-boss", IntfBossCB, p_intf );
    var_AddCallback( pl_Get(p_intf), "intf-show", IntfRaiseMainCB, p_intf );

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( pl_Get(p_intf), "intf-popupmenu", PopupMenuCB, p_intf );


    /* Final Sizing, restoration and placement of the interface */
    if( settings->value( "MainWindow/playlist-visible", false ).toBool() )
        togglePlaylist();

    QVLCTools::restoreWidgetPosition( settings, this, QSize(600, 420) );

    b_interfaceFullScreen = isFullScreen();

    setVisible( !b_hideAfterCreation );

    /* Switch to minimal view if needed, must be called after the show() */
    if( b_minimalView )
        toggleMinimalView( true );

    computeMinimumSize();
}

MainInterface::~MainInterface()
{
    /* Unsure we hide the videoWidget before destroying it */
    if( stackCentralOldWidget == videoWidget )
        showTab( bgWidget );

    if( videoWidget )
        releaseVideoSlot();

    /* Be sure to kill the actionsManager... Only used in the MI and control */
    ActionsManager::killInstance();

    /* Delete the FSC controller */
    delete fullscreenControls;

    RendererManager::killInstance();

    /* Save states */

    settings->beginGroup("MainWindow");
    settings->setValue( "pl-dock-status", b_plDocked );

    /* Save playlist state */
    settings->setValue( "playlist-visible", playlistVisible );

    settings->setValue( "adv-controls",
                        getControlsVisibilityStatus() & CONTROLS_ADVANCED );
    settings->setValue( "status-bar-visible", b_statusbarVisible );

    /* Save the stackCentralW sizes */
    settings->setValue( "bgSize", stackWidgetsSizes[bgWidget] );
    settings->setValue( "playlistSize", stackWidgetsSizes[playlistWidget] );
    settings->endGroup();

    /* Save this size */
    QVLCTools::saveWidgetPosition(settings, this);

    /* Unregister callbacks */
    var_DelCallback( pl_Get(p_intf), "intf-boss", IntfBossCB, p_intf );
    var_DelCallback( pl_Get(p_intf), "intf-show", IntfRaiseMainCB, p_intf );
    var_DelCallback( pl_Get(p_intf), "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_DelCallback( pl_Get(p_intf), "intf-popupmenu", PopupMenuCB, p_intf );

    p_intf->p_sys->p_mi = NULL;
}

void MainInterface::computeMinimumSize()
{
    int minWidth = 80;
    if( menuBar()->isVisible() )
        minWidth += controls->sizeHint().width();

    setMinimumWidth( minWidth );
}

/*****************************
 *   Main UI handling        *
 *****************************/
void MainInterface::recreateToolbars()
{
    bool b_adv = getControlsVisibilityStatus() & CONTROLS_ADVANCED;

    delete controls;
    delete inputC;

    controls = new ControlsWidget( p_intf, b_adv, this );
    inputC = new InputControlsWidget( p_intf, this );
    mainLayout->insertWidget( 2, inputC );
    mainLayout->insertWidget( settings->value( "MainWindow/ToolbarPos", false ).toBool() ? 0: 3,
                              controls );

    if( fullscreenControls )
    {
        delete fullscreenControls;
        fullscreenControls = new FullscreenControllerWidget( p_intf, this );
        CONNECT( fullscreenControls, keyPressed( QKeyEvent * ),
                 this, handleKeyPress( QKeyEvent * ) );
        THEMIM->requestVoutUpdate();
    }

    setMinimalView( b_minimalView );
}

void MainInterface::reloadPrefs()
{
    i_notificationSetting = var_InheritInteger( p_intf, "qt-notification" );
    b_pauseOnMinimize = var_InheritBool( p_intf, "qt-pause-minimized" );
    if( !var_InheritBool( p_intf, "qt-fs-controller" ) && fullscreenControls )
    {
        delete fullscreenControls;
        fullscreenControls = NULL;
    }
}

void MainInterface::createResumePanel( QWidget *w )
{
    resumePanel = new QWidget( w );
    resumePanel->hide();
    QHBoxLayout *resumePanelLayout = new QHBoxLayout( resumePanel );
    resumePanelLayout->setSpacing( 0 ); resumePanelLayout->setMargin( 0 );

    QLabel *continuePixmapLabel = new QLabel();
    continuePixmapLabel->setPixmap( ImageHelper::loadSvgToPixmap( ":/menu/help.svg" , fontMetrics().height(), fontMetrics().height()) );
    continuePixmapLabel->setContentsMargins( 5, 0, 5, 0 );

    QLabel *continueLabel = new QLabel( qtr( "Do you want to restart the playback where left off?") );

    QToolButton *cancel = new QToolButton( resumePanel );
    cancel->setAutoRaise( true );
    cancel->setText( "X" );

    QPushButton *ok = new QPushButton( qtr( "&Continue" )  );

    resumePanelLayout->addWidget( continuePixmapLabel );
    resumePanelLayout->addWidget( continueLabel );
    resumePanelLayout->addStretch( 1 );
    resumePanelLayout->addWidget( ok );
    resumePanelLayout->addWidget( cancel );

    resumeTimer = new QTimer( resumePanel );
    resumeTimer->setSingleShot( true );
    resumeTimer->setInterval( 6000 );

    CONNECT( resumeTimer, timeout(), this, hideResumePanel() );
    CONNECT( cancel, clicked(), this, hideResumePanel() );
    CONNECT( THEMIM->getIM(), resumePlayback(vlc_tick_t), this, showResumePanel(vlc_tick_t) );
    BUTTONACT( ok, resumePlayback() );

    w->layout()->addWidget( resumePanel );
}

void MainInterface::showResumePanel( vlc_tick_t _time ) {
    int setting = var_InheritInteger( p_intf, "qt-continue" );

    if( setting == 0 )
        return;

    i_resumeTime = _time;

    if( setting == 2)
        resumePlayback();
    else
    {
        if( !isFullScreen() && !isMaximized() && !b_isWindowTiled )
            resizeWindow( width(), height() + resumePanel->height() );
        resumePanel->setVisible(true);
        resumeTimer->start();
    }
}

void MainInterface::hideResumePanel()
{
    if( resumePanel->isVisible() )
    {
        if( !isFullScreen() && !isMaximized() && !b_isWindowTiled )
            resizeWindow( width(), height() - resumePanel->height() );
        resumePanel->hide();
        resumeTimer->stop();
    }
}

void MainInterface::resumePlayback()
{
    if( THEMIM->getIM()->hasInput() ) {
        var_SetInteger( THEMIM->getInput(), "time", i_resumeTime );
    }
    hideResumePanel();
}

void MainInterface::onInputChanged( bool hasInput )
{
    if( hasInput == false )
        return;
    int autoRaise = var_InheritInteger( p_intf, "qt-auto-raise" );
    if ( autoRaise == MainInterface::RAISE_NEVER )
        return;
    if( THEMIM->getIM()->hasVideo() == true )
    {
        if( ( autoRaise & MainInterface::RAISE_VIDEO ) == 0 )
            return;
    }
    else if ( ( autoRaise & MainInterface::RAISE_AUDIO ) == 0 )
        return;
    emit askRaise();
}

void MainInterface::createMainWidget( QSettings *creationSettings )
{
    /* Create the main Widget and the mainLayout */
    QWidget *main = new QWidget;
    setCentralWidget( main );
    mainLayout = new QVBoxLayout( main );
    main->setContentsMargins( 0, 0, 0, 0 );
    mainLayout->setSpacing( 0 ); mainLayout->setMargin( 0 );

    createResumePanel( main );
    /* */
    stackCentralW = new QVLCStackedWidget( main );

    /* Bg Cone */
    if ( QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY
         && var_InheritBool( p_intf, "qt-icon-change" ) )
    {
        bgWidget = new EasterEggBackgroundWidget( p_intf );
        CONNECT( this, kc_pressed(), bgWidget, animate() );
    }
    else
        bgWidget = new BackgroundWidget( p_intf );

    stackCentralW->addWidget( bgWidget );
    if ( !var_InheritBool( p_intf, "qt-bgcone" ) )
        bgWidget->setWithArt( false );
    else
        if ( var_InheritBool( p_intf, "qt-bgcone-expands" ) )
            bgWidget->setExpandstoHeight( true );

    /* And video Outputs */
    if( var_InheritBool( p_intf, "embedded-video" ) )
    {
        videoWidget = new VideoWidget( p_intf, stackCentralW );
        stackCentralW->addWidget( videoWidget );
    }
    mainLayout->insertWidget( 1, stackCentralW );

    stackWidgetsSizes[bgWidget] =
        creationSettings->value( "MainWindow/bgSize", QSize( 600, 0 ) ).toSize();
    /* Resize even if no-auto-resize, because we are at creation */
    resizeStack( stackWidgetsSizes[bgWidget].width(), stackWidgetsSizes[bgWidget].height() );

    /* Create the CONTROLS Widget */
    controls = new ControlsWidget( p_intf,
        creationSettings->value( "MainWindow/adv-controls", false ).toBool(), this );
    inputC = new InputControlsWidget( p_intf, this );

    mainLayout->insertWidget( 2, inputC );
    mainLayout->insertWidget(
        creationSettings->value( "MainWindow/ToolbarPos", false ).toBool() ? 0: 3,
        controls );

    /* Visualisation, disabled for now, they SUCK */
    #if 0
    visualSelector = new VisualSelector( p_intf );
    mainLayout->insertWidget( 0, visualSelector );
    visualSelector->hide();
    #endif


    /* Enable the popup menu in the MI */
    main->setContextMenuPolicy( Qt::CustomContextMenu );
    CONNECT( main, customContextMenuRequested( const QPoint& ),
             THEDP, setPopupMenu() );

    if ( depth() > 8 ) /* 8bit depth has too many issues with opacity */
        /* Create the FULLSCREEN CONTROLS Widget */
        if( var_InheritBool( p_intf, "qt-fs-controller" ) )
        {
            fullscreenControls = new FullscreenControllerWidget( p_intf, this );
            CONNECT( fullscreenControls, keyPressed( QKeyEvent * ),
                     this, handleKeyPress( QKeyEvent * ) );
        }

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

inline void MainInterface::createStatusBar()
{
    /****************
     *  Status Bar  *
     ****************/
    /* Widgets Creation*/
    QStatusBar *statusBarr = statusBar();

    TimeLabel *timeLabel = new TimeLabel( p_intf );
    nameLabel = new ClickableQLabel();
    nameLabel->setTextInteractionFlags( Qt::TextSelectableByMouse
                                      | Qt::TextSelectableByKeyboard );
    SpeedLabel *speedLabel = new SpeedLabel( p_intf, this );

    /* Styling those labels */
    timeLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    speedLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    nameLabel->setFrameStyle( QFrame::Sunken | QFrame::StyledPanel);
    timeLabel->setStyleSheet(
            "QLabel:hover { background-color: rgba(255, 255, 255, 50%) }" );
    speedLabel->setStyleSheet(
            "QLabel:hover { background-color: rgba(255, 255, 255, 50%) }" );
    /* pad both label and its tooltip */
    nameLabel->setStyleSheet( "padding-left: 5px; padding-right: 5px;" );

    /* and adding those */
    statusBarr->addWidget( nameLabel, 8 );
    statusBarr->addPermanentWidget( speedLabel, 0 );
    statusBarr->addPermanentWidget( timeLabel, 0 );

    CONNECT( nameLabel, doubleClicked(), THEDP, epgDialog() );
    /* timeLabel behaviour:
       - double clicking opens the goto time dialog
       - right-clicking and clicking just toggle between remaining and
         elapsed time.*/
    CONNECT( timeLabel, doubleClicked(), THEDP, gotoTimeDialog() );

    CONNECT( THEMIM->getIM(), encryptionChanged( bool ),
             this, showCryptedLabel( bool ) );

    /* This shouldn't be necessary, but for somehow reason, the statusBarr
       starts at height of 20px and when a text is shown it needs more space.
       But, as the QMainWindow policy doesn't allow statusBar to change QMW's
       geometry, we need to force a height. If you have a better idea, please
       tell me -- jb
     */
    statusBarr->setFixedHeight( statusBarr->sizeHint().height() + 2 );
}

/**********************************************************************
 * Handling of sizing of the components
 **********************************************************************/

void MainInterface::debug()
{
#ifdef DEBUG_INTF
    if( controls ) {
        msg_Dbg( p_intf, "Controls size: %i - %i", controls->size().height(), controls->size().width() );
        msg_Dbg( p_intf, "Controls minimumsize: %i - %i", controls->minimumSize().height(), controls->minimumSize().width() );
        msg_Dbg( p_intf, "Controls sizeHint: %i - %i", controls->sizeHint().height(), controls->sizeHint().width() );
    }

    msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
    msg_Dbg( p_intf, "sizeHint: %i - %i", sizeHint().height(), sizeHint().width() );
    msg_Dbg( p_intf, "minimumsize: %i - %i", minimumSize().height(), minimumSize().width() );

    msg_Dbg( p_intf, "Stack size: %i - %i", stackCentralW->size().height(), stackCentralW->size().width() );
    msg_Dbg( p_intf, "Stack sizeHint: %i - %i", stackCentralW->sizeHint().height(), stackCentralW->sizeHint().width() );
    msg_Dbg( p_intf, "Central size: %i - %i", centralWidget()->size().height(), centralWidget()->size().width() );
#endif
}

inline void MainInterface::showVideo() { showTab( videoWidget ); }
inline void MainInterface::restoreStackOldWidget( bool video_closing )
            { showTab( stackCentralOldWidget, video_closing ); }

inline void MainInterface::showTab( QWidget *widget, bool video_closing )
{
    if ( !widget ) widget = bgWidget; /* trying to restore a null oldwidget */
#ifdef DEBUG_INTF
    if ( stackCentralOldWidget )
        msg_Dbg( p_intf, "Old stackCentralOldWidget %s at index %i",
                 stackCentralOldWidget->metaObject()->className(),
                 stackCentralW->indexOf( stackCentralOldWidget ) );
    msg_Dbg( p_intf, "ShowTab request for %s", widget->metaObject()->className() );
#endif
    if ( stackCentralW->currentWidget() == widget )
        return;

    /* fixing when the playlist has been undocked after being hidden.
       restoreStackOldWidget() is called when video stops but
       stackCentralOldWidget would still be pointing to playlist */
    if ( widget == playlistWidget && !isPlDocked() )
        widget = bgWidget;

    stackCentralOldWidget = stackCentralW->currentWidget();
    if( !isFullScreen() )
        stackWidgetsSizes[stackCentralOldWidget] = stackCentralW->size();

    /* If we are playing video, embedded */
    if( !video_closing && videoWidget && THEMIM->getIM()->hasVideo() )
    {
        /* Video -> Playlist */
        if( videoWidget == stackCentralOldWidget && widget == playlistWidget )
        {
            stackCentralW->removeWidget( videoWidget );
            videoWidget->show(); videoWidget->raise();
        }

        /* Playlist -> Video */
        if( playlistWidget == stackCentralOldWidget && widget == videoWidget )
        {
            playlistWidget->artContainer->removeWidget( videoWidget );
            videoWidget->show(); videoWidget->raise();
            stackCentralW->addWidget( videoWidget );
        }

        /* Embedded playlist -> Non-embedded playlist */
        if( bgWidget == stackCentralOldWidget && widget == videoWidget )
        {
            /* In rare case when video is started before the interface */
            if( playlistWidget != NULL )
                playlistWidget->artContainer->removeWidget( videoWidget );
            videoWidget->show(); videoWidget->raise();
            stackCentralW->addWidget( videoWidget );
            stackCentralW->setCurrentWidget( videoWidget );
        }
    }

    stackCentralW->setCurrentWidget( widget );
    if( b_autoresize )
    {
        QSize size = stackWidgetsSizes[widget];
        if( size.isValid() )
            resizeStack( size.width(), size.height() );
    }

#ifdef DEBUG_INTF
    msg_Dbg( p_intf, "Stack state changed to %s, index %i",
              stackCentralW->currentWidget()->metaObject()->className(),
              stackCentralW->currentIndex() );
    msg_Dbg( p_intf, "New stackCentralOldWidget %s at index %i",
              stackCentralOldWidget->metaObject()->className(),
              stackCentralW->indexOf( stackCentralOldWidget ) );
#endif

    /* This part is done later, to account for the new pl size */
    if( !video_closing && videoWidget && THEMIM->getIM()->hasVideo() &&
        videoWidget == stackCentralOldWidget && widget == playlistWidget )
    {
        playlistWidget->artContainer->addWidget( videoWidget );
        playlistWidget->artContainer->setCurrentWidget( videoWidget );
    }
}

void MainInterface::toggleFSC()
{
   if( !fullscreenControls ) return;

   IMEvent *eShow = new IMEvent( IMEvent::FullscreenControlToggle );
   QApplication::postEvent( fullscreenControls, eShow );
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
    };

    if( videoActive.test_and_set() )
        return false;

    p_wnd->ops = &ops;
    p_wnd->info.has_double_click = true;
    p_wnd->sys = this;
    return true;
}

void MainInterface::getVideoSlot( struct vout_window_t *p_wnd,
                                  unsigned i_width, unsigned i_height,
                                  bool fullscreen )
{
    /* Hidden or minimized, activate */
    if( isHidden() || isMinimized() )
        toggleUpdateSystrayMenu();

    /* Request the videoWidget */
    if ( !videoWidget )
    {
        videoWidget = new VideoWidget( p_intf, stackCentralW );
        stackCentralW->addWidget( videoWidget );
    }

    videoWidget->request( p_wnd );
    if( true ) /* The videoWidget is available */
    {
        setVideoFullScreen( fullscreen );

        /* Consider the video active now */
        showVideo();

        /* Ask videoWidget to resize correctly, if we are in normal mode */
        if( b_autoresize ) {
#if HAS_QT56
            qreal factor = videoWidget->devicePixelRatioF();

            i_width = qRound( (qreal) i_width / factor );
            i_height = qRound( (qreal) i_height / factor );
#endif

            videoWidget->setSize( i_width, i_height );
        }
    }
}

/* Function that is CONNECTED to the previous emit */
void MainInterface::releaseVideoSlot( void )
{
    /* This function is called when the embedded video window is destroyed,
     * or in the rare case that the embedded window is still here but the
     * Qt interface exits. */
    assert( videoWidget );
    videoWidget->release();
    setVideoOnTop( false );
    setVideoFullScreen( false );
    hideResumePanel();

    if( stackCentralW->currentWidget() == videoWidget )
        restoreStackOldWidget( true );
    else if( playlistWidget &&
             playlistWidget->artContainer->currentWidget() == videoWidget )
    {
        playlistWidget->artContainer->setCurrentIndex( 0 );
        stackCentralW->addWidget( videoWidget );
    }

    /* We don't want to have a blank video to popup */
    stackCentralOldWidget = bgWidget;
}

// The provided size is in physical pixels, coming from the core.
void MainInterface::setVideoSize( unsigned int w, unsigned int h )
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
#if HAS_QT56
            float factor = videoWidget->devicePixelRatioF();
#else
            float factor = 1.0f;
#endif
            if( (float)h / factor > screen.height() )
            {
                w = screen.width();
                h = screen.height();
                if( !b_minimalView )
                {
                    if( menuBar()->isVisible() )
                        h -= menuBar()->height();
                    if( controls->isVisible() )
                        h -= controls->height();
                    if( statusBar()->isVisible() )
                        h -= statusBar()->height();
                    if( inputC->isVisible() )
                        h -= inputC->height();
                }
                h -= style()->pixelMetric(QStyle::PM_TitleBarHeight);
                h -= style()->pixelMetric(QStyle::PM_LayoutBottomMargin);
                h -= 2 * style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
            }
            else
            {
                // Convert the size in logical pixels
                w = qRound( (float)w / factor );
                h = qRound( (float)h / factor );
                msg_Dbg( p_intf, "Logical video size: %ux%u", w, h );
            }
            videoWidget->setSize( w, h );
        }
        else
            videoWidget->setSize( videoWidget->width(), videoWidget->height() );
    }
}

void MainInterface::videoSizeChanged( int w, int h )
{
    if( !playlistWidget || playlistWidget->artContainer->currentWidget() != videoWidget )
        resizeStack( w, h );
}

void MainInterface::setVideoFullScreen( bool fs )
{
    b_videoFullScreen = fs;
    if( fs )
    {
        int numscreen = var_InheritInteger( p_intf, "qt-fullscreen-screennumber" );

        if ( numscreen >= 0 && numscreen < QApplication::desktop()->screenCount() )
        {
            if( fullscreenControls )
                fullscreenControls->setTargetScreen( numscreen );

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

        if( playlistWidget != NULL && playlistWidget->artContainer->currentWidget() == videoWidget )
            showTab( videoWidget );

        /* we won't be able to get its windowed sized once in fullscreen, so update it now */
        stackWidgetsSizes[stackCentralW->currentWidget()] = stackCentralW->size();

        /* */
        displayNormalView();
        setInterfaceFullScreen( true );
    }
    else
    {
        setMinimalView( b_minimalView );
        setInterfaceFullScreen( b_interfaceFullScreen );
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
        if( pendingResize.isValid() )
        {
            /* apply resize requested while fullscreen was enabled */
            resizeStack( pendingResize.width(), pendingResize.height() );
            pendingResize = QSize(); // consume
        }

    }
    videoWidget->sync();
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
}

/* Asynchronous calls for video window contrlos */
int MainInterface::enableVideo( vout_window_t *p_wnd,
                                 const vout_window_cfg_t *cfg )
{
    MainInterface *p_mi = (MainInterface *)p_wnd->sys;

    msg_Dbg( p_wnd, "requesting video window..." );
    /* This is a blocking call signal. Results are stored directly in the
     * vout_window_t and boolean pointers. Beware of deadlocks! */
    emit p_mi->askGetVideo( p_wnd, cfg->width, cfg->height,
                            cfg->is_fullscreen );
    return VLC_SUCCESS;
}

void MainInterface::disableVideo( vout_window_t *p_wnd )
{
    MainInterface *p_mi = (MainInterface *)p_wnd->sys;

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

   emit p_mi->askVideoSetFullScreen( false );
}

void MainInterface::requestVideoFullScreen( vout_window_t *wnd, const char * )
{
    MainInterface *p_mi = (MainInterface *)wnd->sys;

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
}

/*****************************************************************************
 * Playlist, Visualisation and Menus handling
 *****************************************************************************/
/**
 * Toggle the playlist widget or dialog
 **/
void MainInterface::createPlaylist()
{
    PlaylistDialog *dialog = PlaylistDialog::getInstance( p_intf );

    if( b_plDocked )
    {
        playlistWidget = dialog->exportPlaylistWidget();
        stackCentralW->addWidget( playlistWidget );
        stackWidgetsSizes[playlistWidget] = settings->value( "playlistSize", QSize( 600, 300 ) ).toSize();
    }
    CONNECT( dialog, visibilityChanged(bool), this, setPlaylistVisibility(bool) );
}

void MainInterface::togglePlaylist()
{
    if( !playlistWidget ) createPlaylist();

    PlaylistDialog *dialog = PlaylistDialog::getInstance( p_intf );
    if( b_plDocked )
    {
        if ( dialog->hasPlaylistWidget() )
            playlistWidget = dialog->exportPlaylistWidget();
        /* Playlist is not visible, show it */
        if( stackCentralW->currentWidget() != playlistWidget )
        {
            if( stackCentralW->indexOf( playlistWidget ) == -1 )
                stackCentralW->addWidget( playlistWidget );
            showTab( playlistWidget );
        }
        else /* Hide it! */
        {
            restoreStackOldWidget();
        }
        playlistVisible = ( stackCentralW->currentWidget() == playlistWidget );
    }
    else
    {
        playlistVisible = !playlistVisible;
        if ( ! dialog->hasPlaylistWidget() )
            dialog->importPlaylistWidget( playlistWidget );
        if ( playlistVisible )
            dialog->show();
        else
            dialog->hide();
    }
    debug();
}

const Qt::Key MainInterface::kc[10] =
{
    Qt::Key_Up, Qt::Key_Up,
    Qt::Key_Down, Qt::Key_Down,
    Qt::Key_Left, Qt::Key_Right, Qt::Key_Left, Qt::Key_Right,
    Qt::Key_B, Qt::Key_A
};

void MainInterface::dockPlaylist( bool p_docked )
{
    if( b_plDocked == p_docked ) return;
    /* some extra check */
    if ( b_plDocked && !playlistWidget ) createPlaylist();

    b_plDocked = p_docked;
    PlaylistDialog *dialog = PlaylistDialog::getInstance( p_intf );

    if( !p_docked ) /* Previously docked */
    {
        playlistVisible = playlistWidget->isVisible();

        /* repositioning the videowidget __before__ exporting the
           playlistwidget into the playlist dialog avoids two unneeded
           calls to the server in the qt library to reparent the underlying
           native window back and forth.
           For Wayland, this is mandatory since reparenting is not implemented.
           For X11 or Windows, this is just an optimization. */
        if ( videoWidget && THEMIM->getIM()->hasVideo() )
            showTab(videoWidget);
        else
            showTab(bgWidget);

        /* playlistwidget exported into the playlist dialog */
        stackCentralW->removeWidget( playlistWidget );
        dialog->importPlaylistWidget( playlistWidget );
        if ( playlistVisible ) dialog->show();
    }
    else /* Previously undocked */
    {
        playlistVisible = dialog->isVisible() && !( videoWidget && THEMIM->getIM()->hasVideo() );
        dialog->hide();
        playlistWidget = dialog->exportPlaylistWidget();
        stackCentralW->addWidget( playlistWidget );

        /* If playlist is invisible don't show it */
        if( playlistVisible ) showTab( playlistWidget );
    }
}

/*
 * displayNormalView is the private function used by
 * the SLOT setVideoFullScreen to restore the menuBar
 * if minimal view is off
 */
void MainInterface::displayNormalView()
{
    menuBar()->setVisible( false );
    controls->setVisible( false );
    statusBar()->setVisible( false );
    inputC->setVisible( false );
}

/*
 * setMinimalView is the private function used by
 * the SLOT toggleMinimalView
 */
void MainInterface::setMinimalView( bool b_minimal )
{
    bool b_menuBarVisible = menuBar()->isVisible();
    bool b_controlsVisible = controls->isVisible();
    bool b_statusBarVisible = statusBar()->isVisible();
    bool b_inputCVisible = inputC->isVisible();

    if( !isFullScreen() && !isMaximized() && b_minimal && !b_isWindowTiled )
    {
        int i_heightChange = 0;

        if( b_menuBarVisible )
            i_heightChange += menuBar()->height();
        if( b_controlsVisible )
            i_heightChange += controls->height();
        if( b_statusBarVisible )
            i_heightChange += statusBar()->height();
        if( b_inputCVisible )
            i_heightChange += inputC->height();

        if( i_heightChange != 0 )
            resizeWindow( width(), height() - i_heightChange );
    }

    menuBar()->setVisible( !b_minimal );
    controls->setVisible( !b_minimal );
    statusBar()->setVisible( !b_minimal && b_statusbarVisible );
    inputC->setVisible( !b_minimal );

    if( !isFullScreen() && !isMaximized() && !b_minimal && !b_isWindowTiled )
    {
        int i_heightChange = 0;

        if( !b_menuBarVisible && menuBar()->isVisible() )
            i_heightChange += menuBar()->height();
        if( !b_controlsVisible && controls->isVisible() )
            i_heightChange += controls->height();
        if( !b_statusBarVisible && statusBar()->isVisible() )
            i_heightChange += statusBar()->height();
        if( !b_inputCVisible && inputC->isVisible() )
            i_heightChange += inputC->height();

        if( i_heightChange != 0 )
            resizeWindow( width(), height() + i_heightChange );
    }
}

/*
 * This public SLOT is used for moving to minimal View Mode
 *
 * If b_minimal is false, then we are normalView
 */
void MainInterface::toggleMinimalView( bool b_minimal )
{
    if( !b_minimalView && b_autoresize ) /* Normal mode */
    {
        if( stackCentralW->currentWidget() == bgWidget )
        {
            if( stackCentralW->height() < 16 )
            {
                resizeStack( stackCentralW->width(), 100 );
            }
        }
    }
    b_minimalView = b_minimal;
    if( !b_videoFullScreen )
    {
        setMinimalView( b_minimalView );
        computeMinimumSize();
    }

    emit minimalViewToggled( b_minimalView );
}

/* toggling advanced controls buttons */
void MainInterface::toggleAdvancedButtons()
{
    controls->toggleAdvanced();
//    if( fullscreenControls ) fullscreenControls->toggleAdvanced();
}

/* Get the visibility status of the controls (hidden or not, advanced or not) */
int MainInterface::getControlsVisibilityStatus()
{
    if( !controls ) return 0;
    return( (controls->isVisible() ? CONTROLS_VISIBLE : CONTROLS_HIDDEN )
            + CONTROLS_ADVANCED * controls->b_advancedVisible );
}

StandardPLPanel *MainInterface::getPlaylistView()
{
    if( !playlistWidget ) return NULL;
    else return playlistWidget->mainView;
}

void MainInterface::setStatusBarVisibility( bool b_visible )
{
    statusBar()->setVisible( b_visible );
    b_statusbarVisible = b_visible;
    if( controls ) controls->setGripVisible( !b_statusbarVisible );
}


void MainInterface::setPlaylistVisibility( bool b_visible )
{
    if( isPlDocked() || THEDP->isDying() || (playlistWidget && playlistWidget->isMinimized() ) )
        return;

    playlistVisible = b_visible;
}

/************************************************************************
 * Other stuff
 ************************************************************************/
void MainInterface::setName( const QString& name )
{
    input_name = name; /* store it for the QSystray use */
    /* Display it in the status bar, but also as a Tooltip in case it doesn't
       fit in the label */
    nameLabel->setText( name );
    nameLabel->setToolTip( name );
}

/**
 * Give the decorations of the Main Window a correct Name.
 * If nothing is given, set it to VLC...
 **/
void MainInterface::setVLCWindowsTitle( const QString& aTitle )
{
    if( aTitle.isEmpty() )
    {
        setWindowTitle( qtr( "VLC media player" ) );
    }
    else
    {
        setWindowTitle( aTitle + " - " + qtr( "VLC media player" ) );
    }
}

void MainInterface::showCryptedLabel( bool b_show )
{
    if( cryptedLabel == NULL )
    {
        cryptedLabel = new QLabel;
        // The lock icon is not the right one for DRM protection/scrambled.
        //cryptedLabel->setPixmap( QPixmap( ":/lock.svg" ) );
        cryptedLabel->setText( "DRM" );
        statusBar()->addWidget( cryptedLabel );
    }

    cryptedLabel->setVisible( b_show );
}

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

    CONNECT( sysTray, activated( QSystemTrayIcon::ActivationReason ),
             this, handleSystrayClick( QSystemTrayIcon::ActivationReason ) );

    /* Connects on nameChanged() */
    CONNECT( THEMIM->getIM(), nameChanged( const QString& ),
             this, updateSystrayTooltipName( const QString& ) );
    /* Connect PLAY_STATUS on the systray */
    CONNECT( THEMIM->getIM(), playingStatusChanged( int ),
             this, updateSystrayTooltipStatus( int ) );
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
void MainInterface::updateSystrayTooltipStatus( int i_status )
{
    switch( i_status )
    {
    case PLAYING_S:
        sysTray->setToolTip( input_name );
        break;
    case PAUSE_S:
        sysTray->setToolTip( input_name + " - " + qtr( "Paused") );
        break;
    default:
        sysTray->setToolTip( qtr( "VLC media player" ) );
        break;
    }
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

            if( THEMIM->getIM()->playingStatus() == PLAYING_S &&
                THEMIM->getIM()->hasVideo() && !THEMIM->getIM()->hasVisualisation() &&
                b_pauseOnMinimize )
            {
                b_hasPausedWhenMinimized = true;
                THEMIM->pause();
            }
        }
        else if( oldState & Qt::WindowMinimized && !( newState & Qt::WindowMinimized ) )
        {
            if( b_hasPausedWhenMinimized )
            {
                THEMIM->play();
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
    if( mimeData->urls().count() == 1 && THEMIM->getIM()->hasInput() )
    {
        if( !input_AddSlave( THEMIM->getInput(), SLAVE_TYPE_SPU,
                 qtu( mimeData->urls()[0].toString() ), true, true, true ) )
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
void MainInterface::keyPressEvent( QKeyEvent *e )
{
    handleKeyPress( e );

    /* easter eggs sequence handling */
    if ( e->key() == kc[ i_kc_offset ] )
        i_kc_offset++;
    else
        i_kc_offset = 0;

    if ( i_kc_offset == (sizeof( kc ) / sizeof( Qt::Key )) )
    {
        i_kc_offset = 0;
        emit kc_pressed();
    }
}

void MainInterface::handleKeyPress( QKeyEvent *e )
{
    if( ( ( e->modifiers() & Qt::ControlModifier ) && ( e->key() == Qt::Key_H ) ) ||
        ( b_minimalView && !b_videoFullScreen && e->key() == Qt::Key_Escape ) )
    {
        toggleMinimalView( !b_minimalView );
        e->accept();
    }
    else if( ( e->modifiers() & Qt::ControlModifier ) && ( e->key() == Qt::Key_K ) &&
        playlistWidget )
    {
        playlistWidget->setSearchFieldFocus();
        e->accept();
    }

    int i_vlck = qtEventToVLCKey( e );
    if( i_vlck > 0 )
    {
        var_SetInteger( vlc_object_instance(p_intf), "key-pressed", i_vlck );
        e->accept();
    }
    else
        e->ignore();
}

void MainInterface::wheelEvent( QWheelEvent *e )
{
    int i_vlckey = qtWheelEventToVLCKey( e );
    var_SetInteger( vlc_object_instance(p_intf), "key-pressed", i_vlckey );
    e->accept();
}

void MainInterface::closeEvent( QCloseEvent *e )
{
//  hide();
    if ( b_minimalView )
        setMinimalView( false );
    emit askToQuit(); /* ask THEDP to quit, so we have a unique method */
    /* Accept session quit. Otherwise we break the desktop mamager. */
    e->accept();
}

bool MainInterface::eventFilter( QObject *obj, QEvent *event )
{
    if ( event->type() == MainInterface::ToolbarsNeedRebuild ) {
        event->accept();
        recreateToolbars();
        return true;
    } else {
        return QObject::eventFilter( obj, event );
    }
}

void MainInterface::toolBarConfUpdated()
{
    QApplication::postEvent( this, new QEvent( MainInterface::ToolbarsNeedRebuild ) );
}

void MainInterface::setInterfaceFullScreen( bool fs )
{
    if( fs )
        setWindowState( windowState() | Qt::WindowFullScreen );
    else
        setWindowState( windowState() & ~Qt::WindowFullScreen );
}
void MainInterface::toggleInterfaceFullScreen()
{
    b_interfaceFullScreen = !b_interfaceFullScreen;
    if( !b_videoFullScreen )
        setInterfaceFullScreen( b_interfaceFullScreen );
    emit fullscreenInterfaceToggled( b_interfaceFullScreen );
}

void MainInterface::emitBoss()
{
    emit askBoss();
}
void MainInterface::setBoss()
{
    THEMIM->pause();
    if( sysTray )
    {
        hide();
    }
    else
    {
        showMinimized();
    }
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

void MainInterface::voutReleaseMouseEvents()
{
    if (videoWidget)
    {
        QPoint pos = QCursor::pos();
        QPoint localpos = videoWidget->mapFromGlobal(pos);
        int buttons = QApplication::mouseButtons();
        int i_button = 1;
        while (buttons != 0)
        {
            if ( (buttons & 1) != 0 )
            {
                QMouseEvent new_e( QEvent::MouseButtonRelease, localpos,
                                   (Qt::MouseButton)i_button, (Qt::MouseButton)i_button, Qt::NoModifier );
                QApplication::sendEvent(videoWidget, &new_e);
            }
            buttons >>= 1;
            i_button <<= 1;
        }

    }
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
    p_intf->p_sys->p_mi->toggleFSC();

    /* Show event */
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
