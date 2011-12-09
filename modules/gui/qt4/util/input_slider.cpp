/*****************************************************************************
 * input_slider.cpp : VolumeSlider and SeekSlider
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ludovic Fauvet <etix@videolan.org>
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

#include "qt4.hpp"

#include "util/input_slider.hpp"
#include "adapters/seekpoints.hpp"
#include <vlc_aout_intf.h>

#include <QPaintEvent>
#include <QPainter>
#include <QBitmap>
#include <QPainter>
#include <QStyleOptionSlider>
#include <QLinearGradient>
#include <QTimer>
#include <QRadialGradient>
#include <QLinearGradient>
#include <QSize>
#include <QPalette>
#include <QColor>
#include <QPoint>
#include <QPropertyAnimation>
#include <QApplication>

#define MINIMUM 0
#define MAXIMUM 1000
#define CHAPTERSSPOTSIZE 3
#define FADEDURATION 300
#define FADEOUTDELAY 2000

SeekSlider::SeekSlider( QWidget *_parent ) : QSlider( _parent )
{
    SeekSlider( Qt::Horizontal, _parent );
}

SeekSlider::SeekSlider( Qt::Orientation q, QWidget *_parent )
          : QSlider( q, _parent )
{
    b_isSliding = false;
    f_buffering = 1.0;
    mHandleOpacity = 1.0;
    chapters = NULL;

    /* Timer used to fire intermediate updatePos() when sliding */
    seekLimitTimer = new QTimer( this );
    seekLimitTimer->setSingleShot( true );

    /* Tooltip bubble */
    mTimeTooltip = new TimeTooltip( this );
    mTimeTooltip->setMouseTracking( true );

    /* Properties */
    setRange( MINIMUM, MAXIMUM );
    setSingleStep( 2 );
    setPageStep( 10 );
    setMouseTracking( true );
    setTracking( true );
    setFocusPolicy( Qt::NoFocus );

    /* Init to 0 */
    setPosition( -1.0, 0, 0 );
    secstotimestr( psz_length, 0 );

    animHandle = new QPropertyAnimation( this, "handleOpacity", this );
    animHandle->setDuration( FADEDURATION );
    animHandle->setStartValue( 0.0 );
    animHandle->setEndValue( 1.0 );

    hideHandleTimer = new QTimer( this );
    hideHandleTimer->setSingleShot( true );
    hideHandleTimer->setInterval( FADEOUTDELAY );

    CONNECT( this, sliderMoved( int ), this, startSeekTimer() );
    CONNECT( seekLimitTimer, timeout(), this, updatePos() );
    CONNECT( hideHandleTimer, timeout(), this, hideHandle() );
    mTimeTooltip->installEventFilter( this );
}

SeekSlider::~SeekSlider()
{
    delete chapters;
}

/***
 * \brief Sets the chapters seekpoints adapter
 *
 * \params SeekPoints initilized with current intf thread
***/
void SeekSlider::setChapters( SeekPoints *chapters_ )
{
    delete chapters;
    chapters = chapters_;
    chapters->setParent( this );
}

/***
 * \brief Main public method, superseeding setValue. Disabling the slider when neeeded
 *
 * \param pos Position, between 0 and 1. -1 disables the slider
 * \param time Elapsed time. Unused
 * \param legnth Duration time.
 ***/
void SeekSlider::setPosition( float pos, int64_t time, int length )
{
    VLC_UNUSED(time);
    if( pos == -1.0 )
    {
        setEnabled( false );
        b_isSliding = false;
    }
    else
        setEnabled( true );

    if( !b_isSliding )
        setValue( (int)( pos * 1000.0 ) );

    inputLength = length;
}

void SeekSlider::startSeekTimer()
{
    /* Only fire one update, when sliding, every 150ms */
    if( b_isSliding && !seekLimitTimer->isActive() )
        seekLimitTimer->start( 150 );
}

void SeekSlider::updatePos()
{
    float f_pos = (float)( value() ) / 1000.0;
    emit sliderDragged( f_pos ); /* Send new position to VLC's core */
}

void SeekSlider::updateBuffering( float f_buffering_ )
{
    f_buffering = f_buffering_;
    repaint();
}

void SeekSlider::mouseReleaseEvent( QMouseEvent *event )
{
    event->accept();
    b_isSliding = false;
    seekLimitTimer->stop(); /* We're not sliding anymore: only last seek on release */
    if ( b_is_jumping )
    {
        b_is_jumping = false;
        return;
    }
    QSlider::mouseReleaseEvent( event );
    updatePos();
}

void SeekSlider::mousePressEvent( QMouseEvent* event )
{
    /* Right-click */
    if( event->button() != Qt::LeftButton &&
        event->button() != Qt::MidButton )
    {
        QSlider::mousePressEvent( event );
        return;
    }

    b_is_jumping = false;
    /* handle chapter clicks */
    int i_width = size().width();
    if ( chapters && inputLength && i_width)
    {
        if ( orientation() == Qt::Horizontal ) /* TODO: vertical */
        {
             /* only on chapters zone */
            if ( event->y() < CHAPTERSSPOTSIZE ||
                 event->y() > ( size().height() - CHAPTERSSPOTSIZE ) )
            {
                QList<SeekPoint> points = chapters->getPoints();
                int i_selected = -1;
                bool b_startsnonzero = false; /* as we always starts at 1 */
                if ( points.count() > 0 ) /* do we need an extra offset ? */
                    b_startsnonzero = ( points.at(0).time > 0 );
                int i_min_diff = i_width + 1;
                for( int i = 0 ; i < points.count() ; i++ )
                {
                    int x = points.at(i).time / 1000000.0 / inputLength * i_width;
                    int diff_x = abs( x - event->x() );
                    if ( diff_x < i_min_diff )
                    {
                        i_min_diff = diff_x;
                        i_selected = i + ( ( b_startsnonzero )? 1 : 0 );
                    } else break;
                }
                if ( i_selected && i_min_diff < 4 ) // max 4px around mark
                {
                    chapters->jumpTo( i_selected );
                    event->accept();
                    b_is_jumping = true;
                    return;
                }
            }
        }
    }

    b_isSliding = true ;
    setValue( QStyle::sliderValueFromPosition( MINIMUM, MAXIMUM, event->x(), width(), false ) );
    event->accept();
}

void SeekSlider::mouseMoveEvent( QMouseEvent *event )
{
    if( b_isSliding )
    {
        setValue( QStyle::sliderValueFromPosition( MINIMUM, MAXIMUM, event->x(), width(), false) );
        emit sliderMoved( value() );
    }

    /* Tooltip */
    if ( inputLength > 0 )
    {
        int posX = qMax( rect().left(), qMin( rect().right(), event->x() ) );

        QString chapterLabel;
        QPoint p( event->globalX() - ( event->x() - posX ) - ( mTimeTooltip->width() / 2 ),
                  QWidget::mapToGlobal( pos() ).y() - ( mTimeTooltip->height() + 2 ) );

        if ( orientation() == Qt::Horizontal ) /* TODO: vertical */
        {
                QList<SeekPoint> points = chapters->getPoints();
                int i_selected = -1;
                bool b_startsnonzero = false;
                if ( points.count() > 0 )
                    b_startsnonzero = ( points.at(0).time > 0 );
                for( int i = 0 ; i < points.count() ; i++ )
                {
                    int x = points.at(i).time / 1000000.0 / inputLength * size().width();
                    if ( event->x() >= x )
                        i_selected = i + ( ( b_startsnonzero )? 1 : 0 );
                }
                if ( i_selected >= 0 )
                    chapterLabel = points.at( i_selected ).name;
        }

        secstotimestr( psz_length, ( posX * inputLength ) / size().width() );
        mTimeTooltip->setText( psz_length, chapterLabel );
        mTimeTooltip->move( p );
    }
    event->accept();
}

void SeekSlider::wheelEvent( QWheelEvent *event )
{
    /* Don't do anything if we are for somehow reason sliding */
    if( !b_isSliding )
    {
        setValue( value() + event->delta() / 12 ); /* 12 = 8 * 15 / 10
         Since delta is in 1/8 of ° and mouse have steps of 15 °
         and that our slider is in 0.1% and we want one step to be a 1%
         increment of position */
        emit sliderDragged( value() / 1000.0 );
    }
    event->accept();
}

void SeekSlider::enterEvent( QEvent * )
{
    /* Cancel the fade-out timer */
    hideHandleTimer->stop();
    /* Only start the fade-in if needed */
    if( animHandle->direction() != QAbstractAnimation::Forward )
    {
        /* If pause is called while not running Qt will complain */
        if( animHandle->state() == QAbstractAnimation::Running )
            animHandle->pause();
        animHandle->setDirection( QAbstractAnimation::Forward );
        animHandle->start();
    }
    /* Don't show the tooltip if the slider is disabled or a menu is open */
    if( isEnabled() && inputLength > 0 && !qApp->activePopupWidget() )
        mTimeTooltip->show();
}

void SeekSlider::leaveEvent( QEvent * )
{
    hideHandleTimer->start();
    /* Hide the tooltip
       - if the mouse leave the slider rect (Note: it can still be
         over the tooltip!)
       - if another window is on the way of the cursor */
    if( !rect().contains( mapFromGlobal( QCursor::pos() ) ) ||
      ( !isActiveWindow() && !mTimeTooltip->isActiveWindow() ) )
    {
        mTimeTooltip->hide();
    }
}

void SeekSlider::hideEvent( QHideEvent * )
{
    mTimeTooltip->hide();
}

bool SeekSlider::eventFilter( QObject *obj, QEvent *event )
{
    if( obj == mTimeTooltip )
    {
        if( event->type() == QEvent::Leave ||
            event->type() == QEvent::MouseMove )
        {
            QMouseEvent *e = static_cast<QMouseEvent*>( event );
            if( !rect().contains( mapFromGlobal( e->globalPos() ) ) )
                mTimeTooltip->hide();
        }
        return false;
    }
    else
        return QSlider::eventFilter( obj, event );
}

QSize SeekSlider::sizeHint() const
{
    return ( orientation() == Qt::Horizontal ) ? QSize( 100, 18 )
                                               : QSize( 18, 100 );
}

QSize SeekSlider::handleSize() const
{
    const int size = ( orientation() == Qt::Horizontal ? height() : width() );
    return QSize( size, size );
}

void SeekSlider::paintEvent( QPaintEvent *event )
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
    QRect barRect       = rect();

    // adjust positions based on the current orientation
    if ( option.sliderPosition != 0 )
    {
        switch ( orientation() )
        {
            case Qt::Horizontal:
                sliderPos = ( ( (qreal)width() ) / (qreal)range )
                        * (qreal)option.sliderPosition;
                break;
            case Qt::Vertical:
                sliderPos = ( ( (qreal)height() ) / (qreal)range )
                        * (qreal)option.sliderPosition;
                break;
        }
    }

    switch ( orientation() )
    {
        case Qt::Horizontal:
            barRect.setHeight( handleSize().height() /2 );
            break;
        case Qt::Vertical:
            barRect.setWidth( handleSize().width() /2 );
            break;
    }

    barRect.moveCenter( rect().center() );

    // set the background color and gradient
    QColor backgroundBase( 135, 135, 135 );
    QLinearGradient backgroundGradient( 0, 0, 0, height() );
    backgroundGradient.setColorAt( 0.0, backgroundBase );
    backgroundGradient.setColorAt( 1.0, backgroundBase.lighter( 150 ) );

    // set the foreground color and gradient
    QColor foregroundBase( 50, 156, 255 );
    QLinearGradient foregroundGradient( 0, 0, 0, height() );
    foregroundGradient.setColorAt( 0.0,  foregroundBase );
    foregroundGradient.setColorAt( 1.0,  foregroundBase.darker( 140 ) );

    // draw a slight 3d effect on the bottom
    painter.setPen( QColor( 230, 230, 230 ) );
    painter.setBrush( Qt::NoBrush );
    painter.drawRoundedRect( barRect.adjusted( 0, 2, 0, 0 ), barCorner, barCorner );

    // draw background
    painter.setPen( Qt::NoPen );
    painter.setBrush( backgroundGradient );
    painter.drawRoundedRect( barRect, barCorner, barCorner );

    // adjusted foreground rectangle
    QRect valueRect = barRect.adjusted( 1, 1, -1, 0 );

    switch ( orientation() )
    {
        case Qt::Horizontal:
            valueRect.setWidth( qMin( width(), int( sliderPos ) ) );
            break;
        case Qt::Vertical:
            valueRect.setHeight( qMin( height(), int( sliderPos ) ) );
            valueRect.moveBottom( rect().bottom() );
            break;
    }

    if ( option.sliderPosition > minimum() && option.sliderPosition <= maximum() )
    {
        // draw foreground
        painter.setPen( Qt::NoPen );
        painter.setBrush( foregroundGradient );
        painter.drawRoundedRect( valueRect, barCorner, barCorner );
    }

    // draw buffering overlay
    if ( f_buffering < 1.0 )
    {
        QRect innerRect = barRect.adjusted( 1, 1,
                            barRect.width() * ( -1.0 + f_buffering ) - 1, 0 );
        QColor overlayColor = QColor( "Orange" );
        overlayColor.setAlpha( 128 );
        painter.setBrush( overlayColor );
        painter.drawRoundedRect( innerRect, barCorner, barCorner );
    }

    if ( option.state & QStyle::State_MouseOver || isAnimationRunning() )
    {
        /* draw chapters tickpoints */
        if ( chapters && inputLength && size().width() )
        {
            QColor background = palette().color( QPalette::Active, QPalette::Background );
            QColor foreground = palette().color( QPalette::Active, QPalette::WindowText );
            foreground.setHsv( foreground.hue(),
                            ( background.saturation() + foreground.saturation() ) / 2,
                            ( background.value() + foreground.value() ) / 2 );
            if ( orientation() == Qt::Horizontal ) /* TODO: vertical */
            {
                QList<SeekPoint> points = chapters->getPoints();
                foreach( SeekPoint point, points )
                {
                    int x = point.time / 1000000.0 / inputLength * size().width();
                    painter.setPen( foreground );
                    painter.setBrush( Qt::NoBrush );
                    painter.drawLine( x, 0, x, CHAPTERSSPOTSIZE );
                    painter.drawLine( x, height(), x, height() - CHAPTERSSPOTSIZE );
                }
            }
        }

        // draw handle
        if ( sliderPos != -1 )
        {
            const int margin = 0;
            QSize hSize = handleSize() - QSize( 6, 6 );
            QPoint pos;

            switch ( orientation() )
            {
                case Qt::Horizontal:
                    pos = QPoint( sliderPos - ( hSize.width() / 2 ), 2 );
                    pos.rx() = qMax( margin, pos.x() );
                    pos.rx() = qMin( width() - hSize.width() - margin, pos.x() );
                    break;
                case Qt::Vertical:
                    pos = QPoint( 2, height() - ( sliderPos + ( hSize.height() / 2 ) ) );
                    pos.ry() = qMax( margin, pos.y() );
                    pos.ry() = qMin( height() - hSize.height() - margin, pos.y() );
                    break;
            }

            QPalette p;
            QPoint shadowPos( pos - QPoint( 2, 2 ) );
            QSize sSize( handleSize() - QSize( 2, 2 ) );

            // prepare the handle's gradient
            QLinearGradient handleGradient( 0, 0, 0, hSize.height() );
            handleGradient.setColorAt( 0.0, p.midlight().color() );
            handleGradient.setColorAt( 0.9, p.mid().color() );

            // prepare the handle's shadow gradient
            QColor shadowDark( p.shadow().color().darker( 150 ) );
            QColor shadowLight( p.shadow().color().lighter( 180 ) );
            shadowLight.setAlpha( 50 );

            QRadialGradient shadowGradient( shadowPos.x() + ( sSize.width() / 2 ),
                                            shadowPos.y() + ( sSize.height() / 2 ),
                                            qMax( sSize.width(), sSize.height() ) / 2 );
            shadowGradient.setColorAt( 0.4, shadowDark );
            shadowGradient.setColorAt( 1.0, shadowLight );

            painter.setPen( Qt::NoPen );
            painter.setOpacity( mHandleOpacity );

            // draw the handle's shadow
            painter.setBrush( shadowGradient );
            painter.drawEllipse( shadowPos.x(), shadowPos.y() + 1, sSize.width(), sSize.height() );

            // finally draw the handle
            painter.setBrush( handleGradient );
            painter.drawEllipse( pos.x(), pos.y(), hSize.width(), hSize.height() );
        }
    }
}

qreal SeekSlider::handleOpacity() const
{
    return mHandleOpacity;
}

void SeekSlider::setHandleOpacity(qreal opacity)
{
    mHandleOpacity = opacity;
    /* Request a new paintevent */
    update();
}

void SeekSlider::hideHandle()
{
    /* If pause is called while not running Qt will complain */
    if( animHandle->state() == QAbstractAnimation::Running )
        animHandle->pause();
    /* Play the animation backward */
    animHandle->setDirection( QAbstractAnimation::Backward );
    animHandle->start();
}

bool SeekSlider::isAnimationRunning() const
{
    return animHandle->state() == QAbstractAnimation::Running
            || hideHandleTimer->isActive();
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
    if( colorList.count() < 12 )
        for( int i = colorList.count(); i < 12; i++)
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
    add_colors( gradient, gradient2, 0.45, 3, 4, 5 );
    add_colors( gradient, gradient2, 0.55, 6, 7, 8 );
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

    QColor background = palette().color( QPalette::Active, QPalette::Background );
    QColor foreground = palette().color( QPalette::Active, QPalette::WindowText );
    foreground.setHsv( foreground.hue(),
                    ( background.saturation() + foreground.saturation() ) / 2,
                    ( background.value() + foreground.value() ) / 2 );
    painter.setPen( foreground );
    QFont font; font.setPixelSize( 9 );
    painter.setFont( font );
    const QRect rect( 0, 0, 34, 15 );
    painter.drawText( rect, Qt::AlignRight | Qt::AlignVCenter,
                      QString::number( value() ) + '%' );

    painter.end();
    e->accept();
}
