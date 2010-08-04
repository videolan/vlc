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
    setFrameStyle( QFrame::Box );
    setAlignment( Qt::AlignLeft | Qt::AlignTop );

    m_startTime = QDateTime::currentDateTime();

    QGraphicsScene *EPGscene = new QGraphicsScene( this );

    setScene( EPGscene );
}

void EPGView::setScale( double scaleFactor )
{
    m_scaleFactor = scaleFactor;
    QMatrix matrix;
    matrix.scale( scaleFactor, 1 );
    setMatrix( matrix );
}

void EPGView::updateStartTime()
{
    QList<QGraphicsItem*> itemList = items();

    /* Set the new start time. */
    for ( int i = 0; i < itemList.count(); ++i )
    {
        EPGItem* item = qgraphicsitem_cast<EPGItem*>( itemList.at( i ) );
        if ( !item )
            continue;
        if( i == 0 )
            m_startTime = item->start();
        if ( item->start() < m_startTime )
            m_startTime = item->start();
    }

    /* Update the position of all items. */
    for ( int i = 0; i < itemList.count(); ++i )
    {
        EPGItem* item = qgraphicsitem_cast<EPGItem*>( itemList.at( i ) );
        if ( !item )
            continue;
        item->updatePos();
    }

    // Our start time may have changed.
    emit startTimeChanged( m_startTime );
}

const QDateTime& EPGView::startTime()
{
    return m_startTime;
}

void EPGView::addEvent( EPGEvent* event )
{
    if ( !m_channels.contains( event->channelName ) )
        m_channels.append( event->channelName );

    EPGItem* item = new EPGItem( this );
    item->setChannelNb( m_channels.indexOf( event->channelName ) );
    item->setStart( event->start );
    item->setDuration( event->duration );
    item->setName( event->name );
    item->setDescription( event->description );
    item->setShortDescription( event->shortDescription );
    item->setCurrent( event->current );

    event->item = item;

    scene()->addItem( item );
}

void EPGView::delEvent( EPGEvent* event )
{
    if( event->item == NULL )
        return;

    int channelNb = event->item->getChannelNb();

    // Remove the item.
    scene()->removeItem( event->item );
    event->item = NULL;

    // Look if the channel is still used by other events.
    QList<QGraphicsItem*> itemList = items();
    bool b_used = false;
    for( int i = 0; i < itemList.count(); ++i )
    {
        EPGItem* item = qgraphicsitem_cast<EPGItem*>( itemList.at( i ) );
        if ( !item )
            continue;
        if( item->getChannelNb() == channelNb )
        {
            b_used = true;
            break;
        }
    }

    // If the channel is no more used, then we remove it from the list
    // and decrease the channel number of the concerned items.
    if( !b_used )
    {
        m_channels.removeAt( channelNb );
        for( int i = 0; i < itemList.count(); ++i )
        {
            EPGItem* item = qgraphicsitem_cast<EPGItem*>( itemList.at( i ) );
            if ( !item )
                continue;
            int itemChannelNb = item->getChannelNb();
            if( itemChannelNb > channelNb )
                item->setChannelNb( itemChannelNb - 1 );
        }
    }
}

void EPGView::updateDuration()
{
    QDateTime lastItem;
    QList<QGraphicsItem*> list = items();

    for ( int i = 0; i < list.count(); ++i )
    {
        EPGItem* item = qgraphicsitem_cast<EPGItem*>( list.at( i ) );
        if ( !item ) continue;
        QDateTime itemEnd = item->start().addSecs( item->duration() );

        if ( itemEnd > lastItem )
            lastItem = itemEnd;
    }
    m_duration = m_startTime.secsTo( lastItem );
    emit durationChanged( m_duration );
}

QList<QString> EPGView::getChannelList()
{
    return m_channels;
}

void EPGView::eventFocused( EPGEvent *ev )
{
    emit eventFocusedChanged( ev );
}
