/*****************************************************************************
 * Controller_widget.cpp : Controller Widget for the controllers
 ****************************************************************************
 * Copyright ( C ) 2006-2008 the VideoLAN team
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

#include "input_manager.hpp"         /* Get notification of Volume Change */
#include "util/input_slider.hpp"     /* SoundSlider */

#include <vlc_aout.h>                /* Volume functions */

#include <QLabel>
#include <QHBoxLayout>
#include <QSpinBox>

SoundWidget::SoundWidget( QWidget *_parent, intf_thread_t * _p_intf,
                          bool b_shiny )
                         : b_my_volume( false ), QWidget( _parent )
{
    p_intf = _p_intf;
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setSpacing( 0 ); layout->setMargin( 0 );
    hVolLabel = new VolumeClickHandler( p_intf, this );

    volMuteLabel = new QLabel;
    volMuteLabel->setPixmap( QPixmap( ":/volume-medium" ) );
    volMuteLabel->installEventFilter( hVolLabel );
    layout->addWidget( volMuteLabel );

    if( b_shiny )
    {
        volumeSlider = new SoundSlider( this,
            config_GetInt( p_intf, "volume-step" ),
            config_GetInt( p_intf, "qt-volume-complete" ),
            config_GetPsz( p_intf, "qt-slider-colours" ) );
    }
    else
    {
        volumeSlider = new QSlider( this );
        volumeSlider->setOrientation( Qt::Horizontal );
    }
    volumeSlider->setMaximumSize( QSize( 200, 40 ) );
    volumeSlider->setMinimumSize( QSize( 85, 30 ) );
    volumeSlider->setFocusPolicy( Qt::NoFocus );
    layout->addWidget( volumeSlider );

    /* Set the volume from the config */
    volumeSlider->setValue( ( config_GetInt( p_intf, "volume" ) ) *
                              VOLUME_MAX / (AOUT_VOLUME_MAX/2) );

    /* Force the update at build time in order to have a muted icon if needed */
    updateVolume( volumeSlider->value() );

    /* Volume control connection */
    CONNECT( volumeSlider, valueChanged( int ), this, updateVolume( int ) );
    CONNECT( THEMIM, volumeChanged( void ), this, updateVolume( void ) );
}

void SoundWidget::updateVolume( int i_sliderVolume )
{
    if( !b_my_volume )
    {
        int i_res = i_sliderVolume  * (AOUT_VOLUME_MAX / 2) / VOLUME_MAX;
        aout_VolumeSet( p_intf, i_res );
    }
    if( i_sliderVolume == 0 )
    {
        volMuteLabel->setPixmap( QPixmap(":/volume-muted" ) );
        volMuteLabel->setToolTip( qtr( "Unmute" ) );
        return;
    }

    if( i_sliderVolume < VOLUME_MAX / 3 )
        volMuteLabel->setPixmap( QPixmap( ":/volume-low" ) );
    else if( i_sliderVolume > (VOLUME_MAX * 2 / 3 ) )
        volMuteLabel->setPixmap( QPixmap( ":/volume-high" ) );
    else volMuteLabel->setPixmap( QPixmap( ":/volume-medium" ) );
    volMuteLabel->setToolTip( qtr( "Mute" ) );
}

void SoundWidget::updateVolume()
{
    /* Audio part */
    audio_volume_t i_volume;
    aout_VolumeGet( p_intf, &i_volume );
    i_volume = ( i_volume *  VOLUME_MAX )/ (AOUT_VOLUME_MAX/2);
    int i_gauge = volumeSlider->value();
    b_my_volume = false;
    if( i_volume - i_gauge > 1 || i_gauge - i_volume > 1 )
    {
        b_my_volume = true;
        volumeSlider->setValue( i_volume );
        b_my_volume = false;
    }
}

void TeletextController::toggleTeletextTransparency( bool b_transparent )
{
    telexTransparent->setIcon( b_transparent ? QIcon( ":/tvtelx" )
                                             : QIcon( ":/tvtelx-trans" ) );
}

void TeletextController::enableTeletextButtons( bool b_enabled )
{
    telexOn->setChecked( b_enabled );
    telexTransparent->setEnabled( b_enabled );
    telexPage->setEnabled( b_enabled );
}

void PlayButton::updateButton( bool b_playing )
{
    setIcon( b_playing ? QIcon( ":/pause_b" ) : QIcon( ":/play_b" ) );
    setToolTip( b_playing ? qtr( "Pause the playback" )
                          : qtr( I_PLAY_TOOLTIP ) );
}

void AtoB_Button::setIcons( bool timeA, bool timeB )
{
    if( !timeA && !timeB)
    {
        setIcon( QIcon( ":/atob_nob" ) );
        setToolTip( qtr( "Loop from point A to point B continuously\n"
                         "Click to set point A" ) );
    }
    else if( timeA && !timeB )
    {
        setIcon( QIcon( ":/atob_noa" ) );
        setToolTip( qtr( "Click to set point B" ) );
    }
    else if( timeA && timeB )
    {
        setIcon( QIcon( ":/atob" ) );
        setToolTip( qtr( "Stop the A to B loop" ) );
    }
}

bool VolumeClickHandler::eventFilter( QObject *obj, QEvent *e )
{
    if (e->type() == QEvent::MouseButtonPress  )
    {
        aout_VolumeMute( p_intf, NULL );
        audio_volume_t i_volume;
        aout_VolumeGet( p_intf, &i_volume );
//        m->updateVolume( i_volume *  VOLUME_MAX / (AOUT_VOLUME_MAX/2) );
        return true;
    }
    return false;
}

