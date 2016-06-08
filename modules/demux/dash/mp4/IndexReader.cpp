/*
 * IndexReader.cpp
 *****************************************************************************
 * Copyright (C) 2015 - VideoLAN and VLC authors
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "IndexReader.hpp"
#include "../mpd/Representation.h"
#include "../mpd/MPD.h"

using namespace adaptive::mp4;
using namespace dash::mp4;
using namespace dash::mpd;

IndexReader::IndexReader(vlc_object_t *obj)
    : AtomsReader(obj)
{
}

bool IndexReader::parseIndex(block_t *p_block, BaseRepresentation *rep, uint64_t i_fileoffset)
{
    if(!rep || !parseBlock(p_block))
        return false;

    MP4_Box_t *sidxbox = MP4_BoxGet(rootbox, "sidx");
    if (sidxbox)
    {
        Representation::SplitPoint point;
        std::vector<Representation::SplitPoint> splitlist;
        MP4_Box_data_sidx_t *sidx = sidxbox->data.p_sidx;
        /* sidx refers to offsets from end of sidx pos in the file + first offset */
        point.offset = sidx->i_first_offset + i_fileoffset + sidxbox->i_pos + sidxbox->i_size;
        point.time = 0;
        for(uint16_t i=0; i<sidx->i_reference_count && sidx->i_timescale; i++)
        {
            splitlist.push_back(point);
            point.offset += sidx->p_items[i].i_referenced_size;
            point.duration = CLOCK_FREQ * sidx->p_items[i].i_subsegment_duration /
                    sidx->i_timescale;
            point.time += point.duration;
        }
        rep->SplitUsingIndex(splitlist);
        rep->getPlaylist()->debug();
    }

    return true;
}
