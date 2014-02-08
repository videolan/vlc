/*****************************************************************************
 * animators.cpp: Object animators
 ****************************************************************************
 * Copyright (C) 2006-2014 the VideoLAN team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "animators.hpp"

#include <QWidget>
#include <QPixmap>

PixmapAnimator::PixmapAnimator( QWidget *parent, QList<QString> frames )
    : QAbstractAnimation( parent ), current_frame( 0 )
{
    foreach( QString name, frames )
        pixmaps.append( new QPixmap( name ) );
    currentPixmap = pixmaps.at( 0 );
    setFps( frames.count() ); /* default to 1 sec loop */
    setLoopCount( -1 );
}

void PixmapAnimator::updateCurrentTime( int msecs )
{
    int i = msecs / interval;
    if ( i >= pixmaps.count() ) i = pixmaps.count() - 1; /* roundings */
    if ( i != current_frame )
    {
        current_frame = i;
        currentPixmap = pixmaps.at( current_frame );
        emit pixmapReady( *currentPixmap );
    }
}

