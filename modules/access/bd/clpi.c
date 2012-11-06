/*****************************************************************************
 * clpi.c: BluRay Disc CLPI
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
#include <limits.h>

#include <vlc_common.h>
#include <vlc_bits.h>
#include "clpi.h"

/* */
void bd_clpi_stc_Parse( bd_clpi_stc_t *p_stc, bs_t *s )
{
    p_stc->i_pcr_pid = bs_read( s, 16 );
    p_stc->i_packet = bs_read( s, 32 );
    p_stc->i_start = bs_read( s, 32 );
    p_stc->i_end = bs_read( s, 32 );
}

void bd_clpi_stream_Parse( bd_clpi_stream_t *p_stream, bs_t *s )
{
    p_stream->i_pid = bs_read( s, 16 );

    const int i_length = bs_read( s, 8 );

    p_stream->i_type = bs_read( s, 8 );

    /* Ignore the rest */
    if( i_length > 1 )
        bs_skip( s, 8*i_length - 8 );
}

void bd_clpi_ep_map_Clean( bd_clpi_ep_map_t *p_ep_map )
{
    free( p_ep_map->p_ep );
}
int bd_clpi_ep_map_Parse( bd_clpi_ep_map_t *p_ep_map,
                          bs_t *s, const int i_ep_map_start )
{
    p_ep_map->i_pid = bs_read( s, 16 );
    bs_skip( s, 10 );
    p_ep_map->i_type = bs_read( s, 4 );

    const int i_coarse = bs_read( s, 16 );
    const int i_fine = bs_read( s, 18 );
    const uint32_t i_coarse_start = bs_read( s, 32 );

    p_ep_map->i_ep = i_fine;
    p_ep_map->p_ep = calloc( i_fine, sizeof(*p_ep_map->p_ep) );
    if( !p_ep_map->p_ep )
        return VLC_EGENERIC;

    bs_t cs = *s;
    bs_skip( &cs, 8*(i_ep_map_start + i_coarse_start) - bs_pos( s ) );

    const uint32_t i_fine_start = bs_read( &cs, 32 );

    for( int i = 0; i < i_coarse; i++ )
    {
        const int      i_fine_id = bs_read( &cs, 18 );
        const int      i_pts = bs_read( &cs, 14 );
        const uint32_t i_packet = bs_read( &cs, 32 );

        for( int j = i_fine_id; j < p_ep_map->i_ep; j++ )
        {
            p_ep_map->p_ep[j].i_pts = (int64_t)(i_pts & ~1) << 19;
            p_ep_map->p_ep[j].i_packet = i_packet & ~( (1 << 17) - 1 );
        }
    }

    bs_t fs = *s;
    bs_skip( &fs, 8*(i_ep_map_start + i_coarse_start + i_fine_start) - bs_pos( s ) );
    for( int i = 0; i < i_fine; i++ )
    {
        const bool b_angle_point = bs_read( &fs, 1 );
        bs_skip( &fs, 3 );  /* I end position offset */
        const int i_pts = bs_read( &fs, 11 );
        const int i_packet = bs_read( &fs, 17 );

        p_ep_map->p_ep[i].b_angle_point = b_angle_point;
        p_ep_map->p_ep[i].i_pts |= i_pts << 9;
        p_ep_map->p_ep[i].i_packet |= i_packet;
    }
    return VLC_SUCCESS;
}

void bd_clpi_Clean( bd_clpi_t *p_clpi )
{
    free( p_clpi->p_stc );

    free( p_clpi->p_stream );

    for( int i = 0; i < p_clpi->i_ep_map; i++ )
        bd_clpi_ep_map_Clean( &p_clpi->p_ep_map[i] );
    free( p_clpi->p_ep_map );
}

int bd_clpi_Parse( bd_clpi_t *p_clpi, bs_t *s, int i_id )
{
    const int i_start = bs_pos( s ) / 8;

    /* */
    if( bs_read( s, 32 ) != 0x48444D56 )
        return VLC_EGENERIC;
    const uint32_t i_version = bs_read( s, 32 );
    if( i_version != 0x30313030 && i_version != 0x30323030 )
        return VLC_EGENERIC;

    /* */
    const uint32_t i_sequence_start = bs_read( s, 32 );
    const uint32_t i_program_start = bs_read( s, 32 );
    const uint32_t i_cpi_start = bs_read( s, 32 );
    bs_skip( s, 32 );   /* mark start */
    bs_skip( s, 32 );   /* extension start */

    /* */
    p_clpi->i_id = i_id;

    /* Read sequence */
    bs_t ss = *s;
    bs_skip( &ss, 8 * ( i_start + i_sequence_start ) - bs_pos( s ) );
    bs_skip( &ss, 32 ); /* Length */
    bs_skip( &ss, 8 );
    bs_skip( &ss, 8 );  /* ATC sequence count (MUST be 1 ?) */
    bs_skip( &ss, 32 ); /* ATC start (MUST be 0) */
    const int i_stc = bs_read( &ss, 8 );
    bs_skip( &ss, 8 );  /* STC ID offset (MUST be 0 ? */

    p_clpi->p_stc = calloc( i_stc, sizeof(*p_clpi->p_stc) );
    for( p_clpi->i_stc = 0; p_clpi->i_stc < i_stc; p_clpi->i_stc++ )
    {
        if( !p_clpi->p_stc )
            break;
        bd_clpi_stc_Parse( &p_clpi->p_stc[p_clpi->i_stc], &ss );
    }

    /* Program */
    bs_t ps = *s;
    bs_skip( &ps, 8 * ( i_start + i_program_start ) - bs_pos( s ) );
    bs_skip( &ps, 32 ); /* Length */
    bs_skip( &ps, 8 );
    bs_skip( &ps, 8 );  /* Program count (MUST be 1 ?) */
    bs_skip( &ps, 32 ); /* Program sequence start (MUST be 0) */
    p_clpi->i_pmt_pid = bs_read( &ps, 16 );
    const int i_stream = bs_read( &ps, 8 );
    bs_skip( &ps, 8 );  /* Group count (MUST be 1 ?) */

    p_clpi->p_stream = calloc( i_stream, sizeof(*p_clpi->p_stream) );
    for( p_clpi->i_stream = 0; p_clpi->i_stream < i_stream; p_clpi->i_stream++ )
    {
        if( !p_clpi->p_stream )
            break;
        bd_clpi_stream_Parse( &p_clpi->p_stream[p_clpi->i_stream], &ps );
    }

    /* Read CPI */
    bs_t cs = *s;
    bs_skip( &cs, 8 * ( i_start + i_cpi_start ) - bs_pos( s ) );

    const uint32_t i_cpi_length = bs_read( &cs, 32 );
    if( i_cpi_length > 0 )
    {
        bs_skip( &cs, 12 );
        bs_skip( &cs, 4 );  /* Type (MUST be 1) */

        /* EPMap */
        const int i_epmap_start = bs_pos( &cs ) / 8;
        bs_skip( &cs, 8 );
        const int i_ep_map = bs_read( &cs, 8 );

        p_clpi->p_ep_map = calloc( i_ep_map, sizeof(*p_clpi->p_ep_map) );
        for( p_clpi->i_ep_map = 0; p_clpi->i_ep_map < i_ep_map; p_clpi->i_ep_map++ )
        {
            if( !p_clpi->p_ep_map )
                break;

            if( bd_clpi_ep_map_Parse( &p_clpi->p_ep_map[p_clpi->i_ep_map],
                                      &cs, i_epmap_start ) )
                break;
        }
    }
    else
    {
        p_clpi->i_ep_map = 0;
        p_clpi->p_ep_map = NULL;
    }
    return VLC_SUCCESS;
}
