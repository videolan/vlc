/*****************************************************************************
 * Controller_widget.cpp : Controller Widget for the controllers
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
 * $Id$
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
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

#include "controller_widget.hpp"
#include "controller.hpp"

#include "input_manager.hpp"         /* Get notification of Volume Change */
#include "util/input_slider.hpp"     /* SoundSlider */

#include <vlc_aout.h>                /* Volume functions */

#include <QLabel>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QMenu>
#include <QWidgetAction>
#include <QMouseEvent>

SoundWidget::SoundWidget( QWidget *_parent, intf_thread_t * _p_intf,
                          bool b_shiny, bool b_special )
                         : QWidget( _parent ), p_intf( _p_intf),
                           b_my_volume( false )
{
    /* We need a layout for this widget */
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setSpacing( 0 ); layout->setMargin( 0 );

    /* We need a Label for the pix */
    volMuteLabel = new QLabel;
    volMuteLabel->setPixmap( QPixmap( ":/toolbar/volume-medium" ) );

    /* We might need a subLayout too */
    QVBoxLayout *subLayout;

    volMuteLabel->installEventFilter( this );

    /* Normal View, click on icon mutes */
    if( !b_special )
    {
        volumeMenu = NULL; subLayout = NULL;
        volumeControlWidget = NULL;
    }
    else
    {
        /* Special view, click on button shows the slider */
        b_shiny = false;

        volumeControlWidget = new QFrame;
        subLayout = new QVBoxLayout( volumeControlWidget );
        subLayout->setLayoutMargins( 4, 4, 4, 4, 4 );
        volumeMenu = new QMenu( this );

        QWidgetAction *widgetAction = new QWidgetAction( volumeControlWidget );
        widgetAction->setDefaultWidget( volumeControlWidget );
        volumeMenu->addAction( widgetAction );
    }

    /* And add the label */
    layout->addWidget( volMuteLabel );

    /* Slider creation: shiny or clean */
    if( b_shiny )
    {
        volumeSlider = new SoundSlider( this,
            config_GetInt( p_intf, "volume-step" ),
            config_GetInt( p_intf, "qt-volume-complete" ),
            config_GetPsz( p_intf, "qt-slider-colours" ) );
    }
    else
    {
        volumeSlider = new QSlider( NULL );
        volumeSlider->setOrientation( b_special ? Qt::Vertical
                                                : Qt::Horizontal );
        volumeSlider->setMaximum( config_GetInt( p_intf, "qt-volume-complete" )
                                  ? 400 : 200 );
    }
    if( volumeSlider->orientation() ==  Qt::Horizontal )
    {
        volumeSlider->setMaximumSize( QSize( 200, 40 ) );
        volumeSlider->setMinimumSize( QSize( 85, 30 ) );
    }

    volumeSlider->setFocusPolicy( Qt::NoFocus );
    if( b_special )
        subLayout->addWidget( volumeSlider );
    else
        layout->addWidget( volumeSlider, 0, Qt::AlignBottom  );

    /* Set the volume from the config */
    volumeSlider->setValue( qRound( ( (qreal)config_GetInt( p_intf, "volume" ) ) *
                              VOLUME_MAX / (AOUT_VOLUME_MAX/2) ) );

    /* Force the update at build time in order to have a muted icon if needed */
    updateVolume( volumeSlider->value() );

    /* Volume control connection */
    CONNECT( volumeSlider, valueChanged( int ), this, updateVolume( int ) );
    CONNECT( THEMIM, volumeChanged( void ), this, updateVolume( void ) );
}

SoundWidget::~SoundWidget()
{
    delete volumeSlider;
    delete volumeControlWidget;
}

void SoundWidget::updateVolume( int i_sliderVolume )
{
    if( !b_my_volume )
    {
        int i_res = i_sliderVolume  * (AOUT_VOLUME_MAX / 2) / VOLUME_MAX;
        playlist_t *p_playlist = pl_Hold( p_intf );
        aout_VolumeSet( p_playlist, i_res );
        pl_Release( p_intf );
    }
    if( i_sliderVolume == 0 )
    {
        volMuteLabel->setPixmap( QPixmap(":/toolbar/volume-muted" ) );
        volMuteLabel->setToolTip(qfu(vlc_pgettext("Tooltip|Unmute", "Unmute")));
        return;
    }

    if( i_sliderVolume < VOLUME_MAX / 3 )
        volMuteLabel->setPixmap( QPixmap( ":/toolbar/volume-low" ) );
    else if( i_sliderVolume > (VOLUME_MAX * 2 / 3 ) )
        volMuteLabel->setPixmap( QPixmap( ":/toolbar/volume-high" ) );
    else volMuteLabel->setPixmap( QPixmap( ":/toolbar/volume-medium" ) );
    volMuteLabel->setToolTip( qfu(vlc_pgettext("Tooltip|Mute", "Mute")) );
}

void SoundWidget::updateVolume()
{
    /* Audio part */
    audio_volume_t i_volume;
    playlist_t *p_playlist = pl_Hold( p_intf );

    aout_VolumeGet( p_playlist, &i_volume );
    pl_Release( p_intf );
    i_volume = ( ( i_volume + 1 ) *  VOLUME_MAX )/ (AOUT_VOLUME_MAX/2);
    int i_gauge = volumeSlider->value();
    b_my_volume = false;
    if( i_volume - i_gauge > 1 || i_gauge - i_volume > 1 )
    {
        b_my_volume = true;
        volumeSlider->setValue( i_volume );
        b_my_volume = false;
    }
}

void SoundWidget::showVolumeMenu( QPoint pos )
{
    volumeMenu->setFixedHeight( volumeMenu->sizeHint().height() );
    volumeMenu->exec( QCursor::pos() - pos - QPoint( 0, volumeMenu->height()/2 )
                          + QPoint( width(), height() /2) );
}

bool SoundWidget::eventFilter( QObject *obj, QEvent *e )
{
    VLC_UNUSED( obj );
    if (e->type() == QEvent::MouseButtonPress  )
    {
        if( volumeSlider->orientation() ==  Qt::Vertical )
        {
            QMouseEvent *event = static_cast<QMouseEvent*>(e);
            showVolumeMenu( event->pos() );
        }
        else
        {
            playlist_t *p_playlist = pl_Hold( p_intf );

            aout_ToggleMute( p_playlist, NULL );
            pl_Release( p_intf );
        }
        e->accept();
        return true;
    }
    else
    {
        e->ignore();
        return false;
    }
}

/**
 * Play Button
 **/
void PlayButton::updateButton( bool b_playing )
{
    setIcon( b_playing ? QIcon( ":/toolbar/pause_b" ) : QIcon( ":/toolbar/play_b" ) );
    setToolTip( b_playing ? qtr( "Pause the playback" )
                          : qtr( I_PLAY_TOOLTIP ) );
}

void AtoB_Button::setIcons( bool timeA, bool timeB )
{
    if( !timeA && !timeB)
    {
        setIcon( QIcon( ":/toolbar/atob_nob" ) );
        setToolTip( qtr( "Loop from point A to point B continuously\n"
                         "Click to set point A" ) );
    }
    else if( timeA && !timeB )
    {
        setIcon( QIcon( ":/toolbar/atob_noa" ) );
        setToolTip( qtr( "Click to set point B" ) );
    }
    else if( timeA && timeB )
    {
        setIcon( QIcon( ":/toolbar/atob" ) );
        setToolTip( qtr( "Stop the A to B loop" ) );
    }
}

void LoopButton::updateIcons( int value )
{
    setChecked( value != NORMAL );
    setIcon( ( value == REPEAT_ALL ) ? QIcon( ":/buttons/playlist/repeat_all" )
                                     : QIcon( ":/buttons/playlist/repeat_one" ) );
}
