/*****************************************************************************
 * input.c: input thread
 * Read an MPEG2 stream, demultiplex and parse it before sending it to
 * decoders.
 *****************************************************************************
 * Copyright (C) 1998-2002 VideoLAN
 * $Id: input.c,v 1.239 2003/09/12 18:34:45 fenrir Exp $
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

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/vout.h>

#ifdef HAVE_SYS_TIMES_H
#   include <sys/times.h>
#endif

#include "vlc_playlist.h"

#include "stream_output.h"

#include "vlc_interface.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static  int RunThread       ( input_thread_t *p_input );
static  int InitThread      ( input_thread_t *p_input );
static void ErrorThread     ( input_thread_t *p_input );
static void EndThread       ( input_thread_t *p_input );

static void ParseOption     ( input_thread_t *p_input,
                              const char *psz_option );

static es_out_t *EsOutCreate ( input_thread_t * );
static void      EsOutRelease( es_out_t * );

/*****************************************************************************
 * Callbacks
 *****************************************************************************/
static int PositionCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *p_data );
static int TimeCallback    ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *p_data );
static int StateCallback   ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *p_data );
static int RateCallback    ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *p_data );

/*****************************************************************************
 * input_CreateThread: creates a new input thread
 *****************************************************************************
 * This function creates a new input, and returns a pointer
 * to its description. On error, it returns NULL.
 *****************************************************************************/
input_thread_t *__input_CreateThread( vlc_object_t *p_parent,
                                      playlist_item_t *p_item )
{
    input_thread_t *    p_input;                        /* thread descriptor */
    input_info_category_t * p_info;
    vlc_value_t val;
    int i;

    /* Allocate descriptor */
    p_input = vlc_object_create( p_parent, VLC_OBJECT_INPUT );
    if( p_input == NULL )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    /* Parse input options */
    for( i = 0; i < p_item->i_options; i++ )
    {
        ParseOption( p_input, p_item->ppsz_options[i] );
    }

    /* Create a few object variables we'll need later on */
    var_Create( p_input, "video", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_input, "audio", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_input, "audio-channel", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );
    var_Create( p_input, "spu-channel", VLC_VAR_INTEGER|VLC_VAR_DOINHERIT );

    var_Create( p_input, "sout", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_input, "sout-audio", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_input, "sout-video", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_input, "sout-keep",  VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* play status */

    /* position variable */
    var_Create( p_input, "position",  VLC_VAR_FLOAT );  /* position 0.0 -> 1.0 */
    val.f_float = 0.0;
    var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );
    var_AddCallback( p_input, "position", PositionCallback, NULL );

    /* time variable */
    var_Create( p_input, "time",  VLC_VAR_TIME );
    val.i_time = 0;
    var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );
    var_AddCallback( p_input, "time", TimeCallback, NULL );

    /* length variable */
    var_Create( p_input, "length",  VLC_VAR_TIME );
    val.i_time = 0;
    var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );

    /* rate variable */
    var_Create( p_input, "rate", VLC_VAR_INTEGER );
    var_Create( p_input, "rate-slower", VLC_VAR_VOID );
    var_Create( p_input, "rate-faster", VLC_VAR_VOID );
    val.i_int = DEFAULT_RATE;
    var_Change( p_input, "rate", VLC_VAR_SETVALUE, &val, NULL );
    var_AddCallback( p_input, "rate", RateCallback, NULL );
    var_AddCallback( p_input, "rate-slower", RateCallback, NULL );
    var_AddCallback( p_input, "rate-faster", RateCallback, NULL );

    /* state variable */
    var_Create( p_input, "state", VLC_VAR_INTEGER );
    val.i_int = INIT_S;
    var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );
    var_AddCallback( p_input, "state", StateCallback, NULL );


    /* Initialize thread properties */
    p_input->b_eof      = 0;

    /* Set target */
    p_input->psz_source = strdup( p_item->psz_uri );

    /* Stream */
    p_input->s = NULL;

    /* es out */
    p_input->p_es_out = NULL;

    /* Demux */
    p_input->p_demux   = NULL;
    p_input->pf_demux  = NULL;
    p_input->pf_rewind = NULL;
    p_input->pf_demux_control = NULL;

    /* Access */
    p_input->p_access = NULL;

    p_input->i_bufsize = 0;
    p_input->i_mtu = 0;
    p_input->i_pts_delay = DEFAULT_PTS_DELAY;

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
    input_AddArea( p_input, 0, 1 );
    p_input->stream.p_selected_area = p_input->stream.pp_areas[0];

    /* Initialize stream control properties. */
    p_input->stream.control.i_status = INIT_S;
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

    p_info = input_InfoCategory( p_input, _("General") );
    input_AddInfo( p_info, _("Playlist Item"), p_input->psz_source );
    vlc_object_attach( p_input, p_parent );

    /* Create thread and wait for its readiness. */
    if( vlc_thread_create( p_input, "input", RunThread,
                           VLC_THREAD_PRIORITY_INPUT, VLC_TRUE ) )
    {
        msg_Err( p_input, "cannot create input thread" );
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
    vlc_value_t  val;
    mtime_t      i_update_next = -1;

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
    p_input->stream.b_changed        = 1;
    p_input->stream.control.i_status = PLAYING_S;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    val.i_int = PLAYING_S;
    var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );

    while( !p_input->b_die && !p_input->b_error && !p_input->b_eof )
    {
        unsigned int i, i_count;

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
            if( p_input->stream.p_selected_area->i_size > 0 )
            {
                unsigned int i;
                double f = (double)p_input->stream.p_selected_area->i_seek /
                           (double)p_input->stream.p_selected_area->i_size;

                vlc_mutex_unlock( &p_input->stream.stream_lock );
                demux_Control( p_input, DEMUX_SET_POSITION, f );
                vlc_mutex_lock( &p_input->stream.stream_lock );

                /* Escape all decoders for the stream discontinuity they
                 * will encounter. */
                input_EscapeDiscontinuity( p_input );

                for( i = 0; i < p_input->stream.i_pgrm_number; i++ )
                {
                    pgrm_descriptor_t * p_pgrm=p_input->stream.pp_programs[i];

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

        if( i_count == 0 )
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

        if( !p_input->b_error && !p_input->b_eof && i_update_next < mdate() )
        {
            double d;

            /* update input status variables */
            if( !demux_Control( p_input, DEMUX_GET_POSITION, &d ) )
            {
                val.f_float = (float)d;
                var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );
            }
            if( !demux_Control( p_input, DEMUX_GET_TIME, &val.i_time ) )
            {
                var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );
            }
            if( !demux_Control( p_input, DEMUX_GET_LENGTH, &val.i_time ) )
            {
                var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );
            }

            i_update_next = mdate() + 200000LL;
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
    char * psz_parser = p_input->psz_dupsource = strdup(p_input->psz_source);
    vlc_value_t val;

    /* Skip the plug-in names */
    while( *psz_parser && *psz_parser != ':' )
    {
        psz_parser++;
    }
#if defined( WIN32 ) || defined( UNDER_CE )
    if( psz_parser - p_input->psz_dupsource == 1 )
    {
        msg_Warn( p_input, "drive letter %c: found in source string",
                           p_input->psz_dupsource[0] ) ;
        psz_parser = "";
    }
#endif

    if( !*psz_parser )
    {
        p_input->psz_access = p_input->psz_demux = "";
        p_input->psz_name = p_input->psz_source;
        free( p_input->psz_dupsource );
        p_input->psz_dupsource = NULL;
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
        psz_parser = p_input->psz_dupsource;

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
        return VLC_EGENERIC;
    }

    /* Initialize optional stream output. (before demuxer)*/
    var_Get( p_input, "sout", &val );
    if( val.psz_string != NULL )
    {
        if ( *val.psz_string && (p_input->stream.p_sout =
             sout_NewInstance( p_input, val.psz_string )) == NULL )
        {
            msg_Err( p_input, "cannot start stream output instance, aborting" );
            free( val.psz_string );
            return VLC_EGENERIC;
        }
        free( val.psz_string );
    }

    p_input->p_es_out = EsOutCreate( p_input );

    /* Find and open appropriate access module */
    p_input->p_access = module_Need( p_input, "access",
                                     p_input->psz_access );

    if ( p_input->p_access == NULL
          && (*p_input->psz_demux || *p_input->psz_access) )
    {
        /* Maybe we got something like :
         * /Volumes/toto:titi/gabu.mpg */
        p_input->psz_access = p_input->psz_demux = "";
        p_input->psz_name = p_input->psz_source;
        free( p_input->psz_dupsource);
        p_input->psz_dupsource = NULL;

        p_input->p_access = module_Need( p_input, "access",
                                         p_input->psz_access );
    }

    if( p_input->p_access == NULL )
    {
        msg_Err( p_input, "no suitable access module for `%s/%s://%s'",
                 p_input->psz_access, p_input->psz_demux, p_input->psz_name );
        if ( p_input->stream.p_sout != NULL )
        {
            sout_DeleteInstance( p_input->stream.p_sout );
        }
        return VLC_EGENERIC;
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

    /* If the desynchronisation requested by the user is < 0, we need to
     * cache more data. */
    if( p_input->p_vlc->i_desync < 0 )
        p_input->i_pts_delay -= p_input->p_vlc->i_desync;

    if( p_input->p_current_data == NULL && p_input->pf_read != NULL )
    {
        while( !input_FillBuffer( p_input ) )
        {
            if( p_input->b_die || p_input->b_error || p_input->b_eof )
            {
                module_Unneed( p_input, p_input->p_access );
                if ( p_input->stream.p_sout != NULL )
                {
                    sout_DeleteInstance( p_input->stream.p_sout );
                }
                return VLC_EGENERIC;
            }
        }
    }

    /* Create the stream_t facilities */
    p_input->s = stream_OpenInput( p_input );
    if( p_input->s == NULL )
    {
        /* should nver occur yet */

        msg_Err( p_input, "cannot create stream_t !" );
        module_Unneed( p_input, p_input->p_access );
        if ( p_input->stream.p_sout != NULL )
        {
            sout_DeleteInstance( p_input->stream.p_sout );
        }
        return VLC_EGENERIC;
    }

    /* Find and open appropriate demux module */
    p_input->p_demux = module_Need( p_input, "demux",
                                    p_input->psz_demux );

    if( p_input->p_demux == NULL )
    {
        msg_Err( p_input, "no suitable demux module for `%s/%s://%s'",
                 p_input->psz_access, p_input->psz_demux, p_input->psz_name );
        stream_Release( p_input->s );
        module_Unneed( p_input, p_input->p_access );
        if ( p_input->stream.p_sout != NULL )
        {
            sout_DeleteInstance( p_input->stream.p_sout );
        }
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
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

    msg_Dbg( p_input, "%ld loops consuming user: %ld, system: %ld",
             p_input->c_loops, cpu_usage.tms_utime, cpu_usage.tms_stime );
#else
    msg_Dbg( p_input, "%ld loops", p_input->c_loops );
#endif

    input_DumpStream( p_input );

    /* Free all ES and destroy all decoder threads */
    input_EndStream( p_input );

    /* Close optional stream output instance */
    if ( p_input->stream.p_sout != NULL )
    {
        vlc_object_t *p_pl = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        vlc_value_t keep;

        if( var_Get( p_input, "sout-keep", &keep ) >= 0 && keep.b_bool && p_pl )
        {
            /* attach sout to the playlist */
            msg_Warn( p_input, "keeping sout" );
            vlc_object_detach( p_input->stream.p_sout );
            vlc_object_attach( p_input->stream.p_sout, p_pl );
        }
        else
        {
            msg_Warn( p_input, "destroying sout" );
            sout_DeleteInstance( p_input->stream.p_sout );
        }
        if( p_pl )
        {
            vlc_object_release( p_pl );
        }
    }

    /* Free demultiplexer's data */
    module_Unneed( p_input, p_input->p_demux );

    /* Destroy the stream_t facilities */
    stream_Release( p_input->s );

    /* Destroy es out */
    EsOutRelease( p_input->p_es_out );

    /* Close the access plug-in */
    module_Unneed( p_input, p_input->p_access );

    input_AccessEnd( p_input );

    /* Free info structures XXX destroy es before 'cause vorbis */
    msg_Dbg( p_input, "freeing info structures...");
    input_DelInfo( p_input );

    free( p_input->psz_source );
    if ( p_input->psz_dupsource != NULL ) free( p_input->psz_dupsource );

    /* Tell we're dead */
    p_input->b_dead = 1;
}

/*****************************************************************************
 * ParseOption: parses the options for the input
 *****************************************************************************
 * This function parses the input (config) options and creates their associated
 * object variables.
 * Options are of the form "[no[-]]foo[=bar]" where foo is the option name and
 * bar is the value of the option.
 *****************************************************************************/
static void ParseOption( input_thread_t *p_input, const char *psz_option )
{
    char *psz_name = (char *)psz_option;
    char *psz_value = strchr( psz_option, '=' );
    int  i_name_len, i_type;
    vlc_bool_t b_isno = VLC_FALSE;
    vlc_value_t val;

    if( psz_value ) i_name_len = psz_value - psz_option;
    else i_name_len = strlen( psz_option );

    /* It's too much of an hassle to remove the ':' when we parse
     * the cmd line :) */
    if( i_name_len && *psz_name == ':' )
    {
        psz_name++;
        i_name_len--;
    }

    if( i_name_len == 0 ) return;

    psz_name = strndup( psz_name, i_name_len );
    if( psz_value ) psz_value++;

    i_type = config_GetType( p_input, psz_name );

    if( !i_type && !psz_value )
    {
        /* check for "no-foo" or "nofoo" */
        if( !strncmp( psz_name, "no-", 3 ) )
        {
            memmove( psz_name, psz_name + 3, strlen(psz_name) + 1 - 3 );
        }
        else if( !strncmp( psz_name, "no", 2 ) )
        {
            memmove( psz_name, psz_name + 2, strlen(psz_name) + 1 - 2 );
        }
        else goto cleanup;           /* Option doesn't exist */

        b_isno = VLC_TRUE;
        i_type = config_GetType( p_input, psz_name );

        if( !i_type ) goto cleanup;  /* Option doesn't exist */
    }
    else if( !i_type ) goto cleanup; /* Option doesn't exist */

    if( ( i_type != VLC_VAR_BOOL ) &&
        ( !psz_value || !*psz_value ) ) goto cleanup; /* Invalid value */

    /* Create the variable in the input object.
     * Children of the input object will be able to retreive this value
     * thanks to the inheritance property of the object variables. */
    var_Create( p_input, psz_name, i_type );

    switch( i_type )
    {
    case VLC_VAR_BOOL:
        val.b_bool = !b_isno;
        break;

    case VLC_VAR_INTEGER:
        val.i_int = atoi( psz_value );
        break;

    case VLC_VAR_FLOAT:
        val.f_float = atof( psz_value );
        break;

    case VLC_VAR_STRING:
    case VLC_VAR_FILE:
    case VLC_VAR_DIRECTORY:
        val.psz_string = psz_value;
        break;

    default:
        goto cleanup;
        break;
    }

    var_Set( p_input, psz_name, val );

    msg_Dbg( p_input, "set input option: %s to %s", psz_name, psz_value );

  cleanup:
    if( psz_name ) free( psz_name );
    return;
}

/*****************************************************************************
 * es_out_t input handler
 *****************************************************************************/
struct es_out_sys_t
{
    input_thread_t *p_input;

    int         i_id;
    es_out_id_t **id;
};
struct es_out_id_t
{
    es_descriptor_t *p_es;
};

static es_out_id_t *EsOutAdd    ( es_out_t *, es_format_t * );
static int          EsOutSend   ( es_out_t *, es_out_id_t *, pes_packet_t * );
static void         EsOutDel    ( es_out_t *, es_out_id_t * );
static int          EsOutControl( es_out_t *, int i_query, va_list );

static es_out_t *EsOutCreate( input_thread_t *p_input )
{
    es_out_t *out = malloc( sizeof( es_out_t ) );

    out->pf_add     = EsOutAdd;
    out->pf_send    = EsOutSend;
    out->pf_del     = EsOutDel;
    out->pf_control = EsOutControl;

    out->p_sys = malloc( sizeof( es_out_sys_t ) );
    out->p_sys->p_input = p_input;
    out->p_sys->i_id    = 0;
    out->p_sys->id      = NULL;

    return out;
}
static void      EsOutRelease( es_out_t *out )
{
    free( out->p_sys );
    free( out );
}

static es_out_id_t *EsOutAdd( es_out_t *out, es_format_t *fmt )
{
    es_out_id_t *id = malloc( sizeof( es_out_id_t ) );

    return id;
}
static int EsOutSend( es_out_t *out, es_out_id_t *id, pes_packet_t *p_pes )
{
    return VLC_SUCCESS;
}
static void EsOutDel( es_out_t *out, es_out_id_t *id )
{
    free( id );
}
static int EsOutControl( es_out_t *out, int i_query, va_list args )
{
    return VLC_EGENERIC;
}

/*****************************************************************************
 * Callbacks  (position, time, state, rate )
 *****************************************************************************/
static int PositionCallback( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;

    msg_Warn( p_input, "cmd=%s old=%f new=%f",
              psz_cmd,
              oldval.f_float, newval.f_float );

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_seek =
        (int64_t)( newval.f_float * (double)p_input->stream.p_selected_area->i_size );
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return VLC_SUCCESS;
}

static int TimeCallback    ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;

    /* FIXME TODO FIXME */
    msg_Warn( p_input, "cmd=%s old=%lld new=%lld",
              psz_cmd,
              oldval.i_time, newval.i_time );

    vlc_mutex_lock( &p_input->stream.stream_lock );
    p_input->stream.p_selected_area->i_seek =
        newval.i_time / 1000000 * 50 * p_input->stream.i_mux_rate;
    vlc_mutex_unlock( &p_input->stream.stream_lock );

    return VLC_SUCCESS;
}

static int StateCallback   ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    msg_Warn( p_input, "cmd=%s old=%d new=%d",
              psz_cmd, oldval.i_int, newval.i_int );

    switch( newval.i_int )
    {
        case PLAYING_S:
            input_SetStatus( p_input, INPUT_STATUS_PLAY );
            return VLC_SUCCESS;
        case PAUSE_S:
            input_SetStatus( p_input, INPUT_STATUS_PAUSE );
            return VLC_SUCCESS;
        case END_S:
            input_SetStatus( p_input, INPUT_STATUS_END );
            return VLC_SUCCESS;
        default:
            msg_Err( p_input, "cannot set new state (invalid)" );
            return VLC_EGENERIC;
    }
}

static int RateCallback    ( vlc_object_t *p_this, char const *psz_cmd,
                             vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;

    if( !strcmp( psz_cmd, "rate-slower" ) )
    {
        input_SetStatus( p_input, INPUT_STATUS_SLOWER );
    }
    else if( !strcmp( psz_cmd, "rate-faster" ) )
    {
        input_SetStatus( p_input, INPUT_STATUS_FASTER );
    }
    else
    {
        msg_Warn( p_input, "cmd=%s old=%d new=%d",
                  psz_cmd, oldval.i_int, newval.i_int );
        input_SetRate( p_input, newval.i_int );
    }
    return VLC_SUCCESS;
}

