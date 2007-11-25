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
#include <QDockWidget>
#include <QToolBar>
#include <QGroupBox>

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
     * Ask for the network policy on FIRST STARTUP
     */
    if( config_GetInt( p_intf, "privacy-ask") )
    {
        QList<ConfigControl *> controls;
        privacyDialog( controls );

        QList<ConfigControl *>::Iterator i;
        for(  i = controls.begin() ; i != controls.end() ; i++ )
        {
            ConfigControl *c = qobject_cast<ConfigControl *>(*i);
            c->doApply( p_intf );
        }

        config_PutInt( p_intf,  "privacy-ask" , 0 );
        config_SaveConfigFile( p_intf, NULL );
    }

    /**
     *  Configuration and settings
     **/
    settings = new QSettings( "vlc", "vlc-qt-interface" );
    settings->beginGroup( "MainWindow" );

    /* Main settings */
    setFocusPolicy( Qt::StrongFocus );
    setAcceptDrops( true );
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
    visualSelectorEnabled = settings->value( "visual-selector", false ).toBool();
    notificationEnabled = config_GetInt( p_intf, "qt-notification" )
                          ? true : false;

    /**************************
     *  UI and Widgets design
     **************************/
    setVLCWindowsTitle();
    handleMainUi( settings );

    /* Create a Dock to get the playlist */
    dockPL = new QDockWidget( qtr("Playlist"), this );
    dockPL->setAllowedAreas( Qt::LeftDockWidgetArea
                           | Qt::RightDockWidgetArea
                           | Qt::BottomDockWidgetArea );
    dockPL->setFeatures( QDockWidget::AllDockWidgetFeatures );
    dockPL->setWidget( playlistWidget );

    /* Menu Bar */
    QVLCMenu::createMenuBar( this, p_intf, visualSelectorEnabled );

    /****************
     *  Status Bar  *
     ****************/

    /* Widgets Creation*/
    b_remainingTime = false;
    timeLabel = new TimeLabel;
    nameLabel = new QLabel;
    nameLabel->setTextInteractionFlags( Qt::TextSelectableByMouse
                                      | Qt::TextSelectableByKeyboard );
    speedLabel = new QLabel( "1.00x" );
    speedLabel->setContextMenuPolicy ( Qt::CustomContextMenu );

    /* Styling those labels */
    timeLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    speedLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    nameLabel->setFrameStyle( QFrame::Sunken | QFrame::StyledPanel);

    /* and adding those */
    statusBar()->addWidget( nameLabel, 8 );
    statusBar()->addPermanentWidget( speedLabel, 0 );
    statusBar()->addPermanentWidget( timeLabel, 2 );

    /* timeLabel behaviour:
       - double clicking opens the goto time dialog 
       - right-clicking and clicking just toggle between remaining and
         elapsed time.*/
    CONNECT( timeLabel, timeLabelClicked(), this, toggleTimeDisplay() );
    CONNECT( timeLabel, timeLabelDoubleClicked(), THEDP, gotoTimeDialog() );
    CONNECT( timeLabel, timeLabelDoubleClicked(), this, toggleTimeDisplay() );

    /* Speed Label behaviour:
       - right click gives the vertical speed slider */
    CONNECT( speedLabel, customContextMenuRequested( QPoint ),
             this, showSpeedMenu( QPoint ) );

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
            hide(); //FIXME BUG HERE
        }
        else msg_Warn( p_intf, "You can't minize if you haven't a system "
                "tray bar" );
    }
    if( config_GetInt( p_intf, "qt-system-tray") )
        b_createSystray = true;

    if( b_systrayAvailable && b_createSystray )
            createSystray();

    if( config_GetInt( p_intf, "qt-minimal-view" ) )
        toggleMinimalView();

    /* Init input manager */
    MainInputManager::getInstance( p_intf );
    ON_TIMEOUT( updateOnTimer() );
    //ON_TIMEOUT( debug() );

    /********************
     * Various CONNECTs *
     ********************/

    /* Connect the input manager to the GUI elements it manages */
    /* It is also connected to the control->slider, see the ControlsWidget */
    CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
             this, setDisplayPosition( float, int, int ) );

    CONNECT( THEMIM->getIM(), rateChanged( int ), this, setRate( int ) );

    /**
     * Connects on nameChanged()
     */
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

    CONNECT( this, askReleaseVideo( void * ), this, releaseVideoSlot( void * ) );

    CONNECT( dockPL, topLevelChanged( bool ), this, doComponentsUpdate() );
    // DEBUG FIXME
    hide();
    updateGeometry();
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

    settings->setValue( "playlist-floats", dockPL->isFloating() );
    settings->setValue( "adv-controls", getControlsVisibilityStatus() & CONTROLS_ADVANCED );
    settings->setValue( "pos", pos() );
    if( playlistWidget )
        playlistWidget->saveSettings( settings );
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
    bool b_shiny = config_GetInt( p_intf, "qt-blingbling" );
    controls = new ControlsWidget( p_intf,
                   settings->value( "adv-controls", false ).toBool(),
                   b_shiny );

    /* Configure the Controls, the playlist button doesn't trigger THEDP
       but the toggle from this MainInterface */
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
        CONNECT( this, askBgWidgetToToggle(), bgWidget, toggle() );
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
    setMinimumSize( PREF_W, PREF_H );
    updateGeometry();
}


void MainInterface::privacyDialog( QList<ConfigControl *> controls )
{
    QDialog *privacy = new QDialog( this );

    privacy->setWindowTitle( qtr( "Privacy and Network policies" ) );

    QGridLayout *gLayout = new QGridLayout( privacy );

    QGroupBox *blabla = new QGroupBox( qtr( "Privacy and Network Warning" ) );
    QGridLayout *blablaLayout = new QGridLayout( blabla );
    QLabel *text = new QLabel( qtr(
        "<p>The <i>VideoLAN Team</i> doesn't like when an application goes "
        "online without authorisation.</p>\n "
        "<p><i>VLC media player</i> can request limited information on "
        "Internet, espically to get CD Covers and songs metadata or to know "
        "if updates are available.</p>\n"
        "<p><i>VLC media player</i> <b>DOES NOT</b> send or collect <b>ANY</b> "
        "information, even anonymously about your "
        "usage.</p>\n"
        "<p>Therefore please check the following options, the default being "
        "almost no access on the web.</p>\n") );
    text->setWordWrap( true );
    text->setTextFormat( Qt::RichText );

    blablaLayout->addWidget( text, 0, 0 ) ;

    QGroupBox *options = new QGroupBox;
    QGridLayout *optionsLayout = new QGridLayout( options );

    gLayout->addWidget( blabla, 0, 0, 1, 3 );
    gLayout->addWidget( options, 1, 0, 1, 3 );
    module_config_t *p_config;
    ConfigControl *control;
    int line = 0;

#define CONFIG_GENERIC( option, type )                            \
    p_config =  config_FindConfig( VLC_OBJECT(p_intf), option );  \
    if( p_config )                                                \
    {                                                             \
        control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                p_config, options, false, optionsLayout, line );  \
        controls.append( control );                               \
    }

#define CONFIG_GENERIC_NOBOOL( option, type )                     \
    p_config =  config_FindConfig( VLC_OBJECT(p_intf), option );  \
    if( p_config )                                                \
    {                                                             \
        control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                p_config, options, optionsLayout, line );  \
        controls.append( control );                               \
    }

    CONFIG_GENERIC( "album-art", IntegerList ); line++;
    CONFIG_GENERIC_NOBOOL( "fetch-meta", Bool ); line++;
    CONFIG_GENERIC_NOBOOL( "qt-updates-notif", Bool );

    QPushButton *ok = new QPushButton( qtr( "Ok" ) );

    gLayout->addWidget( ok, 2, 2 );

    CONNECT( ok, clicked(), privacy, accept() );
    privacy->exec();
}

void MainInterface::debug()
{
    msg_Dbg( p_intf, "size: %i - %i", controls->size().height(), controls->size().width() );
    msg_Dbg( p_intf, "sizeHint: %i - %i", controls->sizeHint().height(), controls->sizeHint().width() );
}
/**********************************************************************
 * Handling of sizing of the components
 **********************************************************************/
QSize MainInterface::sizeHint() const
{
    msg_Dbg( p_intf, "Annoying debug to remove, main sizeHint was called %i %i ",
    menuBar()->size().height(), menuBar()->size().width() );

    QSize tempSize = controls->sizeHint() +
        QSize( 100, menuBar()->size().height() + statusBar()->size().height() );

    if( VISIBLE( bgWidget ) )
        tempSize += bgWidget->sizeHint();
    else if( videoIsActive )
        tempSize += videoWidget->size();

    if( !dockPL->isFloating() && dockPL->widget() )
        tempSize += dockPL->widget()->size();

    return tempSize;
}

#if 0
void MainInterface::calculateInterfaceSize()
{
    int width = 0, height = 0;
    if( VISIBLE( bgWidget ) )
    {
        width  = bgWidget->widgetSize.width();
        height = bgWidget->widgetSize.height();
    }
    else if( videoIsActive )
    {
        width  = videoWidget->widgetSize.width() ;
        height = videoWidget->widgetSize.height();
    }
    else
    {
        width  = PREF_W - addSize.width();
        height = PREF_H - addSize.height();
    }
    if( !dockPL->isFloating() && dockPL->widget() )
    {
        width  += dockPL->widget()->width();
        height += dockPL->widget()->height();
    }
    if( VISIBLE( visualSelector ) )
        height += visualSelector->height();
    mainSize = QSize( width + addSize.width(), height + addSize.height() );
}

void MainInterface::resizeEvent( QResizeEvent *e )
{
    if( videoWidget )
        videoWidget->widgetSize.setWidth( e->size().width() - addSize.width() );
    if( videoWidget && videoIsActive && videoWidget->widgetSize.height() > 1 )
    {
        SET_WH( videoWidget, e->size().width() - addSize.width(),
                             e->size().height()  - addSize.height() );
        videoWidget->updateGeometry();
    }
    if( VISIBLE( playlistWidget ) )
    {
        //FIXME
//        SET_WH( playlistWidget , e->size().width() - addSize.width(),
              //                   e->size().height() - addSize.height() );
        playlistWidget->updateGeometry();
    }
}
#endif

/****************************************************************************
 * Small right-click menu for rate control
 ****************************************************************************/
void MainInterface::showSpeedMenu( QPoint pos )
{
    speedControlMenu->exec( QCursor::pos() - pos
            + QPoint( 0, speedLabel->height() ) );
}

/****************************************************************************
 * Video Handling
 ****************************************************************************/
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


void *MainInterface::requestVideo( vout_thread_t *p_nvout, int *pi_x,
                                   int *pi_y, unsigned int *pi_width,
                                   unsigned int *pi_height )
{
    void *ret = videoWidget->request( p_nvout,pi_x, pi_y, pi_width, pi_height );
    if( ret )
    {
        videoIsActive = true;
        bool bgWasVisible = false;

        /* Did we have a bg */
        if( VISIBLE( bgWidget) )
        {
            bgWasVisible = true;
            emit askBgWidgetToToggle();
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
        updateGeometry();
    }
    return ret;
}

void MainInterface::releaseVideo( void *p_win )
{
    emit askReleaseVideo( p_win );
}

void MainInterface::releaseVideoSlot( void *p_win )
{
    videoWidget->release( p_win );
    videoWidget->hide();

    if( bgWidget )
        bgWidget->show();

    videoIsActive = false;
    updateGeometry();
}

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
            videoWidget->updateGeometry();
            updateGeometry();
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
    /* If no playlist exist, then create one and attach it to the DockPL*/
    if( !playlistWidget )
    {
        msg_Dbg( p_intf, "Creating a new playlist" );
        playlistWidget = new PlaylistWidget( p_intf, settings );
        if( bgWidget )
            CONNECT( playlistWidget, artSet( QString ),
                     bgWidget, setArt(QString) );

        //FIXME
/*        playlistWidget->widgetSize = settings->value( "playlistSize",
                                               QSize( 650, 310 ) ).toSize();*/
        /* Add it to the parent DockWidget */
        dockPL->setWidget( playlistWidget );

        /* Add the dock to the main Interface */
        addDockWidget( Qt::BottomDockWidgetArea, dockPL );

        msg_Dbg( p_intf, "Creating a new playlist" );

        /* Make the playlist floating is requested. Default is not. */
        if( settings->value( "playlist-floats", false ).toBool() );
        {
            msg_Dbg( p_intf, "we don't want it inside");
            dockPL->setFloating( true );
        }
    }
    else
    {
    /* toggle the display */
       TOGGLEV( dockPL );
       resize(sizeHint());
    }
#if 0
    doComponentsUpdate();
#endif
    updateGeometry();
}

void MainInterface::undockPlaylist()
{
    dockPL->setFloating( true );
    updateGeometry();
#if 0
    doComponentsUpdate();
#endif
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

void MainInterface::toggleMinimalView()
{
    TOGGLEV( menuBar() );
    TOGGLEV( controls );
    TOGGLEV( statusBar() );
    updateGeometry();
}

/* Video widget cannot do this synchronously as it runs in another thread */
/* Well, could it, actually ? Probably dangerous ... */
void MainInterface::doComponentsUpdate()
{
    msg_Dbg( p_intf, "trying" );

    resize( sizeHint() );
}


void MainInterface::toggleAdvanced()
{
    controls->toggleAdvanced();
}

int MainInterface::getControlsVisibilityStatus()
{
    return( (controls->isVisible() ? CONTROLS_VISIBLE : CONTROLS_HIDDEN )
                + CONTROLS_ADVANCED * controls->b_advancedVisible );
}

/************************************************************************
 * Other stuff
 ************************************************************************/
void MainInterface::setDisplayPosition( float pos, int time, int length )
{
    char psz_length[MSTRTIME_MAX_SIZE], psz_time[MSTRTIME_MAX_SIZE];
    secstotimestr( psz_length, length );
    secstotimestr( psz_time, ( b_remainingTime && length ) ? length - time
                                                           : time );

    QString title;
    title.sprintf( "%s/%s", psz_time,
                            ( !length && time ) ? "--:--" : psz_length );

    /* Add a minus to remaining time*/
    if( b_remainingTime && length ) timeLabel->setText( " -"+title+" " );
    else timeLabel->setText( " "+title+" " );
}

void MainInterface::toggleTimeDisplay()
{
    b_remainingTime = ( b_remainingTime ? false : true );
}

void MainInterface::setName( QString name )
{
    input_name = name; /* store it for the QSystray use */
    /* Display it in the status bar, but also as a Tooltip in case it doesn't
       fit in the label */
    nameLabel->setText( " " + name + " " );
    nameLabel->setToolTip( " " + name +" " );
}

void MainInterface::setStatus( int status )
{
    /* Forward the status to the controls to toggle Play/Pause */
    controls->setStatus( status );
    /* And in the systray for the menu */
    if( sysTray )
        QVLCMenu::updateSystrayMenu( this, p_intf );
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
#if 0
    if( need_components_update )
    {
        doComponentsUpdate();
        need_components_update = false;
    }
#endif

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
#if 0
    if( event->type() == PLDockEvent_Type )
    {
        PlaylistDialog::killInstance();
        playlistEmbeddedFlag = true;
        menuBar()->clear();
        QVLCMenu::createMenuBar(this, p_intf, true, visualSelectorEnabled);
        togglePlaylist();
    }
#endif
    /*else */
    if ( event->type() == SetVideoOnTopEvent_Type )
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
    if( ( e->modifiers() &  Qt::ControlModifier ) && ( e->key() & Qt::Key_H )
          && menuBar()->isHidden() )
    {
        toggleMinimalView();
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
