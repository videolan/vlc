/*****************************************************************************
 * main_interface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2009 the VideoLAN team
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
#include "dialogs/external.hpp"

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
#include <QStackedWidget>

#ifdef WIN32
 #include <vlc_windows_interfaces.h>
 #include <QBitmap>
#endif

#include <assert.h>

#include <vlc_keys.h> /* Wheel event */
#include <vlc_vout_window.h>
#include <vlc_vout.h>

/* Callback prototypes */
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param );
static int IntfShowCB( vlc_object_t *p_this, const char *psz_variable,
                       vlc_value_t old_val, vlc_value_t new_val, void *param );

MainInterface::MainInterface( intf_thread_t *_p_intf ) : QVLCMW( _p_intf )
{
    /* Variables initialisation */
    // need_components_update = false;
    bgWidget             = NULL;
    videoWidget          = NULL;
    playlistWidget       = NULL;
#ifndef HAVE_MAEMO
    sysTray              = NULL;
#endif
    playlistVisible      = false;
    input_name           = "";
    fullscreenControls   = NULL;
    cryptedLabel         = NULL;
    controls             = NULL;
    inputC               = NULL;
    b_shouldHide         = false;

    stackCentralOldState = HIDDEN_TAB;
    i_bg_height          = 0;

    /* Ask for privacy */
    askForPrivacy();

    /**
     *  Configuration and settings
     *  Pre-building of interface
     **/
    /* Main settings */
    setFocusPolicy( Qt::StrongFocus );
    setAcceptDrops( true );
    setWindowRole( "vlc-main" );
    setWindowIcon( QApplication::windowIcon() );
    setWindowOpacity( config_GetFloat( p_intf, "qt-opacity" ) );

    /* Set The Video In emebedded Mode or not */
    videoEmbeddedFlag = config_GetInt( p_intf, "embedded-video" );

    /* Does the interface resize to video size or the opposite */
    b_keep_size = !config_GetInt( p_intf, "qt-video-autoresize" );

    /* Are we in the enhanced always-video mode or not ? */
    i_visualmode = config_GetInt( p_intf, "qt-display-mode" );

        /* Do we want anoying popups or not */
    notificationEnabled = (bool)config_GetInt( p_intf, "qt-notification" );

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

    /**************
     * Status Bar *
     **************/
    createStatusBar();

    /**************************
     *  UI and Widgets design
     **************************/
    setVLCWindowsTitle();
    createMainWidget( settings );

    /************
     * Menu Bar *
     ************/
    QVLCMenu::createMenuBar( this, p_intf );
    CONNECT( THEMIM->getIM(), voutListChanged( vout_thread_t **, int ),
             this, destroyPopupMenu() );

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

    /*********************************
     * Create the Systray Management *
     *********************************/
    initSystray();

    /********************
     * Input Manager    *
     ********************/
    MainInputManager::getInstance( p_intf );

    /************************************************************
     * Connect the input manager to the GUI elements it manages *
     ************************************************************/
    /**
     * Connects on nameChanged()
     * Those connects are different because options can impeach them to trigger.
     **/
    /* Main Interface statusbar */
    CONNECT( THEMIM->getIM(), nameChanged( const QString& ),
             this, setName( const QString& ) );
    /* and systray */
#ifndef HAVE_MAEMO
    if( sysTray )
    {
        CONNECT( THEMIM->getIM(), nameChanged( const QString& ),
                 this, updateSystrayTooltipName( const QString& ) );
    }
#endif
    /* and title of the Main Interface*/
    if( config_GetInt( p_intf, "qt-name-in-title" ) )
    {
        CONNECT( THEMIM->getIM(), nameChanged( const QString& ),
                 this, setVLCWindowsTitle( const QString& ) );
    }

    /**
     * CONNECTS on PLAY_STATUS
     **/
    /* Status on the systray */
#ifndef HAVE_MAEMO
    if( sysTray )
    {
        CONNECT( THEMIM->getIM(), statusChanged( int ),
                 this, updateSystrayTooltipStatus( int ) );
    }
#endif

    /* END CONNECTS ON IM */

    /************
     * Callbacks
     ************/
    var_AddCallback( p_intf->p_libvlc, "intf-show", IntfShowCB, p_intf );

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( p_intf->p_libvlc, "intf-popupmenu", PopupMenuCB, p_intf );


    /* VideoWidget connects for asynchronous calls */
    connect( this, SIGNAL(askGetVideo(WId*,int*,int*,unsigned*,unsigned *)),
             this, SLOT(getVideoSlot(WId*,int*,int*,unsigned*,unsigned*)),
             Qt::BlockingQueuedConnection );
    connect( this, SIGNAL(askReleaseVideo( void )),
             this, SLOT(releaseVideoSlot( void )),
             Qt::BlockingQueuedConnection );

    if( videoWidget )
    {
        CONNECT( this, askVideoToResize( unsigned int, unsigned int ),
                 videoWidget, SetSizing( unsigned int, unsigned int ) );
        CONNECT( this, askVideoSetFullScreen( bool ),
                 videoWidget, SetFullScreen( bool ) );
        CONNECT( videoWidget, keyPressed( QKeyEvent * ),
                 this, handleKeyPress( QKeyEvent * ) );
    }

    CONNECT( this, askUpdate(), this, doComponentsUpdate() );
    CONNECT( THEDP, toolBarConfUpdated(), this, recreateToolbars() );

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

    bool b_pl_visible = settings->value( "playlist-visible", 0 ).toInt();
    settings->endGroup();

    /* Playlist */
    if( b_pl_visible ) togglePlaylist();

    /* Enable the popup menu in the MI */
    setContextMenuPolicy( Qt::CustomContextMenu );
    CONNECT( this, customContextMenuRequested( const QPoint& ),
             this, popupMenu( const QPoint& ) );

    debug();

    /* Final sizing and showing */
    setVisible( !b_shouldHide );
    //setMinimumSize( QSize( 0, 0 ) );
//    setMinimumWidth( __MAX( controls->sizeHint().width(),
  //                          menuBar()->sizeHint().width() ) );

    debug();
    /* And switch to minimal view if needed
       Must be called after the show() */
    if( i_visualmode == QT_MINIMAL_MODE )
        toggleMinimalView( true );

    /* Update the geometry : It is useful if you switch between
       qt-display-modes */
    updateGeometry();
    resize( sizeHint() );

#ifdef WIN32
    createTaskBarButtons();
#endif
}

MainInterface::~MainInterface()
{
    msg_Dbg( p_intf, "Destroying the main interface" );

    /* Unsure we hide the videoWidget before destroying it */
    if( stackCentralOldState == VIDEO_TAB )
    {
        showBg();
    }

    /* Save playlist state */
    if( playlistWidget )
    {
        if( !isDocked() )
            QVLCTools::saveWidgetPosition( p_intf, "Playlist", playlistWidget );

        delete playlistWidget;
    }

#ifdef WIN32
    if( himl )
        ImageList_Destroy( himl );
    if(p_taskbl)
        p_taskbl->vt->Release(p_taskbl);
    CoUninitialize();
#endif

    /* Be sure to kill the actionsManager... FIXME */
    ActionsManager::killInstance();

    /* Delete the FSC controller */
    delete fullscreenControls;

    /* Save states */
    settings->beginGroup( "MainWindow" );
    settings->setValue( "pl-dock-status", (int)i_pl_dock );
    settings->setValue( "playlist-visible", (int)playlistVisible );
    settings->setValue( "adv-controls",
                        getControlsVisibilityStatus() & CONTROLS_ADVANCED );

    settings->setValue( "mainBasedSize", mainBasedSize );
    settings->setValue( "mainVideoSize", mainVideoSize );

    if( bgWidget )
        settings->setValue( "backgroundSize", bgWidget->size() );

    /* Save this size */
    QVLCTools::saveWidgetPosition(settings, this);
    settings->endGroup();


    /* Unregister callbacks */
    var_DelCallback( p_intf->p_libvlc, "intf-show", IntfShowCB, p_intf );
    var_DelCallback( p_intf->p_libvlc, "intf-popupmenu", PopupMenuCB, p_intf );

    p_intf->p_sys->p_mi = NULL;
}

/*****************************
 *   Main UI handling        *
 *****************************/
void MainInterface::recreateToolbars()
{
    msg_Err( p_intf, "Recreating the toolbars" );
    settings->beginGroup( "MainWindow" );
    delete controls;
    delete inputC;
    controls = new ControlsWidget( p_intf, false, this ); /* FIXME */
    CONNECT( controls, advancedControlsToggled( bool ),
             this, doComponentsUpdate() );
    CONNECT( controls, sizeChanged(),
             this, doComponentsUpdate() );
    inputC = new InputControlsWidget( p_intf, this );

    mainLayout->insertWidget( 2, inputC );
    mainLayout->insertWidget( settings->value( "ToolbarPos", 0 ).toInt() ? 0: 3,
                              controls );
    settings->endGroup();
}

void MainInterface::createMainWidget( QSettings *settings )
{
    /* Create the main Widget and the mainLayout */
    QWidget *main = new QWidget;
    setCentralWidget( main );
    mainLayout = new QVBoxLayout( main );

    /* Margins, spacing */
    main->setContentsMargins( 0, 0, 0, 0 );
    mainLayout->setSpacing( 0 );
    mainLayout->setMargin( 0 );

    /* */
    stackCentralW = new QStackedWidget( main );

    /* Bg Cone */
    bgWidget = new BackgroundWidget( p_intf );
    bgWidget->resize(
            settings->value( "backgroundSize", QSize( 300, 200 ) ).toSize() );
    bgWidget->updateGeometry();
    stackCentralW->insertWidget( BACKG_TAB, bgWidget );

    if( i_visualmode != QT_ALWAYS_VIDEO_MODE &&
        i_visualmode != QT_MINIMAL_MODE )
    {
        stackCentralW->hide();
    }

    /* And video Outputs */
    if( videoEmbeddedFlag )
    {
        videoWidget = new VideoWidget( p_intf );
        stackCentralW->insertWidget( VIDEO_TAB, videoWidget );
    }
    mainLayout->insertWidget( 1, stackCentralW, 100 );
    /* Create the CONTROLS Widget */
    controls = new ControlsWidget( p_intf,
                   settings->value( "adv-controls", false ).toBool(), this );
    CONNECT( controls, advancedControlsToggled( bool ),
             this, doComponentsUpdate() );
    CONNECT( controls, sizeChanged(),
             this, doComponentsUpdate() );
    inputC = new InputControlsWidget( p_intf, this );

    //mainLayout->setRowStretch( 1, 10 );
    mainLayout->insertWidget( 2, inputC );
    mainLayout->insertWidget( settings->value( "ToolbarPos", 0 ).toInt() ? 0: 3,
                              controls );

    /* Visualisation */
    /* Disabled for now, they SUCK */
    #if 0
    visualSelector = new VisualSelector( p_intf );
    mainLayout->insertWidget( 0, visualSelector );
    visualSelector->hide();
    #endif

    /* Finish the sizing */
    main->updateGeometry();

    getSettings()->endGroup();
#ifdef WIN32
    if ( depth() > 8 )
#endif
    /* Create the FULLSCREEN CONTROLS Widget */
    if( config_GetInt( p_intf, "qt-fs-controller" ) )
    {
        fullscreenControls = new FullscreenControllerWidget( p_intf, this );
        CONNECT( fullscreenControls, keyPressed( QKeyEvent * ),
                 this, handleKeyPress( QKeyEvent * ) );
    }
}

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

    CONNECT( THEMIM->getIM(), encryptionChanged( bool ),
             this, showCryptedLabel( bool ) );
}

#ifdef WIN32
void MainInterface::createTaskBarButtons()
{
    /*Here is the code for the taskbar thumb buttons
    FIXME:We need pretty buttons in 16x16 px that are handled correctly by masks in Qt
    FIXME:the play button's picture doesn't changed to pause when clicked
    */
    OSVERSIONINFO winVer;
    winVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if( GetVersionEx(&winVer) && winVer.dwMajorVersion > 5 )
    {
        if(himl = ImageList_Create( 15, //cx
                                    18, //cy
                                    ILC_COLOR,//flags
                                    4,//initial nb of images
                                    0//nb of images that can be added
                                    ))
        {
            QPixmap img   = QPixmap(":/toolbar/previous_b");
            QPixmap img2  = QPixmap(":/toolbar/pause_b");
            QPixmap img3  = QPixmap(":/toolbar/play_b");
            QPixmap img4  = QPixmap(":/toolbar/next_b");
            QBitmap mask  = img.createMaskFromColor(Qt::transparent);
            QBitmap mask2 = img2.createMaskFromColor(Qt::transparent);
            QBitmap mask3 = img3.createMaskFromColor(Qt::transparent);
            QBitmap mask4 = img4.createMaskFromColor(Qt::transparent);

            if(-1 == ImageList_Add(himl, img.toWinHBITMAP(QPixmap::PremultipliedAlpha),mask.toWinHBITMAP()))
                msg_Err( p_intf, "ImageList_Add failed" );
            if(-1 == ImageList_Add(himl, img2.toWinHBITMAP(QPixmap::PremultipliedAlpha),mask2.toWinHBITMAP()))
                msg_Err( p_intf, "ImageList_Add failed" );
            if(-1 == ImageList_Add(himl, img3.toWinHBITMAP(QPixmap::PremultipliedAlpha),mask3.toWinHBITMAP()))
                msg_Err( p_intf, "ImageList_Add failed" );
            if(-1 == ImageList_Add(himl, img4.toWinHBITMAP(QPixmap::PremultipliedAlpha),mask4.toWinHBITMAP()))
                msg_Err( p_intf, "ImageList_Add failed" );
        }

        CoInitialize( 0 );

        if( S_OK == CoCreateInstance( &clsid_ITaskbarList,
                    NULL, CLSCTX_INPROC_SERVER,
                    &IID_ITaskbarList3,
                    (void **)&p_taskbl) )
        {
            p_taskbl->vt->HrInit(p_taskbl);

            int msg_value = RegisterWindowMessage("TaskbarButtonCreated");
            //msg_Info( p_intf, "msg value: %04x", msg_value );

            // Define an array of two buttons. These buttons provide images through an
            // image list and also provide tooltips.
            DWORD dwMask = THB_BITMAP | THB_FLAGS;

            THUMBBUTTON thbButtons[2];
            thbButtons[0].dwMask = dwMask;
            thbButtons[0].iId = 0;
            thbButtons[0].iBitmap = 0;
            thbButtons[0].dwFlags = THBF_HIDDEN;

            thbButtons[1].dwMask = dwMask;
            thbButtons[1].iId = 1;
            thbButtons[1].iBitmap = 2;
            thbButtons[1].dwFlags = THBF_HIDDEN;

            thbButtons[2].dwMask = dwMask;
            thbButtons[2].iId = 2;
            thbButtons[2].iBitmap = 3;
            thbButtons[2].dwFlags = THBF_HIDDEN;

            HRESULT hr = p_taskbl->vt->ThumbBarSetImageList(p_taskbl, GetForegroundWindow(), himl );
            if(S_OK != hr)
                msg_Err( p_intf, "ThumbBarSetImageList failed with error %08x", hr );
            if(S_OK != p_taskbl->vt->ThumbBarAddButtons(p_taskbl, GetForegroundWindow(), 3, thbButtons))
                msg_Err( p_intf, "ThumbBarAddButtons failed with error %08x", GetLastError() );

            CONNECT( THEMIM->getIM(), statusChanged( int ), this, changeThumbbarButtons( int ) );
        }
    }
    else
    {
        himl = NULL;
        p_taskbl = NULL;
    }
}
#endif


inline void MainInterface::initSystray()
{
#ifndef HAVE_MAEMO
    bool b_systrayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    bool b_systrayWanted = config_GetInt( p_intf, "qt-system-tray" );

    if( config_GetInt( p_intf, "qt-start-minimized") > 0 )
    {
        if( b_systrayAvailable )
        {
            b_systrayWanted = true;
            b_shouldHide = true;
        }
        else
            msg_Err( p_intf, "cannot start minimized without system tray bar" );
    }

    if( b_systrayAvailable && b_systrayWanted )
            createSystray();
#endif
}

inline void MainInterface::askForPrivacy()
{
#ifndef HAVE_MAEMO
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
#endif
}

int MainInterface::privacyDialog( QList<ConfigControl *> *controls )
{
    QDialog *privacy = new QDialog( this );

    privacy->setWindowTitle( qtr( "Privacy and Network Policies" ) );
    privacy->setWindowRole( "vlc-privacy" );

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
        controls->append( control );                              \
    }

#define CONFIG_GENERIC_NOBOOL( option, type )                     \
    p_config =  config_FindConfig( VLC_OBJECT(p_intf), option );  \
    if( p_config )                                                \
    {                                                             \
        control =  new type ## ConfigControl( VLC_OBJECT(p_intf), \
                p_config, options, optionsLayout, line );         \
        controls->append( control );                              \
    }

    CONFIG_GENERIC( "album-art", IntegerList ); line++;
#ifdef UPDATE_CHECK
    CONFIG_GENERIC_NOBOOL( "qt-updates-notif", Bool ); line++;
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
#if 0
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
#endif

    int nwidth  = __MAX( controls->sizeHint().width(),
                         menuBar()->sizeHint().width() );

    int nheight = controls->isVisible() ?
                  controls->size().height()
                  + inputC->size().height()
                  + menuBar()->size().height()
                  + statusBar()->size().height()
                  : 0 ;

    if( stackCentralW->isVisible() )
        nheight += stackCentralW->height();
        nwidth  = __MAX( nwidth, stackCentralW->width() );

/*    if( VISIBLE( bgWidget ) )
    {
        msg_Warn( p_intf, "Hello here" );
        if( i_bg_height )
            nheight += i_bg_height;
        else
            nheight += bgWidget->size().height();
        nwidth  = __MAX( nwidth, bgWidget->size().width() );
    }
    else if( videoIsActive && videoWidget->isVisible() )
    {
        msg_Warn( p_intf, "Hello there" );
        nheight += videoWidget->sizeHint().height();
        nwidth  = __MAX( nwidth, videoWidget->sizeHint().width() );
    }*/
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


/* Video widget cannot do this synchronously as it runs in another thread */
/* Well, could it, actually ? Probably dangerous ... */

/* This function is called:
   - toggling of minimal View
   - through askUpdate() by Vout thread request video and resize video (zoom)
   - Advanced buttons toggled
 */
void MainInterface::doComponentsUpdate()
{
    if( isFullScreen() || isMaximized() ) return;

    msg_Err( p_intf, "Updating the geometry" );
    /* Here we resize to sizeHint() and not adjustsize because we want
       the videoWidget to be exactly the correctSize */

#if 1
    debug();
#endif
    /* This is WRONG */
    setMinimumSize( 0, 0 );
    resize( sizeHint() );

    //adjustSize()  ;
}

void MainInterface::debug()
{
#if 1
    if( stackCentralW->isVisible() )
        msg_Dbg( p_intf, "CentralStack visible" );
    else
        msg_Dbg( p_intf, "CentralStack inVisible" );
    //msg_Dbg( p_intf, "Stack Size: %i - %i", stackCentralW->sizeHint().height(), stackCentralW->sizeHint().width() );
    msg_Dbg( p_intf, "Stack Size: %i - %i", stackCentralW->size().height(), size().width() );
    msg_Dbg( p_intf, "Stack Size: %i - %i", stackCentralW->widget( VIDEO_TAB )->size().height(), stackCentralW->widget( VIDEO_TAB )->size().width() );

    msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
    msg_Dbg( p_intf, "sizeHint: %i - %i", sizeHint().height(), sizeHint().width() );
    //msg_Dbg( p_intf, "maximumsize: %i - %i", maximumSize().height(), maximumSize().width() );

    msg_Dbg( p_intf, "Stack minimumsize: %i - %i", stackCentralW->minimumSize().height(), stackCentralW->minimumSize().width() );
    msg_Dbg( p_intf, "Controls minimumsize: %i - %i", controls->minimumSize().height(), controls->minimumSize().width() );
    msg_Dbg( p_intf, "Central minimumsize: %i - %i", centralWidget()->minimumSize().height(), centralWidget()->minimumSize().width() );
        msg_Dbg( p_intf, "Menu minimumsize: %i - %i", menuBar()->minimumSize().height(), menuBar()->minimumSize().width() );
            msg_Dbg( p_intf, "Input minimuSize: %i - %i", inputC->minimumSize().height(), inputC->minimumSize().width() );
            msg_Dbg( p_intf, "Status minimumsize: %i - %i", statusBar()->minimumSize().height(), statusBar()->minimumSize().width() );
    msg_Dbg( p_intf, "minimumsize: %i - %i", minimumSize().height(), minimumSize().width() );


    /*if( videoWidget && videoWidget->isVisible() )
    {
        msg_Dbg( p_intf, "size: %i - %i", size().height(), size().width() );
        msg_Dbg( p_intf, "sizeHint: %i - %i", sizeHint().height(), sizeHint().width() );
    }*/
#endif
}

void MainInterface::destroyPopupMenu()
{
    QVLCMenu::PopupMenu(p_intf, false );
}


void MainInterface::toggleFSC()
{
   if( !fullscreenControls ) return;

   IMEvent *eShow = new IMEvent( FullscreenControlToggle_Type, 0 );
   QApplication::postEvent( fullscreenControls, eShow );
}

void MainInterface::popupMenu( const QPoint &p )
{
    /* Ow, that's ugly: don't show the popup menu if cursor over
     * the main menu bar or the status bar */
    if( !childAt( p ) || ( ( childAt( p ) != menuBar() )
                        && ( childAt( p )->parentWidget() != statusBar() ) ) )
        QVLCMenu::PopupMenu( p_intf, true );
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
 * NOTE:
 * You must note change the state of this object or other Qt4 UI objects,
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
    /* Request the videoWidget */
    WId ret = videoWidget->request( pi_x, pi_y,
                                    pi_width, pi_height, b_keep_size );
    *p_id = ret;
    if( ret ) /* The videoWidget is available */
    {
        /* ask videoWidget to show */
        videoWidget->SetSizing( *pi_width, *pi_height );

        /* Consider the video active now */
        showVideo();

        stackCentralW->resize( *pi_width, *pi_height );

        emit askUpdate();
    }
}

inline void MainInterface::showTab( int i_tab )
{
    stackCentralOldState = stackCentralW->currentIndex();

    stackCentralW->setCurrentIndex( i_tab );
    if( stackCentralW->isHidden() ) stackCentralW->show();
}

/* Asynchronous call from the WindowClose function */
void MainInterface::releaseVideo( void )
{
    emit askReleaseVideo( );
}

/* Function that is CONNECTED to the previous emit */
void MainInterface::releaseVideoSlot( void )
{
    videoWidget->release( );

    /* Restore the previous State */
    if( stackCentralOldState == BACKG_TAB )
    {
        showBg();
    }
    else
    {
        stackCentralW->hide();
        stackCentralOldState == -1;
    }

    /* Try to resize, except when you are in Fullscreen mode */
    doComponentsUpdate();
}

/* Asynchronous call from WindowControl function */
int MainInterface::controlVideo( int i_query, va_list args )
{
    switch( i_query )
    {
        /* Debug to check if VOUT_WINDOW_SET_SIZE is called? */
        msg_Dbg( p_intf, "Control Video: %i", i_query );
    case VOUT_WINDOW_SET_SIZE:
    {
        unsigned int i_width  = va_arg( args, unsigned int );
        unsigned int i_height = va_arg( args, unsigned int );
        emit askVideoToResize( i_width, i_height );
        emit askUpdate();
        return VLC_SUCCESS;
    }
    case VOUT_WINDOW_SET_ON_TOP:
    {
        int i_arg = va_arg( args, int );
        QApplication::postEvent( this, new SetVideoOnTopQtEvent( i_arg ) );
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
void MainInterface::togglePlaylist()
{
    /* CREATION
    If no playlist exist, then create one and attach it to the DockPL*/
    if( !playlistWidget )
    {
        playlistWidget = new PlaylistWidget( p_intf );

        i_pl_dock = PL_BOTTOM;
        /*i_pl_dock = (pl_dock_e)getSettings()
                         ->value( "pl-dock-status", PL_UNDOCKED ).toInt();*/

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
            stackCentralW->insertWidget(PLAYL_TAB, playlistWidget );
            stackCentralW->setCurrentWidget( playlistWidget );
            stackCentralW->show();
        }
        playlistVisible = true;

        playlistWidget->show();
    }
    else
    {
    /* toggle the visibility of the playlist */
        if( stackCentralW->currentIndex() != PLAYL_TAB )
        {
            stackCentralW->insertWidget(PLAYL_TAB, playlistWidget );
            stackCentralW->setCurrentWidget( playlistWidget );
            stackCentralW->show();
        }
        else
            stackCentralW->setCurrentIndex( VIDEO_TAB );
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

void MainInterface::toggleMinimalView( bool b_switch )
{
    if( i_visualmode != QT_ALWAYS_VIDEO_MODE &&
        i_visualmode != QT_MINIMAL_MODE )
    { /* NORMAL MODE then */
        stackCentralW->show();
        if( !videoWidget || stackCentralW->currentIndex() != VIDEO_TAB )
        {
            showBg();
        }
        else
        {
            /* If video is visible, then toggle the status of bgWidget */ //bgWasVisible = !bgWasVisible;
            if( stackCentralOldState == BACKG_TAB )
                stackCentralOldState = HIDDEN_TAB;
            else
                stackCentralOldState = BACKG_TAB;
        }
    }

    i_bg_height = stackCentralW->height();

    menuBar()->setVisible( !b_switch );
    controls->setVisible( !b_switch );
    statusBar()->setVisible( !b_switch );
    inputC->setVisible( !b_switch );

    doComponentsUpdate();

    emit minimalViewToggled( b_switch );
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
void MainInterface::setName( const QString& name )
{
    input_name = name; /* store it for the QSystray use */
    /* Display it in the status bar, but also as a Tooltip in case it doesn't
       fit in the label */
    nameLabel->setText( " " + name + " " );
    nameLabel->setToolTip( " " + name +" " );
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

/*****************************************************************************
 * Systray Icon and Systray Menu
 *****************************************************************************/
#ifndef HAVE_MAEMO
/**
 * Create a SystemTray icon and a menu that would go with it.
 * Connects to a click handler on the icon.
 **/
void MainInterface::createSystray()
{
    QIcon iconVLC;
    if( QDate::currentDate().dayOfYear() >= 354 )
        iconVLC =  QIcon( ":/logo/vlc128-christmas.png" );
    else
        iconVLC =  QIcon( ":/logo/vlc128.png" );
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
    QVLCMenu::updateSystrayMenu( this, p_intf );
}
#endif

/************************************************************************
 * D&D Events
 ************************************************************************/
void MainInterface::dropEvent(QDropEvent *event)
{
    dropEventPlay( event, true );
}

void MainInterface::dropEventPlay( QDropEvent *event, bool b_play )
{
    event->setDropAction( Qt::CopyAction );
    if( !event->possibleActions() & Qt::CopyAction )
        return;

    const QMimeData *mimeData = event->mimeData();

    /* D&D of a subtitles file, add it on the fly */
    if( mimeData->urls().size() == 1 && THEMIM->getIM()->hasInput() )
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
        QString s = toNativeSeparators( url.toLocalFile() );

        if( s.length() > 0 ) {
            char* psz_uri = make_URI( qtu(s) );
            playlist_Add( THEPL, psz_uri, NULL,
                          PLAYLIST_APPEND | (first ? PLAYLIST_GO: PLAYLIST_PREPARSE),
                          PLAYLIST_END, true, pl_Unlocked );
            free( psz_uri );
            first = false;
            RecentsMRL::getInstance( p_intf )->addRecent( s );
        }
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
            setWindowFlags( windowFlags() | Qt::WindowStaysOnTopHint );
        else
            setWindowFlags( windowFlags() & ~Qt::WindowStaysOnTopHint );
        show(); /* necessary to apply window flags */
    }
}

void MainInterface::keyPressEvent( QKeyEvent *e )
{
    handleKeyPress( e );
}

void MainInterface::handleKeyPress( QKeyEvent *e )
{
    if( ( e->modifiers() &  Qt::ControlModifier ) && ( e->key() == Qt::Key_H )
          && !menuBar()->isVisible() )
    {
        toggleMinimalView( false );
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
#if 0
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
#endif
    QVLCMW::resizeEvent( event );
    msg_Warn( p_intf, "%i", size().height() );
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
        emit fullscreenInterfaceToggled( false );
    }
    else
    {
        showFullScreen();
        emit fullscreenInterfaceToggled( true );
    }

}

//moc doesn't know about #ifdef, so we have to build this method for every platform
void MainInterface::changeThumbbarButtons( int i_status)
{
#ifdef WIN32
    // Define an array of two buttons. These buttons provide images through an
    // image list and also provide tooltips.
    DWORD dwMask = THB_BITMAP | THB_FLAGS;

    THUMBBUTTON thbButtons[3];
    //prev
    thbButtons[0].dwMask = dwMask;
    thbButtons[0].iId = 0;
    thbButtons[0].iBitmap = 0;

    //play/pause
    thbButtons[1].dwMask = dwMask;
    thbButtons[1].iId = 1;

    //next
    thbButtons[2].dwMask = dwMask;
    thbButtons[2].iId = 2;
    thbButtons[2].iBitmap = 3;

    switch( i_status )
    {
        case  0:
        case  END_S:
            {
                thbButtons[0].dwFlags = THBF_HIDDEN;
                thbButtons[1].dwFlags = THBF_HIDDEN;
                thbButtons[2].dwFlags = THBF_HIDDEN;
                break;
            }
        case PLAYING_S:
            {
                thbButtons[0].dwFlags = THBF_ENABLED;
                thbButtons[1].dwFlags = THBF_ENABLED;
                thbButtons[2].dwFlags = THBF_ENABLED;
                thbButtons[1].iBitmap = 1;
                break;
            }
        case PAUSE_S:
            {
                //thbButtons[0].dwFlags = THBF_ENABLED;
                //thbButtons[1].dwFlags = THBF_ENABLED;
                //thbButtons[2].dwFlags = THBF_ENABLED;
                thbButtons[1].iBitmap = 2;
                break;
            }
    }

    HRESULT hr =  p_taskbl->vt->ThumbBarUpdateButtons(p_taskbl, GetForegroundWindow(), 3, thbButtons);
    if(S_OK != hr)
        msg_Err( p_intf, "ThumbBarUpdateButtons failed with error %08x", hr );
#else
    ;
#endif
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
