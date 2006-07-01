/*****************************************************************************
 * main_inteface.cpp : Main interface
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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

MainInterface::MainInterface( intf_thread_t *_p_intf ) : QMainWindow(),
                                                         p_intf( _p_intf )
{
    /* All UI stuff */
    QVLCFrame::fixStyle( this );
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


//    if( config_GetInt( p_intf, "embedded" ) )
    {
        videoWidget = new VideoWidget( p_intf );
        videoWidget->resize( 1,1 );
        ui.vboxLayout->insertWidget( 0, videoWidget );
    }
    resize( QSize( 500, 121 ) );
    i_saved_width = width();
    i_saved_height = height();

    //QVLCMenu::createMenuBar();

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
}

MainInterface::~MainInterface()
{
}

QSize MainInterface::sizeHint() const
{
    int i_width = __MAX( i_saved_width, p_intf->p_sys->p_video->i_video_width );
    return QSize( i_width, i_saved_height +
                             p_intf->p_sys->p_video->i_video_height );
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
