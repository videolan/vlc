/*****************************************************************************
 * main_interface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "util/qvlcframe.hpp"
#include "util/customwidgets.hpp"
#include "dialogs_provider.hpp"
#include "components/interface_widgets.hpp"
#include "components/playlist/playlist.hpp"
#include "dialogs/extended.hpp"
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
#include <QToolBar>
#include <QGroupBox>
#include <QDate>

#include <assert.h>
#include <vlc_keys.h>
#include <vlc_vout.h>

/* Callback prototypes */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );
static int InteractCallback( vlc_object_t *, const char *, vlc_value_t,
                             vlc_value_t, void *);

MainInterface::MainInterface( intf_thread_t *_p_intf ) : QVLCMW( _p_intf )
{
    /* Variables initialisation */
    // need_components_update = false;
    bgWidget             = NULL;
    videoWidget          = NULL;
    playlistWidget       = NULL;
    sysTray              = NULL;
    videoIsActive        = false;
    playlistVisible      = false;
    input_name           = "";
    fullscreenControls   = NULL;

    /* Ask for privacy */
    askForPrivacy();

    /**
     *  Configuration and settings
     *  Pre-building of interface
     **/
    /* Main settings */
    setFocusPolicy( Qt::StrongFocus );
    setAcceptDrops( true );
    setWindowIcon( QApplication::windowIcon() );
    setWindowOpacity( config_GetFloat( p_intf, "qt-opacity" ) );

    /* Set The Video In emebedded Mode or not */
    videoEmbeddedFlag = config_GetInt( p_intf, "embedded-video" );

    /* Are we in the enhanced always-video mode or not ? */
    i_visualmode = config_GetInt( p_intf, "qt-display-mode" );

    /* Set the other interface settings */
    settings = getSettings();
    settings->beginGroup( "MainWindow" );

    /* Visualisation, not really used yet */
    visualSelectorEnabled = settings->value( "visual-selector", false).toBool();

    /* Do we want anoying popups or not */
    notificationEnabled = (bool)config_GetInt( p_intf, "qt-notification" );

    /**************************
     *  UI and Widgets design
     **************************/
    setVLCWindowsTitle();
    handleMainUi( settings );

#if 0
    /* Create a Dock to get the playlist */
    dockPL = new QDockWidget( qtr( "Playlist" ), this );
    dockPL->setSizePolicy( QSizePolicy::Preferred,
                           QSizePolicy::Expanding );
    dockPL->setFeatures( QDockWidget::AllDockWidgetFeatures );
    dockPL->setAllowedAreas( Qt::LeftDockWidgetArea
                           | Qt::RightDockWidgetArea
                           | Qt::BottomDockWidgetArea );
    dockPL->hide();
#endif

    /**************************
     * Menu Bar and Status Bar
     **************************/
    QVLCMenu::createMenuBar( this, p_intf, visualSelectorEnabled );
    /* StatusBar Creation */
    createStatusBar();


    /********************
     * Input Manager    *
     ********************/
    MainInputManager::getInstance( p_intf );

    /**************************
     * Various CONNECTs on IM *
     **************************/
    /* Connect the input manager to the GUI elements it manages */

    /* It is also connected to the control->slider, see the ControlsWidget */
    CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
             this, setDisplayPosition( float, int, int ) );
    /* Change the SpeedRate in the Status */
    CONNECT( THEMIM->getIM(), rateChanged( int ), this, setRate( int ) );

    /**
     * Connects on nameChanged()
     * Those connects are not merged because different options can trigger
     * them down.
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

    /**
     * CONNECTS on PLAY_STATUS
     **/
    /* Status on the main controller */
    CONNECT( THEMIM->getIM(), statusChanged( int ), this, setStatus( int ) );
    /* and in the systray */
    if( sysTray )
    {
        CONNECT( THEMIM->getIM(), statusChanged( int ), this,
                 updateSystrayTooltipStatus( int ) );
    }

    /* END CONNECTS ON IM */


    /** OnTimeOut **/
    /* TODO Remove this function, but so far, there is no choice because there
       is no intf-should-die variable #1365 */
    ON_TIMEOUT( updateOnTimer() );

    /************
     * Callbacks
     ************/
    var_Create( p_intf, "interaction", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "interaction", InteractCallback, this );
    p_intf->b_interaction = true;

    var_AddCallback( p_intf->p_libvlc, "intf-show", IntfShowCB, p_intf );

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( p_intf->p_libvlc, "intf-popupmenu", PopupMenuCB, p_intf );


    /* VideoWidget connects to avoid different threads speaking to each other */
    CONNECT( this, askReleaseVideo( void * ),
             this, releaseVideoSlot( void * ) );
    if( videoWidget )
        CONNECT( this, askVideoToResize( unsigned int, unsigned int ),
                 videoWidget, SetSizing( unsigned int, unsigned int ) );

    CONNECT( this, askUpdate(), this, doComponentsUpdate() );

    /* Size and placement of interface */
    QVLCTools::restoreWidgetPosition( settings, this, QSize(380, 60) );


    /* Playlist */
    if( settings->value( "playlist-visible", 0 ).toInt() ) togglePlaylist();
    settings->endGroup();


    /* Final sizing and showing */
    setMinimumWidth( __MAX( controls->sizeHint().width(),
                            menuBar()->sizeHint().width() ) );
    show();

    /* And switch to minimal view if needed
       Must be called after the show() */
    if( i_visualmode == QT_MINIMAL_MODE )
        toggleMinimalView();

    /* Update the geometry TODO: is it useful ?*/
    updateGeometry();
    //    resize( sizeHint() );

    /*****************************************************
     * End everything by creating the Systray Management *
     *****************************************************/
    initSystray();
}

MainInterface::~MainInterface()
{
    msg_Dbg( p_intf, "Destroying the main interface" );

    if( playlistWidget )
        playlistWidget->savingSettings();

    settings->beginGroup( "MainWindow" );

    // settings->setValue( "playlist-floats", (int)(dockPL->isFloating()) );
    settings->setValue( "playlist-visible", (int)playlistVisible );
    settings->setValue( "adv-controls",
                        getControlsVisibilityStatus() & CONTROLS_ADVANCED );

    if( !videoIsActive )
    {
        QVLCTools::saveWidgetPosition(settings, this);
    }
    else
    {
        msg_Dbg( p_intf, "Not saving because video is in use." );
    }

    if( bgWidget )
        settings->setValue( "backgroundSize", bgWidget->size() );

    settings->endGroup();

    var_DelCallback( p_intf->p_libvlc, "intf-show", IntfShowCB, p_intf );

    /* Unregister callback for the intf-popupmenu variable */
    var_DelCallback( p_intf->p_libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    p_intf->b_interaction = false;
    var_DelCallback( p_intf, "interaction", InteractCallback, this );

    p_intf->p_sys->p_mi = NULL;
}

/*****************************
 *   Main UI handling        *
 *****************************/

inline void MainInterface::createStatusBar()
{
    /****************
     *  Status Bar  *
     ****************/
    /* Widgets Creation*/
    b_remainingTime = false;
    timeLabel = new TimeLabel;
    timeLabel->setText( " --:--/--:-- " );
    timeLabel->setAlignment( Qt::AlignRight | Qt::AlignVCenter );
    timeLabel->setToolTip( qtr( "Toggle between elapsed and remaining time" ) );
    nameLabel = new QLabel;
    nameLabel->setTextInteractionFlags( Qt::TextSelectableByMouse
                                      | Qt::TextSelectableByKeyboard );
    speedLabel = new SpeedLabel( p_intf, "1.00x" );
    speedLabel->setToolTip(
            qtr( "Current playback speed.\nRight click to adjust" ) );
    speedLabel->setContextMenuPolicy ( Qt::CustomContextMenu );

    /* Styling those labels */
    timeLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    speedLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    nameLabel->setFrameStyle( QFrame::Sunken | QFrame::StyledPanel);

    /* and adding those */
    statusBar()->addWidget( nameLabel, 8 );
    statusBar()->addPermanentWidget( speedLabel, 0 );
    statusBar()->addPermanentWidget( timeLabel, 0 );

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
}

inline void MainInterface::initSystray()
{
    bool b_createSystray = false;
    bool b_systrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    if( config_GetInt( p_intf, "qt-start-minimized") )
    {
        if( b_systrayAvailable )
        {
            b_createSystray = true;
            hide();
        }
        else msg_Err( p_intf, "You can't minimize if you haven't a system "
                "tray bar" );
    }
    if( config_GetInt( p_intf, "qt-system-tray") )
        b_createSystray = true;

    if( b_systrayAvailable && b_createSystray )
            createSystray();
}

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
    QWidget *main = new QWidget;
    setCentralWidget( main );
    mainLayout = new QVBoxLayout( main );

    /* Margins, spacing */
    main->setContentsMargins( 0, 0, 0, 0 );
    main->setSizePolicy( QSizePolicy::Preferred, QSizePolicy::Maximum );
    mainLayout->setSpacing( 0 );
    mainLayout->setMargin( 0 );

    /* Create the CONTROLS Widget */
    bool b_shiny = config_GetInt( p_intf, "qt-blingbling" );
    controls = new ControlsWidget( p_intf, this,
                   settings->value( "adv-controls", false ).toBool(),
                   b_shiny );
    CONNECT( controls, advancedControlsToggled( bool ),
             this, doComponentsUpdate() );

#ifdef WIN32
    if ( depth() > 8 )
#endif
    /* Create the FULLSCREEN CONTROLS Widget */
    if( config_GetInt( p_intf, "qt-fs-controller" ) )
    {
        fullscreenControls = new FullscreenControllerWidget( p_intf, this,
                settings->value( "adv-controls", false ).toBool(),
                b_shiny );
        CONNECT( fullscreenControls, advancedControlsToggled( bool ),
                this, doComponentsUpdate() );
    }

    /* Add the controls Widget to the main Widget */
    mainLayout->insertWidget( 0, controls, 0, Qt::AlignBottom );

    /* Create the Speed Control Widget */
    speedControl = new SpeedControlWidget( p_intf );
    speedControlMenu = new QMenu( this );

    QWidgetAction *widgetAction = new QWidgetAction( speedControl );
    widgetAction->setDefaultWidget( speedControl );
    speedControlMenu->addAction( widgetAction );

    /* Visualisation */
    /* Disabled for now, they SUCK */
    #if 0
    visualSelector = new VisualSelector( p_intf );
    mainLayout->insertWidget( 0, visualSelector );
    visualSelector->hide();
    #endif

    /* Bg Cone */
    bgWidget = new BackgroundWidget( p_intf );
    bgWidget->resize(
            settings->value( "backgroundSize", QSize( 300, 200 ) ).toSize() );
    bgWidget->updateGeometry();
    mainLayout->insertWidget( 0, bgWidget );
    CONNECT( this, askBgWidgetToToggle(), bgWidget, toggle() );

    if( i_visualmode != QT_ALWAYS_VIDEO_MODE &&
        i_visualmode != QT_MINIMAL_MODE )
    {
        bgWidget->hide();
    }

    /* And video Outputs */
    if( videoEmbeddedFlag )
    {
        videoWidget = new VideoWidget( p_intf );
        mainLayout->insertWidget( 0, videoWidget, 10 );
    }

    /* Finish the sizing */
    main->updateGeometry();
}

inline void MainInterface::askForPrivacy()
{
    /**
     * Ask for the network policy on FIRST STARTUP
     **/
    if( config_GetInt( p_intf, "qt-privacy-ask") )
    {
        QList<ConfigControl *> controls;
        if( privacyDialog( &controls ) == QDialog::Accepted )
        {
            QList<ConfigControl *>::Iterator i;
            for(  i = controls.begin() ; i != controls.end() ; i++ )
            {
                ConfigControl *c = qobject_cast<ConfigControl *>(*i);
                c->doApply( p_intf );
            }

            config_PutInt( p_intf,  "qt-privacy-ask" , 0 );
            /* We have to save here because the user may not launch Prefs */
            config_SaveConfigFile( p_intf, NULL );
        }
    }
}

int MainInterface::privacyDialog( QList<ConfigControl *> *controls )
{
    QDialog *privacy = new QDialog();

    privacy->setWindowTitle( qtr( "Privacy and Network Policies" ) );

    QGridLayout *gLayout = new QGridLayout( privacy );

    QGroupBox *blabla = new QGroupBox( qtr( "Privacy and Network Warning" ) );
    QGridLayout *blablaLayout = new QGridLayout( blabla );
    QLabel *text = new QLabel( qtr(
        "<p>The <i>VideoLAN Team</i> doesn't like when an application goes "
        "online without authorization.</p>\n "
        "<p><i>VLC media player</i> can request limited information on "
        "the Internet, especially to get CD covers or to know "
        "if updates are available.</p>\n"
        "<p><i>VLC media player</i> <b>DOES NOT</b> send or collect <b>ANY</b> "
        "information, even anonymously, about your usage.</p>\n"
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
        controls->append( control );                               \
    }

#define CONFIG_GENERIC_NOBOOL( option, type )                     \
    p_config =  config_FindConfig( VLC_OBJECT(p_intf), option );  \
    if( p_config )                                                \
    {                                                             \
        control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                p_config, options, optionsLayout, line );  \
        controls->append( control );                               \
    }

    CONFIG_GENERIC( "album-art", IntegerList ); line++;
#ifdef UPDATE_CHECK
    CONFIG_GENERIC_NOBOOL( "qt-updates-notif", Bool ); line++;
    CONFIG_GENERIC_NOBOOL( "qt-updates-days", Integer ); line++;
#endif

    QPushButton *ok = new QPushButton( qtr( "OK" ) );

    gLayout->addWidget( ok, 2, 2 );

    CONNECT( ok, clicked(), privacy, accept() );
    return privacy->exec();
}


/**********************************************************************
 * Handling of sizing of the components
 **********************************************************************/

/* This function is probably wrong, but we don't have many many choices...
   Since we can't know from the playlist Widget if we are inside a dock or not,
   because the playlist Widget can be called by THEDP, as a separate windows for
   the skins.
   Maybe the other solution is to redefine the sizeHint() of the playlist and
   ask _parent->isFloating()...
   If you think this would be better, please FIXME it...
*/

QSize MainInterface::sizeHint() const
{
    int nwidth  = controls->sizeHint().width();
    int nheight = controls->isVisible() ?
                  controls->size().height()
                  + menuBar()->size().height()
                  + statusBar()->size().height()
                  : 0 ;

    if( VISIBLE( bgWidget ) )
    {
        nheight += bgWidget->size().height();
        nwidth  = bgWidget->size().width();
    }
    else if( videoIsActive && videoWidget->isVisible() )
    {
        nheight += videoWidget->sizeHint().height();
        nwidth  = videoWidget->sizeHint().width();
    }
#if 0
    if( !dockPL->isFloating() && dockPL->isVisible() && dockPL->widget()  )
    {
        nheight += dockPL->size().height();
        nwidth = __MAX( nwidth, dockPL->size().width() );
        msg_Warn( p_intf, "3 %i %i", nheight, nwidth );
    }
#endif
    return QSize( nwidth, nheight );
}

void MainInterface::toggleFSC()
{
   if( !fullscreenControls ) return;

   IMEvent *eShow = new IMEvent( FullscreenControlToggle_Type, 0 );
   QApplication::postEvent( fullscreenControls, static_cast<QEvent *>(eShow) );
}


//FIXME remove me at the end...
void MainInterface::debug()
{
#ifndef NDEBUG
    msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
    msg_Dbg( p_intf, "sizeHint: %i - %i", sizeHint().height(), sizeHint().width() );
    if( videoWidget && videoWidget->isVisible() )
    {
        //    sleep( 10 );
        msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
        msg_Dbg( p_intf, "sizeHint: %i - %i", sizeHint().height(), sizeHint().width() );
    }
    adjustSize();
#endif
}

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

/* This event is used to deal with the fullscreen and always on top
   issue conflict (bug in wx) */
class SetVideoOnTopQtEvent : public QEvent
{
public:
    SetVideoOnTopQtEvent( bool _onTop ) :
      QEvent( (QEvent::Type)SetVideoOnTopEvent_Type ), onTop( _onTop)
    {}

    bool OnTop() const { return onTop; }

private:
    bool onTop;
};

/**
 * README
 * Thou shall not call/resize/hide widgets from on another thread.
 * This is wrong, and this is TEH reason to emit signals on those Video Functions
 **/
void *MainInterface::requestVideo( vout_thread_t *p_nvout, int *pi_x,
                                   int *pi_y, unsigned int *pi_width,
                                   unsigned int *pi_height )
{
    /* Request the videoWidget */
    void *ret = videoWidget->request( p_nvout,pi_x, pi_y, pi_width, pi_height );
    if( ret ) /* The videoWidget is available */
    {
        /* Did we have a bg ? Hide it! */
        if( VISIBLE( bgWidget) )
        {
            bgWasVisible = true;
            emit askBgWidgetToToggle();
        }
        else
            bgWasVisible = false;

        /* Consider the video active now */
        videoIsActive = true;

        emit askUpdate();

        if( fullscreenControls ) fullscreenControls->attachVout( p_nvout );
    }
    return ret;
}

/* Call from the WindowClose function */
void MainInterface::releaseVideo( void *p_win )
{
    if( fullscreenControls ) fullscreenControls->detachVout();
    if( p_win )
        emit askReleaseVideo( p_win );
}

/* Function that is CONNECTED to the previous emit */
void MainInterface::releaseVideoSlot( void *p_win )
{
    videoWidget->release( p_win );

    if( bgWasVisible )
    {
        /* Reset the bg state */
        bgWasVisible = false;
        bgWidget->show();
    }

    videoIsActive = false;

    /* Try to resize, except when you are in Fullscreen mode */
    if( !isFullScreen() ) doComponentsUpdate();
}

/* Call from WindowControl function */
int MainInterface::controlVideo( void *p_window, int i_query, va_list args )
{
    int i_ret = VLC_SUCCESS;
    switch( i_query )
    {
        case VOUT_GET_SIZE:
        {
            unsigned int *pi_width  = va_arg( args, unsigned int * );
            unsigned int *pi_height = va_arg( args, unsigned int * );
            *pi_width = videoWidget->videoSize.width();
            *pi_height = videoWidget->videoSize.height();
            break;
        }
        case VOUT_SET_SIZE:
        {
            unsigned int i_width  = va_arg( args, unsigned int );
            unsigned int i_height = va_arg( args, unsigned int );
            emit askVideoToResize( i_width, i_height );
            emit askUpdate();
            break;
        }
        case VOUT_SET_STAY_ON_TOP:
        {
            int i_arg = va_arg( args, int );
            QApplication::postEvent( this, new SetVideoOnTopQtEvent( i_arg ) );
            break;
        }
        default:
            i_ret = VLC_EGENERIC;
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
    THEDP->playlistDialog();
#if 0
    /* CREATION
    If no playlist exist, then create one and attach it to the DockPL*/
    if( !playlistWidget )
    {
        playlistWidget = new PlaylistWidget( p_intf, settings, dockPL );

        /* Add it to the parent DockWidget */
        dockPL->setWidget( playlistWidget );

        /* Add the dock to the main Interface */
        addDockWidget( Qt::BottomDockWidgetArea, dockPL );

        /* Make the playlist floating is requested. Default is not. */
        settings->beginGroup( "MainWindow" );
        if( settings->value( "playlist-floats", 1 ).toInt() )
        {
            msg_Dbg( p_intf, "we don't want the playlist inside");
            dockPL->setFloating( true );
        }
        settings->endGroup();
        settings->beginGroup( "playlist" );
        dockPL->move( settings->value( "pos", QPoint( 0,0 ) ).toPoint() );
        QSize newSize = settings->value( "size", QSize( 400, 300 ) ).toSize();
        if( newSize.isValid() )
            dockPL->resize( newSize );
        settings->endGroup();

        dockPL->show();
        playlistVisible = true;
    }
    else
    {
    /* toggle the visibility of the playlist */
       TOGGLEV( dockPL );
       resize( sizeHint() );
       playlistVisible = !playlistVisible;
    }
    #endif
}

/* Function called from the menu to undock the playlist */
void MainInterface::undockPlaylist()
{
//    dockPL->setFloating( true );
    adjustSize();
}

void MainInterface::toggleMinimalView()
{
    /* HACK for minimalView, see menus.cpp */
    if( !menuBar()->isVisible() ) QVLCMenu::minimalViewAction->toggle();

    if( i_visualmode != QT_ALWAYS_VIDEO_MODE &&
        i_visualmode != QT_MINIMAL_MODE )
    { /* NORMAL MODE then */
        if( videoWidget->isHidden() ) emit askBgWidgetToToggle();
        else
        {
            /* If video is visible, then toggle the status of bgWidget */
            bgWasVisible = !bgWasVisible;
        }
    }

    TOGGLEV( menuBar() );
    TOGGLEV( controls );
    TOGGLEV( statusBar() );
    doComponentsUpdate();
}

/* Video widget cannot do this synchronously as it runs in another thread */
/* Well, could it, actually ? Probably dangerous ... */
void MainInterface::doComponentsUpdate()
{
    msg_Dbg( p_intf, "Updating the geometry" );
    //    resize( sizeHint() );
    adjustSize();
}

/* toggling advanced controls buttons */
void MainInterface::toggleAdvanced()
{
    controls->toggleAdvanced();
    if( fullscreenControls ) fullscreenControls->toggleAdvanced();
}

/* Get the visibility status of the controls (hidden or not, advanced or not) */
int MainInterface::getControlsVisibilityStatus()
{
    return( (controls->isVisible() ? CONTROLS_VISIBLE : CONTROLS_HIDDEN )
                + CONTROLS_ADVANCED * controls->b_advancedVisible );
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

/************************************************************************
 * Other stuff
 ************************************************************************/
void MainInterface::setDisplayPosition( float pos, int time, int length )
{
    char psz_length[MSTRTIME_MAX_SIZE], psz_time[MSTRTIME_MAX_SIZE];
    secstotimestr( psz_length, length );
    secstotimestr( psz_time, ( b_remainingTime && length ) ? length - time
                                                           : time );

    QString timestr;
    timestr.sprintf( "%s/%s", psz_time,
                            ( !length && time ) ? "--:--" : psz_length );

    /* Add a minus to remaining time*/
    if( b_remainingTime && length ) timeLabel->setText( " -"+timestr+" " );
    else timeLabel->setText( " "+timestr+" " );
}

void MainInterface::toggleTimeDisplay()
{
    b_remainingTime = !b_remainingTime;
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
    msg_Dbg( p_intf, "Updating the stream status: %i", status );

    /* Forward the status to the controls to toggle Play/Pause */
    controls->setStatus( status );
    controls->updateInput();

    if( fullscreenControls )
    {
        fullscreenControls->setStatus( status );
        fullscreenControls->updateInput();
    }

    speedControl->setEnable( THEMIM->getIM()->hasInput() );

    /* And in the systray for the menu */
    if( sysTray )
        QVLCMenu::updateSystrayMenu( this, p_intf );
}

void MainInterface::setRate( int rate )
{
    QString str;
    str.setNum( ( 1000 / (double)rate ), 'f', 2 );
    str.append( "x" );
    speedLabel->setText( str );
    speedLabel->setToolTip( str );
    speedControl->updateControls( rate );
}

void MainInterface::updateOnTimer()
{
    /* No event for dying */
    if( intf_ShouldDie( p_intf ) )
    {
        QApplication::closeAllWindows();
        QApplication::quit();
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
    if( QDate::currentDate().dayOfYear() >= 354 )
        iconVLC =  QIcon( QPixmap( ":/vlc128-christmas.png" ) );
    else
        iconVLC =  QIcon( QPixmap( ":/vlc128.png" ) );
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
        /* Visible */
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
                    QSystemTrayIcon::Information, 3000 );
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
                    QSystemTrayIcon::NoIcon, 3000 );
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
        case  END_S:
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
                                    true ) )
            {
                event->acceptProposedAction();
                return;
            }
        }
     }
     bool first = true;
     foreach( QUrl url, mimeData->urls() )
     {
        QString s = url.toLocalFile();
        if( s.length() > 0 ) {
            playlist_Add( THEPL, qtu(s), NULL,
                          PLAYLIST_APPEND | (first ? PLAYLIST_GO:0),
                          PLAYLIST_END, true, false );
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
    THEDP->quit();
}

void MainInterface::toggleFullScreen( void )
{
    if( isFullScreen() )
        showNormal();
    else
        showFullScreen();
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
 * IntfShowCB: callback triggered by the intf-show libvlc variable.
 *****************************************************************************/
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    p_intf->p_sys->p_mi->toggleFSC();

    /* Show event */
     return VLC_SUCCESS;
}
