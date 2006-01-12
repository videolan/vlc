/*****************************************************************************
 * m4v.c : MPEG-4 Video demuxer
 *****************************************************************************
 * Copyright (C) 2002-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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
#include <stdlib.h>                                      /* malloc(), free() */

#include <vlc/vlc.h>
#include <vlc/input.h>
#include "vlc_codec.h"

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin();
    set_category( CAT_INPUT );
    set_subcategory( SUBCAT_INPUT_DEMUX );
    set_description( _("MPEG-4 video demuxer" ) );
    set_capability( "demux2", 0 );
    set_callbacks( Open, Close );
    add_shortcut( "m4v" );
    add_shortcut( "mp4v" );
vlc_module_end();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
struct demux_sys_t
{
    mtime_t     i_dts;
    es_out_id_t *p_es;

    decoder_t *p_packetizer;
};

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

#define M4V_PACKET_SIZE 4096

/*****************************************************************************
 * Open: initializes demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    vlc_bool_t   b_forced = VLC_FALSE;

    uint8_t     *p_peek;

    es_format_t  fmt;

    if( stream_Peek( p_demux->s, &p_peek, 4 ) < 4 )
    {
        msg_Err( p_demux, "cannot peek" );
        return VLC_EGENERIC;
    }

    if( !strncmp( p_demux->psz_demux, "mp4v", 4 ) ||
        !strncmp( p_demux->psz_demux, "m4v", 4 ) )
    {
        b_forced = VLC_TRUE;
    }

    if( p_peek[0] != 0x00 || p_peek[1] != 0x00 || p_peek[2] != 0x01 || p_peek[3] > 0x2f )
    {
        if( !b_forced )
        {
            msg_Warn( p_demux, "m4v module discarded (no startcode)" );
            return VLC_EGENERIC;
        }

        msg_Warn( p_demux, "this doesn't look like an MPEG-4 ES stream, continuing anyway" );
    }

    p_demux->pf_demux  = Demux;
    p_demux->pf_control= Control;
    p_demux->p_sys     = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->p_es        = NULL;
    p_sys->i_dts       = 1;

    /*
     * Load the mpegvideo packetizer
     */
    p_sys->p_packetizer = vlc_object_create( p_demux, VLC_OBJECT_PACKETIZER );
    p_sys->p_packetizer->pf_decode_audio = NULL;
    p_sys->p_packetizer->pf_decode_video = NULL;
    p_sys->p_packetizer->pf_decode_sub = NULL;
    p_sys->p_packetizer->pf_packetize = NULL;
    es_format_Init( &p_sys->p_packetizer->fmt_in, VIDEO_ES,
                    VLC_FOURCC( 'm', 'p', '4', 'v' ) );
    es_format_Init( &p_sys->p_packetizer->fmt_out, UNKNOWN_ES, 0 );
    p_sys->p_packetizer->p_module =
        module_Need( p_sys->p_packetizer, "packetizer", NULL, 0 );

    if( p_sys->p_packetizer->p_module == NULL)
    {
        vlc_object_destroy( p_sys->p_packetizer );
        msg_Err( p_demux, "cannot find mp4v packetizer" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /*
     * create the output
     */
    es_format_Init( &fmt, VIDEO_ES, VLC_FOURCC( 'm', 'p', '4', 'v' ) );
    p_sys->p_es = es_out_Add( p_demux->out, &fmt );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    module_Unneed( p_sys->p_packetizer, p_sys->p_packetizer->p_module );
    vlc_object_destroy( p_sys->p_packetizer );

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

    if( ( p_block_in = stream_Block( p_demux->s, M4V_PACKET_SIZE ) ) == NULL )
    {
        return 0;
    }

    /* m4v demuxer doesn't set pts/dts at all */
    p_block_in->i_dts = p_sys->i_dts;
    p_block_in->i_pts = 0;

    while( (p_block_out = p_sys->p_packetizer->pf_packetize( p_sys->p_packetizer, &p_block_in )) )
    {
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            if( p_sys->p_es == NULL )
            {
                p_sys->p_packetizer->fmt_out.b_packetized = VLC_TRUE;
                p_sys->p_es = es_out_Add( p_demux->out, &p_sys->p_packetizer->fmt_out);
            }

            es_out_Control( p_demux->out, ES_OUT_SET_PCR, p_sys->i_dts );

            p_block_out->p_next = NULL;
            if( p_block_out->i_pts == p_block_out->i_dts )
            {
                p_block_out->i_pts = p_sys->i_dts;
            }
            else
            {
                p_block_out->i_pts = 0;
            }
            p_block_out->i_dts = p_sys->i_dts;

            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

            p_block_out = p_next;

            /* FIXME FIXME FIXME FIXME */
            p_sys->i_dts += (mtime_t)1000000 / 25;
            /* FIXME FIXME FIXME FIXME */
        }
    }
    return 1;
}

/*****************************************************************************
 * Control:
 *****************************************************************************/
static int Control( demux_t *p_demux, int i_query, va_list args )
{
    /* demux_sys_t *p_sys  = p_demux->p_sys; */
    /* FIXME calculate the bitrate */
    if( i_query == DEMUX_SET_TIME )
        return VLC_EGENERIC;
    else
        return demux2_vaControlHelper( p_demux->s,
                                       0, -1,
                                       0, 1, i_query, args );
}

