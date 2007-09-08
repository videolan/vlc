/*****************************************************************************
 * directslider.hpp : A slider that goes where you click
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          with precious help from ahigerd on #qt@freenode
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#ifndef _DIRECTSLIDER_H_
#define _DIRECTSLIDER_H_

#include <QSlider>
#include <QMouseEvent>
#include <QLayout>

class DirectSlider : public QSlider
{
public:
    DirectSlider( QWidget *_parent ) : QSlider( _parent ) {};
    DirectSlider( Qt::Orientation q,QWidget *_parent ) : QSlider( q,_parent )
    {};
    virtual ~DirectSlider()   {};

    void mousePressEvent(QMouseEvent* event)
    {
        if( event->button() != Qt::LeftButton && event->button() != Qt::MidButton )
        {
            QSlider::mousePressEvent( event );
            return;
        }

        QMouseEvent newEvent( event->type(), event->pos(), event->globalPos(),
                Qt::MouseButton( event->button() ^ Qt::LeftButton ^ Qt::MidButton ),
                Qt::MouseButtons( event->buttons() ^ Qt::LeftButton ^ Qt::MidButton ),
                event->modifiers() );
        QSlider::mousePressEvent( &newEvent );
    }
};
#endif
