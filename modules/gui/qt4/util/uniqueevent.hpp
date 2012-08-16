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
#ifndef UNIQUEEVENT_HPP
#define UNIQUEEVENT_HPP

#include <QObject>
#include <QEvent>
#include <QList>
class QTimer;

class UniqueEvent : public QEvent
{
public:
    UniqueEvent( QEvent::Type type ) : QEvent( type ) {};
    virtual bool equals( UniqueEvent *e ) const = 0;
};

class RateLimitedEventPoster : public QObject
{
    Q_OBJECT

public:
    RateLimitedEventPoster( int i_millisec_interval = -1 );
    ~RateLimitedEventPoster();
    void postEvent( UniqueEvent *e, QObject *target );

private slots:
    void commit();

private:
    struct event_tuple
    {
        QObject *target;
        UniqueEvent *event;
    };
    QList<event_tuple> eventsList;
    QTimer *timer;
};

#endif // UNIQUEEVENT_HPP
