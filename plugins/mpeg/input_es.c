/*****************************************************************************
 * input_es.c: Elementary Stream demux and packet management
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: input_es.c,v 1.5 2001/05/31 03:12:49 sam Exp $
 *
 * Authors: 
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
static struct pes_packet_s *  NewPES    ( void * );
static struct data_packet_s * NewPacket ( void *, size_t );
static void DeletePacket( void *, struct data_packet_s * );
static void DeletePES   ( void *, struct pes_packet_s * );

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
    input.pf_set_area         = NULL;
    input.pf_read             = ESRead;
    input.pf_demux            = ESDemux;
    input.pf_new_packet       = NewPacket;
    input.pf_new_pes          = NewPES;
    input.pf_delete_packet    = DeletePacket;
    input.pf_delete_pes       = DeletePES;
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
    int i_score = 10;

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
    thread_es_data_t *  p_method;

    if( (p_method =
         (thread_es_data_t *)malloc( sizeof(thread_es_data_t) )) == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }
    p_input->p_plugin_data = (void *)p_method;

    p_input->pf_open  = p_input->pf_file_open;
    p_input->pf_close = p_input->pf_file_close;
}

/*****************************************************************************
 * ESEnd: frees unused data
 *****************************************************************************/
static void ESEnd( input_thread_t * p_input )
{
    /* XXX */

    free( p_input->p_plugin_data );
}

/*****************************************************************************
 * SafeRead: reads a chunk of stream and correctly detects errors
 *****************************************************************************/
static __inline__ int SafeRead( input_thread_t * p_input, byte_t * p_buffer,
                                size_t i_len )
{
    thread_es_data_t *  p_method;
    int                 i_error;

    p_method = (thread_es_data_t *)p_input->p_plugin_data;
    while( fread( p_buffer, i_len, 1, p_method->stream ) != 1 )
    {
        if( feof( p_method->stream ) )
        {
            return( 1 );
        }

        if( (i_error = ferror( p_method->stream )) )
        {
            intf_ErrMsg( "Read failed (%s)", strerror(i_error) );
            return( -1 );
        }
    }
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_tell += i_len;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return( 0 );
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
    /* XXX */

    return( 0 );
}

/*****************************************************************************
 * ESSeek: changes the stream position indicator
 *****************************************************************************/
static void ESSeek( input_thread_t * p_input, off_t i_position )
{
    thread_es_data_t *  p_method;

    p_method = (thread_es_data_t *)p_input->p_plugin_data;

    /* A little bourrin but should work for a while --Meuuh */
#ifndef WIN32
    fseeko( p_method->stream, i_position, SEEK_SET );
#else
    fseek( p_method->stream, (long)i_position, SEEK_SET );
#endif

    p_input->stream.p_selected_area->i_tell = i_position;
}

void ESDemux( input_thread_t * p_input, data_packet_t * p_data )
{
    /* XXX */
}

/*
 * Packet management utilities
 */

/*****************************************************************************
 * NewPacket: allocates a data packet
 *****************************************************************************/
static struct data_packet_s * NewPacket( void * p_packet_cache,
                                         size_t l_size )
{ 
    /* XXX */

    return NULL;
}


/*****************************************************************************
 * NewPES: allocates a pes packet
 *****************************************************************************/
static pes_packet_t * NewPES( void * p_packet_cache )
{
    /* XXX */

    return NULL;
}

/*****************************************************************************
 * DeletePacket: deletes a data packet
 *****************************************************************************/
static void DeletePacket( void * p_packet_cache,
                          data_packet_t * p_data )
{
    /* XXX */
}

/*****************************************************************************
 * DeletePES: deletes a PES packet and associated data packets
 *****************************************************************************/
static void DeletePES( void * p_packet_cache, pes_packet_t * p_pes )
{
    /* XXX */
}

