/*****************************************************************************
 * mpeg_es.c : Elementary Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: mpeg_es.c,v 1.11 2002/07/24 15:21:47 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
#include <string.h>                                              /* strdup() */
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <sys/types.h>

/*****************************************************************************
 * Constants
 *****************************************************************************/
#define ES_PACKET_SIZE 65536
#define MAX_PACKETS_IN_FIFO 3

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list );
static int  ESDemux ( input_thread_t * );
static int  ESInit  ( input_thread_t * );
static void ESEnd   ( input_thread_t * );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( _("ISO 13818-2 MPEG Elementary Stream input") )
    ADD_CAPABILITY( DEMUX, 150 )
    ADD_SHORTCUT( "es" )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
    input_getfunctions( &p_module->p_functions->demux );
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list )
{
#define input p_function_list->functions.demux
    input.pf_init             = ESInit;
    input.pf_end              = ESEnd;
    input.pf_demux            = ESDemux;
    input.pf_rewind           = NULL;
#undef input
}

/*
 * Data reading functions
 */

/*****************************************************************************
 * ESInit: initializes ES structures
 *****************************************************************************/
static int ESInit( input_thread_t * p_input )
{
    es_descriptor_t *   p_es;
    byte_t *            p_peek;

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

    if( *p_peek || *(p_peek + 1) || *(p_peek + 2) != 1 )
    {
        if( *p_input->psz_demux && !strncmp( p_input->psz_demux, "es", 3 ) )
        {
            /* User forced */
            msg_Err( p_input, "this doesn't look like an MPEG ES stream, continuing" );
        }
        else
        {
            msg_Warn( p_input, "ES module discarded (no startcode)" );
            return( -1 );
        }
    }
    else if( *(p_peek + 3) > 0xb9 )
    {
        if( *p_input->psz_demux && !strncmp( p_input->psz_demux, "es", 3 ) )
        {
            /* User forced */
            msg_Err( p_input, "this seems to be a system stream (PS plug-in ?), but continuing" );
        }
        else
        {
            msg_Warn( p_input, "ES module discarded (system startcode)" );
            return( -1 );
        }
    }

    if( input_InitStream( p_input, 0 ) == -1 )
    {
        return( -1 );
    }
    input_AddProgram( p_input, 0, 0 );
    p_input->stream.p_selected_program = p_input->stream.pp_programs[0];
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_es = input_AddES( p_input, p_input->stream.p_selected_program, 0xE0, 0 );
    p_es->i_stream_id = 0xE0;
    p_es->i_fourcc = VLC_FOURCC('m','p','g','v');
    p_es->i_cat = VIDEO_ES;
    input_SelectES( p_input, p_es );
    p_input->stream.p_selected_area->i_tell = 0;
    p_input->stream.p_selected_program->b_is_ok = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( 0 );
}

/*****************************************************************************
 * ESEnd: frees unused data
 *****************************************************************************/
static void ESEnd( input_thread_t * p_input )
{
}

/*****************************************************************************
 * ESDemux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, 1 otherwise
 *****************************************************************************/
static int ESDemux( input_thread_t * p_input )
{
    ssize_t         i_read;
    decoder_fifo_t * p_fifo =
        p_input->stream.p_selected_program->pp_es[0]->p_decoder_fifo;
    pes_packet_t *  p_pes;
    data_packet_t * p_data;

    if( p_fifo == NULL )
    {
        return -1;
    }

    i_read = input_SplitBuffer( p_input, &p_data, ES_PACKET_SIZE );

    if ( i_read <= 0 )
    {
        return i_read;
    }

    p_pes = input_NewPES( p_input->p_method_data );

    if( p_pes == NULL )
    {
        msg_Err( p_input, "out of memory" );
        input_DeletePacket( p_input->p_method_data, p_data );
        return -1;
    }

    p_pes->i_rate = p_input->stream.control.i_rate;
    p_pes->p_first = p_pes->p_last = p_data;
    p_pes->i_nb_data = 1;

    vlc_mutex_lock( &p_fifo->data_lock );

    /* If the decoder is waiting for us, wake him up */
    vlc_cond_signal( &p_fifo->data_wait );

    if( p_fifo->i_depth >= MAX_PACKETS_IN_FIFO )
    {
        /* Wait for the decoder. */
        vlc_cond_wait( &p_fifo->data_wait, &p_fifo->data_lock );
    }
    vlc_mutex_unlock( &p_fifo->data_lock );

    if( (p_input->stream.p_selected_program->i_synchro_state == SYNCHRO_REINIT)
         | (input_ClockManageControl( p_input, 
                      p_input->stream.p_selected_program,
                         (mtime_t)0 ) == PAUSE_S) )
    {
        msg_Warn( p_input, "synchro reinit" );
        p_pes->i_pts = mdate() + DEFAULT_PTS_DELAY;
        p_input->stream.p_selected_program->i_synchro_state = SYNCHRO_OK;
    }

    input_DecodePES( p_fifo, p_pes );

    return 1;
}

