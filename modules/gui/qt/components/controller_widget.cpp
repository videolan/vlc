/*****************************************************************************
 * controller_widget.cpp : Controller Widget for the controllers
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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

#include "components/playlist/playlist_controller.hpp"
#include "components/player_controller.hpp"         /* Get notification of Volume Change */
#include "util/input_slider.hpp"     /* SoundSlider */
#include "util/imagehelper.hpp"
#include "vlc_player.h"

#include <math.h>

#include <QLabel>
#include <QHBoxLayout>
#include <QMenu>
#include <QWidgetAction>
#include <QMouseEvent>

#define VOLUME_MAX 125

using namespace vlc::playlist;

SoundWidget::SoundWidget( QWidget *_parent, intf_thread_t * _p_intf,
                          bool b_shiny, bool b_special )
                         : QWidget( _parent ), p_intf( _p_intf),
                           b_is_muted( false ), b_ignore_valuechanged( false )
{
    /* We need a layout for this widget */
    QHBoxLayout *layout = new QHBoxLayout( this );
    layout->setSpacing( 0 ); layout->setMargin( 0 );

    /* We need a Label for the pix */
    volMuteLabel = new QLabel;
    volMuteLabel->setPixmap( ImageHelper::loadSvgToPixmap( ":/toolbar/volume-medium.svg", 16, 16 ) );

    /* We might need a subLayout too */
    QVBoxLayout *subLayout;

    volMuteLabel->installEventFilter( this );

    /* Normal View, click on icon mutes */
    if( !b_special )
    {
        volumeMenu = NULL; subLayout = NULL;
        volumeControlWidget = NULL;

        /* And add the label */
        layout->addWidget( volMuteLabel, 0, b_shiny? Qt::AlignBottom : Qt::AlignCenter );
    }
    else
    {
        /* Special view, click on button shows the slider */
        b_shiny = false;

        volumeControlWidget = new QFrame( this );
        subLayout = new QVBoxLayout( volumeControlWidget );
        subLayout->setContentsMargins( 4, 4, 4, 4 );
        volumeMenu = new QMenu( this );

        QWidgetAction *widgetAction = new QWidgetAction( volumeControlWidget );
        widgetAction->setDefaultWidget( volumeControlWidget );
        volumeMenu->addAction( widgetAction );

        /* And add the label */
        layout->addWidget( volMuteLabel );
    }

    /* Slider creation: shiny or clean */
    if( b_shiny )
    {
        volumeSlider = new SoundSlider( this,
            config_GetFloat( "volume-step" ),
            var_InheritString( p_intf, "qt-slider-colours" ),
            var_InheritInteger( p_intf, "qt-max-volume") );
    }
    else
    {
        volumeSlider = new QSlider( NULL );
        volumeSlider->setAttribute( Qt::WA_MacSmallSize);
        volumeSlider->setOrientation( b_special ? Qt::Vertical
                                                : Qt::Horizontal );
        volumeSlider->setMaximum( 200 );
    }

    volumeSlider->setFocusPolicy( Qt::NoFocus );
    if( b_special )
        subLayout->addWidget( volumeSlider );
    else
        layout->addWidget( volumeSlider, 0, b_shiny? Qt::AlignBottom : Qt::AlignCenter );

    /* Set the volume from the config */
    float volume = THEMIM->getVolume();

    libUpdateVolume( (volume >= 0.f) ? volume : 1.f );
    /* Sync mute status */
    if( THEMIM->isMuted() )
        updateMuteStatus( true );

    /* Volume control connection */
    volumeSlider->setTracking( true );
    CONNECT( volumeSlider, valueChanged( int ), this, valueChangedFilter( int ) );
    CONNECT( this, valueReallyChanged( int ), this, userUpdateVolume( int ) );
    CONNECT( THEMIM, volumeChanged( float ), this, libUpdateVolume( float ) );
    CONNECT( THEMIM, soundMuteChanged( bool ), this, updateMuteStatus( bool ) );

    setAccessibleName( qtr( "Volume slider" ) );
}

void SoundWidget::refreshLabels()
{
    int i_sliderVolume = volumeSlider->value();
    const char *psz_icon = ":/toolbar/volume-muted.svg";

    if( b_is_muted )
    {
        volMuteLabel->setPixmap( ImageHelper::loadSvgToPixmap( psz_icon, 16, 16 ) );
        volMuteLabel->setToolTip(qfu(vlc_pgettext("Tooltip|Unmute", "Unmute")));
        return;
    }

    if( i_sliderVolume < VOLUME_MAX / 3 )
        psz_icon = ":/toolbar/volume-low.svg";
    else if( i_sliderVolume > (VOLUME_MAX * 2 / 3 ) )
        psz_icon = ":/toolbar/volume-high.svg";
    else
        psz_icon = ":/toolbar/volume-medium.svg";

    volMuteLabel->setPixmap( ImageHelper::loadSvgToPixmap( psz_icon, 16, 16 ) );
    volMuteLabel->setToolTip( qfu(vlc_pgettext("Tooltip|Mute", "Mute")) );
}

/* volumeSlider changed value event slot */
void SoundWidget::userUpdateVolume( int i_sliderVolume )
{
    /* Only if volume is set by user action on slider */
    setMuted( false );
    THEMIM->setVolume( i_sliderVolume / 100.f );
    refreshLabels();
}

/* libvlc changed value event slot */
void SoundWidget::libUpdateVolume( float volume )
{
    long i_volume = lroundf(volume * 100.f);
    if( i_volume != volumeSlider->value()  )
    {
        b_ignore_valuechanged = true;
        volumeSlider->setValue( i_volume );
        b_ignore_valuechanged = false;
    }
    refreshLabels();
}

void SoundWidget::valueChangedFilter( int i_val )
{
    /* valueChanged is also emitted when the lib setValue() */
    if ( !b_ignore_valuechanged ) emit valueReallyChanged( i_val );
}

/* libvlc mute/unmute event slot */
void SoundWidget::updateMuteStatus( bool mute )
{
    b_is_muted = mute;

    SoundSlider *soundSlider = qobject_cast<SoundSlider *>(volumeSlider);
    if( soundSlider )
        soundSlider->setMuted( mute );
    refreshLabels();
}

void SoundWidget::showVolumeMenu( QPoint pos )
{
    volumeMenu->setFixedHeight( volumeMenu->sizeHint().height() );
    volumeMenu->exec( QCursor::pos() - pos - QPoint( 0, volumeMenu->height()/2 )
                          + QPoint( width(), height() /2) );
}

void SoundWidget::setMuted( bool mute )
{
    b_is_muted = mute;
    THEMIM->setMuted(mute);
}

bool SoundWidget::eventFilter( QObject *obj, QEvent *e )
{
    VLC_UNUSED( obj );
    if( e->type() == QEvent::MouseButtonPress )
    {
        QMouseEvent *event = static_cast<QMouseEvent*>(e);
        if( event->button() == Qt::LeftButton )
        {
            if( volumeSlider->orientation() ==  Qt::Vertical )
            {
                showVolumeMenu( event->pos() );
            }
            else
            {
                setMuted( !b_is_muted );
            }
            e->accept();
            return true;
        }
    }
    e->ignore();
    return false;
}

/**
 * Play Button
 **/
void PlayButton::updateButtonIcons( bool b_playing )
{
    setIcon( b_playing ? QIcon( ":/toolbar/pause_b.svg" ) : QIcon( ":/toolbar/play_b.svg" ) );
    setToolTip( b_playing ? qtr( "Pause the playback" )
                          : qtr( I_PLAY_TOOLTIP ) );
}

void AtoB_Button::updateButtonIcons( PlayerController::ABLoopState state )
{
    switch( state)
    {
    case PlayerController::ABLOOP_STATE_NONE:
        setIcon( QIcon( ":/toolbar/atob_nob.svg" ) );
        setToolTip( qtr( "Loop from point A to point B continuously\n"
                         "Click to set point A" ) );
        break;
    case PlayerController::ABLOOP_STATE_A:
        setIcon( QIcon( ":/toolbar/atob_noa.svg" ) );
        setToolTip( qtr( "Click to set point B" ) );
        break;
    case PlayerController::ABLOOP_STATE_B:
        setIcon( QIcon( ":/toolbar/atob.svg" ) );
        setToolTip( qtr( "Stop the A to B loop" ) );
        break;
    }
}

void LoopButton::updateButtonIcons( PlaylistControllerModel::PlaybackRepeat value )
{
    setChecked( value != PlaylistControllerModel::PLAYBACK_REPEAT_NONE );
    setIcon( ( value == PlaylistControllerModel::PLAYBACK_REPEAT_CURRENT ) ? QIcon( ":/buttons/playlist/repeat_one.svg" )
                                     : QIcon( ":/buttons/playlist/repeat_all.svg" ) );
}

void AspectRatioComboBox::onRowInserted(const QModelIndex &, int first, int last)
{
    for (int i = first; i <= last; i++)
    {
        QModelIndex index = m_aspectRatioModel->index(i);
        addItem(m_aspectRatioModel->data(index, Qt::DisplayRole).toString());
        if (m_aspectRatioModel->data(index, Qt::CheckStateRole).toBool())
            setCurrentIndex(i);
    }
}

void AspectRatioComboBox::onRowRemoved(const QModelIndex &, int first, int last)
{
    for (int i = last; i >= first; i--)
        removeItem(i);
}

void AspectRatioComboBox::onModelAboutToReset()
{
    clear();
}

void AspectRatioComboBox::onModelReset()
{
    if (m_aspectRatioModel->rowCount() == 0)
    {
        setEnabled(false);
        addItem( qtr("Aspect Ratio") );
    }
    else
    {
        setEnabled(true);
        for (int i = 0; i < m_aspectRatioModel->rowCount(); i++)
            addItem(m_aspectRatioModel->data(m_aspectRatioModel->index(i), Qt::DisplayRole).toString());
    }
}

void AspectRatioComboBox::onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &)
{
    for (int i = topLeft.row(); i <= bottomRight.row(); i++)
    {
        QModelIndex index=  m_aspectRatioModel->index(i);
        setItemText(i, m_aspectRatioModel->data(index, Qt::DisplayRole).toString());
        if (m_aspectRatioModel->data(index, Qt::CheckStateRole).toBool())
            setCurrentIndex(i);
    }
}

void AspectRatioComboBox::updateAspectRatio( int x )
{
    QModelIndex index = m_aspectRatioModel->index(x);
    m_aspectRatioModel->setData(index, true, Qt::CheckStateRole);
}
