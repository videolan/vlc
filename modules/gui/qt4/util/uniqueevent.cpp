/*****************************************************************************
 * Copyright © 2012 VideoLAN
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
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "uniqueevent.hpp"
#include "qt4.hpp"

#include <QTimer>
#include <QApplication>
#include <QMutex>

RateLimitedEventPoster::RateLimitedEventPoster( int i_millisec_interval )
{
    timer = new QTimer();
    mutex = new QMutex();
    timer->setSingleShot( true );
    /* Assuming a 24fps event loop, delays at least events to the next frame */
    if ( i_millisec_interval < 1 )
        i_millisec_interval = 1000 / 48;
    timer->setInterval( i_millisec_interval );
    CONNECT( timer, timeout(), this, commit() );
}

RateLimitedEventPoster::~RateLimitedEventPoster()
{
    timer->stop();
    commit();
    delete timer;
    delete mutex;
}

void RateLimitedEventPoster::postEvent( UniqueEvent *e, QObject *target )
{
    event_tuple newtuple = { target, e };
    mutex->lock();
    foreach( const event_tuple & tuple, eventsList )
    {
        if ( target == tuple.target && tuple.event->equals( e ) )
        {
            delete e;
            mutex->unlock();
            return;
        }
    }
    eventsList << newtuple;
    mutex->unlock();
    if ( eventsList.count() >= 100 ) /* limit lookup time */
    {
        timer->stop();
        commit();
    }
    if ( !timer->isActive() ) timer->start();
}

void RateLimitedEventPoster::commit()
{
    mutex->lock();
    foreach( const event_tuple & tuple, eventsList )
    {
        QApplication::postEvent( tuple.target, tuple.event );
    }
    eventsList.clear();
    mutex->unlock();
}
