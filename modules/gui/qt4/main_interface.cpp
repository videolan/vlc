/*****************************************************************************
 * main_interface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "qt4.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "util/qvlcframe.hpp"
#include "util/customwidgets.hpp"
#include "dialogs_provider.hpp"
#include "components/interface_widgets.hpp"
#include "dialogs/playlist.hpp"
#include "menus.hpp"

#include <QMenuBar>
#include <QCloseEvent>
#include <QPushButton>
#include <QStatusBar>
#include <QKeyEvent>
#include <QUrl>
#include <QSystemTrayIcon>
#include <QSize>
#include <QMenu>
#include <QLabel>
#include <QSlider>
#include <QWidgetAction>

#include <assert.h>
#include <vlc_keys.h>
#include <vlc_vout.h>

#ifdef WIN32
    #define PREF_W 410
    #define PREF_H 151
#else
    #define PREF_W 400
    #define PREF_H 140
#endif

#define SET_WIDTH(i,j) i->widgetSize.setWidth(j)
#define SET_HEIGHT(i,j) i->widgetSize.setHeight(j)
#define SET_WH( i,j,k) i->widgetSize.setWidth(j); i->widgetSize.setHeight(k);

#define DS(i) i.width(),i.height()

/* Callback prototypes */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );
static int InteractCallback( vlc_object_t *, const char *, vlc_value_t,
                             vlc_value_t, void *);
/* Video handling */
static void *DoRequest( intf_thread_t *p_intf, vout_thread_t *p_vout,
                        int *pi1, int *pi2, unsigned int*pi3,unsigned int*pi4)
{
    return p_intf->p_sys->p_mi->requestVideo( p_vout, pi1, pi2, pi3, pi4 );
}
static void DoRelease( intf_thread_t *p_intf, void *p_win )
{
    return p_intf->p_sys->p_mi->releaseVideo( p_win );
}
static int DoControl( intf_thread_t *p_intf, void *p_win, int i_q, va_list a )
{
    return p_intf->p_sys->p_mi->controlVideo( p_win, i_q, a );
}

MainInterface::MainInterface( intf_thread_t *_p_intf ) : QVLCMW( _p_intf )
{
    /* Variables initialisation */
    need_components_update = false;
    bgWidget = NULL; videoWidget = NULL; playlistWidget = NULL;
    embeddedPlaylistWasActive = videoIsActive = false;
    input_name = "";

    /**
     *  Configuration and settings
     **/
    settings = new QSettings( "vlc", "vlc-qt-interface" );
    settings->beginGroup( "MainWindow" );

    /* Main settings */
    setFocusPolicy( Qt::StrongFocus );
    setAcceptDrops(true);
    setWindowIcon( QApplication::windowIcon() );
    setWindowOpacity( config_GetFloat( p_intf, "qt-opacity" ) );

    /* Set The Video In emebedded Mode or not */
    videoEmbeddedFlag = false;
    if( config_GetInt( p_intf, "embedded-video" ) )
        videoEmbeddedFlag = true;

    alwaysVideoFlag = false;
    if( videoEmbeddedFlag && config_GetInt( p_intf, "qt-always-video" ) )
        alwaysVideoFlag = true;

    /* Set the other interface settings */
    playlistEmbeddedFlag = settings->value( "playlist-embedded", true).toBool();
    visualSelectorEnabled = settings->value( "visual-selector", false ).toBool();
    notificationEnabled = config_GetInt( p_intf, "qt-notification" )
                          ? true : false;
    /**************************
     *  UI and Widgets design
     **************************/
    setVLCWindowsTitle();
    handleMainUi( settings );

    /* Menu Bar */
    QVLCMenu::createMenuBar( this, p_intf, playlistEmbeddedFlag,
                             visualSelectorEnabled );

    /* Status Bar */
    /**
     * TODO: clicking on the elapsed time should switch to the remaining time
     **/
    /**
     * TODO: do we add a label for the current Volume ?
     **/
    b_remainingTime = false;
    timeLabel = new TimeLabel;
    nameLabel = new QLabel;
    speedLabel = new QLabel( "1.00x" );
    timeLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    speedLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    statusBar()->addWidget( nameLabel, 8 );
    statusBar()->addPermanentWidget( speedLabel, 0 );
    statusBar()->addPermanentWidget( timeLabel, 2 );
    speedLabel->setContextMenuPolicy ( Qt::CustomContextMenu );
    timeLabel->setContextMenuPolicy ( Qt::CustomContextMenu );
    CONNECT( timeLabel, timeLabelClicked(), this, toggleTimeDisplay() );
    CONNECT( speedLabel, customContextMenuRequested( QPoint ),
             this, showSpeedMenu( QPoint ) );
    CONNECT( timeLabel, customContextMenuRequested( QPoint ),
             this, showTimeMenu( QPoint ) );

    /**********************
     * Systray Management *
     **********************/
    sysTray = NULL;
    bool b_createSystray = false;
    bool b_systrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    if( config_GetInt( p_intf, "qt-start-minimized") )
    {
        if( b_systrayAvailable ){
            b_createSystray = true;
            hide(); //FIXME
        }
        else msg_Warn( p_intf, "You can't minize if you haven't a system "
                "tray bar" );
    }
    if( config_GetInt( p_intf, "qt-system-tray") )
        b_createSystray = true;

    if( b_systrayAvailable && b_createSystray )
            createSystray();

    /* Init input manager */
    MainInputManager::getInstance( p_intf );
    ON_TIMEOUT( updateOnTimer() );

    /**
     * Various CONNECTs
     **/

    /* Connect the input manager to the GUI elements it manages */
    /* It is also connected to the control->slider, see the ControlsWidget */
    CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
             this, setDisplayPosition( float, int, int ) );

    CONNECT( THEMIM->getIM(), rateChanged( int ), this, setRate( int ) );

    /** Connects on nameChanged() */
    /* Naming in the controller statusbar */
    CONNECT( THEMIM->getIM(), nameChanged( QString ), this,
             setName( QString ) );
    /* and in the systray */
    if( sysTray )
    {
        CONNECT( THEMIM->getIM(), nameChanged( QString ), this,
                 updateSystrayTooltipName( QString ) );
    }
    /* and in the title of the controller */
    if( config_GetInt( p_intf, "qt-name-in-title" ) )
    {
        CONNECT( THEMIM->getIM(), nameChanged( QString ), this,
             setVLCWindowsTitle( QString ) );
    }

    /** CONNECTS on PLAY_STATUS **/
    /* Status on the main controller */
    CONNECT( THEMIM->getIM(), statusChanged( int ), this, setStatus( int ) );
    /* and in the systray */
    if( sysTray )
    {
        CONNECT( THEMIM->getIM(), statusChanged( int ), this,
                 updateSystrayTooltipStatus( int ) );
    }

    /**
     * Callbacks
     **/
    var_Create( p_intf, "interaction", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "interaction", InteractCallback, this );
    p_intf->b_interaction = VLC_TRUE;

    /* Register callback for the intf-popupmenu variable */
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist != NULL )
    {
        var_AddCallback( p_playlist, "intf-popupmenu", PopupMenuCB, p_intf );
        var_AddCallback( p_playlist, "intf-show", IntfShowCB, p_intf );
        vlc_object_release( p_playlist );
    }
}

MainInterface::~MainInterface()
{
    /* Unregister callback for the intf-popupmenu variable */
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_playlist != NULL )
    {
        var_DelCallback( p_playlist, "intf-popupmenu", PopupMenuCB, p_intf );
        var_DelCallback( p_playlist, "intf-show", IntfShowCB, p_intf );
        vlc_object_release( p_playlist );
    }

    settings->setValue( "playlist-embedded", playlistEmbeddedFlag );
    settings->setValue( "adv-controls", getControlsVisibilityStatus() & 0x1 );
    settings->setValue( "pos", pos() );
    settings->endGroup();
    delete settings;
    p_intf->b_interaction = VLC_FALSE;
    var_DelCallback( p_intf, "interaction", InteractCallback, this );

    p_intf->pf_request_window = NULL;
    p_intf->pf_release_window = NULL;
    p_intf->pf_control_window = NULL;
}

/*****************************
 *   Main UI handling        *
 *****************************/

/**
 * Give the decorations of the Main Window a correct Name.
 * If nothing is given, set it to VLC...
 **/
void MainInterface::setVLCWindowsTitle( QString aTitle )
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


void MainInterface::handleMainUi( QSettings *settings )
{
    /* Create the main Widget and the mainLayout */
    QWidget *main = new QWidget( this );
    mainLayout = new QVBoxLayout( main );
    setCentralWidget( main );

    /* Margins, spacing */
    main->setContentsMargins( 0, 0, 0, 0 );
    mainLayout->setMargin( 0 );

    /* Create the CONTROLS Widget */
    controls = new ControlsWidget( p_intf,
                   settings->value( "adv-controls", false ).toBool() );

    /* Configure the Controls */
    BUTTON_SET_IMG( controls->playlistButton, "" , playlist_icon.png,
                    playlistEmbeddedFlag ?  qtr( "Show playlist" ) :
                                            qtr( "Open playlist" ) );
    BUTTONACT( controls->playlistButton, togglePlaylist() );

    /* Add the controls Widget to the main Widget */
    mainLayout->addWidget( controls );

    /* Create the Speed Control Widget */
    speedControl = new SpeedControlWidget( p_intf );
    speedControlMenu = new QMenu( this );
    QWidgetAction *widgetAction = new QWidgetAction( this );
    widgetAction->setDefaultWidget( speedControl );
    speedControlMenu->addAction( widgetAction );

    /* Set initial size */
    resize( PREF_W, PREF_H );
    addSize = QSize( mainLayout->margin() * 2, PREF_H );

    /* Visualisation */
    visualSelector = new VisualSelector( p_intf );
    mainLayout->insertWidget( 0, visualSelector );
    visualSelector->hide();

    /* And video Outputs */
    if( alwaysVideoFlag )
    {
        bgWidget = new BackgroundWidget( p_intf );
        bgWidget->widgetSize = settings->value( "backgroundSize",
                                           QSize( 300, 300 ) ).toSize();
        bgWidget->resize( bgWidget->widgetSize );
        bgWidget->updateGeometry();
        mainLayout->insertWidget( 0, bgWidget );
    }

    if( videoEmbeddedFlag )
    {
        videoWidget = new VideoWidget( p_intf );
        videoWidget->widgetSize = QSize( 1, 1 );
        //videoWidget->resize( videoWidget->widgetSize );
        mainLayout->insertWidget( 0, videoWidget );

        p_intf->pf_request_window  = ::DoRequest;
        p_intf->pf_release_window  = ::DoRelease;
        p_intf->pf_control_window  = ::DoControl;
    }

    /* Finish the sizing */
    setMinimumSize( PREF_W, addSize.height() );
}

/**********************************************************************
 * Handling of sizing of the components
 **********************************************************************/
void MainInterface::calculateInterfaceSize()
{
    int width = 0, height = 0;
    if( VISIBLE( bgWidget ) )
    {
        width = bgWidget->widgetSize.width();
        height = bgWidget->widgetSize.height();
        assert( !(playlistWidget && playlistWidget->isVisible() ) );
    }
    else if( VISIBLE( playlistWidget ) )
    {
        width = playlistWidget->widgetSize.width();
        height = playlistWidget->widgetSize.height();
    }
    else if( videoIsActive )
    {
        width =  videoWidget->widgetSize.width() ;
        height = videoWidget->widgetSize.height();
    }
    else
    {
        width = PREF_W - addSize.width();
        height = PREF_H - addSize.height();
    }
    if( VISIBLE( visualSelector ) )
        height += visualSelector->height();
/*    if( VISIBLE( advControls) )
    {
        height += advControls->sizeHint().height();
    }*/
    mainSize = QSize( width + addSize.width(), height + addSize.height() );
}

void MainInterface::resizeEvent( QResizeEvent *e )
{
    videoWidget->widgetSize.setWidth(  e->size().width() - addSize.width() );
    if( videoWidget && videoIsActive && videoWidget->widgetSize.height() > 1 )
    {
        SET_WH( videoWidget, e->size().width() - addSize.width(),
                             e->size().height()  - addSize.height() );
        videoWidget->updateGeometry();
    }
    if( VISIBLE( playlistWidget ) )
    {
        SET_WH( playlistWidget , e->size().width() - addSize.width(),
                                 e->size().height() - addSize.height() );
        playlistWidget->updateGeometry();
    }
}

/****************************************************************************
 * Small right-click menus
 ****************************************************************************/
void MainInterface::showSpeedMenu( QPoint pos )
{
    speedControlMenu->exec( QCursor::pos() - pos + QPoint( 0, speedLabel->height() ) );
}

void MainInterface::showTimeMenu( QPoint pos )
{
    QMenu menu( this );
    menu.addAction(  qtr("Elapsed Time") , this, SLOT( setElapsedTime() ) );
    menu.addAction(  qtr("Remaining Time") , this, SLOT( setRemainTime() ) );
    menu.exec( QCursor::pos() - pos +QPoint( 0, timeLabel->height() ) );
}

/****************************************************************************
 * Video Handling
 ****************************************************************************/
void *MainInterface::requestVideo( vout_thread_t *p_nvout, int *pi_x,
                                   int *pi_y, unsigned int *pi_width,
                                   unsigned int *pi_height )
{
    void *ret = videoWidget->request( p_nvout,pi_x, pi_y, pi_width, pi_height );
    if( ret )
    {
        videoIsActive = true;
        if( VISIBLE( playlistWidget ) )
        {
            embeddedPlaylistWasActive = true;
//            playlistWidget->hide();
        }
        bool bgWasVisible = false;
        if( VISIBLE( bgWidget) )
        {
            bgWasVisible = true;
            bgWidget->hide();
        }
        if( THEMIM->getIM()->hasVideo() || !bgWasVisible )
        {
            videoWidget->widgetSize = QSize( *pi_width, *pi_height );
        }
        else /* Background widget available, use its size */
        {
            /* Ok, our visualizations are bad, so don't do this for the moment
             * use the requested size anyway */
            // videoWidget->widgetSize = bgWidget->widgeTSize;
            videoWidget->widgetSize = QSize( *pi_width, *pi_height );
        }
        videoWidget->updateGeometry(); // Needed for deinterlace
        need_components_update = true;
    }
    return ret;
}

void MainInterface::releaseVideo( void *p_win )
{
    videoWidget->release( p_win );
    videoWidget->widgetSize = QSize( 0, 0 );
    videoWidget->resize( videoWidget->widgetSize );

    if( embeddedPlaylistWasActive )
        playlistWidget->show();
    else if( bgWidget )
        bgWidget->show();

    videoIsActive = false;
    need_components_update = true;
}

class SetVideoOnTopQtEvent : public QEvent
{
public:
    SetVideoOnTopQtEvent( bool _onTop ) :
      QEvent( (QEvent::Type)SetVideoOnTopEvent_Type ), onTop( _onTop)
    {
    }

    bool OnTop() const
    {
        return onTop;
    }

private:
    bool onTop;
};

int MainInterface::controlVideo( void *p_window, int i_query, va_list args )
{
    int i_ret = VLC_EGENERIC;
    switch( i_query )
    {
        case VOUT_GET_SIZE:
        {
            unsigned int *pi_width  = va_arg( args, unsigned int * );
            unsigned int *pi_height = va_arg( args, unsigned int * );
            *pi_width = videoWidget->widgetSize.width();
            *pi_height = videoWidget->widgetSize.height();
            i_ret = VLC_SUCCESS;
            break;
        }
        case VOUT_SET_SIZE:
        {
            unsigned int i_width  = va_arg( args, unsigned int );
            unsigned int i_height = va_arg( args, unsigned int );
            videoWidget->widgetSize = QSize( i_width, i_height );
            // videoWidget->updateGeometry();
            need_components_update = true;
            i_ret = VLC_SUCCESS;
            break;
        }
        case VOUT_SET_STAY_ON_TOP:
        {
            int i_arg = va_arg( args, int );
            QApplication::postEvent( this, new SetVideoOnTopQtEvent( i_arg ) );
            i_ret = VLC_SUCCESS;
            break;
        }
        default:
            msg_Warn( p_intf, "unsupported control query" );
            break;
    }
    return i_ret;
}

/*****************************************************************************
 * Playlist, Visualisation and Menus handling
 *****************************************************************************/
/**
 * Toggle the playlist widget or dialog
 **/
void MainInterface::togglePlaylist()
{
    // Toggle the playlist dialog if not embedded and return
    if( !playlistEmbeddedFlag )
    {
        if( playlistWidget )
        {
            /// \todo Destroy it
        }
        THEDP->playlistDialog();
        return;
    }

    // Create the playlist Widget and destroy the existing dialog
    if( !playlistWidget )
    {
        PlaylistDialog::killInstance();
        playlistWidget = new PlaylistWidget( p_intf );
        mainLayout->insertWidget( 0, playlistWidget );
        playlistWidget->widgetSize = settings->value( "playlistSize",
                                               QSize( 650, 310 ) ).toSize();
        playlistWidget->hide();
        if(bgWidget)
        CONNECT( playlistWidget, artSet( QString ), bgWidget, setArt(QString) );
    }

    // And toggle visibility
    if( VISIBLE( playlistWidget ) )
    {
        playlistWidget->hide();
        if( bgWidget ) bgWidget->show();
        if( videoIsActive )
        {
            videoWidget->widgetSize = savedVideoSize;
            videoWidget->resize( videoWidget->widgetSize );
            videoWidget->updateGeometry();
            if( bgWidget ) bgWidget->hide();
        }
    }
    else
    {
        playlistWidget->show();
        if( videoIsActive )
        {
            savedVideoSize = videoWidget->widgetSize;
            videoWidget->widgetSize.setHeight( 0 );
            videoWidget->resize( videoWidget->widgetSize );
            videoWidget->updateGeometry();
        }
        if( VISIBLE( bgWidget ) ) bgWidget->hide();
    }

    doComponentsUpdate();
}

void MainInterface::undockPlaylist()
{
    if( playlistWidget )
    {
        playlistWidget->hide();
        playlistWidget->deleteLater();
        mainLayout->removeWidget( playlistWidget );
        playlistWidget = NULL;
        playlistEmbeddedFlag = false;

        menuBar()->clear();
        QVLCMenu::createMenuBar( this, p_intf, false, visualSelectorEnabled);

        if( videoIsActive )
        {
            videoWidget->widgetSize = savedVideoSize;
            videoWidget->resize( videoWidget->widgetSize );
            videoWidget->updateGeometry();
        }

        doComponentsUpdate();
        THEDP->playlistDialog();
    }
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
    doComponentsUpdate();
}
#endif

void MainInterface::toggleMenus()
{
    msg_Dbg( p_intf, "I HAS HERE, HIDING YOUR MENUZ: \\_o<~~ coin coin" );
    TOGGLEV( controls );
    TOGGLEV( statusBar() );
    updateGeometry();
}

/* Video widget cannot do this synchronously as it runs in another thread */
/* Well, could it, actually ? Probably dangerous ... */
void MainInterface::doComponentsUpdate()
{
    calculateInterfaceSize();
    resize( mainSize );
}

void MainInterface::toggleAdvanced()
{
    controls->toggleAdvanced();
}

int MainInterface::getControlsVisibilityStatus()
{
    return( (controls->isVisible() ? 0x2 : 0x0 )
                + controls->b_advancedVisible );
}

/************************************************************************
 * Other stuff
 ************************************************************************/
void MainInterface::setDisplayPosition( float pos, int time, int length )
{
    char psz_length[MSTRTIME_MAX_SIZE], psz_time[MSTRTIME_MAX_SIZE];
    secstotimestr( psz_length, length );
    secstotimestr( psz_time, b_remainingTime ? length - time : time );
    QString title; title.sprintf( "%s/%s", psz_time, psz_length );
    if( b_remainingTime ) timeLabel->setText( " -"+title+" " );
    else timeLabel->setText( " "+title+" " );
}

void MainInterface::toggleTimeDisplay()
{
    b_remainingTime = ( b_remainingTime ? false : true );
}

void MainInterface::setElapsedTime(){ b_remainingTime = false; }
void MainInterface::setRemainTime(){ b_remainingTime = true; }

void MainInterface::setName( QString name )
{
    input_name = name;
    nameLabel->setText( " " + name+" " );
}

void MainInterface::setStatus( int status )
{
    controls->setStatus( status );
    if( sysTray )
        updateSystrayMenu( status );
}

void MainInterface::setRate( int rate )
{
    QString str;
    str.setNum( ( 1000 / (double)rate), 'f', 2 );
    str.append( "x" );
    speedLabel->setText( str );
    speedControl->updateControls( rate );
}

void MainInterface::updateOnTimer()
{
    /* \todo Make this event-driven */
    if( intf_ShouldDie( p_intf ) )
    {
        QApplication::closeAllWindows();
        QApplication::quit();
    }
    if( need_components_update )
    {
        doComponentsUpdate();
        need_components_update = false;
    }

    controls->updateOnTimer();
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
    QIcon iconVLC =  QIcon( QPixmap( ":/vlc128.png" ) );
    sysTray = new QSystemTrayIcon( iconVLC, this );
    sysTray->setToolTip( qtr( "VLC media player" ));

    systrayMenu = new QMenu( qtr( "VLC media player" ), this );
    systrayMenu->setIcon( iconVLC );

    QVLCMenu::updateSystrayMenu( this, p_intf, true );
    sysTray->show();

    CONNECT( sysTray, activated( QSystemTrayIcon::ActivationReason ),
            this, handleSystrayClick( QSystemTrayIcon::ActivationReason ) );
}

/**
 * Update the menu of the Systray Icon.
 * May be unneedded, since it just calls QVLCMenu::update
 * FIXME !!!
 **/
void MainInterface::updateSystrayMenu( int status )
{
    QVLCMenu::updateSystrayMenu( this, p_intf ) ;
}

/**
 * Updates the Systray Icon's menu and toggle the main interface
 */
void MainInterface::toggleUpdateSystrayMenu()
{
    if( isHidden() )
    {
        show();
        activateWindow();
    }
    else if( isMinimized() )
    {
        showNormal();
        activateWindow();
    }
    else
    {
#ifdef WIN32
        /* check if any visible window is above vlc in the z-order,
         * but ignore the ones always on top */
        WINDOWINFO wi;
        HWND hwnd;
        wi.cbSize = sizeof( WINDOWINFO );
        for( hwnd = GetNextWindow( internalWinId(), GW_HWNDPREV );
                hwnd && !IsWindowVisible( hwnd );
                hwnd = GetNextWindow( hwnd, GW_HWNDPREV ) );
        if( !hwnd || !GetWindowInfo( hwnd, &wi ) ||
                (wi.dwExStyle&WS_EX_TOPMOST) )
#else
        if( isActiveWindow() )
#endif
        {
            hide();
        }
        else
        {
            activateWindow();
        }
    }
    QVLCMenu::updateSystrayMenu( this, p_intf );
}

void MainInterface::handleSystrayClick(
                                    QSystemTrayIcon::ActivationReason reason )
{
    switch( reason )
    {
        case QSystemTrayIcon::Trigger:
            toggleUpdateSystrayMenu();
            break;
        case QSystemTrayIcon::MiddleClick:
            sysTray->showMessage( qtr( "VLC media player" ),
                    qtr( "Control menu for the player" ),
                    QSystemTrayIcon::Information, 4000 );
            break;
    }
}

/**
 * Updates the name of the systray Icon tooltip.
 * Doesn't check if the systray exists, check before you call it.
 * FIXME !!! Fusion with next function ?
 **/
void MainInterface::updateSystrayTooltipName( QString name )
{
    if( name.isEmpty() )
    {
        sysTray->setToolTip( qtr( "VLC media player" ) );
    }
    else
    {
        sysTray->setToolTip( name );
        if( notificationEnabled && ( isHidden() || isMinimized() ) )
        {
            sysTray->showMessage( qtr( "VLC media player" ), name,
                    QSystemTrayIcon::NoIcon, 4000 );
        }
    }
}

/**
 * Updates the status of the systray Icon tooltip.
 * Doesn't check if the systray exists, check before you call it.
 **/
void MainInterface::updateSystrayTooltipStatus( int i_status )
{
    switch( i_status )
    {
        case  0:
            {
                sysTray->setToolTip( qtr( "VLC media player" ) );
                break;
            }
        case PLAYING_S:
            {
                sysTray->setToolTip( input_name );
                //+ " - " + qtr( "Playing" ) );
                break;
            }
        case PAUSE_S:
            {
                sysTray->setToolTip( input_name + " - "
                        + qtr( "Paused") );
                break;
            }
    }
}

/************************************************************************
 * D&D Events
 ************************************************************************/
void MainInterface::dropEvent(QDropEvent *event)
{
     const QMimeData *mimeData = event->mimeData();

     /* D&D of a subtitles file, add it on the fly */
     if( mimeData->urls().size() == 1 )
     {
        if( THEMIM->getIM()->hasInput() )
        {
            if( input_AddSubtitles( THEMIM->getInput(),
                                    qtu( mimeData->urls()[0].toString() ),
                                    VLC_TRUE ) )
            {
                event->acceptProposedAction();
                return;
            }
        }
     }
     bool first = true;
     foreach( QUrl url, mimeData->urls() ) {
        QString s = url.toString();
        if( s.length() > 0 ) {
            playlist_Add( THEPL, qtu(s), NULL,
                          PLAYLIST_APPEND | (first ? PLAYLIST_GO:0),
                          PLAYLIST_END, VLC_TRUE, VLC_FALSE );
            first = false;
        }
     }
     event->acceptProposedAction();
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
void MainInterface::customEvent( QEvent *event )
{
    if( event->type() == PLDockEvent_Type )
    {
        PlaylistDialog::killInstance();
        playlistEmbeddedFlag = true;
        menuBar()->clear();
        QVLCMenu::createMenuBar(this, p_intf, true, visualSelectorEnabled);
        togglePlaylist();
    }
    else if ( event->type() == SetVideoOnTopEvent_Type )
    {
        SetVideoOnTopQtEvent* p_event = (SetVideoOnTopQtEvent*)event;
        if( p_event->OnTop() )
            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        else
            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        show(); /* necessary to apply window flags?? */
    }
}

void MainInterface::keyPressEvent( QKeyEvent *e )
{
    int i_vlck = qtEventToVLCKey( e );
    if( i_vlck >= 0 )
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
    hide();
    vlc_object_kill( p_intf );
}

/*****************************************************************************
 * Callbacks
 *****************************************************************************/
static int InteractCallback( vlc_object_t *p_this,
                             const char *psz_var, vlc_value_t old_val,
                             vlc_value_t new_val, void *param )
{
    intf_dialog_args_t *p_arg = new intf_dialog_args_t;
    p_arg->p_dialog = (interaction_dialog_t *)(new_val.p_address);
    DialogEvent *event = new DialogEvent( INTF_DIALOG_INTERACTION, 0, p_arg );
    QApplication::postEvent( THEDP, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * PopupMenuCB: callback triggered by the intf-popupmenu playlist variable.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;

    if( p_intf->pf_show_dialog )
    {
        p_intf->pf_show_dialog( p_intf, INTF_DIALOG_POPUPMENU,
                                new_val.b_bool, 0 );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * IntfShowCB: callback triggered by the intf-show playlist variable.
 *****************************************************************************/
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    //p_intf->p_sys->b_intf_show = VLC_TRUE;

    return VLC_SUCCESS;
}
