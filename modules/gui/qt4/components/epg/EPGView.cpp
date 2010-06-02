/*****************************************************************************
 * EPGView.cpp: EPGView
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

#include "EPGView.hpp"
#include "EPGItem.hpp"

#include <QDateTime>
#include <QMatrix>
#include <QPaintEvent>
#include <QScrollBar>
#include <QtDebug>
#include <QGraphicsTextItem>

EPGView::EPGView( QWidget *parent ) : QGraphicsView( parent )
{
    setContentsMargins( 0, 0, 0, 0 );
    setFrameStyle( QFrame::NoFrame );
    setAlignment( Qt::AlignLeft | Qt::AlignTop );
    setViewportUpdateMode( QGraphicsView::FullViewportUpdate );

    m_startTime = QDateTime::currentDateTime();

    QGraphicsScene *EPGscene = new QGraphicsScene( this );

    setScene( EPGscene );

    connect( horizontalScrollBar(), SIGNAL( valueChanged(int) ),
             this, SLOT( updateOverlayPosition(int) ) );

    m_overlay = EPGscene->addRect( 0, 0, 100, 1, QPen(), QBrush( QColor( 40, 86, 255, 220 ) ) );
    m_overlay->setFlag( QGraphicsItem::ItemIgnoresTransformations );
    m_overlay->setZValue( 100 );

    sceneRectChanged( scene()->sceneRect() );

    connect( scene(), SIGNAL( sceneRectChanged(QRectF) ),
             this, SLOT( sceneRectChanged(QRectF) ) );
}

void EPGView::updateOverlayPosition( int value )
{
    int pos = value * matrix().inverted().m11();
    m_overlay->setPos( pos, 0 );
}

void EPGView::setScale( double scaleFactor )
{
    m_scaleFactor = scaleFactor;
    QMatrix matrix;
    matrix.scale( scaleFactor, 1 );
    setMatrix( matrix );
}

void EPGView::setStartTime( const QDateTime& startTime )
{
    QList<QGraphicsItem*> itemList = items();

    int diff = startTime.secsTo( m_startTime );

    for ( int i = 0; i < itemList.count(); ++i )
    {
        EPGItem* item = dynamic_cast<EPGItem*>( itemList.at( i ) );
        if ( !item ) continue;
        item->setStart( item->start().addSecs( diff ) );
    }

    m_startTime = startTime;

    // Our start time has changed
    emit startTimeChanged( startTime );
}

const QDateTime& EPGView::startTime()
{
    return m_startTime;
}

void EPGView::addEvent( EPGEvent* event )
{
    if ( !m_channels.contains( event->channelName ) )
    {
        m_channels.append( event->channelName );
        QGraphicsTextItem* channelTitle = new QGraphicsTextItem( event->channelName, m_overlay );
        channelTitle->setZValue( 101 );
        channelTitle->setPos( 0, m_channels.indexOf( event->channelName ) * TRACKS_HEIGHT );
        channelTitle->setTextWidth( 100 );
    }

    EPGItem* item = new EPGItem( this );
    item->setChannel( m_channels.indexOf( event->channelName ) );
    item->setStart( event->start );
    item->setDuration( event->duration );
    item->setName( event->name );
    item->setDescription( event->description );
    item->setShortDescription( event->shortDescription );
    item->setCurrent( event->current );

    scene()->addItem( item );
}

void EPGView::updateEvent( EPGEvent* event )
{
    //qDebug() << "Update event: " << event->name;
}

void EPGView::delEvent( EPGEvent* event )
{
    //qDebug() << "Del event: " << event->name;
}

void EPGView::drawBackground( QPainter *painter, const QRectF &rect )
{
    painter->setPen( QPen( QColor( 72, 72, 72 ) ) );

    QPointF p = mapToScene( width(), 0 );

    int y = 0;
    for ( int i = 0; i < m_channels.count() + 1; ++i )
    {
        painter->drawLine( 0,
                           y * TRACKS_HEIGHT,
                           p.x(),
                           y * TRACKS_HEIGHT );
        ++y;
    }
}

void EPGView::updateDuration()
{
    QDateTime lastItem;
    QList<QGraphicsItem*> list = items();

    for ( int i = 0; i < list.count(); ++i )
    {
        EPGItem* item = dynamic_cast<EPGItem*>( list.at( i ) );
        if ( !item ) continue;
        QDateTime itemEnd = item->start().addSecs( item->duration() );

        if ( itemEnd > lastItem )
            lastItem = itemEnd;
    }
    m_duration = m_startTime.secsTo( lastItem );
    emit durationChanged( m_duration );
}

void EPGView::eventFocused( EPGEvent *ev )
{
    emit eventFocusedChanged( ev );
}

void EPGView::sceneRectChanged( const QRectF& rect )
{
    m_overlay->setRect( 0, 0, m_overlay->rect().width(), rect.height() );
}
