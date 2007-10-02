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

#include <vlc/vlc.h>
#include <vlc_demux.h>
#include <vlc_input.h>
#include "vlc_codec.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( _("MPEG-4 audio demuxer" ) );
    set_capability( "demux2", 110 );
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
    vlc_bool_t  b_start;
    es_out_id_t *p_es;
    vlc_meta_t  *meta;

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
    module_t    *p_id3;
    const uint8_t *p_peek;
    int         b_forced = VLC_FALSE;

    if( demux2_IsPathExtension( p_demux, ".aac" ) )
        b_forced = VLC_TRUE;

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
    p_sys->b_start     = VLC_TRUE;

    /* Load the mpeg 4 audio packetizer */
    INIT_APACKETIZER( p_sys->p_packetizer,  'm', 'p', '4', 'a'  );
    es_format_Init( &p_sys->p_packetizer->fmt_out, UNKNOWN_ES, 0 );
    LOAD_PACKETIZER_OR_FAIL( p_sys->p_packetizer, "mp4 audio" );

    /* Parse possible id3 header */
    p_demux->p_private = malloc( sizeof( demux_meta_t ) );
    if( !p_demux->p_private )
        return VLC_ENOMEM;
    if( ( p_id3 = module_Need( p_demux, "meta reader", NULL, 0 ) ) )
    {
        demux_meta_t *p_demux_meta = (demux_meta_t *)p_demux->p_private;
        p_sys->meta = p_demux_meta->p_meta;
        p_demux->p_private = NULL;
        module_Unneed( p_demux, p_id3 );
        TAB_CLEAN( p_demux_meta->i_attachments, p_demux_meta->attachments );
    }
    free( p_demux->p_private );
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
    p_sys->b_start = VLC_FALSE;

    while( (p_block_out = p_sys->p_packetizer->pf_packetize(
                                          p_sys->p_packetizer, &p_block_in )) )
    {
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            if( p_sys->p_es == NULL )
            {
                p_sys->p_packetizer->fmt_out.b_packetized = VLC_TRUE;
                vlc_audio_replay_gain_MergeFromMeta( &p_sys->p_packetizer->fmt_out.audio_replay_gain,
                                                     p_sys->meta );
                p_sys->p_es = es_out_Add( p_demux->out,
                                          &p_sys->p_packetizer->fmt_out);
            }

            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_block_out->i_dts );

            p_block_out->p_next = NULL;

            p_sys->i_pts = p_block_out->i_pts;
            if( p_sys->i_pts > M4A_PTS_START + I64C(500000) )
                p_sys->i_bitrate_avg =
                    8*I64C(1000000)*p_sys->i_bytes/(p_sys->i_pts-M4A_PTS_START);

            p_sys->i_bytes += p_block_out->i_buffer;
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
    vlc_meta_t *p_meta;

    int64_t *pi64;
    int i_ret;

    switch( i_query )
    {
    case DEMUX_GET_META:
        p_meta = (vlc_meta_t *)va_arg( args, vlc_meta_t* );
        vlc_meta_Merge( p_meta, p_sys->meta );
        return VLC_SUCCESS;

    case DEMUX_GET_TIME:
        pi64 = (int64_t*)va_arg( args, int64_t * );
        *pi64 = p_sys->i_pts + p_sys->i_time_offset;
        return VLC_SUCCESS;

    case DEMUX_SET_TIME: /* TODO high precision seek for multi-input */
    default:
        i_ret = demux2_vaControlHelper( p_demux->s, 0, -1,
                                        p_sys->i_bitrate_avg, 1, i_query, args);
        /* Fix time_offset */
        if( (i_query == DEMUX_SET_POSITION || i_query == DEMUX_SET_TIME ) &&
            i_ret == VLC_SUCCESS && p_sys->i_bitrate_avg > 0 )
        {
            int64_t i_time = I64C(8000000) * stream_Tell(p_demux->s) /
                p_sys->i_bitrate_avg;

            if( i_time >= 0 )
                p_sys->i_time_offset = i_time - p_sys->i_pts;
        }
        return i_ret;
    }
}

