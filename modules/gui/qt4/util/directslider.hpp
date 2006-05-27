/*****************************************************************************
 * directslider.hpp : A slider that goes where you click
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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

class DirectSlider : public QSlider
{
public:
    DirectSlider( QWidget *_parent ) : QSlider( _parent ) {};
    DirectSlider( Qt::Orientation q,QWidget *_parent ) : QSlider( q,_parent )
    {};
    virtual ~DirectSlider()   {};

    void mousePressEvent(QMouseEvent* event)
    {
        if(event->button() == Qt::LeftButton)
        {
            int pos = (int)(minimum() + 
                          (double)(event->x())/width()*(maximum()-minimum()) );
            setSliderPosition( pos );
            QSlider::mousePressEvent(event);
        }
    }
};
#endif
