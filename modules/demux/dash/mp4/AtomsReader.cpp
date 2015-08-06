/*
 * AtomsReader.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN and VLC authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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
#include "AtomsReader.hpp"
#include "../mpd/Representation.h"
#include "../mpd/MPD.h"

using namespace dash::mp4;
using namespace dash::mpd;

AtomsReader::AtomsReader(vlc_object_t *object_)
{
    object = object_;
    rootbox = NULL;
}

AtomsReader::~AtomsReader()
{
    while(rootbox && rootbox->p_first)
    {
        MP4_Box_t *p_next = rootbox->p_first->p_next;
        MP4_BoxFree( (stream_t *)object, rootbox->p_first );
        rootbox->p_first = p_next;
    }
    delete rootbox;
}

bool AtomsReader::parseBlock(block_t *p_block, BaseRepresentation *rep)
{
    if(!rep)
        return false;

    stream_t *stream = stream_MemoryNew( object, p_block->p_buffer, p_block->i_buffer, true);
    if (stream)
    {
        rootbox = new MP4_Box_t;
        if(!rootbox)
        {
            stream_Delete(stream);
            return false;
        }
        memset(rootbox, 0, sizeof(*rootbox));
        rootbox->i_type = ATOM_root;
        rootbox->i_size = p_block->i_buffer;
        if ( MP4_ReadBoxContainerChildren( stream, rootbox, 0 ) == 1 )
        {
#ifndef NDEBUG
            MP4_BoxDumpStructure(stream, rootbox);
#endif
            MP4_Box_t *sidxbox = MP4_BoxGet(rootbox, "sidx");
            if (sidxbox)
            {
                Representation::SplitPoint point;
                std::vector<Representation::SplitPoint> splitlist;
                MP4_Box_data_sidx_t *sidx = sidxbox->data.p_sidx;
                point.offset = sidx->i_first_offset;
                point.time = 0;
                for(uint16_t i=0; i<sidx->i_reference_count && sidx->i_timescale; i++)
                {
                    splitlist.push_back(point);
                    point.offset += sidx->p_items[i].i_referenced_size;
                    point.time += CLOCK_FREQ * sidx->p_items[i].i_subsegment_duration /
                                  sidx->i_timescale;
                }
                rep->SplitUsingIndex(splitlist);
                rep->getPlaylist()->debug();
            }
        }
        stream_Delete(stream);
    }

    return true;
}
