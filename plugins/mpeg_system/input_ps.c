/*****************************************************************************
 * input_ps.c: PS demux and packet management
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input_ps.c,v 1.4 2001/12/12 13:48:09 massiot Exp $
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

#define MODULE_NAME mpeg_ps
#include "modules_inner.h"

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef STRNCASECMP_IN_STRINGS_H
#   include <strings.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#include <fcntl.h>

#include "common.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "input_ps.h"

#include "debug.h"

#include "modules.h"
#include "modules_export.h"

/*****************************************************************************
 * fseeko: fseeko replacement for BSDI.
 *****************************************************************************/
#ifdef __bsdi__
int    __sfseek __P(( FILE *, fpos_t, int ));
fpos_t __sftell __P(( FILE * ));

static __inline__ off_t fseeko( FILE *p_file, off_t i_offset, int i_pos )
{
    return __sfseek( p_file, i_offset, i_pos );
}
#endif

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  PSProbe         ( probedata_t * );
static int  PSRead          ( struct input_thread_s *,
                             data_packet_t * p_packets[INPUT_READ_ONCE] );
static void PSInit          ( struct input_thread_s * );
static void PSEnd           ( struct input_thread_s * );
static int  PSSetProgram    ( struct input_thread_s * , pgrm_descriptor_t * );
static void PSSeek          ( struct input_thread_s *, off_t );

/*****************************************************************************
 * Declare a buffer manager
 *****************************************************************************/
#define FLAGS           BUFFERS_NOFLAGS
#define NB_LIFO         2
DECLARE_BUFFERS_EMBEDDED( FLAGS, NB_LIFO );
DECLARE_BUFFERS_INIT( FLAGS, NB_LIFO );
DECLARE_BUFFERS_END( FLAGS, NB_LIFO );
DECLARE_BUFFERS_NEWPACKET( FLAGS, NB_LIFO );
DECLARE_BUFFERS_DELETEPACKET( FLAGS, NB_LIFO, 150 );
DECLARE_BUFFERS_NEWPES( FLAGS, NB_LIFO );
DECLARE_BUFFERS_DELETEPES( FLAGS, NB_LIFO, 150, 150 );


/*****************************************************************************
 * Functions exported as capabilities. They are declared as static so that
 * we don't pollute the namespace too much.
 *****************************************************************************/
void _M( input_getfunctions )( function_list_t * p_function_list )
{
#define input p_function_list->functions.input
    p_function_list->pf_probe = PSProbe;
    input.pf_init             = PSInit;
    input.pf_open             = NULL;
    input.pf_close            = NULL;
    input.pf_end              = PSEnd;
    input.pf_init_bit_stream  = InitBitstream;
    input.pf_set_area         = NULL;
    input.pf_set_program      = PSSetProgram;
    input.pf_read             = PSRead;
    input.pf_demux            = input_DemuxPS;
    input.pf_new_packet       = input_NewPacket;
    input.pf_new_pes          = input_NewPES;
    input.pf_delete_packet    = input_DeletePacket;
    input.pf_delete_pes       = input_DeletePES;
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
    int i_score = 10;

    if( TestMethod( INPUT_METHOD_VAR, "ps" ) )
    {
        return( 999 );
    }

    if( ( strlen(psz_name) > 5 ) && (!strncasecmp( psz_name, "file:", 5 )
                                      || !strncasecmp( psz_name, "http:", 5 )) )
    {
        /* If the user specified "file:" or "http:" then it's probably a
         * PS file */
        i_score = 100;
        psz_name += 5;
    }

    return( i_score );
}

/*****************************************************************************
 * PSInit: initializes PS structures
 *****************************************************************************/
static void PSInit( input_thread_t * p_input )
{
    if( (p_input->p_method_data = input_BuffersInit()) == NULL )
    {
        p_input->b_error = 1;
        return;
    }

    if( p_input->p_stream == NULL )
    {
        /* Re-open the socket as a buffered FILE stream */
        p_input->p_stream = fdopen( p_input->i_handle, "r" );

        if( p_input->p_stream == NULL )
        {
            intf_ErrMsg( "Cannot open file (%s)", strerror(errno) );
            p_input->b_error = 1;
            return;
        }
    }

    /* FIXME : detect if InitStream failed */
    input_InitStream( p_input, sizeof( stream_ps_data_t ) );
    input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );
    
    p_input->stream.p_selected_program = 
            p_input->stream.pp_programs[0] ;
    p_input->stream.p_new_program = 
            p_input->stream.pp_programs[0] ;
    
    if( p_input->stream.b_seekable )
    {
        stream_ps_data_t * p_demux_data =
             (stream_ps_data_t *)p_input->stream.pp_programs[0]->p_demux_data;

        rewind( p_input->p_stream );

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
                p_input->pf_delete_packet( p_input->p_method_data, pp_packets[i] );
            }

            /* File too big. */
            if( p_input->stream.p_selected_area->i_tell >
                                                    INPUT_PREPARSE_LENGTH )
            {
                break;
            }
        }
        rewind( p_input->p_stream );
        vlc_mutex_lock( &p_input->stream.stream_lock );

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
                        if( main_GetIntVariable( INPUT_CHANNEL_VAR, 0 )
                                == ((p_es->i_id & 0x1F00) >> 8) )
                        switch( main_GetIntVariable( INPUT_AUDIO_VAR, 0 ) )
                        {
                        case 0:
                            main_PutIntVariable( INPUT_AUDIO_VAR,
                                                 REQUESTED_LPCM );
                        case REQUESTED_LPCM:
                            input_SelectES( p_input, p_es );
                        }
                        break;
                }
            }
                    
        }
#endif
        if( p_main->b_stats )
        {
            input_DumpStream( p_input );
        }
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else
    {
        /* The programs will be added when we read them. */
        vlc_mutex_lock( &p_input->stream.stream_lock );
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
    input_BuffersEnd( p_input->p_method_data );
}

/*****************************************************************************
 * SafeRead: reads a chunk of stream and correctly detects errors
 *****************************************************************************/
static __inline__ int SafeRead( input_thread_t * p_input, byte_t * p_buffer,
                                size_t i_len )
{
    int                 i_error;

    while( fread( p_buffer, i_len, 1, p_input->p_stream ) != 1 )
    {
        if( feof( p_input->p_stream ) )
        {
            return( 1 );
        }

        if( (i_error = ferror( p_input->p_stream )) )
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

    memset( pp_packets, 0, INPUT_READ_ONCE * sizeof(data_packet_t *) );
    for( i_packet = 0; i_packet < INPUT_READ_ONCE; i_packet++ )
    {
        /* Read what we believe to be a packet header. */
        if( (i_error = SafeRead( p_input, p_header, 4 )) )
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
                intf_WarnMsg( 3, "Garbage at input (%.8x)", i_startcode );
            }

            while( (i_startcode & 0xFFFFFF00) != 0x100L )
            {
                i_startcode <<= 8;
                if( (i_dummy = getc( p_input->p_stream )) != EOF )
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
        }

        /* 0x1B9 == SYSTEM_END_CODE, it is only 4 bytes long. */
        if( U32_AT(p_header) != 0x1B9 )
        {
            /* The packet is at least 6 bytes long. */
            if( (i_error = SafeRead( p_input, p_header + 4, 2 )) )
            {
                return( i_error );
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
        }
        else
        {
            /* System End Code */
            i_packet_size = -2;
        }

        /* Fetch a packet of the appropriate size. */
        p_data = p_input->pf_new_packet( p_input->p_method_data,
                                         i_packet_size + 6 );
        if( p_data == NULL )
        {
            intf_ErrMsg( "Out of memory" );
            return( -1 );
        }

        if( U32_AT(p_header) != 0x1B9 )
        {
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
        }
        else
        {
            /* Copy the small header. */
            memcpy( p_data->p_buffer, p_header, 4 );
        }

        /* Give the packet to the other input stages. */
        pp_packets[i_packet] = p_data;
    }

    return( 0 );
}

/*****************************************************************************
 * PSSetProgram: Does nothing since a PS Stream is mono-program
 *****************************************************************************/
static int PSSetProgram( input_thread_t * p_input, 
            pgrm_descriptor_t * p_program)
{
    return( 0 );
}
/*****************************************************************************
 * PSSeek: changes the stream position indicator
 *****************************************************************************/
static void PSSeek( input_thread_t * p_input, off_t i_position )
{
    /* A little bourrin but should work for a while --Meuuh */
#if defined( WIN32 ) || defined( SYS_GNU0_2 )
    fseek( p_input->p_stream, (long)i_position, SEEK_SET );
#else
    fseeko( p_input->p_stream, i_position, SEEK_SET );
#endif

    p_input->stream.p_selected_area->i_tell = i_position;
}

