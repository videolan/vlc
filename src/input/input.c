/*****************************************************************************
 * input.c: input thread
 *****************************************************************************
 * Copyright (C) 1998-2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
 *          Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <limits.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

#include "input_internal.h"
#include "event.h"
#include "es_out.h"
#include "es_out_timeshift.h"
#include "access.h"
#include "demux.h"
#include "stream.h"
#include "item.h"
#include "resource.h"

#include <vlc_sout.h>
#include "../stream_output/stream_output.h"

#include <vlc_dialog.h>
#include <vlc_url.h>
#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_strings.h>
#include <vlc_modules.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void Destructor( input_thread_t * p_input );

static  void *Run            ( void * );

static input_thread_t * Create  ( vlc_object_t *, input_item_t *,
                                  const char *, bool, input_resource_t * );
static  int             Init    ( input_thread_t *p_input );
static void             End     ( input_thread_t *p_input );
static void             MainLoop( input_thread_t *p_input, bool b_interactive );

static void ObjectKillChildrens( input_thread_t *, vlc_object_t * );

static inline int ControlPop( input_thread_t *, int *, vlc_value_t *, mtime_t i_deadline, bool b_postpone_seek );
static void       ControlRelease( int i_type, vlc_value_t val );
static bool       ControlIsSeekRequest( int i_type );
static bool       Control( input_thread_t *, int, vlc_value_t );

static int  UpdateTitleSeekpointFromAccess( input_thread_t * );
static void UpdateGenericFromAccess( input_thread_t * );

static int  UpdateTitleSeekpointFromDemux( input_thread_t * );
static void UpdateGenericFromDemux( input_thread_t * );

static void MRLSections( const char *, int *, int *, int *, int *);

static input_source_t *InputSourceNew( input_thread_t *);
static int  InputSourceInit( input_thread_t *, input_source_t *,
                             const char *, const char *psz_forced_demux,
                             bool b_in_can_fail );
static void InputSourceClean( input_source_t * );
static void InputSourceMeta( input_thread_t *, input_source_t *, vlc_meta_t * );

/* TODO */
//static void InputGetAttachments( input_thread_t *, input_source_t * );
static void SlaveDemux( input_thread_t *p_input, bool *pb_demux_polled );
static void SlaveSeek( input_thread_t *p_input );

static void InputMetaUser( input_thread_t *p_input, vlc_meta_t *p_meta );
static void InputUpdateMeta( input_thread_t *p_input, vlc_meta_t *p_meta );
static void InputGetExtraFiles( input_thread_t *p_input,
                                int *pi_list, char ***pppsz_list,
                                const char *psz_access, const char *psz_path );

static void AppendAttachment( int *pi_attachment, input_attachment_t ***ppp_attachment,
                              int i_new, input_attachment_t **pp_new );

enum {
    SUB_NOFLAG = 0x00,
    SUB_FORCED = 0x01,
    SUB_CANFAIL = 0x02,
};
static void SubtitleAdd( input_thread_t *p_input, char *psz_subtitle, unsigned i_flags );

static void input_ChangeState( input_thread_t *p_input, int i_state ); /* TODO fix name */

#undef input_Create
/**
 * Create a new input_thread_t.
 *
 * You need to call input_Start on it when you are done
 * adding callback on the variables/events you want to monitor.
 *
 * \param p_parent a vlc_object
 * \param p_item an input item
 * \param psz_log an optional prefix for this input logs
 * \param p_resource an optional input ressource
 * \return a pointer to the spawned input thread
 */
input_thread_t *input_Create( vlc_object_t *p_parent,
                              input_item_t *p_item,
                              const char *psz_log, input_resource_t *p_resource )
{
    return Create( p_parent, p_item, psz_log, false, p_resource );
}

#undef input_CreateAndStart
/**
 * Create a new input_thread_t and start it.
 *
 * Provided for convenience.
 *
 * \see input_Create
 */
input_thread_t *input_CreateAndStart( vlc_object_t *p_parent,
                                      input_item_t *p_item, const char *psz_log )
{
    input_thread_t *p_input = input_Create( p_parent, p_item, psz_log, NULL );

    if( input_Start( p_input ) )
    {
        vlc_object_release( p_input );
        return NULL;
    }
    return p_input;
}

#undef input_Read
/**
 * Initialize an input thread and run it until it stops by itself.
 *
 * \param p_parent a vlc_object
 * \param p_item an input item
 * \return an error code, VLC_SUCCESS on success
 */
int input_Read( vlc_object_t *p_parent, input_item_t *p_item )
{
    input_thread_t *p_input = Create( p_parent, p_item, NULL, false, NULL );
    if( !p_input )
        return VLC_EGENERIC;

    if( !Init( p_input ) )
    {
        MainLoop( p_input, false );
        End( p_input );
    }

    vlc_object_release( p_input );
    return VLC_SUCCESS;
}

/**
 * Initialize an input and initialize it to preparse the item
 * This function is blocking. It will only accept parsing regular files.
 *
 * \param p_parent a vlc_object_t
 * \param p_item an input item
 * \return VLC_SUCCESS or an error
 */
int input_Preparse( vlc_object_t *p_parent, input_item_t *p_item )
{
    input_thread_t *p_input;

    /* Allocate descriptor */
    p_input = Create( p_parent, p_item, NULL, true, NULL );
    if( !p_input )
        return VLC_EGENERIC;

    if( !Init( p_input ) )
        End( p_input );

    vlc_object_release( p_input );

    return VLC_SUCCESS;
}

/**
 * Start a input_thread_t created by input_Create.
 *
 * You must not start an already running input_thread_t.
 *
 * \param the input thread to start
 */
int input_Start( input_thread_t *p_input )
{
    /* Create thread and wait for its readiness. */
    p_input->p->is_running = !vlc_clone( &p_input->p->thread,
                                         Run, p_input, VLC_THREAD_PRIORITY_INPUT );
    if( !p_input->p->is_running )
    {
        input_ChangeState( p_input, ERROR_S );
        msg_Err( p_input, "cannot create input thread" );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/**
 * Request a running input thread to stop and die
 *
 * b_abort must be true when a user stop is requested and not because you have
 * detected an error or an eof. It will be used to properly send the
 * INPUT_EVENT_ABORT event.
 *
 * \param p_input the input thread to stop
 * \param b_abort true if the input has been aborted by a user request
 */
void input_Stop( input_thread_t *p_input, bool b_abort )
{
    /* Set die for input and ALL of this childrens (even (grand-)grand-childrens)
     * It is needed here even if it is done in INPUT_CONTROL_SET_DIE handler to
     * unlock the control loop */
    ObjectKillChildrens( p_input, VLC_OBJECT(p_input) );

    vlc_mutex_lock( &p_input->p->lock_control );
    p_input->p->b_abort |= b_abort;
    vlc_mutex_unlock( &p_input->p->lock_control );

    input_ControlPush( p_input, INPUT_CONTROL_SET_DIE, NULL );
}

void input_Join( input_thread_t *p_input )
{
    if( p_input->p->is_running )
        vlc_join( p_input->p->thread, NULL );
}

void input_Release( input_thread_t *p_input )
{
    vlc_object_release( p_input );
}

/**
 * Close an input
 *
 * It does not call input_Stop itself.
 */
void input_Close( input_thread_t *p_input )
{
    input_Join( p_input );
    input_Release( p_input );
}

/**
 * Get the item from an input thread
 * FIXME it does not increase ref count of the item.
 * if it is used after p_input is destroyed nothing prevent it from
 * being freed.
 */
input_item_t *input_GetItem( input_thread_t *p_input )
{
    assert( p_input && p_input->p );
    return p_input->p->p_item;
}

/*****************************************************************************
 * ObjectKillChildrens
 *****************************************************************************/
static void ObjectKillChildrens( input_thread_t *p_input, vlc_object_t *p_obj )
{
    vlc_list_t *p_list;

    /* FIXME ObjectKillChildrens seems a very bad idea in fact */
    if( p_obj == VLC_OBJECT(p_input->p->p_sout) )
        return;

    vlc_object_kill( p_obj );

    p_list = vlc_list_children( p_obj );
    for( int i = 0; i < p_list->i_count; i++ )
        ObjectKillChildrens( p_input, p_list->p_values[i].p_object );
    vlc_list_release( p_list );
}

/*****************************************************************************
 * This function creates a new input, and returns a pointer
 * to its description. On error, it returns NULL.
 *
 * XXX Do not forget to update vlc_input.h if you add new variables.
 *****************************************************************************/
static input_thread_t *Create( vlc_object_t *p_parent, input_item_t *p_item,
                               const char *psz_header, bool b_quick,
                               input_resource_t *p_resource )
{
    input_thread_t *p_input = NULL;                 /* thread descriptor */
    int i;

    /* Allocate descriptor */
    p_input = vlc_custom_create( p_parent, sizeof( *p_input ), "input" );
    if( p_input == NULL )
        return NULL;

    /* Construct a nice name for the input timer */
    char psz_timer_name[255];
    char * psz_name = input_item_GetName( p_item );
    snprintf( psz_timer_name, sizeof(psz_timer_name),
              "input launching for '%s'", psz_name );

    msg_Dbg( p_input, "Creating an input for '%s'", psz_name);

    free( psz_name );

    p_input->p = calloc( 1, sizeof( input_thread_private_t ) );
    if( !p_input->p )
        return NULL;

    /* Parse input options */
    vlc_mutex_lock( &p_item->lock );
    assert( (int)p_item->optflagc == p_item->i_options );
    for( i = 0; i < p_item->i_options; i++ )
        var_OptionParse( VLC_OBJECT(p_input), p_item->ppsz_options[i],
                         !!(p_item->optflagv[i] & VLC_INPUT_OPTION_TRUSTED) );
    vlc_mutex_unlock( &p_item->lock );

    p_input->b_preparsing = b_quick;
    p_input->psz_header = psz_header ? strdup( psz_header ) : NULL;

    /* Init Common fields */
    p_input->b_eof = false;
    p_input->p->b_can_pace_control = true;
    p_input->p->i_start = 0;
    p_input->p->i_time  = 0;
    p_input->p->i_stop  = 0;
    p_input->p->i_run   = 0;
    p_input->p->i_title = 0;
    p_input->p->title = NULL;
    p_input->p->i_title_offset = p_input->p->i_seekpoint_offset = 0;
    p_input->p->i_state = INIT_S;
    p_input->p->i_rate = INPUT_RATE_DEFAULT;
    p_input->p->b_recording = false;
    memset( &p_input->p->bookmark, 0, sizeof(p_input->p->bookmark) );
    TAB_INIT( p_input->p->i_bookmark, p_input->p->pp_bookmark );
    TAB_INIT( p_input->p->i_attachment, p_input->p->attachment );
    p_input->p->p_sout   = NULL;
    p_input->p->b_out_pace_control = false;

    vlc_gc_incref( p_item ); /* Released in Destructor() */
    p_input->p->p_item = p_item;

    /* Init Input fields */
    p_input->p->input.p_access = NULL;
    p_input->p->input.p_stream = NULL;
    p_input->p->input.p_demux  = NULL;
    p_input->p->input.b_title_demux = false;
    p_input->p->input.i_title  = 0;
    p_input->p->input.title    = NULL;
    p_input->p->input.i_title_offset = p_input->p->input.i_seekpoint_offset = 0;
    p_input->p->input.b_can_pace_control = true;
    p_input->p->input.b_can_rate_control = true;
    p_input->p->input.b_rescale_ts = true;
    p_input->p->input.b_eof = false;

    vlc_mutex_lock( &p_item->lock );

    if( !p_item->p_stats )
        p_item->p_stats = stats_NewInputStats( p_input );
    vlc_mutex_unlock( &p_item->lock );

    /* No slave */
    p_input->p->i_slave = 0;
    p_input->p->slave   = NULL;

    /* */
    if( p_resource )
    {
        p_input->p->p_resource_private = NULL;
        p_input->p->p_resource = input_resource_Hold( p_resource );
    }
    else
    {
        p_input->p->p_resource_private = input_resource_New( VLC_OBJECT( p_input ) );
        p_input->p->p_resource = input_resource_Hold( p_input->p->p_resource_private );
    }
    input_resource_SetInput( p_input->p->p_resource, p_input );

    /* Init control buffer */
    vlc_mutex_init( &p_input->p->lock_control );
    vlc_cond_init( &p_input->p->wait_control );
    p_input->p->i_control = 0;
    p_input->p->b_abort = false;
    p_input->p->is_running = false;

    /* Create Object Variables for private use only */
    input_ConfigVarInit( p_input );

    /* Create Objects variables for public Get and Set */
    input_ControlVarInit( p_input );

    /* */
    if( !p_input->b_preparsing )
    {
        char *psz_bookmarks = var_GetNonEmptyString( p_input, "bookmarks" );
        if( psz_bookmarks )
        {
            /* FIXME: have a common cfg parsing routine used by sout and others */
            char *psz_parser, *psz_start, *psz_end;
            psz_parser = psz_bookmarks;
            while( (psz_start = strchr( psz_parser, '{' ) ) )
            {
                 seekpoint_t *p_seekpoint;
                 char backup;
                 psz_start++;
                 psz_end = strchr( psz_start, '}' );
                 if( !psz_end ) break;
                 psz_parser = psz_end + 1;
                 backup = *psz_parser;
                 *psz_parser = 0;
                 *psz_end = ',';

                 p_seekpoint = vlc_seekpoint_New();
                 while( (psz_end = strchr( psz_start, ',' ) ) )
                 {
                     *psz_end = 0;
                     if( !strncmp( psz_start, "name=", 5 ) )
                     {
                         p_seekpoint->psz_name = strdup(psz_start + 5);
                     }
                     else if( !strncmp( psz_start, "bytes=", 6 ) )
                     {
                         p_seekpoint->i_byte_offset = atoll(psz_start + 6);
                     }
                     else if( !strncmp( psz_start, "time=", 5 ) )
                     {
                         p_seekpoint->i_time_offset = atoll(psz_start + 5) *
                                                        1000000;
                     }
                     psz_start = psz_end + 1;
                }
                msg_Dbg( p_input, "adding bookmark: %s, bytes=%"PRId64", time=%"PRId64,
                                  p_seekpoint->psz_name, p_seekpoint->i_byte_offset,
                                  p_seekpoint->i_time_offset );
                input_Control( p_input, INPUT_ADD_BOOKMARK, p_seekpoint );
                vlc_seekpoint_Delete( p_seekpoint );
                *psz_parser = backup;
            }
            free( psz_bookmarks );
        }
    }

    /* Remove 'Now playing' info as it is probably outdated */
    input_item_SetNowPlaying( p_item, NULL );
    input_SendEventMeta( p_input );

    /* */
    if( p_input->b_preparsing )
        p_input->i_flags |= OBJECT_FLAGS_QUIET | OBJECT_FLAGS_NOINTERACT;

    /* Make sure the interaction option is honored */
    if( !var_InheritBool( p_input, "interact" ) )
        p_input->i_flags |= OBJECT_FLAGS_NOINTERACT;

    /* */
    memset( &p_input->p->counters, 0, sizeof( p_input->p->counters ) );
    vlc_mutex_init( &p_input->p->counters.counters_lock );

    p_input->p->p_es_out_display = input_EsOutNew( p_input, p_input->p->i_rate );
    p_input->p->p_es_out = NULL;

    /* Set the destructor when we are sure we are initialized */
    vlc_object_set_destructor( p_input, (vlc_destructor_t)Destructor );

    return p_input;
}

/**
 * Input destructor (called when the object's refcount reaches 0).
 */
static void Destructor( input_thread_t * p_input )
{
#ifndef NDEBUG
    char * psz_name = input_item_GetName( p_input->p->p_item );
    msg_Dbg( p_input, "Destroying the input for '%s'", psz_name);
    free( psz_name );
#endif

    if( p_input->p->p_es_out_display )
        es_out_Delete( p_input->p->p_es_out_display );

    if( p_input->p->p_resource )
        input_resource_Release( p_input->p->p_resource );
    if( p_input->p->p_resource_private )
        input_resource_Release( p_input->p->p_resource_private );

    vlc_gc_decref( p_input->p->p_item );

    vlc_mutex_destroy( &p_input->p->counters.counters_lock );

    for( int i = 0; i < p_input->p->i_control; i++ )
    {
        input_control_t *p_ctrl = &p_input->p->control[i];
        ControlRelease( p_ctrl->i_type, p_ctrl->val );
    }

    vlc_cond_destroy( &p_input->p->wait_control );
    vlc_mutex_destroy( &p_input->p->lock_control );
    free( p_input->p );
}

/*****************************************************************************
 * Run: main thread loop
 * This is the "normal" thread that spawns the input processing chain,
 * reads the stream, cleans up and waits
 *****************************************************************************/
static void *Run( void *obj )
{
    input_thread_t *p_input = (input_thread_t *)obj;
    const int canc = vlc_savecancel();

    if( Init( p_input ) )
        goto exit;

    MainLoop( p_input, true ); /* FIXME it can be wrong (like with VLM) */

    /* Clean up */
    End( p_input );

exit:
    /* Tell we're dead */
    vlc_mutex_lock( &p_input->p->lock_control );
    const bool b_abort = p_input->p->b_abort;
    vlc_mutex_unlock( &p_input->p->lock_control );

    if( b_abort )
        input_SendEventAbort( p_input );
    input_SendEventDead( p_input );

    vlc_restorecancel( canc );
    return NULL;
}

/*****************************************************************************
 * Main loop: Fill buffers from access, and demux
 *****************************************************************************/

/**
 * MainLoopDemux
 * It asks the demuxer to demux some data
 */
static void MainLoopDemux( input_thread_t *p_input, bool *pb_changed, bool *pb_demux_polled, mtime_t i_start_mdate )
{
    int i_ret;

    *pb_changed = false;
    *pb_demux_polled = p_input->p->input.p_demux->pf_demux != NULL;

    if( ( p_input->p->i_stop > 0 && p_input->p->i_time >= p_input->p->i_stop ) ||
        ( p_input->p->i_run > 0 && i_start_mdate+p_input->p->i_run < mdate() ) )
        i_ret = 0; /* EOF */
    else
        i_ret = demux_Demux( p_input->p->input.p_demux );

    if( i_ret > 0 )
    {
        if( p_input->p->input.p_demux->info.i_update )
        {
            if( p_input->p->input.b_title_demux )
            {
                i_ret = UpdateTitleSeekpointFromDemux( p_input );
                *pb_changed = true;
            }
            UpdateGenericFromDemux( p_input );
        }
        else if( p_input->p->input.p_access &&
                 p_input->p->input.p_access->info.i_update )
        {
            if( !p_input->p->input.b_title_demux )
            {
                i_ret = UpdateTitleSeekpointFromAccess( p_input );
                *pb_changed = true;
            }
            UpdateGenericFromAccess( p_input );
        }
    }

    if( i_ret == 0 )    /* EOF */
    {
        msg_Dbg( p_input, "EOF reached" );
        p_input->p->input.b_eof = true;
        es_out_Eos(p_input->p->p_es_out);
    }
    else if( i_ret < 0 )
    {
        input_ChangeState( p_input, ERROR_S );
    }

    if( i_ret > 0 && p_input->p->i_slave > 0 )
    {
        bool b_demux_polled;
        SlaveDemux( p_input, &b_demux_polled );

        *pb_demux_polled |= b_demux_polled;
    }
}

static int MainLoopTryRepeat( input_thread_t *p_input, mtime_t *pi_start_mdate )
{
    int i_repeat = var_GetInteger( p_input, "input-repeat" );
    if( i_repeat == 0 )
        return VLC_EGENERIC;

    vlc_value_t val;

    msg_Dbg( p_input, "repeating the same input (%d)", i_repeat );
    if( i_repeat > 0 )
    {
        i_repeat--;
        var_SetInteger( p_input, "input-repeat", i_repeat );
    }

    /* Seek to start title/seekpoint */
    val.i_int = p_input->p->input.i_title_start -
        p_input->p->input.i_title_offset;
    if( val.i_int < 0 || val.i_int >= p_input->p->input.i_title )
        val.i_int = 0;
    input_ControlPush( p_input,
                       INPUT_CONTROL_SET_TITLE, &val );

    val.i_int = p_input->p->input.i_seekpoint_start -
        p_input->p->input.i_seekpoint_offset;
    if( val.i_int > 0 /* TODO: check upper boundary */ )
        input_ControlPush( p_input,
                           INPUT_CONTROL_SET_SEEKPOINT, &val );

    /* Seek to start position */
    if( p_input->p->i_start > 0 )
    {
        val.i_time = p_input->p->i_start;
        input_ControlPush( p_input, INPUT_CONTROL_SET_TIME, &val );
    }
    else
    {
        val.f_float = 0.0;
        input_ControlPush( p_input, INPUT_CONTROL_SET_POSITION, &val );
    }

    /* */
    *pi_start_mdate = mdate();
    return VLC_SUCCESS;
}

/**
 * MainLoopInterface
 * It update the variables used by the interfaces
 */
static void MainLoopInterface( input_thread_t *p_input )
{
    double f_position = 0.0;
    mtime_t i_time = 0;
    mtime_t i_length = 0;

    /* update input status variables */
    if( demux_Control( p_input->p->input.p_demux,
                       DEMUX_GET_POSITION, &f_position ) )
        f_position = 0.0;

    if( demux_Control( p_input->p->input.p_demux,
                       DEMUX_GET_TIME, &i_time ) )
        i_time = 0;
    p_input->p->i_time = i_time;

    if( demux_Control( p_input->p->input.p_demux,
                       DEMUX_GET_LENGTH, &i_length ) )
        i_length = 0;

    es_out_SetTimes( p_input->p->p_es_out, f_position, i_time, i_length );

    /* update current bookmark */
    vlc_mutex_lock( &p_input->p->p_item->lock );
    p_input->p->bookmark.i_time_offset = i_time;
    if( p_input->p->input.p_stream )
        p_input->p->bookmark.i_byte_offset = stream_Tell( p_input->p->input.p_stream );
    vlc_mutex_unlock( &p_input->p->p_item->lock );
}

/**
 * MainLoopStatistic
 * It updates the globals statics
 */
static void MainLoopStatistic( input_thread_t *p_input )
{
    stats_ComputeInputStats( p_input, p_input->p->p_item->p_stats );
    input_SendEventStatistics( p_input );
}

/**
 * MainLoop
 * The main input loop.
 */
static void MainLoop( input_thread_t *p_input, bool b_interactive )
{
    mtime_t i_start_mdate = mdate();
    mtime_t i_intf_update = 0;
    mtime_t i_statistic_update = 0;
    mtime_t i_last_seek_mdate = 0;
    bool b_pause_after_eof = b_interactive &&
                             var_CreateGetBool( p_input, "play-and-pause" );

    while( vlc_object_alive( p_input ) && !p_input->b_error )
    {
        bool b_force_update;
        vlc_value_t val;
        mtime_t i_current;
        mtime_t i_wakeup;
        bool b_paused;
        bool b_demux_polled;

        /* Demux data */
        b_force_update = false;
        i_wakeup = 0;
        /* FIXME if p_input->p->i_state == PAUSE_S the access/access_demux
         * is paused -> this may cause problem with some of them
         * The same problem can be seen when seeking while paused */
        b_paused = p_input->p->i_state == PAUSE_S &&
                   ( !es_out_GetBuffering( p_input->p->p_es_out ) || p_input->p->input.b_eof );

        b_demux_polled = true;
        if( !b_paused )
        {
            if( !p_input->p->input.b_eof )
            {
                MainLoopDemux( p_input, &b_force_update, &b_demux_polled, i_start_mdate );

                i_wakeup = es_out_GetWakeup( p_input->p->p_es_out );
            }
            else if( !es_out_GetEmpty( p_input->p->p_es_out ) )
            {
                msg_Dbg( p_input, "waiting decoder fifos to empty" );
                i_wakeup = mdate() + INPUT_IDLE_SLEEP;
            }
            /* Pause after eof only if the input is pausable.
             * This way we won't trigger timeshifting for nothing */
            else if( b_pause_after_eof && p_input->p->b_can_pause )
            {
                msg_Dbg( p_input, "pausing at EOF (pause after each)");
                val.i_int = PAUSE_S;
                Control( p_input, INPUT_CONTROL_SET_STATE, val );

                b_paused = true;
            }
            else
            {
                if( MainLoopTryRepeat( p_input, &i_start_mdate ) )
                    break;
                b_pause_after_eof = var_GetBool( p_input, "play-and-pause" );
            }
        }

        /* */
        do {
            mtime_t i_deadline = i_wakeup;
            if( b_paused || !b_demux_polled )
                i_deadline = __MIN( i_intf_update, i_statistic_update );

            /* Handle control */
            for( ;; )
            {
                mtime_t i_limit = i_deadline;

                /* We will postpone the execution of a seek until we have
                 * finished the ES bufferisation (postpone is limited to
                 * 125ms) */
                bool b_buffering = es_out_GetBuffering( p_input->p->p_es_out ) &&
                                   !p_input->p->input.b_eof;
                if( b_buffering )
                {
                    /* When postpone is in order, check the ES level every 20ms */
                    mtime_t i_current = mdate();
                    if( i_last_seek_mdate + INT64_C(125000) >= i_current )
                        i_limit = __MIN( i_deadline, i_current + INT64_C(20000) );
                }

                int i_type;
                if( ControlPop( p_input, &i_type, &val, i_limit, b_buffering ) )
                {
                    if( b_buffering && i_limit < i_deadline )
                        continue;
                    break;
                }

#ifndef NDEBUG
                msg_Dbg( p_input, "control type=%d", i_type );
#endif

                if( Control( p_input, i_type, val ) )
                {
                    if( ControlIsSeekRequest( i_type ) )
                        i_last_seek_mdate = mdate();
                    b_force_update = true;
                }
            }

            /* Update interface and statistics */
            i_current = mdate();
            if( i_intf_update < i_current || b_force_update )
            {
                MainLoopInterface( p_input );
                i_intf_update = i_current + INT64_C(250000);
                b_force_update = false;
            }
            if( i_statistic_update < i_current )
            {
                MainLoopStatistic( p_input );
                i_statistic_update = i_current + INT64_C(1000000);
            }

            /* Update the wakeup time */
            if( i_wakeup != 0 )
                i_wakeup = es_out_GetWakeup( p_input->p->p_es_out );
        } while( i_current < i_wakeup );
    }

    if( !p_input->b_error )
        input_ChangeState( p_input, END_S );
}

static void InitStatistics( input_thread_t * p_input )
{
    if( p_input->b_preparsing ) return;

    /* Prepare statistics */
#define INIT_COUNTER( c, compute ) p_input->p->counters.p_##c = \
 stats_CounterCreate( STATS_##compute);
    if( libvlc_stats( p_input ) )
    {
        INIT_COUNTER( read_bytes, COUNTER );
        INIT_COUNTER( read_packets, COUNTER );
        INIT_COUNTER( demux_read, COUNTER );
        INIT_COUNTER( input_bitrate, DERIVATIVE );
        INIT_COUNTER( demux_bitrate, DERIVATIVE );
        INIT_COUNTER( demux_corrupted, COUNTER );
        INIT_COUNTER( demux_discontinuity, COUNTER );
        INIT_COUNTER( played_abuffers, COUNTER );
        INIT_COUNTER( lost_abuffers, COUNTER );
        INIT_COUNTER( displayed_pictures, COUNTER );
        INIT_COUNTER( lost_pictures, COUNTER );
        INIT_COUNTER( decoded_audio, COUNTER );
        INIT_COUNTER( decoded_video, COUNTER );
        INIT_COUNTER( decoded_sub, COUNTER );
        p_input->p->counters.p_sout_send_bitrate = NULL;
        p_input->p->counters.p_sout_sent_packets = NULL;
        p_input->p->counters.p_sout_sent_bytes = NULL;
    }
}

#ifdef ENABLE_SOUT
static int InitSout( input_thread_t * p_input )
{
    if( p_input->b_preparsing )
        return VLC_SUCCESS;

    /* Find a usable sout and attach it to p_input */
    char *psz = var_GetNonEmptyString( p_input, "sout" );
    if( psz && strncasecmp( p_input->p->p_item->psz_uri, "vlc:", 4 ) )
    {
        p_input->p->p_sout  = input_resource_RequestSout( p_input->p->p_resource, NULL, psz );
        if( !p_input->p->p_sout )
        {
            input_ChangeState( p_input, ERROR_S );
            msg_Err( p_input, "cannot start stream output instance, " \
                              "aborting" );
            free( psz );
            return VLC_EGENERIC;
        }
        if( libvlc_stats( p_input ) )
        {
            INIT_COUNTER( sout_sent_packets, COUNTER );
            INIT_COUNTER( sout_sent_bytes, COUNTER );
            INIT_COUNTER( sout_send_bitrate, DERIVATIVE );
        }
    }
    else
    {
        input_resource_RequestSout( p_input->p->p_resource, NULL, NULL );
    }
    free( psz );

    return VLC_SUCCESS;
}
#endif

static void InitTitle( input_thread_t * p_input )
{
    input_source_t *p_master = &p_input->p->input;

    if( p_input->b_preparsing )
        return;

    vlc_mutex_lock( &p_input->p->p_item->lock );
    /* Create global title (from master) */
    p_input->p->i_title = p_master->i_title;
    p_input->p->title   = p_master->title;
    p_input->p->i_title_offset = p_master->i_title_offset;
    p_input->p->i_seekpoint_offset = p_master->i_seekpoint_offset;
    if( p_input->p->i_title > 0 )
    {
        /* Setup variables */
        input_ControlVarNavigation( p_input );
        input_SendEventTitle( p_input, 0 );
    }

    /* Global flag */
    p_input->p->b_can_pace_control    = p_master->b_can_pace_control;
    p_input->p->b_can_pause        = p_master->b_can_pause;
    p_input->p->b_can_rate_control = p_master->b_can_rate_control;
    vlc_mutex_unlock( &p_input->p->p_item->lock );
}

static void StartTitle( input_thread_t * p_input )
{
    vlc_value_t val;

    /* Start title/chapter */
    val.i_int = p_input->p->input.i_title_start -
                p_input->p->input.i_title_offset;
    if( val.i_int > 0 && val.i_int < p_input->p->input.i_title )
        input_ControlPush( p_input, INPUT_CONTROL_SET_TITLE, &val );

    val.i_int = p_input->p->input.i_seekpoint_start -
                p_input->p->input.i_seekpoint_offset;
    if( val.i_int > 0 /* TODO: check upper boundary */ )
        input_ControlPush( p_input, INPUT_CONTROL_SET_SEEKPOINT, &val );

    /* Start/stop/run time */
    p_input->p->i_start = (int64_t)(1000000.0
                                     * var_GetFloat( p_input, "start-time" ));
    p_input->p->i_stop  = (int64_t)(1000000.0
                                     * var_GetFloat( p_input, "stop-time" ));
    p_input->p->i_run   = (int64_t)(1000000.0
                                     * var_GetFloat( p_input, "run-time" ));
    if( p_input->p->i_run < 0 )
    {
        msg_Warn( p_input, "invalid run-time ignored" );
        p_input->p->i_run = 0;
    }

    if( p_input->p->i_start > 0 )
    {
        vlc_value_t s;

        msg_Dbg( p_input, "starting at time: %ds",
                 (int)( p_input->p->i_start / INT64_C(1000000) ) );

        s.i_time = p_input->p->i_start;
        input_ControlPush( p_input, INPUT_CONTROL_SET_TIME, &s );
    }
    if( p_input->p->i_stop > 0 && p_input->p->i_stop <= p_input->p->i_start )
    {
        msg_Warn( p_input, "invalid stop-time ignored" );
        p_input->p->i_stop = 0;
    }
    p_input->p->b_fast_seek = var_GetBool( p_input, "input-fast-seek" );
}

static void LoadSubtitles( input_thread_t *p_input )
{
    /* Load subtitles */
    /* Get fps and set it if not already set */
    const double f_fps = p_input->p->f_fps;
    if( f_fps > 1.0 )
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

    const int i_delay = var_CreateGetInteger( p_input, "sub-delay" );
    if( i_delay != 0 )
        var_SetTime( p_input, "spu-delay", (mtime_t)i_delay * 100000 );

    /* Look for and add subtitle files */
    unsigned i_flags = SUB_FORCED;

    char *psz_subtitle = var_GetNonEmptyString( p_input, "sub-file" );
    if( psz_subtitle != NULL )
    {
        msg_Dbg( p_input, "forced subtitle: %s", psz_subtitle );
        SubtitleAdd( p_input, psz_subtitle, i_flags );
        i_flags = SUB_NOFLAG;
    }

    if( var_GetBool( p_input, "sub-autodetect-file" ) )
    {
        char *psz_autopath = var_GetNonEmptyString( p_input, "sub-autodetect-path" );
        char **ppsz_subs = subtitles_Detect( p_input, psz_autopath,
                                             p_input->p->p_item->psz_uri );
        free( psz_autopath );

        for( int i = 0; ppsz_subs && ppsz_subs[i]; i++ )
        {
            if( !psz_subtitle || strcmp( psz_subtitle, ppsz_subs[i] ) )
            {
                i_flags |= SUB_CANFAIL;
                SubtitleAdd( p_input, ppsz_subs[i], i_flags );
                i_flags = SUB_NOFLAG;
            }

            free( ppsz_subs[i] );
        }
        free( ppsz_subs );
    }
    free( psz_subtitle );

    /* Load subtitles from attachments */
    int i_attachment = 0;
    input_attachment_t **pp_attachment = NULL;

    vlc_mutex_lock( &p_input->p->p_item->lock );
    for( int i = 0; i < p_input->p->i_attachment; i++ )
    {
        const input_attachment_t *a = p_input->p->attachment[i];
        if( !strcmp( a->psz_mime, "application/x-srt" ) )
            TAB_APPEND( i_attachment, pp_attachment,
                        vlc_input_attachment_New( a->psz_name, NULL,
                                                  a->psz_description, NULL, 0 ) );
    }
    vlc_mutex_unlock( &p_input->p->p_item->lock );

    if( i_attachment > 0 )
        var_Create( p_input, "sub-description", VLC_VAR_STRING );
    for( int i = 0; i < i_attachment; i++ )
    {
        input_attachment_t *a = pp_attachment[i];
        if( !a )
            continue;
        char *psz_mrl;
        if( a->psz_name[i] &&
            asprintf( &psz_mrl, "attachment://%s", a->psz_name ) >= 0 )
        {
            var_SetString( p_input, "sub-description", a->psz_description ? a->psz_description : "");

            SubtitleAdd( p_input, psz_mrl, i_flags );

            i_flags = SUB_NOFLAG;
            free( psz_mrl );
        }
        vlc_input_attachment_Delete( a );
    }
    free( pp_attachment );
    if( i_attachment > 0 )
        var_Destroy( p_input, "sub-description" );
}

static void LoadSlaves( input_thread_t *p_input )
{
    char *psz = var_GetNonEmptyString( p_input, "input-slave" );
    if( !psz )
        return;

    char *psz_org = psz;
    while( psz && *psz )
    {
        while( *psz == ' ' || *psz == '#' )
            psz++;

        char *psz_delim = strchr( psz, '#' );
        if( psz_delim )
            *psz_delim++ = '\0';

        if( *psz == 0 )
            break;

        char *uri = make_URI( psz, NULL );
        psz = psz_delim;
        if( uri == NULL )
            continue;
        msg_Dbg( p_input, "adding slave input '%s'", uri );

        input_source_t *p_slave = InputSourceNew( p_input );
        if( p_slave && !InputSourceInit( p_input, p_slave, uri, NULL, false ) )
            TAB_APPEND( p_input->p->i_slave, p_input->p->slave, p_slave );
        else
            free( p_slave );
        free( uri );
    }
    free( psz_org );
}

static void UpdatePtsDelay( input_thread_t *p_input )
{
    input_thread_private_t *p_sys = p_input->p;

    /* Get max pts delay from input source */
    mtime_t i_pts_delay = p_sys->input.i_pts_delay;
    for( int i = 0; i < p_sys->i_slave; i++ )
        i_pts_delay = __MAX( i_pts_delay, p_sys->slave[i]->i_pts_delay );

    if( i_pts_delay < 0 )
        i_pts_delay = 0;

    /* Take care of audio/spu delay */
    const mtime_t i_audio_delay = var_GetTime( p_input, "audio-delay" );
    const mtime_t i_spu_delay   = var_GetTime( p_input, "spu-delay" );
    const mtime_t i_extra_delay = __MIN( i_audio_delay, i_spu_delay );
    if( i_extra_delay < 0 )
        i_pts_delay -= i_extra_delay;

    /* Update cr_average depending on the caching */
    const int i_cr_average = var_GetInteger( p_input, "cr-average" ) * i_pts_delay / DEFAULT_PTS_DELAY;

    /* */
    es_out_SetDelay( p_input->p->p_es_out_display, AUDIO_ES, i_audio_delay );
    es_out_SetDelay( p_input->p->p_es_out_display, SPU_ES, i_spu_delay );
    es_out_SetJitter( p_input->p->p_es_out, i_pts_delay, 0, i_cr_average );
}

static void InitPrograms( input_thread_t * p_input )
{
    int i_es_out_mode;
    vlc_list_t list;

    /* Compute correct pts_delay */
    UpdatePtsDelay( p_input );

    /* Set up es_out */
    i_es_out_mode = ES_OUT_MODE_AUTO;
    if( p_input->p->p_sout )
    {
        char *prgms;

        if( var_GetBool( p_input, "sout-all" ) )
        {
            i_es_out_mode = ES_OUT_MODE_ALL;
        }
        else
        if( (prgms = var_GetNonEmptyString( p_input, "programs" )) != NULL )
        {
            char *buf;

            TAB_INIT( list.i_count, list.p_values );
            for( const char *prgm = strtok_r( prgms, ",", &buf );
                 prgm != NULL;
                 prgm = strtok_r( NULL, ",", &buf ) )
            {
                vlc_value_t val = { .i_int = atoi( prgm ) };
                INSERT_ELEM( list.p_values, list.i_count, list.i_count, val );
            }

            if( list.i_count > 0 )
                i_es_out_mode = ES_OUT_MODE_PARTIAL;
                /* Note : we should remove the "program" callback. */

            free( prgms );
        }
    }
    es_out_SetMode( p_input->p->p_es_out, i_es_out_mode );

    /* Inform the demuxer about waited group (needed only for DVB) */
    if( i_es_out_mode == ES_OUT_MODE_ALL )
    {
        demux_Control( p_input->p->input.p_demux, DEMUX_SET_GROUP, -1, NULL );
    }
    else if( i_es_out_mode == ES_OUT_MODE_PARTIAL )
    {
        demux_Control( p_input->p->input.p_demux, DEMUX_SET_GROUP, -1,
                       &list );
        TAB_CLEAN( list.i_count, list.p_values );
    }
    else
    {
        demux_Control( p_input->p->input.p_demux, DEMUX_SET_GROUP,
                       es_out_GetGroupForced( p_input->p->p_es_out ), NULL );
    }
}

static int Init( input_thread_t * p_input )
{
    vlc_meta_t *p_meta;
    int i;

    for( i = 0; i < p_input->p->p_item->i_options; i++ )
    {
        if( !strncmp( p_input->p->p_item->ppsz_options[i], "meta-file", 9 ) )
        {
            msg_Dbg( p_input, "Input is a meta file: disabling unneeded options" );
            var_SetString( p_input, "sout", "" );
            var_SetBool( p_input, "sout-all", false );
            var_SetString( p_input, "input-slave", "" );
            var_SetInteger( p_input, "input-repeat", 0 );
            var_SetString( p_input, "sub-file", "" );
            var_SetBool( p_input, "sub-autodetect-file", false );
        }
    }

    InitStatistics( p_input );
#ifdef ENABLE_SOUT
    if( InitSout( p_input ) )
        goto error;
#endif

    /* Create es out */
    p_input->p->p_es_out = input_EsOutTimeshiftNew( p_input, p_input->p->p_es_out_display, p_input->p->i_rate );

    /* */
    input_ChangeState( p_input, OPENING_S );
    input_SendEventCache( p_input, 0.0 );

    /* */
    if( InputSourceInit( p_input, &p_input->p->input,
                         p_input->p->p_item->psz_uri, NULL, false ) )
    {
        goto error;
    }

    InitTitle( p_input );

    /* Load master infos */
    /* Init length */
    mtime_t i_length;
    if( demux_Control( p_input->p->input.p_demux, DEMUX_GET_LENGTH,
                         &i_length ) )
        i_length = 0;
    if( i_length <= 0 )
        i_length = input_item_GetDuration( p_input->p->p_item );
    input_SendEventLength( p_input, i_length );

    input_SendEventPosition( p_input, 0.0, 0 );

    if( !p_input->b_preparsing )
    {
        StartTitle( p_input );
        LoadSubtitles( p_input );
        LoadSlaves( p_input );
        InitPrograms( p_input );

        double f_rate = var_InheritFloat( p_input, "rate" );
        if( f_rate != 0.0 && f_rate != 1.0 )
        {
            vlc_value_t val = { .i_int = INPUT_RATE_DEFAULT / f_rate };
            input_ControlPush( p_input, INPUT_CONTROL_SET_RATE, &val );
        }
    }

    if( !p_input->b_preparsing && p_input->p->p_sout )
    {
        p_input->p->b_out_pace_control = (p_input->p->p_sout->i_out_pace_nocontrol > 0);

        if( p_input->p->b_can_pace_control && p_input->p->b_out_pace_control )
        {
            /* We don't want a high input priority here or we'll
             * end-up sucking up all the CPU time */
            vlc_set_priority( p_input->p->thread, VLC_THREAD_PRIORITY_LOW );
        }

        msg_Dbg( p_input, "starting in %s mode",
                 p_input->p->b_out_pace_control ? "async" : "sync" );
    }

    p_meta = vlc_meta_New();
    if( p_meta )
    {
        /* Get meta data from users */
        InputMetaUser( p_input, p_meta );

        /* Get meta data from master input */
        InputSourceMeta( p_input, &p_input->p->input, p_meta );

        /* And from slave */
        for( int i = 0; i < p_input->p->i_slave; i++ )
            InputSourceMeta( p_input, p_input->p->slave[i], p_meta );

        /* */
        InputUpdateMeta( p_input, p_meta );
    }

    msg_Dbg( p_input, "`%s' successfully opened",
             p_input->p->p_item->psz_uri );

    /* initialization is complete */
    input_ChangeState( p_input, PLAYING_S );

    return VLC_SUCCESS;

error:
    input_ChangeState( p_input, ERROR_S );

    if( p_input->p->p_es_out )
        es_out_Delete( p_input->p->p_es_out );
    es_out_SetMode( p_input->p->p_es_out_display, ES_OUT_MODE_END );
    if( p_input->p->p_resource )
    {
        if( p_input->p->p_sout )
            input_resource_RequestSout( p_input->p->p_resource,
                                         p_input->p->p_sout, NULL );
        input_resource_SetInput( p_input->p->p_resource, NULL );
        if( p_input->p->p_resource_private )
            input_resource_Terminate( p_input->p->p_resource_private );
    }

    if( !p_input->b_preparsing && libvlc_stats( p_input ) )
    {
#define EXIT_COUNTER( c ) do { if( p_input->p->counters.p_##c ) \
                                   stats_CounterClean( p_input->p->counters.p_##c );\
                               p_input->p->counters.p_##c = NULL; } while(0)
        EXIT_COUNTER( read_bytes );
        EXIT_COUNTER( read_packets );
        EXIT_COUNTER( demux_read );
        EXIT_COUNTER( input_bitrate );
        EXIT_COUNTER( demux_bitrate );
        EXIT_COUNTER( demux_corrupted );
        EXIT_COUNTER( demux_discontinuity );
        EXIT_COUNTER( played_abuffers );
        EXIT_COUNTER( lost_abuffers );
        EXIT_COUNTER( displayed_pictures );
        EXIT_COUNTER( lost_pictures );
        EXIT_COUNTER( decoded_audio );
        EXIT_COUNTER( decoded_video );
        EXIT_COUNTER( decoded_sub );

        if( p_input->p->p_sout )
        {
            EXIT_COUNTER( sout_sent_packets );
            EXIT_COUNTER( sout_sent_bytes );
            EXIT_COUNTER( sout_send_bitrate );
        }
#undef EXIT_COUNTER
    }

    /* Mark them deleted */
    p_input->p->input.p_demux = NULL;
    p_input->p->input.p_stream = NULL;
    p_input->p->input.p_access = NULL;
    p_input->p->p_es_out = NULL;
    p_input->p->p_sout = NULL;

    return VLC_EGENERIC;
}

/*****************************************************************************
 * End: end the input thread
 *****************************************************************************/
static void End( input_thread_t * p_input )
{
    int i;

    /* We are at the end */
    input_ChangeState( p_input, END_S );

    /* Clean control variables */
    input_ControlVarStop( p_input );

    /* Stop es out activity */
    es_out_SetMode( p_input->p->p_es_out, ES_OUT_MODE_NONE );

    /* Clean up master */
    InputSourceClean( &p_input->p->input );

    /* Delete slave */
    for( i = 0; i < p_input->p->i_slave; i++ )
    {
        InputSourceClean( p_input->p->slave[i] );
        free( p_input->p->slave[i] );
    }
    free( p_input->p->slave );

    /* Unload all modules */
    if( p_input->p->p_es_out )
        es_out_Delete( p_input->p->p_es_out );
    es_out_SetMode( p_input->p->p_es_out_display, ES_OUT_MODE_END );

    if( !p_input->b_preparsing )
    {
#define CL_CO( c ) stats_CounterClean( p_input->p->counters.p_##c ); p_input->p->counters.p_##c = NULL;
        if( libvlc_stats( p_input ) )
        {
            /* make sure we are up to date */
            stats_ComputeInputStats( p_input, p_input->p->p_item->p_stats );
            CL_CO( read_bytes );
            CL_CO( read_packets );
            CL_CO( demux_read );
            CL_CO( input_bitrate );
            CL_CO( demux_bitrate );
            CL_CO( demux_corrupted );
            CL_CO( demux_discontinuity );
            CL_CO( played_abuffers );
            CL_CO( lost_abuffers );
            CL_CO( displayed_pictures );
            CL_CO( lost_pictures );
            CL_CO( decoded_audio) ;
            CL_CO( decoded_video );
            CL_CO( decoded_sub) ;
        }

        /* Close optional stream output instance */
        if( p_input->p->p_sout )
        {
            CL_CO( sout_sent_packets );
            CL_CO( sout_sent_bytes );
            CL_CO( sout_send_bitrate );
        }
#undef CL_CO
    }

    vlc_mutex_lock( &p_input->p->p_item->lock );
    if( p_input->p->i_attachment > 0 )
    {
        for( i = 0; i < p_input->p->i_attachment; i++ )
            vlc_input_attachment_Delete( p_input->p->attachment[i] );
        TAB_CLEAN( p_input->p->i_attachment, p_input->p->attachment );
    }
    vlc_mutex_unlock( &p_input->p->p_item->lock );

    /* */
    input_resource_RequestSout( p_input->p->p_resource,
                                 p_input->p->p_sout, NULL );
    input_resource_SetInput( p_input->p->p_resource, NULL );
    if( p_input->p->p_resource_private )
        input_resource_Terminate( p_input->p->p_resource_private );
}

/*****************************************************************************
 * Control
 *****************************************************************************/
void input_ControlPush( input_thread_t *p_input,
                        int i_type, vlc_value_t *p_val )
{
    vlc_mutex_lock( &p_input->p->lock_control );
    if( i_type == INPUT_CONTROL_SET_DIE )
    {
        /* Special case, empty the control */
        for( int i = 0; i < p_input->p->i_control; i++ )
        {
            input_control_t *p_ctrl = &p_input->p->control[i];
            ControlRelease( p_ctrl->i_type, p_ctrl->val );
        }
        p_input->p->i_control = 0;
    }

    if( p_input->p->i_control >= INPUT_CONTROL_FIFO_SIZE )
    {
        msg_Err( p_input, "input control fifo overflow, trashing type=%d",
                 i_type );
        if( p_val )
            ControlRelease( i_type, *p_val );
    }
    else
    {
        input_control_t c;
        c.i_type = i_type;
        if( p_val )
            c.val = *p_val;
        else
            memset( &c.val, 0, sizeof(c.val) );

        p_input->p->control[p_input->p->i_control++] = c;
    }
    vlc_cond_signal( &p_input->p->wait_control );
    vlc_mutex_unlock( &p_input->p->lock_control );
}

static int ControlGetReducedIndexLocked( input_thread_t *p_input )
{
    const int i_lt = p_input->p->control[0].i_type;
    int i;
    for( i = 1; i < p_input->p->i_control; i++ )
    {
        const int i_ct = p_input->p->control[i].i_type;

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
            continue;
        }
        else
        {
            /* TODO but that's not that important
                - merge SET_X with SET_X_CMD
                - ignore SET_SEEKPOINT/SET_POSITION/SET_TIME before a SET_TITLE
                - ignore SET_SEEKPOINT/SET_POSITION/SET_TIME before another among them
                - ?
                */
            break;
        }
    }
    return i - 1;
}


static inline int ControlPop( input_thread_t *p_input,
                              int *pi_type, vlc_value_t *p_val,
                              mtime_t i_deadline, bool b_postpone_seek )
{
    input_thread_private_t *p_sys = p_input->p;

    vlc_mutex_lock( &p_sys->lock_control );
    while( p_sys->i_control <= 0 ||
           ( b_postpone_seek && ControlIsSeekRequest( p_sys->control[0].i_type ) ) )
    {
        if( !vlc_object_alive( p_input ) || i_deadline < 0 )
        {
            vlc_mutex_unlock( &p_sys->lock_control );
            return VLC_EGENERIC;
        }

        if( vlc_cond_timedwait( &p_sys->wait_control, &p_sys->lock_control,
                                i_deadline ) )
        {
            vlc_mutex_unlock( &p_sys->lock_control );
            return VLC_EGENERIC;
        }
    }

    /* */
    const int i_index = ControlGetReducedIndexLocked( p_input );

    /* */
    *pi_type = p_sys->control[i_index].i_type;
    *p_val   = p_sys->control[i_index].val;

    p_sys->i_control -= i_index + 1;
    if( p_sys->i_control > 0 )
        memmove( &p_sys->control[0], &p_sys->control[i_index+1],
                 sizeof(*p_sys->control) * p_sys->i_control );
    vlc_mutex_unlock( &p_sys->lock_control );

    return VLC_SUCCESS;
}
static bool ControlIsSeekRequest( int i_type )
{
    switch( i_type )
    {
    case INPUT_CONTROL_SET_POSITION:
    case INPUT_CONTROL_SET_TIME:
    case INPUT_CONTROL_SET_TITLE:
    case INPUT_CONTROL_SET_TITLE_NEXT:
    case INPUT_CONTROL_SET_TITLE_PREV:
    case INPUT_CONTROL_SET_SEEKPOINT:
    case INPUT_CONTROL_SET_SEEKPOINT_NEXT:
    case INPUT_CONTROL_SET_SEEKPOINT_PREV:
    case INPUT_CONTROL_SET_BOOKMARK:
        return true;
    default:
        return false;
    }
}

static void ControlRelease( int i_type, vlc_value_t val )
{
    switch( i_type )
    {
    case INPUT_CONTROL_ADD_SUBTITLE:
    case INPUT_CONTROL_ADD_SLAVE:
        free( val.psz_string );
        break;

    default:
        break;
    }
}

/* Pause input */
static void ControlPause( input_thread_t *p_input, mtime_t i_control_date )
{
    int i_ret = VLC_SUCCESS;
    int i_state = PAUSE_S;

    if( p_input->p->b_can_pause )
    {
        if( p_input->p->input.p_access )
            i_ret = access_Control( p_input->p->input.p_access,
                                     ACCESS_SET_PAUSE_STATE, true );
        else
            i_ret = demux_Control( p_input->p->input.p_demux,
                                    DEMUX_SET_PAUSE_STATE, true );

        if( i_ret )
        {
            msg_Warn( p_input, "cannot set pause state" );
            return;
        }
    }

    /* */
    i_ret = es_out_SetPauseState( p_input->p->p_es_out,
                                  p_input->p->b_can_pause, true,
                                  i_control_date );
    if( i_ret )
    {
        msg_Warn( p_input, "cannot set pause state at es_out level" );
        return;
    }

    /* Switch to new state */
    input_ChangeState( p_input, i_state );
}

static void ControlUnpause( input_thread_t *p_input, mtime_t i_control_date )
{
    int i_ret = VLC_SUCCESS;

    if( p_input->p->b_can_pause )
    {
        if( p_input->p->input.p_access )
            i_ret = access_Control( p_input->p->input.p_access,
                                     ACCESS_SET_PAUSE_STATE, false );
        else
            i_ret = demux_Control( p_input->p->input.p_demux,
                                    DEMUX_SET_PAUSE_STATE, false );
        if( i_ret )
        {
            /* FIXME What to do ? */
            msg_Warn( p_input, "cannot unset pause -> EOF" );
            input_ControlPush( p_input, INPUT_CONTROL_SET_DIE, NULL );
        }
    }

    /* Switch to play */
    input_ChangeState( p_input, PLAYING_S );

    /* */
    if( !i_ret )
        es_out_SetPauseState( p_input->p->p_es_out, false, false, i_control_date );
}

static bool Control( input_thread_t *p_input,
                     int i_type, vlc_value_t val )
{
    const mtime_t i_control_date = mdate();
    /* FIXME b_force_update is abused, it should be carefully checked */
    bool b_force_update = false;

    if( !p_input )
        return b_force_update;

    switch( i_type )
    {
        case INPUT_CONTROL_SET_DIE:
            msg_Dbg( p_input, "control: stopping input" );

            /* Mark all submodules to die */
            ObjectKillChildrens( p_input, VLC_OBJECT(p_input) );
            break;

        case INPUT_CONTROL_SET_POSITION:
        {
            double f_pos;

            if( p_input->p->b_recording )
            {
                msg_Err( p_input, "INPUT_CONTROL_SET_POSITION(_OFFSET) ignored while recording" );
                break;
            }
            f_pos = val.f_float;
            if( i_type != INPUT_CONTROL_SET_POSITION )
                f_pos += var_GetFloat( p_input, "position" );
            if( f_pos < 0.0 )
                f_pos = 0.0;
            else if( f_pos > 1.0 )
                f_pos = 1.0;
            /* Reset the decoders states and clock sync (before calling the demuxer */
            es_out_SetTime( p_input->p->p_es_out, -1 );
            if( demux_Control( p_input->p->input.p_demux, DEMUX_SET_POSITION,
                                f_pos, !p_input->p->b_fast_seek ) )
            {
                msg_Err( p_input, "INPUT_CONTROL_SET_POSITION(_OFFSET) "
                         "%2.1f%% failed", f_pos * 100 );
            }
            else
            {
                if( p_input->p->i_slave > 0 )
                    SlaveSeek( p_input );
                p_input->p->input.b_eof = false;

                b_force_update = true;
            }
            break;
        }

        case INPUT_CONTROL_SET_TIME:
        {
            int64_t i_time;
            int i_ret;

            if( p_input->p->b_recording )
            {
                msg_Err( p_input, "INPUT_CONTROL_SET_TIME(_OFFSET) ignored while recording" );
                break;
            }

            i_time = val.i_time;
            if( i_type != INPUT_CONTROL_SET_TIME )
                i_time += var_GetTime( p_input, "time" );

            if( i_time < 0 )
                i_time = 0;

            /* Reset the decoders states and clock sync (before calling the demuxer */
            es_out_SetTime( p_input->p->p_es_out, -1 );

            i_ret = demux_Control( p_input->p->input.p_demux,
                                   DEMUX_SET_TIME, i_time,
                                   !p_input->p->b_fast_seek );
            if( i_ret )
            {
                int64_t i_length;

                /* Emulate it with a SET_POS */
                if( !demux_Control( p_input->p->input.p_demux,
                                    DEMUX_GET_LENGTH, &i_length ) && i_length > 0 )
                {
                    double f_pos = (double)i_time / (double)i_length;
                    i_ret = demux_Control( p_input->p->input.p_demux,
                                            DEMUX_SET_POSITION, f_pos,
                                            !p_input->p->b_fast_seek );
                }
            }
            if( i_ret )
            {
                msg_Warn( p_input, "INPUT_CONTROL_SET_TIME(_OFFSET) %"PRId64
                         " failed or not possible", i_time );
            }
            else
            {
                if( p_input->p->i_slave > 0 )
                    SlaveSeek( p_input );
                p_input->p->input.b_eof = false;

                b_force_update = true;
            }
            break;
        }

        case INPUT_CONTROL_SET_STATE:
            if( val.i_int != PLAYING_S && val.i_int != PAUSE_S )
                msg_Err( p_input, "invalid state in INPUT_CONTROL_SET_STATE" );
            else if( p_input->p->i_state == PAUSE_S )
            {
                ControlUnpause( p_input, i_control_date );

                b_force_update = true;
            }
            else if( val.i_int == PAUSE_S && p_input->p->i_state == PLAYING_S /* &&
                     p_input->p->b_can_pause */ )
            {
                ControlPause( p_input, i_control_date );

                b_force_update = true;
            }
            else if( val.i_int == PAUSE_S && !p_input->p->b_can_pause && 0 )
            {
                b_force_update = true;

                /* Correct "state" value */
                input_ChangeState( p_input, p_input->p->i_state );
            }
            break;

        case INPUT_CONTROL_SET_RATE:
        {
            /* Get rate and direction */
            int i_rate = abs( val.i_int );
            int i_rate_sign = val.i_int < 0 ? -1 : 1;

            /* Check rate bound */
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

            /* Apply direction */
            if( i_rate_sign < 0 )
            {
                if( p_input->p->input.b_rescale_ts )
                {
                    msg_Dbg( p_input, "cannot set negative rate" );
                    i_rate = p_input->p->i_rate;
                    assert( i_rate > 0 );
                }
                else
                {
                    i_rate *= i_rate_sign;
                }
            }

            if( i_rate != INPUT_RATE_DEFAULT &&
                ( ( !p_input->p->b_can_rate_control && !p_input->p->input.b_rescale_ts ) ||
                  ( p_input->p->p_sout && !p_input->p->b_out_pace_control ) ) )
            {
                msg_Dbg( p_input, "cannot change rate" );
                i_rate = INPUT_RATE_DEFAULT;
            }
            if( i_rate != p_input->p->i_rate &&
                !p_input->p->b_can_pace_control && p_input->p->b_can_rate_control )
            {
                int i_ret;
                if( p_input->p->input.p_access )
                {
                    i_ret = VLC_EGENERIC;
                }
                else
                {
                    if( !p_input->p->input.b_rescale_ts )
                        es_out_Control( p_input->p->p_es_out, ES_OUT_RESET_PCR );

                    i_ret = demux_Control( p_input->p->input.p_demux,
                                            DEMUX_SET_RATE, &i_rate );
                }
                if( i_ret )
                {
                    msg_Warn( p_input, "ACCESS/DEMUX_SET_RATE failed" );
                    i_rate = p_input->p->i_rate;
                }
            }

            /* */
            if( i_rate != p_input->p->i_rate )
            {
                p_input->p->i_rate = i_rate;
                input_SendEventRate( p_input, i_rate );

                if( p_input->p->input.b_rescale_ts )
                {
                    const int i_rate_source = (p_input->p->b_can_pace_control || p_input->p->b_can_rate_control ) ? i_rate : INPUT_RATE_DEFAULT;
                    es_out_SetRate( p_input->p->p_es_out, i_rate_source, i_rate );
                }

                b_force_update = true;
            }
            break;
        }

        case INPUT_CONTROL_SET_PROGRAM:
            /* No need to force update, es_out does it if needed */
            es_out_Control( p_input->p->p_es_out,
                            ES_OUT_SET_GROUP, val.i_int );

            demux_Control( p_input->p->input.p_demux, DEMUX_SET_GROUP, val.i_int,
                            NULL );
            break;

        case INPUT_CONTROL_SET_ES:
            /* No need to force update, es_out does it if needed */
            es_out_Control( p_input->p->p_es_out_display, ES_OUT_SET_ES_BY_ID, val.i_int );
            break;

        case INPUT_CONTROL_RESTART_ES:
            es_out_Control( p_input->p->p_es_out_display, ES_OUT_RESTART_ES_BY_ID, val.i_int );
            break;

        case INPUT_CONTROL_SET_AUDIO_DELAY:
            input_SendEventAudioDelay( p_input, val.i_time );
            UpdatePtsDelay( p_input );
            break;

        case INPUT_CONTROL_SET_SPU_DELAY:
            input_SendEventSubtitleDelay( p_input, val.i_time );
            UpdatePtsDelay( p_input );
            break;

        case INPUT_CONTROL_SET_TITLE:
        case INPUT_CONTROL_SET_TITLE_NEXT:
        case INPUT_CONTROL_SET_TITLE_PREV:
            if( p_input->p->b_recording )
            {
                msg_Err( p_input, "INPUT_CONTROL_SET_TITLE(*) ignored while recording" );
                break;
            }
            if( p_input->p->input.b_title_demux &&
                p_input->p->input.i_title > 0 )
            {
                /* TODO */
                /* FIXME handle demux title */
                demux_t *p_demux = p_input->p->input.p_demux;
                int i_title;

                if( i_type == INPUT_CONTROL_SET_TITLE_PREV )
                    i_title = p_demux->info.i_title - 1;
                else if( i_type == INPUT_CONTROL_SET_TITLE_NEXT )
                    i_title = p_demux->info.i_title + 1;
                else
                    i_title = val.i_int;

                if( i_title >= 0 && i_title < p_input->p->input.i_title )
                {
                    es_out_SetTime( p_input->p->p_es_out, -1 );

                    demux_Control( p_demux, DEMUX_SET_TITLE, i_title );
                    input_SendEventTitle( p_input, i_title );
                }
            }
            else if( p_input->p->input.i_title > 0 )
            {
                access_t *p_access = p_input->p->input.p_access;
                int i_title;

                if( i_type == INPUT_CONTROL_SET_TITLE_PREV )
                    i_title = p_access->info.i_title - 1;
                else if( i_type == INPUT_CONTROL_SET_TITLE_NEXT )
                    i_title = p_access->info.i_title + 1;
                else
                    i_title = val.i_int;

                if( i_title >= 0 && i_title < p_input->p->input.i_title )
                {
                    es_out_SetTime( p_input->p->p_es_out, -1 );

                    stream_Control( p_input->p->input.p_stream, STREAM_CONTROL_ACCESS,
                                    ACCESS_SET_TITLE, i_title );
                    input_SendEventTitle( p_input, i_title );
                }
            }
            break;
        case INPUT_CONTROL_SET_SEEKPOINT:
        case INPUT_CONTROL_SET_SEEKPOINT_NEXT:
        case INPUT_CONTROL_SET_SEEKPOINT_PREV:
            if( p_input->p->b_recording )
            {
                msg_Err( p_input, "INPUT_CONTROL_SET_SEEKPOINT(*) ignored while recording" );
                break;
            }

            if( p_input->p->input.b_title_demux &&
                p_input->p->input.i_title > 0 )
            {
                demux_t *p_demux = p_input->p->input.p_demux;
                int i_seekpoint;
                int64_t i_input_time;
                int64_t i_seekpoint_time;

                if( i_type == INPUT_CONTROL_SET_SEEKPOINT_PREV )
                {
                    i_seekpoint = p_demux->info.i_seekpoint;
                    i_seekpoint_time = p_input->p->input.title[p_demux->info.i_title]->seekpoint[i_seekpoint]->i_time_offset;
                    i_input_time = var_GetTime( p_input, "time" );
                    if( i_seekpoint_time >= 0 && i_input_time >= 0 )
                    {
                        if( i_input_time < i_seekpoint_time + 3000000 )
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
                    p_input->p->input.title[p_demux->info.i_title]->i_seekpoint )
                {

                    es_out_SetTime( p_input->p->p_es_out, -1 );

                    demux_Control( p_demux, DEMUX_SET_SEEKPOINT, i_seekpoint );
                    input_SendEventSeekpoint( p_input, p_demux->info.i_title, i_seekpoint );
                }
            }
            else if( p_input->p->input.i_title > 0 )
            {
                access_t *p_access = p_input->p->input.p_access;
                int i_seekpoint;
                int64_t i_input_time;
                int64_t i_seekpoint_time;

                if( i_type == INPUT_CONTROL_SET_SEEKPOINT_PREV )
                {
                    i_seekpoint = p_access->info.i_seekpoint;
                    i_seekpoint_time = p_input->p->input.title[p_access->info.i_title]->seekpoint[i_seekpoint]->i_time_offset;
                    i_input_time = var_GetTime( p_input, "time" );
                    if( i_seekpoint_time >= 0 && i_input_time >= 0 )
                    {
                        if( i_input_time < i_seekpoint_time + 3000000 )
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
                    p_input->p->input.title[p_access->info.i_title]->i_seekpoint )
                {
                    es_out_SetTime( p_input->p->p_es_out, -1 );

                    stream_Control( p_input->p->input.p_stream, STREAM_CONTROL_ACCESS,
                                    ACCESS_SET_SEEKPOINT, i_seekpoint );
                    input_SendEventSeekpoint( p_input, p_access->info.i_title, i_seekpoint );
                }
            }
            break;

        case INPUT_CONTROL_ADD_SUBTITLE:
            if( val.psz_string )
                SubtitleAdd( p_input, val.psz_string, true );
            break;

        case INPUT_CONTROL_ADD_SLAVE:
            if( val.psz_string )
            {
                char *uri = make_URI( val.psz_string, NULL );
                if( uri == NULL )
                    break;

                input_source_t *slave = InputSourceNew( p_input );

                if( slave && !InputSourceInit( p_input, slave, uri, NULL, false ) )
                {
                    vlc_meta_t *p_meta;
                    int64_t i_time;

                    /* Add the slave */
                    msg_Dbg( p_input, "adding %s as slave on the fly", uri );

                    /* Set position */
                    if( demux_Control( p_input->p->input.p_demux,
                                        DEMUX_GET_TIME, &i_time ) )
                    {
                        msg_Err( p_input, "demux doesn't like DEMUX_GET_TIME" );
                        InputSourceClean( slave );
                        free( slave );
                        break;
                    }
                    if( demux_Control( slave->p_demux,
                                       DEMUX_SET_TIME, i_time, true ) )
                    {
                        msg_Err( p_input, "seek failed for new slave" );
                        InputSourceClean( slave );
                        free( slave );
                        break;
                    }

                    /* Get meta (access and demux) */
                    p_meta = vlc_meta_New();
                    if( p_meta )
                    {
                        access_Control( slave->p_access, ACCESS_GET_META, p_meta );
                        demux_Control( slave->p_demux, DEMUX_GET_META, p_meta );
                        InputUpdateMeta( p_input, p_meta );
                    }

                    TAB_APPEND( p_input->p->i_slave, p_input->p->slave, slave );
                }
                else
                {
                    free( slave );
                    msg_Warn( p_input, "failed to add %s as slave", uri );
                }
                free( uri );
            }
            break;

        case INPUT_CONTROL_SET_RECORD_STATE:
            if( !!p_input->p->b_recording != !!val.b_bool )
            {
                if( p_input->p->input.b_can_stream_record )
                {
                    if( demux_Control( p_input->p->input.p_demux,
                                       DEMUX_SET_RECORD_STATE, val.b_bool ) )
                        val.b_bool = false;
                }
                else
                {
                    if( es_out_SetRecordState( p_input->p->p_es_out_display, val.b_bool ) )
                        val.b_bool = false;
                }
                p_input->p->b_recording = val.b_bool;

                input_SendEventRecord( p_input, val.b_bool );

                b_force_update = true;
            }
            break;

        case INPUT_CONTROL_SET_FRAME_NEXT:
            if( p_input->p->i_state == PAUSE_S )
            {
                es_out_SetFrameNext( p_input->p->p_es_out );
            }
            else if( p_input->p->i_state == PLAYING_S )
            {
                ControlPause( p_input, i_control_date );
            }
            else
            {
                msg_Err( p_input, "invalid state for frame next" );
            }
            b_force_update = true;
            break;

        case INPUT_CONTROL_SET_BOOKMARK:
        {
            seekpoint_t bookmark;

            bookmark.i_time_offset = -1;
            bookmark.i_byte_offset = -1;

            vlc_mutex_lock( &p_input->p->p_item->lock );
            if( val.i_int >= 0 && val.i_int < p_input->p->i_bookmark )
            {
                const seekpoint_t *p_bookmark = p_input->p->pp_bookmark[val.i_int];
                bookmark.i_time_offset = p_bookmark->i_time_offset;
                bookmark.i_byte_offset = p_bookmark->i_byte_offset;
            }
            vlc_mutex_unlock( &p_input->p->p_item->lock );

            if( bookmark.i_time_offset < 0 && bookmark.i_byte_offset < 0 )
            {
                msg_Err( p_input, "invalid bookmark %"PRId64, val.i_int );
                break;
            }

            if( bookmark.i_time_offset >= 0 )
            {
                val.i_time = bookmark.i_time_offset;
                b_force_update = Control( p_input, INPUT_CONTROL_SET_TIME, val );
            }
            else if( bookmark.i_byte_offset >= 0 &&
                     p_input->p->input.p_stream )
            {
                const uint64_t i_size = stream_Size( p_input->p->input.p_stream );
                if( i_size > 0 && (uint64_t)bookmark.i_byte_offset <= i_size )
                {
                    val.f_float = (double)bookmark.i_byte_offset / i_size;
                    b_force_update = Control( p_input, INPUT_CONTROL_SET_POSITION, val );
                }
            }
            break;
        }

        default:
            msg_Err( p_input, "not yet implemented" );
            break;
    }

    ControlRelease( i_type, val );
    return b_force_update;
}

/*****************************************************************************
 * UpdateTitleSeekpoint
 *****************************************************************************/
static int UpdateTitleSeekpoint( input_thread_t *p_input,
                                 int i_title, int i_seekpoint )
{
    int i_title_end = p_input->p->input.i_title_end -
                        p_input->p->input.i_title_offset;
    int i_seekpoint_end = p_input->p->input.i_seekpoint_end -
                            p_input->p->input.i_seekpoint_offset;

    if( i_title_end >= 0 && i_seekpoint_end >= 0 )
    {
        if( i_title > i_title_end ||
            ( i_title == i_title_end && i_seekpoint > i_seekpoint_end ) )
            return 0;
    }
    else if( i_seekpoint_end >= 0 )
    {
        if( i_seekpoint > i_seekpoint_end )
            return 0;
    }
    else if( i_title_end >= 0 )
    {
        if( i_title > i_title_end )
            return 0;
    }
    return 1;
}
/*****************************************************************************
 * Update*FromDemux:
 *****************************************************************************/
static int UpdateTitleSeekpointFromDemux( input_thread_t *p_input )
{
    demux_t *p_demux = p_input->p->input.p_demux;

    /* TODO event-like */
    if( p_demux->info.i_update & INPUT_UPDATE_TITLE )
    {
        input_SendEventTitle( p_input, p_demux->info.i_title );

        p_demux->info.i_update &= ~INPUT_UPDATE_TITLE;
    }
    if( p_demux->info.i_update & INPUT_UPDATE_SEEKPOINT )
    {
        input_SendEventSeekpoint( p_input,
                                  p_demux->info.i_title, p_demux->info.i_seekpoint );

        p_demux->info.i_update &= ~INPUT_UPDATE_SEEKPOINT;
    }

    /* Hmmm only works with master input */
    if( p_input->p->input.p_demux == p_demux )
        return UpdateTitleSeekpoint( p_input,
                                     p_demux->info.i_title,
                                     p_demux->info.i_seekpoint );
    return 1;
}

static void UpdateGenericFromDemux( input_thread_t *p_input )
{
    demux_t *p_demux = p_input->p->input.p_demux;

    if( p_demux->info.i_update & INPUT_UPDATE_META )
    {
        vlc_meta_t *p_meta = vlc_meta_New();
        if( p_meta )
        {
            demux_Control( p_input->p->input.p_demux, DEMUX_GET_META, p_meta );
            InputUpdateMeta( p_input, p_meta );
        }
        p_demux->info.i_update &= ~INPUT_UPDATE_META;
    }
    if( p_demux->info.i_update & INPUT_UPDATE_SIGNAL )
    {
        double quality;
        double strength;

        if( demux_Control( p_demux, DEMUX_GET_SIGNAL, &quality, &strength ) )
            quality = strength = -1.;

        input_SendEventSignal( p_input, quality, strength );

        p_demux->info.i_update &= ~INPUT_UPDATE_SIGNAL;
    }

    p_demux->info.i_update &= ~INPUT_UPDATE_SIZE;
}


/*****************************************************************************
 * Update*FromAccess:
 *****************************************************************************/
static int UpdateTitleSeekpointFromAccess( input_thread_t *p_input )
{
    access_t *p_access = p_input->p->input.p_access;

    if( p_access->info.i_update & INPUT_UPDATE_TITLE )
    {
        input_SendEventTitle( p_input, p_access->info.i_title );

        stream_Control( p_input->p->input.p_stream, STREAM_UPDATE_SIZE );

        p_access->info.i_update &= ~INPUT_UPDATE_TITLE;
    }
    if( p_access->info.i_update & INPUT_UPDATE_SEEKPOINT )
    {
        input_SendEventSeekpoint( p_input,
                                  p_access->info.i_title, p_access->info.i_seekpoint );

        p_access->info.i_update &= ~INPUT_UPDATE_SEEKPOINT;
    }
    /* Hmmm only works with master input */
    if( p_input->p->input.p_access == p_access )
        return UpdateTitleSeekpoint( p_input,
                                     p_access->info.i_title,
                                     p_access->info.i_seekpoint );
    return 1;
}
static void UpdateGenericFromAccess( input_thread_t *p_input )
{
    access_t *p_access = p_input->p->input.p_access;

    if( p_access->info.i_update & INPUT_UPDATE_META )
    {
        /* TODO maybe multi - access ? */
        vlc_meta_t *p_meta = vlc_meta_New();
        if( p_meta )
        {
            access_Control( p_input->p->input.p_access, ACCESS_GET_META, p_meta );
            InputUpdateMeta( p_input, p_meta );
        }
        p_access->info.i_update &= ~INPUT_UPDATE_META;
    }
    if( p_access->info.i_update & INPUT_UPDATE_SIGNAL )
    {
        double f_quality;
        double f_strength;

        if( access_Control( p_access, ACCESS_GET_SIGNAL, &f_quality, &f_strength ) )
            f_quality = f_strength = -1;

        input_SendEventSignal( p_input, f_quality, f_strength );

        p_access->info.i_update &= ~INPUT_UPDATE_SIGNAL;
    }

    p_access->info.i_update &= ~INPUT_UPDATE_SIZE;
}

/*****************************************************************************
 * InputSourceNew:
 *****************************************************************************/
static input_source_t *InputSourceNew( input_thread_t *p_input )
{
    VLC_UNUSED(p_input);

    return calloc( 1,  sizeof( input_source_t ) );
}

/*****************************************************************************
 * InputSourceInit:
 *****************************************************************************/
static int InputSourceInit( input_thread_t *p_input,
                            input_source_t *in, const char *psz_mrl,
                            const char *psz_forced_demux, bool b_in_can_fail )
{
    const char *psz_access, *psz_demux, *psz_path, *psz_anchor;
    char *psz_var_demux = NULL;
    double f_fps;

    assert( psz_mrl );
    char *psz_dup = strdup( psz_mrl );

    if( psz_dup == NULL )
        goto error;

    /* Split uri */
    input_SplitMRL( &psz_access, &psz_demux, &psz_path, &psz_anchor, psz_dup );

    msg_Dbg( p_input, "`%s' gives access `%s' demux `%s' path `%s'",
             psz_mrl, psz_access, psz_demux, psz_path );
    if( !p_input->b_preparsing )
    {
        /* Find optional titles and seekpoints */
        MRLSections( psz_anchor, &in->i_title_start, &in->i_title_end,
                     &in->i_seekpoint_start, &in->i_seekpoint_end );
        if( psz_forced_demux && *psz_forced_demux )
        {
            psz_demux = psz_forced_demux;
        }
        else if( *psz_demux == '\0' )
        {
            /* special hack for forcing a demuxer with --demux=module
             * (and do nothing with a list) */
            psz_var_demux = var_GetNonEmptyString( p_input, "demux" );

            if( psz_var_demux != NULL &&
                !strchr(psz_var_demux, ',' ) &&
                !strchr(psz_var_demux, ':' ) )
            {
                psz_demux = psz_var_demux;

                msg_Dbg( p_input, "enforced demux ` %s'", psz_demux );
            }
        }

        /* Try access_demux first */
        in->p_demux = demux_New( p_input, p_input, psz_access, psz_demux, psz_path,
                                  NULL, p_input->p->p_es_out, false );
    }
    else
    {
        /* Preparsing is only for file:// */
        if( *psz_demux )
            goto error;
        if( strcmp( psz_access, "file" ) )
            goto error;
        msg_Dbg( p_input, "trying to pre-parse %s",  psz_path );
    }

    if( in->p_demux )
    {
        /* Get infos from access_demux */
        in->b_title_demux = true;
        if( demux_Control( in->p_demux, DEMUX_GET_TITLE_INFO,
                            &in->title, &in->i_title,
                            &in->i_title_offset, &in->i_seekpoint_offset ) )
        {
            TAB_INIT( in->i_title, in->title );
        }
        if( demux_Control( in->p_demux, DEMUX_CAN_CONTROL_PACE,
                            &in->b_can_pace_control ) )
            in->b_can_pace_control = false;

        assert( in->p_demux->pf_demux != NULL || !in->b_can_pace_control );

        if( !in->b_can_pace_control )
        {
            if( demux_Control( in->p_demux, DEMUX_CAN_CONTROL_RATE,
                                &in->b_can_rate_control, &in->b_rescale_ts ) )
            {
                in->b_can_rate_control = false;
                in->b_rescale_ts = true; /* not used */
            }
        }
        else
        {
            in->b_can_rate_control = true;
            in->b_rescale_ts = true;
        }
        if( demux_Control( in->p_demux, DEMUX_CAN_PAUSE,
                            &in->b_can_pause ) )
            in->b_can_pause = false;
        var_SetBool( p_input, "can-pause", in->b_can_pause || !in->b_can_pace_control ); /* XXX temporary because of es_out_timeshift*/
        var_SetBool( p_input, "can-rate", !in->b_can_pace_control || in->b_can_rate_control ); /* XXX temporary because of es_out_timeshift*/
        var_SetBool( p_input, "can-rewind", !in->b_rescale_ts && !in->b_can_pace_control && in->b_can_rate_control );

        bool b_can_seek;
        if( demux_Control( in->p_demux, DEMUX_CAN_SEEK, &b_can_seek ) )
            b_can_seek = false;
        var_SetBool( p_input, "can-seek", b_can_seek );
    }
    else
    {
        /* Now try a real access */
        in->p_access = access_New( p_input, p_input, psz_access, psz_demux, psz_path );
        if( in->p_access == NULL )
        {
            if( vlc_object_alive( p_input ) )
            {
                msg_Err( p_input, "open of `%s' failed", psz_mrl );
                if( !b_in_can_fail )
                    dialog_Fatal( p_input, _("Your input can't be opened"),
                                   _("VLC is unable to open the MRL '%s'."
                                     " Check the log for details."), psz_mrl );
            }
            goto error;
        }

        /* Get infos from access */
        if( !p_input->b_preparsing )
        {
            bool b_can_seek;

            in->b_title_demux = false;
            if( access_Control( in->p_access, ACCESS_GET_TITLE_INFO,
                                 &in->title, &in->i_title,
                                &in->i_title_offset, &in->i_seekpoint_offset ) )

            {
                TAB_INIT( in->i_title, in->title );
            }
            access_Control( in->p_access, ACCESS_CAN_CONTROL_PACE,
                             &in->b_can_pace_control );
            in->b_can_rate_control = in->b_can_pace_control;
            in->b_rescale_ts = true;

            access_Control( in->p_access, ACCESS_CAN_PAUSE, &in->b_can_pause );
            var_SetBool( p_input, "can-pause", in->b_can_pause || !in->b_can_pace_control ); /* XXX temporary because of es_out_timeshift*/
            var_SetBool( p_input, "can-rate", !in->b_can_pace_control || in->b_can_rate_control ); /* XXX temporary because of es_out_timeshift*/
            var_SetBool( p_input, "can-rewind", !in->b_rescale_ts && !in->b_can_pace_control );

            access_Control( in->p_access, ACCESS_CAN_SEEK, &b_can_seek );
            var_SetBool( p_input, "can-seek", b_can_seek );
        }

        /* */
        int  i_input_list;
        char **ppsz_input_list;

        TAB_INIT( i_input_list, ppsz_input_list );

        /* On master stream only, use input-list */
        if( &p_input->p->input == in )
        {
            char *psz_list;
            char *psz_parser;

            psz_list =
            psz_parser = var_CreateGetNonEmptyString( p_input, "input-list" );

            while( psz_parser && *psz_parser )
            {
                char *p = strchr( psz_parser, ',' );
                if( p )
                    *p++ = '\0';

                if( *psz_parser )
                {
                    char *psz_name = strdup( psz_parser );
                    if( psz_name )
                        TAB_APPEND( i_input_list, ppsz_input_list, psz_name );
                }

                psz_parser = p;
            }
            free( psz_list );
        }
        /* Autodetect extra files if none specified */
        if( i_input_list <= 0 )
        {
            InputGetExtraFiles( p_input, &i_input_list, &ppsz_input_list,
                                psz_access, psz_path );
        }
        if( i_input_list > 0 )
            TAB_APPEND( i_input_list, ppsz_input_list, NULL );

        /* Create the stream_t */
        in->p_stream = stream_AccessNew( in->p_access, ppsz_input_list );
        if( ppsz_input_list )
        {
            for( int i = 0; ppsz_input_list[i] != NULL; i++ )
                free( ppsz_input_list[i] );
            TAB_CLEAN( i_input_list, ppsz_input_list );
        }

        if( in->p_stream == NULL )
        {
            msg_Warn( p_input, "cannot create a stream_t from access" );
            goto error;
        }

        /* Add stream filters */
        char *psz_stream_filter = var_GetNonEmptyString( p_input,
                                                         "stream-filter" );
        in->p_stream = stream_FilterChainNew( in->p_stream,
                                              psz_stream_filter,
                                              var_GetBool( p_input, "input-record-native" ) );
        free( psz_stream_filter );

        /* Open a demuxer */
        if( *psz_demux == '\0' && *in->p_access->psz_demux )
        {
            psz_demux = in->p_access->psz_demux;
        }

        in->p_demux = demux_New( p_input, p_input, psz_access, psz_demux,
                   /* Take access/stream redirections into account: */
                   in->p_stream->psz_path ? in->p_stream->psz_path : psz_path,
                                 in->p_stream, p_input->p->p_es_out,
                                 p_input->b_preparsing );

        if( in->p_demux == NULL )
        {
            if( vlc_object_alive( p_input ) )
            {
                msg_Err( p_input, "no suitable demux module for `%s/%s://%s'",
                         psz_access, psz_demux, psz_path );
                if( !b_in_can_fail )
                    dialog_Fatal( VLC_OBJECT( p_input ),
                                  _("VLC can't recognize the input's format"),
                                  _("The format of '%s' cannot be detected. "
                                    "Have a look at the log for details."), psz_mrl );
            }
            goto error;
        }
        assert( in->p_demux->pf_demux != NULL );

        /* Get title from demux */
        if( !p_input->b_preparsing && in->i_title <= 0 )
        {
            if( demux_Control( in->p_demux, DEMUX_GET_TITLE_INFO,
                                &in->title, &in->i_title,
                                &in->i_title_offset, &in->i_seekpoint_offset ))
            {
                TAB_INIT( in->i_title, in->title );
            }
            else
            {
                in->b_title_demux = true;
            }
        }
    }

    free( psz_var_demux );
    free( psz_dup );

    /* Set record capabilities */
    if( demux_Control( in->p_demux, DEMUX_CAN_RECORD, &in->b_can_stream_record ) )
        in->b_can_stream_record = false;
#ifdef ENABLE_SOUT
    if( !var_GetBool( p_input, "input-record-native" ) )
        in->b_can_stream_record = false;
    var_SetBool( p_input, "can-record", true );
#else
    var_SetBool( p_input, "can-record", in->b_can_stream_record );
#endif

    /* get attachment
     * FIXME improve for b_preparsing: move it after GET_META and check psz_arturl */
    if( !p_input->b_preparsing )
    {
        int i_attachment;
        input_attachment_t **attachment;
        if( !demux_Control( in->p_demux, DEMUX_GET_ATTACHMENTS,
                             &attachment, &i_attachment ) )
        {
            vlc_mutex_lock( &p_input->p->p_item->lock );
            AppendAttachment( &p_input->p->i_attachment, &p_input->p->attachment,
                              i_attachment, attachment );
            vlc_mutex_unlock( &p_input->p->p_item->lock );
        }

        /* PTS delay: request from demux first. This is required for
         * access_demux and some special cases like SDP demux. Otherwise,
         * fallback to access */
        if( demux_Control( in->p_demux, DEMUX_GET_PTS_DELAY,
                           &in->i_pts_delay ) )
        {
            /* GET_PTS_DELAY is mandatory for access_demux */
            assert( in->p_access );
            access_Control( in->p_access,
                            ACCESS_GET_PTS_DELAY, &in->i_pts_delay );
        }
        if( in->i_pts_delay > INPUT_PTS_DELAY_MAX )
            in->i_pts_delay = INPUT_PTS_DELAY_MAX;
        else if( in->i_pts_delay < 0 )
            in->i_pts_delay = 0;
    }

    if( !demux_Control( in->p_demux, DEMUX_GET_FPS, &f_fps ) && f_fps > 0.0 )
    {
        vlc_mutex_lock( &p_input->p->p_item->lock );
        p_input->p->f_fps = f_fps;
        vlc_mutex_unlock( &p_input->p->p_item->lock );
    }

    if( var_GetInteger( p_input, "clock-synchro" ) != -1 )
        in->b_can_pace_control = !var_GetInteger( p_input, "clock-synchro" );

    return VLC_SUCCESS;

error:
    if( in->p_demux )
        demux_Delete( in->p_demux );

    if( in->p_stream )
        stream_Delete( in->p_stream );

    if( in->p_access )
        access_Delete( in->p_access );

    free( psz_var_demux );
    free( psz_dup );

    return VLC_EGENERIC;
}

/*****************************************************************************
 * InputSourceClean:
 *****************************************************************************/
static void InputSourceClean( input_source_t *in )
{
    int i;

    if( in->p_demux )
        demux_Delete( in->p_demux );

    if( in->p_stream )
        stream_Delete( in->p_stream );

    if( in->p_access )
        access_Delete( in->p_access );

    if( in->i_title > 0 )
    {
        for( i = 0; i < in->i_title; i++ )
            vlc_input_title_Delete( in->title[i] );
        TAB_CLEAN( in->i_title, in->title );
    }
}

/*****************************************************************************
 * InputSourceMeta:
 *****************************************************************************/
static void InputSourceMeta( input_thread_t *p_input,
                             input_source_t *p_source, vlc_meta_t *p_meta )
{
    access_t *p_access = p_source->p_access;
    demux_t *p_demux = p_source->p_demux;

    /* XXX Remember that checking against p_item->p_meta->i_status & ITEM_PREPARSED
     * is a bad idea */

    bool has_meta;

    /* Read access meta */
    has_meta = p_access && !access_Control( p_access, ACCESS_GET_META, p_meta );

    /* Read demux meta */
    has_meta |= !demux_Control( p_demux, DEMUX_GET_META, p_meta );

    bool has_unsupported;
    if( demux_Control( p_demux, DEMUX_HAS_UNSUPPORTED_META, &has_unsupported ) )
        has_unsupported = true;

    /* If the demux report unsupported meta data, or if we don't have meta data
     * try an external "meta reader" */
    if( has_meta && !has_unsupported )
        return;

    demux_meta_t *p_demux_meta =
        vlc_custom_create( p_demux, sizeof( *p_demux_meta ), "demux meta" );
    if( !p_demux_meta )
        return;
    p_demux_meta->p_demux = p_demux;
    p_demux_meta->p_item = p_input->p->p_item;

    module_t *p_id3 = module_need( p_demux_meta, "meta reader", NULL, false );
    if( p_id3 )
    {
        if( p_demux_meta->p_meta )
        {
            vlc_meta_Merge( p_meta, p_demux_meta->p_meta );
            vlc_meta_Delete( p_demux_meta->p_meta );
        }

        if( p_demux_meta->i_attachments > 0 )
        {
            vlc_mutex_lock( &p_input->p->p_item->lock );
            AppendAttachment( &p_input->p->i_attachment, &p_input->p->attachment,
                              p_demux_meta->i_attachments, p_demux_meta->attachments );
            vlc_mutex_unlock( &p_input->p->p_item->lock );
        }
        module_unneed( p_demux, p_id3 );
    }
    vlc_object_release( p_demux_meta );
}


static void SlaveDemux( input_thread_t *p_input, bool *pb_demux_polled )
{
    int64_t i_time;
    int i;

    *pb_demux_polled = false;
    if( demux_Control( p_input->p->input.p_demux, DEMUX_GET_TIME, &i_time ) )
    {
        msg_Err( p_input, "demux doesn't like DEMUX_GET_TIME" );
        return;
    }

    for( i = 0; i < p_input->p->i_slave; i++ )
    {
        input_source_t *in = p_input->p->slave[i];
        int i_ret;

        if( in->b_eof )
            continue;

        const bool b_demux_polled = in->p_demux->pf_demux != NULL;
        if( !b_demux_polled )
            continue;

        *pb_demux_polled = true;

        /* Call demux_Demux until we have read enough data */
        if( demux_Control( in->p_demux, DEMUX_SET_NEXT_DEMUX_TIME, i_time ) )
        {
            for( ;; )
            {
                int64_t i_stime;
                if( demux_Control( in->p_demux, DEMUX_GET_TIME, &i_stime ) )
                {
                    msg_Err( p_input, "slave[%d] doesn't like "
                             "DEMUX_GET_TIME -> EOF", i );
                    i_ret = 0;
                    break;
                }

                if( i_stime >= i_time )
                {
                    i_ret = 1;
                    break;
                }

                if( ( i_ret = demux_Demux( in->p_demux ) ) <= 0 )
                    break;
            }
        }
        else
        {
            i_ret = demux_Demux( in->p_demux );
        }

        if( i_ret <= 0 )
        {
            msg_Dbg( p_input, "slave %d EOF", i );
            in->b_eof = true;
        }
    }
}

static void SlaveSeek( input_thread_t *p_input )
{
    int64_t i_time;
    int i;

    if( demux_Control( p_input->p->input.p_demux, DEMUX_GET_TIME, &i_time ) )
    {
        msg_Err( p_input, "demux doesn't like DEMUX_GET_TIME" );
        return;
    }

    for( i = 0; i < p_input->p->i_slave; i++ )
    {
        input_source_t *in = p_input->p->slave[i];

        if( demux_Control( in->p_demux, DEMUX_SET_TIME, i_time, true ) )
        {
            if( !in->b_eof )
                msg_Err( p_input, "seek failed for slave %d -> EOF", i );
            in->b_eof = true;
        }
        else
        {
            in->b_eof = false;
        }
    }
}

/*****************************************************************************
 * InputMetaUser:
 *****************************************************************************/
static void InputMetaUser( input_thread_t *p_input, vlc_meta_t *p_meta )
{
    static const struct { int i_meta; const char *psz_name; } p_list[] = {
        { vlc_meta_Title,       "meta-title" },
        { vlc_meta_Artist,      "meta-artist" },
        { vlc_meta_Genre,       "meta-genre" },
        { vlc_meta_Copyright,   "meta-copyright" },
        { vlc_meta_Description, "meta-description" },
        { vlc_meta_Date,        "meta-date" },
        { vlc_meta_URL,         "meta-url" },
        { 0, NULL }
    };

    /* Get meta information from user */
    for( int i = 0; p_list[i].psz_name; i++ )
    {
        char *psz_string = var_GetNonEmptyString( p_input, p_list[i].psz_name );
        if( !psz_string )
            continue;

        EnsureUTF8( psz_string );
        vlc_meta_Set( p_meta, p_list[i].i_meta, psz_string );
        free( psz_string );
    }
}

/*****************************************************************************
 * InputUpdateMeta: merge p_item meta data with p_meta taking care of
 * arturl and locking issue.
 *****************************************************************************/
static void InputUpdateMeta( input_thread_t *p_input, vlc_meta_t *p_meta )
{
    es_out_ControlSetMeta( p_input->p->p_es_out, p_meta );
    vlc_meta_Delete( p_meta );
}

static void AppendAttachment( int *pi_attachment, input_attachment_t ***ppp_attachment,
                              int i_new, input_attachment_t **pp_new )
{
    int i_attachment = *pi_attachment;
    input_attachment_t **attachment = *ppp_attachment;
    int i;

    attachment = xrealloc( attachment,
                    sizeof(input_attachment_t**) * ( i_attachment + i_new ) );
    for( i = 0; i < i_new; i++ )
        attachment[i_attachment++] = pp_new[i];
    free( pp_new );

    /* */
    *pi_attachment = i_attachment;
    *ppp_attachment = attachment;
}
/*****************************************************************************
 * InputGetExtraFiles
 *  Autodetect extra input list
 *****************************************************************************/
static void InputGetExtraFilesPattern( input_thread_t *p_input,
                                       int *pi_list, char ***pppsz_list,
                                       const char *psz_path,
                                       const char *psz_match,
                                       const char *psz_format,
                                       int i_start, int i_stop )
{
    int i_list;
    char **ppsz_list;

    TAB_INIT( i_list, ppsz_list );

    char *psz_base = strdup( psz_path );
    if( !psz_base )
        goto exit;

    /* Remove the extension */
    char *psz_end = &psz_base[strlen(psz_base)-strlen(psz_match)];
    assert( psz_end >= psz_base);
    *psz_end = '\0';

    /* Try to list files */
    for( int i = i_start; i <= i_stop; i++ )
    {
        struct stat st;
        char *psz_file;

        if( asprintf( &psz_file, psz_format, psz_base, i ) < 0 )
            break;

        if( vlc_stat( psz_file, &st ) || !S_ISREG( st.st_mode ) || !st.st_size )
        {
            free( psz_file );
            break;
        }

        msg_Dbg( p_input, "Detected extra file `%s'", psz_file );
        TAB_APPEND( i_list, ppsz_list, psz_file );
    }
    free( psz_base );
exit:
    *pi_list = i_list;
    *pppsz_list = ppsz_list;
}

static void InputGetExtraFiles( input_thread_t *p_input,
                                int *pi_list, char ***pppsz_list,
                                const char *psz_access, const char *psz_path )
{
    static const struct
    {
        const char *psz_match;
        const char *psz_format;
        int i_start;
        int i_stop;
    } p_pattern[] = {
        /* XXX the order is important */
        { ".001",         "%s.%.3d",        2, 999 },
        { NULL, NULL, 0, 0 }
    };

    TAB_INIT( *pi_list, *pppsz_list );

    if( ( psz_access && *psz_access && strcmp( psz_access, "file" ) ) || !psz_path )
        return;

    const size_t i_path = strlen(psz_path);

    for( int i = 0; p_pattern[i].psz_match != NULL; i++ )
    {
        const size_t i_ext = strlen(p_pattern[i].psz_match );

        if( i_path < i_ext )
            continue;
        if( !strcmp( &psz_path[i_path-i_ext], p_pattern[i].psz_match ) )
        {
            InputGetExtraFilesPattern( p_input, pi_list, pppsz_list,
                                       psz_path,
                                       p_pattern[i].psz_match, p_pattern[i].psz_format,
                                       p_pattern[i].i_start, p_pattern[i].i_stop );
            return;
        }
    }
}

/* */
static void input_ChangeState( input_thread_t *p_input, int i_state )
{
    const bool b_changed = p_input->p->i_state != i_state;

    p_input->p->i_state = i_state;
    if( i_state == ERROR_S )
        p_input->b_error = true;
    else if( i_state == END_S )
        p_input->b_eof = true;

    if( b_changed )
    {
        input_item_SetErrorWhenReading( p_input->p->p_item, p_input->b_error );
        input_SendEventState( p_input, i_state );
    }
}


/*****************************************************************************
 * MRLSplit: parse the access, demux and url part of the
 *           Media Resource Locator.
 *****************************************************************************/
void input_SplitMRL( const char **access, const char **demux,
                     const char **path, const char **anchor, char *buf )
{
    char *p;

    /* Separate <path> from <access>[/<demux>]:// */
    p = strstr( buf, "://" );
    if( p != NULL )
    {
        *p = '\0';
        p += 3; /* skips "://" */
        *path = p;

        /* Remove HTML anchor if present (not supported).
         * The hash symbol itself should be URI-encoded. */
        p = strchr( p, '#' );
        if( p != NULL )
        {
            *(p++) = '\0';
            *anchor = p;
        }
        else
            *anchor = "";
    }
    else
    {
#ifndef NDEBUG
        fprintf( stderr, "%s(\"%s\") probably not a valid URI!\n", __func__,
                 buf );
#endif
        /* Note: this is a valid non const pointer to "": */
        *path = buf + strlen( buf );
    }

    /* Separate access from demux */
    p = strchr( buf, '/' );
    if( p != NULL )
    {
        *(p++) = '\0';
        if( p[0] == '$' )
            p++;
        *demux = p;
    }
    else
        *demux = "";

    /* We really don't want module name substitution here! */
    p = buf;
    if( p[0] == '$' )
        p++;
    *access = p;
}

static const char *MRLSeekPoint( const char *str, int *title, int *chapter )
{
    char *end;
    unsigned long u;

    /* Look for the title */
    u = strtoul( str, &end, 0 );
    *title = (str == end || u > (unsigned long)INT_MAX) ? -1 : (int)u;
    str = end;

    /* Look for the chapter */
    if( *str == ':' )
    {
        str++;
        u = strtoul( str, &end, 0 );
        *chapter = (str == end || u > (unsigned long)INT_MAX) ? -1 : (int)u;
        str = end;
    }
    else
        *chapter = -1;

    return str;
}


/*****************************************************************************
 * MRLSections: parse title and seekpoint info from the Media Resource Locator.
 *
 * Syntax:
 * [url][@[title_start][:chapter_start][-[title_end][:chapter_end]]]
 *****************************************************************************/
static void MRLSections( const char *p,
                         int *pi_title_start, int *pi_title_end,
                         int *pi_chapter_start, int *pi_chapter_end )
{
    *pi_title_start = *pi_title_end = *pi_chapter_start = *pi_chapter_end = -1;

    int title_start, chapter_start, title_end, chapter_end;

    if( !p )
        return;

    if( *p != '-' )
        p = MRLSeekPoint( p, &title_start, &chapter_start );
    else
        title_start = chapter_start = -1;

    if( *p == '-' )
        p = MRLSeekPoint( p + 1, &title_end, &chapter_end );
    else
        title_end = chapter_end = -1;

    if( *p ) /* syntax error */
        return;

    *pi_title_start = title_start;
    *pi_title_end = title_end;
    *pi_chapter_start = chapter_start;
    *pi_chapter_end = chapter_end;
}

/*****************************************************************************
 * input_AddSubtitles: add a subtitles file and enable it
 *****************************************************************************/
static void SubtitleAdd( input_thread_t *p_input, char *psz_subtitle, unsigned i_flags )
{
    input_source_t *sub;
    vlc_value_t count;
    vlc_value_t list;
    char *psz_path, *psz_extension;

    /* if we are provided a subtitle.sub file,
     * see if we don't have a subtitle.idx and use it instead */
    psz_path = strdup( psz_subtitle );
    if( psz_path )
    {
        psz_extension = strrchr( psz_path, '.');
        if( psz_extension && strcmp( psz_extension, ".sub" ) == 0 )
        {
            struct stat st;

            strcpy( psz_extension, ".idx" );

            if( !vlc_stat( psz_path, &st ) && S_ISREG( st.st_mode ) )
            {
                msg_Dbg( p_input, "using %s subtitles file instead of %s",
                         psz_path, psz_subtitle );
                strcpy( psz_subtitle, psz_path );
            }
        }
        free( psz_path );
    }

    char *url = make_URI( psz_subtitle, "file" );

    var_Change( p_input, "spu-es", VLC_VAR_CHOICESCOUNT, &count, NULL );

    sub = InputSourceNew( p_input );
    if( !sub || !url
     || InputSourceInit( p_input, sub, url, "subtitle", (i_flags & SUB_CANFAIL) ) )
    {
        free( sub );
        free( url );
        return;
    }
    free( url );
    TAB_APPEND( p_input->p->i_slave, p_input->p->slave, sub );

    /* Select the ES */
    if( (i_flags & SUB_FORCED) && !var_Change( p_input, "spu-es", VLC_VAR_GETLIST, &list, NULL ) )
    {
        if( count.i_int == 0 )
            count.i_int++;
        /* if it was first one, there is disable too */

        if( count.i_int < list.p_list->i_count )
        {
            const int i_id = list.p_list->p_values[count.i_int].i_int;

            es_out_Control( p_input->p->p_es_out_display, ES_OUT_SET_ES_DEFAULT_BY_ID, i_id );
            es_out_Control( p_input->p->p_es_out_display, ES_OUT_SET_ES_BY_ID, i_id );
        }
        var_FreeList( &list, NULL );
    }
}

/*****************************************************************************
 * Statistics
 *****************************************************************************/
void input_UpdateStatistic( input_thread_t *p_input,
                            input_statistic_t i_type, int i_delta )
{
    assert( p_input->p->i_state != INIT_S );

    vlc_mutex_lock( &p_input->p->counters.counters_lock);
    switch( i_type )
    {
#define I(c) stats_Update( p_input->p->counters.c, i_delta, NULL )
    case INPUT_STATISTIC_DECODED_VIDEO:
        I(p_decoded_video);
        break;
    case INPUT_STATISTIC_DECODED_AUDIO:
        I(p_decoded_audio);
        break;
    case INPUT_STATISTIC_DECODED_SUBTITLE:
        I(p_decoded_sub);
        break;
    case INPUT_STATISTIC_SENT_PACKET:
        I(p_sout_sent_packets);
        break;
#undef I
    case INPUT_STATISTIC_SENT_BYTE:
    {
        uint64_t bytes;

        stats_Update( p_input->p->counters.p_sout_sent_bytes, i_delta, &bytes );
        stats_Update( p_input->p->counters.p_sout_send_bitrate, bytes, NULL );
        break;
    }
    default:
        msg_Err( p_input, "Invalid statistic type %d (internal error)", i_type );
        break;
    }
    vlc_mutex_unlock( &p_input->p->counters.counters_lock);
}

/**/
/* TODO FIXME nearly the same logic that snapshot code */
char *input_CreateFilename( vlc_object_t *p_obj, const char *psz_path, const char *psz_prefix, const char *psz_extension )
{
    char *psz_file;
    DIR *path;

    path = vlc_opendir( psz_path );
    if( path )
    {
        closedir( path );

        char *psz_tmp = str_format( p_obj, psz_prefix );
        if( !psz_tmp )
            return NULL;

        filename_sanitize( psz_tmp );

        if( asprintf( &psz_file, "%s"DIR_SEP"%s%s%s",
                      psz_path, psz_tmp,
                      psz_extension ? "." : "",
                      psz_extension ? psz_extension : "" ) < 0 )
            psz_file = NULL;
        free( psz_tmp );
        return psz_file;
    }
    else
    {
        psz_file = str_format( p_obj, psz_path );
        path_sanitize( psz_file );
        return psz_file;
    }
}

