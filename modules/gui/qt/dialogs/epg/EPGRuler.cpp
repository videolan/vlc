/*****************************************************************************
 * EPGRuler.cpp: EPGRuler
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
 *
 * Authors: Ludovic Fauvet <etix@l0cal.com>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "EPGRuler.hpp"

class QPaintEvent;
#include <QPainter>
#include <QDateTime>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QPalette>
#include <QColor>

EPGRuler::EPGRuler( QWidget* parent )
    : QWidget( parent )
{
    setContentsMargins( 0, 0, 0, 0 );
    setMinimumHeight( height() );
    setMaximumHeight( height() );
    m_offset = 0;
    m_scale = 1.0;
}

void EPGRuler::setScale( double scale )
{
    m_scale = scale;
    update();
}

void EPGRuler::setRange( const QDateTime& startTime, const QDateTime& endTime )
{
    if( !startTime.isValid() || !endTime.isValid() )
    {
        m_startTime = QDateTime();
    }
    else
    {
        m_startTime = startTime;
        m_duration = startTime.secsTo( endTime );
    }
    update();
}

void EPGRuler::setOffset( int offset )
{
    m_offset = offset;
    update();
}

void EPGRuler::paintEvent( QPaintEvent *event )
{
    Q_UNUSED( event );
    const QSize margin( 0, contentsMargins().top() );
    const QSize header( 0, maximumHeight() - contentsMargins().top() );
    const int spacing = m_scale * 3600;
    QPainter p( this );

    if( !m_startTime.isValid() )
    {
        QWidget::paintEvent( event );
        return;
    }

    QDateTime localStartTime;
    localStartTime = m_startTime.addSecs( m_offset / m_scale );

    QDateTime diff( localStartTime );
    diff.setTime( QTime( localStartTime.time().hour(), 0, 0, 0 ) );

    int secondsToHour = localStartTime.secsTo( diff );

    /* draw hour blocks, with right bound being hourmark */
    QPoint here( secondsToHour * m_scale, margin.height() );
    QPoint previous( -1, 0 );
    QDateTime current( localStartTime.addSecs( secondsToHour ) );
    current = current.addSecs( -3600 );

    QColor fillColor;
    while ( here.rx() < width() + spacing )
    {
        QRect area( QPoint( previous.x() + 1, margin.height() ), here );
        area.adjust( 0, 0, 0, header.height() );
        QString timeString = current.toString( "hh'h'" );
        /* Show Day */
        if ( current.time().hour() == 0 )
            timeString += current.date().toString( " ddd dd" );

        if ( m_startTime.date().daysTo( current.date() ) % 2 == 0 )
            fillColor = palette().color( QPalette::Window );
        else
            fillColor = palette().color( QPalette::Dark );
        p.fillRect( area, fillColor );
        p.drawLine( area.topRight(), area.bottomRight() );
        p.drawText( area, Qt::AlignLeft, timeString );
        previous = here;
        here.rx() += spacing;
        current = current.addSecs( 3600 );
    }

    /* draw current time line */
    here.rx() = localStartTime.secsTo( QDateTime::currentDateTime() ) * m_scale;
    if ( here.x() <= width() && here.x() >= 0 )
    {
        p.setPen( QPen( QColor( 255, 0 , 0, 128 ) ) );
        p.drawLine( here, QPoint( here.x(), here.y() + header.height() ) );
    }
}
