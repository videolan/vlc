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
#define WHEIGHT   25  // px
#define SOUNDMIN  0   // %
#define SOUNDMAX  200 // % OR 400 ?
#define SOUNDSTEP 5   // %

SoundSlider::SoundSlider( QWidget *_parent ) : QAbstractSlider( _parent )
{
    padding = 5;
    setRange( SOUNDMIN, SOUNDMAX );

    pixGradient = QPixmap( QSize( WLENGTH, WHEIGHT ) );
//    QBixmap mask = QBitmap( QPixmap );

    QPainter p( &pixGradient );
    QLinearGradient gradient( 0, 0, WLENGTH, WHEIGHT );
    gradient.setColorAt( 0.0, Qt::white );
    gradient.setColorAt( 1.0, Qt::blue );
    p.setPen( Qt::NoPen );
    p.setBrush( gradient );

//static const QPointF points[3] = { QPointF( 0.0, WHEIGHT ),
  //        QPointF( WLENGTH, WHEIGHT ),  QPointF( WLENGTH, 0.0 ) };

 //   p.drawConvexPolygon( points, 3 );

    p.drawRect( pixGradient.rect() );
    p.end();

 //   pixGradient.setMask( mask );
}

void SoundSlider::wheelEvent( QWheelEvent *event )
{
    int newvalue = value() + event->delta() / ( 8 * 15 ) * SOUNDSTEP;
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
                    WLENGTH + 15 * 2, WHEIGHT + 2 );
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
    const QRectF boundsG( padding, 0, offset , pixGradient.height() );
    painter.drawPixmap( boundsG, pixGradient, boundsG );

    painter.end();
/*    QPainter painter( this );
    printf( "%i\n", value() );

    QLinearGradient gradient( 0.0, 0.0, WLENGTH, WHEIGHT );
    gradient.setColorAt( 0.0, Qt::white );
    gradient.setColorAt( 1.0, Qt::blue );

    painter.setPen( QPen( QBrush( Qt::black ), 0, Qt::SolidLine, Qt::RoundCap ) );
    painter.setBrush( gradient );
    painter.setRenderHint( QPainter::Antialiasing );

    painter.end();*/
}

