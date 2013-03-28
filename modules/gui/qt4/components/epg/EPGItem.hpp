/*****************************************************************************
 * EPGItem.hpp : EPGItem
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

#ifndef EPGITEM_H
#define EPGITEM_H

#include "qt4.hpp"

#include <vlc_epg.h>
#include <QGraphicsItem>
#include <QDateTime>

class QPainter;
class QString;

class EPGView;

class EPGItem : public QGraphicsItem
{
public:
    EPGItem( vlc_epg_event_t *data, EPGView *view );

    virtual QRectF boundingRect() const;
    virtual void paint( QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0 );

    const QDateTime& start() const;
    QDateTime end() const;

    int duration() const;
    const QString& name() const { return m_name; }
    QString description() const;
    int rating() const { return m_rating; }
    bool setData( vlc_epg_event_t * );
    void setRow( unsigned int );
    void setCurrent( bool );
    void setDuration( int duration );
    void setRating( uint8_t i_rating );
    void updatePos();
    bool endsBefore( const QDateTime & ) const;
    bool playsAt( const QDateTime & ) const;

protected:
    virtual void focusInEvent( QFocusEvent * event );
    virtual void hoverEnterEvent ( QGraphicsSceneHoverEvent * );
    virtual void hoverLeaveEvent ( QGraphicsSceneHoverEvent * );

private:
    EPGView     *m_view;
    QRectF      m_boundingRect;
    unsigned int i_row;

    QDateTime   m_start;
    int         m_duration;
    QString     m_name;
    QString     m_description;
    QString     m_shortDescription;
    bool        m_current;
    uint8_t     m_rating;
};

#endif // EPGITEM_H
