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
#include "actions_manager.hpp"

#include "util/customwidgets.hpp"
#include "util/qt_dirs.hpp"

#include "components/interface_widgets.hpp"
#include "components/controller.hpp"
#include "components/playlist/playlist.hpp"

#include "menus.hpp"
#include "recents.hpp"

#include <QCloseEvent>
#include <QKeyEvent>

#include <QUrl>
#include <QSize>
#include <QDate>

#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>

#include <assert.h>

#include <vlc_keys.h> /* Wheel event */
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

    /* Does the interface resize to video size or the opposite */
    b_keep_size = !config_GetInt( p_intf, "qt-video-autoresize" );

    /* Are we in the enhanced always-video mode or not ? */
    i_visualmode = config_GetInt( p_intf, "qt-display-mode" );

    /* Set the other interface settings */
    settings = getSettings();
    settings->beginGroup( "MainWindow" );

    /**
     * Retrieve saved sizes for main window
     *   mainBasedSize = based window size for normal mode
     *                  (no video, no background)
     *   mainVideoSize = window size with video (all modes)
     **/
    mainBasedSize = settings->value( "mainBasedSize", QSize( 350, 120 ) ).toSize();
    mainVideoSize = settings->value( "mainVideoSize", QSize( 400, 300 ) ).toSize();

    /* Visualisation, not really used yet */
    visualSelectorEnabled = settings->value( "visual-selector", false).toBool();

    /* Do we want anoying popups or not */
    notificationEnabled = (bool)config_GetInt( p_intf, "qt-notification" );

    /**************
     * Status Bar *
     **************/
    createStatusBar();

    /**************************
     *  UI and Widgets design
     **************************/
    setVLCWindowsTitle();
    handleMainUi( settings );

    /************
     * Menu Bar *
     ************/
    QVLCMenu::createMenuBar( this, p_intf, visualSelectorEnabled );

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

    /********************
     * Input Manager    *
     ********************/
    MainInputManager::getInstance( p_intf );

    /**************************
     * Various CONNECTs on IM *
     **************************/
    /* Connect the input manager to the GUI elements it manages */

    /**
     * Connects on nameChanged()
     * Those connects are not merged because different options can trigger
     * them down.
     */
    /* Naming in the controller statusbar */
    CONNECT( THEMIM->getIM(), nameChanged( QString ),
             this, setName( QString ) );
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
    /* Status on the systray */
    if( sysTray )
    {
        CONNECT( THEMIM->getIM(), statusChanged( int ),
                 this, updateSystrayTooltipStatus( int ) );
    }

    /* END CONNECTS ON IM */


    /************
     * Callbacks
     ************/
    var_Create( p_intf, "interaction", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "interaction", InteractCallback, this );
    interaction_Register( p_intf );

    var_AddCallback( p_intf->p_libvlc, "intf-show", IntfShowCB, p_intf );

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( p_intf->p_libvlc, "intf-popupmenu", PopupMenuCB, p_intf );


    /* VideoWidget connects to avoid different threads speaking to each other */
    connect( this, SIGNAL(askReleaseVideo( void )),
             this, SLOT(releaseVideoSlot( void )), Qt::BlockingQueuedConnection );

    if( videoWidget )
        CONNECT( this, askVideoToResize( unsigned int, unsigned int ),
                 videoWidget, SetSizing( unsigned int, unsigned int ) );

    CONNECT( this, askUpdate(), this, doComponentsUpdate() );

    /* Size and placement of interface */
    settings->beginGroup( "MainWindow" );
    QVLCTools::restoreWidgetPosition( settings, this, QSize(380, 60) );

    /* resize to previously saved main window size if appicable */
    if( b_keep_size )
    {
       if( i_visualmode == QT_ALWAYS_VIDEO_MODE ||
           i_visualmode == QT_MINIMAL_MODE )
       {
           resize( mainVideoSize );
       }
       else
       {
           resize( mainBasedSize );
       }
    }

    bool b_visible = settings->value( "playlist-visible", 0 ).toInt();
    settings->endGroup();

    /* Playlist */
    if( b_visible ) togglePlaylist();

    /* Final sizing and showing */
    setMinimumWidth( __MAX( controls->sizeHint().width(),
                            menuBar()->sizeHint().width() ) );
    show();

    /* And switch to minimal view if needed
       Must be called after the show() */
    if( i_visualmode == QT_MINIMAL_MODE )
        toggleMinimalView();

    /* Update the geometry : It is useful if you switch between
       qt-display-modes ?*/
    updateGeometry();
    resize( sizeHint() );

    /*****************************************************
     * End everything by creating the Systray Management *
     *****************************************************/
    initSystray();
}

MainInterface::~MainInterface()
{
    msg_Dbg( p_intf, "Destroying the main interface" );

    if( videoIsActive ) videoWidget->hide();

    if( playlistWidget )
    {
        if( !isDocked() )
            QVLCTools::saveWidgetPosition( p_intf, "Playlist", playlistWidget );

        delete playlistWidget;
    }

    ActionsManager::killInstance();

    if( fullscreenControls ) delete fullscreenControls;

    settings->beginGroup( "MainWindow" );

    settings->setValue( "pl-dock-status", (int)i_pl_dock );
    settings->setValue( "playlist-visible", (int)playlistVisible );
    settings->setValue( "adv-controls",
                        getControlsVisibilityStatus() & CONTROLS_ADVANCED );

    settings->setValue( "mainBasedSize", mainBasedSize );
    settings->setValue( "mainVideoSize", mainVideoSize );

    if( bgWidget )
        settings->setValue( "backgroundSize", bgWidget->size() );

    QVLCTools::saveWidgetPosition(settings, this);
    settings->endGroup();

    var_DelCallback( p_intf->p_libvlc, "intf-show", IntfShowCB, p_intf );

    /* Unregister callback for the intf-popupmenu variable */
    var_DelCallback( p_intf->p_libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    interaction_Unregister( p_intf );
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
    QStatusBar *statusBarr = statusBar();

    TimeLabel *timeLabel = new TimeLabel( p_intf );
    nameLabel = new QLabel( this );
    nameLabel->setTextInteractionFlags( Qt::TextSelectableByMouse
                                      | Qt::TextSelectableByKeyboard );
    SpeedLabel *speedLabel = new SpeedLabel( p_intf, "1.00x", this );

    /* Styling those labels */
    timeLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    speedLabel->setFrameStyle( QFrame::Sunken | QFrame::Panel );
    nameLabel->setFrameStyle( QFrame::Sunken | QFrame::StyledPanel);

    /* and adding those */
    statusBarr->addWidget( nameLabel, 8 );
    statusBarr->addPermanentWidget( speedLabel, 0 );
    statusBarr->addPermanentWidget( timeLabel, 0 );

    /* timeLabel behaviour:
       - double clicking opens the goto time dialog
       - right-clicking and clicking just toggle between remaining and
         elapsed time.*/
    CONNECT( timeLabel, timeLabelDoubleClicked(), THEDP, gotoTimeDialog() );
}

inline void MainInterface::initSystray()
{
    bool b_systrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    bool b_systrayWanted = config_GetInt( p_intf, "qt-system-tray" );

    if( config_GetInt( p_intf, "qt-start-minimized") > 0 )
    {
        if( b_systrayAvailable )
        {
            b_systrayWanted = true;
            hide();
        }
        else
            msg_Err( p_intf, "cannot start minimized without system tray bar" );
    }

    if( b_systrayAvailable && b_systrayWanted )
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
    controls = new ControlsWidget( p_intf,
                   settings->value( "adv-controls", false ).toBool(), this );
    CONNECT( controls, advancedControlsToggled( bool ),
             this, doComponentsUpdate() );
    inputC = new InputControlsWidget( p_intf, this );

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
    CONNECT( this, askBgWidgetToToggle(), bgWidget, toggle() );

    if( i_visualmode != QT_ALWAYS_VIDEO_MODE &&
        i_visualmode != QT_MINIMAL_MODE )
    {
        bgWidget->hide();
    }

    /* And video Outputs */
    if( videoEmbeddedFlag )
        videoWidget = new VideoWidget( p_intf );

    /* Add the controls Widget to the main Widget */
    mainLayout->insertWidget( 0, bgWidget );
    if( videoWidget ) mainLayout->insertWidget( 0, videoWidget, 10 );
    mainLayout->insertWidget( 2, inputC, 0, Qt::AlignBottom );
    mainLayout->insertWidget( settings->value( "ToolbarPos", 0 ).toInt() ? 0: 3,
                              controls, 0, Qt::AlignBottom );

    /* Finish the sizing */
    main->updateGeometry();

    getSettings()->endGroup();
#ifdef WIN32
    if ( depth() > 8 )
#endif
    /* Create the FULLSCREEN CONTROLS Widget */
    if( config_GetInt( p_intf, "qt-fs-controller" ) )
    {
        fullscreenControls = new FullscreenControllerWidget( p_intf );
    }
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
    QDialog *privacy = new QDialog( this );

    privacy->setWindowTitle( qtr( "Privacy and Network Policies" ) );

    QGridLayout *gLayout = new QGridLayout( privacy );

    QGroupBox *blabla = new QGroupBox( qtr( "Privacy and Network Warning" ) );
    QGridLayout *blablaLayout = new QGridLayout( blabla );
    QLabel *text = new QLabel( qtr(
        "<p>The <i>VideoLAN Team</i> doesn't like when an application goes "
        "online without authorization.</p>\n "
        "<p><i>VLC media player</i> can retreive limited information from "
        "the Internet in order to get CD covers or to check "
        "for available updates.</p>\n"
        "<p><i>VLC media player</i> <b>DOES NOT</b> send or collect <b>ANY</b> "
        "information, even anonymously, about your usage.</p>\n"
        "<p>Therefore please select from the following options, the default being "
        "almost no access to the web.</p>\n") );
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
    if( b_keep_size )
    {
        if( i_visualmode == QT_ALWAYS_VIDEO_MODE ||
            i_visualmode == QT_MINIMAL_MODE )
        {
                return mainVideoSize;
        }
        else
        {
            if( VISIBLE( bgWidget) ||
                ( videoIsActive && videoWidget->isVisible() )
              )
                return mainVideoSize;
            else
                return mainBasedSize;
        }
    }

    int nwidth  = controls->sizeHint().width();
    int nheight = controls->isVisible() ?
                  controls->size().height()
                  + inputC->size().height()
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
   QApplication::postEvent( fullscreenControls, eShow );
}

void MainInterface::debug()
{
#ifndef NDEBUG
    msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
    msg_Dbg( p_intf, "sizeHint: %i - %i", sizeHint().height(), sizeHint().width() );
    if( videoWidget && videoWidget->isVisible() )
    {
        msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
        msg_Dbg( p_intf, "sizeHint: %i - %i", sizeHint().height(), sizeHint().width() );
    }
#endif
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
 * This is wrong, and this is THE reason to emit signals on those Video Functions
 **/
WId MainInterface::requestVideo( vout_thread_t *p_nvout, int *pi_x,
                                 int *pi_y, unsigned int *pi_width,
                                 unsigned int *pi_height )
{
    /* Request the videoWidget */
    WId ret = videoWidget->request( p_nvout,pi_x, pi_y,
                                    pi_width, pi_height, b_keep_size );
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
    }
    return ret;
}

/* Call from the WindowClose function */
void MainInterface::releaseVideo( void )
{
    emit askReleaseVideo( );
}

/* Function that is CONNECTED to the previous emit */
void MainInterface::releaseVideoSlot( void )
{
    videoWidget->release( );

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
int MainInterface::controlVideo( int i_query, va_list args )
{
    int i_ret = VLC_SUCCESS;
    switch( i_query )
    {
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
    /* CREATION
    If no playlist exist, then create one and attach it to the DockPL*/
    if( !playlistWidget )
    {
        playlistWidget = new PlaylistWidget( p_intf );

        i_pl_dock = PL_UNDOCKED;
/*        i_pl_dock = (pl_dock_e)getSettings()
                         ->value( "pl-dock-status", PL_UNDOCKED ).toInt(); */

        if( i_pl_dock == PL_UNDOCKED )
        {
            playlistWidget->setWindowFlags( Qt::Window );

            /* This will restore the geometry but will not work for position,
               because of parenting */
            QVLCTools::restoreWidgetPosition( p_intf, "Playlist",
                    playlistWidget, QSize( 600, 300 ) );
        }
        else
        {
            mainLayout->insertWidget( 4, playlistWidget );
        }
        playlistVisible = true;

        playlistWidget->show();
    }
    else
    {
    /* toggle the visibility of the playlist */
       TOGGLEV( playlistWidget );
       playlistVisible = !playlistVisible;
       //doComponentsUpdate(); //resize( sizeHint() );
    }
}

/* Function called from the menu to undock the playlist */
void MainInterface::undockPlaylist()
{
//    dockPL->setFloating( true );
//    adjustSize();
}

void MainInterface::dockPlaylist( pl_dock_e i_pos )
{
}

void MainInterface::toggleMinimalView()
{
    if( i_visualmode != QT_ALWAYS_VIDEO_MODE &&
        i_visualmode != QT_MINIMAL_MODE )
    { /* NORMAL MODE then */
        if( !videoWidget || videoWidget->isHidden() ) emit askBgWidgetToToggle();
        else
        {
            /* If video is visible, then toggle the status of bgWidget */
            bgWasVisible = !bgWasVisible;
        }
    }

    TOGGLEV( menuBar() );
    TOGGLEV( controls );
    TOGGLEV( statusBar() );
    TOGGLEV( inputC );
    doComponentsUpdate();

    QVLCMenu::minimalViewAction->setChecked( bgWasVisible );
}

/* Video widget cannot do this synchronously as it runs in another thread */
/* Well, could it, actually ? Probably dangerous ... */

/* This function is called:
   - toggling of minimal View
   - through askUpdate() by Vout thread request video and resize video (zoom)
   - Advanced buttons toggled
 */
void MainInterface::doComponentsUpdate()
{
    msg_Dbg( p_intf, "Updating the geometry" );
    /* Here we resize to sizeHint() and not adjustsize because we want
       the videoWidget to be exactly the correctSize */
    resize( sizeHint() );
    //    adjustSize()  ;
#ifndef NDEBUG
    debug();
#endif
}

/* toggling advanced controls buttons */
void MainInterface::toggleAdvanced()
{
    controls->toggleAdvanced();
//    if( fullscreenControls ) fullscreenControls->toggleAdvanced();
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
void MainInterface::setName( QString name )
{
    input_name = name; /* store it for the QSystray use */
    /* Display it in the status bar, but also as a Tooltip in case it doesn't
       fit in the label */
    nameLabel->setText( " " + name + " " );
    nameLabel->setToolTip( " " + name +" " );
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
        /* Visible (possibly under other windows) */
#ifdef WIN32
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
    QVLCMenu::updateSystrayMenu( this, p_intf );
}

void MainInterface::handleSystrayClick(
                                    QSystemTrayIcon::ActivationReason reason )
{
    switch( reason )
    {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            toggleUpdateSystrayMenu();
            break;
        case QSystemTrayIcon::MiddleClick:
        case QSystemTrayIcon::Context:
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

    QVLCMenu::updateSystrayMenu( this, p_intf );
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
    dropEventPlay( event, true );
}

void MainInterface::dropEventPlay( QDropEvent *event, bool b_play )
{
     const QMimeData *mimeData = event->mimeData();

     /* D&D of a subtitles file, add it on the fly */
     if( mimeData->urls().size() == 1 )
     {
        if( THEMIM->getIM()->hasInput() )
        {
            if( !input_AddSubtitle( THEMIM->getInput(),
                                    qtu( toNativeSeparators(
                                         mimeData->urls()[0].toLocalFile() ) ),
                                    true ) )
            {
                event->acceptProposedAction();
                return;
            }
        }
     }
     bool first = b_play;
     foreach( const QUrl &url, mimeData->urls() )
     {
        QString s = toNativeSeparators( url.toLocalFile() );

        if( s.length() > 0 ) {
            playlist_Add( THEPL, qtu(s), NULL,
                          PLAYLIST_APPEND | (first ? PLAYLIST_GO: 0),
                          PLAYLIST_END, true, false );
            first = false;
            RecentsMRL::getInstance( p_intf )->addRecent( s );
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
    if ( event->type() == (int)SetVideoOnTopEvent_Type )
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
    if( ( e->modifiers() &  Qt::ControlModifier ) && ( e->key() == Qt::Key_H )
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

void MainInterface::resizeEvent( QResizeEvent * event )
{
    if( b_keep_size )
    {
        if( i_visualmode == QT_ALWAYS_VIDEO_MODE ||
            i_visualmode == QT_MINIMAL_MODE )
        {
                mainVideoSize = size();
        }
        else
        {
            if( VISIBLE( bgWidget) ||
                ( videoIsActive && videoWidget->isVisible() )
              )
                mainVideoSize = size();
            else
                mainBasedSize = size();
        }
    }
}

void MainInterface::wheelEvent( QWheelEvent *e )
{
    int i_vlckey = qtWheelEventToVLCKey( e );
    var_SetInteger( p_intf->p_libvlc, "key-pressed", i_vlckey );
    e->accept();
}

void MainInterface::closeEvent( QCloseEvent *e )
{
    e->accept();
    hide();
    THEDP->quit();
}

void MainInterface::toggleFullScreen( void )
{
    if( isFullScreen() )
    {
        showNormal();
        emit askUpdate(); // Needed if video was launched after the F11
        QVLCMenu::fullscreenViewAction->setChecked( false );
    }
    else
    {
        showFullScreen();
        QVLCMenu::fullscreenViewAction->setChecked( true );
    }

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
    QApplication::postEvent( THEDP, event );
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
                                new_val.b_bool, NULL );
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

