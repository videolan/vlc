/*****************************************************************************
 * vlm.c: VLM interface plugin
 *****************************************************************************
 * Copyright (C) 2000-2005 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Simon Latapie <garf@videolan.org>
 *          Laurent Aimar <fenrir@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
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

#include <stdio.h>
#include <ctype.h>                                              /* tolower() */
#include <time.h>                                                 /* ctime() */
#include <limits.h>
#include <assert.h>
#include <sys/time.h>                                      /* gettimeofday() */

#include <vlc_vlm.h>
#include <vlc_modules.h>

#include <vlc_input.h>
#include <vlc_stream.h>
#include "vlm_internal.h"
#include "vlm_event.h"
#include <vlc_vod.h>
#include <vlc_sout.h>
#include <vlc_url.h>
#include "../stream_output/stream_output.h"
#include "../libvlc.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

static void* Manage( void * );
static int vlm_MediaVodControl( void *, vod_media_t *, const char *, int, va_list );

typedef struct preparse_data_t
{
    vlc_sem_t *p_sem;
    bool b_mux;
} preparse_data_t;

static int InputEventPreparse( vlc_object_t *p_this, char const *psz_cmd,
                               vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    preparse_data_t *p_pre = p_data;

    if( newval.i_int == INPUT_EVENT_DEAD ||
        ( p_pre->b_mux && newval.i_int == INPUT_EVENT_ITEM_META ) )
        vlc_sem_post( p_pre->p_sem );

    return VLC_SUCCESS;
}

static int InputEvent( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval,
                       void *p_data )
{
    VLC_UNUSED(psz_cmd);
    VLC_UNUSED(oldval);
    input_thread_t *p_input = (input_thread_t *)p_this;
    vlm_t *p_vlm = libvlc_priv( p_input->p_libvlc )->p_vlm;
    assert( p_vlm );
    vlm_media_sys_t *p_media = p_data;
    const char *psz_instance_name = NULL;

    if( newval.i_int == INPUT_EVENT_STATE )
    {
        for( int i = 0; i < p_media->i_instance; i++ )
        {
            if( p_media->instance[i]->p_input == p_input )
            {
                psz_instance_name = p_media->instance[i]->psz_name;
                break;
            }
        }
        vlm_SendEventMediaInstanceState( p_vlm, p_media->cfg.id, p_media->cfg.psz_name, psz_instance_name, var_GetInteger( p_input, "state" ) );

        vlc_mutex_lock( &p_vlm->lock_manage );
        p_vlm->input_state_changed = true;
        vlc_cond_signal( &p_vlm->wait_manage );
        vlc_mutex_unlock( &p_vlm->lock_manage );
    }
    return VLC_SUCCESS;
}

static vlc_mutex_t vlm_mutex = VLC_STATIC_MUTEX;

#undef vlm_New
/*****************************************************************************
 * vlm_New:
 *****************************************************************************/
vlm_t *vlm_New ( vlc_object_t *p_this )
{
    vlm_t *p_vlm = NULL, **pp_vlm = &(libvlc_priv (p_this->p_libvlc)->p_vlm);
    char *psz_vlmconf;

    /* Avoid multiple creation */
    vlc_mutex_lock( &vlm_mutex );

    p_vlm = *pp_vlm;
    if( p_vlm )
    {   /* VLM already exists */
        if( likely( p_vlm->users < UINT_MAX ) )
            p_vlm->users++;
        else
            p_vlm = NULL;
        vlc_mutex_unlock( &vlm_mutex );
        return p_vlm;
    }

    msg_Dbg( p_this, "creating VLM" );

    p_vlm = vlc_custom_create( p_this->p_libvlc, sizeof( *p_vlm ),
                               "vlm daemon" );
    if( !p_vlm )
    {
        vlc_mutex_unlock( &vlm_mutex );
        return NULL;
    }

    vlc_mutex_init( &p_vlm->lock );
    vlc_mutex_init( &p_vlm->lock_manage );
    vlc_cond_init_daytime( &p_vlm->wait_manage );
    p_vlm->users = 1;
    p_vlm->input_state_changed = false;
    p_vlm->i_id = 1;
    TAB_INIT( p_vlm->i_media, p_vlm->media );
    TAB_INIT( p_vlm->i_schedule, p_vlm->schedule );
    p_vlm->p_vod = NULL;
    var_Create( p_vlm, "intf-event", VLC_VAR_ADDRESS );

    if( vlc_clone( &p_vlm->thread, Manage, p_vlm, VLC_THREAD_PRIORITY_LOW ) )
    {
        vlc_cond_destroy( &p_vlm->wait_manage );
        vlc_mutex_destroy( &p_vlm->lock );
        vlc_mutex_destroy( &p_vlm->lock_manage );
        vlc_object_release( p_vlm );
        vlc_mutex_unlock( &vlm_mutex );
        return NULL;
    }

    *pp_vlm = p_vlm; /* for future reference */

    /* Load our configuration file */
    psz_vlmconf = var_CreateGetString( p_vlm, "vlm-conf" );
    if( psz_vlmconf && *psz_vlmconf )
    {
        vlm_message_t *p_message = NULL;
        char *psz_buffer = NULL;

        msg_Dbg( p_this, "loading VLM configuration" );
        if( asprintf(&psz_buffer, "load %s", psz_vlmconf ) != -1 )
        {
            msg_Dbg( p_this, "%s", psz_buffer );
            if( vlm_ExecuteCommand( p_vlm, psz_buffer, &p_message ) )
                msg_Warn( p_this, "error while loading the configuration file" );

            vlm_MessageDelete( p_message );
            free( psz_buffer );
        }
    }
    free( psz_vlmconf );

    vlc_mutex_unlock( &vlm_mutex );

    return p_vlm;
}

/*****************************************************************************
 * vlm_Delete:
 *****************************************************************************/
void vlm_Delete( vlm_t *p_vlm )
{
    /* vlm_Delete() is serialized against itself, and against vlm_New().
     * This mutex protects libvlc_priv->p_vlm and p_vlm->users. */
    vlc_mutex_lock( &vlm_mutex );
    assert( p_vlm->users > 0 );
    if( --p_vlm->users == 0 )
        assert( libvlc_priv(p_vlm->p_libvlc)->p_vlm == p_vlm );
    else
        p_vlm = NULL;

    if( p_vlm == NULL )
    {
        vlc_mutex_unlock( &vlm_mutex );
        return;
    }

    /* Destroy and release VLM */
    vlc_mutex_lock( &p_vlm->lock );
    vlm_ControlInternal( p_vlm, VLM_CLEAR_MEDIAS );
    TAB_CLEAN( p_vlm->i_media, p_vlm->media );

    vlm_ControlInternal( p_vlm, VLM_CLEAR_SCHEDULES );
    TAB_CLEAN( p_vlm->i_schedule, p_vlm->schedule );
    vlc_mutex_unlock( &p_vlm->lock );

    vlc_cancel( p_vlm->thread );

    if( p_vlm->p_vod )
    {
        module_unneed( p_vlm->p_vod, p_vlm->p_vod->p_module );
        vlc_object_release( p_vlm->p_vod );
    }

    libvlc_priv(p_vlm->p_libvlc)->p_vlm = NULL;
    vlc_mutex_unlock( &vlm_mutex );

    vlc_join( p_vlm->thread, NULL );

    vlc_cond_destroy( &p_vlm->wait_manage );
    vlc_mutex_destroy( &p_vlm->lock );
    vlc_mutex_destroy( &p_vlm->lock_manage );
    vlc_object_release( p_vlm );
}

/*****************************************************************************
 * vlm_ExecuteCommand:
 *****************************************************************************/
int vlm_ExecuteCommand( vlm_t *p_vlm, const char *psz_command,
                        vlm_message_t **pp_message)
{
    int i_result;

    vlc_mutex_lock( &p_vlm->lock );
    i_result = ExecuteCommand( p_vlm, psz_command, pp_message );
    vlc_mutex_unlock( &p_vlm->lock );

    return i_result;
}


int64_t vlm_Date(void)
{
    struct timeval tv;

    (void)gettimeofday( &tv, NULL );
    return tv.tv_sec * INT64_C(1000000) + tv.tv_usec;
}


/*****************************************************************************
 *
 *****************************************************************************/
static int vlm_MediaVodControl( void *p_private, vod_media_t *p_vod_media,
                                const char *psz_id, int i_query, va_list args )
{
    vlm_t *vlm = (vlm_t *)p_private;
    int i, i_ret;
    const char *psz;
    int64_t id;

    if( !p_private || !p_vod_media )
        return VLC_EGENERIC;

    vlc_mutex_lock( &vlm->lock );

    /* Find media id */
    for( i = 0, id = -1; i < vlm->i_media; i++ )
    {
        if( p_vod_media == vlm->media[i]->vod.p_media )
        {
            id = vlm->media[i]->cfg.id;
            break;
        }
    }
    if( id == -1 )
    {
        vlc_mutex_unlock( &vlm->lock );
        return VLC_EGENERIC;
    }

    switch( i_query )
    {
    case VOD_MEDIA_PLAY:
    {
        psz = (const char *)va_arg( args, const char * );
        int64_t *i_time = (int64_t *)va_arg( args, int64_t *);
        bool b_retry = false;
        if (*i_time < 0)
        {
            /* No start time requested: return the current NPT */
            i_ret = vlm_ControlInternal( vlm, VLM_GET_MEDIA_INSTANCE_TIME, id, psz_id, i_time );
            /* The instance is not running yet, it will start at 0 */
            if (i_ret)
                *i_time = 0;
        }
        else
        {
            /* We want to seek before unpausing, but it won't
             * work if the instance is not running yet. */
            b_retry = vlm_ControlInternal( vlm, VLM_SET_MEDIA_INSTANCE_TIME, id, psz_id, *i_time );
        }

        i_ret = vlm_ControlInternal( vlm, VLM_START_MEDIA_VOD_INSTANCE, id, psz_id, 0, psz );

        if (!i_ret && b_retry)
            i_ret = vlm_ControlInternal( vlm, VLM_SET_MEDIA_INSTANCE_TIME, id, psz_id, *i_time );
        break;
    }

    case VOD_MEDIA_PAUSE:
    {
        int64_t *i_time = (int64_t *)va_arg( args, int64_t *);
        i_ret = vlm_ControlInternal( vlm, VLM_PAUSE_MEDIA_INSTANCE, id, psz_id );
        if (!i_ret)
            i_ret = vlm_ControlInternal( vlm, VLM_GET_MEDIA_INSTANCE_TIME, id, psz_id, i_time );
        break;
    }

    case VOD_MEDIA_STOP:
        i_ret = vlm_ControlInternal( vlm, VLM_STOP_MEDIA_INSTANCE, id, psz_id );
        break;

    case VOD_MEDIA_SEEK:
    {
        int64_t i_time = (int64_t)va_arg( args, int64_t );
        i_ret = vlm_ControlInternal( vlm, VLM_SET_MEDIA_INSTANCE_TIME, id, psz_id, i_time );
        break;
    }

    case VOD_MEDIA_REWIND:
    {
        double d_scale = (double)va_arg( args, double );
        double d_position;

        vlm_ControlInternal( vlm, VLM_GET_MEDIA_INSTANCE_POSITION, id, psz_id, &d_position );
        d_position -= (d_scale / 1000.0);
        if( d_position < 0.0 )
            d_position = 0.0;
        i_ret = vlm_ControlInternal( vlm, VLM_SET_MEDIA_INSTANCE_POSITION, id, psz_id, d_position );
        break;
    }

    case VOD_MEDIA_FORWARD:
    {
        double d_scale = (double)va_arg( args, double );
        double d_position;

        vlm_ControlInternal( vlm, VLM_GET_MEDIA_INSTANCE_POSITION, id, psz_id, &d_position );
        d_position += (d_scale / 1000.0);
        if( d_position > 1.0 )
            d_position = 1.0;
        i_ret = vlm_ControlInternal( vlm, VLM_SET_MEDIA_INSTANCE_POSITION, id, psz_id, d_position );
        break;
    }

    default:
        i_ret = VLC_EGENERIC;
        break;
    }

    vlc_mutex_unlock( &vlm->lock );

    return i_ret;
}


/*****************************************************************************
 * Manage:
 *****************************************************************************/
static void* Manage( void* p_object )
{
    vlm_t *vlm = (vlm_t*)p_object;
    int i, j;
    mtime_t i_lastcheck;
    mtime_t i_time;
    mtime_t i_nextschedule = 0;

    i_lastcheck = vlm_Date();

    for( ;; )
    {
        char **ppsz_scheduled_commands = NULL;
        int    i_scheduled_commands = 0;
        bool scheduled_command = false;

        vlc_mutex_lock( &vlm->lock_manage );
        mutex_cleanup_push( &vlm->lock_manage );
        while( !vlm->input_state_changed && !scheduled_command )
        {
            if( i_nextschedule )
                scheduled_command = vlc_cond_timedwait( &vlm->wait_manage, &vlm->lock_manage, i_nextschedule ) != 0;
            else
                vlc_cond_wait( &vlm->wait_manage, &vlm->lock_manage );
        }
        vlm->input_state_changed = false;
        vlc_cleanup_run( );

        int canc = vlc_savecancel ();
        /* destroy the inputs that wants to die, and launch the next input */
        vlc_mutex_lock( &vlm->lock );
        for( i = 0; i < vlm->i_media; i++ )
        {
            vlm_media_sys_t *p_media = vlm->media[i];

            for( j = 0; j < p_media->i_instance; )
            {
                vlm_media_instance_sys_t *p_instance = p_media->instance[j];

                if( p_instance->p_input && ( p_instance->p_input->b_eof || p_instance->p_input->b_error ) )
                {
                    int i_new_input_index;

                    /* */
                    i_new_input_index = p_instance->i_index + 1;
                    if( !p_media->cfg.b_vod && p_media->cfg.broadcast.b_loop && i_new_input_index >= p_media->cfg.i_input )
                        i_new_input_index = 0;

                    /* FIXME implement multiple input with VOD */
                    if( p_media->cfg.b_vod || i_new_input_index >= p_media->cfg.i_input )
                        vlm_ControlInternal( vlm, VLM_STOP_MEDIA_INSTANCE, p_media->cfg.id, p_instance->psz_name );
                    else
                        vlm_ControlInternal( vlm, VLM_START_MEDIA_BROADCAST_INSTANCE, p_media->cfg.id, p_instance->psz_name, i_new_input_index );

                    j = 0;
                }
                else
                {
                    j++;
                }
            }
        }

        /* scheduling */
        i_time = vlm_Date();
        i_nextschedule = 0;

        for( i = 0; i < vlm->i_schedule; i++ )
        {
            mtime_t i_real_date = vlm->schedule[i]->i_date;

            if( vlm->schedule[i]->b_enabled )
            {
                if( vlm->schedule[i]->i_date == 0 ) // now !
                {
                    vlm->schedule[i]->i_date = (i_time / 1000000) * 1000000 ;
                    i_real_date = i_time;
                }
                else if( vlm->schedule[i]->i_period != 0 )
                {
                    int j = 0;
                    while( vlm->schedule[i]->i_date + j *
                           vlm->schedule[i]->i_period <= i_lastcheck &&
                           ( vlm->schedule[i]->i_repeat > j ||
                             vlm->schedule[i]->i_repeat == -1 ) )
                    {
                        j++;
                    }

                    i_real_date = vlm->schedule[i]->i_date + j *
                        vlm->schedule[i]->i_period;
                }

                if( i_real_date <= i_time )
                {
                    if( i_real_date > i_lastcheck )
                    {
                        for( j = 0; j < vlm->schedule[i]->i_command; j++ )
                        {
                            TAB_APPEND( i_scheduled_commands,
                                        ppsz_scheduled_commands,
                                        strdup(vlm->schedule[i]->command[j] ) );
                        }
                    }
                }
                else if( i_nextschedule == 0 || i_real_date < i_nextschedule )
                {
                    i_nextschedule = i_real_date;
                }
            }
        }

        while( i_scheduled_commands )
        {
            vlm_message_t *message = NULL;
            char *psz_command = ppsz_scheduled_commands[0];
            ExecuteCommand( vlm, psz_command,&message );

            /* for now, drop the message */
            vlm_MessageDelete( message );
            TAB_REMOVE( i_scheduled_commands,
                        ppsz_scheduled_commands,
                        psz_command );
            free( psz_command );
        }

        i_lastcheck = i_time;
        vlc_mutex_unlock( &vlm->lock );
        vlc_restorecancel (canc);
    }

    return NULL;
}

/* New API
 */
/*
typedef struct
{
    struct
    {
        int i_connection_count;
        int i_connection_active;
    } vod;
    struct
    {
        int        i_count;
        bool b_playing;
        int        i_playing_index;
    } broadcast;

} vlm_media_status_t;
*/

/* */
static vlm_media_sys_t *vlm_ControlMediaGetById( vlm_t *p_vlm, int64_t id )
{
    int i;

    for( i = 0; i < p_vlm->i_media; i++ )
    {
        if( p_vlm->media[i]->cfg.id == id )
            return p_vlm->media[i];
    }
    return NULL;
}
static vlm_media_sys_t *vlm_ControlMediaGetByName( vlm_t *p_vlm, const char *psz_name )
{
    int i;

    for( i = 0; i < p_vlm->i_media; i++ )
    {
        if( !strcmp( p_vlm->media[i]->cfg.psz_name, psz_name ) )
            return p_vlm->media[i];
    }
    return NULL;
}
static int vlm_MediaDescriptionCheck( vlm_t *p_vlm, vlm_media_t *p_cfg )
{
    int i;

    if( !p_cfg || !p_cfg->psz_name ||
        !strcmp( p_cfg->psz_name, "all" ) || !strcmp( p_cfg->psz_name, "media" ) || !strcmp( p_cfg->psz_name, "schedule" ) )
        return VLC_EGENERIC;

    for( i = 0; i < p_vlm->i_media; i++ )
    {
        if( p_vlm->media[i]->cfg.id == p_cfg->id )
            continue;
        if( !strcmp( p_vlm->media[i]->cfg.psz_name, p_cfg->psz_name ) )
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}


/* Called after a media description is changed/added */
static int vlm_OnMediaUpdate( vlm_t *p_vlm, vlm_media_sys_t *p_media )
{
    vlm_media_t *p_cfg = &p_media->cfg;
    /* Check if we need to create/delete a vod media */
    if( p_cfg->b_vod && p_vlm->p_vod )
    {
        if( !p_cfg->b_enabled && p_media->vod.p_media )
        {
            p_vlm->p_vod->pf_media_del( p_vlm->p_vod, p_media->vod.p_media );
            p_media->vod.p_media = NULL;
        }
        else if( p_cfg->b_enabled && !p_media->vod.p_media && p_cfg->i_input )
        {
            /* Pre-parse the input */
            input_thread_t *p_input;
            char *psz_output;
            char *psz_header;
            char *psz_dup;
            int i;

            vlc_gc_decref( p_media->vod.p_item );

            if( strstr( p_cfg->ppsz_input[0], "://" ) == NULL )
            {
                char *psz_uri = vlc_path2uri( p_cfg->ppsz_input[0], NULL );
                p_media->vod.p_item = input_item_New( psz_uri,
                                                      p_cfg->psz_name );
                free( psz_uri );
            }
            else
                p_media->vod.p_item = input_item_New( p_cfg->ppsz_input[0],
                                                      p_cfg->psz_name );

            if( p_cfg->psz_output )
            {
                if( asprintf( &psz_output, "%s:description", p_cfg->psz_output )  == -1 )
                    psz_output = NULL;
            }
            else
                psz_output = strdup( "#description" );

            if( psz_output && asprintf( &psz_dup, "sout=%s", psz_output ) != -1 )
            {
                input_item_AddOption( p_media->vod.p_item, psz_dup, VLC_INPUT_OPTION_TRUSTED );
                free( psz_dup );
            }
            free( psz_output );

            for( i = 0; i < p_cfg->i_option; i++ )
                input_item_AddOption( p_media->vod.p_item,
                                      p_cfg->ppsz_option[i], VLC_INPUT_OPTION_TRUSTED );

            if( asprintf( &psz_header, _("Media: %s"), p_cfg->psz_name ) == -1 )
                psz_header = NULL;

            sout_description_data_t data;
            TAB_INIT(data.i_es, data.es);

            p_input = input_Create( p_vlm->p_vod, p_media->vod.p_item, psz_header, NULL );
            if( p_input )
            {
                vlc_sem_t sem_preparse;
                vlc_sem_init( &sem_preparse, 0 );

                preparse_data_t preparse = { .p_sem = &sem_preparse,
                                    .b_mux = (p_cfg->vod.psz_mux != NULL) };
                var_AddCallback( p_input, "intf-event", InputEventPreparse,
                                 &preparse );

                data.sem = &sem_preparse;
                var_Create( p_input, "sout-description-data", VLC_VAR_ADDRESS );
                var_SetAddress( p_input, "sout-description-data", &data );

                if( !input_Start( p_input ) )
                    vlc_sem_wait( &sem_preparse );

                var_DelCallback( p_input, "intf-event", InputEventPreparse,
                                 &preparse );

                input_Stop( p_input, true );
                input_Close( p_input );
                vlc_sem_destroy( &sem_preparse );
            }
            free( psz_header );

            /* XXX: Don't do it that way, but properly use a new input item ref. */
            input_item_t item = *p_media->vod.p_item;;
            if( p_cfg->vod.psz_mux )
            {
                const char *psz_mux;
                if (!strcmp(p_cfg->vod.psz_mux, "ps"))
                    psz_mux = "mp2p";
                else if (!strcmp(p_cfg->vod.psz_mux, "ts"))
                    psz_mux = "mp2t";
                else
                    psz_mux = p_cfg->vod.psz_mux;

                es_format_t es, *p_es = &es;
                union {
                    char text[5];
                    unsigned char utext[5];
                    uint32_t value;
                } fourcc;

                sprintf( fourcc.text, "%4.4s", psz_mux );
                for( int i = 0; i < 4; i++ )
                    fourcc.utext[i] = tolower(fourcc.utext[i]);

                item.i_es = 1;
                item.es = &p_es;
                es_format_Init( &es, VIDEO_ES, fourcc.value );
            }
            else
            {
                item.i_es = data.i_es;
                item.es = data.es;
            }
            p_media->vod.p_media = p_vlm->p_vod->pf_media_new( p_vlm->p_vod,
                                                    p_cfg->psz_name, &item );

            TAB_CLEAN(data.i_es, data.es);
        }
    }
    else if ( p_cfg->b_vod )
        msg_Err( p_vlm, "vod server is not loaded" );
    else
    {
        /* TODO start media if needed */
    }

    /* TODO add support of var vlm_media_broadcast/vlm_media_vod */

    vlm_SendEventMediaChanged( p_vlm, p_cfg->id, p_cfg->psz_name );
    return VLC_SUCCESS;
}
static int vlm_ControlMediaChange( vlm_t *p_vlm, vlm_media_t *p_cfg )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, p_cfg->id );

    /* */
    if( !p_media || vlm_MediaDescriptionCheck( p_vlm, p_cfg ) )
        return VLC_EGENERIC;
    if( ( p_media->cfg.b_vod && !p_cfg->b_vod ) || ( !p_media->cfg.b_vod && p_cfg->b_vod ) )
        return VLC_EGENERIC;

    if( 0 )
    {
        /* TODO check what are the changes being done (stop instance if needed) */
    }

    vlm_media_Clean( &p_media->cfg );
    vlm_media_Copy( &p_media->cfg, p_cfg );

    return vlm_OnMediaUpdate( p_vlm, p_media );
}

static int vlm_ControlMediaAdd( vlm_t *p_vlm, vlm_media_t *p_cfg, int64_t *p_id )
{
    vlm_media_sys_t *p_media;

    if( vlm_MediaDescriptionCheck( p_vlm, p_cfg ) || vlm_ControlMediaGetByName( p_vlm, p_cfg->psz_name ) )
    {
        msg_Err( p_vlm, "invalid media description" );
        return VLC_EGENERIC;
    }
    /* Check if we need to load the VOD server */
    if( p_cfg->b_vod && !p_vlm->p_vod )
    {
        p_vlm->p_vod = vlc_custom_create( VLC_OBJECT(p_vlm), sizeof( vod_t ),
                                          "vod server" );
        p_vlm->p_vod->p_module = module_need( p_vlm->p_vod, "vod server", "$vod-server", false );
        if( !p_vlm->p_vod->p_module )
        {
            msg_Err( p_vlm, "cannot find vod server" );
            vlc_object_release( p_vlm->p_vod );
            p_vlm->p_vod = NULL;
            return VLC_EGENERIC;
        }

        p_vlm->p_vod->p_data = p_vlm;
        p_vlm->p_vod->pf_media_control = vlm_MediaVodControl;
    }

    p_media = calloc( 1, sizeof( vlm_media_sys_t ) );
    if( !p_media )
        return VLC_ENOMEM;

    vlm_media_Copy( &p_media->cfg, p_cfg );
    p_media->cfg.id = p_vlm->i_id++;
    /* FIXME do we do something here if enabled is true ? */

    p_media->vod.p_item = input_item_New( NULL, NULL );

    p_media->vod.p_media = NULL;
    TAB_INIT( p_media->i_instance, p_media->instance );

    /* */
    TAB_APPEND( p_vlm->i_media, p_vlm->media, p_media );

    if( p_id )
        *p_id = p_media->cfg.id;

    /* */
    vlm_SendEventMediaAdded( p_vlm, p_media->cfg.id, p_media->cfg.psz_name );
    return vlm_OnMediaUpdate( p_vlm, p_media );
}

static int vlm_ControlMediaDel( vlm_t *p_vlm, int64_t id )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );

    if( !p_media )
        return VLC_EGENERIC;

    while( p_media->i_instance > 0 )
        vlm_ControlInternal( p_vlm, VLM_STOP_MEDIA_INSTANCE, id, p_media->instance[0]->psz_name );

    if( p_media->cfg.b_vod )
    {
        p_media->cfg.b_enabled = false;
        vlm_OnMediaUpdate( p_vlm, p_media );
    }

    /* */
    vlm_SendEventMediaRemoved( p_vlm, id, p_media->cfg.psz_name );

    vlm_media_Clean( &p_media->cfg );

    vlc_gc_decref( p_media->vod.p_item );

    if( p_media->vod.p_media )
        p_vlm->p_vod->pf_media_del( p_vlm->p_vod, p_media->vod.p_media );

    TAB_REMOVE( p_vlm->i_media, p_vlm->media, p_media );
    free( p_media );

    return VLC_SUCCESS;
}

static int vlm_ControlMediaGets( vlm_t *p_vlm, vlm_media_t ***ppp_dsc, int *pi_dsc )
{
    vlm_media_t **pp_dsc;
    int                     i_dsc;
    int i;

    TAB_INIT( i_dsc, pp_dsc );
    for( i = 0; i < p_vlm->i_media; i++ )
    {
        vlm_media_t *p_dsc = vlm_media_Duplicate( &p_vlm->media[i]->cfg );
        TAB_APPEND( i_dsc, pp_dsc, p_dsc );
    }

    *ppp_dsc = pp_dsc;
    *pi_dsc = i_dsc;

    return VLC_SUCCESS;
}
static int vlm_ControlMediaClear( vlm_t *p_vlm )
{
    while( p_vlm->i_media > 0 )
        vlm_ControlMediaDel( p_vlm, p_vlm->media[0]->cfg.id );

    return VLC_SUCCESS;
}
static int vlm_ControlMediaGet( vlm_t *p_vlm, int64_t id, vlm_media_t **pp_dsc )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    if( !p_media )
        return VLC_EGENERIC;

    *pp_dsc = vlm_media_Duplicate( &p_media->cfg );
    return VLC_SUCCESS;
}
static int vlm_ControlMediaGetId( vlm_t *p_vlm, const char *psz_name, int64_t *p_id )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetByName( p_vlm, psz_name );
    if( !p_media )
        return VLC_EGENERIC;

    *p_id = p_media->cfg.id;
    return VLC_SUCCESS;
}

static vlm_media_instance_sys_t *vlm_ControlMediaInstanceGetByName( vlm_media_sys_t *p_media, const char *psz_id )
{
    int i;

    for( i = 0; i < p_media->i_instance; i++ )
    {
        const char *psz = p_media->instance[i]->psz_name;
        if( ( psz == NULL && psz_id == NULL ) ||
            ( psz && psz_id && !strcmp( psz, psz_id ) ) )
            return p_media->instance[i];
    }
    return NULL;
}
static vlm_media_instance_sys_t *vlm_MediaInstanceNew( vlm_t *p_vlm, const char *psz_name )
{
    vlm_media_instance_sys_t *p_instance = calloc( 1, sizeof(vlm_media_instance_sys_t) );
    if( !p_instance )
        return NULL;

    p_instance->psz_name = NULL;
    if( psz_name )
        p_instance->psz_name = strdup( psz_name );

    p_instance->p_item = input_item_New( NULL, NULL );

    p_instance->i_index = 0;
    p_instance->b_sout_keep = false;
    p_instance->p_parent = vlc_object_create( p_vlm, sizeof (vlc_object_t) );
    p_instance->p_input = NULL;
    p_instance->p_input_resource = input_resource_New( p_instance->p_parent );

    return p_instance;
}
static void vlm_MediaInstanceDelete( vlm_t *p_vlm, int64_t id, vlm_media_instance_sys_t *p_instance, vlm_media_sys_t *p_media )
{
    input_thread_t *p_input = p_instance->p_input;
    if( p_input )
    {
        input_Stop( p_input, true );
        input_Join( p_input );
        var_DelCallback( p_instance->p_input, "intf-event", InputEvent, p_media );
        input_Release( p_input );

        vlm_SendEventMediaInstanceStopped( p_vlm, id, p_media->cfg.psz_name );
    }
    input_resource_Terminate( p_instance->p_input_resource );
    input_resource_Release( p_instance->p_input_resource );
    vlc_object_release( p_instance->p_parent );

    TAB_REMOVE( p_media->i_instance, p_media->instance, p_instance );
    vlc_gc_decref( p_instance->p_item );
    free( p_instance->psz_name );
    free( p_instance );
}


static int vlm_ControlMediaInstanceStart( vlm_t *p_vlm, int64_t id, const char *psz_id, int i_input_index, const char *psz_vod_output )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;
    char *psz_log;

    if( !p_media || !p_media->cfg.b_enabled || p_media->cfg.i_input <= 0 )
        return VLC_EGENERIC;

    /* TODO support multiple input for VOD with sout-keep ? */

    if( ( p_media->cfg.b_vod && !psz_vod_output ) || ( !p_media->cfg.b_vod && psz_vod_output ) )
        return VLC_EGENERIC;

    if( i_input_index < 0 || i_input_index >= p_media->cfg.i_input )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance )
    {
        vlm_media_t *p_cfg = &p_media->cfg;
        int i;

        p_instance = vlm_MediaInstanceNew( p_vlm, psz_id );
        if( !p_instance )
            return VLC_ENOMEM;

        if ( p_cfg->b_vod )
        {
            var_Create( p_instance->p_parent, "vod-media", VLC_VAR_ADDRESS );
            var_SetAddress( p_instance->p_parent, "vod-media",
                            p_media->vod.p_media );
            var_Create( p_instance->p_parent, "vod-session", VLC_VAR_STRING );
            var_SetString( p_instance->p_parent, "vod-session", psz_id );
        }

        if( p_cfg->psz_output != NULL || psz_vod_output != NULL )
        {
            char *psz_buffer;
            if( asprintf( &psz_buffer, "sout=%s%s%s",
                      p_cfg->psz_output ? p_cfg->psz_output : "",
                      (p_cfg->psz_output && psz_vod_output) ? ":" : psz_vod_output ? "#" : "",
                      psz_vod_output ? psz_vod_output : "" ) != -1 )
            {
                input_item_AddOption( p_instance->p_item, psz_buffer, VLC_INPUT_OPTION_TRUSTED );
                free( psz_buffer );
            }
        }

        for( i = 0; i < p_cfg->i_option; i++ )
        {
            if( !strcmp( p_cfg->ppsz_option[i], "sout-keep" ) )
                p_instance->b_sout_keep = true;
            else if( !strcmp( p_cfg->ppsz_option[i], "nosout-keep" ) || !strcmp( p_cfg->ppsz_option[i], "no-sout-keep" ) )
                p_instance->b_sout_keep = false;
            else
                input_item_AddOption( p_instance->p_item, p_cfg->ppsz_option[i], VLC_INPUT_OPTION_TRUSTED );
        }
        TAB_APPEND( p_media->i_instance, p_media->instance, p_instance );
    }

    /* Stop old instance */
    input_thread_t *p_input = p_instance->p_input;
    if( p_input )
    {
        if( p_instance->i_index == i_input_index &&
            !p_input->b_eof && !p_input->b_error )
        {
            if( var_GetInteger( p_input, "state" ) == PAUSE_S )
                var_SetInteger( p_input, "state",  PLAYING_S );
            return VLC_SUCCESS;
        }


        input_Stop( p_input, true );
        input_Join( p_input );
        var_DelCallback( p_instance->p_input, "intf-event", InputEvent, p_media );
        input_Release( p_input );

        if( !p_instance->b_sout_keep )
            input_resource_TerminateSout( p_instance->p_input_resource );
        input_resource_TerminateVout( p_instance->p_input_resource );

        vlm_SendEventMediaInstanceStopped( p_vlm, id, p_media->cfg.psz_name );
    }

    /* Start new one */
    p_instance->i_index = i_input_index;
    if( strstr( p_media->cfg.ppsz_input[p_instance->i_index], "://" ) == NULL )
    {
        char *psz_uri = vlc_path2uri(
                          p_media->cfg.ppsz_input[p_instance->i_index], NULL );
        input_item_SetURI( p_instance->p_item, psz_uri ) ;
        free( psz_uri );
    }
    else
        input_item_SetURI( p_instance->p_item, p_media->cfg.ppsz_input[p_instance->i_index] ) ;

    if( asprintf( &psz_log, _("Media: %s"), p_media->cfg.psz_name ) != -1 )
    {
        p_instance->p_input = input_Create( p_instance->p_parent,
                                            p_instance->p_item, psz_log,
                                            p_instance->p_input_resource );
        if( p_instance->p_input )
        {
            var_AddCallback( p_instance->p_input, "intf-event", InputEvent, p_media );

            if( input_Start( p_instance->p_input ) != VLC_SUCCESS )
            {
                var_DelCallback( p_instance->p_input, "intf-event", InputEvent, p_media );
                vlc_object_release( p_instance->p_input );
                p_instance->p_input = NULL;
            }
        }

        if( !p_instance->p_input )
        {
            vlm_MediaInstanceDelete( p_vlm, id, p_instance, p_media );
        }
        else
        {
            vlm_SendEventMediaInstanceStarted( p_vlm, id, p_media->cfg.psz_name );
        }
        free( psz_log );
    }

    return VLC_SUCCESS;
}

static int vlm_ControlMediaInstanceStop( vlm_t *p_vlm, int64_t id, const char *psz_id )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;

    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance )
        return VLC_EGENERIC;

    vlm_MediaInstanceDelete( p_vlm, id, p_instance, p_media );

    return VLC_SUCCESS;
}
static int vlm_ControlMediaInstancePause( vlm_t *p_vlm, int64_t id, const char *psz_id )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;
    int i_state;

    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance || !p_instance->p_input )
        return VLC_EGENERIC;

    /* Toggle pause state */
    i_state = var_GetInteger( p_instance->p_input, "state" );
    if( i_state == PAUSE_S && !p_media->cfg.b_vod )
        var_SetInteger( p_instance->p_input, "state", PLAYING_S );
    else if( i_state == PLAYING_S )
        var_SetInteger( p_instance->p_input, "state", PAUSE_S );
    return VLC_SUCCESS;
}
static int vlm_ControlMediaInstanceGetTimePosition( vlm_t *p_vlm, int64_t id, const char *psz_id, int64_t *pi_time, double *pd_position )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;

    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance || !p_instance->p_input )
        return VLC_EGENERIC;

    if( pi_time )
        *pi_time = var_GetTime( p_instance->p_input, "time" );
    if( pd_position )
        *pd_position = var_GetFloat( p_instance->p_input, "position" );
    return VLC_SUCCESS;
}
static int vlm_ControlMediaInstanceSetTimePosition( vlm_t *p_vlm, int64_t id, const char *psz_id, int64_t i_time, double d_position )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;

    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance || !p_instance->p_input )
        return VLC_EGENERIC;

    if( i_time >= 0 )
        return var_SetTime( p_instance->p_input, "time", i_time );
    else if( d_position >= 0 && d_position <= 100 )
        return var_SetFloat( p_instance->p_input, "position", d_position );
    return VLC_EGENERIC;
}

static int vlm_ControlMediaInstanceGets( vlm_t *p_vlm, int64_t id, vlm_media_instance_t ***ppp_idsc, int *pi_instance )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_t **pp_idsc;
    int                              i_idsc;
    int i;

    if( !p_media )
        return VLC_EGENERIC;

    TAB_INIT( i_idsc, pp_idsc );
    for( i = 0; i < p_media->i_instance; i++ )
    {
        vlm_media_instance_sys_t *p_instance = p_media->instance[i];
        vlm_media_instance_t *p_idsc = vlm_media_instance_New();

        if( p_instance->psz_name )
            p_idsc->psz_name = strdup( p_instance->psz_name );
        if( p_instance->p_input )
        {
            p_idsc->i_time = var_GetTime( p_instance->p_input, "time" );
            p_idsc->i_length = var_GetTime( p_instance->p_input, "length" );
            p_idsc->d_position = var_GetFloat( p_instance->p_input, "position" );
            if( var_GetInteger( p_instance->p_input, "state" ) == PAUSE_S )
                p_idsc->b_paused = true;
            p_idsc->i_rate = INPUT_RATE_DEFAULT
                             / var_GetFloat( p_instance->p_input, "rate" );
        }

        TAB_APPEND( i_idsc, pp_idsc, p_idsc );
    }
    *ppp_idsc = pp_idsc;
    *pi_instance = i_idsc;
    return VLC_SUCCESS;
}

static int vlm_ControlMediaInstanceClear( vlm_t *p_vlm, int64_t id )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );

    if( !p_media )
        return VLC_EGENERIC;

    while( p_media->i_instance > 0 )
        vlm_ControlMediaInstanceStop( p_vlm, id, p_media->instance[0]->psz_name );

    return VLC_SUCCESS;
}

static int vlm_ControlScheduleClear( vlm_t *p_vlm )
{
    while( p_vlm->i_schedule > 0 )
        vlm_ScheduleDelete( p_vlm, p_vlm->schedule[0] );

    return VLC_SUCCESS;
}

static int vlm_vaControlInternal( vlm_t *p_vlm, int i_query, va_list args )
{
    vlm_media_t *p_dsc;
    vlm_media_t **pp_dsc;
    vlm_media_t ***ppp_dsc;
    vlm_media_instance_t ***ppp_idsc;
    const char *psz_id;
    const char *psz_vod;
    int64_t *p_id;
    int64_t id;
    int i_int;
    int *pi_int;

    int64_t *pi_i64;
    int64_t i_i64;
    double *pd_double;
    double d_double;

    switch( i_query )
    {
    /* Media control */
    case VLM_GET_MEDIAS:
        ppp_dsc = (vlm_media_t ***)va_arg( args, vlm_media_t *** );
        pi_int = (int *)va_arg( args, int * );
        return vlm_ControlMediaGets( p_vlm, ppp_dsc, pi_int );

    case VLM_CLEAR_MEDIAS:
        return vlm_ControlMediaClear( p_vlm );

    case VLM_CHANGE_MEDIA:
        p_dsc = (vlm_media_t*)va_arg( args, vlm_media_t * );
        return vlm_ControlMediaChange( p_vlm, p_dsc );

    case VLM_ADD_MEDIA:
        p_dsc = (vlm_media_t*)va_arg( args, vlm_media_t * );
        p_id = (int64_t*)va_arg( args, int64_t * );
        return vlm_ControlMediaAdd( p_vlm, p_dsc, p_id );

    case VLM_DEL_MEDIA:
        id = (int64_t)va_arg( args, int64_t );
        return vlm_ControlMediaDel( p_vlm, id );

    case VLM_GET_MEDIA:
        id = (int64_t)va_arg( args, int64_t );
        pp_dsc = (vlm_media_t **)va_arg( args, vlm_media_t ** );
        return vlm_ControlMediaGet( p_vlm, id, pp_dsc );

    case VLM_GET_MEDIA_ID:
        psz_id = (const char*)va_arg( args, const char * );
        p_id = (int64_t*)va_arg( args, int64_t * );
        return vlm_ControlMediaGetId( p_vlm, psz_id, p_id );


    /* Media instance control */
    case VLM_GET_MEDIA_INSTANCES:
        id = (int64_t)va_arg( args, int64_t );
        ppp_idsc = (vlm_media_instance_t ***)va_arg( args, vlm_media_instance_t *** );
        pi_int = (int *)va_arg( args, int *);
        return vlm_ControlMediaInstanceGets( p_vlm, id, ppp_idsc, pi_int );

    case VLM_CLEAR_MEDIA_INSTANCES:
        id = (int64_t)va_arg( args, int64_t );
        return vlm_ControlMediaInstanceClear( p_vlm, id );


    case VLM_START_MEDIA_BROADCAST_INSTANCE:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        i_int = (int)va_arg( args, int );
        return vlm_ControlMediaInstanceStart( p_vlm, id, psz_id, i_int, NULL );

    case VLM_START_MEDIA_VOD_INSTANCE:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        i_int = (int)va_arg( args, int );
        psz_vod = (const char*)va_arg( args, const char* );
        if( !psz_vod )
            return VLC_EGENERIC;
        return vlm_ControlMediaInstanceStart( p_vlm, id, psz_id, i_int, psz_vod );

    case VLM_STOP_MEDIA_INSTANCE:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        return vlm_ControlMediaInstanceStop( p_vlm, id, psz_id );

    case VLM_PAUSE_MEDIA_INSTANCE:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        return vlm_ControlMediaInstancePause( p_vlm, id, psz_id );

    case VLM_GET_MEDIA_INSTANCE_TIME:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        pi_i64 = (int64_t*)va_arg( args, int64_t * );
        return vlm_ControlMediaInstanceGetTimePosition( p_vlm, id, psz_id, pi_i64, NULL );
    case VLM_GET_MEDIA_INSTANCE_POSITION:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        pd_double = (double*)va_arg( args, double* );
        return vlm_ControlMediaInstanceGetTimePosition( p_vlm, id, psz_id, NULL, pd_double );

    case VLM_SET_MEDIA_INSTANCE_TIME:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        i_i64 = (int64_t)va_arg( args, int64_t);
        return vlm_ControlMediaInstanceSetTimePosition( p_vlm, id, psz_id, i_i64, -1 );
    case VLM_SET_MEDIA_INSTANCE_POSITION:
        id = (int64_t)va_arg( args, int64_t );
        psz_id = (const char*)va_arg( args, const char* );
        d_double = (double)va_arg( args, double );
        return vlm_ControlMediaInstanceSetTimePosition( p_vlm, id, psz_id, -1, d_double );

    case VLM_CLEAR_SCHEDULES:
        return vlm_ControlScheduleClear( p_vlm );

    default:
        msg_Err( p_vlm, "unknown VLM query" );
        return VLC_EGENERIC;
    }
}

int vlm_ControlInternal( vlm_t *p_vlm, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = vlm_vaControlInternal( p_vlm, i_query, args );
    va_end( args );

    return i_result;
}

int vlm_Control( vlm_t *p_vlm, int i_query, ... )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );

    vlc_mutex_lock( &p_vlm->lock );
    i_result = vlm_vaControlInternal( p_vlm, i_query, args );
    vlc_mutex_unlock( &p_vlm->lock );

    va_end( args );

    return i_result;
}

