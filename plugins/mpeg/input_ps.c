/*****************************************************************************
 * input_ps.c: PS demux and packet management
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: input_ps.c,v 1.14 2001/04/13 05:36:12 stef Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Cyril Deguet <asmax@via.ecp.fr>
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

#define MODULE_NAME ps
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
#include <unistd.h>
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

#include "input_ps.h"
#include "mpeg_system.h"

#include "debug.h"

#include "modules.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  PSProbe     ( probedata_t * );
static int  PSRead      ( struct input_thread_s *,
                          data_packet_t * p_packets[INPUT_READ_ONCE] );
static void PSInit      ( struct input_thread_s * );
static void PSEnd       ( struct input_thread_s * );
static void PSSeek      ( struct input_thread_s *, off_t );
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
    p_function_list->pf_probe = PSProbe;
    input.pf_init             = PSInit;
    input.pf_open             = input_FileOpen;
    input.pf_close            = input_FileClose;
    input.pf_end              = PSEnd;
    input.pf_set_area         = NULL;
    input.pf_read             = PSRead;
    input.pf_demux            = input_DemuxPS;
    input.pf_new_packet       = NewPacket;
    input.pf_new_pes          = NewPES;
    input.pf_delete_packet    = DeletePacket;
    input.pf_delete_pes       = DeletePES;
    input.pf_rewind           = NULL;
    input.pf_seek             = PSSeek;
#undef input
}

/*
 * Data reading functions
 */

/*****************************************************************************
 * PSProbe: verifies that the stream is a PS stream
 *****************************************************************************/
static int PSProbe( probedata_t *p_data )
{
    input_thread_t * p_input = (input_thread_t *)p_data;

    char * psz_name = p_input->p_source;
    int i_handle;
    int i_score = 10;

    if( TestMethod( INPUT_METHOD_VAR, "ps" ) )
    {
        return( 999 );
    }

    if( ( strlen(psz_name) > 5 ) && !strncasecmp( psz_name, "file:", 5 ) )
    {
        /* If the user specified "file:" then it's probably a file */
        i_score = 100;
        psz_name += 5;
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
 * PSInit: initializes PS structures
 *****************************************************************************/
static void PSInit( input_thread_t * p_input )
{
    thread_ps_data_t *  p_method;

    if( (p_method =
         (thread_ps_data_t *)malloc( sizeof(thread_ps_data_t) )) == NULL )
    {
        intf_ErrMsg( "Out of memory" );
        p_input->b_error = 1;
        return;
    }

    p_input->p_plugin_data = (void *)p_method;
    p_input->p_method_data = NULL;

    /* Re-open the socket as a buffered FILE stream */
    if( (p_method->stream = fdopen( p_input->i_handle, "r" )) == NULL )
    {
        intf_ErrMsg( "Cannot open file (%s)", strerror(errno) );
        p_input->b_error = 1;
        return;
    }
    rewind( p_method->stream );

    /* FIXME : detect if InitStream failed */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );
    input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );

    if( p_input->stream.b_seekable )
    {
        stream_ps_data_t * p_demux_data =
             (stream_ps_data_t *)p_input->stream.pp_programs[0]->p_demux_data;

        /* Pre-parse the stream to gather stream_descriptor_t. */
        p_input->stream.pp_programs[0]->b_is_ok = 0;
        p_demux_data->i_PSM_version = EMPTY_PSM_VERSION;

        while( !p_input->b_die && !p_input->b_error
                && !p_demux_data->b_has_PSM )
        {
            int                 i_result, i;
            data_packet_t *     pp_packets[INPUT_READ_ONCE];

            i_result = PSRead( p_input, pp_packets );
            if( i_result == 1 )
            {
                /* EOF */
                vlc_mutex_lock( &p_input->stream.stream_lock );
                p_input->stream.pp_programs[0]->b_is_ok = 1;
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                break;
            }
            if( i_result == -1 )
            {
                p_input->b_error = 1;
                break;
            }

            for( i = 0; i < INPUT_READ_ONCE && pp_packets[i] != NULL; i++ )
            {
                /* FIXME: use i_p_config_t */
                input_ParsePS( p_input, pp_packets[i] );
                DeletePacket( p_input->p_method_data, pp_packets[i] );
            }

            /* File too big. */
            if( p_input->stream.p_selected_area->i_tell >
                                                    INPUT_PREPARSE_LENGTH )
            {
                break;
            }
        }
        rewind( p_method->stream );
        vlc_mutex_lock( &p_input->stream.stream_lock );

        /* file input method */
        p_input->stream.i_method = INPUT_METHOD_FILE;

        p_input->stream.p_selected_area->i_tell = 0;
        if( p_demux_data->b_has_PSM )
        {
            /* (The PSM decoder will care about spawning the decoders) */
            p_input->stream.pp_programs[0]->b_is_ok = 1;
        }
#ifdef AUTO_SPAWN
        else
        {
            /* (We have to do it ourselves) */
            int                 i_es;

            /* FIXME: we should do multiple passes in case an audio type
             * is not present */
            for( i_es = 0;
                 i_es < p_input->stream.pp_programs[0]->i_es_number;
                 i_es++ )
            {
#define p_es p_input->stream.pp_programs[0]->pp_es[i_es]
                switch( p_es->i_type )
                {
                    case MPEG1_VIDEO_ES:
                    case MPEG2_VIDEO_ES:
                        input_SelectES( p_input, p_es );
                        break;

                    case MPEG1_AUDIO_ES:
                    case MPEG2_AUDIO_ES:
                        if( main_GetIntVariable( INPUT_CHANNEL_VAR, 0 )
                                == (p_es->i_id & 0x1F) )
                        switch( main_GetIntVariable( INPUT_AUDIO_VAR, 0 ) )
                        {
                        case 0:
                            main_PutIntVariable( INPUT_AUDIO_VAR,
                                                 REQUESTED_MPEG );
                        case REQUESTED_MPEG:
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case AC3_AUDIO_ES:
                        if( main_GetIntVariable( INPUT_CHANNEL_VAR, 0 )
                                == ((p_es->i_id & 0xF00) >> 8) )
                        switch( main_GetIntVariable( INPUT_AUDIO_VAR, 0 ) )
                        {
                        case 0:
                            main_PutIntVariable( INPUT_AUDIO_VAR,
                                                 REQUESTED_AC3 );
                        case REQUESTED_AC3:
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case DVD_SPU_ES:
                        if( main_GetIntVariable( INPUT_SUBTITLE_VAR, -1 )
                                == ((p_es->i_id & 0x1F00) >> 8) )
                        {
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case LPCM_AUDIO_ES:
                        /* FIXME ! */
                        break;
                }
            }
                    
        }
#endif
#ifdef STATS
        input_DumpStream( p_input );
#endif
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else
    {
        /* The programs will be added when we read them. */
        vlc_mutex_lock( &p_input->stream.stream_lock );

        /* file input method */
        p_input->stream.i_method = INPUT_METHOD_FILE;

        p_input->stream.pp_programs[0]->b_is_ok = 0;
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
}

/*****************************************************************************
 * PSEnd: frees unused data
 *****************************************************************************/
static void PSEnd( input_thread_t * p_input )
{
    free( p_input->p_plugin_data );
}

/*****************************************************************************
 * SafeRead: reads a chunk of stream and correctly detects errors
 *****************************************************************************/
static __inline__ int SafeRead( input_thread_t * p_input, byte_t * p_buffer,
                                size_t i_len )
{
    thread_ps_data_t *  p_method;
    int                 i_error;

    p_method = (thread_ps_data_t *)p_input->p_plugin_data;
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
 * PSRead: reads data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 if everything went well, and 1 in case of
 * EOF.
 *****************************************************************************/
static int PSRead( input_thread_t * p_input,
                   data_packet_t * pp_packets[INPUT_READ_ONCE] )
{
    byte_t              p_header[6];
    data_packet_t *     p_data;
    size_t              i_packet_size;
    int                 i_packet, i_error;
    thread_ps_data_t *  p_method;

    p_method = (thread_ps_data_t *)p_input->p_plugin_data;

    memset( pp_packets, 0, INPUT_READ_ONCE * sizeof(data_packet_t *) );
    for( i_packet = 0; i_packet < INPUT_READ_ONCE; i_packet++ )
    {
        /* Read what we believe to be a packet header. */
        if( (i_error = SafeRead( p_input, p_header, 6 )) )
        {
            return( i_error );
        }

        if( (U32_AT(p_header) & 0xFFFFFF00) != 0x100L )
        {
            /* This is not the startcode of a packet. Read the stream
             * until we find one. */
            u32         i_startcode = U32_AT(p_header);
            int         i_dummy;

            if( i_startcode )
            {
                /* It is common for MPEG-1 streams to pad with zeros
                 * (although it is forbidden by the recommendation), so
                 * don't bother everybody in this case. */
                intf_WarnMsg( 1, "Garbage at input (%.8x)", i_startcode );
            }

            while( (i_startcode & 0xFFFFFF00) != 0x100L )
            {
                i_startcode <<= 8;
                if( (i_dummy = getc( p_method->stream )) != EOF )
                {
                    i_startcode |= i_dummy;
                }
                else
                {
                    return( 1 );
                }
            }
            /* Packet found. */
            *(u32 *)p_header = U32_AT(&i_startcode);
            if( (i_error = SafeRead( p_input, p_header + 4, 2 )) )
            {
                return( i_error );
            }
        }

        if( U32_AT(p_header) != 0x1BA )
        {
            /* That's the case for all packets, except pack header. */
            i_packet_size = U16_AT(&p_header[4]);
        }
        else
        {
            /* Pack header. */
            if( (p_header[4] & 0xC0) == 0x40 )
            {
                /* MPEG-2 */
                i_packet_size = 8;
            }
            else if( (p_header[4] & 0xF0) == 0x20 )
            {
                /* MPEG-1 */
                i_packet_size = 6;
            }
            else
            {
                intf_ErrMsg( "Unable to determine stream type" );
                return( -1 );
            }
        }

        /* Fetch a packet of the appropriate size. */
        if( (p_data = NewPacket( p_input, i_packet_size + 6 )) == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            return( -1 );
        }

        /* Copy the header we already read. */
        memcpy( p_data->p_buffer, p_header, 6 );

        /* Read the remaining of the packet. */
        if( i_packet_size && (i_error =
                SafeRead( p_input, p_data->p_buffer + 6, i_packet_size )) )
        {
            return( i_error );
        }

        /* In MPEG-2 pack headers we still have to read stuffing bytes. */
        if( U32_AT(p_header) == 0x1BA )
        {
            if( i_packet_size == 8 && (p_data->p_buffer[13] & 0x7) != 0 )
            {
                /* MPEG-2 stuffing bytes */
                byte_t      p_garbage[8];
                if( (i_error = SafeRead( p_input, p_garbage,
                                         p_data->p_buffer[13] & 0x7)) )
                {
                    return( i_error );
                }
            }
        }

        /* Give the packet to the other input stages. */
        pp_packets[i_packet] = p_data;
    }

    return( 0 );
}

/*****************************************************************************
 * PSSeek: changes the stream position indicator
 *****************************************************************************/
static void PSSeek( input_thread_t * p_input, off_t i_position )
{
    thread_ps_data_t *  p_method;

    p_method = (thread_ps_data_t *)p_input->p_plugin_data;

    /* A little bourrin but should work for a while --Meuuh */
    fseeko( p_method->stream, i_position, SEEK_SET );

    p_input->stream.p_selected_area->i_tell = i_position;
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

    /* Safety check */
    if( i_size > INPUT_MAX_PACKET_SIZE )
    {
        intf_ErrMsg( "Packet too big (%d)", i_size );
        return NULL;
    }

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

    /* Initialize data */
    p_data->p_next = NULL;
    p_data->b_discard_payload = 0;

    p_data->p_payload_start = p_data->p_buffer;
    p_data->p_payload_end = p_data->p_buffer + i_size;

    return( p_data );
}

/*****************************************************************************
 * NewPES: allocates a pes packet
 *****************************************************************************/
static pes_packet_t * NewPES( void * p_garbage )
{
    pes_packet_t * p_pes;

    if( (p_pes = (pes_packet_t *)malloc( sizeof(pes_packet_t) )) == NULL )
    {
        intf_DbgMsg( "Out of memory" );
        return NULL;
    }

    p_pes->b_data_alignment = p_pes->b_discontinuity =
        p_pes->i_pts = p_pes->i_dts = 0;
    p_pes->i_pes_size = 0;
    p_pes->p_first = NULL;

    return( p_pes );
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

