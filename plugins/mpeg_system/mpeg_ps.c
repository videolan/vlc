/*****************************************************************************
 * mpeg_ps.c : Program Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: mpeg_ps.c,v 1.4 2002/03/01 00:33:18 massiot Exp $
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

#include <videolan/vlc.h>

#include <sys/types.h>

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

/*****************************************************************************
 * Constants
 *****************************************************************************/
#define PS_READ_ONCE 50

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void input_getfunctions( function_list_t * p_function_list );
static int  PSDemux         ( struct input_thread_s * );
static int  PSInit          ( struct input_thread_s * );
static void PSEnd           ( struct input_thread_s * );

/*****************************************************************************
 * Build configuration tree.
 *****************************************************************************/
MODULE_CONFIG_START
MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "ISO 13818-1 MPEG Program Stream input" )
    ADD_CAPABILITY( DEMUX, 100 )
    ADD_SHORTCUT( "ps" )
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
    input.pf_init             = PSInit;
    input.pf_end              = PSEnd;
    input.pf_demux            = PSDemux;
    input.pf_rewind           = NULL;
#undef input
}

/*
 * Data reading functions
 */

/*****************************************************************************
 * PSRead: reads one PS packet
 *****************************************************************************/
#define PEEK( SIZE )                                                        \
    i_error = input_Peek( p_input, &p_peek, SIZE );                         \
    if( i_error == -1 )                                                     \
    {                                                                       \
        return( -1 );                                                       \
    }                                                                       \
    else if( i_error < SIZE )                                               \
    {                                                                       \
        /* EOF */                                                           \
        return( 0 );                                                        \
    }

static __inline__ ssize_t PSRead( input_thread_t * p_input,
                                  data_packet_t ** pp_data )
{
    byte_t *            p_peek;
    size_t              i_packet_size;
    ssize_t             i_error, i_read;

    /* Read what we believe to be a packet header. */
    PEEK( 4 );

    if( *p_peek || *(p_peek + 1) || *(p_peek + 2) != 1 )
    {
        if( *p_peek || *(p_peek + 1) || *(p_peek + 2) )
        {
            /* It is common for MPEG-1 streams to pad with zeros
             * (although it is forbidden by the recommendation), so
             * don't bother everybody in this case. */
            intf_WarnMsg( 3, "input warning: garbage at input (0x%x%x%x%x)",
                 *p_peek, *(p_peek + 1), *(p_peek + 2), *(p_peek + 3) );
        }

        /* This is not the startcode of a packet. Read the stream
         * until we find one. */
        while( *p_peek || *(p_peek + 1) || *(p_peek + 2) != 1 )
        {
            p_input->p_current_data++;
            PEEK( 4 );
        }
        /* Packet found. */
    }

    /* 0x1B9 == SYSTEM_END_CODE, it is only 4 bytes long. */
    if( p_peek[3] != 0xB9 )
    {
        /* The packet is at least 6 bytes long. */
        PEEK( 6 );

        if( p_peek[3] != 0xBA )
        {
            /* That's the case for all packets, except pack header. */
            i_packet_size = (p_peek[4] << 8) | p_peek[5];
        }
        else
        {
            /* Pack header. */
            if( (p_peek[4] & 0xC0) == 0x40 )
            {
                /* MPEG-2 */
                i_packet_size = 8;
            }
            else if( (p_peek[4] & 0xF0) == 0x20 )
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
    i_read = input_SplitBuffer( p_input, pp_data, i_packet_size + 6 );
    if( i_read <= 0 )
    {
        return( i_read );
    }

    /* In MPEG-2 pack headers we still have to read stuffing bytes. */
    if( ((*pp_data)->p_demux_start[3] == 0xBA) && (i_packet_size == 8) )
    {
        size_t i_stuffing = ((*pp_data)->p_demux_start[13] & 0x7);
        /* Force refill of the input buffer - though we don't care
         * about p_peek. Please note that this is unoptimized. */
        PEEK( i_stuffing );
        p_input->p_current_data += i_stuffing;
    }

    return( 1 );
}

/*****************************************************************************
 * PSInit: initializes PS structures
 *****************************************************************************/
static int PSInit( input_thread_t * p_input )
{
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
        intf_ErrMsg( "input error: cannot peek() (mpeg_ps)" );
        return( -1 );
    }

    if( *p_peek || *(p_peek + 1) || *(p_peek + 2) != 1 )
    {
        if( p_input->psz_demux && strncmp( p_input->psz_demux, "ps", 3 ) )
        {
            /* User forced */
            intf_ErrMsg( "input error: this doesn't seem like an MPEG stream, continuing" );
        }
        else
        {
            intf_WarnMsg( 2, "input: PS plug-in discarded (no startcode)" );
            return( -1 );
        }
    }
    else if( *(p_peek + 3) <= 0xb9 )
    {
        if( p_input->psz_demux && strncmp( p_input->psz_demux, "ps", 3 ) )
        {
            /* User forced */
            intf_ErrMsg( "input error: this seems to be an elementary stream (ES plug-in ?),");
            intf_ErrMsg( "but continuing" );
        }
        else
        {
            intf_WarnMsg( 2, "input: PS plug-in discarded (ES startcode)" );
            return( -1 );
        }
    }

    if( input_InitStream( p_input, sizeof( stream_ps_data_t ) ) == -1 )
    {
        return( -1 );
    }
    input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );
    
    p_input->stream.p_selected_program = 
            p_input->stream.pp_programs[0] ;
    p_input->stream.p_new_program = 
            p_input->stream.pp_programs[0] ;
    
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
            ssize_t             i_result;
            data_packet_t *     p_data;

            i_result = PSRead( p_input, &p_data );

            if( i_result == 0 )
            {
                /* EOF */
                vlc_mutex_lock( &p_input->stream.stream_lock );
                p_input->stream.pp_programs[0]->b_is_ok = 1;
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                break;
            }
            else if( i_result == -1 )
            {
                p_input->b_error = 1;
                break;
            }

            input_ParsePS( p_input, p_data );
            input_DeletePacket( p_input->p_method_data, p_data );

            /* File too big. */
            if( p_input->stream.p_selected_area->i_tell >
                                                    INPUT_PREPARSE_LENGTH )
            {
                break;
            }
        }
        p_input->pf_seek( p_input, (off_t)0 );
        input_AccessReinit( p_input );
        vlc_mutex_lock( &p_input->stream.stream_lock );

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
                        if( config_GetIntVariable( INPUT_CHANNEL_VAR )
                                == (p_es->i_id & 0x1F) ||
                              ( config_GetIntVariable( INPUT_CHANNEL_VAR ) < 0
                                && !(p_es->i_id & 0x1F) ) )
                        switch( config_GetIntVariable( INPUT_AUDIO_VAR ) )
                        {
                        case -1:
                        case REQUESTED_MPEG:
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case AC3_AUDIO_ES:
                        if( config_GetIntVariable( INPUT_CHANNEL_VAR )
                                == ((p_es->i_id & 0xF00) >> 8) ||
                              ( config_GetIntVariable( INPUT_CHANNEL_VAR ) < 0
                                && !((p_es->i_id & 0xF00) >> 8) ) )
                        switch( config_GetIntVariable( INPUT_AUDIO_VAR ) )
                        {
                        case -1:
                        case REQUESTED_AC3:
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case DVD_SPU_ES:
                        if( config_GetIntVariable( INPUT_SUBTITLE_VAR )
                                == ((p_es->i_id & 0x1F00) >> 8) )
                        {
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case LPCM_AUDIO_ES:
                        if( config_GetIntVariable( INPUT_CHANNEL_VAR )
                                == ((p_es->i_id & 0x1F00) >> 8) ||
                              ( config_GetIntVariable( INPUT_CHANNEL_VAR ) < 0
                                && !((p_es->i_id & 0x1F00) >> 8) ) )
                        switch( config_GetIntVariable( INPUT_AUDIO_VAR ) )
                        {
                        case -1:
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

    return( 0 );
}

/*****************************************************************************
 * PSEnd: frees unused data
 *****************************************************************************/
static void PSEnd( input_thread_t * p_input )
{
}

/*****************************************************************************
 * PSDemux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * packets.
 *****************************************************************************/
static int PSDemux( input_thread_t * p_input )
{
    int                 i;

    for( i = 0; i < PS_READ_ONCE; i++ )
    {
        data_packet_t *     p_data;
        ssize_t             i_result;

        i_result = PSRead( p_input, &p_data );

        if( i_result <= 0 )
        {
            return( i_result );
        }

        input_DemuxPS( p_input, p_data );
    }

    return( i );
}

