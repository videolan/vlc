/*****************************************************************************
 * flac.c : FLAC demux module for vlc
 *****************************************************************************
 * Copyright (C) 2001-2003 VideoLAN
 * $Id: flac.c,v 1.11 2004/02/25 17:48:52 fenrir Exp $
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc_codec.h>

#define STREAMINFO_SIZE 38
#define FLAC_PACKET_SIZE 16384

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );
static int  Demux ( input_thread_t * );

struct demux_sys_t
{
    vlc_bool_t  b_start;
    es_out_id_t *p_es;

    /* Packetizer */
    decoder_t *p_packetizer;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();                                      
    set_description( _("FLAC demuxer") );                       
    set_capability( "demux", 155 );
    set_callbacks( Open, Close );
    add_shortcut( "flac" );
vlc_module_end();

/*****************************************************************************
 * Open: initializes ES structures
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    demux_sys_t    *p_sys;
    int            i_peek;
    byte_t *       p_peek;
    es_format_t    fmt;

    p_input->pf_demux = Demux;
    p_input->pf_demux_control = demux_vaControlDefault;
    p_input->pf_rewind = NULL;

    /* Have a peep at the show. */
    if( input_Peek( p_input, &p_peek, 4 ) < 4 )
    {
        /* Stream shorter than 4 bytes... */
        msg_Err( p_input, "cannot peek()" );
        return VLC_EGENERIC;
    }

    if( p_peek[0]!='f' || p_peek[1]!='L' || p_peek[2]!='a' || p_peek[3]!='C' )
    {
        if( p_input->psz_demux && !strncmp( p_input->psz_demux, "flac", 4 ) )
        {
            /* User forced */
            msg_Err( p_input, "this doesn't look like a flac stream, "
                     "continuing anyway" );
        }
        else
        {
            msg_Warn( p_input, "flac module discarded (no startcode)" );
            return VLC_EGENERIC;
        }
    }

    p_input->p_demux_data = p_sys = malloc( sizeof( demux_sys_t ) );
    es_format_Init( &fmt, AUDIO_ES, VLC_FOURCC( 'f', 'l', 'a', 'c' ) );
    p_sys->b_start = VLC_TRUE;

    /* We need to read and store the STREAMINFO metadata */
    i_peek = stream_Peek( p_input->s, &p_peek, 8 );
    if( p_peek[4] & 0x7F )
    {
        msg_Err( p_input, "this isn't a STREAMINFO metadata block" );
        return VLC_EGENERIC;
    }

    if( ((p_peek[5]<<16)+(p_peek[6]<<8)+p_peek[7]) != (STREAMINFO_SIZE - 4) )
    {
        msg_Err( p_input, "invalid size for a STREAMINFO metadata block" );
        return VLC_EGENERIC;
    }

    /*
     * Load the FLAC packetizer
     */
    p_sys->p_packetizer = vlc_object_create( p_input, VLC_OBJECT_DECODER );
    p_sys->p_packetizer->pf_decode_audio = 0;
    p_sys->p_packetizer->pf_decode_video = 0;
    p_sys->p_packetizer->pf_decode_sub = 0;
    p_sys->p_packetizer->pf_packetize = 0;

    /* Initialization of decoder structure */
    es_format_Init( &p_sys->p_packetizer->fmt_in, AUDIO_ES,
                    VLC_FOURCC( 'f', 'l', 'a', 'c' ) );

    /* Store STREAMINFO for the decoder and packetizer */
    p_sys->p_packetizer->fmt_in.i_extra = fmt.i_extra = STREAMINFO_SIZE + 4;
    p_sys->p_packetizer->fmt_in.p_extra = malloc( STREAMINFO_SIZE + 4 );
    stream_Read( p_input->s, p_sys->p_packetizer->fmt_in.p_extra,
                 STREAMINFO_SIZE + 4 );

    /* Fake this as the last metadata block */
    ((uint8_t*)p_sys->p_packetizer->fmt_in.p_extra)[4] |= 0x80;
    fmt.p_extra = malloc( STREAMINFO_SIZE + 4 );
    memcpy( fmt.p_extra, p_sys->p_packetizer->fmt_in.p_extra,
            STREAMINFO_SIZE + 4 );

    p_sys->p_packetizer->p_module =
        module_Need( p_sys->p_packetizer, "packetizer", NULL );
    if( !p_sys->p_packetizer->p_module )
    {
        if( p_sys->p_packetizer->fmt_in.p_extra )
            free( p_sys->p_packetizer->fmt_in.p_extra );

        vlc_object_destroy( p_sys->p_packetizer );
        msg_Err( p_input, "cannot find flac packetizer" );
        return VLC_EGENERIC;
    }

    /* Create one program */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1 )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "cannot init stream" );
        return VLC_EGENERIC;
    }
    p_input->stream.i_mux_rate = 0;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

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

    /* Unneed module */
    module_Unneed( p_sys->p_packetizer, p_sys->p_packetizer->p_module );

    if( p_sys->p_packetizer->fmt_in.p_extra )
        free( p_sys->p_packetizer->fmt_in.p_extra );

    /* Delete the decoder */
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

    if( !( p_block_in = stream_Block( p_input->s, FLAC_PACKET_SIZE ) ) )
    {
        return 0;
    }

    if( p_sys->b_start )
    {
        p_block_in->i_pts = p_block_in->i_dts = 1;
        p_sys->b_start = VLC_FALSE;
    }
    else
    {
        p_block_in->i_pts = p_block_in->i_dts = 0;
    }

    while( (p_block_out = p_sys->p_packetizer->pf_packetize(
                p_sys->p_packetizer, &p_block_in )) )
    {
        while( p_block_out )
        {
            block_t *p_next = p_block_out->p_next;

            input_ClockManageRef( p_input,
                                  p_input->stream.p_selected_program,
                                  p_block_out->i_pts * 9 / 100 );

            p_block_out->i_dts = p_block_out->i_pts =
                input_ClockGetTS( p_input, p_input->stream.p_selected_program,
                                  p_block_out->i_pts * 9 / 100 );

            es_out_Send( p_input->p_es_out, p_sys->p_es, p_block_out );

            p_block_out = p_next;
        }
    }

    return 1;
}
