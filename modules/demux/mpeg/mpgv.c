/*****************************************************************************
 * mpgv.c : MPEG-I/II Video demuxer
 *****************************************************************************
 * Copyright (C) 2001-2004 VLC authors and VideoLAN
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Open ( vlc_object_t * );
static void Close( vlc_object_t * );

vlc_module_begin ()
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_DEMUX )
    set_description( N_("MPEG-I/II video demuxer" ) )
    set_capability( "demux", 5 )
    set_callbacks( Open, Close )
    add_shortcut( "mpgv" )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
typedef struct
{
    bool  b_start;

    es_out_id_t *p_es;

    decoder_t *p_packetizer;
} demux_sys_t;

static int Demux( demux_t * );
static int Control( demux_t *, int, va_list );

#define MPGV_PACKET_SIZE 4096

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    vlc_packetizer_Destroy( p_sys->p_packetizer );
    free( p_sys );
}

static int CheckMPEGStartCode( const uint8_t *p_peek )
{
    switch( p_peek[3] )
    {
        case 0x00:
            if( (p_peek[5] & 0x38) == 0x00 )
                return VLC_EGENERIC;
            break;
        case 0xB0:
        case 0xB1:
        case 0xB6:
            return VLC_EGENERIC;
        default:
            if( p_peek[3] > 0xB9 )
                return VLC_EGENERIC;
            break;
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Open: initializes demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    demux_t     *p_demux = (demux_t*)p_this;
    demux_sys_t *p_sys;
    bool   b_forced = false;

    const uint8_t *p_peek;

    es_format_t  fmt;

    if( vlc_stream_Peek( p_demux->s, &p_peek, 8 ) < 8 )
        return VLC_EGENERIC;

    if( p_demux->obj.force )
        b_forced = true;

    if( p_peek[0] != 0x00 || p_peek[1] != 0x00 || p_peek[2] != 0x01 )
    {
        if( !b_forced ) return VLC_EGENERIC;

        msg_Err( p_demux, "this doesn't look like an MPEG ES stream, continuing" );
    }

    if( CheckMPEGStartCode( p_peek ) != VLC_SUCCESS )
    {
        if( !b_forced ) return VLC_EGENERIC;
        msg_Err( p_demux, "this seems to be a system stream (PS plug-in ?), but continuing" );
    }
    p_demux->pf_demux  = Demux;
    p_demux->pf_control= Control;
    p_demux->p_sys     = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->b_start     = true;
    p_sys->p_es        = NULL;

    /* Load the mpegvideo packetizer */
    es_format_Init( &fmt, VIDEO_ES, VLC_CODEC_MPGV );
    p_sys->p_packetizer = vlc_packetizer_New( p_demux, &fmt, "mpeg video" );
    if( !p_sys->p_packetizer )
    {
        free( p_sys );
        return VLC_EGENERIC;
    }

    /* create the output */
    p_sys->p_es = es_out_Add( p_demux->out, &fmt );
    if( p_sys->p_es == NULL )
    {
        Close( p_this );
        return VLC_EGENERIC;
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
    demux_sys_t  *p_sys = p_demux->p_sys;
    block_t *p_block_in, *p_block_out;
    bool b_eof = false;

    if( ( p_block_in = vlc_stream_Block( p_demux->s, MPGV_PACKET_SIZE ) ) == NULL )
    {
        b_eof = true;
    }

    if( p_block_in )
    {
        p_block_in->i_pts =
        p_block_in->i_dts = ( p_sys->b_start ) ? VLC_TICK_0 : VLC_TICK_INVALID;
    }

    while( (p_block_out = p_sys->p_packetizer->pf_packetize( p_sys->p_packetizer,
                                                             p_block_in ? &p_block_in : NULL )) )
    {
        p_sys->b_start = false;

        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            es_out_SetPCR( p_demux->out, p_block_out->i_dts );

            p_block_out->p_next = NULL;
            es_out_Send( p_demux->out, p_sys->p_es, p_block_out );

            p_block_out = p_next;
        }
    }
    return (b_eof) ? VLC_DEMUXER_EOF : VLC_DEMUXER_SUCCESS;
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
        return demux_vaControlHelper( p_demux->s,
                                       0, -1,
                                       0, 1, i_query, args );
}
