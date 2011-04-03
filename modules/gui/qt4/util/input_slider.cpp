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
#include <QPainter>
#include <QStyleOptionSlider>
#include <QLinearGradient>


#define MINIMUM 0
#define MAXIMUM 1000

InputSlider::InputSlider( QWidget *_parent ) : QSlider( _parent )
{
    InputSlider( Qt::Horizontal, _parent );
}

InputSlider::InputSlider( Qt::Orientation q, QWidget *_parent )
            : QSlider( q, _parent )
{
    b_isSliding = false;

    /* Timer used to fire intermediate updatePos() when sliding */
    seekLimitTimer = new QTimer(this);
    seekLimitTimer->setSingleShot(true);

    /* Properties */
    setRange( MINIMUM, MAXIMUM );
    setSingleStep( 2 );
    setPageStep( 10 );
    setMouseTracking(true);
    setTracking( true );
    setFocusPolicy( Qt::NoFocus );

    /* Init to 0 */
    setPosition( -1.0, 0, 0 );
    secstotimestr( psz_length, 0 );

    CONNECT( this, sliderMoved(int), this, startSeekTimer( int ) );
    CONNECT( seekLimitTimer, timeout(), this, updatePos() );
}

/***
 * \brief Public interface, like setValue,  but disabling the slider too
 ***/
void InputSlider::setPosition( float pos, int64_t a, int b )
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

void InputSlider::startSeekTimer( int new_value )
{
    /* Only fire one update, when sliding, every 150ms */
    if( b_isSliding && !seekLimitTimer->isActive() )
        seekLimitTimer->start( 150 );
}

void InputSlider::updatePos()
{
    float f_pos = (float)(value())/1000.0;
    emit sliderDragged( f_pos ); /* Send new position to VLC's core */
}

void InputSlider::mouseReleaseEvent( QMouseEvent *event )
{
    event->accept();
    b_isSliding = false;
    seekLimitTimer->stop(); /* We're not sliding anymore: only last seek on release */
    QSlider::mouseReleaseEvent( event );
    updatePos();
}

void InputSlider::mousePressEvent(QMouseEvent* event)
{
    /* Right-click */
    if( event->button() != Qt::LeftButton &&
        event->button() != Qt::MidButton )
    {
        QSlider::mousePressEvent( event );
        return;
    }

    b_isSliding = true ;
    setValue( QStyle::sliderValueFromPosition( MINIMUM, MAXIMUM, event->x(), width(), false) );
    event->accept();
}

void InputSlider::mouseMoveEvent(QMouseEvent *event)
{
    if( b_isSliding )
    {
        setValue( QStyle::sliderValueFromPosition( MINIMUM, MAXIMUM, event->x(), width(), false) );
    }

    /* Tooltip */
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

QSize InputSlider::sizeHint() const
{
    return ( orientation() == Qt::Horizontal ) ? QSize( 100, 18 )
                                               : QSize( 18, 100 );
}

QSize InputSlider::handleSize() const
{
    const int size = ( orientation() == Qt::Horizontal ? height() : width() );
    return QSize( size, size );
}

void InputSlider::paintEvent( QPaintEvent *event )
{
    Q_UNUSED( event );

    QStyleOptionSlider option;
    initStyleOption( &option );

    /* */
    QPainter painter( this );
    painter.setRenderHints( QPainter::Antialiasing );

    // draw bar
    const int barCorner = 3;
    qreal sliderPos     = -1;
    int range           = MAXIMUM;
    QRect barRect = rect();

    if ( option.sliderPosition != 0 ) {
        switch ( orientation() ) {
            case Qt::Horizontal:
                sliderPos = ( ( (qreal)width() ) /(qreal)range ) *(qreal)option.sliderPosition;
                break;
            case Qt::Vertical:
                sliderPos = ( ( (qreal)height() ) /(qreal)range ) *(qreal)option.sliderPosition;
                break;
        }
    }

    switch ( orientation() ) {
        case Qt::Horizontal:
            barRect.setHeight( handleSize().height() /2 );
            break;
        case Qt::Vertical:
            barRect.setWidth( handleSize().width() /2 );
            break;
    }

    barRect.moveCenter( rect().center() );

    QLinearGradient backgroundGradient( 0, 0, 0, height() );
    backgroundGradient.setColorAt( 0.0, QColor( 126, 126, 126 ) );
    backgroundGradient.setColorAt( 0.30, QColor( 110, 110, 110 ) );
    backgroundGradient.setColorAt( 0.31, QColor( 101, 101, 101 ) );
    backgroundGradient.setColorAt( 1.0, QColor( 86, 86, 86 ) );

    QLinearGradient foregroundGradient( 0, 0, 0, height() );
    foregroundGradient.setColorAt( 0.0,  QColor( 26, 49, 128 ) );
    foregroundGradient.setColorAt( 0.30, QColor( 28, 77, 175) );
    foregroundGradient.setColorAt( 0.32, QColor( 32, 85, 177) );
    foregroundGradient.setColorAt( 1.0,  QColor( 81, 50, 210 ) );

    //foregroundGradient.setColorAt( 0.0, palette().color( QPalette::Inactive, QPalette::Mid ) );
    //foregroundGradient.setColorAt( 0.30, palette().color( QPalette::Inactive, QPalette::Light ) );
    //foregroundGradient.setColorAt( 1.0, palette().color( QPalette::Inactive, QPalette::Midlight ) );

    //foregroundGradient.setColorAt( 0.0, QColor( 35, 213, 7 ) );
    //foregroundGradient.setColorAt( 0.30, QColor( 37, 133, 21 ) );
    //foregroundGradient.setColorAt( 1.0, QColor( 81, 215, 55 ) );

    painter.setPen( Qt::NoPen );
    painter.setBrush( backgroundGradient );
    painter.drawRoundedRect( barRect.adjusted( 0, 0, 0, 0 ), barCorner, barCorner );

    switch ( orientation() ) {
        case Qt::Horizontal:
            barRect.setWidth( qMin( width(), int( sliderPos ) ) );
            break;
        case Qt::Vertical:
            barRect.setHeight( qMin( height(), int( sliderPos ) ) );
            barRect.moveBottom( rect().bottom() );
            break;
    }

    if ( option.sliderPosition > minimum() && option.sliderPosition <= maximum() ) {
        painter.setPen( Qt::black );
        painter.setBrush( foregroundGradient );
        painter.drawRoundedRect( barRect.adjusted( 1, 1, -1, -1 ), barCorner, barCorner );
    }

    // draw handle
    if ( option.state & QStyle::State_MouseOver ) {
        QLinearGradient buttonGradient( 0, 0, 0, height() );
        buttonGradient.setColorAt( 0.0, QColor( 1, 92, 195 ) );
        buttonGradient.setColorAt( 0.40, QColor( 41, 80, 124 ) );
        buttonGradient.setColorAt( 1.0, QColor( 1, 92, 195 ) );

        painter.setPen( QPen( Qt::blue ) );
        painter.setBrush( buttonGradient );

        if ( sliderPos != -1 ) {
            const int margin = 5;
            QSize hs = handleSize() -QSize( 2, 2 );
            QPoint pos;

            switch ( orientation() ) {
                case Qt::Horizontal:
                    pos = QPoint( sliderPos -( handleSize().width() /2 ), 1 );
                    pos.rx() = qMax( margin, pos.x() );
                    pos.rx() = qMin( width() -hs.width() -margin, pos.x() );
                    break;
                case Qt::Vertical:
                    pos = QPoint( 1, height() -( sliderPos +( handleSize().height() /2 ) ) );
                    pos.ry() = qMax( margin, pos.y() );
                    pos.ry() = qMin( height() -hs.height() -margin, pos.y() );
                    break;
            }

            painter.drawEllipse( pos.x(), pos.y(), hs.width(), hs.height() );
        }
    }
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

    setFixedSize( pixOutside.size() );

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

