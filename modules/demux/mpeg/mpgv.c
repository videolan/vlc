/*****************************************************************************
 * mpgv.c : MPEG-I/II Video demuxer
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: mpgv.c,v 1.6 2004/03/03 20:39:52 gbazin Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
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
static int  Open    ( vlc_object_t * );
static void Close  ( vlc_object_t * );

vlc_module_begin();
    set_description( _("MPEG-I/II video demuxer" ) );
    set_capability( "demux", 100 );
    set_callbacks( Open, Close );
    add_shortcut( "mpgv" );
vlc_module_end();

/* TODO:
 * - free bitrate
 */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Demux       ( input_thread_t * );

struct demux_sys_t
{
    vlc_bool_t  b_start;

    es_out_id_t *p_es;

    decoder_t *p_packetizer;
};

#define MPGV_PACKET_SIZE 4096

/*****************************************************************************
 * Open: initializes demux structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;
    vlc_bool_t     b_forced = VLC_FALSE;

    uint8_t        *p_peek;

    es_format_t    fmt;
    char psz_description[50];

    if( stream_Peek( p_input->s, &p_peek, 4 ) < 4 )
    {
        msg_Err( p_input, "cannot peek" );
        return VLC_EGENERIC;
    }

    if( p_input->psz_demux && !strncmp( p_input->psz_demux, "mpgv", 4 ) )
    {
        b_forced = VLC_TRUE;
    }

    if( p_peek[0] != 0x00 || p_peek[1] != 0x00 || p_peek[2] != 0x01 )
    {
        if( !b_forced )
        {
            msg_Warn( p_input, "ES module discarded (no startcode)" );
            return VLC_EGENERIC;
        }

        msg_Err( p_input, "this doesn't look like an MPEG ES stream, continuing" );
    }

    if( p_peek[3] > 0xb9 )
    {
        if( !b_forced )
        {
            msg_Warn( p_input, "ES module discarded (system startcode)" );
            return VLC_EGENERIC;
        }
        msg_Err( p_input, "this seems to be a system stream (PS plug-in ?), but continuing" );
    }

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        return VLC_EGENERIC;
    }
    p_input->stream.i_mux_rate = 0 / 50;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    p_input->pf_demux  = Demux;
    p_input->pf_rewind = NULL;
    p_input->pf_demux_control = demux_vaControlDefault;

    p_input->p_demux_data = p_sys = malloc( sizeof( demux_sys_t ) );
    p_sys->b_start = VLC_TRUE;
    p_sys->p_es    = NULL;

    /*
     * Load the mpegvideo packetizer
     */
    p_sys->p_packetizer = vlc_object_create( p_input, VLC_OBJECT_PACKETIZER );
    p_sys->p_packetizer->pf_decode_audio = NULL;
    p_sys->p_packetizer->pf_decode_video = NULL;
    p_sys->p_packetizer->pf_decode_sub = NULL;
    p_sys->p_packetizer->pf_packetize = NULL;
    es_format_Init( &p_sys->p_packetizer->fmt_in, VIDEO_ES,
                    VLC_FOURCC( 'm', 'p', 'g', 'v' ) );
    es_format_Init( &p_sys->p_packetizer->fmt_out, UNKNOWN_ES, 0 );
    p_sys->p_packetizer->p_module =
        module_Need( p_sys->p_packetizer, "packetizer", NULL, 0 );

    if( p_sys->p_packetizer->p_module == NULL)
    {
        vlc_object_destroy( p_sys->p_packetizer );
        msg_Err( p_input, "cannot find mpgv packetizer" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    /*
     * create the output
     */
    es_format_Init( &fmt, VIDEO_ES, VLC_FOURCC( 'm', 'p', 'g', 'v' ) );
    sprintf( psz_description, "MPEG-I/II Video" );
    fmt.psz_description = strdup( psz_description );
    p_sys->p_es = es_out_Add( p_input->p_es_out, &fmt );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: frees unused data
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t*)p_this;
    demux_sys_t    *p_sys = p_input->p_demux_data;

    module_Unneed( p_sys->p_packetizer, p_sys->p_packetizer->p_module );
    vlc_object_destroy( p_sys->p_packetizer );

    free( p_sys );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    demux_sys_t  *p_sys = p_input->p_demux_data;
    block_t *p_block_in, *p_block_out;

    if( ( p_block_in = stream_Block( p_input->s, MPGV_PACKET_SIZE ) ) == NULL )
    {
        return 0;
    }

    if( p_sys->b_start )
    {
        p_block_in->i_pts =
        p_block_in->i_dts = 1;
    }
    else
    {
        p_block_in->i_pts =
        p_block_in->i_dts = 0;
    }

    while( (p_block_out = p_sys->p_packetizer->pf_packetize( p_sys->p_packetizer, &p_block_in )) )
    {
        p_sys->b_start = VLC_FALSE;

        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            input_ClockManageRef( p_input,
                                  p_input->stream.p_selected_program,
                                  p_block_out->i_dts * 9 / 100 );

            p_block_out->i_dts =
                input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                                  p_block_out->i_dts * 9 / 100 );
            if( p_block_out->i_pts > 0 )
            {
                p_block_out->i_pts =
                    input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                                      p_block_out->i_pts * 9 / 100 );
            }
            else
            {
                p_block_out->i_pts = 0;
            }

            p_block_out->p_next = NULL;
            es_out_Send( p_input->p_es_out, p_sys->p_es, p_block_out );

            p_block_out = p_next;
        }
    }
    return 1;
}

