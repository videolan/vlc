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
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneHoverEvent>
#include <QStyle>

#include "EPGItem.hpp"
#include "EPGView.hpp"
#include "EPGEvent.hpp"

EPGItem::EPGItem( EPGView *view )
    : m_view( view )
{
    m_current = false;
    m_simultaneous = false;
    m_boundingRect.setHeight( TRACKS_HEIGHT );
    setFlags( QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemIsFocusable);
    setAcceptHoverEvents( true );
}

QRectF EPGItem::boundingRect() const
{
    return m_boundingRect;
}

void EPGItem::paint( QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget*)
{
    QPen pen;
    QColor gradientColor;
    QLinearGradient gradient( m_boundingRect.topLeft(), m_boundingRect.bottomLeft() );

    // Draw in view's coordinates
    painter->setWorldMatrixEnabled( false );

    // Draw high-quality items
    //painter->setRenderHint( QPainter::Antialiasing );

    // Get the transformations required to map the text on the viewport
    QTransform viewPortTransform = m_view->viewportTransform();
    QRectF mapped = deviceTransform( viewPortTransform ).mapRect( boundingRect() );

    if ( m_current || m_simultaneous )
        gradientColor.setRgb( 244, 125, 0 , m_simultaneous ? 192 : 255 );
    else
        gradientColor.setRgb( 201, 217, 242 );

    gradient.setColorAt( 0.0, gradientColor.lighter( 120 ) );
    gradient.setColorAt( 1.0, gradientColor );

    pen.setColor( option->state & QStyle::State_MouseOver || hasFocus()
                  ? QColor( 0, 0, 0 ) : QColor( 192, 192, 192 ) );

    pen.setStyle( option->state & QStyle::State_MouseOver && !hasFocus()
                  ? Qt::DashLine : Qt::SolidLine );

    painter->setBrush( QBrush( gradient ) );
    painter->setPen( pen );
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

void EPGItem::setData( EPGEvent *event )
{
    m_start = event->start;
    m_name = event->name;
    m_description = event->description;
    m_shortDescription = event->shortDescription;
    m_current = event->current;
    m_simultaneous = event->simultaneous;
    setDuration( event->duration );
    updatePos();
    setToolTip( m_name );
    update();
}

void EPGItem::setDuration( int duration )
{
    m_duration = duration;
    m_boundingRect.setWidth( duration );
}

void EPGItem::updatePos()
{
    int x = m_view->startTime().secsTo( m_start );
    setPos( x, m_channelNb * TRACKS_HEIGHT );
}

void EPGItem::hoverEnterEvent ( QGraphicsSceneHoverEvent * event )
{
    event->accept();
    update();
}

void EPGItem::focusInEvent( QFocusEvent * event )
{
    EPGEvent *evEPG = new EPGEvent( m_name );
    evEPG->description = m_description;
    evEPG->shortDescription = m_shortDescription;
    evEPG->start = m_start;
    evEPG->duration = m_duration;
    m_view->eventFocused( evEPG );
    update();
}
