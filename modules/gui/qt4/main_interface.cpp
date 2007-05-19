/*****************************************************************************
 * main_interface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
#include "util/input_slider.hpp"
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

#include <assert.h>
#include <vlc_keys.h>
#include <vlc_vout.h>

#ifdef WIN32
    #define PREF_W 410
    #define PREF_H 121
#else
    #define PREF_W 450
    #define PREF_H 125
#endif

#define VISIBLE(i) (i && i->isVisible())

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
    /* Configuration */
    settings = new QSettings( "VideoLAN", "VLC" );
    settings->beginGroup( "MainWindow" );

    setWindowIcon( QApplication::windowIcon() );

    need_components_update = false;
    bgWidget = NULL; videoWidget = NULL; playlistWidget = NULL;
    embeddedPlaylistWasActive = videoIsActive = false;
 
    input_name = "";

    videoEmbeddedFlag = false;
    if( config_GetInt( p_intf, "embedded-video" ) ) videoEmbeddedFlag = true;

    alwaysVideoFlag = false;
    if( videoEmbeddedFlag && config_GetInt( p_intf, "qt-always-video" ))
        alwaysVideoFlag = true;

    playlistEmbeddedFlag = settings->value("playlist-embedded", true).toBool();
    advControlsEnabled= settings->value( "adv-controls", false ).toBool();
    visualSelectorEnabled= settings->value( "visual-selector", false ).toBool();

    /* UI */
    setVLCWindowsTitle();
    handleMainUi( settings );
    QVLCMenu::createMenuBar( this, p_intf, playlistEmbeddedFlag,
                             advControlsEnabled, visualSelectorEnabled );
    timeLabel = new QLabel( 0 );
    nameLabel = new QLabel( 0 );
    statusBar()->addWidget( nameLabel, 4 );
    statusBar()->addPermanentWidget( timeLabel, 1 );

    setFocusPolicy( Qt::StrongFocus );
    setAcceptDrops(true);

    /* Init input manager */
    MainInputManager::getInstance( p_intf );
    ON_TIMEOUT( updateOnTimer() );

    /* Volume control */
    CONNECT( ui.volumeSlider, valueChanged(int), this, updateVolume(int) );

    /* Connect the input manager to the GUI elements it manages */
    CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
             slider, setPosition( float,int, int ) );
    CONNECT( THEMIM->getIM(), positionUpdated( float, int, int ),
             this, setDisplay( float, int, int ) );

    /* Naming in the controller */
    CONNECT( THEMIM->getIM(), nameChanged( QString ), this,
             setName( QString ) );
    if( config_GetInt( p_intf, "qt-system-tray" ) )
    {
        CONNECT( THEMIM->getIM(), nameChanged( QString ), this,
                 updateSystrayTooltipName(QString));
    }
    if( config_GetInt( p_intf, "qt-name-in-title" ) )
    {
        CONNECT( THEMIM->getIM(), nameChanged( QString ), this,
             setVLCWindowsTitle( QString ) );
    }

    /* PLAY_STATUS */
    CONNECT( THEMIM->getIM(), statusChanged( int ), this, setStatus( int ) );
    CONNECT( THEMIM->getIM(), navigationChanged( int ),
             this, setNavigation(int) );
    if( config_GetInt( p_intf, "qt-system-tray" ) )
    {
        CONNECT( THEMIM->getIM(), statusChanged( int ), this,
                 updateSystrayTooltipStatus(int));
    }
    CONNECT( slider, sliderDragged( float ),
             THEMIM->getIM(), sliderUpdate( float ) );

    /* Buttons */
    CONNECT( ui.prevSectionButton, clicked(), THEMIM->getIM(),
             sectionPrev() );
    CONNECT( ui.nextSectionButton, clicked(), THEMIM->getIM(),
             sectionNext() );
    CONNECT( ui.menuButton, clicked(), THEMIM->getIM(),
             sectionMenu() );

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
    if( QSystemTrayIcon::isSystemTrayAvailable() &&
                        ( config_GetInt( p_intf, "qt-start-mininimized") == 1))
    {
        hide();
        createSystrayMenu();
    }
    if( QSystemTrayIcon::isSystemTrayAvailable() &&
                              ( config_GetInt( p_intf, "qt-system-tray") == 1))
            createSystrayMenu();

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
    settings->setValue( "adv-controls", advControlsEnabled );
    settings->setValue( "pos", pos() );
    settings->endGroup();
    delete settings;
    p_intf->b_interaction = VLC_FALSE;
    var_DelCallback( p_intf, "interaction", InteractCallback, this );

    p_intf->pf_request_window = NULL;
    p_intf->pf_release_window = NULL;
    p_intf->pf_control_window = NULL;
}

void MainInterface::setVLCWindowsTitle( QString aTitle )
{
    if( aTitle.isEmpty() )
    {
        this->setWindowTitle( qtr( "VLC media player" ) );
    }
    else
    {
        this->setWindowTitle( aTitle + " - " + qtr( "VLC media player" ) );
    }
}

void MainInterface::handleMainUi( QSettings *settings )
{
    QWidget *main = new QWidget( this );
    setCentralWidget( main );
    ui.setupUi( centralWidget() );

    slider = new InputSlider( Qt::Horizontal, NULL );
    ui.vboxLayout->insertWidget( 0, slider );
    ui.discFrame->hide();
    BUTTON_SET_IMG( ui.prevSectionButton, "", previous.png, "" );
    BUTTON_SET_IMG( ui.nextSectionButton, "", next.png, "" );
    BUTTON_SET_IMG( ui.menuButton, "", previous.png, "" );

    BUTTON_SET_ACT_I( ui.prevButton, "" , previous.png,
                      qtr("Previous"), prev() );
    BUTTON_SET_ACT_I( ui.nextButton, "", next.png, qtr("Next"), next() );
    BUTTON_SET_ACT_I( ui.playButton, "", play.png, qtr("Play"), play() );
    BUTTON_SET_ACT_I( ui.stopButton, "", stop.png, qtr("Stop"), stop() );

    /* Volume */
    ui.volMuteLabel->setPixmap( QPixmap( ":/pixmaps/volume-low.png" ) );
    ui.volumeSlider->setMaximum( 100 );
    ui.volMuteLabel->setToolTip( qtr( "Mute" ) );
    VolumeClickHandler *h = new VolumeClickHandler( p_intf, this );
    ui.volMuteLabel->installEventFilter(h);
    ui.volumeSlider->setFocusPolicy( Qt::NoFocus );

    BUTTON_SET_IMG( ui.playlistButton, "" , playlist_icon.png,
                        playlistEmbeddedFlag ?  qtr( "Show playlist" ) :
                                                qtr( "Open playlist" ) );
    BUTTONACT( ui.playlistButton, playlist() );

    /* Set initial size */
    resize ( PREF_W, PREF_H );

    addSize = QSize( ui.vboxLayout->margin() * 2, PREF_H );

    advControls = new ControlsWidget( p_intf );
    ui.vboxLayout->insertWidget( 0, advControls );
    advControls->updateGeometry();
    if( !advControlsEnabled ) advControls->hide();
    need_components_update = true;

    visualSelector = new VisualSelector( p_intf );
    ui.vboxLayout->insertWidget( 0, visualSelector );
    visualSelector->hide();

    if( alwaysVideoFlag )
    {
        bgWidget = new BackgroundWidget( p_intf );
        bgWidget->widgetSize = settings->value( "backgroundSize",
                                                QSize( 200, 200 ) ).toSize();
        bgWidget->resize( bgWidget->widgetSize );
        bgWidget->updateGeometry();
        ui.vboxLayout->insertWidget( 0, bgWidget );
    }

    if( videoEmbeddedFlag )
    {
        videoWidget = new VideoWidget( p_intf );
        videoWidget->widgetSize = QSize( 1, 1 );
        videoWidget->resize( videoWidget->widgetSize );
        ui.vboxLayout->insertWidget( 0, videoWidget );

        p_intf->pf_request_window  = ::DoRequest;
        p_intf->pf_release_window  = ::DoRelease;
        p_intf->pf_control_window  = ::DoControl;
    }
    setMinimumSize( PREF_W, addSize.height() );
}

void MainInterface::createSystrayMenu()
{
    QIcon iconVLC =  QIcon( QPixmap( ":/vlc128.png" ) );
    sysTray = new QSystemTrayIcon( iconVLC, this );
    systrayMenu = new QMenu( qtr( "VLC media player" ), this );
    systrayMenu->setIcon( iconVLC );
    sysTray->setToolTip( qtr( "VLC media player" ));
    QVLCMenu::updateSystrayMenu( this, p_intf, true );
    sysTray->show();
    CONNECT( sysTray, activated(  QSystemTrayIcon::ActivationReason ),
            this, handleSystrayClick( QSystemTrayIcon::ActivationReason ) );
}

void MainInterface::updateSystrayMenu( int status )
{
    QVLCMenu::updateSystrayMenu( this, p_intf ) ;
}

void MainInterface::toggleUpdateSystrayMenu()
{
    QVLCMenu::updateSystrayMenu( this, p_intf );
    toggleVisible();
}

void MainInterface::handleSystrayClick( QSystemTrayIcon::ActivationReason reason )
{
    switch( reason )
    {
        case QSystemTrayIcon::Trigger:
            this->toggleVisible(); break;
        case QSystemTrayIcon::MiddleClick:
            sysTray->showMessage( qtr( "VLC media player" ),
                    qtr( "Control menu for the player" ),
                    QSystemTrayIcon::Information, 4000 );
            break;
    }
}


void MainInterface::updateSystrayTooltipName( QString name )
{
    if( name.isEmpty() )
    {
        sysTray->setToolTip( qtr( "VLC media player" ) );
    }
    else
    {
        sysTray->setToolTip( name );
    }
}

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
/**********************************************************************
 * Handling of the components
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
    if( VISIBLE( advControls) )
    {
        height += advControls->sizeHint().height();
    }
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
//            bgWidget->hide();
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
//        videoWidget->updateGeometry(); /// FIXME: Needed ?
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
        ;//playlistWidget->show();
    else if( bgWidget )
        ;//bgWidget->show();

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

void MainInterface::advanced()
{
    if( !VISIBLE( advControls ) )
    {
        advControls->show();
        advControlsEnabled = true;
    }
    else
    {
        advControls->hide();
        advControlsEnabled = false;
    }
    doComponentsUpdate();
}

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

void MainInterface::playlist()
{
    // Toggle the playlist dialog
    if( !playlistEmbeddedFlag )
    {
        if( playlistWidget )
        {
            /// \todo Destroy it
        }
        THEDP->playlistDialog();
        return;
    }

    if( !playlistWidget )
    {
        PlaylistDialog::killInstance();
        playlistWidget = new PlaylistWidget( p_intf );
        ui.vboxLayout->insertWidget( 0, playlistWidget );
        playlistWidget->widgetSize = settings->value( "playlistSize",
                                               QSize( 650, 310 ) ).toSize();
        playlistWidget->hide();
    }
    if( VISIBLE( playlistWidget ) )
    {
        playlistWidget->hide();
        if( videoIsActive )
        {
            videoWidget->widgetSize = savedVideoSize;
            videoWidget->resize( videoWidget->widgetSize );
            videoWidget->updateGeometry();
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

/* Video widget cannot do this synchronously as it runs in another thread */
/* Well, could it, actually ? Probably dangerous ... */
void MainInterface::doComponentsUpdate()
{
    calculateInterfaceSize();
    resize( mainSize );
}

void MainInterface::undockPlaylist()
{
    if( playlistWidget )
    {
        playlistWidget->hide();
        playlistWidget->deleteLater();
        ui.vboxLayout->removeWidget( playlistWidget );
        playlistWidget = NULL;
        playlistEmbeddedFlag = false;

        menuBar()->clear();
        QVLCMenu::createMenuBar( this, p_intf, false, advControlsEnabled,
                                 visualSelectorEnabled);

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

void MainInterface::customEvent( QEvent *event )
{
    if( event->type() == PLDockEvent_Type )
    {
        PlaylistDialog::killInstance();
        playlistEmbeddedFlag = true;
        menuBar()->clear();
        QVLCMenu::createMenuBar(this, p_intf, true, advControlsEnabled,
                                visualSelectorEnabled);
        playlist();
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


/************************************************************************
 * D&D
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
 * Other stuff
 ************************************************************************/
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

void MainInterface::stop()
{
    THEMIM->stop();
}

void MainInterface::play()
{
    if( playlist_IsEmpty(THEPL) )
    {
        /* The playlist is empty, open a file requester */
        THEDP->openFileDialog();
        setStatus( 0 );
        return;
    }
    THEMIM->togglePlayPause();
}

void MainInterface::prev()
{
    THEMIM->prev();
}

void MainInterface::next()
{
    THEMIM->next();
}

void MainInterface::setDisplay( float pos, int time, int length )
{
    char psz_length[MSTRTIME_MAX_SIZE], psz_time[MSTRTIME_MAX_SIZE];
    secstotimestr( psz_length, length );
    secstotimestr( psz_time, time );
    QString title;
    title.sprintf( "%s/%s", psz_time, psz_length );
    timeLabel->setText( " "+title+" " );
}

void MainInterface::setName( QString name )
{
    input_name = name;
    nameLabel->setText( " " + name+" " );

}

void MainInterface::setStatus( int status )
{
    if( status == 1 ) // Playing
        ui.playButton->setIcon( QIcon( ":/pixmaps/pause.png" ) );
    else
        ui.playButton->setIcon( QIcon( ":/pixmaps/play.png" ) );
    updateSystrayMenu( status );
}

#define HELP_MENU N_("Menu")
#define HELP_PCH N_("Previous chapter")
#define HELP_NCH N_("Next chapter")
#define HELP_PTR N_("Previous track")
#define HELP_NTR N_("Next track")

void MainInterface::setNavigation( int navigation )
{
    // 1 = chapter, 2 = title, 0 = no
    if( navigation == 0 )
    {
        ui.discFrame->hide();
    } else if( navigation == 1 ) {
        ui.prevSectionButton->show();
        ui.prevSectionButton->setToolTip( qfu(HELP_PCH) );
        ui.nextSectionButton->show();
        ui.nextSectionButton->setToolTip( qfu(HELP_NCH) );
        ui.menuButton->show();
        ui.discFrame->show();
    } else {
        ui.prevSectionButton->show();
        ui.prevSectionButton->setToolTip( qfu(HELP_PCH) );
        ui.nextSectionButton->show();
        ui.nextSectionButton->setToolTip( qfu(HELP_NCH) );
        ui.menuButton->hide();
        ui.discFrame->show();
    }
}

static bool b_my_volume;

void MainInterface::updateOnTimer()
{
    /* \todo Make this event-driven */
    advControls->enableInput( THEMIM->getIM()->hasInput() );
    advControls->enableVideo( THEMIM->getIM()->hasVideo() );

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

    audio_volume_t i_volume;
    aout_VolumeGet( p_intf, &i_volume );
    i_volume = (i_volume *  200 )/ AOUT_VOLUME_MAX ;
    int i_gauge = ui.volumeSlider->value();
    b_my_volume = false;
    if( i_volume - i_gauge > 1 || i_gauge - i_volume > 1 )
    {
        b_my_volume = true;
        ui.volumeSlider->setValue( i_volume );
        b_my_volume = false;
    }
}

void MainInterface::closeEvent( QCloseEvent *e )
{
    hide();
    vlc_object_kill( p_intf );
}

void MainInterface::updateVolume( int sliderVolume )
{
    if( !b_my_volume )
    {
        int i_res = sliderVolume * AOUT_VOLUME_MAX /
                            (2*ui.volumeSlider->maximum() );
        aout_VolumeSet( p_intf, i_res );
    }
}

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
