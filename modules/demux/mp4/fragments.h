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

typedef struct mp4_fragment_t mp4_fragment_t;

struct mp4_fragment_t
{
    uint64_t i_chunk_range_min_offset;
    uint64_t i_chunk_range_max_offset;

    struct
    {
        unsigned int i_track_ID;
        stime_t i_duration; // movie scaled
    } *p_durations;

    unsigned int i_durations;

    MP4_Box_t *p_moox;
    mp4_fragment_t *p_next;
};

typedef struct
{
    mp4_fragment_t moov; /* known fragments (moof following moov) */
    mp4_fragment_t *p_last;
} mp4_fragments_t;

static inline mp4_fragment_t * MP4_Fragment_Moov(mp4_fragments_t *p_fragments)
{
    return &p_fragments->moov;
}

mp4_fragment_t * MP4_Fragment_New( MP4_Box_t *, unsigned );
void MP4_Fragment_Clean(mp4_fragment_t *);

static inline void MP4_Fragment_Delete( mp4_fragment_t *p_fragment )
{
    MP4_Fragment_Clean( p_fragment );
    free( p_fragment );
}

bool MP4_Fragments_Init(mp4_fragments_t *);
void MP4_Fragments_Clean(mp4_fragments_t *);
void MP4_Fragments_Insert(mp4_fragments_t *, mp4_fragment_t *);
void MP4_Fragments_Remove( mp4_fragments_t *, mp4_fragment_t * );

stime_t GetTrackTotalDuration( mp4_fragments_t *p_frags, unsigned int i_track_ID );
mp4_fragment_t * GetFragmentByAtomPos( mp4_fragments_t *p_frags, uint64_t i_pos );
mp4_fragment_t * GetFragmentByPos( mp4_fragments_t *p_frags, uint64_t i_pos, bool b_exact );
mp4_fragment_t * GetFragmentByTime( mp4_fragments_t *p_frags, const mtime_t i_time,
                                    unsigned i_tracks_id, unsigned *pi_tracks_id,
                                    uint32_t i_movie_timescale );
stime_t GetTrackFragmentTimeOffset( mp4_fragments_t *p_frags, mp4_fragment_t *p_fragment,
                                    unsigned int i_track_ID );
void DumpFragments( vlc_object_t *p_obj, mp4_fragments_t *p_frags, uint32_t i_movie_timescale );
#endif
