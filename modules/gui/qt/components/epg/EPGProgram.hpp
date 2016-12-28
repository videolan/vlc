/*****************************************************************************
 * EPGProgram.hpp:
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
#ifndef EPGPROGRAM_HPP
#define EPGPROGRAM_HPP

#include "qt.hpp"

#include <vlc_epg.h>
#include <QObject>
#include <QMap>
#include <QHash>
#include <QDateTime>

class EPGView;
class EPGItem;

class EPGProgram : public QObject
{
    Q_OBJECT

    public:
        EPGProgram( EPGView *, const vlc_epg_t * );
        virtual ~EPGProgram();

        void pruneEvents( const QDateTime & );
        void updateEvents( const vlc_epg_event_t * const *, size_t, const vlc_epg_event_t *,
                           QDateTime * );
        void updateEventPos();
        size_t getPosition() const;
        void setPosition( size_t );
        void activate();
        const EPGItem * getCurrent() const;
        const QString & getName() const;
        static bool lessThan( const EPGProgram *, const EPGProgram * );

        QHash<uint32_t, EPGItem *> eventsbyid;
        QMap<QDateTime, const EPGItem *> eventsbytime;

    private:
        const EPGItem *current;
        EPGView *view;
        size_t pos;

        QString name;
        uint32_t id;
        uint16_t sourceid;
};

#endif // EPGPROGRAM_HPP
