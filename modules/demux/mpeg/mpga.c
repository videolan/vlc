/*****************************************************************************
 * mpga.c : MPEG-I/II Audio input module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *          Gildas Bazin <gbazin@videolan.org>
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
#include <vlc_input.h>

#define MPGA_PACKET_SIZE 1024

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( N_("MPEG audio / MP3 demuxer" ) );
    set_capability( "demux", 100 );
    set_callbacks( Open, Close );
    add_shortcut( "mpga" );
    add_shortcut( "mp3" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux  ( demux_t * );
static int Control( demux_t *, int, va_list );

struct demux_sys_t
{
    es_out_id_t *p_es;

    bool  b_start;
    decoder_t   *p_packetizer;

    mtime_t     i_pts;
    mtime_t     i_time_offset;
    int         i_bitrate_avg;  /* extracted from Xing header */

    bool b_initial_sync_failed;

    int i_xing_frames;
    int i_xing_bytes;
    int i_xing_bitrate_avg;
    int i_xing_frame_samples;
};

static int HeaderCheck( uint32_t h )
{
    if( ((( h >> 21 )&0x07FF) != 0x07FF )   /* header sync */
        || (((h >> 17)&0x03) == 0 )         /* valid layer ?*/
        || (((h >> 12)&0x0F) == 0x0F )
        || (((h >> 12)&0x0F) == 0x00 )      /* valid bitrate ? */
        || (((h >> 10) & 0x03) == 0x03 )    /* valide sampling freq ? */
        || ((h & 0x03) == 0x02 ))           /* valid emphasis ? */
    {
        return false;
    }
    return true;
}

#define MPGA_VERSION( h )   ( 1 - (((h)>>19)&0x01) )
#define MPGA_LAYER( h )     ( 3 - (((h)>>17)&0x03) )
#define MPGA_MODE(h)        (((h)>> 6)&0x03)

static int mpga_frame_samples( uint32_t h )
{
    switch( MPGA_LAYER(h) )
    {
        case 0:
            return 384;
        case 1:
            return 1152;
        case 2:
            return MPGA_VERSION(h) ? 576 : 1152;
        default:
            return 0;
    }
}

/*****************************************************************************
 * Open: initializes demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    bool   b_forced = false;

    uint32_t     header;
    const uint8_t     *p_peek;

    if( demux_IsPathExtension( p_demux, ".mp3" ) )
        b_forced = true;

    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 ) return VLC_EGENERIC;

    if( !HeaderCheck( header = GetDWBE( p_peek ) ) )
    {
        bool b_ok = false;
        int i_peek;

        if( !p_demux->b_force && !b_forced ) return VLC_EGENERIC;

        i_peek = stream_Peek( p_demux->s, &p_peek, 8096 );
        while( i_peek > 4 )
        {
            if( HeaderCheck( header = GetDWBE( p_peek ) ) )
            {
                b_ok = true;
                break;
            }
            p_peek += 1;
            i_peek -= 1;
        }
        if( !b_ok && !p_demux->b_force ) return VLC_EGENERIC;
    }

    DEMUX_INIT_COMMON(); p_sys = p_demux->p_sys;
    memset( p_sys, 0, sizeof( demux_sys_t ) );
    p_sys->p_es = 0;
    p_sys->b_start = true;

    /* Load the mpeg audio packetizer */
    INIT_APACKETIZER( p_sys->p_packetizer, 'm', 'p', 'g', 'a' );
    es_format_Init( &p_sys->p_packetizer->fmt_out, UNKNOWN_ES, 0 );
    LOAD_PACKETIZER_OR_FAIL( p_sys->p_packetizer, "mpga" );

    /* Xing header */
    if( HeaderCheck( header ) )
    {
        int i_xing, i_skip;
        const uint8_t *p_xing;

        if( ( i_xing = stream_Peek( p_demux->s, &p_xing, 1024 ) ) < 21 )
            return VLC_SUCCESS; /* No header */

        if( MPGA_VERSION( header ) == 0 )
        {
            i_skip = MPGA_MODE( header ) != 3 ? 36 : 21;
        }
        else
        {
            i_skip = MPGA_MODE( header ) != 3 ? 21 : 13;
        }

        if( i_skip + 8 < i_xing && !strncmp( (char *)&p_xing[i_skip], "Xing", 4 ) )
        {
            unsigned int i_flags = GetDWBE( &p_xing[i_skip+4] );

            p_xing += i_skip + 8;
            i_xing -= i_skip + 8;

            i_skip = 0;
            if( i_flags&0x01 && i_skip + 4 <= i_xing )   /* XING_FRAMES */
            {
                p_sys->i_xing_frames = GetDWBE( &p_xing[i_skip] );
                i_skip += 4;
            }
            if( i_flags&0x02 && i_skip + 4 <= i_xing )   /* XING_BYTES */
            {
                p_sys->i_xing_bytes = GetDWBE( &p_xing[i_skip] );
                i_skip += 4;
            }
            if( i_flags&0x04 )   /* XING_TOC */
            {
                i_skip += 100;
            }

            // FIXME: doesn't return the right bitrage average, at least
            // with some MP3's
            if( i_flags&0x08 && i_skip + 4 <= i_xing )   /* XING_VBR */
            {
                p_sys->i_xing_bitrate_avg = GetDWBE( &p_xing[i_skip] );
                msg_Dbg( p_demux, "xing vbr value present (%d)",
                         p_sys->i_xing_bitrate_avg );
            }

            if( p_sys->i_xing_frames > 0 && p_sys->i_xing_bytes > 0 )
            {
                p_sys->i_xing_frame_samples = mpga_frame_samples( header );
                msg_Dbg( p_demux, "xing frames&bytes value present "
                         "(%d bytes, %d frames, %d samples/frame)",
                         p_sys->i_xing_bytes, p_sys->i_xing_frames,
                         p_sys->i_xing_frame_samples );
            }
        }
    }

    if( p_sys->i_xing_bytes && p_sys->i_xing_frames &&
        p_sys->i_xing_frame_samples )
    {
        p_sys->i_bitrate_avg = p_sys->i_xing_bytes * INT64_C(8) *
            p_sys->p_packetizer->fmt_out.audio.i_rate /
            p_sys->i_xing_frames / p_sys->i_xing_frame_samples;
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block_in, *p_block_out;

    if( ( p_block_in = stream_Block( p_demux->s, MPGA_PACKET_SIZE ) ) == NULL )
    {
        return 0;
    }

    p_block_in->i_pts = p_block_in->i_dts = p_sys->b_start || p_sys->b_initial_sync_failed ? 1 : 0;
    p_sys->b_initial_sync_failed = p_sys->b_start; /* Only try to resync once */

    while( ( p_block_out = p_sys->p_packetizer->pf_packetize( p_sys->p_packetizer, &p_block_in ) ) )
    {
        p_sys->b_initial_sync_failed = false;
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            if( !p_sys->p_es )
            {
                p_sys->p_packetizer->fmt_out.b_packetized = true;
                p_sys->p_es = es_out_Add( p_demux->out,
                                          &p_sys->p_packetizer->fmt_out);

                if( p_sys->i_bitrate_avg <= 0 )
                    p_sys->i_bitrate_avg = p_sys->p_packetizer->fmt_out.i_bitrate;
            }

            p_sys->i_pts = p_block_out->i_pts;

            /* Correct timestamp */
            p_block_out->i_pts += p_sys->i_time_offset;
            p_block_out->i_dts += p_sys->i_time_offset;

            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block_out->i_dts );

            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

            p_block_out = p_next;
        }
    }

    if( p_sys->b_initial_sync_failed )
        msg_Dbg( p_demux, "did not sync on first block" );
    p_sys->b_start = false;
    return 1;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    DESTROY_PACKETIZER( p_sys->p_packetizer );

    free( p_sys );
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    demux_sys_t *p_sys  = p_demux->p_sys;
    int64_t *pi64;
    bool *pb_bool;
    int i_ret;
    va_list args_save;

    va_copy ( args_save, args );

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

        case DEMUX_GET_LENGTH:
            i_ret = demux_vaControlHelper( p_demux->s, 0, -1,
                                            p_sys->i_bitrate_avg, 1, i_query,
                                            args );
            /* No bitrate, we can't have it precisely, but we can compute
             * a raw approximation with time/position */
            if( i_ret && !p_sys->i_bitrate_avg )
            {
                float f_pos = (double)(uint64_t)( stream_Tell( p_demux->s ) ) /
                              (double)(uint64_t)( stream_Size( p_demux->s ) );
                /* The first few seconds are guaranteed to be very whacky,
                 * don't bother trying ... Too bad */
                if( f_pos < 0.01 ||
                    (p_sys->i_pts + p_sys->i_time_offset) < 8000000 )
                    return VLC_EGENERIC;

                pi64 = (int64_t *)va_arg( args_save, int64_t * );
                *pi64 = (p_sys->i_pts + p_sys->i_time_offset) / f_pos;
                return VLC_SUCCESS;
            }
            va_end( args_save );
            return i_ret;

        case DEMUX_SET_TIME:
            /* FIXME TODO: implement a high precision seek (with mp3 parsing)
             * needed for multi-input */
        default:
            i_ret = demux_vaControlHelper( p_demux->s, 0, -1,
                                            p_sys->i_bitrate_avg, 1, i_query,
                                            args );
            if( !i_ret && p_sys->i_bitrate_avg > 0 &&
                (i_query == DEMUX_SET_POSITION || i_query == DEMUX_SET_TIME) )
            {
                int64_t i_time = INT64_C(8000000) * stream_Tell(p_demux->s) /
                    p_sys->i_bitrate_avg;

                /* Fix time_offset */
                if( i_time >= 0 )
                    p_sys->i_time_offset = i_time - p_sys->i_pts;
            }
            return i_ret;
    }
}
