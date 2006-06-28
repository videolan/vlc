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

#include "main_interface.hpp"
#include "input_manager.hpp"
#include "util/input_slider.hpp"
#include "util/qvlcframe.hpp"
#include "dialogs_provider.hpp"
#include <QCloseEvent>
#include <assert.h>
#include <QPushButton>

MainInterface::MainInterface( intf_thread_t *_p_intf ) : QMainWindow(), p_intf( _p_intf )
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

    resize( QSize( 450, 80 ) );

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

void MainInterface::stop()
{
    /// \todo store playlist globally
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist ) return;
    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );
}
void MainInterface::play()
{
    /// \todo store playlist globally
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist ) return;
    playlist_Play( p_playlist );
    vlc_object_release( p_playlist );
}
void MainInterface::prev()
{
    /// \todo store playlist globally
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist ) return;
    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );
}
void MainInterface::next()
{
    /// \todo store playlist globally
    playlist_t *p_playlist = (playlist_t *)vlc_object_find( p_intf,
                                VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_playlist ) return;
    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );
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
