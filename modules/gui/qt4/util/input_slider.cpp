/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "qt4.hpp"
#include "util/input_slider.hpp"

#include <QPaintEvent>
#include <QPainter>
#include <QBitmap>
#include <QStyle>

InputSlider::InputSlider( QWidget *_parent ) : DirectSlider( _parent )
{
    InputSlider::InputSlider( Qt::Horizontal, _parent );
}

InputSlider::InputSlider( Qt::Orientation q,QWidget *_parent ) :
                                 DirectSlider( q, _parent )
{
    mymove = false;
    setMinimum( 0 );
    setMouseTracking(true);
    setMaximum( 1000 );
    setSingleStep( 2 );
    setPageStep( 10 );
    setTracking( true );
    CONNECT( this, valueChanged(int), this, userDrag( int ) );
}

void InputSlider::setPosition( float pos, int a, int b )
{
    if( pos == 0.0 )
        setEnabled( false );
    else
        setEnabled( true );
    mymove = true;
    setValue( (int)(pos * 1000.0 ) );
    mymove = false;
    inputLength = b;
}

void InputSlider::userDrag( int new_value )
{
    float f_pos = (float)(new_value)/1000.0;
    if( !mymove )
    {
        emit sliderDragged( f_pos );
    }
}

void InputSlider::mouseMoveEvent(QMouseEvent *event)
{
    char psz_length[MSTRTIME_MAX_SIZE];
    secstotimestr( psz_length, ( event->x() * inputLength) / size().width() );
    setToolTip( psz_length );
}

#define WLENGTH   100 // px
#define WHEIGHT   28  // px
#define SOUNDMIN  0   // %
#define SOUNDMAX  200 // % OR 400 ?

SoundSlider::SoundSlider( QWidget *_parent, int _i_step, bool b_hard )
                        : QAbstractSlider( _parent )
{
    padding = 3;

    f_step = ( _i_step * 100 ) / AOUT_VOLUME_MAX ;
    setRange( SOUNDMIN, b_hard ? (2 * SOUNDMAX) : SOUNDMAX  );

    pixOutside = QPixmap( ":/pixmaps/volume-slider-outside.png" );

    const QPixmap temp( ":/pixmaps/volume-slider-inside.png" );
    const QBitmap mask( temp.createHeuristicMask() );

    setMinimumSize( pixOutside.size() );

    pixGradient = QPixmap( mask.size() );

    QPainter p( &pixGradient );
    QLinearGradient gradient( padding, 2, WLENGTH , 2 );
    gradient.setColorAt( 0.0, Qt::white );
    gradient.setColorAt( 0.2, QColor( 20, 226, 20 ) );
    gradient.setColorAt( 0.5, QColor( 255, 176, 15 ) );
    gradient.setColorAt( 1.0, QColor( 235, 30, 20 ) );
    p.setPen( Qt::NoPen );
    p.setBrush( gradient );

    p.drawRect( pixGradient.rect() );
    p.end();

   pixGradient.setMask( mask );
}

void SoundSlider::wheelEvent( QWheelEvent *event )
{
    int newvalue = value() + event->delta() / ( 8 * 15 ) * f_step;
    setValue( __MIN( __MAX( minimum(), newvalue ), maximum() ) );

    emit sliderReleased();
}

void SoundSlider::mousePressEvent( QMouseEvent *event )
{
    if( event->button() != Qt::RightButton )
    {
        /* We enter the sliding mode */
        b_sliding = true;
        i_oldvalue = value();
        emit sliderPressed();
    }
}

void SoundSlider::mouseReleaseEvent( QMouseEvent *event )
{
    if( event->button() != Qt::RightButton )
    {
        if( !b_outside && value() != i_oldvalue )
        {
            emit sliderReleased();
            setValue( value() );
        }
        b_sliding = false;
        b_outside = false;
    }
}

void SoundSlider::mouseMoveEvent( QMouseEvent *event )
{
    if( b_sliding )
    {
        QRect rect( padding - 15,     padding - 1,
                    WLENGTH + 15 * 2, WHEIGHT + 4 );
        if( !rect.contains( event->pos() ) )
        { /* We are outside */
            if ( !b_outside )
                setValue( i_oldvalue );
            b_outside = true;
        }
        else
        { /* We are inside */
            b_outside = false;
            changeValue( event->x() - padding );
            emit sliderMoved( value() );
        }
    }
    else
        event->ignore();
}

void SoundSlider::changeValue( int x )
{
    setValue( QStyle::sliderValueFromPosition(
                minimum(), maximum(), x, width() - 2 * padding ) );
}

void SoundSlider::paintEvent(QPaintEvent *e)
{
    QPainter painter( this );
    const int offset = int( double( ( width() - 2 * padding ) * value() ) / maximum() );

    const QRectF boundsG( 0, 0, offset , pixGradient.height() );
    painter.drawPixmap( boundsG, pixGradient, boundsG );

    const QRectF boundsO( 0, 0, pixOutside.width(), pixOutside.height() );
    painter.drawPixmap( boundsO, pixOutside, boundsO );

    painter.end();
}

