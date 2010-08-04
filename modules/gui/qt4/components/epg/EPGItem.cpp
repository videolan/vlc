/*****************************************************************************
 * EPGItem.cpp: EPGItem
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

#include <QTransform>
#include <QFont>
#include <QFontMetrics>
#include <QDebug>
#include <QDateTime>
#include <QFocusEvent>
#include <QGraphicsScene>

#include "EPGItem.hpp"
#include "EPGView.hpp"
#include "EPGEvent.hpp"

EPGItem::EPGItem( EPGView *view )
    : m_view( view )
{
    m_current = false;

    m_boundingRect.setHeight( TRACKS_HEIGHT );
    setFlags( QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
}

QRectF EPGItem::boundingRect() const
{
    return m_boundingRect;
}

void EPGItem::paint( QPainter *painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    // Draw in view's coordinates
    painter->setWorldMatrixEnabled( false );

    // Draw high-quality items
    //painter->setRenderHint( QPainter::Antialiasing );

    // Get the transformations required to map the text on the viewport
    QTransform viewPortTransform = m_view->viewportTransform();
    QRectF mapped = deviceTransform( viewPortTransform ).mapRect( boundingRect() );

    if ( m_current )
    {
        painter->setBrush( QBrush( QColor( 244, 102, 146 ) ) );
        painter->setPen( QPen( QColor( 244, 102, 146 ) ) );
    }
    else
    {
        painter->setBrush( QBrush( QColor( 201, 217, 242 ) ) );
        painter->setPen( QPen( QColor( 201, 217, 242 ) ) );
    }

    mapped.adjust( 1, 2, -1, -2 );
    painter->drawRoundedRect( mapped, 10, 10 );

    /* Draw text */

    // Setup the font
    QFont f = painter->font();

    // Get the font metrics
    QFontMetrics fm = painter->fontMetrics();

    // Adjust the drawing rect
    mapped.adjust( 6, 6, -6, -6 );

    painter->setPen( Qt::black );
    /* Draw the title. */
    painter->drawText( mapped, Qt::AlignTop | Qt::AlignLeft, fm.elidedText( m_name, Qt::ElideRight, mapped.width() ) );

    mapped.adjust( 0, 20, 0, 0 );

    QDateTime m_end = m_start.addSecs( m_duration );
    f.setPixelSize( 10 );
    f.setItalic( true );
    painter->setFont( f );

    /* Draw the hours. */
    painter->drawText( mapped, Qt::AlignTop | Qt::AlignLeft,
                       fm.elidedText( m_start.toString( "hh:mm" ) + " - " +
                                      m_end.toString( "hh:mm" ),
                                      Qt::ElideRight, mapped.width() ) );
}

const QDateTime& EPGItem::start() const
{
    return m_start;
}

int EPGItem::duration() const
{
    return m_duration;
}

int EPGItem::getChannelNb() const
{
    return m_channelNb;
}

void EPGItem::setChannelNb( int channelNb )
{
    //qDebug() << "Channel" << channelNb;
    m_channelNb = channelNb;
    updatePos();
}

void EPGItem::setStart( const QDateTime& start )
{
    m_start = start;
    updatePos();
}

void EPGItem::setDuration( int duration )
{
    m_duration = duration;
    m_boundingRect.setWidth( duration );
}

void EPGItem::setName( const QString& name )
{
    m_name = name;
}

void EPGItem::setDescription( const QString& description )
{
    m_description = description;
}

void EPGItem::setShortDescription( const QString& shortDescription )
{
    m_shortDescription = shortDescription;
}

void EPGItem::setCurrent( bool current )
{
    m_current = current;
}

void EPGItem::updatePos()
{
    int x = m_view->startTime().secsTo( m_start );
    setPos( x, m_channelNb * TRACKS_HEIGHT );
}

void EPGItem::focusInEvent( QFocusEvent * event )
{
    EPGEvent *evEPG = new EPGEvent( m_name );
    evEPG->description = m_description;
    evEPG->shortDescription = m_shortDescription;
    evEPG->start = m_start;
    evEPG->duration = m_duration;
    m_view->eventFocused( evEPG );
}
