/*****************************************************************************
 * input_es.c: Elementary Stream demux and packet management
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: input_es.c,v 1.12 2002/01/21 23:57:46 massiot Exp $
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

    vlc_mutex_lock( &p_input->stream.stream_lock );
    if( p_fifo->i_depth >= MAX_PACKETS_IN_FIFO )
    {
        /* Wait for the decoder. */
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }
    vlc_mutex_unlock( &p_input->stream.stream_lock );

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
}

