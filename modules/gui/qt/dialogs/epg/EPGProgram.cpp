/*****************************************************************************
 * EPGProgram.cpp:
 ****************************************************************************
 * Copyright © 2016 VideoLAN Authors
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
#include "EPGProgram.hpp"
#include "EPGItem.hpp"
#include "EPGView.hpp"

#include <QtAlgorithms>

EPGProgram::EPGProgram( EPGView *view_, const vlc_epg_t *p_epg ) : QObject( view_ )
{
    name = QString(p_epg->psz_name);
    id = p_epg->i_id;
    sourceid = p_epg->i_source_id;
    view = view_;
    pos = 0;
    current = NULL;
}

EPGProgram::~EPGProgram()
{
    qDeleteAll(eventsbyid);
    eventsbytime.clear();
}

bool EPGProgram::lessThan( const EPGProgram *a, const EPGProgram *b )
{
    return a->pos < b->pos;
}

size_t EPGProgram::getPosition() const
{
    return pos;
}

const EPGItem * EPGProgram::getCurrent() const
{
    return current;
}

const QString & EPGProgram::getName() const
{
    return name;
}

void EPGProgram::setPosition( size_t i )
{
    pos = i;
}

void EPGProgram::activate()
{
    view->activateProgram( sourceid );
}

void EPGProgram::pruneEvents( const QDateTime &date )
{
    QMap<QDateTime, const EPGItem *>::iterator it = eventsbytime.begin();
    for( ; it != eventsbytime.end(); )
    {
        const EPGItem *item = *it;
        if( item->endsBefore( date ) ) /* Expired item ? */
        {
            EPGItem *modifiableitem = eventsbyid.take( item->eventID() );
            view->scene()->removeItem( modifiableitem );
            delete modifiableitem;
            it = eventsbytime.erase( it );
        }
        else break;
    }
}

void EPGProgram::updateEventPos()
{
    foreach( EPGItem *item, eventsbyid )
        item->updatePos();
}

void EPGProgram::updateEvents( const vlc_epg_event_t * const * pp_events, size_t i_events,
                               const vlc_epg_event_t *p_current, QDateTime *mindate )
{
    for( size_t i=0; i<i_events; i++ )
    {
        const vlc_epg_event_t *p_event = pp_events[i];
        QDateTime eventStart = QDateTime::fromTime_t( p_event->i_start );
        if( !mindate->isValid() || eventStart < *mindate )
            *mindate = eventStart;

        EPGItem *epgItem = NULL;
        QHash<uint32_t, EPGItem *>::iterator it = eventsbyid.find( p_event->i_id );
        if ( it != eventsbyid.end() )
        {
            epgItem = *it;

            /* Update our existing programs */
            if( eventStart != epgItem->start() )
            {
                eventsbytime.remove( epgItem->start() );
                eventsbytime.insert( eventStart, epgItem );
            }

            epgItem->setData( p_event ); /* updates our entry */
        }
        else if( !eventsbytime.contains( eventStart ) /* !Inconsistency */ )
        {
            /* Insert a new program entry */
            epgItem = new EPGItem( p_event, view, this );

            /* Effectively insert our new program */
            eventsbyid.insert( p_event->i_id, epgItem );
            eventsbytime.insert( eventStart, epgItem );

            view->scene()->addItem( epgItem );

            /* First Insert, needs to focus by default then */
            if( !view->hasFocus() )
                view->focusItem( epgItem );
        }

        if( p_current == p_event )
            current = epgItem;
    }
}

