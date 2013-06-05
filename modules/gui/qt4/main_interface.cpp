/*****************************************************************************
 * main_interface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2011 VideoLAN and AUTHORS
 * $Id$
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

#include "qt4.hpp"

#include "main_interface.hpp"
#include "input_manager.hpp"                    // Creation
#include "actions_manager.hpp"                  // killInstance

#include "util/customwidgets.hpp"               // qtEventToVLCKey, QVLCStackedWidget
#include "util/qt_dirs.hpp"                     // toNativeSeparators

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

#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QStackedWidget>
#include <QFileInfo>

#include <vlc_keys.h>                       /* Wheel event */
#include <vlc_vout_display.h>               /* vout_thread_t and VOUT_ events */

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

MainInterface::MainInterface( intf_thread_t *_p_intf ) : QVLCMW( _p_intf )
{
    /* Variables initialisation */
    bgWidget             = NULL;
    videoWidget          = NULL;
    playlistWidget       = NULL;
    stackCentralOldWidget= NULL;
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
#ifdef Q_WS_MAC
    setAttribute( Qt::WA_MacBrushedMetal );
#endif

    /* Is video in embedded in the UI or not */
    b_videoEmbedded = var_InheritBool( p_intf, "embedded-video" );

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

#ifdef _WIN32
    /* Volume keys */
    p_intf->p_sys->disable_volume_keys = var_InheritBool( p_intf, "qt-disable-volume-keys" );
#endif

    /* */
    b_plDocked = getSettings()->value( "MainWindow/pl-dock-status", true ).toBool();


    /**************************
     *  UI and Widgets design
     **************************/
    setVLCWindowsTitle();

    /************
     * Menu Bar *
     ************/
    VLCMenuBar::createMenuBar( this, p_intf );
    CONNECT( THEMIM->getIM(), voutListChanged( vout_thread_t **, int ),
             this, destroyPopupMenu() );

    createMainWidget( settings );

    /**************
     * Status Bar *
     **************/
    createStatusBar();
    setStatusBarVisibility( getSettings()->value( "MainWindow/status-bar-visible", false ).toBool() );

    /********************
     * Input Manager    *
     ********************/
    MainInputManager::getInstance( p_intf );

#ifdef _WIN32
    himl = NULL;
    p_taskbl = NULL;
    taskbar_wmsg = RegisterWindowMessage(TEXT("TaskbarButtonCreated"));
#endif

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
    /* END CONNECTS ON IM */

    /* VideoWidget connects for asynchronous calls */
    b_videoFullScreen = false;
    connect( this, SIGNAL(askGetVideo(WId*,int*,int*,unsigned*,unsigned *)),
             this, SLOT(getVideoSlot(WId*,int*,int*,unsigned*,unsigned*)),
             Qt::BlockingQueuedConnection );
    connect( this, SIGNAL(askReleaseVideo( void )),
             this, SLOT(releaseVideoSlot( void )),
             Qt::BlockingQueuedConnection );
    CONNECT( this, askVideoOnTop(bool), this, setVideoOnTop(bool));

    if( videoWidget )
    {
        if( b_autoresize )
        {
            CONNECT( this, askVideoToResize( unsigned int, unsigned int ),
                     this, setVideoSize( unsigned int, unsigned int ) );
            CONNECT( videoWidget, sizeChanged( int, int ),
                     this, videoSizeChanged( int,  int ) );
        }
        CONNECT( this, askVideoSetFullScreen( bool ),
                 this, setVideoFullScreen( bool ) );
    }

    CONNECT( THEDP, toolBarConfUpdated(), this, toolBarConfUpdated() );
    installEventFilter( this );

    CONNECT( this, askToQuit(), THEDP, quit() );

    CONNECT( this, askBoss(), this, setBoss() );
    CONNECT( this, askRaise(), this, setRaise() );

    /** END of CONNECTS**/


    /************
     * Callbacks
     ************/
    var_AddCallback( p_intf->p_libvlc, "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_AddCallback( p_intf->p_libvlc, "intf-boss", IntfBossCB, p_intf );
    var_AddCallback( p_intf->p_libvlc, "intf-show", IntfRaiseMainCB, p_intf );

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( p_intf->p_libvlc, "intf-popupmenu", PopupMenuCB, p_intf );


    /* Final Sizing, restoration and placement of the interface */
    if( settings->value( "MainWindow/playlist-visible", false ).toBool() )
        togglePlaylist();

    QVLCTools::restoreWidgetPosition( settings, this, QSize(600, 420) );

    b_interfaceFullScreen = isFullScreen();

    setVisible( !b_hideAfterCreation );

    computeMinimumSize();

    /* Switch to minimal view if needed, must be called after the show() */
    if( b_minimalView )
        toggleMinimalView( true );
}

MainInterface::~MainInterface()
{
    /* Unsure we hide the videoWidget before destroying it */
    if( stackCentralOldWidget == videoWidget )
        showTab( bgWidget );

    if( videoWidget )
        releaseVideoSlot();

#ifdef _WIN32
    if( himl )
        ImageList_Destroy( himl );
    if(p_taskbl)
        p_taskbl->Release();
    CoUninitialize();
#endif

    /* Be sure to kill the actionsManager... Only used in the MI and control */
    ActionsManager::killInstance();

    /* Delete the FSC controller */
    delete fullscreenControls;

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

    delete statusBar();

    /* Unregister callbacks */
    var_DelCallback( p_intf->p_libvlc, "intf-boss", IntfBossCB, p_intf );
    var_DelCallback( p_intf->p_libvlc, "intf-show", IntfRaiseMainCB, p_intf );
    var_DelCallback( p_intf->p_libvlc, "intf-toggle-fscontrol", IntfShowCB, p_intf );
    var_DelCallback( p_intf->p_libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    p_intf->p_sys->p_mi = NULL;
}

void MainInterface::computeMinimumSize()
{
    int minWidth = 30;
    if( menuBar()->isVisible() )
        minWidth += __MAX( controls->sizeHint().width(), menuBar()->sizeHint().width() );

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
    mainLayout->insertWidget( settings->value( "MainWindow/ToolbarPos", 0 ).toInt() ? 0: 3,
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
#ifdef _WIN32
    p_intf->p_sys->disable_volume_keys = var_InheritBool( p_intf, "qt-disable-volume-keys" );
#endif
    if( !var_InheritBool( p_intf, "qt-fs-controller" ) && fullscreenControls )
    {
        delete fullscreenControls;
        fullscreenControls = NULL;
    }
}

void MainInterface::createMainWidget( QSettings *creationSettings )
{
    /* Create the main Widget and the mainLayout */
    QWidget *main = new QWidget;
    setCentralWidget( main );
    mainLayout = new QVBoxLayout( main );
    main->setContentsMargins( 0, 0, 0, 0 );
    mainLayout->setSpacing( 0 ); mainLayout->setMargin( 0 );

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
    if( b_videoEmbedded )
    {
        videoWidget = new VideoWidget( p_intf );
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
        creationSettings->value( "MainWindow/ToolbarPos", 0 ).toInt() ? 0: 3,
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
             this, popupMenu( const QPoint& ) );

    if ( depth() > 8 ) /* 8bit depth has too many issues with opacity */
        /* Create the FULLSCREEN CONTROLS Widget */
        if( var_InheritBool( p_intf, "qt-fs-controller" ) )
        {
            fullscreenControls = new FullscreenControllerWidget( p_intf, this );
            CONNECT( fullscreenControls, keyPressed( QKeyEvent * ),
                     this, handleKeyPress( QKeyEvent * ) );
        }
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

    CONNECT( THEMIM->getIM(), seekRequested( float ),
             timeLabel, setDisplayPosition( float ) );

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
    msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
    msg_Dbg( p_intf, "sizeHint: %i - %i", sizeHint().height(), sizeHint().width() );
    msg_Dbg( p_intf, "minimumsize: %i - %i", minimumSize().height(), minimumSize().width() );

    msg_Dbg( p_intf, "Stack size: %i - %i", stackCentralW->size().height(), stackCentralW->size().width() );
    msg_Dbg( p_intf, "Stack sizeHint: %i - %i", stackCentralW->sizeHint().height(), stackCentralW->sizeHint().width() );
    msg_Dbg( p_intf, "Central size: %i - %i", centralWidget()->size().height(), centralWidget()->size().width() );
#endif
}

inline void MainInterface::showVideo() { showTab( videoWidget ); }
inline void MainInterface::restoreStackOldWidget()
            { showTab( stackCentralOldWidget ); }

inline void MainInterface::showTab( QWidget *widget )
{
    if ( !widget ) widget = bgWidget; /* trying to restore a null oldwidget */
#ifdef DEBUG_INTF
    if ( stackCentralOldWidget )
        msg_Dbg( p_intf, "Old stackCentralOldWidget %s at index %i",
                 stackCentralOldWidget->metaObject()->className(),
                 stackCentralW->indexOf( stackCentralOldWidget ) );
    msg_Dbg( p_intf, "ShowTab request for %s", widget->metaObject()->className() );
#endif
    /* fixing when the playlist has been undocked after being hidden.
       restoreStackOldWidget() is called when video stops but
       stackCentralOldWidget would still be pointing to playlist */
    if ( widget == playlistWidget && !isPlDocked() )
        widget = bgWidget;

    stackCentralOldWidget = stackCentralW->currentWidget();
    stackWidgetsSizes[stackCentralOldWidget] = stackCentralW->size();

    /* If we are playing video, embedded */
    if( videoWidget && THEMIM->getIM()->hasVideo() )
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
        resizeStack( stackWidgetsSizes[widget].width(), stackWidgetsSizes[widget].height() );

#ifdef DEBUG_INTF
    msg_Dbg( p_intf, "Stack state changed to %s, index %i",
              stackCentralW->currentWidget()->metaObject()->className(),
              stackCentralW->currentIndex() );
    msg_Dbg( p_intf, "New stackCentralOldWidget %s at index %i",
              stackCentralOldWidget->metaObject()->className(),
              stackCentralW->indexOf( stackCentralOldWidget ) );
#endif

    /* This part is done later, to account for the new pl size */
    if( videoWidget && THEMIM->getIM()->hasVideo() &&
        videoWidget == stackCentralOldWidget && widget == playlistWidget )
    {
        playlistWidget->artContainer->addWidget( videoWidget );
        playlistWidget->artContainer->setCurrentWidget( videoWidget );
    }
}

void MainInterface::destroyPopupMenu()
{
    VLCMenuBar::PopupMenu( p_intf, false );
}

void MainInterface::popupMenu( const QPoint & )
{
    VLCMenuBar::PopupMenu( p_intf, true );
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
 * You must not change the state of this object or other Qt4 UI objects,
 * from the video output thread - only from the Qt4 UI main loop thread.
 * All window provider queries must be handled through signals or events.
 * That's why we have all those emit statements...
 */
WId MainInterface::getVideo( int *pi_x, int *pi_y,
                             unsigned int *pi_width, unsigned int *pi_height )
{
    if( !videoWidget )
        return 0;

    /* This is a blocking call signal. Results are returned through pointers.
     * Beware of deadlocks! */
    WId id;
    emit askGetVideo( &id, pi_x, pi_y, pi_width, pi_height );
    return id;
}

void MainInterface::getVideoSlot( WId *p_id, int *pi_x, int *pi_y,
                                  unsigned *pi_width, unsigned *pi_height )
{
    /* Hidden or minimized, activate */
    if( isHidden() || isMinimized() )
        toggleUpdateSystrayMenu();

    /* Request the videoWidget */
    WId ret = videoWidget->request( pi_x, pi_y,
                                    pi_width, pi_height, !b_autoresize );
    *p_id = ret;
    if( ret ) /* The videoWidget is available */
    {
        /* Consider the video active now */
        showVideo();

        /* Ask videoWidget to resize correctly, if we are in normal mode */
        if( !isFullScreen() && !isMaximized() && b_autoresize )
            videoWidget->SetSizing( *pi_width, *pi_height );
    }
}

/* Asynchronous call from the WindowClose function */
void MainInterface::releaseVideo( void )
{
    emit askReleaseVideo();
}

/* Function that is CONNECTED to the previous emit */
void MainInterface::releaseVideoSlot( void )
{
    /* This function is called when the embedded video window is destroyed,
     * or in the rare case that the embedded window is still here but the
     * Qt4 interface exits. */
    assert( videoWidget );
    videoWidget->release();
    setVideoOnTop( false );
    setVideoFullScreen( false );

    if( stackCentralW->currentWidget() == videoWidget )
        restoreStackOldWidget();
    else if( playlistWidget &&
             playlistWidget->artContainer->currentWidget() == videoWidget )
    {
        playlistWidget->artContainer->setCurrentIndex( 0 );
        stackCentralW->addWidget( videoWidget );
    }

    /* We don't want to have a blank video to popup */
    stackCentralOldWidget = bgWidget;
}

void MainInterface::setVideoSize( unsigned int w, unsigned int h )
{
    if( !isFullScreen() && !isMaximized() )
        videoWidget->SetSizing( w, h );
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
        /* if user hasn't defined screennumber, or screennumber that is bigger
         * than current number of screens, take screennumber where current interface
         * is
         */
        if( numscreen == -1 || numscreen > QApplication::desktop()->numScreens() )
            numscreen = QApplication::desktop()->screenNumber( p_intf->p_sys->p_mi );

        QRect screenres = QApplication::desktop()->screenGeometry( numscreen );

        /* To be sure window is on proper-screen in xinerama */
        if( !screenres.contains( pos() ) )
        {
            msg_Dbg( p_intf, "Moving video to correct screen");
            move( QPoint( screenres.x(), screenres.y() ) );
        }

        /* */
        if( playlistWidget != NULL && playlistWidget->artContainer->currentWidget() == videoWidget )
        {
            showTab( videoWidget );
        }

        /* */
        setMinimalView( true );
        setInterfaceFullScreen( true );
    }
    else
    {
        /* TODO do we want to restore screen and position ? (when
         * qt-fullscreen-screennumber is forced) */
        setMinimalView( b_minimalView );
        setInterfaceFullScreen( b_interfaceFullScreen );
    }
    videoWidget->sync();
}

/* Slot to change the video always-on-top flag.
 * Emit askVideoOnTop() to invoke this from other thread. */
void MainInterface::setVideoOnTop( bool on_top )
{
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

/* Asynchronous call from WindowControl function */
int MainInterface::controlVideo( int i_query, va_list args )
{
    switch( i_query )
    {
    case VOUT_WINDOW_SET_SIZE:
    {
        unsigned int i_width  = va_arg( args, unsigned int );
        unsigned int i_height = va_arg( args, unsigned int );

        emit askVideoToResize( i_width, i_height );
        return VLC_SUCCESS;
    }
    case VOUT_WINDOW_SET_STATE:
    {
        unsigned i_arg = va_arg( args, unsigned );
        unsigned on_top = i_arg & VOUT_WINDOW_STATE_ABOVE;

        emit askVideoOnTop( on_top != 0 );
        return VLC_SUCCESS;
    }
    case VOUT_WINDOW_SET_FULLSCREEN:
    {
        bool b_fs = va_arg( args, int );

        emit askVideoSetFullScreen( b_fs );
        return VLC_SUCCESS;
    }
    default:
        msg_Warn( p_intf, "unsupported control query" );
        return VLC_EGENERIC;
    }
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
        stackCentralW->removeWidget( playlistWidget );
        dialog->importPlaylistWidget( playlistWidget );
        if ( playlistVisible ) dialog->show();
        restoreStackOldWidget();
    }
    else /* Previously undocked */
    {
        playlistVisible = dialog->isVisible();
        dialog->hide();
        playlistWidget = dialog->exportPlaylistWidget();
        stackCentralW->addWidget( playlistWidget );

        /* If playlist is invisible don't show it */
        if( playlistVisible ) showTab( playlistWidget );
    }
}

/*
 * setMinimalView is the private function used by
 * the SLOT toggleMinimalView and setVideoFullScreen
 */
void MainInterface::setMinimalView( bool b_minimal )
{
    menuBar()->setVisible( !b_minimal );
    controls->setVisible( !b_minimal );
    statusBar()->setVisible( !b_minimal && b_statusbarVisible );
    inputC->setVisible( !b_minimal );
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
    if ( !isPlDocked() )
        playlistVisible = b_visible;
}

#if 0
void MainInterface::visual()
{
    if( !VISIBLE( visualSelector) )
    {
        visualSelector->show();
        if( !THEMIM->getIM()->hasVideo() )
        {
            /* Show the background widget */
        }
        visualSelectorEnabled = true;
    }
    else
    {
        /* Stop any currently running visualization */
        visualSelector->hide();
        visualSelectorEnabled = false;
    }
}
#endif

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
        //cryptedLabel->setPixmap( QPixmap( ":/lock" ) );
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
        iconVLC =  QIcon( ":/logo/vlc128-xmas.png" );
    else
        iconVLC =  QIcon( ":/logo/vlc128.png" );
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
#ifdef _WIN32
        /* check if any visible window is above vlc in the z-order,
         * but ignore the ones always on top
         * and the ones which can't be activated */
        WINDOWINFO wi;
        HWND hwnd;
        wi.cbSize = sizeof( WINDOWINFO );
        for( hwnd = GetNextWindow( internalWinId(), GW_HWNDPREV );
                hwnd && ( !IsWindowVisible( hwnd ) ||
                    ( GetWindowInfo( hwnd, &wi ) &&
                      (wi.dwExStyle&WS_EX_NOACTIVATE) ) );
                hwnd = GetNextWindow( hwnd, GW_HWNDPREV ) );
            if( !hwnd || !GetWindowInfo( hwnd, &wi ) ||
                (wi.dwExStyle&WS_EX_TOPMOST) )
            {
                hide();
            }
            else
            {
                activateWindow();
            }
#else
        hide();
#endif
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
#ifdef Q_WS_MAC
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
 * \param b_playlist true to add to playlist, false to add to media library
 * \return nothing
 */
void MainInterface::dropEventPlay( QDropEvent *event, bool b_play, bool b_playlist )
{
    if( event->possibleActions() & ( Qt::CopyAction | Qt::MoveAction | Qt::LinkAction ) )
       event->setDropAction( Qt::CopyAction );
    else
        return;

    const QMimeData *mimeData = event->mimeData();

    /* D&D of a subtitles file, add it on the fly */
    if( mimeData->urls().count() == 1 && THEMIM->getIM()->hasInput() )
    {
        if( !input_AddSubtitle( THEMIM->getInput(),
                 qtu( toNativeSeparators( mimeData->urls()[0].toLocalFile() ) ),
                 true ) )
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
            if( mrl.length() > 0 )
            {
                playlist_Add( THEPL, qtu(mrl), NULL,
                          PLAYLIST_APPEND | (first ? PLAYLIST_GO: PLAYLIST_PREPARSE),
                          PLAYLIST_END, b_playlist, pl_Unlocked );
                first = false;
                RecentsMRL::getInstance( p_intf )->addRecent( mrl );
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
        playlist_Add( THEPL, qtu(mrl), NULL,
                      PLAYLIST_APPEND | (first ? PLAYLIST_GO: PLAYLIST_PREPARSE),
                      PLAYLIST_END, b_playlist, pl_Unlocked );
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

    int i_vlck = qtEventToVLCKey( e );
    if( i_vlck > 0 )
    {
        var_SetInteger( p_intf->p_libvlc, "key-pressed", i_vlck );
        e->accept();
    }
    else
        e->ignore();
}

void MainInterface::wheelEvent( QWheelEvent *e )
{
    int i_vlckey = qtWheelEventToVLCKey( e );
    var_SetInteger( p_intf->p_libvlc, "key-pressed", i_vlckey );
    e->accept();
}

void MainInterface::closeEvent( QCloseEvent *e )
{
//  hide();
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

/*****************************************************************************
 * PopupMenuCB: callback triggered by the intf-popupmenu playlist variable.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_variable ); VLC_UNUSED( old_val );

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
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_variable ); VLC_UNUSED( old_val );
    VLC_UNUSED( new_val );

    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->toggleFSC();

    /* Show event */
     return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfRaiseMainCB: callback triggered by the intf-show-main libvlc variable.
 *****************************************************************************/
static int IntfRaiseMainCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_variable ); VLC_UNUSED( old_val );
    VLC_UNUSED( new_val );

    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->emitRaise();

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfBossCB: callback triggered by the intf-boss libvlc variable.
 *****************************************************************************/
static int IntfBossCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    VLC_UNUSED( p_this ); VLC_UNUSED( psz_variable ); VLC_UNUSED( old_val );
    VLC_UNUSED( new_val );

    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->emitBoss();

    return VLC_SUCCESS;
}
