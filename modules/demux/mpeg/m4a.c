/*****************************************************************************
 * m4a.c : MPEG-4 audio demuxer
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( N_("MPEG-4 audio demuxer" ) );
    set_capability( "demux", 110 );
    set_callbacks( Open, Close );
    add_shortcut( "m4a" );
    add_shortcut( "mp4a" );
    add_shortcut( "aac" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct demux_sys_t
{
    bool  b_start;
    es_out_id_t *p_es;

    decoder_t   *p_packetizer;

    mtime_t     i_pts;
    int64_t     i_bytes;
    mtime_t     i_time_offset;
    int         i_bitrate_avg;
};

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

#define M4A_PACKET_SIZE 4096
#define M4A_PTS_START 1

/*****************************************************************************
 * Open: initializes demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    const uint8_t *p_peek;
    int         b_forced = false;

    if( demux_IsPathExtension( p_demux, ".aac" ) )
        b_forced = true;

    if( !p_demux->b_force && !b_forced )
        return VLC_EGENERIC;

    /* peek the begining (10 is for adts header) */
    if( stream_Peek( p_demux->s, &p_peek, 10 ) < 10 )
    {
        msg_Err( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }
    if( !strncmp( (char *)p_peek, "ADIF", 4 ) )
    {
        msg_Err( p_demux, "ADIF file. Not yet supported. (Please report)" );
        return VLC_EGENERIC;
    }

    p_demux->pf_demux  = Demux;
    p_demux->pf_control= Control;
    p_demux->p_sys     = p_sys = calloc( sizeof( demux_sys_t ), 1 );
    p_sys->b_start     = true;

    /* Load the mpeg 4 audio packetizer */
    INIT_APACKETIZER( p_sys->p_packetizer,  'm', 'p', '4', 'a'  );
    es_format_Init( &p_sys->p_packetizer->fmt_out, UNKNOWN_ES, 0 );
    LOAD_PACKETIZER_OR_FAIL( p_sys->p_packetizer, "mp4 audio" );

    return VLC_SUCCESS;
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
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( demux_t *p_demux)
{
    demux_sys_t *p_sys = p_demux->p_sys;
    block_t *p_block_in, *p_block_out;

    if( ( p_block_in = stream_Block( p_demux->s, M4A_PACKET_SIZE ) ) == NULL )
    {
        return 0;
    }

    p_block_in->i_pts = p_block_in->i_dts = p_sys->b_start ? M4A_PTS_START : 0;
    p_sys->b_start = false;

    while( (p_block_out = p_sys->p_packetizer->pf_packetize(
                                          p_sys->p_packetizer, &p_block_in )) )
    {
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            if( p_sys->p_es == NULL )
            {
                p_sys->p_packetizer->fmt_out.b_packetized = true;
                p_sys->p_es = es_out_Add( p_demux->out,
                                          &p_sys->p_packetizer->fmt_out);
            }

            p_sys->i_pts = p_block_out->i_pts;
            if( p_sys->i_pts > M4A_PTS_START + INT64_C(500000) )
                p_sys->i_bitrate_avg =
                    8*INT64_C(1000000)*p_sys->i_bytes/(p_sys->i_pts-M4A_PTS_START);

            p_sys->i_bytes += p_block_out->i_buffer;

            /* Correct timestamp */
            p_block_out->i_pts += p_sys->i_time_offset;
            p_block_out->i_dts += p_sys->i_time_offset;

            /* */
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
    demux_sys_t *p_sys = p_demux->p_sys;
    bool *pb_bool;
    int64_t *pi64;
    int i_ret;

    switch( i_query )
    {
    case DEMUX_HAS_UNSUPPORTED_META:
        pb_bool = (bool *)va_arg( args, bool* );
        *pb_bool = true;
        return VLC_SUCCESS;

    case DEMUX_GET_TIME:
        pi64 = (int64_t*)va_arg( args, int64_t * );
        *pi64 = p_sys->i_pts + p_sys->i_time_offset;
        return VLC_SUCCESS;

    case DEMUX_SET_TIME: /* TODO high precision seek for multi-input */
    default:
        i_ret = demux_vaControlHelper( p_demux->s, 0, -1,
                                        p_sys->i_bitrate_avg, 1, i_query, args);
        /* Fix time_offset */
        if( (i_query == DEMUX_SET_POSITION || i_query == DEMUX_SET_TIME ) &&
            i_ret == VLC_SUCCESS && p_sys->i_bitrate_avg > 0 )
        {
            int64_t i_time = INT64_C(8000000) * stream_Tell(p_demux->s) /
                p_sys->i_bitrate_avg;

            if( i_time >= 0 )
                p_sys->i_time_offset = i_time - p_sys->i_pts;
        }
        return i_ret;
    }
}

