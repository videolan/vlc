/*****************************************************************************
 * EPGView.hpp : EPGView
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

#ifndef EPGVIEW_H
#define EPGVIEW_H

#include "EPGItem.hpp"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QDateTime>

#define TRACKS_HEIGHT 60

typedef QMap<QDateTime, EPGItem *> EPGEventByTimeQMap;
typedef QMap<QString, EPGEventByTimeQMap* > EPGTimeMapByChannelQMap;

class EPGGraphicsScene : public QGraphicsScene
{
Q_OBJECT
public:
    explicit EPGGraphicsScene( QObject *parent = 0 );
protected:
    void drawBackground ( QPainter *, const QRectF &);
};

class EPGView : public QGraphicsView
{
Q_OBJECT

public:
    explicit EPGView( QWidget *parent = 0 );
    ~EPGView();

    void            setScale( double scaleFactor );

    void            updateStartTime();
    const QDateTime& startTime() const;
    const QDateTime& baseTime() const;

    bool            addEPGEvent( vlc_epg_event_t*, QString, bool );
    void            removeEPGEvent( vlc_epg_event_t*, QString );
    void            updateDuration();
    void            reset();
    void            cleanup();
    bool            hasValidData() const;

signals:
    void            startTimeChanged( const QDateTime& startTime );
    void            durationChanged( int seconds );
    void            itemFocused( EPGItem * );
    void            channelAdded( QString );
    void            channelRemoved( QString );
protected:

    QDateTime       m_startTime;
    QDateTime       m_baseTime;
    int             m_scaleFactor;
    int             m_duration;

public slots:
    void            focusItem( EPGItem * );
private:
    EPGTimeMapByChannelQMap epgitemsByChannel;
    void updateChannels();
    QMutex mutex;
};

#endif // EPGVIEW_H
