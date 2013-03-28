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
#include <QRectF>

EPGGraphicsScene::EPGGraphicsScene( QObject *parent ) : QGraphicsScene( parent )
{}

void EPGGraphicsScene::drawBackground( QPainter *painter, const QRectF &rect)
{
    EPGView *epgView = qobject_cast<EPGView *>(parent());

    /* day change */
    QDateTime rectstarttime = epgView->startTime().addSecs( rect.left() );
    QDateTime nextdaylimit = QDateTime( rectstarttime.date() );
    QRectF area( rect );
    while( area.left() < width() )
    {
        nextdaylimit = nextdaylimit.addDays( 1 );
        area.setRight( epgView->startTime().secsTo( nextdaylimit ) );

        if ( epgView->startTime().date().daysTo( nextdaylimit.date() ) % 2 != 0 )
            painter->fillRect( area, palette().color( QPalette::Base ) );
        else
            painter->fillRect( area, palette().color( QPalette::AlternateBase ) );

        area.setLeft( area.right() + 1 );
    }

    /* channels lines */
    painter->setPen( QPen( QColor( 224, 224, 224 ) ) );
    for( int y = rect.top() + TRACKS_HEIGHT ; y < rect.bottom() ; y += TRACKS_HEIGHT )
       painter->drawLine( QLineF( rect.left(), y, rect.right(), y ) );

    /* current hour line */
    int x = epgView->startTime().secsTo( epgView->baseTime() );
    painter->setPen( QPen( QColor( 255, 192, 192 ) ) );
    painter->drawLine( QLineF( x, rect.top(), x, rect.bottom() ) );
}

EPGView::EPGView( QWidget *parent ) : QGraphicsView( parent )
{
    setContentsMargins( 0, 0, 0, 0 );
    setFrameStyle( QFrame::Box );
    setAlignment( Qt::AlignLeft | Qt::AlignTop );

    m_startTime = QDateTime::currentDateTime();

    EPGGraphicsScene *EPGscene = new EPGGraphicsScene( this );

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
    mutex.lock();
    foreach( EPGEventByTimeQMap *epgItemByTime, epgitemsByChannel.values() )
    {
        foreach( EPGItem *epgItem, epgItemByTime->values() )
        {
            epgItem->updatePos();
        }
    }
    mutex.unlock();
}

void EPGView::updateChannels()
{
    /* Make sure our items goes to the correct row */
    unsigned int channelIndex = 0;
    mutex.lock();
    foreach( EPGEventByTimeQMap *epgItemByTime, epgitemsByChannel.values() )
    {
        foreach( EPGItem *epgItem, epgItemByTime->values() )
            epgItem->setRow( channelIndex );
        channelIndex++;
    }
    mutex.unlock();
}

const QDateTime& EPGView::startTime() const
{
    return m_startTime;
}

const QDateTime& EPGView::baseTime() const
{
    return m_baseTime;
}

bool EPGView::hasValidData() const
{
    return !epgitemsByChannel.isEmpty();
}

static void cleanOverlapped( EPGEventByTimeQMap *epgItemByTime, EPGItem *epgItem, QGraphicsScene *scene )
{
    QDateTime epgItemTime = epgItem->start();
    QDateTime epgItemTimeEnd = epgItem->end();
    /* Clean overlapped programs */
    foreach(const QDateTime existingTimes, epgItemByTime->keys())
    {
        if ( existingTimes > epgItemTimeEnd ) break; /* Can't overlap later items */
        if ( existingTimes != epgItemTime )
        {
            EPGItem *otherEPGItem = epgItemByTime->value( existingTimes );
            if ( otherEPGItem->playsAt( epgItemTime.addSecs( 1 ) )
                || /* add/minus one sec because next one can start at prev end min */
                 otherEPGItem->playsAt( epgItemTimeEnd.addSecs( -1 ) ) )
            {
                epgItemByTime->remove( otherEPGItem->start() );
                scene->removeItem( otherEPGItem );
                delete otherEPGItem;
            }
        }
    }
}

bool EPGView::addEPGEvent( vlc_epg_event_t *eventdata, QString channelName, bool b_current )
{
    /* Init our nested map if required */
    EPGEventByTimeQMap *epgItemByTime;
    EPGItem *epgItem;
    bool b_refresh_channels = false;

    QDateTime eventStart = QDateTime::fromTime_t( eventdata->i_start );
    if ( eventStart.addSecs( eventdata->i_duration ) < m_baseTime )
        return false; /* EPG feed sent expired item */
    if ( eventStart < m_startTime )
    {
        m_startTime = eventStart;
        emit startTimeChanged( m_startTime );
    }

    mutex.lock();
    if ( !epgitemsByChannel.contains( channelName ) )
    {
        epgItemByTime = new EPGEventByTimeQMap();
        epgitemsByChannel.insert( channelName, epgItemByTime );
        emit channelAdded( channelName );
        b_refresh_channels = true;
    } else {
        epgItemByTime = epgitemsByChannel.value( channelName );
    }

    if ( epgItemByTime->contains( eventStart ) )
    {
        /* Update our existing programs */
        epgItem = epgItemByTime->value( eventStart );
        epgItem->setCurrent( b_current );
        if ( epgItem->setData( eventdata ) ) /* updates our entry */
            cleanOverlapped( epgItemByTime, epgItem, scene() );
        mutex.unlock();
        return false;
    } else {
        /* Insert a new program entry */
        epgItem = new EPGItem( eventdata, this );
        cleanOverlapped( epgItemByTime, epgItem, scene() );
        /* Effectively insert our new program */
        epgItem->setCurrent( b_current );
        epgItemByTime->insert( eventStart, epgItem );
        scene()->addItem( epgItem );
        /* update only our row (without calling the updatechannels()) */
        epgItem->setRow( epgitemsByChannel.keys().indexOf( channelName ) );

        /* First Insert, needs to focus by default then */
        if ( epgitemsByChannel.keys().count() == 1 &&
             epgItemByTime->count() == 1 )
            focusItem( epgItem );
    }
    mutex.unlock();

    /* Update rows on each item */
    if ( b_refresh_channels ) updateChannels();

    return true;
}

void EPGView::removeEPGEvent( vlc_epg_event_t *eventdata, QString channelName )
{
    EPGEventByTimeQMap *epgItemByTime;
    QDateTime eventStart = QDateTime::fromTime_t( eventdata->i_start );
    EPGItem *epgItem;
    bool b_update_channels = false;

    mutex.lock();
    if ( epgitemsByChannel.contains( channelName ) )
    {
        epgItemByTime = epgitemsByChannel.value( channelName );

        if ( epgItemByTime->contains( eventStart ) )
        { /* delete our EPGItem */
            epgItem = epgItemByTime->value( eventStart );
            epgItemByTime->remove( eventStart );
            scene()->removeItem( epgItem );
            delete epgItem;
        }

        if ( epgItemByTime->keys().isEmpty() )
        { /* Now unused channel */
            epgitemsByChannel.remove( channelName );
            delete epgItemByTime;
            emit channelRemoved( channelName );
            b_update_channels = true;
        }
    }
    mutex.unlock();

    if ( b_update_channels ) updateChannels();
}

void EPGView::reset()
{
    /* clean our items storage and remove them from the scene */
    EPGEventByTimeQMap *epgItemByTime;
    EPGItem *epgItem;
    mutex.lock();
    foreach( const QString &channelName, epgitemsByChannel.keys() )
    {
        epgItemByTime = epgitemsByChannel[ channelName ];
        foreach( const QDateTime &key, epgItemByTime->keys() )
        {
            epgItem = epgItemByTime->value( key );
            scene()->removeItem( epgItem );
            epgItemByTime->remove( key );
            delete epgItem;
        }
        epgitemsByChannel.remove( channelName );
        delete epgItemByTime;
        emit channelRemoved( channelName ); /* notify others */
    }
    mutex.unlock();
}

void EPGView::cleanup()
{
    /* remove expired items and clear their current flag */
    EPGEventByTimeQMap *epgItemByTime;
    EPGItem *epgItem;
    m_baseTime = QDateTime::currentDateTime();
    QDateTime lowestTime = m_baseTime;
    bool b_timechanged = false;
    bool b_update_channels = false;

    mutex.lock();
    foreach( const QString &channelName, epgitemsByChannel.keys() )
    {
        epgItemByTime = epgitemsByChannel[ channelName ];
        foreach( const QDateTime &key, epgItemByTime->keys() )
        {
            epgItem = epgItemByTime->value( key );
            if ( epgItem->endsBefore( baseTime() ) ) /* Expired item ? */
            {
                scene()->removeItem( epgItem );
                epgItemByTime->remove( key );
                delete epgItem;
            } else {
                epgItem->setCurrent( false ); /* if stream doesn't update */
                if ( lowestTime > epgItem->start() )
                {
                    lowestTime = epgItem->start(); /* update our reference */
                    b_timechanged = true;
                }
            }
        }

        if ( epgItemByTime->keys().isEmpty() )
        { /* Now unused channel */
            epgitemsByChannel.remove( channelName );
            delete epgItemByTime;
            emit channelRemoved( channelName );
            b_update_channels = true;
        }
    }
    mutex.unlock();

    if ( b_timechanged )
    {
        m_startTime = lowestTime;
        emit startTimeChanged( m_startTime );
    }

    if ( b_update_channels ) updateChannels();
}

EPGView::~EPGView()
{
    reset();
}

void EPGView::updateDuration()
{
    QDateTime maxItemTime;
    mutex.lock();
    foreach( EPGEventByTimeQMap *epgItemByTime, epgitemsByChannel.values() )
        foreach( EPGItem *epgItem, epgItemByTime->values() )
            if ( epgItem->end() > maxItemTime ) maxItemTime = epgItem->end();
    mutex.unlock();
    m_duration = m_startTime.secsTo( maxItemTime );
    emit durationChanged( m_duration );
}

void EPGView::focusItem( EPGItem *epgItem )
{
    emit itemFocused( epgItem );
}
