/*****************************************************************************
 * input_ps.c: PS demux and packet management
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"

#include "intf_msg.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"

#include "input.h"

#include "input_ps.h"
#include "mpeg_system.h"

#include "debug.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  PSProbe     ( struct input_thread_s * );
static void PSRead      ( struct input_thread_s *,
                          data_packet_t * p_packets[INPUT_READ_ONCE] );
static void PSInit      ( struct input_thread_s * );
static void PSEnd       ( struct input_thread_s * );
static struct data_packet_s * NewPacket ( void *, size_t );
static void DeletePacket( void *, struct data_packet_s * );
static void DeletePES   ( void *, struct pes_packet_s * );

/*
 * Data reading functions
 */

/*****************************************************************************
 * PSProbe: verifies that the stream is a PS stream
 *****************************************************************************/
static int PSProbe( input_thread_t * p_input )
{
    /* verify that the first three bytes are 0x000001, or unscramble and
     * re-do. */
    return 1;
}

/*****************************************************************************
 * PSInit: initializes PS structures
 *****************************************************************************/
static void PSInit( input_thread_t * p_input )
{
    thread_ps_data_t *  p_method;
    stream_ps_data_t *  p_demux;

    if( (p_method =
         (thread_ps_data_t *)malloc( sizeof(thread_ps_data_t) )) == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }

    p_input->p_method_data = (void *)p_method;

    /* Re-open the socket as a buffered FILE stream */
    if( (p_method->stream = fdopen( p_input->i_handle, "r" )) == NULL )
    {
        intf_ErrMsg( "Cannot open file (%s)", strerror(errno) );
        p_input->b_error = 1;
        return;
    }
    fseek( p_method->stream, 0, SEEK_SET );

    /* Pre-parse the stream to gather stream_descriptor_t. */

    /* FIXME */
    p_input->stream.pp_programs =
         (pgrm_descriptor_t **)malloc( sizeof(pgrm_descriptor_t *) );
    p_input->stream.pp_programs[0] =
         (pgrm_descriptor_t *)malloc( sizeof(pgrm_descriptor_t) );
    p_input->stream.pp_programs[0]->i_synchro_state = SYNCHRO_START;
    p_input->stream.pp_programs[0]->delta_cr = 0;
    p_input->stream.pp_programs[0]->last_cr = 0;
    p_input->stream.pp_programs[0]->c_average_count = 0;

    p_demux = (stream_ps_data_t *)malloc( sizeof( stream_ps_data_t) );
    p_input->stream.p_demux_data = (void *)p_demux;
    p_demux->b_is_PSM_complete = 0;
}

/*****************************************************************************
 * PSEnd: frees unused data
 *****************************************************************************/
static void PSEnd( input_thread_t * p_input )
{
    free( p_input->stream.p_demux_data );
    free( p_input->p_method_data );
}

/*****************************************************************************
 * PSRead: reads a data packet
 *****************************************************************************/
/* FIXME: read INPUT_READ_ONCE packet at once */
static void PSRead( input_thread_t * p_input,
                    data_packet_t * p_packets[INPUT_READ_ONCE] )
{
    byte_t              p_header[6];
    data_packet_t *     p_data;
    int                 i_packet_size;
    thread_ps_data_t *  p_method;

    p_method = (thread_ps_data_t *)p_input->p_method_data;

    while( fread( p_header, 6, 1, p_method->stream ) != 1 )
    {
        int             i_error;
        if( (i_error = ferror( p_method->stream )) )
        {
            intf_ErrMsg( "Read 1 failed (%s)", strerror(i_error) );
            p_input->b_error = 1;
            return;
        }

        if( feof( p_method->stream ) )
        {
            intf_ErrMsg( "EOF reached" );
            p_input->b_error = 1;
            return;
        }
    }

    if( (U32_AT(p_header) & 0xFFFFFF00) != 0x100L )
    {
        u32         i_buffer = U32_AT(p_header);
        intf_ErrMsg( "Garbage at input (%x)\n", i_buffer );
        while( (i_buffer & 0xFFFFFF00) != 0x100L )
        {
            i_buffer <<= 8;
            i_buffer |= getc( p_method->stream );
            if( feof(p_method->stream) || ferror(p_method->stream) )
            {
                p_input->b_error = 1;
                return;
            }
        }
        *(u32 *)p_header = i_buffer;
        fread( p_header + 4, 2, 1, p_method->stream );
    }

    if( U32_AT(p_header) != 0x1BA )
    {
        i_packet_size = U16_AT(&p_header[4]);
    }
    else
    {
        i_packet_size = 8;
    }

    if( (p_data = NewPacket( p_input, i_packet_size + 6 )) == NULL )
    {
        p_input->b_error = 1;
        intf_ErrMsg( "Out of memory" );
        return;
    }

    memcpy( p_data->p_buffer, p_header, 6 );

    /* FIXME: catch EINTR ! */
    while( fread( p_data->p_buffer + 6, i_packet_size,
               1, p_method->stream ) != 1 )
    {
        int             i_error;
        if( (i_error = ferror( p_method->stream)) )
        {
            intf_ErrMsg( "Read 1 failed (%s)", strerror(i_error) );
            p_input->b_error = 1;
            return;
        }

        if( feof( p_method->stream ) )
        {
            intf_ErrMsg( "EOF reached" );
            p_input->b_error = 1;
            return;
        }
    }

    if( U32_AT(p_header) == 0x1BA )
    {
        /* stuffing_bytes */
        byte_t      p_garbage[8];
        /* FIXME: catch EINTR ! */
        if( (p_data->p_buffer[13] & 0x3) != 0 )
        {
            fread( p_garbage, p_garbage[0] & 0x3, 1,
                   p_method->stream );
        }
    }

    memset( p_packets, 0, sizeof(p_packets) );
    p_packets[0] = p_data;
}


/*
 * Packet management utilities
 */

/*****************************************************************************
 * NewPacket: allocates a data packet
 *****************************************************************************/
static struct data_packet_s * NewPacket( void * p_garbage,
                                         size_t i_size )
{
    data_packet_t * p_data;

    if( (p_data = (data_packet_t *)malloc( sizeof(data_packet_t) )) == NULL )
    {
        intf_DbgMsg( "Out of memory" );
        return NULL;
    }

    if( (p_data->p_buffer = (byte_t *)malloc( i_size )) == NULL )
    {
        intf_DbgMsg( "Out of memory" );
        free( p_data );
        return NULL;
    }

    p_data->p_payload_start = p_data->p_buffer;
    p_data->p_payload_end = p_data->p_buffer + i_size;

    return( p_data );
}

/*****************************************************************************
 * DeletePacket: deletes a data packet
 *****************************************************************************/
static void DeletePacket( void * p_garbage,
                          data_packet_t * p_data )
{
    ASSERT(p_data);
    ASSERT(p_data->p_buffer);
    free( p_data->p_buffer );
    free( p_data );
}

/*****************************************************************************
 * DeletePES: deletes a PES packet and associated data packets
 *****************************************************************************/
static void DeletePES( void * p_garbage, pes_packet_t * p_pes )
{
    data_packet_t *     p_data;
    data_packet_t *     p_next;

    p_data = p_pes->p_first;

    while( p_data != NULL )
    {
        p_next = p_data->p_next;
        free( p_data->p_buffer );
        free( p_data );
        p_data = p_next;
    }

    free( p_pes );
}

/*****************************************************************************
 * PSKludge: fakes a PS plugin (FIXME)
 *****************************************************************************/
input_capabilities_t * PSKludge( void )
{
    input_capabilities_t *  p_plugin;

    p_plugin = (input_capabilities_t *)malloc( sizeof(input_capabilities_t) );
    p_plugin->pf_init = PSInit;
    p_plugin->pf_read = PSRead;
    p_plugin->pf_demux = input_DemuxPS; /* FIXME: use i_p_config_t ! */
    p_plugin->pf_new_packet = NewPacket;
    p_plugin->pf_delete_packet = DeletePacket;
    p_plugin->pf_delete_pes = DeletePES;
    p_plugin->pf_rewind = NULL;
    p_plugin->pf_seek = NULL;

    return( p_plugin );
}
