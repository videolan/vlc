/*****************************************************************************
 * m4v.c : MPEG-4 video Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: m4v.c,v 1.1 2003/01/12 06:39:45 fenrir Exp $
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
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate ( vlc_object_t * );
static int  Demux ( input_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("MPEG-4 video elementary stream demux" ) );
    set_capability( "demux", 0 );
    set_callbacks( Activate, NULL );
    add_shortcut( "m4v" );
vlc_module_end();

/*****************************************************************************
 * Definitions of structures  and functions used by this plugins 
 *****************************************************************************/

struct demux_sys_t
{
    mtime_t i_dts;

    es_descriptor_t *p_es;
};


/*****************************************************************************
 * Activate: initializes MPEGaudio structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t * p_input = (input_thread_t *)p_this;
    demux_sys_t * p_demux;
    input_info_category_t * p_category;

    uint8_t *p_peek;

    /* Set the demux function */
    p_input->pf_demux = Demux;

    /* Initialize access plug-in structures. */
    if( p_input->i_mtu == 0 )
    {
        /* Improve speed. */
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    /* Have a peep at the show. */
    if( input_Peek( p_input, &p_peek, 4 ) < 4 )
    {
        /* Stream shorter than 4 bytes... */
        msg_Err( p_input, "cannot peek()" );
        return( -1 );
    }

    if( p_peek[0] != 0x00 || p_peek[1] != 0x00 || p_peek[2] != 0x01 || p_peek[3] > 0x2f )
    {
        if( *p_input->psz_demux && !strncmp( p_input->psz_demux, "m4v", 3 ) )
        {
            /* user forced */
            msg_Warn( p_input, "this doesn't look like an MPEG-4 ES stream, continuing" );
        }
        else
        {
            msg_Warn( p_input, "m4v module discarded (invalid startcode)" );
            return( -1 );
        }
    }

    /* create p_demux and init it */
    if( !( p_demux = p_input->p_demux_data = malloc( sizeof(demux_sys_t) ) ) )
    {
        msg_Err( p_input, "out of memory" );
        return( -1 );
    }
    memset( p_demux, 0, sizeof(demux_sys_t) );
    p_demux->i_dts = 0;

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( input_InitStream( p_input, 0 ) == -1)
    {
        msg_Err( p_input, "cannot init stream" );
        free( p_input->p_demux_data );
        return( -1 );
    }
    if( input_AddProgram( p_input, 0, 0) == NULL )
    {
        msg_Err( p_input, "cannot add program" );
        free( p_input->p_demux_data );
        return( -1 );
    }
    p_input->stream.pp_programs[0]->b_is_ok = 0;
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];

    /* create our ES */
    p_demux->p_es = input_AddES( p_input,
                                 p_input->stream.p_selected_program,
                                 1, /* id */
                                 0 );
    if( !p_demux->p_es )
    {
        vlc_mutex_unlock( &p_input->stream.stream_lock );
        msg_Err( p_input, "out of memory" );
        free( p_input->p_demux_data );
        return( -1 );
    }
    p_demux->p_es->i_stream_id = 1;
    p_demux->p_es->i_fourcc = VLC_FOURCC('m','p','4','v');
    p_demux->p_es->i_cat = VIDEO_ES;

    input_SelectES( p_input, p_demux->p_es );

    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_category = input_InfoCategory( p_input, "mpeg" );
    input_AddInfo( p_category, "input type", "video MPEG-4 (raw ES)" );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( 0 );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int FindStartCode( uint8_t *p_data, int i_size, uint8_t i_startcode, uint8_t i_mask )
{
    int i_pos = 0;

    for( i_pos = 0; i_size >= 4; i_pos++,i_size--,p_data++ )
    {
        if( p_data[0] == 0 && p_data[1] == 0 && 
            p_data[2] == 1 && ( p_data[3]&i_mask) == i_startcode )
        {
            return( i_pos );
        }
    }
    return( i_pos );
}

static void PESAddDataPacket( pes_packet_t *p_pes, data_packet_t *p_data )
{
    if( !p_pes->p_first )
    {
        p_pes->p_first = p_data;
        p_pes->i_nb_data = 1;
        p_pes->i_pes_size = p_data->p_payload_end - p_data->p_payload_start;
    }
    else
    {
        p_pes->p_last->p_next  = p_data;
        p_pes->i_nb_data++;
        p_pes->i_pes_size += p_data->p_payload_end - p_data->p_payload_start;
    }
    p_pes->p_last  = p_data;
}

static int Demux( input_thread_t * p_input )
{
    demux_sys_t  *p_demux = p_input->p_demux_data;
    pes_packet_t *p_pes;
    data_packet_t *p_data;
    uint8_t *p_peek;
    int     i_peek;
    int     i_size;
    int     i_read;
    int     b_vop;

    input_ClockManageRef( p_input,
                          p_input->stream.p_selected_program,
                          p_demux->i_dts );

    if( (p_pes = input_NewPES( p_input->p_method_data ) ) == NULL )
    {
        msg_Err( p_input, "cannot allocate new PES" );
        return( -1 );
    }

    /* we will read data into this pes until we found a vop header */
    for( ; ; )
    {
        /* Have a peep at the show. */
        if( ( i_peek = input_Peek( p_input, &p_peek, 512 ) ) < 5 )
        {
            /* Stream shorter than 4 bytes... */
            msg_Err( p_input, "cannot peek()" );
            return( 0 );
        }

        /* vop startcode */
        if( ( i_size = FindStartCode( p_peek, i_peek, 0xb6, 0xff ) ) == 0 )
        {
            break;
        }

        if( ( i_read = input_SplitBuffer( p_input,
                                          &p_data,
                                          i_size ) ) < 0 )
        {
            msg_Err( p_input, "error while reading data" );
            break;
        }
        PESAddDataPacket( p_pes, p_data );
    }
    b_vop = 1;
    for( ; ; )
    {
        /* Have a peep at the show. */
        if( ( i_peek = input_Peek( p_input, &p_peek, 512 ) ) < 5 )
        {
            /* Stream shorter than 4 bytes... */
            msg_Err( p_input, "cannot peek()" );
            return( 0 );
        }

        /* vop startcode */
        if( b_vop )
            i_size = FindStartCode( p_peek + 1, i_peek - 1, 0x00, 0x00 ) + 1;
        else
            i_size = FindStartCode( p_peek, i_peek, 0x00, 0x00 );
        b_vop = 0;

        if( i_size == 0 )
        {
            break;
        }

        if( ( i_read = input_SplitBuffer( p_input,
                                          &p_data,
                                          i_size ) ) < 0 )
        {
            msg_Err( p_input, "error while reading data" );
            break;
        }
        PESAddDataPacket( p_pes, p_data );
    }

    p_pes->i_dts =
        p_pes->i_pts = input_ClockGetTS( p_input,
                                         p_input->stream.p_selected_program,
                                         p_demux->i_dts );

    if( !p_demux->p_es->p_decoder_fifo )
    {
        msg_Err( p_input, "no video decoder" );
        input_DeletePES( p_input->p_method_data, p_pes );
        return( -1 );
    }
    else
    {
        input_DecodePES( p_demux->p_es->p_decoder_fifo, p_pes );
    }
    /* FIXME FIXME FIXME FIXME */
    p_demux->i_dts += (mtime_t)90000 / 25;
    /* FIXME FIXME FIXME FIXME */

    return( 1 );
}


