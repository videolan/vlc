/*****************************************************************************
 * input_manager.cpp : Manage an input and interact with its GUI elements
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "util/input_slider.hpp"

#include <QPaintEvent>
#include <QPainter>
#include <QBitmap>
#include <QStyle>

InputSlider::InputSlider( QWidget *_parent ) : QSlider( _parent )
{
    InputSlider::InputSlider( Qt::Horizontal, _parent );
}

InputSlider::InputSlider( Qt::Orientation q,QWidget *_parent ) :
                                 QSlider( q, _parent )
{
    b_sliding = false;
    setMinimum( 0 );
    setMouseTracking(true);
    setMaximum( 1000 );
    setSingleStep( 2 );
    setPageStep( 10 );
    setTracking( true );
    secstotimestr( psz_length, 0 );
    CONNECT( this, valueChanged(int), this, userDrag( int ) );
}

void InputSlider::setPosition( float pos, int a, int b )
{
    if( pos == 0.0 )
        setEnabled( false );
    else
        setEnabled( true );

    if( !b_sliding )
        setValue( (int)(pos * 1000.0 ) );
    inputLength = b;
}

void InputSlider::userDrag( int new_value )
{
    if( b_sliding )
    {
        float f_pos = (float)(new_value)/1000.0;
        emit sliderDragged( f_pos );
    }
}

void InputSlider::mouseReleaseEvent( QMouseEvent *event )
{
    b_sliding = false;
}

void InputSlider::mousePressEvent(QMouseEvent* event)
{
    b_sliding = true ;
    if( event->button() != Qt::LeftButton &&
        event->button() != Qt::MidButton )
    {
        QSlider::mousePressEvent( event );
        return;
    }

    QMouseEvent newEvent( event->type(), event->pos(), event->globalPos(),
        Qt::MouseButton( event->button() ^ Qt::LeftButton ^ Qt::MidButton ),
        Qt::MouseButtons( event->buttons() ^ Qt::LeftButton ^ Qt::MidButton ),
        event->modifiers() );
    QSlider::mousePressEvent( &newEvent );
}

void InputSlider::mouseMoveEvent(QMouseEvent *event)
{
    if( b_sliding )
    {
        QSlider::mouseMoveEvent( event );
    }

    secstotimestr( psz_length, ( event->x() * inputLength) / size().width() );
    setToolTip( psz_length );
}

/* This work is derived from Amarok's work under GPLv2+
    - Mark Kretschmann
    - Gábor Lehel
   */
#define WLENGTH   90 // px
#define WHEIGHT   25  // px
#define SOUNDMIN  0   // %
#define SOUNDMAX  200 // % OR 400 ?

SoundSlider::SoundSlider( QWidget *_parent, int _i_step, bool b_hard,
                          char *psz_colors )
                        : QAbstractSlider( _parent )
{
    paddingL = 5;
    paddingR = 3;

    f_step = ( _i_step * 100 ) / AOUT_VOLUME_MAX ;
    setRange( SOUNDMIN, b_hard ? (2 * SOUNDMAX) : SOUNDMAX  );
    setMouseTracking( true );
    b_sliding = false;

    pixOutside = QPixmap( ":/pixmaps/volume-slider-outside.png" );

    const QPixmap temp( ":/pixmaps/volume-slider-inside.png" );
    const QBitmap mask( temp.createHeuristicMask() );

    setMinimumSize( pixOutside.size() );

    pixGradient = QPixmap( mask.size() );

    /* Gradient building from the preferences */
    QLinearGradient gradient( paddingL, 4, WLENGTH + paddingL , 4 );

    QStringList colorList = qfu( psz_colors ).split( ";" );
    /* Fill with 255 if the list is too short */
    if( colorList.size() < 12 )
        for( int i = colorList.size(); i < 12; i++)
            colorList.append( "255" );

#define c(i) colorList.at(i).toInt()
    gradient.setColorAt( 0.0, QColor( c(0), c(1), c(2) ) );
    gradient.setColorAt( 0.2, QColor( c(3), c(4), c(5) ) );
    gradient.setColorAt( 0.5, QColor( c(6), c(7), c(8) ) );
    gradient.setColorAt( 1.0, QColor( c(9), c(10), c(11) ) );

    QPainter painter( &pixGradient );
    painter.setPen( Qt::NoPen );
    painter.setBrush( gradient );
    painter.drawRect( pixGradient.rect() );
    painter.end();

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
        changeValue( event->x() - paddingL );
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
        QRect rect( paddingL - 15,    -1,
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
            changeValue( event->x() - paddingL );
            emit sliderMoved( value() );
        }
    }
    else
    {
        int i = ( event->x() - paddingL ) * maximum() / WLENGTH;
        i = __MIN( __MAX( 0, i ), maximum() );
        setToolTip( QString("%1  \%" ).arg( i ) );
    }
}

void SoundSlider::changeValue( int x )
{
    setValue( x * maximum() / WLENGTH );
}

void SoundSlider::paintEvent(QPaintEvent *e)
{
    QPainter painter( this );
    const int offset = int(
            double( WLENGTH * value() ) /
            double( maximum() ) ) + paddingL;

    const QRectF boundsG( 0, 0, offset , pixGradient.height() );
    painter.drawPixmap( boundsG, pixGradient, boundsG );

    const QRectF boundsO( 0, 0, pixOutside.width(), pixOutside.height() );
    painter.drawPixmap( boundsO, pixOutside, boundsO );

    painter.setPen( palette().color( QPalette::Active, QPalette::Mid ) );
    QFont font; font.setPixelSize( 9 );
    painter.setFont( font );
    const QRect rect( 0, 0, 34, 15 );
    painter.drawText( rect, Qt::AlignRight | Qt::AlignVCenter,
                      QString::number( value() ) + '%' );

    painter.end();
}

