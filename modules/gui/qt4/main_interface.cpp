/*****************************************************************************
 * main_inteface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#include "qt4.hpp"
#include "main_interface.hpp"
#include "input_manager.hpp"
#include "util/input_slider.hpp"
#include "util/qvlcframe.hpp"
#include "dialogs_provider.hpp"
#include "components/interface_widgets.hpp"
#include "dialogs/playlist.hpp"
#include "menus.hpp"

#include <QCloseEvent>
#include <QPushButton>
#include <QStatusBar>
#include <QKeyEvent>

#include <assert.h>
#include <vlc_keys.h>
#include <vlc/vout.h>

#ifdef WIN32
    #define PREF_W 410
    #define PREF_H 121
#else
    #define PREF_W 450
    #define PREF_H 125
#endif

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
    settings = new QSettings( "VideoLAN", "VLC" );
    settings->beginGroup( "MainWindow" );

    need_components_update = false;

    setWindowTitle( QString::fromUtf8( _("VLC media player") ) );
    handleMainUi( settings );

    QVLCMenu::createMenuBar( menuBar(), p_intf );
    /* Status bar */
    timeLabel = new QLabel( 0 );
    nameLabel = new QLabel( 0 );
    statusBar()->addWidget( nameLabel, 4 );
    statusBar()->addPermanentWidget( timeLabel, 1 );

    setFocusPolicy( Qt::StrongFocus );

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
    CONNECT( THEMIM->getIM(), nameChanged( QString ), this,setName( QString ) );
    CONNECT( THEMIM->getIM(), statusChanged( int ), this, setStatus( int ) );
    CONNECT( slider, sliderDragged( float ),
             THEMIM->getIM(), sliderUpdate( float ) );

    /* Actions */
    BUTTONACT( ui.playButton, play() );
    BUTTONACT( ui.stopButton, stop() );
    BUTTONACT( ui.nextButton, next() );
    BUTTONACT( ui.prevButton, prev() );
    BUTTONACT( ui.playlistButton, playlist() );

    var_Create( p_intf, "interaction", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "interaction", InteractCallback, this );
    p_intf->b_interaction = VLC_TRUE;
}

MainInterface::~MainInterface()
{
    /// \todo Save everything
    p_intf->b_interaction = VLC_FALSE;
    var_DelCallback( p_intf, "interaction", InteractCallback, this );

    p_intf->pf_request_window = NULL;
    p_intf->pf_release_window = NULL;
    p_intf->pf_control_window = NULL;
}

void MainInterface::handleMainUi( QSettings *settings )
{
    QWidget *main = new QWidget( this );
    setCentralWidget( main );
    ui.setupUi( centralWidget() );

    slider = new InputSlider( Qt::Horizontal, NULL );
    ui.hboxLayout->insertWidget( 0, slider );

    ui.prevButton->setText( "" );
    ui.nextButton->setText( "" );
    ui.playButton->setText( "" );
    ui.stopButton->setText( "" );
    ui.prevButton->setIcon( QIcon( ":/pixmaps/previous.png" ) );
    ui.nextButton->setIcon( QIcon( ":/pixmaps/next.png" ) );
    ui.playButton->setIcon( QIcon( ":/pixmaps/play.png" ) );
    ui.stopButton->setIcon( QIcon( ":/pixmaps/stop.png" ) );

    /* Volume */
    ui.volLowLabel->setPixmap( QPixmap( ":/pixmaps/volume-low.png" ) );
    ui.volHighLabel->setPixmap( QPixmap( ":/pixmaps/volume-high.png" ) );
    ui.volumeSlider->setMaximum( 100 );

    VolumeClickHandler *h = new VolumeClickHandler( this );
    ui.volLowLabel->installEventFilter(h);
    ui.volHighLabel->installEventFilter(h);
    ui.volumeSlider->setFocusPolicy( Qt::NoFocus );

    ui.playlistButton->setText( "" );
    ui.playlistButton->setIcon( QIcon( ":/pixmaps/volume-low.png" ) );

    /* Fetch configuration from settings and vlc config */
    videoEmbeddedFlag = false;
    if( config_GetInt( p_intf, "embedded-video" ) )
        videoEmbeddedFlag = true;

    playlistEmbeddedFlag = true;
    /// \todo fetch playlist settings

    resize ( PREF_W, PREF_H );

    if( videoEmbeddedFlag )
    {
        videoWidget = new VideoWidget( p_intf );
        videoWidget->widgetSize = QSize( 1, 1 );
        videoWidget->resize( videoWidget->widgetSize );
        ui.vboxLayout->insertWidget( 0, videoWidget );

        p_intf->pf_request_window  = ::DoRequest;
        p_intf->pf_release_window  = ::DoRelease;
        p_intf->pf_control_window  = ::DoControl;

        if( config_GetInt( p_intf, "qt-always-video" ))
        {
            bgWidget = new BackgroundWidget( p_intf );
            bgWidget->widgetSize = settings->value( "backgroundSize",
                                                QSize( 200, 200 ) ).toSize();
            ui.vboxLayout->insertWidget( 0, bgWidget ); 
            bgWidget->hide();
        }
    }

    // Size for fixed elements
    addSize = QSize( ui.vboxLayout->margin() * 2, PREF_H );

    calculateInterfaceSize();
    resize( mainSize );
    /// \bug still needed ?
    mainSize = size();

    setMinimumSize( PREF_W, addSize.height() );
}

/**********************************************************************
 * Handling of the components
 **********************************************************************/
void MainInterface::calculateInterfaceSize()
{
    int width = 0, height = 0;
    if( bgWidget->isVisible() )
    {
        width += bgWidget->widgetSize.width();
        height += bgWidget->widgetSize.height();
        assert( !playlistWidget->isVisible() );
    }
    if( playlistWidget->isVisible() )
    {
        width += playlistWidget->widgetSize.width();
        height += playlistWidget->widgetSize.height();
    }
    mainSize.setWidth( width + videoWidget->widgetSize.width() + 
                                               addSize.width() );
    mainSize.setHeight( height + videoWidget->widgetSize.height() +
                                                 addSize.height() );
}

/// To update !!
void MainInterface::resizeEvent( QResizeEvent *e )
{
    videoWidget->widgetSize.setHeight( e->size().height() - addSize.height() );
    videoWidget->widgetSize.setWidth( e->size().width() - addSize.width() );
    p_intf->p_sys->p_video->updateGeometry() ;
}

void *MainInterface::requestVideo( vout_thread_t *p_nvout, int *pi_x,
                                   int *pi_y, unsigned int *pi_width,
                                   unsigned int *pi_height )
{
    void *ret = videoWidget->request( p_nvout,pi_x, pi_y, pi_width, pi_height );
    videoWidget->widgetSize = QSize( *pi_width, *pi_height );
    videoWidget->updateGeometry(); /// FIXME: Needed ?
    need_components_update = true;
    return ret;
}

void MainInterface::releaseVideo( void *p_win )
{
    videoWidget->release( p_win );
    videoWidget->widgetSize = QSize( 1, 1 );
    videoWidget->updateGeometry();
    need_components_update = true;
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
//          if( !i_width && p_vout ) i_width = p_vout->i_window_width;
//          if( !i_height && p_vout ) i_height = p_vout->i_window_height;
            videoWidget->widgetSize = QSize( i_width, i_height );
            videoWidget->updateGeometry();
            need_components_update = true;
            i_ret = VLC_SUCCESS;
            break;
        }
        case VOUT_SET_STAY_ON_TOP:
        default:
            msg_Warn( p_intf, "unsupported control query" );
            break;
    }
    return i_ret;
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
                                               QSize( 600, 300 ) ).toSize();
    }
    /// Todo, reset its size ?
    if( playlistWidget->isVisible() ) playlistWidget->show();
    else playlistWidget->hide();

    calculateInterfaceSize();
    resize( mainSize );
}

void MainInterface::doComponentsUpdate()
{

}

/************************************************************************
 * Other stuff
 ************************************************************************/
void MainInterface::keyPressEvent( QKeyEvent *e )
{
    int i_vlck = 0;
    /* Handle modifiers */
    if( e->modifiers()& Qt::ShiftModifier ) i_vlck |= KEY_MODIFIER_SHIFT;
    if( e->modifiers()& Qt::AltModifier ) i_vlck |= KEY_MODIFIER_ALT;
    if( e->modifiers()& Qt::ControlModifier ) i_vlck |= KEY_MODIFIER_CTRL;
    if( e->modifiers()& Qt::MetaModifier ) i_vlck |= KEY_MODIFIER_META;

    bool found = false;
    /* Look for some special keys */
#define HANDLE( qt, vk ) case Qt::qt : i_vlck |= vk; found = true;break
    switch( e->key() )
    {
        HANDLE( Key_Left, KEY_LEFT );
        HANDLE( Key_Right, KEY_RIGHT );
        HANDLE( Key_Up, KEY_UP );
        HANDLE( Key_Down, KEY_DOWN );
        HANDLE( Key_Space, KEY_SPACE );
        HANDLE( Key_Escape, KEY_ESC );
        HANDLE( Key_Enter, KEY_ENTER );
        HANDLE( Key_F1, KEY_F1 );
        HANDLE( Key_F2, KEY_F2 );
        HANDLE( Key_F3, KEY_F3 );
        HANDLE( Key_F4, KEY_F4 );
        HANDLE( Key_F5, KEY_F5 );
        HANDLE( Key_F6, KEY_F6 );
        HANDLE( Key_F7, KEY_F7 );
        HANDLE( Key_F8, KEY_F8 );
        HANDLE( Key_F9, KEY_F9 );
        HANDLE( Key_F10, KEY_F10 );
        HANDLE( Key_F11, KEY_F11 );
        HANDLE( Key_F12, KEY_F12 );
        HANDLE( Key_PageUp, KEY_PAGEUP );
        HANDLE( Key_PageDown, KEY_PAGEDOWN );
        HANDLE( Key_Home, KEY_HOME );
        HANDLE( Key_End, KEY_END );
        HANDLE( Key_Insert, KEY_INSERT );
        HANDLE( Key_Delete, KEY_DELETE );

    }
    if( !found )
    {
        /* Force lowercase */
        if( e->key() >= Qt::Key_A && e->key() <= Qt::Key_Z )
            i_vlck += e->key() + 32;
        /* Rest of the ascii range */
        else if( e->key() >= Qt::Key_Space && e->key() <= Qt::Key_AsciiTilde )
            i_vlck += e->key();
    }
    if( i_vlck >= 0 )
    {
        var_SetInteger( p_intf->p_libvlc, "key-pressed", i_vlck );
        e->accept();
    }
    else
        e->ignore();
}

void MainInterface::stop()
{
    playlist_Stop( THEPL );
}
void MainInterface::play()
{
    if( !THEPL->i_size || !THEPL->i_enabled )
    {
        /* The playlist is empty, open a file requester */
        THEDP->openDialog();
        setStatus( 0 );
        return;
    }
    THEMIM->togglePlayPause();
}
void MainInterface::prev()
{
    playlist_Prev( THEPL );
}
void MainInterface::next()
{
    playlist_Next( THEPL );
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
    nameLabel->setText( " " + name+" " );
}

void MainInterface::setStatus( int status )
{
    if( status == 1 ) // Playing
        ui.playButton->setIcon( QIcon( ":/pixmaps/pause.png" ) );
    else
        ui.playButton->setIcon( QIcon( ":/pixmaps/play.png" ) );
}

static bool b_my_volume;

void MainInterface::updateOnTimer()
{
    if( p_intf->b_die )
    {
        QApplication::closeAllWindows();
        DialogsProvider::killInstance();
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
    p_intf->b_die = VLC_TRUE;
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

    MainInterface *p_interface = (MainInterface*)param;
    DialogEvent *event = new DialogEvent( INTF_DIALOG_INTERACTION, 0, p_arg );
    QApplication::postEvent( THEDP, static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}
