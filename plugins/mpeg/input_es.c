/*****************************************************************************
 * input_es.c: Elementary Stream demux and packet management
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: input_es.c,v 1.7 2001/06/27 09:53:56 massiot Exp $
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

#define MODULE_NAME es
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#include <fcntl.h>

#if defined( WIN32 )
#   include <io.h>
#   include "input_iovec.h"
#else
#   include <sys/uio.h>                                      /* struct iovec */
#endif

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "intf_msg.h"

#include "main.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"

#include "input_es.h"
#include "mpeg_system.h"
#include "input_netlist.h"

#include "debug.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  ESProbe     ( probedata_t * );
static int  ESRead      ( struct input_thread_s *,
                          data_packet_t * p_packets[INPUT_READ_ONCE] );
static void ESInit      ( struct input_thread_s * );
static void ESEnd       ( struct input_thread_s * );
static void ESSeek      ( struct input_thread_s *, off_t );
static void ESDemux     ( struct input_thread_s *, struct data_packet_s * );
static void ESNextDataPacket( struct bit_stream_s * );
static void ESInitBitstream( struct bit_stream_s *, struct decoder_fifo_s *,
                        void (* pf_bitstream_callback)( struct bit_stream_s *,
                                                        boolean_t ),
                        void * );


/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list )
{
#define input p_function_list->functions.input
    p_function_list->pf_probe = ESProbe;
    input.pf_init             = ESInit;
    input.pf_open             = NULL; /* Set in ESInit */
    input.pf_close            = NULL;
    input.pf_end              = ESEnd;
    input.pf_init_bit_stream  = ESInitBitstream;
    input.pf_set_area         = NULL;
    input.pf_read             = ESRead;
    input.pf_demux            = ESDemux;
    input.pf_new_packet       = input_NetlistNewPacket;
    input.pf_new_pes          = input_NetlistNewPES;
    input.pf_delete_packet    = input_NetlistDeletePacket;
    input.pf_delete_pes       = input_NetlistDeletePES;
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
    input_thread_t * p_input = (input_thread_t *)p_data;

    char * psz_name = p_input->p_source;
    int i_handle;
    int i_score = 5;

    if( TestMethod( INPUT_METHOD_VAR, "es" ) )
    {
        return( 999 );
    }

    i_handle = open( psz_name, 0 );
    if( i_handle == -1 )
    {
        return( 0 );
    }
    close( i_handle );

    return( i_score );
}

/*****************************************************************************
 * ESInit: initializes ES structures
 *****************************************************************************/
static void ESInit( input_thread_t * p_input )
{
    es_descriptor_t *   p_es;

    p_input->p_method_data = NULL;

    /* Initialize netlist */
    if( input_NetlistInit( p_input, NB_DATA, NB_PES, ES_PACKET_SIZE,
                           INPUT_READ_ONCE ) )
    {
        intf_ErrMsg( "ES input : Could not initialize netlist" );
        return;
    }

    p_input->pf_open  = p_input->pf_file_open;
    p_input->pf_close = p_input->pf_file_close;

    /* FIXME : detect if InitStream failed */
    input_InitStream( p_input, 0 );
    input_AddProgram( p_input, 0, 0 );
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_es = input_AddES( p_input, p_input->stream.pp_programs[0], 0xE0, 0 );
    p_es->i_stream_id = 0xE0;
    p_es->i_type = MPEG1_VIDEO_ES;
    p_es->i_cat = VIDEO_ES;
    input_SelectES( p_input, p_es );
    p_input->stream.i_method = INPUT_METHOD_FILE;
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.pp_programs[0]->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * ESEnd: frees unused data
 *****************************************************************************/
static void ESEnd( input_thread_t * p_input )
{
}

/*****************************************************************************
 * ESRead: reads data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 if everything went well, and 1 in case of
 * EOF.
 *****************************************************************************/
static int ESRead( input_thread_t * p_input,
                   data_packet_t * pp_packets[INPUT_READ_ONCE] )
{
    int             i_read;
    struct iovec  * p_iovec;

    /* Get iovecs */
    p_iovec = input_NetlistGetiovec( p_input->p_method_data );

    if ( p_iovec == NULL )
    {
        return( -1 ); /* empty netlist */
    }

    memset( pp_packets, 0, INPUT_READ_ONCE * sizeof(data_packet_t *) );

    i_read = readv( p_input->i_handle, p_iovec, INPUT_READ_ONCE );
    if( i_read == -1 )
    {
        intf_ErrMsg( "input error: ES readv error" );
        return( -1 );
    }

    input_NetlistMviovec( p_input->p_method_data,
             (int)(i_read/ES_PACKET_SIZE), pp_packets );

    p_input->stream.p_selected_area->i_tell += i_read;

    return( 0 );
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
 * ESDemux: fakes a demultiplexer
 *****************************************************************************/
static void ESDemux( input_thread_t * p_input, data_packet_t * p_data )
{
    pes_packet_t *  p_pes = p_input->pf_new_pes( p_input->p_method_data );
    decoder_fifo_t * p_fifo =
        p_input->stream.pp_programs[0]->pp_es[0]->p_decoder_fifo;

    if( p_pes == NULL )
    {
        intf_ErrMsg("Out of memory");
        p_input->b_error = 1;
        return;
    }

    p_pes->i_rate = p_input->stream.control.i_rate;
    p_pes->p_first = p_data;

    if( (p_input->stream.pp_programs[0]->i_synchro_state == SYNCHRO_REINIT)
         | (input_ClockManageControl( p_input, p_input->stream.pp_programs[0],
                                  (mtime_t)0 ) == PAUSE_S) )
    {
        intf_WarnMsg( 2, "synchro reinit" );
        p_pes->i_pts = mdate() + DEFAULT_PTS_DELAY;
        p_input->stream.pp_programs[0]->i_synchro_state = SYNCHRO_OK;
    }

    input_DecodePES( p_fifo, p_pes );

    vlc_mutex_lock( &p_fifo->data_lock );
    if( ( (DECODER_FIFO_END( *p_fifo ) - DECODER_FIFO_START( *p_fifo ))
            & FIFO_SIZE ) >= MAX_PACKETS_IN_FIFO )
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
            /* We are going to read/write the start and end indexes of the
             * decoder fifo and to use the fifo's conditional variable,
             * that's why we need to take the lock before. */
            vlc_mutex_lock( &p_fifo->data_lock );

            /* Free the previous PES packet. */
            p_fifo->pf_delete_pes( p_fifo->p_packets_mgt,
                                   DECODER_FIFO_START( *p_fifo ) );
            DECODER_FIFO_INCSTART( *p_fifo );

            if( DECODER_FIFO_ISEMPTY( *p_fifo ) )
            {
                /* Signal the input thread we're waiting. */
                vlc_cond_signal( &p_fifo->data_wait );

                /* Wait for the input to tell us when we receive a packet. */
                vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
            }

            /* The next byte could be found in the next PES packet */
            p_bit_stream->p_data = DECODER_FIFO_START( *p_fifo )->p_first;

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
