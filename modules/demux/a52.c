/*****************************************************************************
 * a52.c : raw A/52 stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2007 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_demux.h>
#include <vlc_codec.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( N_("Raw A/52 demuxer") );
    set_capability( "demux", 145 );
    set_callbacks( Open, Close );
    add_shortcut( "a52" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

struct demux_sys_t
{
    bool  b_start;
    es_out_id_t *p_es;

    /* Packetizer */
    decoder_t *p_packetizer;

    mtime_t i_pts;
    mtime_t i_time_offset;

    int i_mux_rate;
    bool b_big_endian;
};

static int CheckSync( const uint8_t *p_peek, bool *p_big_endian );

#define PCM_FRAME_SIZE (1536 * 4)
#define A52_PACKET_SIZE (1024)
#define A52_PEEK_SIZE (4 * PCM_FRAME_SIZE)
#define A52_PROBE_SIZE (512*1024)
#define A52_MAX_HEADER_SIZE 10

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    const uint8_t *p_peek;
    int         i_peek = 0;
    bool  b_big_endian = 0; /* Arbitrary initialisation */

    /* Check if we are dealing with a WAV file */
    if( stream_Peek( p_demux->s, &p_peek, 12+8 ) == 12+8 &&
        !memcmp( p_peek, "RIFF", 4 ) && !memcmp( &p_peek[8], "WAVE", 4 ) )
    {
        /* Skip the wave header */
        i_peek = 12 + 8;
        while( memcmp( p_peek + i_peek - 8, "data", 4 ) )
        {
            uint32_t i_len = GetDWLE( p_peek + i_peek - 4 );
            if( i_len > A52_PROBE_SIZE || i_peek + i_len > A52_PROBE_SIZE )
                return VLC_EGENERIC;

            i_peek += i_len + 8;
            if( stream_Peek( p_demux->s, &p_peek, i_peek ) != i_peek )
                return VLC_EGENERIC;
        }

        /* TODO: should check wave format and sample_rate */

        /* Some A52 wav files don't begin with a sync code so we do a more
         * extensive search */
        int i_size = stream_Peek( p_demux->s, &p_peek, i_peek + A52_PEEK_SIZE * 2);
        i_size -= (PCM_FRAME_SIZE + A52_MAX_HEADER_SIZE);

        while( i_peek < i_size )
        {
            if( CheckSync( p_peek + i_peek, &b_big_endian ) != VLC_SUCCESS )
                /* The data is stored in 16 bits words */
                i_peek += 2;
            else
            {
                /* Check following sync code */
                if( CheckSync( p_peek + i_peek + PCM_FRAME_SIZE,
                               &b_big_endian ) != VLC_SUCCESS )
                {
                    i_peek += 2;
                    continue;
                }

                break;
            }
        }
    }

    /* Have a peep at the show. */
    CHECK_PEEK( p_peek, i_peek + A52_MAX_HEADER_SIZE * 2 );

    if( CheckSync( p_peek + i_peek, &b_big_endian ) != VLC_SUCCESS )
    {
        if( !p_demux->b_force )
            return VLC_EGENERIC;

        /* User forced */
        msg_Err( p_demux, "this doesn't look like a A52 audio stream, "
                 "continuing anyway" );
    }

    /* Fill p_demux fields */
    DEMUX_INIT_COMMON(); p_sys = p_demux->p_sys;
    p_sys->b_start = true;
    p_sys->i_mux_rate = 0;
    p_sys->b_big_endian = b_big_endian;
    p_sys->i_pts = 0;
    p_sys->i_time_offset = 0;

    /* Load the A52 packetizer */
    INIT_APACKETIZER( p_sys->p_packetizer, 'a', '5', '2', ' ' );
    LOAD_PACKETIZER_OR_FAIL( p_sys->p_packetizer, "A52" );

    /* Create one program */
    p_sys->p_es = es_out_Add( p_demux->out, &p_sys->p_packetizer->fmt_in );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t        *p_demux = (demux_t*)p_this;
    demux_sys_t    *p_sys = p_demux->p_sys;

    DESTROY_PACKETIZER( p_sys->p_packetizer );
    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t     *p_block_in, *p_block_out;

     /* Align stream */
    int64_t i_pos = stream_Tell( p_demux->s );
    if( i_pos % 2 ) stream_Read( p_demux->s, NULL, 1 );

    if( !( p_block_in = stream_Block( p_demux->s, A52_PACKET_SIZE ) ) )
    {
        return 0;
    }

    if( !p_sys->b_big_endian && p_block_in->i_buffer )
    {
        /* Convert to big endian */

#ifdef HAVE_SWAB
        swab(p_block_in->p_buffer, p_block_in->p_buffer, p_block_in->i_buffer);

#else
        int i;
        uint8_t *p_tmp, tmp;
        p_tmp = p_block_in->p_buffer;
        for( i = p_block_in->i_buffer / 2 ; i-- ; )
        {
            tmp = p_tmp[0];
            p_tmp[0] = p_tmp[1];
            p_tmp[1] = tmp;
            p_tmp += 2;
        }
#endif
    }

    if( p_sys->b_start )
        p_block_in->i_pts = p_block_in->i_dts = 1;
    else
        p_block_in->i_pts = p_block_in->i_dts = 0;

    while( (p_block_out = p_sys->p_packetizer->pf_packetize(
                p_sys->p_packetizer, &p_block_in )) )
    {
        p_sys->b_start = false;

        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            /* We assume a constant bitrate */
            if( p_block_out->i_length )
            {
                p_sys->i_mux_rate =
                    p_block_out->i_buffer * INT64_C(1000000)/p_block_out->i_length;
            }
            p_sys->i_pts = p_block_out->i_pts;

            /* Correct timestamp */
            p_block_out->i_pts += p_sys->i_time_offset;
            p_block_out->i_dts += p_sys->i_time_offset;

            /* set PCR */
            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block_out->i_dts );

            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

            p_block_out = p_next;
        }
    }

    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    bool *pb_bool;
    int64_t *pi64;
    int i_ret;

    switch( i_query )
    {
    case DEMUX_HAS_UNSUPPORTED_META:
        pb_bool = (bool*)va_arg( args, bool* );
        *pb_bool = true;
        return VLC_SUCCESS;

    case DEMUX_GET_TIME:
        pi64 = (int64_t*)va_arg( args, int64_t * );
        *pi64 = p_sys->i_pts + p_sys->i_time_offset;
        return VLC_SUCCESS;

    case DEMUX_SET_TIME: /* TODO implement a high precicsion seek */
    default:
        i_ret = demux_vaControlHelper( p_demux->s,
                                       0, -1,
                                       8*p_sys->i_mux_rate, 1, i_query, args );
        if( !i_ret && p_sys->i_mux_rate > 0 &&
            ( i_query == DEMUX_SET_POSITION || i_query == DEMUX_SET_TIME ) )
        {

            const int64_t i_time = INT64_C(1000000) * stream_Tell(p_demux->s) /
                                        p_sys->i_mux_rate;

            /* Fix time_offset */
            if( i_time >= 0 )
                p_sys->i_time_offset = i_time - p_sys->i_pts;
        }
        return i_ret;
    }
}

/*****************************************************************************
 * CheckSync: Check if buffer starts with an A52 sync code
 *****************************************************************************/
static int CheckSync( const uint8_t *p_peek, bool *p_big_endian )
{
    /* Little endian version of the bitstream */
    if( p_peek[0] == 0x77 && p_peek[1] == 0x0b &&
        p_peek[4] < 0x60 /* bsid < 12 */ )
    {
        *p_big_endian = false;
        return VLC_SUCCESS;
    }
    /* Big endian version of the bitstream */
    else if( p_peek[0] == 0x0b && p_peek[1] == 0x77 &&
             p_peek[5] < 0x60 /* bsid < 12 */ )
    {
        *p_big_endian = true;
        return VLC_SUCCESS;
    }

    return VLC_EGENERIC;
}


