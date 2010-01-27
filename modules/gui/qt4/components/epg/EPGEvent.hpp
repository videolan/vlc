/*****************************************************************************
 * EPGEvent.h : EPGEvent
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

#ifndef EPGEVENT_H
#define EPGEVENT_H

class QString;
#include <QDateTime>

class EPGEvent
{
public:
    EPGEvent( const QString& eventName )
        : current( false ), updated( true )
    {
        name = eventName;
    }

    QDateTime   start;
    int         duration;
    QString     name;
    QString     description;
    QString     shortDescription;
    QString     channelName;
    bool        current;
    bool        updated;
};

#endif // EPGEVENT_H
