/*****************************************************************************
 * input_es.c: Elementary Stream demux and packet management
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: input_es.c,v 1.11 2001/12/30 07:09:55 sam Exp $
 *
 * Author: Christophe Massiot <massiot@via.ecp.fr>
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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <videolan/vlc.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#endif

#include <fcntl.h>

#if defined( WIN32 )
#   include <io.h>                                                 /* read() */
#else
#   include <sys/uio.h>                                      /* struct iovec */
#endif

#if defined( WIN32 )
#   include "input_iovec.h"
#endif

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "input_es.h"

#include "debug.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  ESProbe     ( probedata_t * );
static int  ESRead      ( struct input_thread_s *, data_packet_t ** );
static void ESInit          ( struct input_thread_s * );
static void ESEnd           ( struct input_thread_s * );
static void ESSeek          ( struct input_thread_s *, off_t );
static int  ESSetProgram    ( struct input_thread_s *, pgrm_descriptor_t * );
static void ESDemux         ( struct input_thread_s *, 
                                struct data_packet_s * );
static void ESNextDataPacket( struct bit_stream_s * );
static void ESInitBitstream( struct bit_stream_s *, struct decoder_fifo_s *,
                        void (* pf_bitstream_callback)( struct bit_stream_s *,
                                                        boolean_t ),
                        void * );

/*****************************************************************************
 * Declare a buffer manager
 *****************************************************************************/
#define FLAGS           BUFFERS_UNIQUE_SIZE
#define NB_LIFO         1
DECLARE_BUFFERS_EMBEDDED( FLAGS, NB_LIFO );
DECLARE_BUFFERS_INIT( FLAGS, NB_LIFO );
DECLARE_BUFFERS_END( FLAGS, NB_LIFO );
DECLARE_BUFFERS_NEWPACKET( FLAGS, NB_LIFO );
DECLARE_BUFFERS_DELETEPACKET( FLAGS, NB_LIFO, 150 );
DECLARE_BUFFERS_NEWPES( FLAGS, NB_LIFO );
DECLARE_BUFFERS_DELETEPES( FLAGS, NB_LIFO, 150 );
DECLARE_BUFFERS_TOIO( FLAGS, ES_PACKET_SIZE );

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list )
{
#define input p_function_list->functions.input
    p_function_list->pf_probe = ESProbe;
    input.pf_init             = ESInit;
    input.pf_open             = NULL;
    input.pf_close            = NULL;
    input.pf_end              = ESEnd;
    input.pf_init_bit_stream  = ESInitBitstream;
    input.pf_set_area         = NULL;
    input.pf_set_program      = ESSetProgram;
    input.pf_read             = ESRead;
    input.pf_demux            = ESDemux;
    input.pf_new_packet       = input_NewPacket;
    input.pf_new_pes          = input_NewPES;
    input.pf_delete_packet    = input_DeletePacket;
    input.pf_delete_pes       = input_DeletePES;
    input.pf_rewind           = NULL;
    input.pf_seek             = ESSeek;
#undef input
}

/*
 * Data reading functions
 */

/*****************************************************************************
 * ESProbe: verifies that the stream is a ES stream
 *****************************************************************************/
static int ESProbe( probedata_t *p_data )
{
    int i_score = 5;

    return( i_score );
}

/*****************************************************************************
 * ESInit: initializes ES structures
 *****************************************************************************/
static void ESInit( input_thread_t * p_input )
{
    es_descriptor_t *   p_es;

    p_input->p_method_data = NULL;

    if( (p_input->p_method_data = input_BuffersInit()) == NULL )
    {
        p_input->b_error = 1;
        return;
    }

    /* FIXME : detect if InitStream failed */
    input_InitStream( p_input, 0 );
    input_AddProgram( p_input, 0, 0 );
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_es = input_AddES( p_input, p_input->stream.p_selected_program, 0xE0, 0 );
    p_es->i_stream_id = 0xE0;
    p_es->i_type = MPEG1_VIDEO_ES;
    p_es->i_cat = VIDEO_ES;
    input_SelectES( p_input, p_es );
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * ESEnd: frees unused data
 *****************************************************************************/
static void ESEnd( input_thread_t * p_input )
{
    input_BuffersEnd( p_input->p_method_data );
}

/*****************************************************************************
 * ESRead: reads data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * packets.
 *****************************************************************************/
static int ESRead( input_thread_t * p_input,
                   data_packet_t ** pp_data )
{
    int             i_read;
    struct iovec    p_iovec[ES_READ_ONCE];
    data_packet_t * p_data;

    /* Get iovecs */
    *pp_data = p_data = input_BuffersToIO( p_input->p_method_data, p_iovec,
                                           ES_READ_ONCE );

    if ( p_data == NULL )
    {
        return( -1 );
    }

    i_read = readv( p_input->i_handle, p_iovec, ES_READ_ONCE );
    if( i_read == -1 )
    {
        intf_ErrMsg( "input error: ES readv error" );
        p_input->pf_delete_packet( p_input->p_method_data, p_data );
        return( -1 );
    }
    p_input->stream.p_selected_area->i_tell += i_read;
    i_read /= ES_PACKET_SIZE;

    if( i_read != ES_READ_ONCE )
    {
        /* We got fewer packets than wanted. Give remaining packets
         * back to the buffer allocator. */
        int i_loop;

        for( i_loop = 0; i_loop < i_read; i_loop++ )
        {
            pp_data = &(*pp_data)->p_next;
        }

        p_input->pf_delete_packet( p_input->p_method_data, *pp_data );
        *pp_data = NULL;
    }

    return( i_read );
}

/*****************************************************************************
 * ESSeek: changes the stream position indicator
 *****************************************************************************/
static void ESSeek( input_thread_t * p_input, off_t i_position )
{
    lseek( p_input->i_handle, i_position, SEEK_SET );

    p_input->stream.p_selected_area->i_tell = i_position;
}


/*****************************************************************************
 * ESSetProgram: Does nothing
 *****************************************************************************/
static int ESSetProgram( input_thread_t * p_input, pgrm_descriptor_t * p_pgrm )
{
    return( 0 );
}

/*****************************************************************************
 * ESDemux: fakes a demultiplexer
 *****************************************************************************/
static void ESDemux( input_thread_t * p_input, data_packet_t * p_data )
{
    pes_packet_t *  p_pes = p_input->pf_new_pes( p_input->p_method_data );
    decoder_fifo_t * p_fifo =
        p_input->stream.p_selected_program->pp_es[0]->p_decoder_fifo;

    if( p_pes == NULL )
    {
        intf_ErrMsg("Out of memory");
        p_input->b_error = 1;
        return;
    }

    p_pes->i_rate = p_input->stream.control.i_rate;
    p_pes->p_first = p_pes->p_last = p_data;
    p_pes->i_nb_data = 1;

    if( (p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT)
         | (input_ClockManageControl( p_input, 
                          p_input->stream.p_selected_program,
                         (mtime_t)0 ) == PAUSE_S) )
    {
        intf_WarnMsg( 2, "synchro reinit" );
        p_pes->i_pts = mdate() + DEFAULT_PTS_DELAY;
        p_input->stream.p_selected_program->i_synchro_state = SYNCHRO_OK;
    }

    input_DecodePES( p_fifo, p_pes );

    vlc_mutex_lock( &p_fifo->data_lock );
    if( p_fifo->i_depth >= MAX_PACKETS_IN_FIFO )
    {
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }
    vlc_mutex_unlock( &p_fifo->data_lock );
}

/*****************************************************************************
 * ESNextDataPacket: signals the input thread if there isn't enough packets
 * available
 *****************************************************************************/
static void ESNextDataPacket( bit_stream_t * p_bit_stream )
{
    decoder_fifo_t *    p_fifo = p_bit_stream->p_decoder_fifo;
    boolean_t           b_new_pes;

    /* We are looking for the next data packet that contains real data,
     * and not just a PES header */
    do
    {
        /* We were reading the last data packet of this PES packet... It's
         * time to jump to the next PES packet */
        if( p_bit_stream->p_data->p_next == NULL )
        {
            pes_packet_t * p_next;

            vlc_mutex_lock( &p_fifo->data_lock );

            /* Free the previous PES packet. */
            p_next = p_fifo->p_first->p_next;
            p_fifo->p_first->p_next = NULL;
            p_fifo->pf_delete_pes( p_fifo->p_packets_mgt,
                                   p_fifo->p_first );
            p_fifo->p_first = p_next;
            p_fifo->i_depth--;

            if( p_fifo->p_first == NULL )
            {
                /* No PES in the FIFO. p_last is no longer valid. */
                p_fifo->pp_last = &p_fifo->p_first;

                /* Signal the input thread we're waiting. */
                vlc_cond_signal( &p_fifo->data_wait );

                /* Wait for the input to tell us when we receive a packet. */
                vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
            }

            /* The next byte could be found in the next PES packet */
            p_bit_stream->p_data = p_fifo->p_first->p_first;

            vlc_mutex_unlock( &p_fifo->data_lock );

            b_new_pes = 1;
        }
        else
        {
            /* Perhaps the next data packet of the current PES packet contains
             * real data (ie its payload's size is greater than 0). */
            p_bit_stream->p_data = p_bit_stream->p_data->p_next;

            b_new_pes = 0;
        }
    } while ( p_bit_stream->p_data->p_payload_start
               == p_bit_stream->p_data->p_payload_end );

    /* We've found a data packet which contains interesting data... */
    p_bit_stream->p_byte = p_bit_stream->p_data->p_payload_start;
    p_bit_stream->p_end  = p_bit_stream->p_data->p_payload_end;

    /* Call back the decoder. */
    if( p_bit_stream->pf_bitstream_callback != NULL )
    {
        p_bit_stream->pf_bitstream_callback( p_bit_stream, b_new_pes );
    }
}

/*****************************************************************************
 * ESInitBitstream: changes pf_next_data_packet
 *****************************************************************************/
static void ESInitBitstream( bit_stream_t * p_bit_stream,
                             decoder_fifo_t * p_decoder_fifo,
                        void (* pf_bitstream_callback)( struct bit_stream_s *,
                                                        boolean_t ),
                            void * p_callback_arg )
{
    InitBitstream( p_bit_stream, p_decoder_fifo, pf_bitstream_callback,
                   p_callback_arg );
    p_bit_stream->pf_next_data_packet = ESNextDataPacket;
}
