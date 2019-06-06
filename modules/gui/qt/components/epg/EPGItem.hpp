/*****************************************************************************
 * EPGItem.hpp : EPGItem
 ****************************************************************************
 * Copyright © 2009-2010 VideoLAN
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

#include "qt.hpp"

#include <vlc_epg.h>
#include <QGraphicsItem>
#include <QDateTime>

class QPainter;
class QString;
class EPGView;
class EPGProgram;

class EPGItem : public QGraphicsItem
{
public:
    EPGItem( const vlc_epg_event_t *data, EPGView *view, EPGProgram * );

    QRectF boundingRect() const Q_DECL_OVERRIDE;
    void paint( QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0 ) Q_DECL_OVERRIDE;

    const QDateTime& start() const;
    QDateTime end() const;

    uint32_t duration() const;
    uint16_t eventID() const;
    const QString& name() const { return m_name; }
    QString description() const;
    int rating() const { return m_rating; }
    bool setData( const vlc_epg_event_t * );
    void setDuration( uint32_t duration );
    void setRating( uint8_t i_rating );
    void updatePos();
    bool endsBefore( const QDateTime & ) const;
    bool playsAt( const QDateTime & ) const;
    const QList<QPair<QString, QString>> &descriptionItems() const;

protected:
    void focusInEvent( QFocusEvent * event ) Q_DECL_OVERRIDE;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent * ) Q_DECL_OVERRIDE;
    void hoverEnterEvent ( QGraphicsSceneHoverEvent * ) Q_DECL_OVERRIDE;
    void hoverLeaveEvent ( QGraphicsSceneHoverEvent * ) Q_DECL_OVERRIDE;

private:
    EPGProgram *program;
    EPGView     *m_view;
    QRectF      m_boundingRect;

    QDateTime   m_start;
    uint32_t    m_duration;
    uint16_t    m_id;
    QString     m_name;
    QString     m_description;
    QString     m_shortDescription;
    QList<QPair<QString, QString>> m_descitems;
    uint8_t     m_rating;
};

#endif // EPGITEM_H
