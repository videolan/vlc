/*****************************************************************************
 * input.c: input thread
 * Read an MPEG2 stream, demultiplex and parse it before sending it to
 * decoders.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: input.c,v 1.191 2002/03/18 21:04:01 xav Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Alexis Guillard <alexis.guillard@bt.com>
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

#include <videolan/vlc.h>

#include <string.h>
#include <errno.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "netutils.h"

#include "intf_playlist.h"

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "input_ext-plugins.h"

#include "interface.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static  int RunThread       ( input_thread_t *p_input );
static  int InitThread      ( input_thread_t *p_input );
static void ErrorThread     ( input_thread_t *p_input );
static void CloseThread     ( input_thread_t *p_input );
static void DestroyThread   ( input_thread_t *p_input );
static void EndThread       ( input_thread_t *p_input );

/*****************************************************************************
 * input_InitBank: initialize the input bank.
 *****************************************************************************/
void input_InitBank ( void )
{
    p_input_bank->i_count = 0;

    /* XXX: Workaround for old interface modules */
    p_input_bank->pp_input[0] = NULL;

    vlc_mutex_init( &p_input_bank->lock );
}

/*****************************************************************************
 * input_EndBank: empty the input bank.
 *****************************************************************************
 * This function ends all unused inputs and empties the bank in
 * case of success.
 *****************************************************************************/
void input_EndBank ( void )
{
    int i_input;

    /* Ask all remaining video outputs to die */
    for( i_input = 0; i_input < p_input_bank->i_count; i_input++ )
    {
        input_StopThread(
                p_input_bank->pp_input[ i_input ], NULL );
        input_DestroyThread(
                p_input_bank->pp_input[ i_input ] );
    }

    vlc_mutex_destroy( &p_input_bank->lock );
}

/*****************************************************************************
 * input_CreateThread: creates a new input thread
 *****************************************************************************
 * This function creates a new input, and returns a pointer
 * to its description. On error, it returns NULL.
 * If pi_status is NULL, then the function will block until the thread is ready.
 * If not, it will be updated using one of the THREAD_* constants.
 *****************************************************************************/
input_thread_t *input_CreateThread ( playlist_item_t *p_item, int *pi_status )
{
    input_thread_t *    p_input;                        /* thread descriptor */

    /* Allocate descriptor */
    p_input = (input_thread_t *)malloc( sizeof(input_thread_t) );
    if( p_input == NULL )
    {
        intf_ErrMsg( "input error: can't allocate input thread (%s)",
                     strerror(errno) );
        return( NULL );
    }

    /* Initialize thread properties */
    p_input->b_die      = 0;
    p_input->b_error    = 0;
    p_input->b_eof      = 0;

    /* Set target */
    p_input->psz_source = strdup( p_item->psz_name );

    /* Set status */
    p_input->i_status   = THREAD_CREATE;

    /* Demux */
    p_input->p_demux_module = NULL;
    p_input->pf_init    = NULL;
    p_input->pf_end     = NULL;
    p_input->pf_demux   = NULL;
    p_input->pf_rewind  = NULL;

    /* Access */
    p_input->p_access_module = NULL;
    p_input->pf_open        = NULL;
    p_input->pf_close       = NULL;
    p_input->pf_read        = NULL;
    p_input->pf_seek        = NULL;
    p_input->pf_set_area    = NULL;
    p_input->pf_set_program = NULL;
    
    p_input->i_bufsize = 0;
    p_input->i_mtu = 0;

    /* Initialize statistics */
    p_input->c_loops                    = 0;
    p_input->stream.c_packets_read      = 0;
    p_input->stream.c_packets_trashed   = 0;

    /* Set locks. */
    vlc_mutex_init( &p_input->stream.stream_lock );
    vlc_cond_init( &p_input->stream.stream_wait );
    vlc_mutex_init( &p_input->stream.control.control_lock );

    /* Initialize stream description */
    p_input->stream.b_changed = 0;
    p_input->stream.i_es_number = 0;
    p_input->stream.i_selected_es_number = 0;
    p_input->stream.i_pgrm_number = 0;
    p_input->stream.i_new_status = p_input->stream.i_new_rate = 0;
    p_input->stream.b_new_mute = MUTE_NO_CHANGE;
    p_input->stream.i_mux_rate = 0;

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
    p_input->stream.control.b_grayscale = config_GetIntVariable( "grayscale" );
    p_input->stream.control.i_smp = config_GetIntVariable( "vdec_smp" );

    intf_WarnMsg( 1, "input: playlist item `%s'", p_input->psz_source );

    /* Create thread. */
    if( vlc_thread_create( &p_input->thread_id, "input",
                           (vlc_thread_func_t)RunThread, (void *) p_input ) )
    {
        intf_ErrMsg( "input error: can't create input thread (%s)",
                     strerror(errno) );
        free( p_input );
        return( NULL );
    }

#if 0
    /* If status is NULL, wait until the thread is created */
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        } while( (i_status != THREAD_READY) && (i_status != THREAD_ERROR)
                && (i_status != THREAD_FATAL) );
    }
#endif

    return( p_input );
}

/*****************************************************************************
 * input_StopThread: mark an input thread as zombie
 *****************************************************************************
 * This function should not return until the thread is effectively cancelled.
 *****************************************************************************/
void input_StopThread( input_thread_t *p_input, int *pi_status )
{
    /* Make the thread exit from a possible vlc_cond_wait() */
    vlc_mutex_lock( &p_input->stream.stream_lock );

    /* Request thread destruction */
    p_input->b_die = 1;

    vlc_cond_signal( &p_input->stream.stream_wait );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    /* If status is NULL, wait until thread has been destroyed */
#if 0
    if( pi_status == NULL )
    {
        do
        {
            msleep( THREAD_SLEEP );
        } while ( (i_status != THREAD_OVER) && (i_status != THREAD_ERROR)
                  && (i_status != THREAD_FATAL) );
    }
#endif
}

/*****************************************************************************
 * input_DestroyThread: mark an input thread as zombie
 *****************************************************************************
 * This function should not return until the thread is effectively cancelled.
 *****************************************************************************/
void input_DestroyThread( input_thread_t *p_input )
{
    /* Join the thread */
    vlc_thread_join( p_input->thread_id );

    /* Destroy Mutex locks */
    vlc_mutex_destroy( &p_input->stream.control.control_lock );
    vlc_cond_destroy( &p_input->stream.stream_wait );
    vlc_mutex_destroy( &p_input->stream.stream_lock );
    
    /* Free input structure */
    free( p_input );
}

/*****************************************************************************
 * RunThread: main thread loop
 *****************************************************************************
 * Thread in charge of processing the network packets and demultiplexing.
 *****************************************************************************/
static int RunThread( input_thread_t *p_input )
{
    if( InitThread( p_input ) )
    {
        /* If we failed, wait before we are killed, and exit */
        p_input->i_status = THREAD_ERROR;
        p_input->b_error = 1;
        ErrorThread( p_input );
        DestroyThread( p_input );
        return 0;
    }

    p_input->i_status = THREAD_READY;

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

                p_input->pf_set_program( p_input, 
                        p_input->stream.p_new_program );

                for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
                {
                    pgrm_descriptor_t * p_pgrm
                                            = p_input->stream.pp_programs[i];
                    /* Escape all decoders for the stream discontinuity they
                     * will encounter. */
                    input_EscapeDiscontinuity( p_input, p_pgrm );

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

                p_input->pf_set_area( p_input, p_input->stream.p_new_area );

                for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
                {
                    pgrm_descriptor_t * p_pgrm
                                            = p_input->stream.pp_programs[i];
                    /* Escape all decoders for the stream discontinuity they
                     * will encounter. */
                    input_EscapeDiscontinuity( p_input, p_pgrm );

                    /* Reinitialize synchro. */
                    p_pgrm->i_synchro_state = SYNCHRO_REINIT;
                }

                input_AccessReinit( p_input );
            }
            p_input->stream.p_new_area = NULL;
        }

        if( p_input->stream.p_selected_area->i_seek != NO_SEEK )
        {
            if( p_input->stream.b_seekable && p_input->pf_seek != NULL )
            {
                off_t i_new_pos = p_input->stream.p_selected_area->i_seek;
                vlc_mutex_unlock( &p_input->stream.stream_lock );
                p_input->pf_seek( p_input, i_new_pos );
                vlc_mutex_lock( &p_input->stream.stream_lock );

                for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
                {
                    pgrm_descriptor_t * p_pgrm
                                            = p_input->stream.pp_programs[i];
                    /* Escape all decoders for the stream discontinuity they
                     * will encounter. */
                    input_EscapeDiscontinuity( p_input, p_pgrm );

                    /* Reinitialize synchro. */
                    p_pgrm->i_synchro_state = SYNCHRO_REINIT;
                }

                /* Reinitialize buffer manager. */
                input_AccessReinit( p_input );
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
             * interface is allowed to do so. */
            intf_WarnMsg( 3, "input: EOF reached" );
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

    DestroyThread( p_input );

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
        intf_WarnMsg( 2, "Drive letter %c: specified in source string",
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

    intf_WarnMsg( 2, "input: access `%s', demux `%s', name `%s'",
                  p_input->psz_access, p_input->psz_demux,
                  p_input->psz_name );

    if( input_AccessInit( p_input ) == -1 )
    {
        return( -1 );
    }

    /* Find and open appropriate access plug-in. */
    p_input->p_access_module = module_Need( MODULE_CAPABILITY_ACCESS,
                                 p_input->psz_access,
                                 (void *)p_input );

    if( p_input->p_access_module == NULL )
    {
        intf_ErrMsg( "input error: no suitable access plug-in for `%s/%s:%s'",
                     p_input->psz_access, p_input->psz_demux,
                     p_input->psz_name );
        return( -1 );
    }

#define f p_input->p_access_module->p_functions->access.functions.access
    p_input->pf_open          = f.pf_open;
    p_input->pf_close         = f.pf_close;
    p_input->pf_read          = f.pf_read;
    p_input->pf_set_area      = f.pf_set_area;
    p_input->pf_set_program   = f.pf_set_program;
    p_input->pf_seek          = f.pf_seek;
#undef f

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
            if( p_input->b_die || p_input->b_error )
            {
                module_Unneed( p_input->p_access_module );
                return( -1 );
            }
        }
    }

    /* Find and open appropriate demux plug-in. */
    p_input->p_demux_module = module_Need( MODULE_CAPABILITY_DEMUX,
                                 p_input->psz_demux,
                                 (void *)p_input );

    if( p_input->p_demux_module == NULL )
    {
        intf_ErrMsg( "input error: no suitable demux plug-in for `%s/%s:%s'",
                     p_input->psz_access, p_input->psz_demux,
                     p_input->psz_name );
        module_Unneed( p_input->p_access_module );
        return( -1 );
    }

#define f p_input->p_demux_module->p_functions->demux.functions.demux
    p_input->pf_init          = f.pf_init;
    p_input->pf_end           = f.pf_end;
    p_input->pf_demux         = f.pf_demux;
    p_input->pf_rewind        = f.pf_rewind;
#undef f

    return( 0 );
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
    /* Store status */
    p_input->i_status = THREAD_END;

    if( p_main->b_stats )
    {
#ifdef HAVE_SYS_TIMES_H
        /* Display statistics */
        struct tms  cpu_usage;
        times( &cpu_usage );

        intf_StatMsg( "input stats: %d loops consuming user: %d, system: %d",
                      p_input->c_loops,
                      cpu_usage.tms_utime, cpu_usage.tms_stime );
#else
        intf_StatMsg( "input stats: %d loops", p_input->c_loops );
#endif

        input_DumpStream( p_input );
    }

    /* Free all ES and destroy all decoder threads */
    input_EndStream( p_input );

    /* Free demultiplexer's data */
    p_input->pf_end( p_input );
    module_Unneed( p_input->p_demux_module );

    /* Close the access plug-in */
    CloseThread( p_input );
}

/*****************************************************************************
 * CloseThread: close the target
 *****************************************************************************/
static void CloseThread( input_thread_t * p_input )
{
    p_input->pf_close( p_input );
    module_Unneed( p_input->p_access_module );

    input_AccessEnd( p_input );

    free( p_input->psz_source );
}

/*****************************************************************************
 * DestroyThread: destroy the input thread
 *****************************************************************************/
static void DestroyThread( input_thread_t * p_input )
{
    /* Update status */
    p_input->i_status = THREAD_OVER;
}

