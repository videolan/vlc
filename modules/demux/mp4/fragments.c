/*****************************************************************************
 * fragments.c : MP4 fragments
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "fragments.h"
#include <limits.h>

void MP4_Fragments_Index_Delete( mp4_fragments_index_t *p_index )
{
    if( p_index )
    {
        free( p_index->pi_pos );
        free( p_index->p_times );
        free( p_index );
    }
}

mp4_fragments_index_t * MP4_Fragments_Index_New( unsigned i_tracks, unsigned i_num )
{
    if( !i_tracks || !i_num || SIZE_MAX / i_num < i_tracks )
        return NULL;
    mp4_fragments_index_t *p_index = malloc( sizeof(*p_index) );
    if( p_index )
    {
        p_index->p_times = calloc( (size_t)i_num * i_tracks, sizeof(*p_index->p_times) );
        p_index->pi_pos = calloc( i_num, sizeof(*p_index->pi_pos) );
        if( !p_index->p_times || !p_index->pi_pos )
        {
            MP4_Fragments_Index_Delete( p_index );
            return NULL;
        }
        p_index->i_entries = i_num;
        p_index->i_last_time = 0;
        p_index->i_tracks = i_tracks;
    }
    return p_index;
}

stime_t MP4_Fragment_Index_GetTrackStartTime( mp4_fragments_index_t *p_index,
                                              unsigned i_track_index, uint64_t i_moof_pos )
{
    for( size_t i=0; i<p_index->i_entries; i++ )
    {
        if( p_index->pi_pos[i] >= i_moof_pos )
            return p_index->p_times[i * p_index->i_tracks + i_track_index];
    }
    return 0;
}

stime_t MP4_Fragment_Index_GetTrackDuration( mp4_fragments_index_t *p_index, unsigned i )
{
    return p_index->p_times[(size_t)(p_index->i_entries - 1) * p_index->i_tracks + i];
}

bool MP4_Fragments_Index_Lookup( mp4_fragments_index_t *p_index, stime_t *pi_time,
                                 uint64_t *pi_pos, unsigned i_track_index )
{
    if( *pi_time >= p_index->i_last_time || p_index->i_entries < 1 ||
        i_track_index >= p_index->i_tracks )
        return false;

    for( size_t i=1; i<p_index->i_entries; i++ )
    {
        if( p_index->p_times[i * p_index->i_tracks + i_track_index] > *pi_time )
        {
            *pi_time = p_index->p_times[(i - 1) * p_index->i_tracks + i_track_index];
            *pi_pos = p_index->pi_pos[i - 1];
            return true;
        }
    }

    *pi_time = p_index->p_times[(size_t)(p_index->i_entries - 1) * p_index->i_tracks];
    *pi_pos = p_index->pi_pos[p_index->i_entries - 1];
    return true;
}

#ifdef MP4_VERBOSE
void MP4_Fragments_Index_Dump( vlc_object_t *p_obj, const mp4_fragments_index_t *p_index,
                               uint32_t i_movie_timescale )
{
    for( size_t i=0; i<p_index->i_entries; i++ )
    {
        char *psz_starts = NULL;

        stime_t i_end;
        if( i + 1 == p_index->i_entries )
            i_end = p_index->i_last_time;
        else
            i_end = p_index->p_times[(i + 1) * p_index->i_tracks];

        for( unsigned j=0; j<p_index->i_tracks; j++ )
        {
            char *psz_start = NULL;
            if( 0 < asprintf( &psz_start, "%s [%u]%"PRId64"ms ",
                      (psz_starts) ? psz_starts : "", j,
                  INT64_C( 1000 ) * p_index->p_times[i * p_index->i_tracks + j] / i_movie_timescale ) )
            {
                free( psz_starts );
                psz_starts = psz_start;
            }
        }

        msg_Dbg( p_obj, "fragment offset @%"PRId64" %"PRId64"ms, start %s",
                 p_index->pi_pos[i],
                 INT64_C( 1000 ) * i_end / i_movie_timescale, psz_starts );

        free( psz_starts );
    }
}
#endif
