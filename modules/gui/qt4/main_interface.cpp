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
#include "components/video_widget.hpp"
#include <QCloseEvent>
#include <assert.h>
#include <QPushButton>
#include <QStatusBar>
#include <QKeyEvent>
#include "menus.hpp"
#include <vlc_keys.h>

#ifdef WIN32
    #define PREF_W 410
    #define PREF_H 121
#else
    #define PREF_W 450
    #define PREF_H 125
#endif

static int InteractCallback( vlc_object_t *, const char *, vlc_value_t,
                             vlc_value_t, void *);

MainInterface::MainInterface( intf_thread_t *_p_intf ) : QVLCMW( _p_intf )
{
    /* All UI stuff */
    QWidget *main = new QWidget( this );
    setCentralWidget( main );
    setWindowTitle( QString::fromUtf8( _("VLC media player") ) );
    ui.setupUi( centralWidget() );

    setFocusPolicy( Qt::StrongFocus );

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
    ui.volLowLabel->setPixmap( QPixmap( ":/pixmaps/volume-low.png" ) );
    ui.volHighLabel->setPixmap( QPixmap( ":/pixmaps/volume-high.png" ) );
    ui.volumeSlider->setMaximum( 100 );
    ui.playlistButton->setText( "" );
    ui.playlistButton->setIcon( QIcon( ":/pixmaps/volume-low.png" ) );

    VolumeClickHandler *h = new VolumeClickHandler( this );
    ui.volLowLabel->installEventFilter(h);
    ui.volHighLabel->installEventFilter(h);

    ui.volumeSlider->setFocusPolicy( Qt::NoFocus );

    QVLCMenu::createMenuBar( menuBar(), p_intf );

    timeLabel = new QLabel( 0 );
    nameLabel = new QLabel( 0 );
    statusBar()->addWidget( nameLabel, 4 );
    statusBar()->addPermanentWidget( timeLabel, 1 );

    resize ( PREF_W, PREF_H );
    if( config_GetInt( p_intf, "embedded" ) )
    {
        videoWidget = new VideoWidget( p_intf, config_GetInt( p_intf,
                                         "qt-always-video" ) ? true:false );
        if( config_GetInt( p_intf, "qt-always-video" ) )
        {
            QSettings settings( "VideoLAN", "VLC" );
            settings.beginGroup( "MainWindow" );
            videoSize = settings.value( "videoSize", QSize( 200, 200 ) ).
                                                toSize();
        }
        else
            videoSize = QSize( 1,1 );
        videoWidget->resize( videoSize );
        ui.vboxLayout->insertWidget( 0, videoWidget );
    }
    readSettings( "MainWindow" );

    addSize = QSize( ui.vboxLayout->margin() * 2, PREF_H );
    mainSize.setWidth( videoSize.width() + addSize.width() );
    mainSize.setHeight( videoSize.height() + addSize.height() );
    resize( mainSize );
    mainSize = size();

    setMinimumSize( PREF_W, addSize.height() );

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
    CONNECT( ui.playlistButton, clicked(), THEDP, playlistDialog() );

    var_Create( p_intf, "interaction", VLC_VAR_ADDRESS );
    var_AddCallback( p_intf, "interaction", InteractCallback, this );
    p_intf->b_interaction = VLC_TRUE;
}

MainInterface::~MainInterface()
{
    writeSettings( "MainWindow" );
    if( config_GetInt( p_intf, "qt-always-video" ) )
    {
        QSettings s("VideoLAN", "VLC" );
        s.beginGroup( "MainWindow" );
        s.setValue( "videoSize", videoSize );
        s.endGroup();
    }
    p_intf->b_interaction = VLC_FALSE;
    var_DelCallback( p_intf, "interaction", InteractCallback, this );
}

void MainInterface::resizeEvent( QResizeEvent *e )
{
    videoSize.setHeight( e->size().height() - addSize.height() );
    videoSize.setWidth( e->size().width() - addSize.width() );
    p_intf->p_sys->p_video->updateGeometry() ;
}

void MainInterface::keyPressEvent( QKeyEvent *e )
{
    int i_vlck = 0;
    /* Handle modifiers */
    if( e->modifiers()& Qt::ShiftModifier ) i_vlck |= KEY_MODIFIER_SHIFT;
    if( e->modifiers()& Qt::AltModifier ) i_vlck |= KEY_MODIFIER_ALT;
    if( e->modifiers()& Qt::ControlModifier ) i_vlck |= KEY_MODIFIER_CTRL;
    if( e->modifiers()& Qt::MetaModifier ) i_vlck |= KEY_MODIFIER_META;

    fprintf( stderr, "After modifiers %x\n", i_vlck );
    bool found = false;
    fprintf( stderr, "Qt %x\n", e->key() );
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
    fprintf( stderr, "After keys %x\n", i_vlck );
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
