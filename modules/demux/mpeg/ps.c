/*****************************************************************************
 * ps.c : Program Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2000-2001 VideoLAN
 * $Id: ps.c,v 1.2 2002/08/07 00:29:36 sam Exp $
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

#include "system.h"

/*****************************************************************************
 * Constants
 *****************************************************************************/
#define PS_READ_ONCE 50

/*****************************************************************************
 * Private structure
 *****************************************************************************/
struct demux_sys_t
{
    module_t *   p_module;
    mpeg_demux_t mpeg;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Activate   ( vlc_object_t * );
static void Deactivate ( vlc_object_t * );
static int  Demux      ( input_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("ISO 13818-1 MPEG Program Stream input") );
    set_capability( "demux", 100 );
    set_callbacks( Activate, Deactivate );
    add_shortcut( "ps" );
vlc_module_end();

/*****************************************************************************
 * Activate: initialize PS structures
 *****************************************************************************/
static int Activate( vlc_object_t * p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;
    demux_sys_t *       p_demux;
    byte_t *            p_peek;

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
        return -1;
    }

    if( *p_peek || *(p_peek + 1) || *(p_peek + 2) != 1 )
    {
        if( *p_input->psz_demux && !strncmp( p_input->psz_demux, "ps", 3 ) )
        {
            /* User forced */
            msg_Err( p_input, "this does not look like an MPEG PS stream, continuing" );
        }
        else
        {
            msg_Warn( p_input, "this does not look like an MPEG PS stream, "
                               "but continuing anyway" );
        }
    }
    else if( *(p_peek + 3) <= 0xb9 )
    {
        if( *p_input->psz_demux && !strncmp( p_input->psz_demux, "ps", 3 ) )
        {
            /* User forced */
            msg_Err( p_input, "this seems to be an elementary stream (ES module?), but continuing" );
        }
        else
        {
            msg_Warn( p_input, "this seems to be an elementary stream (ES module?), but continuing" );
        }
    }

    p_demux = p_input->p_demux_data = malloc( sizeof(demux_sys_t ) );
    if( p_demux == NULL )
    {
        return -1;
    }

    p_input->p_private = (void*)&p_demux->mpeg;
    p_demux->p_module = module_Need( p_input, "mpeg-system", NULL );
    if( p_demux->p_module == NULL )
    {
        free( p_input->p_demux_data );
        return -1;
    }

    if( input_InitStream( p_input, sizeof( stream_ps_data_t ) ) == -1 )
    {
        module_Unneed( p_input, p_demux->p_module );
        free( p_input->p_demux_data );
        return -1;
    }
    input_AddProgram( p_input, 0, sizeof( stream_ps_data_t ) );
    
    p_input->stream.p_selected_program = 
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

            i_result = p_demux->mpeg.pf_read_ps( p_input, &p_data );

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

            p_demux->mpeg.pf_parse_ps( p_input, p_data );
            input_DeletePacket( p_input->p_method_data, p_data );

            /* File too big. */
            if( p_input->stream.p_selected_area->i_tell >
                                                    INPUT_PREPARSE_LENGTH )
            {
                break;
            }
        }
        input_AccessReinit( p_input );
        p_input->pf_seek( p_input, (off_t)0 );
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
                switch( p_es->i_fourcc )
                {
                    case VLC_FOURCC('m','p','g','v'):
                        input_SelectES( p_input, p_es );
                        break;

                    case VLC_FOURCC('m','p','g','a'):
                        if( config_GetInt( p_input, "audio-channel" )
                                == (p_es->i_id & 0x1F) ||
                           ( config_GetInt( p_input, "audio-channel" ) < 0
                              && !(p_es->i_id & 0x1F) ) )
                        switch( config_GetInt( p_input, "audio-type" ) )
                        {
                        case -1:
                        case REQUESTED_MPEG:
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case VLC_FOURCC('a','5','2',' '):
                        if( config_GetInt( p_input, "audio-channel" )
                                == ((p_es->i_id & 0xF00) >> 8) ||
                           ( config_GetInt( p_input, "audio-channel" ) < 0
                              && !((p_es->i_id & 0xF00) >> 8) ) )
                        switch( config_GetInt( p_input, "audio-type" ) )
                        {
                        case -1:
                        case REQUESTED_A52:
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case VLC_FOURCC('s','p','u',' '):
                        if( config_GetInt( p_input, "spu-channel" )
                                == ((p_es->i_id & 0x1F00) >> 8) )
                        {
                            input_SelectES( p_input, p_es );
                        }
                        break;

                    case VLC_FOURCC('l','p','c','m'):
                        if( config_GetInt( p_input, "audio-channel" )
                                == ((p_es->i_id & 0x1F00) >> 8) ||
                           ( config_GetInt( p_input, "audio-channel" ) < 0
                              && !((p_es->i_id & 0x1F00) >> 8) ) )
                        switch( config_GetInt( p_input, "audio-type" ) )
                        {
                        case -1:
                        case REQUESTED_LPCM:
                            input_SelectES( p_input, p_es );
                        }
                        break;
                }
#undef p_es
            }
        }
#endif
        input_DumpStream( p_input );
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
    else
    {
        /* The programs will be added when we read them. */
        vlc_mutex_lock( &p_input->stream.stream_lock );
        p_input->stream.pp_programs[0]->b_is_ok = 0;
        vlc_mutex_unlock( &p_input->stream.stream_lock );
    }

    return 0;
}

/*****************************************************************************
 * Deactivate: deinitialize PS structures
 *****************************************************************************/
static void Deactivate( vlc_object_t * p_this )
{
    input_thread_t *    p_input = (input_thread_t *)p_this;

    module_Unneed( p_input, p_input->p_demux_data->p_module );
    free( p_input->p_demux_data );
}

/*****************************************************************************
 * Demux: reads and demuxes data packets
 *****************************************************************************
 * Returns -1 in case of error, 0 in case of EOF, otherwise the number of
 * packets.
 *****************************************************************************/
static int Demux( input_thread_t * p_input )
{
    int i;

    for( i = 0; i < PS_READ_ONCE; i++ )
    {
        data_packet_t *     p_data;
        ssize_t             i_result;
        i_result = p_input->p_demux_data->mpeg.pf_read_ps( p_input, &p_data );

        if( i_result <= 0 )
        {
            return i_result;
        }

        p_input->p_demux_data->mpeg.pf_demux_ps( p_input, p_data );
    }

    return i;
}

