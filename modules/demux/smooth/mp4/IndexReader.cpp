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
#include "../../adaptive/playlist/BaseRepresentation.h"
#include "../../adaptive/playlist/BaseAdaptationSet.h"
#include "../../adaptive/playlist/SegmentTemplate.h"
#include "../../adaptive/playlist/SegmentTimeline.h"
#include "../../adaptive/playlist/AbstractPlaylist.hpp"

using namespace adaptive::mp4;
using namespace smooth::mp4;

IndexReader::IndexReader(vlc_object_t *obj)
    : AtomsReader(obj)
{
}

bool IndexReader::parseIndex(block_t *p_block, BaseRepresentation *rep)
{
    if(!rep || !parseBlock(p_block))
        return false;

    /* Do track ID fixup */
    const MP4_Box_t *tfhd_box = MP4_BoxGet( rootbox, "moof/traf/tfhd" );
    if ( tfhd_box )
        SetDWBE( &p_block->p_buffer[tfhd_box->i_pos + 8 + 4], 0x01 );

    if(!rep->getPlaylist()->isLive())
        return true;

    const MP4_Box_t *uuid_box = MP4_BoxGet( rootbox, "moof/traf/uuid" );
    while( uuid_box && uuid_box->i_type == ATOM_uuid )
    {
        if ( !CmpUUID( &uuid_box->i_uuid, &TfrfBoxUUID ) )
            break;
        uuid_box = uuid_box->p_next;
    }

    if(!uuid_box)
        return false;

    SegmentTimeline *timelineadd = new (std::nothrow) SegmentTimeline(rep->inheritTimescale());
    if (timelineadd)
    {
        const MP4_Box_data_tfrf_t *p_tfrfdata = uuid_box->data.p_tfrf;
        for ( uint8_t i=0; i<p_tfrfdata->i_fragment_count; i++ )
        {
            uint64_t dur = p_tfrfdata->p_tfrf_data_fields[i].i_fragment_duration;
            uint64_t stime = p_tfrfdata->p_tfrf_data_fields[i].i_fragment_abs_time;
            timelineadd->addElement(i+1, dur, 0, stime);
        }

        rep->mergeWithTimeline(timelineadd);
        delete timelineadd;

#ifndef NDEBUG
        msg_Dbg(rep->getPlaylist()->getVLCObject(), "Updated timeline from tfrf");
        rep->debug(rep->getPlaylist()->getVLCObject(), 0);
#endif
    }

    return true;
}
