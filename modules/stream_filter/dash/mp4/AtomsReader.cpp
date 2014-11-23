/*
 * AtomsReader.cpp
 *****************************************************************************
 * Copyright (C) 2014 - VideoLAN authors
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
#include "mpd/Representation.h"
#include "mpd/MPD.h"

using namespace dash::mp4;
using namespace dash::mpd;

AtomsReader::AtomsReader(ISegment *segment_)
{
    segment = segment_;
    rootbox = NULL;
}

AtomsReader::~AtomsReader()
{
    vlc_object_t *object = segment->getRepresentation()->getMPD()->getVLCObject();
    while(rootbox && rootbox->p_first)
    {
        MP4_Box_t *p_next = rootbox->p_first->p_next;
        MP4_BoxFree( (stream_t *)object, rootbox->p_first );
        rootbox->p_first = p_next;
    }
    delete rootbox;
}

bool AtomsReader::parseBlock(void *buffer, size_t size)
{
    if(!segment->getRepresentation())
        return false;
    vlc_object_t *object = segment->getRepresentation()->getMPD()->getVLCObject();
    stream_t *stream = stream_MemoryNew( object, (uint8_t *)buffer, size, true);
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
        rootbox->i_size = size;
        if ( MP4_ReadBoxContainerChildren( stream, rootbox, 0 ) == 1 )
        {
#ifndef NDEBUG
            MP4_BoxDumpStructure(stream, rootbox);
#endif
            MP4_Box_t *sidxbox = MP4_BoxGet(rootbox, "sidx");
            if (sidxbox)
            {
                MP4_Box_data_sidx_t *sidx = sidxbox->data.p_sidx;
                size_t offset = sidx->i_first_offset;
                for(uint16_t i=0; i<sidx->i_reference_count; i++)
                {
                    std::cerr << " offset " << offset << std::endl;
                    offset += sidx->p_items[i].i_referenced_size;
                }
            }
            std::cerr << "index seg " << ((uint8_t *)buffer)[4] << ((uint8_t *)buffer)[5] << std::endl;
        }
        stream_Delete(stream);
    }

    return true;
}
