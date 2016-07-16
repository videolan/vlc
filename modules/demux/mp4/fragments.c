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

mp4_fragment_t * MP4_Fragment_New( MP4_Box_t *p_atom, unsigned i_tracks )
{
    mp4_fragment_t *p_new = calloc( 1, sizeof(mp4_fragment_t) );
    if( p_new )
    {
        p_new->p_moox = p_atom;
        p_new->p_durations = calloc( i_tracks, sizeof(*p_new->p_durations) );
        if( !p_new->p_durations )
        {
            free( p_new );
            return NULL;
        }
        p_new->i_durations = i_tracks;
    }
    return p_new;
}

void MP4_Fragment_Clean( mp4_fragment_t *p_fragment )
{
    free( p_fragment->p_durations );
}

void MP4_Fragments_Remove( mp4_fragments_t *p_frags, mp4_fragment_t *p_fragment )
{
    mp4_fragment_t *p_current = &p_frags->moov;
    while( p_current )
    {
        if( p_fragment == p_current->p_next )
        {
            p_current->p_next = p_fragment->p_next;
            p_fragment->p_next = NULL;
            if( p_frags->p_last == p_fragment )
                p_frags->p_last = p_current;
            return;
        }
    }
}

void MP4_Fragments_Clean( mp4_fragments_t *p_frags )
{
    while( p_frags->moov.p_next )
    {
        mp4_fragment_t *p_fragment = p_frags->moov.p_next->p_next;
        MP4_Fragment_Clean( p_frags->moov.p_next );
        free( p_frags->moov.p_next );
        p_frags->moov.p_next = p_fragment;
    }
    MP4_Fragment_Clean( &p_frags->moov );
}

void MP4_Fragments_Insert( mp4_fragments_t *p_frags, mp4_fragment_t *p_new )
{
    mp4_fragment_t *p_fragment = p_frags->p_last;
    /* tail append lookup */
    if( p_fragment && p_fragment->p_moox->i_pos < p_new->p_moox->i_pos )
    {
        p_new ->p_next = p_fragment->p_next;
        p_fragment->p_next = p_new;
        p_frags->p_last = p_new;
        return;
    }

    /* start from head */
    p_fragment = MP4_Fragment_Moov( p_frags )->p_next;
    while ( p_fragment && p_fragment->p_moox->i_pos < p_new->p_moox->i_pos )
    {
        p_fragment = p_fragment->p_next;
    }

    if( p_fragment )
    {
        p_new->p_next = p_fragment->p_next;
        p_fragment->p_next = p_new;

        if( p_fragment == p_frags->p_last )
            p_frags->p_last = p_new;
    }
    else
    {
        MP4_Fragment_Moov( p_frags )->p_next = p_new;
        p_frags->p_last = p_new;
    }
}

bool MP4_Fragments_Init( mp4_fragments_t *p_frags )
{
    memset( &p_frags->moov, 0, sizeof(mp4_fragment_t) );
    return true;
}

static stime_t GetTrackDurationInFragment( const mp4_fragment_t *p_fragment,
                                           unsigned int i_track_ID )
{
    for( unsigned int i=0; i<p_fragment->i_durations; i++ )
    {
        if( i_track_ID == p_fragment->p_durations[i].i_track_ID )
            return p_fragment->p_durations[i].i_duration;
    }
    return 0;
}

stime_t GetTrackTotalDuration( mp4_fragments_t *p_frags, unsigned int i_track_ID )
{
    stime_t i_duration = 0;
    const mp4_fragment_t *p_fragment = MP4_Fragment_Moov( p_frags );
    while ( p_fragment && p_fragment->p_durations )
    {
        i_duration += GetTrackDurationInFragment( p_fragment, i_track_ID );
        p_fragment = p_fragment->p_next;
    }
    return i_duration;
}

mp4_fragment_t * GetFragmentByAtomPos( mp4_fragments_t *p_frags, uint64_t i_pos )
{
    mp4_fragment_t *p_fragment = MP4_Fragment_Moov( p_frags );
    do
    {
        if( p_fragment && p_fragment->p_moox && p_fragment->p_moox->i_pos >= i_pos )
        {
            if( p_fragment->p_moox->i_pos != i_pos )
                p_fragment = NULL;
            break;
        }
        p_fragment = p_fragment->p_next;
    } while( p_fragment );
    return p_fragment;
}

mp4_fragment_t * GetFragmentByPos( mp4_fragments_t *p_frags, uint64_t i_pos, bool b_exact )
{
    mp4_fragment_t *p_fragment = MP4_Fragment_Moov( p_frags );
    while ( p_fragment )
    {
        if ( i_pos <= p_fragment->i_chunk_range_max_offset &&
             ( !b_exact || i_pos >= p_fragment->i_chunk_range_min_offset ) )
        {
            return p_fragment;
        }
        else
        {
            p_fragment = p_fragment->p_next;
        }
    }
    return NULL;
}

/* Get a matching fragment data start by clock time */
mp4_fragment_t * GetFragmentByTime( mp4_fragments_t *p_frags, const mtime_t i_time,
                                    unsigned i_tracks_id, unsigned *pi_tracks_id,
                                    uint32_t i_movie_timescale )
{
    const stime_t i_scaled_time = i_time * i_movie_timescale / CLOCK_FREQ;
    mp4_fragment_t *p_fragment = MP4_Fragment_Moov( p_frags );
    stime_t *pi_tracks_fragduration_total = calloc( i_tracks_id, sizeof(stime_t) );
    mtime_t i_segment_start = 0;

    if( p_fragment->i_chunk_range_max_offset == 0 )
        p_fragment = p_fragment->p_next;

    while ( p_fragment && pi_tracks_fragduration_total )
    {
        mtime_t i_segment_end = 0;
        for( unsigned int i=0; i<i_tracks_id; i++ )
        {
            pi_tracks_fragduration_total[i] += GetTrackDurationInFragment( p_fragment, pi_tracks_id[i] );
            i_segment_end = __MAX(i_segment_end, pi_tracks_fragduration_total[i]);
        }

        if ( i_scaled_time >= i_segment_start &&
             i_scaled_time <= i_segment_end )
        {
            free( pi_tracks_fragduration_total );
            return p_fragment;
        }
        else
        {
            i_segment_start = i_segment_end; /* end = next segment start */
            p_fragment = p_fragment->p_next;
        }
    }

    free( pi_tracks_fragduration_total );
    return NULL;
}

/* Returns fragment scaled time offset */
stime_t GetTrackFragmentTimeOffset( mp4_fragments_t *p_frags, mp4_fragment_t *p_fragment,
                                     unsigned int i_track_ID )
{
    stime_t i_base_scaledtime = 0;
    mp4_fragment_t *p_current = MP4_Fragment_Moov( p_frags );
    while ( p_current != p_fragment )
    {
        if ( p_current != MP4_Fragment_Moov( p_frags ) ||
             p_current->i_chunk_range_max_offset )
        {
            i_base_scaledtime += GetTrackDurationInFragment( p_current, i_track_ID );
        }
        p_current = p_current->p_next;
    }
    return i_base_scaledtime;
}

void DumpFragments( vlc_object_t *p_obj, mp4_fragments_t *p_frags, uint32_t i_movie_timescale )
{
    mtime_t i_total_duration = 0;
    const mp4_fragment_t *p_fragment = MP4_Fragment_Moov( p_frags );

    while ( p_fragment )
    {
        char *psz_durations = NULL;
        mtime_t i_frag_duration = 0;
        for( unsigned i=0; i<p_fragment->i_durations; i++ )
        {
            if( i==0 )
                i_frag_duration += CLOCK_FREQ * p_fragment->p_durations[0].i_duration / i_movie_timescale;

            char *psz_duration = NULL;
            if( 0 < asprintf( &psz_duration, "%s [%u]%"PRId64" ",
                      (psz_durations) ? psz_durations : "",
                      p_fragment->p_durations[i].i_track_ID,
                      CLOCK_FREQ * p_fragment->p_durations[i].i_duration / i_movie_timescale ) )
            {
                free( psz_durations );
                psz_durations = psz_duration;
            }
        }

        msg_Dbg( p_obj, "fragment offset %"PRId64", data %"PRIu64"<->%"PRIu64" @%"PRId64", durations %s",
                 p_fragment->p_moox->i_pos, p_fragment->i_chunk_range_min_offset, p_fragment->i_chunk_range_max_offset,
                 i_total_duration, psz_durations );

        free( psz_durations );

        i_total_duration += i_frag_duration;
        p_fragment = p_fragment->p_next;
    }
}
