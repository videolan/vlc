/*****************************************************************************
 * EPGRuler.cpp: EPGRuler
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
 * $Id$
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

#include <QPainter>
#include <QFont>
#include <QPaintEvent>
#include <QtDebug>
#include <QDateTime>

EPGRuler::EPGRuler( QWidget* parent )
    : QWidget( parent )
{
    setContentsMargins( 0, 0, 0, 0 );
    setMinimumHeight( 30 );
    setMaximumHeight( 30 );
    m_offset = 0;
}

void EPGRuler::setScale( double scale )
{
    m_scale = scale;
    update();
}

void EPGRuler::setStartTime( const QDateTime& startTime )
{
    m_startTime = startTime;
    update();
}

void EPGRuler::setDuration( int duration )
{
    m_duration = duration;
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

    QPainter p( this );

    int secondsOffset = m_offset / m_scale;

    QDateTime localStartTime;
    localStartTime = m_startTime.addSecs( secondsOffset );

    QDateTime diff( localStartTime );
    diff.setTime( QTime( localStartTime.time().hour(), 0, 0, 0 ) );

    int secondsToHour = localStartTime.secsTo( diff );

    QDateTime current( localStartTime.addSecs( secondsToHour ) );

    int spacing = ( m_scale * 60 ) * 60;
    int posx = secondsToHour * m_scale;

    // Count the number of items to draw
    int itemsToDraw = ( width() / spacing ) + 1;

    for ( ; itemsToDraw >= 0; --itemsToDraw )
    {
        p.drawLine( posx, 15, posx, 30 );
        p.drawText( posx + 1, 12, 50, 15, Qt::AlignLeft, current.toString( "hh'h'" ) );
        posx += spacing;
        current = current.addSecs( 60 * 60 );
    }

    /* draw current time line */
    posx = localStartTime.secsTo( QDateTime::currentDateTime() ) * m_scale;
    if ( posx <= width() && posx >= 0 )
    {
        QPen pen( QPen( QColor( 255, 0 , 0, 128 ) ) );
        pen.setWidth( 3 );
        p.setPen( pen );
        p.drawLine( posx - 1, 15, posx - 1, 30 );
    }
}
