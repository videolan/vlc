/*****************************************************************************
 * input.c: input thread
 * Read an MPEG2 stream, demultiplex and parse it before sending it to
 * decoders.
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: input.c,v 1.213 2002/10/08 16:02:05 sam Exp $
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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vlc/vlc.h>

#include <string.h>
#include <errno.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "netutils.h"
#include "vlc_playlist.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "stream_output.h"

#include "interface.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static  int RunThread       ( input_thread_t *p_input );
static  int InitThread      ( input_thread_t *p_input );
static void ErrorThread     ( input_thread_t *p_input );
static void EndThread       ( input_thread_t *p_input );

/*****************************************************************************
 * input_CreateThread: creates a new input thread
 *****************************************************************************
 * This function creates a new input, and returns a pointer
 * to its description. On error, it returns NULL.
 * If pi_status is NULL, then the function will block until the thread is ready.
 * If not, it will be updated using one of the THREAD_* constants.
 *****************************************************************************/
input_thread_t *__input_CreateThread( vlc_object_t *p_parent,
                                      playlist_item_t *p_item, int *pi_status )
{
    input_thread_t *    p_input;                        /* thread descriptor */
    input_info_category_t * p_info;

    /* Allocate descriptor */
    p_input = vlc_object_create( p_parent, VLC_OBJECT_INPUT );
    if( p_input == NULL )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    /* Initialize thread properties */
    p_input->b_eof      = 0;

    /* Set target */
    p_input->psz_source = strdup( p_item->psz_name );

    /* Demux */
    p_input->p_demux = NULL;

    /* Access */
    p_input->p_access = NULL;
    
    p_input->i_bufsize = 0;
    p_input->i_mtu = 0;

    /* Initialize statistics */
    p_input->c_loops                    = 0;
    p_input->stream.c_packets_read      = 0;
    p_input->stream.c_packets_trashed   = 0;

    /* Set locks. */
    vlc_mutex_init( p_input, &p_input->stream.stream_lock );
    vlc_cond_init( p_input, &p_input->stream.stream_wait );
    vlc_mutex_init( p_input, &p_input->stream.control.control_lock );

    /* Initialize stream description */
    p_input->stream.b_changed = 0;
    p_input->stream.i_es_number = 0;
    p_input->stream.i_selected_es_number = 0;
    p_input->stream.i_pgrm_number = 0;
    p_input->stream.i_new_status = p_input->stream.i_new_rate = 0;
    p_input->stream.b_new_mute = MUTE_NO_CHANGE;
    p_input->stream.i_mux_rate = 0;
    p_input->stream.b_seekable = 0;
    p_input->stream.p_sout = NULL;

    /* no stream, no program, no area, no es */
    p_input->stream.p_new_program = NULL;

    p_input->stream.i_area_nb = 0;
    p_input->stream.pp_areas = NULL;
    p_input->stream.p_selected_area = NULL;
    p_input->stream.p_new_area = NULL;

    p_input->stream.pp_selected_es = NULL;
    p_input->stream.p_removed_es = NULL;
    p_input->stream.p_newly_selected_es = NULL;

    /* By default there is one area in a stream */
    input_AddArea( p_input );
    p_input->stream.p_selected_area = p_input->stream.pp_areas[0];

    /* Initialize stream control properties. */
    p_input->stream.control.i_status = PLAYING_S;
    p_input->stream.control.i_rate = DEFAULT_RATE;
    p_input->stream.control.b_mute = 0;
    p_input->stream.control.b_grayscale = config_GetInt( p_input, "grayscale" );

    /* Initialize input info */
    p_input->stream.p_info = malloc( sizeof( input_info_category_t ) );
    if( !p_input->stream.p_info )
    {
        msg_Err( p_input, "No memory!" );
        return NULL;
    }
    p_input->stream.p_info->psz_name = strdup("General") ;
    p_input->stream.p_info->p_info = NULL;
    p_input->stream.p_info->p_next = NULL;

    msg_Info( p_input, "playlist item `%s'", p_input->psz_source );

    p_info = input_InfoCategory( p_input, "General" );
    input_AddInfo( p_info, "playlist item", p_input->psz_source );
    vlc_object_attach( p_input, p_parent );

    /* Create thread and wait for its readiness. */
    if( vlc_thread_create( p_input, "input", RunThread,
                           VLC_THREAD_PRIORITY_INPUT, VLC_TRUE ) )
    {
        msg_Err( p_input, "cannot create input thread (%s)", strerror(errno) );
        free( p_input );
        return NULL;
    }

    return p_input;
}

/*****************************************************************************
 * input_StopThread: mark an input thread as zombie
 *****************************************************************************
 * This function should not return until the thread is effectively cancelled.
 *****************************************************************************/
void input_StopThread( input_thread_t *p_input )
{
    /* Make the thread exit from a possible vlc_cond_wait() */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    /* Request thread destruction */
    p_input->b_die = 1;

    vlc_cond_signal( &p_input->stream.stream_wait );
    vlc_mutex_unlock( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * input_DestroyThread: mark an input thread as zombie
 *****************************************************************************
 * This function should not return until the thread is effectively cancelled.
 *****************************************************************************/
void input_DestroyThread( input_thread_t *p_input )
{
    /* Join the thread */
    vlc_thread_join( p_input );

    /* Destroy Mutex locks */
    vlc_mutex_destroy( &p_input->stream.control.control_lock );
    vlc_cond_destroy( &p_input->stream.stream_wait );
    vlc_mutex_destroy( &p_input->stream.stream_lock );
}

/*****************************************************************************
 * RunThread: main thread loop
 *****************************************************************************
 * Thread in charge of processing the network packets and demultiplexing.
 *****************************************************************************/
static int RunThread( input_thread_t *p_input )
{
    /* Signal right now, otherwise we'll get stuck in a peek */
    vlc_thread_ready( p_input );

    if( InitThread( p_input ) )
    {
        /* If we failed, wait before we are killed, and exit */
        p_input->b_error = 1;
        ErrorThread( p_input );
        p_input->b_dead = 1;
        return 0;
    }

    /* initialization is complete */
    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.b_changed = 1;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    while( !p_input->b_die && !p_input->b_error && !p_input->b_eof )
    {
        int i, i_count;

        p_input->c_loops++;

        vlc_mutex_lock( &p_input->stream.stream_lock );

        if( p_input->stream.p_new_program )
        {
            if( p_input->pf_set_program != NULL )
            {

                /* Reinitialize buffer manager. */
                input_AccessReinit( p_input );
                
                p_input->pf_set_program( p_input, 
                                         p_input->stream.p_new_program );

                /* Escape all decoders for the stream discontinuity they
                 * will encounter. */
                input_EscapeDiscontinuity( p_input );

                for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
                {
                    pgrm_descriptor_t * p_pgrm
                                            = p_input->stream.pp_programs[i];

                    /* Reinitialize synchro. */
                    p_pgrm->i_synchro_state = SYNCHRO_REINIT;
                }
            }
            p_input->stream.p_new_program = NULL;
        }
        
        if( p_input->stream.p_new_area )
        {
            if( p_input->stream.b_seekable && p_input->pf_set_area != NULL )
            {
                input_AccessReinit( p_input );

                p_input->pf_set_area( p_input, p_input->stream.p_new_area );

                /* Escape all decoders for the stream discontinuity they
                 * will encounter. */
                input_EscapeDiscontinuity( p_input );

                for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
                {
                    pgrm_descriptor_t * p_pgrm
                                            = p_input->stream.pp_programs[i];

                    /* Reinitialize synchro. */
                    p_pgrm->i_synchro_state = SYNCHRO_REINIT;
                }
            }
            p_input->stream.p_new_area = NULL;
        }

        if( p_input->stream.p_selected_area->i_seek != NO_SEEK )
        {
            if( p_input->stream.b_seekable
                 && p_input->pf_seek != NULL )
            {
                off_t i_new_pos;

                /* Reinitialize buffer manager. */
                input_AccessReinit( p_input );

                i_new_pos = p_input->stream.p_selected_area->i_seek;
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                p_input->pf_seek( p_input, i_new_pos );
                vlc_mutex_lock( &p_input->stream.stream_lock );

                /* Escape all decoders for the stream discontinuity they
                 * will encounter. */
                input_EscapeDiscontinuity( p_input );

                for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
                {
                    pgrm_descriptor_t * p_pgrm
                                            = p_input->stream.pp_programs[i];

                    /* Reinitialize synchro. */
                    p_pgrm->i_synchro_state = SYNCHRO_REINIT;
                }
            }
            p_input->stream.p_selected_area->i_seek = NO_SEEK;
        }

        if( p_input->stream.p_removed_es )
        {
            input_UnselectES( p_input, p_input->stream.p_removed_es );
            p_input->stream.p_removed_es = NULL;
        }

        if( p_input->stream.p_newly_selected_es )
        {
            input_SelectES( p_input, p_input->stream.p_newly_selected_es );
            p_input->stream.p_newly_selected_es = NULL;
        }

        if( p_input->stream.b_new_mute != MUTE_NO_CHANGE )
        {
            if( p_input->stream.b_new_mute )
            {
                input_EscapeAudioDiscontinuity( p_input );
            }

            vlc_mutex_lock( &p_input->stream.control.control_lock );
            p_input->stream.control.b_mute = p_input->stream.b_new_mute;
            vlc_mutex_unlock( &p_input->stream.control.control_lock );

            p_input->stream.b_new_mute = MUTE_NO_CHANGE;
        }

        vlc_mutex_unlock( &p_input->stream.stream_lock );

        /* Read and demultiplex some data. */
        i_count = p_input->pf_demux( p_input );

        if( i_count == 0 && p_input->stream.b_seekable )
        {
            /* End of file - we do not set b_die because only the
             * playlist is allowed to do so. */
            msg_Info( p_input, "EOF reached" );
            p_input->b_eof = 1;
        }
        else if( i_count < 0 )
        {
            p_input->b_error = 1;
        }
    }

    if( p_input->b_error || p_input->b_eof )
    {
        ErrorThread( p_input );
    }

    EndThread( p_input );

    return 0;
}

/*****************************************************************************
 * InitThread: init the input Thread
 *****************************************************************************/
static int InitThread( input_thread_t * p_input )
{
    /* Parse source string. Syntax : [[<access>][/<demux>]:][<source>] */
    char * psz_parser = p_input->psz_source;

    /* Skip the plug-in names */
    while( *psz_parser && *psz_parser != ':' )
    {
        psz_parser++;
    }
#ifdef WIN32
    if( psz_parser - p_input->psz_source == 1 )
    {
        msg_Warn( p_input, "drive letter %c: found in source string",
                           p_input->psz_source ) ;
        psz_parser = "";
    }
#endif

    if( !*psz_parser )
    {
        p_input->psz_access = p_input->psz_demux = "";
        p_input->psz_name = p_input->psz_source;
    }
    else
    {
        *psz_parser++ = '\0';

        /* let's skip '//' */
        if( psz_parser[0] == '/' && psz_parser[1] == '/' )
        {
            psz_parser += 2 ;
        } 

        p_input->psz_name = psz_parser ;

        /* Come back to parse the access and demux plug-ins */
        psz_parser = p_input->psz_source;

        if( !*psz_parser )
        {
            /* No access */
            p_input->psz_access = "";
        }
        else if( *psz_parser == '/' )
        {
            /* No access */
            p_input->psz_access = "";
            psz_parser++;
        }
        else
        {
            p_input->psz_access = psz_parser;

            while( *psz_parser && *psz_parser != '/' )
            {
                psz_parser++;
            }

            if( *psz_parser == '/' )
            {
                *psz_parser++ = '\0';
            }
        }

        if( !*psz_parser )
        {
            /* No demux */
            p_input->psz_demux = "";
        }
        else
        {
            p_input->psz_demux = psz_parser;
        }
    }

    msg_Dbg( p_input, "access `%s', demux `%s', name `%s'",
             p_input->psz_access, p_input->psz_demux, p_input->psz_name );

    if( input_AccessInit( p_input ) == -1 )
    {
        return -1;
    }

    /* Find and open appropriate access module */
    p_input->p_access = module_Need( p_input, "access",
                                     p_input->psz_access );

    if( p_input->p_access == NULL )
    {
        msg_Err( p_input, "no suitable access module for `%s/%s://%s'",
                 p_input->psz_access, p_input->psz_demux, p_input->psz_name );
        return -1;
    }

    /* Waiting for stream. */
    if( p_input->i_mtu )
    {
        p_input->i_bufsize = p_input->i_mtu;
    }
    else
    {
        p_input->i_bufsize = INPUT_DEFAULT_BUFSIZE;
    }

    if( p_input->p_current_data == NULL && p_input->pf_read != NULL )
    {
        while( !input_FillBuffer( p_input ) )
        {
            if( p_input->b_die || p_input->b_error || p_input->b_eof )
            {
                module_Unneed( p_input, p_input->p_access );
                return -1;
            }
        }
    }

    /* Find and open appropriate demux module */
    p_input->p_demux = module_Need( p_input, "demux",
                                    p_input->psz_demux );

    if( p_input->p_demux== NULL )
    {
        msg_Err( p_input, "no suitable demux module for `%s/%s://%s'",
                 p_input->psz_access, p_input->psz_demux, p_input->psz_name );
        module_Unneed( p_input, p_input->p_access );
        return -1;
    }

    /* Initialize optional stream output. */
    psz_parser = config_GetPsz( p_input, "sout" );
    if ( psz_parser != NULL )
    {
        if ( *psz_parser &&
             (p_input->stream.p_sout = sout_NewInstance( p_input, psz_parser ))
               == NULL )
        {
            msg_Err( p_input, "cannot start stream output instance, aborting" );
            free( psz_parser );
            module_Unneed( p_input, p_input->p_access );
            module_Unneed( p_input, p_input->p_demux );
            return -1;
        }

        free( psz_parser );
    }

    return 0;
}

/*****************************************************************************
 * ErrorThread: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occured during thread main's loop.
 *****************************************************************************/
static void ErrorThread( input_thread_t *p_input )
{
    while( !p_input->b_die )
    {
        /* Sleep a while */
        msleep( INPUT_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * EndThread: end the input thread
 *****************************************************************************/
static void EndThread( input_thread_t * p_input )
{
#ifdef HAVE_SYS_TIMES_H
    /* Display statistics */
    struct tms  cpu_usage;
    times( &cpu_usage );

    msg_Dbg( p_input, "%d loops consuming user: %d, system: %d",
             p_input->c_loops, cpu_usage.tms_utime, cpu_usage.tms_stime );
#else
    msg_Dbg( p_input, "%d loops", p_input->c_loops );
#endif

    /* Free info structures */
    msg_Dbg( p_input, "freeing info structures...");
    input_DelInfo( p_input );
    
    input_DumpStream( p_input );

    /* Free all ES and destroy all decoder threads */
    input_EndStream( p_input );

    /* Close optional stream output instance */
    if ( p_input->stream.p_sout != NULL )
    {
        sout_DeleteInstance( p_input->stream.p_sout );
    }

    /* Free demultiplexer's data */
    module_Unneed( p_input, p_input->p_demux );

    /* Close the access plug-in */
    module_Unneed( p_input, p_input->p_access );

    input_AccessEnd( p_input );

    free( p_input->psz_source );

    /* Tell we're dead */
    p_input->b_dead = 1;
}

