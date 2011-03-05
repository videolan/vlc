/*****************************************************************************
 * EPGItem.h : EPGItem
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

#include <QGraphicsItem>

class QPainter;
class QString;
class QDateTime;

class EPGView;
class EPGEvent;

class EPGItem : public QGraphicsItem
{
public:
    EPGItem( EPGView *view );
    virtual ~EPGItem() { }

    virtual QRectF boundingRect() const;
    virtual void paint( QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0 );

    const QDateTime& start() const;

    int duration() const;
    int getChannelNb() const;

    void setData( EPGEvent * );
    void setChannelNb( int channelNb );
    void setDuration( int duration );
    void updatePos();

protected:
    virtual void focusInEvent( QFocusEvent * event );
    virtual void hoverEnterEvent ( QGraphicsSceneHoverEvent * );

private:
    EPGView     *m_view;
    QRectF      m_boundingRect;
    int         m_channelNb;

    /*FIXME: Bad object design. We shouldn't need to clone this EPGEvent data */
    QDateTime   m_start;
    int         m_duration;
    QString     m_name;
    QString     m_description;
    QString     m_shortDescription;
    bool        m_current;
    bool        m_simultaneous;
};

#endif // EPGITEM_H
