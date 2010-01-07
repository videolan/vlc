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

InputSlider::InputSlider( QWidget *_parent ) : QSlider( _parent )
{
    InputSlider::InputSlider( Qt::Horizontal, _parent );
}

InputSlider::InputSlider( Qt::Orientation q, QWidget *_parent ) :
                                 QSlider( q, _parent )
{
    b_isSliding = false;
    lastSeeked  = 0;

    timer = new QTimer(this);
    timer->setSingleShot(true);

    /* Properties */
    setRange( 0, 1000 );
    setSingleStep( 2 );
    setPageStep( 10 );
    setMouseTracking(true);
    setTracking( true );
    setFocusPolicy( Qt::NoFocus );

    /* Init to 0 */
    setPosition( -1.0, 0, 0 );
    secstotimestr( psz_length, 0 );

    CONNECT( this, valueChanged(int), this, userDrag( int ) );
    CONNECT( timer, timeout(), this, seekTick() );
}

void InputSlider::setPosition( float pos, int a, int b )
{
    if( pos == -1.0 )
    {
        setEnabled( false );
        b_isSliding = false;
    }
    else
        setEnabled( true );

    if( !b_isSliding )
        setValue( (int)(pos * 1000.0 ) );

    inputLength = b;
}

void InputSlider::userDrag( int new_value )
{
    if( b_isSliding && !timer->isActive() )
        timer->start( 150 );
}

void InputSlider::seekTick()
{
    if( value() != lastSeeked )
    {
        lastSeeked = value();
        float f_pos = (float)(lastSeeked)/1000.0;
        emit sliderDragged( f_pos );
    }
}

void InputSlider::mouseReleaseEvent( QMouseEvent *event )
{
    b_isSliding = false;
    event->accept();
    QSlider::mouseReleaseEvent( event );
}

void InputSlider::mousePressEvent(QMouseEvent* event)
{
    b_isSliding = true ;
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

    seekTick();
}

void InputSlider::mouseMoveEvent(QMouseEvent *event)
{
    if( b_isSliding )
    {
        QSlider::mouseMoveEvent( event );
    }

    secstotimestr( psz_length, ( event->x() * inputLength) / size().width() );
    setToolTip( psz_length );
    event->accept();
}

void InputSlider::wheelEvent( QWheelEvent *event)
{
    /* Don't do anything if we are for somehow reason sliding */
    if( !b_isSliding )
    {
        setValue( value() + event->delta()/12 ); /* 12 = 8 * 15 / 10
         Since delta is in 1/8 of ° and mouse have steps of 15 °
         and that our slider is in 0.1% and we want one step to be a 1%
         increment of position */
        emit sliderDragged( value()/1000.0 );
    }
    /* We do accept because for we don't want the parent to change the sound
       vol */
    event->accept();
}

/* This work is derived from Amarok's work under GPLv2+
    - Mark Kretschmann
    - Gábor Lehel
   */
#define WLENGTH   80 // px
#define WHEIGHT   22  // px
#define SOUNDMIN  0   // %
#define SOUNDMAX  200 // % OR 400 ?

SoundSlider::SoundSlider( QWidget *_parent, int _i_step, bool b_hard,
                          char *psz_colors )
                        : QAbstractSlider( _parent )
{
    f_step = ( _i_step * 100 ) / AOUT_VOLUME_MAX ;
    setRange( SOUNDMIN, b_hard ? (2 * SOUNDMAX) : SOUNDMAX  );
    setMouseTracking( true );
    b_isSliding = false;
    b_mouseOutside = true;
    b_isMuted = false;

    pixOutside = QPixmap( ":/toolbar/volslide-outside" );

    const QPixmap temp( ":/toolbar/volslide-inside" );
    const QBitmap mask( temp.createHeuristicMask() );

    setMinimumSize( pixOutside.size() );

    pixGradient = QPixmap( mask.size() );
    pixGradient2 = QPixmap( mask.size() );

    /* Gradient building from the preferences */
    QLinearGradient gradient( paddingL, 2, WLENGTH + paddingL , 2 );
    QLinearGradient gradient2( paddingL, 2, WLENGTH + paddingL , 2 );

    QStringList colorList = qfu( psz_colors ).split( ";" );
    free( psz_colors );

    /* Fill with 255 if the list is too short */
    if( colorList.size() < 12 )
        for( int i = colorList.size(); i < 12; i++)
            colorList.append( "255" );

    /* Regular colors */
#define c(i) colorList.at(i).toInt()
#define add_color(gradient, range, c1, c2, c3) \
    gradient.setColorAt( range, QColor( c(c1), c(c2), c(c3) ) );

    /* Desaturated colors */
#define desaturate(c) c->setHsvF( c->hueF(), 0.2 , 0.5, 1.0 )
#define add_desaturated_color(gradient, range, c1, c2, c3) \
    foo = new QColor( c(c1), c(c2), c(c3) );\
    desaturate( foo ); gradient.setColorAt( range, *foo );\
    delete foo;

    /* combine the two helpers */
#define add_colors( gradient1, gradient2, range, c1, c2, c3 )\
    add_color( gradient1, range, c1, c2, c3 ); \
    add_desaturated_color( gradient2, range, c1, c2, c3 );

    QColor * foo;
    add_colors( gradient, gradient2, 0.0, 0, 1, 2 );
    add_colors( gradient, gradient2, 0.22, 3, 4, 5 );
    add_colors( gradient, gradient2, 0.5, 6, 7, 8 );
    add_colors( gradient, gradient2, 1.0, 9, 10, 11 );

    QPainter painter( &pixGradient );
    painter.setPen( Qt::NoPen );
    painter.setBrush( gradient );
    painter.drawRect( pixGradient.rect() );
    painter.end();

    painter.begin( &pixGradient2 );
    painter.setPen( Qt::NoPen );
    painter.setBrush( gradient2 );
    painter.drawRect( pixGradient2.rect() );
    painter.end();

    pixGradient.setMask( mask );
    pixGradient2.setMask( mask );
}

void SoundSlider::wheelEvent( QWheelEvent *event )
{
    int newvalue = value() + event->delta() / ( 8 * 15 ) * f_step;
    setValue( __MIN( __MAX( minimum(), newvalue ), maximum() ) );

    emit sliderReleased();
    emit sliderMoved( value() );
}

void SoundSlider::mousePressEvent( QMouseEvent *event )
{
    if( event->button() != Qt::RightButton )
    {
        /* We enter the sliding mode */
        b_isSliding = true;
        i_oldvalue = value();
        emit sliderPressed();
        changeValue( event->x() - paddingL );
        emit sliderMoved( value() );
    }
}

void SoundSlider::mouseReleaseEvent( QMouseEvent *event )
{
    if( event->button() != Qt::RightButton )
    {
        if( !b_mouseOutside && value() != i_oldvalue )
        {
            emit sliderReleased();
            setValue( value() );
            emit sliderMoved( value() );
        }
        b_isSliding = false;
        b_mouseOutside = false;
    }
}

void SoundSlider::mouseMoveEvent( QMouseEvent *event )
{
    if( b_isSliding )
    {
        QRect rect( paddingL - 15,    -1,
                    WLENGTH + 15 * 2 , WHEIGHT + 5 );
        if( !rect.contains( event->pos() ) )
        { /* We are outside */
            if ( !b_mouseOutside )
                setValue( i_oldvalue );
            b_mouseOutside = true;
        }
        else
        { /* We are inside */
            b_mouseOutside = false;
            changeValue( event->x() - paddingL );
            emit sliderMoved( value() );
        }
    }
    else
    {
        int i = ( ( event->x() - paddingL ) * maximum() + 40 ) / WLENGTH;
        i = __MIN( __MAX( 0, i ), maximum() );
        setToolTip( QString("%1  \%" ).arg( i ) );
    }
}

void SoundSlider::changeValue( int x )
{
    setValue( (x * maximum() + 40 ) / WLENGTH );
}

void SoundSlider::setMuted( bool m )
{
    b_isMuted = m;
    update();
}

void SoundSlider::paintEvent( QPaintEvent *e )
{
    QPainter painter( this );
    QPixmap *pixGradient;
    if (b_isMuted)
        pixGradient = &this->pixGradient2;
    else
        pixGradient = &this->pixGradient;

    const int offset = int( ( WLENGTH * value() + 100 ) / maximum() ) + paddingL;

    const QRectF boundsG( 0, 0, offset , pixGradient->height() );
    painter.drawPixmap( boundsG, *pixGradient, boundsG );

    const QRectF boundsO( 0, 0, pixOutside.width(), pixOutside.height() );
    painter.drawPixmap( boundsO, pixOutside, boundsO );

    painter.setPen( palette().color( QPalette::Active, QPalette::Mid ) );
    QFont font; font.setPixelSize( 9 );
    painter.setFont( font );
    const QRect rect( 0, 0, 34, 15 );
    painter.drawText( rect, Qt::AlignRight | Qt::AlignVCenter,
                      QString::number( value() ) + '%' );

    painter.end();
    e->accept();
}

