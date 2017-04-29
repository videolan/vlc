/*****************************************************************************
 * fragments.h : MP4 fragments
 *****************************************************************************
 * Copyright (C) 2001-2015 VLC authors and VideoLAN
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
#ifndef VLC_MP4_FRAGMENTS_H_
#define VLC_MP4_FRAGMENTS_H_

#include <vlc_common.h>
#include "libmp4.h"

typedef struct mp4_fragments_index_t
{
    uint64_t *pi_pos;
    stime_t  *p_times; // movie scaled
    unsigned i_entries;
    stime_t i_last_time; // movie scaled
    unsigned i_tracks;
} mp4_fragments_index_t;

void MP4_Fragments_Index_Delete( mp4_fragments_index_t *p_index );
mp4_fragments_index_t * MP4_Fragments_Index_New( unsigned i_tracks, unsigned i_num );

stime_t MP4_Fragment_Index_GetTrackStartTime( mp4_fragments_index_t *p_index,
                                              unsigned i_track_index, uint64_t i_moof_pos );
stime_t MP4_Fragment_Index_GetTrackDuration( mp4_fragments_index_t *p_index, unsigned i_track_index );

bool MP4_Fragments_Index_Lookup( mp4_fragments_index_t *p_index,
                                 stime_t *pi_time, uint64_t *pi_pos, unsigned i_track_index );

#ifdef MP4_VERBOSE
void MP4_Fragments_Index_Dump( vlc_object_t *p_obj, const mp4_fragments_index_t *p_index,
                                uint32_t i_movie_timescale );
#endif

#endif
