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

static int InteractCallback( vlc_object_t *, const char *, vlc_value_t,
                             vlc_value_t, void *);

MainInterface::MainInterface( intf_thread_t *_p_intf ) : QVLCMW( _p_intf )
{
    /* All UI stuff */
    QWidget *main = new QWidget( this );
    setCentralWidget( main );
    setWindowTitle( QString::fromUtf8( _("VLC media player") ) );
    ui.setupUi( centralWidget() );

    slider = new InputSlider( Qt::Horizontal, ui.sliderBox );
    QVBoxLayout *box_layout = new QVBoxLayout();
    box_layout->addWidget( slider );
    ui.sliderBox->setLayout( box_layout );
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

    //QVLCMenu::createMenuBar();

    resize (500, 131 );
    fprintf( stderr, "Before creating the video widget, size is %ix%i\n", size().width(), size().height() );
//    if( config_GetInt( p_intf, "embedded" ) )

    {
        videoWidget = new VideoWidget( p_intf );
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
    fprintf( stderr, "Margin : %i\n",ui.vboxLayout->margin() );
    readSettings( "MainWindow" );

    addSize = QSize( ui.vboxLayout->margin() * 2, 131 );
    
    if( config_GetInt( p_intf, "qt-always-video" ) )
        mainSize = videoSize + addSize;
    else
        mainSize = QSize( 500,131 );
        resize( 500,131 );
    resize( mainSize );
    mainSize = size();

    fprintf( stderr, "Size is %ix%i - Video %ix%i\n", mainSize.width(), mainSize.height(), videoSize.width(), videoSize.height() );

    fprintf( stderr, "Additional size around video %ix%i", addSize.width(), addSize.height() );
    setMinimumSize( 500, addSize.height() );

    /* Init input manager */
    MainInputManager::getInstance( p_intf );

    /* Get timer updates */
    connect( DialogsProvider::getInstance(NULL)->fixed_timer,
             SIGNAL( timeout() ), this, SLOT(updateOnTimer() ) );

    /* Connect the input manager to the GUI elements it manages */
    connect( MainInputManager::getInstance( p_intf )->getIM(),
             SIGNAL(positionUpdated( float, int, int ) ),
             slider, SLOT( setPosition( float,int, int ) ) );
    connect( slider, SIGNAL( sliderDragged( float ) ),
             MainInputManager::getInstance( p_intf )->getIM(),
             SLOT( sliderUpdate( float ) ) );
    connect( MainInputManager::getInstance( p_intf )->getIM(),
             SIGNAL( positionUpdated( float, int, int ) ),
             this, SLOT( setDisplay( float, int, int ) ) );

    /* Actions */
    connect( ui.playButton, SLOT( clicked() ), this, SLOT( play() ) );
    connect( ui.stopButton, SLOT( clicked() ), this, SLOT( stop() ) );
    connect( ui.nextButton, SLOT( clicked() ), this, SLOT( next() ) );
    connect( ui.prevButton, SLOT( clicked() ), this, SLOT( prev() ) );

    connect( ui.playlistButton, SLOT(clicked() ), 
             DialogsProvider::getInstance( p_intf ), SLOT( playlistDialog() ) );

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
    fprintf( stderr, "Resized to %ix%i\n", e->size().width(), e->size().height() );

     fprintf( stderr, "MI constraints %ix%i -> %ix%i\n",
                             p_intf->p_sys->p_mi->minimumSize().width(),
                             p_intf->p_sys->p_mi->minimumSize().height(),
                               p_intf->p_sys->p_mi->maximumSize().width(),
                               p_intf->p_sys->p_mi->maximumSize().height() );
     
        videoSize.setHeight( e->size().height() - addSize.height() );
        videoSize.setWidth( e->size().width() - addSize.width() );
    p_intf->p_sys->p_video->updateGeometry() ;
}

void MainInterface::stop()
{
    playlist_Stop( THEPL );
}
void MainInterface::play()
{
    playlist_Play( THEPL );
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
    ui.sliderBox->setTitle( title );
}

void MainInterface::updateOnTimer()
{
    if( p_intf->b_die )
    {
        QApplication::quit();
    }
}

void MainInterface::closeEvent( QCloseEvent *e )
{
    hide();
    p_intf->b_die = VLC_TRUE;
}

static int InteractCallback( vlc_object_t *p_this,
                             const char *psz_var, vlc_value_t old_val,
                             vlc_value_t new_val, void *param )
{
    intf_dialog_args_t *p_arg = new intf_dialog_args_t;
    p_arg->p_dialog = (interaction_dialog_t *)(new_val.p_address);
    
    MainInterface *p_interface = (MainInterface*)param;
    DialogEvent *event = new DialogEvent( INTF_DIALOG_INTERACTION, 0, p_arg );
    QApplication::postEvent( DialogsProvider::getInstance( NULL ),
                                             static_cast<QEvent*>(event) );
    return VLC_SUCCESS;
}
