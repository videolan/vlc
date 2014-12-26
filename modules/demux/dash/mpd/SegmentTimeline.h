/*****************************************************************************
 * SegmentTimeline.cpp: Implement the SegmentTimeline tag.
 *****************************************************************************
 * Copyright (C) 1998-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Hugo Beauzée-Luyssen <beauze.h@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef SEGMENTTIMELINE_H
#define SEGMENTTIMELINE_H

#include <sys/types.h>
#include <list>
#include <stdint.h>

namespace dash
{
    namespace mpd
    {
        class SegmentTimeline
        {
            public:
                struct  Element
                {
                    Element();
                    Element( const Element& e );
                    int64_t     t;
                    int64_t     d;
                    int         r;
                };
                SegmentTimeline();
                ~SegmentTimeline();
                int                     getTimescale() const;
                void                    setTimescale( int timescale );
                void                    addElement( Element* e );
                const Element*          getElement( unsigned int index ) const;

            private:
                int                     timescale;
                std::list<Element*>     elements;
        };
    }
}

#endif // SEGMENTTIMELINE_H
