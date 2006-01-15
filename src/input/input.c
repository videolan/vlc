/*****************************************************************************
 * input.c: input thread
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <ctype.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/decoder.h>
#include <vlc/vout.h>

#include "input_internal.h"

#include "stream_output.h"
#include "vlc_playlist.h"
#include "vlc_interface.h"
#include "vlc_interaction.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static  int Run  ( input_thread_t *p_input );
static  int RunAndClean  ( input_thread_t *p_input );

static input_thread_t * Create  ( vlc_object_t *, input_item_t *, vlc_bool_t );
static  int             Init    ( input_thread_t *p_input, vlc_bool_t b_quick );
static void             Error   ( input_thread_t *p_input );
static void             End     ( input_thread_t *p_input );
static void             MainLoop( input_thread_t *p_input );

static inline int ControlPopNoLock( input_thread_t *, int *, vlc_value_t * );
static void       ControlReduce( input_thread_t * );
static vlc_bool_t Control( input_thread_t *, int, vlc_value_t );


static int  UpdateFromAccess( input_thread_t * );
static int  UpdateFromDemux( input_thread_t * );
static int  UpdateMeta( input_thread_t *, vlc_bool_t );

static void UpdateItemLength( input_thread_t *, int64_t i_length, vlc_bool_t );

static void ParseOption( input_thread_t *p_input, const char *psz_option );

static void DecodeUrl( char * );
static void MRLSections( input_thread_t *, char *, int *, int *, int *, int *);

static input_source_t *InputSourceNew( input_thread_t *);
static int  InputSourceInit( input_thread_t *, input_source_t *,
                             char *, char *psz_forced_demux,
                             vlc_bool_t b_quick );
static void InputSourceClean( input_thread_t *, input_source_t * );

static void SlaveDemux( input_thread_t *p_input );
static void SlaveSeek( input_thread_t *p_input );

static vlc_meta_t *InputMetaUser( input_thread_t *p_input );

/*****************************************************************************
 * This function creates a new input, and returns a pointer
 * to its description. On error, it returns NULL.
 *
 * Variables for _public_ use:
 * * Get and Set:
 *  - state
 *  - rate,rate-slower, rate-faster
 *  - position, position-offset
 *  - time, time-offset
 *  - title,title-next,title-prev
 *  - chapter,chapter-next, chapter-prev
 *  - program, audio-es, video-es, spu-es
 *  - audio-delay, spu-delay
 *  - bookmark
 * * Get only:
 *  - length
 *  - bookmarks
 *  - seekable (if you can seek, it doesn't say if 'bar display' has be shown or not, for that check position != 0.0)
 * * For intf callback upon changes
 *  - intf-change
 * TODO explain when Callback is called
 * TODO complete this list (?)
 *****************************************************************************/
static input_thread_t *Create( vlc_object_t *p_parent, input_item_t *p_item,
                               vlc_bool_t b_quick )
{
    input_thread_t *p_input;                        /* thread descriptor */
    vlc_value_t val;
    int i;

    /* Allocate descriptor */
    p_input = vlc_object_create( p_parent, VLC_OBJECT_INPUT );
    if( p_input == NULL )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    /* Init Common fields */
    p_input->b_eof = VLC_FALSE;
    p_input->b_can_pace_control = VLC_TRUE;
    p_input->i_start = 0;
    p_input->i_time  = 0;
    p_input->i_stop  = 0;
    p_input->i_title = 0;
    p_input->title   = NULL;
    p_input->i_title_offset = p_input->i_seekpoint_offset = 0;
    p_input->i_state = INIT_S;
    p_input->i_rate  = INPUT_RATE_DEFAULT;
    p_input->i_bookmark = 0;
    p_input->bookmark = NULL;
    p_input->p_meta  = NULL;
    p_input->p_es_out = NULL;
    p_input->p_sout  = NULL;
    p_input->b_out_pace_control = VLC_FALSE;
    p_input->i_pts_delay = 0;

    /* Init Input fields */
    p_input->input.p_item = p_item;
    p_input->input.p_access = NULL;
    p_input->input.p_stream = NULL;
    p_input->input.p_demux  = NULL;
    p_input->input.b_title_demux = VLC_FALSE;
    p_input->input.i_title  = 0;
    p_input->input.title    = NULL;
    p_input->input.i_title_offset = p_input->input.i_seekpoint_offset = 0;
    p_input->input.b_can_pace_control = VLC_TRUE;
    p_input->input.b_eof = VLC_FALSE;
    p_input->input.i_cr_average = 0;

    stats_ReinitInputStats( p_item->p_stats );

    /* No slave */
    p_input->i_slave = 0;
    p_input->slave   = NULL;

    /* Init control buffer */
    vlc_mutex_init( p_input, &p_input->lock_control );
    p_input->i_control = 0;

    /* Parse input options */
    vlc_mutex_lock( &p_item->lock );
    for( i = 0; i < p_item->i_options; i++ )
    {
        ParseOption( p_input, p_item->ppsz_options[i] );
    }
    vlc_mutex_unlock( &p_item->lock );

    /* Create Object Variables for private use only */
    input_ConfigVarInit( p_input );

    /* Create Objects variables for public Get and Set */
    input_ControlVarInit( p_input );
    p_input->input.i_cr_average = var_GetInteger( p_input, "cr-average" );

    if( !b_quick )
    {
        var_Get( p_input, "bookmarks", &val );
        if( val.psz_string )
        {
            /* FIXME: have a common cfg parsing routine used by sout and others */
            char *psz_parser, *psz_start, *psz_end;
            psz_parser = val.psz_string;
            while( (psz_start = strchr( psz_parser, '{' ) ) )
            {
                 seekpoint_t *p_seekpoint = vlc_seekpoint_New();
                 char backup;
                 psz_start++;
                 psz_end = strchr( psz_start, '}' );
                 if( !psz_end ) break;
                 psz_parser = psz_end + 1;
                 backup = *psz_parser;
                 *psz_parser = 0;
                 *psz_end = ',';
                 while( (psz_end = strchr( psz_start, ',' ) ) )
                 {
                     *psz_end = 0;
                     if( !strncmp( psz_start, "name=", 5 ) )
                     {
                         p_seekpoint->psz_name = psz_start + 5;
                     }
                     else if( !strncmp( psz_start, "bytes=", 6 ) )
                     {
                         p_seekpoint->i_byte_offset = atoll(psz_start + 6);
                     }
                     else if( !strncmp( psz_start, "time=", 5 ) )
                     {
                         p_seekpoint->i_time_offset = atoll(psz_start + 5) * 1000000;
                     }
                     psz_start = psz_end + 1;
                }
                msg_Dbg( p_input, "adding bookmark: %s, bytes="I64Fd", time="I64Fd,
                                  p_seekpoint->psz_name, p_seekpoint->i_byte_offset,
                                  p_seekpoint->i_time_offset );
                input_Control( p_input, INPUT_ADD_BOOKMARK, p_seekpoint );
                vlc_seekpoint_Delete( p_seekpoint );
                *psz_parser = backup;
            }
            free( val.psz_string );
        }
    }

    /* Remove 'Now playing' info as it is probably outdated */
    input_Control( p_input, INPUT_DEL_INFO, _("Meta-information"),
                    VLC_META_NOW_PLAYING );

    return p_input;
}

/**
 * Initialize an input thread and run it. You will need to monitor the thread to clean
 * up after it is done
 *
 * \param p_parent a vlc_object
 * \param p_item an input item
 * \return a pointer to the spawned input thread
 */
input_thread_t *__input_CreateThread( vlc_object_t *p_parent,
                                      input_item_t *p_item )

{
    input_thread_t *p_input;                        /* thread descriptor */

    p_input = Create( p_parent, p_item, VLC_FALSE );
    /* Now we can attach our new input */
    vlc_object_attach( p_input, p_parent );

    /* Create thread and wait for its readiness. */
    if( vlc_thread_create( p_input, "input", Run,
                            VLC_THREAD_PRIORITY_INPUT, VLC_TRUE ) )
    {
        msg_Err( p_input, "cannot create input thread" );
        vlc_object_detach( p_input );
        vlc_object_destroy( p_input );
        return NULL;
    }

    return p_input;
}


/**
 * Initialize an input thread and run it. This thread will clean after himself,
 * you can forget about it. It can work either in blocking or non-blocking mode
 *
 * \param p_parent a vlc_object
 * \param p_item an input item
 * \param b_block should we block until read is finished ?
 * \return the input object id if non blocking, an error code else
 */
int __input_Read( vlc_object_t *p_parent, input_item_t *p_item,
                   vlc_bool_t b_block )
{
    input_thread_t *p_input;                        /* thread descriptor */

    p_input = Create( p_parent, p_item, VLC_FALSE );
    /* Now we can attach our new input */
    vlc_object_attach( p_input, p_parent );

    if( b_block )
    {
        RunAndClean( p_input );
        return VLC_SUCCESS;
    }
    else
    {
        if( vlc_thread_create( p_input, "input", RunAndClean,
                               VLC_THREAD_PRIORITY_INPUT, VLC_TRUE ) )
        {
            msg_Err( p_input, "cannot create input thread" );
            vlc_object_detach( p_input );
            vlc_object_destroy( p_input );
            return VLC_EGENERIC;
        }
    }
    return p_input->i_object_id;
}

/**
 * Initialize an input and initialize it to preparse the item
 * This function is blocking. It will only accept to parse files
 *
 * \param p_parent a vlc_object_t
 * \param p_item an input item
 * \return VLC_SUCCESS or an error
 */
int __input_Preparse( vlc_object_t *p_parent, input_item_t *p_item )
{
    input_thread_t *p_input;                        /* thread descriptor */

    /* Allocate descriptor */
    p_input = Create( p_parent, p_item, VLC_TRUE );

    /* Now we can attach our new input */
    vlc_object_attach( p_input, p_parent );

    Init( p_input, VLC_TRUE );

    /* Clean up master */
    InputSourceClean( p_input, &p_input->input );

    /* Kill access and demux */
    if( p_input->input.p_access ) p_input->input.p_access->b_die = VLC_TRUE;
    if( p_input->input.p_demux ) p_input->input.p_access->b_die = VLC_TRUE;

    /* Unload all modules */
    if( p_input->p_es_out ) input_EsOutDelete( p_input->p_es_out );

    /* Delete meta */
    if( p_input->p_meta ) vlc_meta_Delete( p_input->p_meta );

    vlc_object_detach( p_input );
    vlc_object_destroy( p_input );

    return VLC_SUCCESS;
}

/**
 * Request a running input thread to stop and die
 *
 * \param the input thread to stop
 */
void input_StopThread( input_thread_t *p_input )
{
    vlc_list_t *p_list;
    int i;

    /* Set die for input */
    p_input->b_die = VLC_TRUE;

    /* We cannot touch p_input fields directly (we can from another thread),
     * so use the vlc_object_find way, it's perfectly safe */

    /* Set die for all access */
    p_list = vlc_list_find( p_input, VLC_OBJECT_ACCESS, FIND_CHILD );
    for( i = 0; i < p_list->i_count; i++ )
    {
        p_list->p_values[i].p_object->b_die = VLC_TRUE;
    }
    vlc_list_release( p_list );

    /* Set die for all stream */
    p_list = vlc_list_find( p_input, VLC_OBJECT_STREAM, FIND_CHILD );
    for( i = 0; i < p_list->i_count; i++ )
    {
        p_list->p_values[i].p_object->b_die = VLC_TRUE;
    }
    vlc_list_release( p_list );

    /* Set die for all demux */
    p_list = vlc_list_find( p_input, VLC_OBJECT_DEMUX, FIND_CHILD );
    for( i = 0; i < p_list->i_count; i++ )
    {
        p_list->p_values[i].p_object->b_die = VLC_TRUE;
    }
    vlc_list_release( p_list );

    input_ControlPush( p_input, INPUT_CONTROL_SET_DIE, NULL );
}

/**
 * Clean up a dead input thread
 * This function does not return until the thread is effectively cancelled.
 *
 * \param the input thread to kill
 */
void input_DestroyThread( input_thread_t *p_input )
{
    /* Join the thread */
    vlc_thread_join( p_input );

    /* Delete input lock (only after thread joined) */
    vlc_mutex_destroy( &p_input->lock_control );

    /* TODO: maybe input_DestroyThread should also delete p_input instead
     * of the playlist but I'm not sure if it's possible */
}

/*****************************************************************************
 * Run: main thread loop
 * This is the "normal" thread that spawns the input processing chain,
 * reads the stream, cleans up and waits
 *****************************************************************************/
static int Run( input_thread_t *p_input )
{

    /* Signal that the thread is launched */
    vlc_thread_ready( p_input );

    if( Init( p_input, VLC_FALSE ) )
    {
        /* If we failed, wait before we are killed, and exit */
        p_input->b_error = VLC_TRUE;

        Error( p_input );

        /* Tell we're dead */
        p_input->b_dead = VLC_TRUE;

        return 0;
    }

    MainLoop( p_input );

    if( !p_input->b_eof && !p_input->b_error && p_input->input.b_eof )
    {
        /* We have finish to demux data but not to play them */
        while( !p_input->b_die )
        {
            if( input_EsOutDecodersEmpty( p_input->p_es_out ) )
                break;

            msg_Dbg( p_input, "waiting decoder fifos to empty" );

            msleep( INPUT_IDLE_SLEEP );
        }

        /* We have finished */
        p_input->b_eof = VLC_TRUE;
    }

    /* Wait we are asked to die */
    if( !p_input->b_die )
    {
        Error( p_input );
    }

    /* Clean up */
    End( p_input );

    return 0;
}

/*****************************************************************************
 * RunAndClean: main thread loop
 * This is the "just forget me" thread that spawns the input processing chain,
 * reads the stream, cleans up and releases memory
 *****************************************************************************/
static int RunAndClean( input_thread_t *p_input )
{
    /* Signal that the thread is launched */
    vlc_thread_ready( p_input );

    if( Init( p_input, VLC_FALSE ) )
    {
        /* If we failed, just exit */
        return 0;
    }

    MainLoop( p_input );

    if( !p_input->b_eof && !p_input->b_error && p_input->input.b_eof )
    {
        /* We have finish to demux data but not to play them */
        while( !p_input->b_die )
        {
            if( input_EsOutDecodersEmpty( p_input->p_es_out ) )
                break;

            msg_Dbg( p_input, "waiting decoder fifos to empty" );

            msleep( INPUT_IDLE_SLEEP );
        }
        /* We have finished */
        p_input->b_eof = VLC_TRUE;
    }

    /* Clean up */
    End( p_input );

    /* Release memory */
    vlc_object_detach( p_input );
    vlc_object_destroy( p_input );

    return 0;
}


/*****************************************************************************
 * Main loop: Fill buffers from access, and demux
 *****************************************************************************/
static void MainLoop( input_thread_t *p_input )
{
    int64_t i_intf_update = 0;
    while( !p_input->b_die && !p_input->b_error && !p_input->input.b_eof )
    {
        vlc_bool_t b_force_update = VLC_FALSE;
        int i_ret;
        int i_type;
        vlc_value_t val;

        /* Do the read */
        if( p_input->i_state != PAUSE_S  )
        {
            if( p_input->i_stop <= 0 || p_input->i_time < p_input->i_stop )
                i_ret=p_input->input.p_demux->pf_demux(p_input->input.p_demux);
            else
                i_ret = 0;  /* EOF */

            if( i_ret > 0 )
            {
                /* TODO */
                if( p_input->input.b_title_demux &&
                    p_input->input.p_demux->info.i_update )
                {
                    i_ret = UpdateFromDemux( p_input );
                    b_force_update = VLC_TRUE;
                }
                else if( !p_input->input.b_title_demux &&
                          p_input->input.p_access &&
                          p_input->input.p_access->info.i_update )
                {
                    i_ret = UpdateFromAccess( p_input );
                    b_force_update = VLC_TRUE;
                }
            }

            if( i_ret == 0 )    /* EOF */
            {
                vlc_value_t repeat;

                var_Get( p_input, "input-repeat", &repeat );
                if( repeat.i_int == 0 )
                {
                    /* End of file - we do not set b_die because only the
                     * playlist is allowed to do so. */
                    msg_Dbg( p_input, "EOF reached" );
                    p_input->input.b_eof = VLC_TRUE;
                }
                else
                {
                    msg_Dbg( p_input, "repeating the same input (%d)",
                             repeat.i_int );
                    if( repeat.i_int > 0 )
                    {
                        repeat.i_int--;
                        var_Set( p_input, "input-repeat", repeat );
                    }

                    /* Seek to start title/seekpoint */
                    val.i_int = p_input->input.i_title_start -
                        p_input->input.i_title_offset;
                    if( val.i_int < 0 || val.i_int >= p_input->input.i_title )
                        val.i_int = 0;
                    input_ControlPush( p_input,
                                       INPUT_CONTROL_SET_TITLE, &val );

                    val.i_int = p_input->input.i_seekpoint_start -
                        p_input->input.i_seekpoint_offset;
                    if( val.i_int > 0 /* TODO: check upper boundary */ )
                        input_ControlPush( p_input,
                                           INPUT_CONTROL_SET_SEEKPOINT, &val );

                    /* Seek to start position */
                    if( p_input->i_start > 0 )
                    {
                        val.i_time = p_input->i_start;
                        input_ControlPush( p_input, INPUT_CONTROL_SET_TIME,
                                           &val );
                    }
                    else
                    {
                        val.f_float = 0.0;
                        input_ControlPush( p_input, INPUT_CONTROL_SET_POSITION,
                                           &val );
                    }
                }
            }
            else if( i_ret < 0 )
            {
                p_input->b_error = VLC_TRUE;
            }

            if( i_ret > 0 && p_input->i_slave > 0 )
            {
                SlaveDemux( p_input );
            }
        }
        else
        {
            /* Small wait */
            msleep( 10*1000 );
        }

        /* Handle control */
        vlc_mutex_lock( &p_input->lock_control );
        ControlReduce( p_input );
        while( !ControlPopNoLock( p_input, &i_type, &val ) )
        {
            msg_Dbg( p_input, "control type=%d", i_type );
            if( Control( p_input, i_type, val ) )
                b_force_update = VLC_TRUE;
        }
        vlc_mutex_unlock( &p_input->lock_control );

        if( b_force_update || i_intf_update < mdate() )
        {
            vlc_value_t val;
            double f_pos;
            int64_t i_time, i_length;
            /* update input status variables */
            if( !demux2_Control( p_input->input.p_demux,
                                 DEMUX_GET_POSITION, &f_pos ) )
            {
                val.f_float = (float)f_pos;
                var_Change( p_input, "position", VLC_VAR_SETVALUE, &val, NULL );
            }
            if( !demux2_Control( p_input->input.p_demux,
                                 DEMUX_GET_TIME, &i_time ) )
            {
                p_input->i_time = i_time;
                val.i_time = i_time;
                var_Change( p_input, "time", VLC_VAR_SETVALUE, &val, NULL );
            }
            if( !demux2_Control( p_input->input.p_demux,
                                 DEMUX_GET_LENGTH, &i_length ) )
            {
                vlc_value_t old_val;
                var_Get( p_input, "length", &old_val );
                val.i_time = i_length;
                var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );

                if( old_val.i_time != val.i_time )
                {
                    UpdateItemLength( p_input, i_length, VLC_TRUE );
                }
            }

            var_SetBool( p_input, "intf-change", VLC_TRUE );
            i_intf_update = mdate() + I64C(150000);
        }
    }
}


static int Init( input_thread_t * p_input, vlc_bool_t b_quick )
{
    char *psz;
    char *psz_subtitle;
    vlc_value_t val;
    double f_fps;
    vlc_meta_t *p_meta, *p_meta_tmp;
    int i_es_out_mode;
    int i, i_delay;

    /* Initialize optional stream output. (before access/demuxer)
     * XXX: we add a special case if the uri starts by vlc.
     * else 'vlc in.file --sout "" vlc:quit'  cannot work (the output will
     * be destroyed in case of a file).
     * (this will break playing of file starting by 'vlc:' but I don't
     * want to add more logic, just force file by file:// or code it ;)
     */
    if( !b_quick )
    {
        counter_t *p_counter;
        stats_Create( p_input, "read_bytes", VLC_VAR_INTEGER, STATS_COUNTER );
        stats_Create( p_input, "read_packets", VLC_VAR_INTEGER, STATS_COUNTER );
        stats_Create( p_input, "demux_read", VLC_VAR_INTEGER, STATS_COUNTER );
        stats_Create( p_input, "input_bitrate", VLC_VAR_FLOAT,
                               STATS_DERIVATIVE );
        stats_Create( p_input, "demux_bitrate", VLC_VAR_FLOAT,
                               STATS_DERIVATIVE );
        p_counter = stats_CounterGet( p_input, p_input->i_object_id,
                                      "input_bitrate" );
        if( p_counter ) p_counter->update_interval = 1000000;

        psz = var_GetString( p_input, "sout" );
        if( *psz && strncasecmp( p_input->input.p_item->psz_uri, "vlc:", 4 ) )
        {
            p_input->p_sout = sout_NewInstance( p_input, psz );
            if( p_input->p_sout == NULL )
            {
                msg_Err( p_input, "cannot start stream output instance, " \
                                  "aborting" );
                free( psz );
                return VLC_EGENERIC;
            }
        }
        free( psz );
    }

    /* Create es out */
    p_input->p_es_out = input_EsOutNew( p_input );
    es_out_Control( p_input->p_es_out, ES_OUT_SET_ACTIVE, VLC_FALSE );
    es_out_Control( p_input->p_es_out, ES_OUT_SET_MODE, ES_OUT_MODE_NONE );

    if( InputSourceInit( p_input, &p_input->input,
                         p_input->input.p_item->psz_uri, NULL, b_quick ) )
    {
        goto error;
    }

    /* Create global title (from master) */
    if( !b_quick )
    {
        p_input->i_title = p_input->input.i_title;
        p_input->title   = p_input->input.title;
        p_input->i_title_offset = p_input->input.i_title_offset;
        p_input->i_seekpoint_offset = p_input->input.i_seekpoint_offset;
        if( p_input->i_title > 0 )
        {
            /* Setup variables */
            input_ControlVarNavigation( p_input );
            input_ControlVarTitle( p_input, 0 );
        }

        /* Global flag */
        p_input->b_can_pace_control = p_input->input.b_can_pace_control;
        p_input->b_can_pause        = p_input->input.b_can_pause;

        /* Fix pts delay */
        if( p_input->i_pts_delay < 0 )
            p_input->i_pts_delay = 0;

        /* If the desynchronisation requested by the user is < 0, we need to
         * cache more data. */
        var_Get( p_input, "audio-desync", &val );
        if( val.i_int < 0 ) p_input->i_pts_delay -= (val.i_int * 1000);

        /* Update cr_average depending on the caching */
        p_input->input.i_cr_average *= (10 * p_input->i_pts_delay / 200000);
        p_input->input.i_cr_average /= 10;
        if( p_input->input.i_cr_average < 10 ) p_input->input.i_cr_average = 10;
    }

    /* Load master infos */
    /* Init length */
    if( !demux2_Control( p_input->input.p_demux, DEMUX_GET_LENGTH,
                         &val.i_time ) && val.i_time > 0 )
    {
        var_Change( p_input, "length", VLC_VAR_SETVALUE, &val, NULL );
        UpdateItemLength( p_input, val.i_time, b_quick );
        p_input->input.p_item->i_duration = val.i_time;
    }

    /* Start title/chapter */
    if( !b_quick )
    {
        val.i_int = p_input->input.i_title_start -
                    p_input->input.i_title_offset;
        if( val.i_int > 0 && val.i_int < p_input->input.i_title )
            input_ControlPush( p_input, INPUT_CONTROL_SET_TITLE, &val );
        val.i_int = p_input->input.i_seekpoint_start -
                    p_input->input.i_seekpoint_offset;
        if( val.i_int > 0 /* TODO: check upper boundary */ )
            input_ControlPush( p_input, INPUT_CONTROL_SET_SEEKPOINT, &val );

        /* Start time*/
        /* Set start time */
        p_input->i_start = (int64_t)var_GetInteger( p_input, "start-time" ) *
                           I64C(1000000);
        p_input->i_stop  = (int64_t)var_GetInteger( p_input, "stop-time" ) *
                           I64C(1000000);

        if( p_input->i_start > 0 )
        {
            if( p_input->i_start >= val.i_time )
            {
                msg_Warn( p_input, "invalid start-time ignored" );
            }
            else
            {
                vlc_value_t s;

                msg_Dbg( p_input, "starting at time: %ds",
                                  (int)( p_input->i_start / I64C(1000000) ) );

                s.i_time = p_input->i_start;
                input_ControlPush( p_input, INPUT_CONTROL_SET_TIME, &s );
            }
        }
        if( p_input->i_stop > 0 && p_input->i_stop <= p_input->i_start )
        {
            msg_Warn( p_input, "invalid stop-time ignored" );
            p_input->i_stop = 0;
        }


        /* Load subtitles */
        /* Get fps and set it if not already set */
        if( !demux2_Control( p_input->input.p_demux, DEMUX_GET_FPS, &f_fps ) &&
            f_fps > 1.0 )
        {
            float f_requested_fps;

            var_Create( p_input, "sub-original-fps", VLC_VAR_FLOAT );
            var_SetFloat( p_input, "sub-original-fps", f_fps );

            f_requested_fps = var_CreateGetFloat( p_input, "sub-fps" );
            if( f_requested_fps != f_fps )
            {
                var_Create( p_input, "sub-fps", VLC_VAR_FLOAT|
                                                VLC_VAR_DOINHERIT );
                var_SetFloat( p_input, "sub-fps", f_requested_fps );
            }
        }

        i_delay = var_CreateGetInteger( p_input, "sub-delay" );
        if( i_delay != 0 )
        {
            var_SetTime( p_input, "spu-delay", (mtime_t)i_delay * 100000 );
        }


        /* Look for and add subtitle files */
        psz_subtitle = var_GetString( p_input, "sub-file" );
        if( *psz_subtitle )
        {
            input_source_t *sub;
            vlc_value_t count;
            vlc_value_t list;

            msg_Dbg( p_input, "forced subtitle: %s", psz_subtitle );

            var_Change( p_input, "spu-es", VLC_VAR_CHOICESCOUNT, &count, NULL );

            /* */
            sub = InputSourceNew( p_input );
            if( !InputSourceInit( p_input, sub, psz_subtitle, "subtitle",
                                  VLC_FALSE ) )
            {
                TAB_APPEND( p_input->i_slave, p_input->slave, sub );

                /* Select the ES */
                if( !var_Change( p_input, "spu-es", VLC_VAR_GETLIST, &list,
                                 NULL ) )
                {
                    if( count.i_int == 0 )
                        count.i_int++;
                        /* if it was first one, there is disable too */

                    if( count.i_int < list.p_list->i_count )
                    {
                        input_ControlPush( p_input, INPUT_CONTROL_SET_ES,
                                          &list.p_list->p_values[count.i_int] );
                    }
                    var_Change( p_input, "spu-es", VLC_VAR_FREELIST, &list,
                                NULL );
                }
            }
        }

        var_Get( p_input, "sub-autodetect-file", &val );
        if( val.b_bool )
        {
            char *psz_autopath = var_GetString( p_input, "sub-autodetect-path" );
            char **subs = subtitles_Detect( p_input, psz_autopath,
                                            p_input->input.p_item->psz_uri );
            input_source_t *sub;

            for( i = 0; subs && subs[i]; i++ )
            {
                if( strcmp( psz_subtitle, subs[i] ) )
                {
                    sub = InputSourceNew( p_input );
                    if( !InputSourceInit( p_input, sub, subs[i], "subtitle",
                                          VLC_FALSE ) )
                    {
                         TAB_APPEND( p_input->i_slave, p_input->slave, sub );
                    }
                }
                free( subs[i] );
            }
            if( subs ) free( subs );
            if( psz_autopath ) free( psz_autopath );
        }
        free( psz_subtitle );

        /* Look for slave */
        psz = var_GetString( p_input, "input-slave" );
        if( *psz )
        {
            char *psz_delim;
            input_source_t *slave;
            while( psz && *psz )
            {
                while( *psz == ' ' || *psz == '#' )
                {
                    psz++;
                }
                if( ( psz_delim = strchr( psz, '#' ) ) )
                {
                    *psz_delim++ = '\0';
                }
                if( *psz == 0 )
                {
                    break;
                }

                msg_Dbg( p_input, "adding slave input '%s'", psz );
                slave = InputSourceNew( p_input );
                if( !InputSourceInit( p_input, slave, psz, NULL, VLC_FALSE ) )
                {
                    TAB_APPEND( p_input->i_slave, p_input->slave, slave );
                }
                psz = psz_delim;
            }
        }
        if( psz ) free( psz );
    }
    else
    {
        p_input->i_start = 0;
        p_input->i_start = 0;
    }

    /* Set up es_out */
    if( !b_quick )
    {
        es_out_Control( p_input->p_es_out, ES_OUT_SET_ACTIVE, VLC_TRUE );
        i_es_out_mode = ES_OUT_MODE_AUTO;
        val.p_list = NULL;
        if( p_input->p_sout )
        {
            var_Get( p_input, "sout-all", &val );
            if ( val.b_bool )
            {
                i_es_out_mode = ES_OUT_MODE_ALL;
                val.p_list = NULL;
            }
            else
            {
                var_Get( p_input, "programs", &val );
                if ( val.p_list && val.p_list->i_count )
                {
                    i_es_out_mode = ES_OUT_MODE_PARTIAL;
                    /* Note : we should remove the "program" callback. */
                }
                else
                    var_Change( p_input, "programs", VLC_VAR_FREELIST, &val,
                                NULL );
            }
        }
        es_out_Control( p_input->p_es_out, ES_OUT_SET_MODE, i_es_out_mode );

        /* Inform the demuxer about waited group (needed only for DVB) */
        if( i_es_out_mode == ES_OUT_MODE_ALL )
        {
            demux2_Control( p_input->input.p_demux, DEMUX_SET_GROUP, -1, NULL );
        }
        else if( i_es_out_mode == ES_OUT_MODE_PARTIAL )
        {
            demux2_Control( p_input->input.p_demux, DEMUX_SET_GROUP, -1,
                            val.p_list );
        }
        else
        {
            demux2_Control( p_input->input.p_demux, DEMUX_SET_GROUP,
                           (int) var_GetInteger( p_input, "program" ), NULL );
        }

        if( p_input->p_sout )
        {
            if( p_input->p_sout->i_out_pace_nocontrol > 0 )
            {
                p_input->b_out_pace_control = VLC_FALSE;
            }
            else
            {
                p_input->b_out_pace_control = VLC_TRUE;
            }

            if( p_input->b_can_pace_control && p_input->b_out_pace_control )
            {
                /* We don't want a high input priority here or we'll
                 * end-up sucking up all the CPU time */
                vlc_thread_set_priority( p_input, VLC_THREAD_PRIORITY_LOW );
            }

            msg_Dbg( p_input, "starting in %s mode",
                     p_input->b_out_pace_control ? "asynch" : "synch" );
        }
    }

    /* Get meta data from users */
    p_meta_tmp = InputMetaUser( p_input );

    /* Get meta data from master input */
    if( demux2_Control( p_input->input.p_demux, DEMUX_GET_META, &p_meta ) )
        p_meta = NULL;

    /* Merge them */
    if( p_meta == NULL )
    {
        p_meta = p_meta_tmp;
    }
    else if( p_meta_tmp )
    {
        vlc_meta_Merge( p_meta, p_meta_tmp );
        vlc_meta_Delete( p_meta_tmp );
    }

    /* Access_file does not give any meta, and there are no slave */
    if( !b_quick )
    {
        if( !p_input->input.p_access ||
            access2_Control( p_input->input.p_access, ACCESS_GET_META,
                             &p_meta_tmp))
            p_meta_tmp = NULL;

        if( p_meta == NULL )
        {
            p_meta = p_meta_tmp;
        }
        else if( p_meta_tmp )
        {
            vlc_meta_Merge( p_meta, p_meta_tmp );
            vlc_meta_Delete( p_meta_tmp );
        }

        /* Get meta data from slave input */
        for( i = 0; i < p_input->i_slave; i++ )
        {
            vlc_meta_t *p_meta_slave;

            if( !demux2_Control( p_input->slave[i]->p_demux,
                                 DEMUX_GET_META, &p_meta_slave ) )
            {
                if( p_meta == NULL )
                {
                    p_meta = p_meta_slave;
                }
                else if( p_meta_slave )
                {
                    vlc_meta_Merge( p_meta, p_meta_slave );
                    vlc_meta_Delete( p_meta_slave );
                }
            }

            if( p_input->slave[i]->p_access &&
                !access2_Control( p_input->slave[i]->p_access,
                                  ACCESS_GET_META, &p_meta_slave ) )
            {
                if( p_meta == NULL )
                {
                    p_meta = p_meta_slave;
                }
                else if( p_meta_slave )
                {
                    vlc_meta_Merge( p_meta, p_meta_slave );
                    vlc_meta_Delete( p_meta_slave );
                }
            }
        }
    }

    p_input->p_meta = p_meta;
    UpdateMeta( p_input, b_quick );

    if( !b_quick )
    {
        msg_Dbg( p_input, "`%s' successfully opened",
                 p_input->input.p_item->psz_uri );

    }

    /* initialization is complete */
    p_input->i_state = PLAYING_S;

    val.i_int = PLAYING_S;
    var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );

    return VLC_SUCCESS;

error:
    if( p_input->p_es_out )
        input_EsOutDelete( p_input->p_es_out );

    if( p_input->p_sout )
        sout_DeleteInstance( p_input->p_sout );

    /* Mark them deleted */
    p_input->input.p_demux = NULL;
    p_input->input.p_stream = NULL;
    p_input->input.p_access = NULL;
    p_input->p_es_out = NULL;
    p_input->p_sout = NULL;

    return VLC_EGENERIC;
}

/*****************************************************************************
 * Error: RunThread() error loop
 *****************************************************************************
 * This function is called when an error occurred during thread main's loop.
 *****************************************************************************/
static void Error( input_thread_t *p_input )
{
    while( !p_input->b_die )
    {
        /* Sleep a while */
        msleep( INPUT_IDLE_SLEEP );
    }
}

/*****************************************************************************
 * End: end the input thread
 *****************************************************************************/
static void End( input_thread_t * p_input )
{
    vlc_value_t val;
    int i;

    msg_Dbg( p_input, "closing input" );

    /* We are at the end */
    p_input->i_state = END_S;

    val.i_int = END_S;
    var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );

    /* Clean control variables */
    input_ControlVarClean( p_input );

    /* Clean up master */
    InputSourceClean( p_input, &p_input->input );

    /* Delete slave */
    for( i = 0; i < p_input->i_slave; i++ )
    {
        InputSourceClean( p_input, p_input->slave[i] );
        free( p_input->slave[i] );
    }
    if( p_input->slave ) free( p_input->slave );

    /* Unload all modules */
    if( p_input->p_es_out )
        input_EsOutDelete( p_input->p_es_out );

    /* Close optional stream output instance */
    if( p_input->p_sout )
    {
        vlc_object_t *p_pl =
            vlc_object_find( p_input, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
        vlc_value_t keep;

        if( var_Get( p_input, "sout-keep", &keep ) >= 0 && keep.b_bool && p_pl )
        {
            /* attach sout to the playlist */
            msg_Dbg( p_input, "keeping sout" );
            vlc_object_detach( p_input->p_sout );
            vlc_object_attach( p_input->p_sout, p_pl );
        }
        else
        {
            msg_Dbg( p_input, "destroying sout" );
            sout_DeleteInstance( p_input->p_sout );
        }
        if( p_pl )
            vlc_object_release( p_pl );
    }

    /* Delete meta */
    if( p_input->p_meta )
        vlc_meta_Delete( p_input->p_meta );

    /* Tell we're dead */
    p_input->b_dead = VLC_TRUE;
}

/*****************************************************************************
 * Control
 *****************************************************************************/
static inline int ControlPopNoLock( input_thread_t *p_input,
                                    int *pi_type, vlc_value_t *p_val )
{
    if( p_input->i_control <= 0 )
    {
        return VLC_EGENERIC;
    }

    *pi_type = p_input->control[0].i_type;
    *p_val   = p_input->control[0].val;

    p_input->i_control--;
    if( p_input->i_control > 0 )
    {
        int i;

        for( i = 0; i < p_input->i_control; i++ )
        {
            p_input->control[i].i_type = p_input->control[i+1].i_type;
            p_input->control[i].val    = p_input->control[i+1].val;
        }
    }

    return VLC_SUCCESS;
}

static void ControlReduce( input_thread_t *p_input )
{
    int i;
    for( i = 1; i < p_input->i_control; i++ )
    {
        const int i_lt = p_input->control[i-1].i_type;
        const int i_ct = p_input->control[i].i_type;

        /* XXX We can't merge INPUT_CONTROL_SET_ES */
/*        msg_Dbg( p_input, "[%d/%d] l=%d c=%d", i, p_input->i_control,
                 i_lt, i_ct );
*/
        if( i_lt == i_ct &&
            ( i_ct == INPUT_CONTROL_SET_STATE ||
              i_ct == INPUT_CONTROL_SET_RATE ||
              i_ct == INPUT_CONTROL_SET_POSITION ||
              i_ct == INPUT_CONTROL_SET_TIME ||
              i_ct == INPUT_CONTROL_SET_PROGRAM ||
              i_ct == INPUT_CONTROL_SET_TITLE ||
              i_ct == INPUT_CONTROL_SET_SEEKPOINT ||
              i_ct == INPUT_CONTROL_SET_BOOKMARK ) )
        {
            int j;
//            msg_Dbg( p_input, "merged at %d", i );
            /* Remove the i-1 */
            for( j = i; j <  p_input->i_control; j++ )
                p_input->control[j-1] = p_input->control[j];
            p_input->i_control--;
        }
        else
        {
            /* TODO but that's not that important
                - merge SET_X with SET_X_CMD
                - remove SET_SEEKPOINT/SET_POSITION/SET_TIME before a SET_TITLE
                - remove SET_SEEKPOINT/SET_POSITION/SET_TIME before another among them
                - ?
                */
        }
    }
}

static vlc_bool_t Control( input_thread_t *p_input, int i_type,
                           vlc_value_t val )
{
    vlc_bool_t b_force_update = VLC_FALSE;

    if( !p_input ) return b_force_update;

    switch( i_type )
    {
        case INPUT_CONTROL_SET_DIE:
            msg_Dbg( p_input, "control: stopping input" );
            /* Mark all submodules to die */
            if( p_input->input.p_access )
                p_input->input.p_access->b_die = VLC_TRUE;
            if( p_input->input.p_stream )
                p_input->input.p_stream->b_die = VLC_TRUE;
            p_input->input.p_demux->b_die = VLC_TRUE;

            p_input->b_die = VLC_TRUE;
            break;

        case INPUT_CONTROL_SET_POSITION:
        case INPUT_CONTROL_SET_POSITION_OFFSET:
        {
            double f_pos;
            if( i_type == INPUT_CONTROL_SET_POSITION )
            {
                f_pos = val.f_float;
            }
            else
            {
                /* Should not fail */
                demux2_Control( p_input->input.p_demux,
                                DEMUX_GET_POSITION, &f_pos );
                f_pos += val.f_float;
            }
            if( f_pos < 0.0 ) f_pos = 0.0;
            if( f_pos > 1.0 ) f_pos = 1.0;
            /* Reset the decoders states and clock synch (before calling the demuxer */
            es_out_Control( p_input->p_es_out, ES_OUT_RESET_PCR );
            input_EsOutDiscontinuity( p_input->p_es_out, VLC_FALSE );
            if( demux2_Control( p_input->input.p_demux, DEMUX_SET_POSITION,
                                f_pos ) )
            {
                msg_Err( p_input, "INPUT_CONTROL_SET_POSITION(_OFFSET) "
                         "%2.1f%% failed", f_pos * 100 );
            }
            else
            {
                if( p_input->i_slave > 0 )
                    SlaveSeek( p_input );

                b_force_update = VLC_TRUE;
            }
            break;
        }

        case INPUT_CONTROL_SET_TIME:
        case INPUT_CONTROL_SET_TIME_OFFSET:
        {
            int64_t i_time;
            int i_ret;

            if( i_type == INPUT_CONTROL_SET_TIME )
            {
                i_time = val.i_time;
            }
            else
            {
                /* Should not fail */
                demux2_Control( p_input->input.p_demux,
                                DEMUX_GET_TIME, &i_time );
                i_time += val.i_time;
            }
            if( i_time < 0 ) i_time = 0;

            /* Reset the decoders states and clock synch (before calling the demuxer */
            es_out_Control( p_input->p_es_out, ES_OUT_RESET_PCR );
            input_EsOutDiscontinuity( p_input->p_es_out, VLC_FALSE );

            i_ret = demux2_Control( p_input->input.p_demux,
                                    DEMUX_SET_TIME, i_time );
            if( i_ret )
            {
                int64_t i_length;

                /* Emulate it with a SET_POS */
                demux2_Control( p_input->input.p_demux,
                                DEMUX_GET_LENGTH, &i_length );
                if( i_length > 0 )
                {
                    double f_pos = (double)i_time / (double)i_length;
                    i_ret = demux2_Control( p_input->input.p_demux,
                                            DEMUX_SET_POSITION, f_pos );
                }
            }
            if( i_ret )
            {
                msg_Warn( p_input, "INPUT_CONTROL_SET_TIME(_OFFSET) "I64Fd
                         " failed or not possible", i_time );
            }
            else
            {
                if( p_input->i_slave > 0 )
                    SlaveSeek( p_input );

                b_force_update = VLC_TRUE;
            }
            break;
        }

        case INPUT_CONTROL_SET_STATE:
            if( ( val.i_int == PLAYING_S && p_input->i_state == PAUSE_S ) ||
                ( val.i_int == PAUSE_S && p_input->i_state == PAUSE_S ) )
            {
                int i_ret;
                if( p_input->input.p_access )
                    i_ret = access2_Control( p_input->input.p_access,
                                             ACCESS_SET_PAUSE_STATE, VLC_FALSE );
                else
                    i_ret = demux2_Control( p_input->input.p_demux,
                                            DEMUX_SET_PAUSE_STATE, VLC_FALSE );

                if( i_ret )
                {
                    /* FIXME What to do ? */
                    msg_Warn( p_input, "cannot unset pause -> EOF" );
                    vlc_mutex_unlock( &p_input->lock_control );
                    input_ControlPush( p_input, INPUT_CONTROL_SET_DIE, NULL );
                    vlc_mutex_lock( &p_input->lock_control );
                }

                b_force_update = VLC_TRUE;

                /* Switch to play */
                p_input->i_state = PLAYING_S;
                val.i_int = PLAYING_S;
                var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );

                /* Reset clock */
                es_out_Control( p_input->p_es_out, ES_OUT_RESET_PCR );
                input_EsOutDiscontinuity( p_input->p_es_out, VLC_FALSE );
            }
            else if( val.i_int == PAUSE_S && p_input->i_state == PLAYING_S &&
                     p_input->b_can_pause )
            {
                int i_ret;
                if( p_input->input.p_access )
                    i_ret = access2_Control( p_input->input.p_access,
                                             ACCESS_SET_PAUSE_STATE, VLC_TRUE );
                else
                    i_ret = demux2_Control( p_input->input.p_demux,
                                            DEMUX_SET_PAUSE_STATE, VLC_TRUE );

                b_force_update = VLC_TRUE;

                if( i_ret )
                {
                    msg_Warn( p_input, "cannot set pause state" );
                    val.i_int = p_input->i_state;
                }
                else
                {
                    val.i_int = PAUSE_S;
                }

                /* Switch to new state */
                p_input->i_state = val.i_int;
                var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );
            }
            else if( val.i_int == PAUSE_S && !p_input->b_can_pause )
            {
                b_force_update = VLC_TRUE;

                /* Correct "state" value */
                val.i_int = p_input->i_state;
                var_Change( p_input, "state", VLC_VAR_SETVALUE, &val, NULL );
            }
            else if( val.i_int != PLAYING_S && val.i_int != PAUSE_S )
            {
                msg_Err( p_input, "invalid state in INPUT_CONTROL_SET_STATE" );
            }
            break;

        case INPUT_CONTROL_SET_RATE:
        case INPUT_CONTROL_SET_RATE_SLOWER:
        case INPUT_CONTROL_SET_RATE_FASTER:
        {
            int i_rate;

            if( i_type == INPUT_CONTROL_SET_RATE_SLOWER )
                i_rate = p_input->i_rate * 2;
            else if( i_type == INPUT_CONTROL_SET_RATE_FASTER )
                i_rate = p_input->i_rate / 2;
            else
                i_rate = val.i_int;

            if( i_rate < INPUT_RATE_MIN )
            {
                msg_Dbg( p_input, "cannot set rate faster" );
                i_rate = INPUT_RATE_MIN;
            }
            else if( i_rate > INPUT_RATE_MAX )
            {
                msg_Dbg( p_input, "cannot set rate slower" );
                i_rate = INPUT_RATE_MAX;
            }
            if( i_rate != INPUT_RATE_DEFAULT &&
                ( !p_input->b_can_pace_control ||
                  ( p_input->p_sout && !p_input->b_out_pace_control ) ) )
            {
                msg_Dbg( p_input, "cannot change rate" );
                i_rate = INPUT_RATE_DEFAULT;
            }
            if( i_rate != p_input->i_rate )
            {
                p_input->i_rate  = i_rate;
                val.i_int = i_rate;
                var_Change( p_input, "rate", VLC_VAR_SETVALUE, &val, NULL );

                /* We haven't send data to decoder when rate != default */
                if( i_rate == INPUT_RATE_DEFAULT )
                    input_EsOutDiscontinuity( p_input->p_es_out, VLC_TRUE );

                /* Reset clock */
                es_out_Control( p_input->p_es_out, ES_OUT_RESET_PCR );

                b_force_update = VLC_TRUE;
            }
            break;
        }

        case INPUT_CONTROL_SET_PROGRAM:
            /* No need to force update, es_out does it if needed */
            es_out_Control( p_input->p_es_out,
                            ES_OUT_SET_GROUP, val.i_int );

            demux2_Control( p_input->input.p_demux, DEMUX_SET_GROUP, val.i_int,
                            NULL );
            break;

        case INPUT_CONTROL_SET_ES:
            /* No need to force update, es_out does it if needed */
            es_out_Control( p_input->p_es_out, ES_OUT_SET_ES,
                            input_EsOutGetFromID( p_input->p_es_out,
                                                  val.i_int ) );
            break;

        case INPUT_CONTROL_SET_AUDIO_DELAY:
            input_EsOutSetDelay( p_input->p_es_out,
                                 AUDIO_ES, val.i_time );
            var_Change( p_input, "audio-delay", VLC_VAR_SETVALUE, &val, NULL );
            break;

        case INPUT_CONTROL_SET_SPU_DELAY:
            input_EsOutSetDelay( p_input->p_es_out,
                                 SPU_ES, val.i_time );
            var_Change( p_input, "spu-delay", VLC_VAR_SETVALUE, &val, NULL );
            break;

        case INPUT_CONTROL_SET_TITLE:
        case INPUT_CONTROL_SET_TITLE_NEXT:
        case INPUT_CONTROL_SET_TITLE_PREV:
            if( p_input->input.b_title_demux &&
                p_input->input.i_title > 0 )
            {
                /* TODO */
                /* FIXME handle demux title */
                demux_t *p_demux = p_input->input.p_demux;
                int i_title;

                if( i_type == INPUT_CONTROL_SET_TITLE_PREV )
                    i_title = p_demux->info.i_title - 1;
                else if( i_type == INPUT_CONTROL_SET_TITLE_NEXT )
                    i_title = p_demux->info.i_title + 1;
                else
                    i_title = val.i_int;

                if( i_title >= 0 && i_title < p_input->input.i_title )
                {
                    input_EsOutDiscontinuity( p_input->p_es_out, VLC_FALSE );
                    es_out_Control( p_input->p_es_out, ES_OUT_RESET_PCR );

                    demux2_Control( p_demux, DEMUX_SET_TITLE, i_title );
                    input_ControlVarTitle( p_input, i_title );
                }
            }
            else if( p_input->input.i_title > 0 )
            {
                access_t *p_access = p_input->input.p_access;
                int i_title;

                if( i_type == INPUT_CONTROL_SET_TITLE_PREV )
                    i_title = p_access->info.i_title - 1;
                else if( i_type == INPUT_CONTROL_SET_TITLE_NEXT )
                    i_title = p_access->info.i_title + 1;
                else
                    i_title = val.i_int;

                if( i_title >= 0 && i_title < p_input->input.i_title )
                {
                    input_EsOutDiscontinuity( p_input->p_es_out, VLC_FALSE );
                    es_out_Control( p_input->p_es_out, ES_OUT_RESET_PCR );
                    
                    access2_Control( p_access, ACCESS_SET_TITLE, i_title );
                    stream_AccessReset( p_input->input.p_stream );
                }
            }
            break;
        case INPUT_CONTROL_SET_SEEKPOINT:
        case INPUT_CONTROL_SET_SEEKPOINT_NEXT:
        case INPUT_CONTROL_SET_SEEKPOINT_PREV:
            if( p_input->input.b_title_demux &&
                p_input->input.i_title > 0 )
            {
                demux_t *p_demux = p_input->input.p_demux;
                int i_seekpoint;
                int64_t i_input_time;
                int64_t i_seekpoint_time;

                if( i_type == INPUT_CONTROL_SET_SEEKPOINT_PREV )
                {
                    i_seekpoint = p_demux->info.i_seekpoint;
                    i_seekpoint_time = p_input->input.title[p_demux->info.i_title]->seekpoint[i_seekpoint]->i_time_offset;
                    if( i_seekpoint_time >= 0 &&
                         !demux2_Control( p_demux,
                                          DEMUX_GET_TIME, &i_input_time ) )
                    {
                        if ( i_input_time < i_seekpoint_time + 3000000 )
                            i_seekpoint--;
                    }
                    else
                        i_seekpoint--;
                }
                else if( i_type == INPUT_CONTROL_SET_SEEKPOINT_NEXT )
                    i_seekpoint = p_demux->info.i_seekpoint + 1;
                else
                    i_seekpoint = val.i_int;

                if( i_seekpoint >= 0 && i_seekpoint <
                    p_input->input.title[p_demux->info.i_title]->i_seekpoint )
                {
                    input_EsOutDiscontinuity( p_input->p_es_out, VLC_FALSE );
                    es_out_Control( p_input->p_es_out, ES_OUT_RESET_PCR );
                    
                    demux2_Control( p_demux, DEMUX_SET_SEEKPOINT, i_seekpoint );
                }
            }
            else if( p_input->input.i_title > 0 )
            {
                demux_t *p_demux = p_input->input.p_demux;
                access_t *p_access = p_input->input.p_access;
                int i_seekpoint;
                int64_t i_input_time;
                int64_t i_seekpoint_time;

                if( i_type == INPUT_CONTROL_SET_SEEKPOINT_PREV )
                {
                    i_seekpoint = p_access->info.i_seekpoint;
                    i_seekpoint_time = p_input->input.title[p_access->info.i_title]->seekpoint[i_seekpoint]->i_time_offset;
                    if( i_seekpoint_time >= 0 &&
                        demux2_Control( p_demux,
                                        DEMUX_GET_TIME, &i_input_time ) )
                    {
                        if ( i_input_time < i_seekpoint_time + 3000000 )
                            i_seekpoint--;
                    }
                    else
                        i_seekpoint--;
                }
                else if( i_type == INPUT_CONTROL_SET_SEEKPOINT_NEXT ) 
                    i_seekpoint = p_access->info.i_seekpoint + 1;
                else
                    i_seekpoint = val.i_int;

                if( i_seekpoint >= 0 && i_seekpoint <
                    p_input->input.title[p_access->info.i_title]->i_seekpoint )
                {
                    input_EsOutDiscontinuity( p_input->p_es_out, VLC_FALSE );
                    es_out_Control( p_input->p_es_out, ES_OUT_RESET_PCR );
                    
                    access2_Control( p_access, ACCESS_SET_SEEKPOINT, i_seekpoint );
                    stream_AccessReset( p_input->input.p_stream );
                }
            }
            break;

        case INPUT_CONTROL_ADD_SLAVE:
            if( val.psz_string )
            {
                input_source_t *slave = InputSourceNew( p_input );

                if( !InputSourceInit( p_input, slave, val.psz_string, NULL,
                                      VLC_FALSE ) )
                {
                    vlc_meta_t *p_meta_new = NULL;
                    vlc_meta_t *p_meta;
                    int64_t i_time;

                    /* Add the slave */
                    msg_Dbg( p_input, "adding %s as slave on the fly",
                             val.psz_string );

                    /* Set position */
                    if( demux2_Control( p_input->input.p_demux,
                                        DEMUX_GET_TIME, &i_time ) )
                    {
                        msg_Err( p_input, "demux doesn't like DEMUX_GET_TIME" );
                        InputSourceClean( p_input, slave );
                        free( slave );
                        break;
                    }
                    if( demux2_Control( slave->p_demux,
                                        DEMUX_SET_TIME, i_time ) )
                    {
                        msg_Err( p_input, "seek failed for new slave" );
                        InputSourceClean( p_input, slave );
                        free( slave );
                        break;
                    }


                    /* Get meta (access and demux) */
                    if( access2_Control( slave->p_access,
                                          ACCESS_GET_META, &p_meta_new ) )
                        p_meta_new = NULL;
                    if( !demux2_Control( slave->p_demux,
                                         DEMUX_GET_META, &p_meta ) )
                    {
                        if( p_meta_new )
                        {
                            vlc_meta_Merge( p_meta_new, p_meta );
                            vlc_meta_Delete( p_meta );
                        }
                        else
                        {
                            p_meta_new = p_meta;
                        }
                    }
                    /* Update meta */
                    if( p_meta_new )
                    {
                        if( p_input->p_meta )
                        {
                            vlc_meta_Merge( p_input->p_meta, p_meta_new );
                            vlc_meta_Delete( p_meta_new );
                        }
                        else
                        {
                            p_input->p_meta = p_meta_new;
                        }
                        UpdateMeta( p_input, VLC_FALSE );
                    }

                    TAB_APPEND( p_input->i_slave, p_input->slave, slave );
                }
                else
                {
                    msg_Warn( p_input, "failed to add %s as slave",
                              val.psz_string );
                }

                free( val.psz_string );
            }
            break;

        case INPUT_CONTROL_SET_BOOKMARK:
        default:
            msg_Err( p_input, "not yet implemented" );
            break;
    }

    return b_force_update;
}

/*****************************************************************************
 * UpdateFromDemux:
 *****************************************************************************/
static int UpdateFromDemux( input_thread_t *p_input )
{
    demux_t *p_demux = p_input->input.p_demux;
    vlc_value_t v;

    if( p_demux->info.i_update & INPUT_UPDATE_TITLE )
    {
        v.i_int = p_demux->info.i_title;
        var_Change( p_input, "title", VLC_VAR_SETVALUE, &v, NULL );

        input_ControlVarTitle( p_input, p_demux->info.i_title );

        p_demux->info.i_update &= ~INPUT_UPDATE_TITLE;
    }
    if( p_demux->info.i_update & INPUT_UPDATE_SEEKPOINT )
    {
        v.i_int = p_demux->info.i_seekpoint;
        var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &v, NULL);

        p_demux->info.i_update &= ~INPUT_UPDATE_SEEKPOINT;
    }
    p_demux->info.i_update &= ~INPUT_UPDATE_SIZE;

    /* Hmmm only works with master input */
    if( p_input->input.p_demux == p_demux )
    {
        int i_title_end = p_input->input.i_title_end -
            p_input->input.i_title_offset;
        int i_seekpoint_end = p_input->input.i_seekpoint_end -
            p_input->input.i_seekpoint_offset;

        if( i_title_end >= 0 && i_seekpoint_end >= 0 )
        {
            if( p_demux->info.i_title > i_title_end ||
                ( p_demux->info.i_title == i_title_end &&
                  p_demux->info.i_seekpoint > i_seekpoint_end ) ) return 0;
        }
        else if( i_seekpoint_end >=0 )
        {
            if( p_demux->info.i_seekpoint > i_seekpoint_end ) return 0;
        }
        else if( i_title_end >= 0 )
        {
            if( p_demux->info.i_title > i_title_end ) return 0;
        }
    }

    return 1;
}

/*****************************************************************************
 * UpdateFromAccess:
 *****************************************************************************/
static int UpdateFromAccess( input_thread_t *p_input )
{
    access_t *p_access = p_input->input.p_access;
    vlc_value_t v;

    if( p_access->info.i_update & INPUT_UPDATE_TITLE )
    {
        v.i_int = p_access->info.i_title;
        var_Change( p_input, "title", VLC_VAR_SETVALUE, &v, NULL );

        input_ControlVarTitle( p_input, p_access->info.i_title );

        stream_AccessUpdate( p_input->input.p_stream );

        p_access->info.i_update &= ~INPUT_UPDATE_TITLE;
    }
    if( p_access->info.i_update & INPUT_UPDATE_SEEKPOINT )
    {
        v.i_int = p_access->info.i_seekpoint;
        var_Change( p_input, "chapter", VLC_VAR_SETVALUE, &v, NULL);

        p_access->info.i_update &= ~INPUT_UPDATE_SEEKPOINT;
    }
    if( p_access->info.i_update & INPUT_UPDATE_META )
    {
        /* TODO maybe multi - access ? */
        vlc_meta_t *p_meta;
        if( !access2_Control( p_input->input.p_access,ACCESS_GET_META,&p_meta))
        {
            if( p_input->p_meta )
            {
                vlc_meta_Merge( p_input->p_meta, p_meta );
                vlc_meta_Delete( p_meta );
            }
            else
            {
                p_input->p_meta = p_meta;
            }

            UpdateMeta( p_input, VLC_FALSE );
            var_SetBool( p_input, "item-change", p_input->input.p_item->i_id );
        }
        p_access->info.i_update &= ~INPUT_UPDATE_META;
    }

    p_access->info.i_update &= ~INPUT_UPDATE_SIZE;

    /* Hmmm only works with master input */
    if( p_input->input.p_access == p_access )
    {
        int i_title_end = p_input->input.i_title_end -
            p_input->input.i_title_offset;
        int i_seekpoint_end = p_input->input.i_seekpoint_end -
            p_input->input.i_seekpoint_offset;

        if( i_title_end >= 0 && i_seekpoint_end >=0 )
        {
            if( p_access->info.i_title > i_title_end ||
                ( p_access->info.i_title == i_title_end &&
                  p_access->info.i_seekpoint > i_seekpoint_end ) ) return 0;
        }
        else if( i_seekpoint_end >=0 )
        {
            if( p_access->info.i_seekpoint > i_seekpoint_end ) return 0;
        }
        else if( i_title_end >= 0 )
        {
            if( p_access->info.i_title > i_title_end ) return 0;
        }
    }

    return 1;
}

/*****************************************************************************
 * UpdateMeta:
 *****************************************************************************/
static int  UpdateMeta( input_thread_t *p_input, vlc_bool_t b_quick )
{
    vlc_meta_t *p_meta = p_input->p_meta;
    int i;

    if( !p_meta || p_meta->i_meta == 0 )
        return VLC_SUCCESS;

    if( !b_quick ) msg_Dbg( p_input, "meta information:" );
    for( i = 0; i < p_meta->i_meta; i++ )
    {
        if( !b_quick )
            msg_Dbg( p_input, "  - '%s' = '%s'",
                     _(p_meta->name[i]), p_meta->value[i] );

        if( !strcmp(p_meta->name[i], VLC_META_TITLE) && p_meta->value[i] &&
            !p_input->input.p_item->b_fixed_name )
            input_Control( p_input, INPUT_SET_NAME, p_meta->value[i] );

        if( !strcmp( p_meta->name[i], VLC_META_AUTHOR ) )
            input_Control( p_input, INPUT_ADD_INFO, _("General"),
                           _("Author"), p_meta->value[i] );

        input_Control( p_input, INPUT_ADD_INFO, _("Meta-information"),
                      _(p_meta->name[i]), "%s", p_meta->value[i] );
    }

    for( i = 0; i < p_meta->i_track; i++ )
    {
        vlc_meta_t *tk = p_meta->track[i];
        int j;

        if( tk->i_meta > 0 )
        {
            char *psz_cat = malloc( strlen(_("Stream")) + 10 );

            msg_Dbg( p_input, "  - track[%d]:", i );

            sprintf( psz_cat, "%s %d", _("Stream"), i );
            for( j = 0; j < tk->i_meta; j++ )
            {
                msg_Dbg( p_input, "     - '%s' = '%s'", _(tk->name[j]),
                         tk->value[j] );

                input_Control( p_input, INPUT_ADD_INFO, psz_cat,
                               _(tk->name[j]), "%s", tk->value[j] );
            }
        }
    }

    if( p_input->p_sout && p_input->p_sout->p_meta == NULL )
    {
        p_input->p_sout->p_meta = vlc_meta_Duplicate( p_meta );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * UpdateItemLength:
 *****************************************************************************/
static void UpdateItemLength( input_thread_t *p_input, int64_t i_length,
                              vlc_bool_t b_quick )
{
    playlist_t *p_playlist;
    char psz_buffer[MSTRTIME_MAX_SIZE];

    vlc_mutex_lock( &p_input->input.p_item->lock );
    p_input->input.p_item->i_duration = i_length;
    vlc_mutex_unlock( &p_input->input.p_item->lock );

        p_playlist = vlc_object_find( p_input, VLC_OBJECT_PLAYLIST,
                                               FIND_PARENT);
    if( p_playlist )
    {
        var_SetInteger( p_playlist, "item-change",
                        p_input->input.p_item->i_id );
        vlc_object_release( p_playlist );
    }

    input_Control( p_input, INPUT_ADD_INFO, _("General"), _("Duration"),
                   msecstotimestr( psz_buffer, i_length / 1000 ) );
}

/*****************************************************************************
 * InputSourceNew:
 *****************************************************************************/
static input_source_t *InputSourceNew( input_thread_t *p_input )
{
    input_source_t *in = (input_source_t*) malloc( sizeof( input_source_t ) );
   
    if( !in )
    {
        msg_Err( p_input, "out of memory for new input source" );
        return NULL;
    }

    in->p_item   = NULL;
    in->p_access = NULL;
    in->p_stream = NULL;
    in->p_demux  = NULL;
    in->b_title_demux = VLC_FALSE;
    in->i_title  = 0;
    in->title    = NULL;
    in->b_can_pace_control = VLC_TRUE;
    in->b_eof = VLC_FALSE;
    in->i_cr_average = 0;

    return in;
}

/*****************************************************************************
 * InputSourceInit:
 *****************************************************************************/
static int InputSourceInit( input_thread_t *p_input,
                            input_source_t *in, char *psz_mrl,
                            char *psz_forced_demux, vlc_bool_t b_quick )
{
    char *psz_dup = strdup( psz_mrl );
    char *psz_access;
    char *psz_demux;
    char *psz_path;
    char *psz_tmp;
    char *psz;
    vlc_value_t val;

    if( !in ) return VLC_EGENERIC;

    /* Split uri */
    if( !b_quick )
    {
        MRLSplit( VLC_OBJECT(p_input), psz_dup,
                  &psz_access, &psz_demux, &psz_path );

        msg_Dbg( p_input, "`%s' gives access `%s' demux `%s' path `%s'",
                 psz_mrl, psz_access, psz_demux, psz_path );

        /* Hack to allow udp://@:port syntax */
        if( !psz_access ||
            (strncmp( psz_access, "udp", 3 ) &&
             strncmp( psz_access, "rtp", 3 )) )

        /* Find optional titles and seekpoints */
        MRLSections( p_input, psz_path, &in->i_title_start, &in->i_title_end,
                     &in->i_seekpoint_start, &in->i_seekpoint_end );

        if( psz_forced_demux && *psz_forced_demux )
        {
            psz_demux = psz_forced_demux;
        }
        else if( !psz_demux || *psz_demux == '\0' )
        {
            /* special hack for forcing a demuxer with --demux=module
             * (and do nothing with a list) */
            char *psz_var_demux = var_GetString( p_input, "demux" );

            if( *psz_var_demux != '\0' &&
                !strchr(psz_var_demux, ',' ) &&
                !strchr(psz_var_demux, ':' ) )
            {
                psz_demux = psz_var_demux;

                msg_Dbg( p_input, "Enforce demux ` %s'", psz_demux );
            }
            else if( psz_var_demux )
            {
                free( psz_var_demux );
            }
        }

        /* Try access_demux if no demux given */
        if( *psz_demux == '\0' )
        {
            in->p_demux = demux2_New( p_input, psz_access, psz_demux, psz_path,
                                      NULL, p_input->p_es_out, VLC_FALSE );
        }
    }
    else
    {
        psz_path = psz_mrl;
        msg_Dbg( p_input, "trying to preparse %s",  psz_path );
        psz_demux = strdup( "" );
        psz_access = strdup( "file" );
    }

    if( in->p_demux )
    {
        int64_t i_pts_delay;

        /* Get infos from access_demux */
        demux2_Control( in->p_demux,
                        DEMUX_GET_PTS_DELAY, &i_pts_delay );
        p_input->i_pts_delay = __MAX( p_input->i_pts_delay, i_pts_delay );

        in->b_title_demux = VLC_TRUE;
        if( demux2_Control( in->p_demux, DEMUX_GET_TITLE_INFO,
                            &in->title, &in->i_title,
                            &in->i_title_offset, &in->i_seekpoint_offset ) )
        {
            in->i_title = 0;
            in->title   = NULL;
        }
        demux2_Control( in->p_demux, DEMUX_CAN_CONTROL_PACE,
                        &in->b_can_pace_control );
        demux2_Control( in->p_demux, DEMUX_CAN_PAUSE,
                        &in->b_can_pause );

        /* FIXME todo
        demux2_Control( in->p_demux, DEMUX_CAN_SEEK,
                        &val.b_bool );
        */
    }
    else
    {
        int64_t i_pts_delay;

        /* Now try a real access */
        in->p_access = access2_New( p_input, psz_access, psz_demux, psz_path,
                                    b_quick );

        /* Access failed, URL encoded ? */
        if( in->p_access == NULL && strchr( psz_path, '%' ) )
        {
            DecodeUrl( psz_path );

            msg_Dbg( p_input, "retrying with access `%s' demux `%s' path `%s'",
                     psz_access, psz_demux, psz_path );

            in->p_access = access2_New( p_input,
                                        psz_access, psz_demux, psz_path,
                                        b_quick );
        }
#ifndef WIN32      /* Remove this gross hack from the win32 build as colons
                        * are forbidden in filenames on Win32. */

        /* Maybe we got something like: /Volumes/toto:titi/gabu.mpg */
        if( in->p_access == NULL &&
            *psz_access == '\0' && ( *psz_demux || *psz_path ) )
        {
            if( psz_dup ) free( psz_dup );
            psz_dup = strdup( psz_mrl );
            psz_access = "";
            psz_demux = "";
            psz_path = psz_dup;

            in->p_access = access2_New( p_input,
                                        psz_access, psz_demux, psz_path,
                                        b_quick );
        }
#endif

        if( in->p_access == NULL )
        {
            msg_Err( p_input, "no suitable access module for `%s'", psz_mrl );
            intf_UserFatal( VLC_OBJECT( p_input),
                            _("Errors"),"Unable to open '%s'", psz_mrl );
            goto error;
        }

        /* */
        psz_tmp = psz = var_GetString( p_input, "access-filter" );
        while( psz && *psz )
        {
            access_t *p_access = in->p_access;
            char *end = strchr( psz, ':' );

            if( end )
                *end++ = '\0';

            in->p_access = access2_FilterNew( in->p_access, psz );
            if( in->p_access == NULL )
            {
                in->p_access = p_access;
                msg_Warn( p_input, "failed to insert access filter %s",
                          psz );
            }

            psz = end;
        }
        if( psz_tmp ) free( psz_tmp );

        /* Get infos from access */
        if( !b_quick )
        {
            access2_Control( in->p_access,
                             ACCESS_GET_PTS_DELAY, &i_pts_delay );
            p_input->i_pts_delay = __MAX( p_input->i_pts_delay, i_pts_delay );

            in->b_title_demux = VLC_FALSE;
            if( access2_Control( in->p_access, ACCESS_GET_TITLE_INFO,
                                 &in->title, &in->i_title,
                                &in->i_title_offset, &in->i_seekpoint_offset ) )

            {
                in->i_title = 0;
                in->title   = NULL;
            }
            access2_Control( in->p_access, ACCESS_CAN_CONTROL_PACE,
                             &in->b_can_pace_control );
            access2_Control( in->p_access, ACCESS_CAN_PAUSE,
                             &in->b_can_pause );
            access2_Control( in->p_access, ACCESS_CAN_SEEK,
                             &val.b_bool );
            var_Set( p_input, "seekable", val );
        }

        /* Create the stream_t */
        in->p_stream = stream_AccessNew( in->p_access, b_quick );
        if( in->p_stream == NULL )
        {
            msg_Warn( p_input, "cannot create a stream_t from access" );
            goto error;
        }

        /* Open a demuxer */
        if( *psz_demux == '\0' && *in->p_access->psz_demux )
        {
            psz_demux = in->p_access->psz_demux;
        }
        in->p_demux = demux2_New( p_input, psz_access, psz_demux, psz_path,
                                  in->p_stream, p_input->p_es_out, b_quick );
        if( in->p_demux == NULL )
        {
            msg_Err( p_input, "no suitable demux module for `%s/%s://%s'",
                     psz_access, psz_demux, psz_path );
            intf_UserFatal( VLC_OBJECT( p_input), _("Errors"),
                            "Unrecognized format for '%s'", psz_mrl );
            goto error;
        }

        /* TODO get title from demux */
        if( !b_quick && in->i_title <= 0 )
        {
            if( demux2_Control( in->p_demux, DEMUX_GET_TITLE_INFO,
                                &in->title, &in->i_title,
                                &in->i_title_offset, &in->i_seekpoint_offset ))
            {
                in->i_title = 0;
                in->title   = NULL;
            }
            else
            {
                in->b_title_demux = VLC_TRUE;
            }
        }
    }

    if( var_GetInteger( p_input, "clock-synchro" ) != -1 )
        in->b_can_pace_control = !var_GetInteger( p_input, "clock-synchro" );

    if( psz_dup ) free( psz_dup );
    return VLC_SUCCESS;

error:
    if( in->p_demux )
        demux2_Delete( in->p_demux );

    if( in->p_stream )
        stream_Delete( in->p_stream );

    if( in->p_access )
        access2_Delete( in->p_access );
    if( psz_dup ) free( psz_dup );

    return VLC_EGENERIC;
}

/*****************************************************************************
 * InputSourceClean:
 *****************************************************************************/
static void InputSourceClean( input_thread_t *p_input, input_source_t *in )
{
    if( in->p_demux )
        demux2_Delete( in->p_demux );

    if( in->p_stream )
        stream_Delete( in->p_stream );

    if( in->p_access )
        access2_Delete( in->p_access );

    if( in->i_title > 0 )
    {
        int i;
        for( i = 0; i < in->i_title; i++ )
        {
            vlc_input_title_Delete( in->title[i] );
        }
        free( in->title );
    }
}

static void SlaveDemux( input_thread_t *p_input )
{
    int64_t i_time;
    int i;
    
    if( demux2_Control( p_input->input.p_demux, DEMUX_GET_TIME, &i_time ) )
    {
        msg_Err( p_input, "demux doesn't like DEMUX_GET_TIME" );
        return;
    }

    for( i = 0; i < p_input->i_slave; i++ )
    {
        input_source_t *in = p_input->slave[i];
        int i_ret = 1;

        if( in->b_eof )
            continue;

        if( demux2_Control( in->p_demux, DEMUX_SET_NEXT_DEMUX_TIME, i_time ) )
        {
            for( ;; )
            {
                int64_t i_stime;
                if( demux2_Control( in->p_demux, DEMUX_GET_TIME, &i_stime ) )
                {
                    msg_Err( p_input, "slave[%d] doesn't like "
                             "DEMUX_GET_TIME -> EOF", i );
                    i_ret = 0;
                    break;
                }

                if( i_stime >= i_time )
                    break;

                if( ( i_ret = in->p_demux->pf_demux( in->p_demux ) ) <= 0 )
                    break;
            }
        }
        else
        {
            i_ret = in->p_demux->pf_demux( in->p_demux );
        }

        if( i_ret <= 0 )
        {
            msg_Dbg( p_input, "slave %d EOF", i );
            in->b_eof = VLC_TRUE;
        }
    }
}

static void SlaveSeek( input_thread_t *p_input )
{
    int64_t i_time;
    int i;

    if( demux2_Control( p_input->input.p_demux, DEMUX_GET_TIME, &i_time ) )
    {
        msg_Err( p_input, "demux doesn't like DEMUX_GET_TIME" );
        return;
    }

    for( i = 0; i < p_input->i_slave; i++ )
    {
        input_source_t *in = p_input->slave[i];

        if( demux2_Control( in->p_demux, DEMUX_SET_TIME, i_time ) )
        {
            msg_Err( p_input, "seek failed for slave %d -> EOF", i );
            in->b_eof = VLC_TRUE;
        }
    }
}
/*****************************************************************************
 * InputMetaUser:
 *****************************************************************************/
static vlc_meta_t *InputMetaUser( input_thread_t *p_input )
{
    vlc_meta_t *p_meta;
    vlc_value_t val;

    if( ( p_meta = vlc_meta_New() ) == NULL )
        return NULL;

    /* Get meta information from user */
#define GET_META( c, s ) \
    var_Get( p_input, (s), &val );  \
    if( *val.psz_string )       \
        vlc_meta_Add( p_meta, c, val.psz_string ); \
    free( val.psz_string )

    GET_META( VLC_META_TITLE, "meta-title" );
    GET_META( VLC_META_AUTHOR, "meta-author" );
    GET_META( VLC_META_ARTIST, "meta-artist" );
    GET_META( VLC_META_GENRE, "meta-genre" );
    GET_META( VLC_META_COPYRIGHT, "meta-copyright" );
    GET_META( VLC_META_DESCRIPTION, "meta-description" );
    GET_META( VLC_META_DATE, "meta-date" );
    GET_META( VLC_META_URL, "meta-url" );
#undef GET_META

    return p_meta;
}

/*****************************************************************************
 * DecodeUrl: decode a given encoded url
 *****************************************************************************/
static void DecodeUrl( char *psz )
{
    char *dup = strdup( psz );
    char *p = dup;

    while( *p )
    {
        if( *p == '%' )
        {
            char val[3];
            p++;
            if( !*p )
            {
                break;
            }

            val[0] = *p++;
            val[1] = *p++;
            val[2] = '\0';

            *psz++ = strtol( val, NULL, 16 );
        }
        else if( *p == '+' )
        {
            *psz++ = ' ';
            p++;
        }
        else
        {
            *psz++ = *p++;
        }
    }
    if( psz ) *psz++  ='\0';
    if( dup ) free( dup );
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

    /* FIXME: :programs should be handled generically */
    if( !strcmp( psz_name, "programs" ) )
        i_type = VLC_VAR_LIST;
    else
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
        val.i_int = strtol( psz_value, NULL, 0 );
        break;

    case VLC_VAR_FLOAT:
        val.f_float = atof( psz_value );
        break;

    case VLC_VAR_STRING:
    case VLC_VAR_MODULE:
    case VLC_VAR_FILE:
    case VLC_VAR_DIRECTORY:
        val.psz_string = psz_value;
        break;

    case VLC_VAR_LIST:
    {
        char *psz_orig, *psz_var;
        vlc_list_t *p_list = malloc(sizeof(vlc_list_t));
        val.p_list = p_list;
        p_list->i_count = 0;

        psz_var = psz_orig = strdup(psz_value);
        while( psz_var && *psz_var )
        {
            char *psz_item = psz_var;
            vlc_value_t val2;
            while( *psz_var && *psz_var != ',' ) psz_var++;
            if( *psz_var == ',' )
            {
                *psz_var = '\0';
                psz_var++;
            }
            val2.i_int = strtol( psz_item, NULL, 0 );
            INSERT_ELEM( p_list->p_values, p_list->i_count,
                         p_list->i_count, val2 );
            /* p_list->i_count is incremented twice by INSERT_ELEM */
            p_list->i_count--;
            INSERT_ELEM( p_list->pi_types, p_list->i_count,
                         p_list->i_count, VLC_VAR_INTEGER );
        }
        if( psz_orig ) free( psz_orig );
        break;
    }

    default:
        goto cleanup;
        break;
    }

    var_Set( p_input, psz_name, val );

    msg_Dbg( p_input, "set input option: %s to %s", psz_name,
             psz_value ? psz_value : ( val.b_bool ? "true" : "false") );

  cleanup:
    if( psz_name ) free( psz_name );
    return;
}

/*****************************************************************************
 * MRLSplit: parse the access, demux and url part of the
 *           Media Resource Locator.
 *****************************************************************************/
void MRLSplit( vlc_object_t *p_input, char *psz_dup,
               char **ppsz_access, char **ppsz_demux, char **ppsz_path )
{
    char *psz_access = NULL;
    char *psz_demux  = NULL;
    char *psz_path   = NULL;
    char *psz, *psz_check;

    psz = strchr( psz_dup, ':' );

    /* '@' not allowed in access/demux part */
    psz_check = strchr( psz_dup, '@' );
    if( psz_check && psz_check < psz ) psz = 0;

#if defined( WIN32 ) || defined( UNDER_CE )
    if( psz - psz_dup == 1 )
    {
        msg_Warn( p_input, "drive letter %c: found in source", *psz_dup );
        psz_path = psz_dup;
    }
    else
#endif

    if( psz )
    {
        *psz++ = '\0';
        if( psz[0] == '/' && psz[1] == '/' ) psz += 2;

        psz_path = psz;

        psz = strchr( psz_dup, '/' );
        if( psz )
        {
            *psz++ = '\0';
            psz_demux = psz;
        }

        psz_access = psz_dup;
    }
    else
    {
        psz_path = psz_dup;
    }

    if( !psz_access ) *ppsz_access = "";
    else *ppsz_access = psz_access;

    if( !psz_demux ) *ppsz_demux = "";
    else *ppsz_demux = psz_demux;

    if( !psz_path ) *ppsz_path = "";
    else *ppsz_path = psz_path;
}

/*****************************************************************************
 * MRLSections: parse title and seekpoint info from the Media Resource Locator.
 *
 * Syntax:
 * [url][@[title-start][:chapter-start][-[title-end][:chapter-end]]]
 *****************************************************************************/
static void MRLSections( input_thread_t *p_input, char *psz_source,
                         int *pi_title_start, int *pi_title_end,
                         int *pi_chapter_start, int *pi_chapter_end )
{
    char *psz, *psz_end, *psz_next, *psz_check;

    *pi_title_start = *pi_title_end = -1;
    *pi_chapter_start = *pi_chapter_end = -1;

    /* Start by parsing titles and chapters */
    if( !psz_source || !( psz = strrchr( psz_source, '@' ) ) ) return;

    /* Check we are really dealing with a title/chapter section */
    psz_check = psz + 1;
    if( !*psz_check ) return;
    if( isdigit(*psz_check) ) strtol( psz_check, &psz_check, 0 );
    if( *psz_check != ':' && *psz_check != '-' && *psz_check ) return;
    if( *psz_check == ':' && ++psz_check )
        if( isdigit(*psz_check) ) strtol( psz_check, &psz_check, 0 );
    if( *psz_check != '-' && *psz_check ) return;
    if( *psz_check == '-' && ++psz_check )
        if( isdigit(*psz_check) ) strtol( psz_check, &psz_check, 0 );
    if( *psz_check != ':' && *psz_check ) return;
    if( *psz_check == ':' && ++psz_check )
        if( isdigit(*psz_check) ) strtol( psz_check, &psz_check, 0 );
    if( *psz_check ) return;

    /* Separate start and end */
    *psz++ = 0;
    if( ( psz_end = strchr( psz, '-' ) ) ) *psz_end++ = 0;

    /* Look for the start title */
    *pi_title_start = strtol( psz, &psz_next, 0 );
    if( !*pi_title_start && psz == psz_next ) *pi_title_start = -1;
    *pi_title_end = *pi_title_start;
    psz = psz_next;

    /* Look for the start chapter */
    if( *psz ) psz++;
    *pi_chapter_start = strtol( psz, &psz_next, 0 );
    if( !*pi_chapter_start && psz == psz_next ) *pi_chapter_start = -1;
    *pi_chapter_end = *pi_chapter_start;

    if( psz_end )
    {
        /* Look for the end title */
        *pi_title_end = strtol( psz_end, &psz_next, 0 );
        if( !*pi_title_end && psz_end == psz_next ) *pi_title_end = -1;
        psz_end = psz_next;

        /* Look for the end chapter */
        if( *psz_end ) psz_end++;
        *pi_chapter_end = strtol( psz_end, &psz_next, 0 );
        if( !*pi_chapter_end && psz_end == psz_next ) *pi_chapter_end = -1;
    }

    msg_Dbg( p_input, "source=`%s' title=%d/%d seekpoint=%d/%d",
             psz_source, *pi_title_start, *pi_chapter_start,
             *pi_title_end, *pi_chapter_end );
}


/***********************************************************************
 * Info management functions
 ***********************************************************************/
/**
 * Get a info item from a given category in a given input item.
 *
 * \param p_i The input item to get info from
 * \param psz_cat String representing the category for the info
 * \param psz_name String representing the name of the desired info
 * \return A pointer to the string with the given info if found, or an
 *         empty string otherwise. The caller should free the returned
 *         pointer.
 */
char *vlc_input_item_GetInfo( input_item_t *p_i,
                              const char *psz_cat,
                              const char *psz_name )
{
    int i,j;

    vlc_mutex_lock( &p_i->lock );

    for( i = 0 ; i< p_i->i_categories  ; i++ )
    {
        info_category_t *p_cat = p_i->pp_categories[i];

        if( !psz_cat || strcmp( p_cat->psz_name, psz_cat ) )
            continue;

        for( j = 0; j < p_cat->i_infos ; j++ )
        {
            if( !strcmp( p_cat->pp_infos[j]->psz_name, psz_name ) )
            {
                char *psz_ret = strdup( p_cat->pp_infos[j]->psz_value );
                vlc_mutex_unlock( &p_i->lock );
                return psz_ret;
            }
        }
    }
    vlc_mutex_unlock( &p_i->lock );
    return strdup( "" );
}

int vlc_input_item_AddInfo( input_item_t *p_i,
                            const char *psz_cat,
                            const char *psz_name,
                            const char *psz_format, ... )
{
    va_list args;
    int i;
    info_t *p_info = NULL;
    info_category_t *p_cat = NULL ;

    vlc_mutex_lock( &p_i->lock );

    for( i = 0 ; i < p_i->i_categories ; i ++ )
    {
        if( !strcmp( p_i->pp_categories[i]->psz_name, psz_cat ) )
        {
            p_cat = p_i->pp_categories[i];
            break;
        }
    }
    if( !p_cat )
    {
        if( !(p_cat = (info_category_t *)malloc( sizeof(info_category_t) )) )
        {
            vlc_mutex_unlock( &p_i->lock );
            return VLC_EGENERIC;
        }
        p_cat->psz_name = strdup( psz_cat );
        p_cat->i_infos = 0;
        p_cat->pp_infos = 0;
        INSERT_ELEM( p_i->pp_categories, p_i->i_categories, p_i->i_categories,
                     p_cat );
    }

    for( i = 0; i< p_cat->i_infos; i++ )
    {
        if( !strcmp( p_cat->pp_infos[i]->psz_name, psz_name ) )
        {
            p_info = p_cat->pp_infos[i];
            break;
        }
    }

    if( !p_info )
    {
        if( ( p_info = (info_t *)malloc( sizeof( info_t ) ) ) == NULL )
        {
            vlc_mutex_unlock( &p_i->lock );
            return VLC_EGENERIC;
        }
        INSERT_ELEM( p_cat->pp_infos, p_cat->i_infos, p_cat->i_infos, p_info );
        p_info->psz_name = strdup( psz_name );
    }
    else
    {
        if( p_info->psz_value ) free( p_info->psz_value );
    }

    va_start( args, psz_format );
    vasprintf( &p_info->psz_value, psz_format, args);
    va_end( args );

    vlc_mutex_unlock( &p_i->lock );

    return VLC_SUCCESS;
}

