/*****************************************************************************
 * vlm.c: VLM interface plugin
 *****************************************************************************
 * Copyright (C) 2000-2005 VLC authors and VideoLAN
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

#include <vlc_vlm.h>
#include <vlc_modules.h>

#include <vlc_player.h>
#include <vlc_stream.h>
#include "vlm_internal.h"
#include "vlm_event.h"
#include <vlc_sout.h>
#include <vlc_url.h>
#include "../stream_output/stream_output.h"
#include "../libvlc.h"
#include "input_internal.h"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/

static void* Manage( void * );

static void player_on_state_changed(vlc_player_t *player,
                                    enum vlc_player_state new_state, void *data)
{
    vlm_media_sys_t *p_media = data;
    vlm_t *p_vlm = libvlc_priv( vlc_object_instance(p_media) )->p_vlm;
    assert( p_vlm );
    const char *psz_instance_name = NULL;

    for( int i = 0; i < p_media->i_instance; i++ )
    {
        if( p_media->instance[i]->player == player )
        {
            psz_instance_name = p_media->instance[i]->psz_name;
            break;
        }
    }
    assert(psz_instance_name);
    enum vlm_state_e vlm_state;
    switch (new_state)
    {
        case VLC_PLAYER_STATE_STOPPED:
            vlm_state = vlc_player_GetError(player) ? VLM_ERROR_S : VLM_INIT_S;
            break;
        case VLC_PLAYER_STATE_STARTED:
            vlm_state = VLM_OPENING_S;
            break;
        case VLC_PLAYER_STATE_PLAYING:
            vlm_state = VLM_PLAYING_S;
            break;
        case VLC_PLAYER_STATE_PAUSED:
            vlm_state = VLM_PAUSE_S;
            break;
        case VLC_PLAYER_STATE_STOPPING:
            vlm_state = vlc_player_GetError(player) ? VLM_ERROR_S : VLM_END_S;
            break;
        default:
            vlc_assert_unreachable();
    }
    vlm_SendEventMediaInstanceState( p_vlm, p_media->cfg.id, p_media->cfg.psz_name, psz_instance_name, vlm_state );

    vlc_mutex_lock( &p_vlm->lock_manage );
    p_vlm->input_state_changed = true;
    vlc_cond_signal( &p_vlm->wait_manage );
    vlc_mutex_unlock( &p_vlm->lock_manage );
}

static vlc_mutex_t vlm_mutex = VLC_STATIC_MUTEX;

/*****************************************************************************
 * vlm_New:
 *****************************************************************************/
vlm_t *vlm_New( libvlc_int_t *libvlc, const char *psz_vlmconf )
{
    vlm_t *p_vlm = NULL, **pp_vlm = &(libvlc_priv(libvlc)->p_vlm);
    vlc_object_t *p_this = VLC_OBJECT(libvlc);

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

    p_vlm = vlc_custom_create( p_this, sizeof( *p_vlm ), "vlm daemon" );
    if( !p_vlm )
    {
        vlc_mutex_unlock( &vlm_mutex );
        return NULL;
    }

    vlc_mutex_init( &p_vlm->lock );
    vlc_mutex_init( &p_vlm->lock_manage );
    vlc_cond_init( &p_vlm->wait_manage );
    p_vlm->users = 1;
    p_vlm->input_state_changed = false;
    p_vlm->exiting = false;
    p_vlm->i_id = 1;
    TAB_INIT( p_vlm->i_media, p_vlm->media );
    TAB_INIT( p_vlm->i_schedule, p_vlm->schedule );
    var_Create( p_vlm, "intf-event", VLC_VAR_ADDRESS );

    if( vlc_clone( &p_vlm->thread, Manage, p_vlm, VLC_THREAD_PRIORITY_LOW ) )
    {
        vlc_object_delete(p_vlm);
        vlc_mutex_unlock( &vlm_mutex );
        return NULL;
    }

    *pp_vlm = p_vlm; /* for future reference */

    vlc_mutex_unlock( &vlm_mutex );

    /* Load our configuration file */
    if( psz_vlmconf != NULL )
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
        assert( libvlc_priv(vlc_object_instance(p_vlm))->p_vlm == p_vlm );
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

    vlc_mutex_lock( &p_vlm->lock_manage );
    p_vlm->exiting = true;
    vlc_cond_signal( &p_vlm->wait_manage );
    vlc_mutex_unlock( &p_vlm->lock_manage );

    libvlc_priv(vlc_object_instance(p_vlm))->p_vlm = NULL;
    vlc_mutex_unlock( &vlm_mutex );

    vlc_join( p_vlm->thread, NULL );
    vlc_object_delete(p_vlm);
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

/*****************************************************************************
 * Manage:
 *****************************************************************************/
static void* Manage( void* p_object )
{
    vlm_t *vlm = (vlm_t*)p_object;
    time_t lastcheck;
    bool exiting;

    time(&lastcheck);

    do
    {
        char **ppsz_scheduled_commands = NULL;
        int    i_scheduled_commands = 0;

        /* destroy the inputs that wants to die, and launch the next input */
        vlc_mutex_lock( &vlm->lock );
        for( int i = 0; i < vlm->i_media; i++ )
        {
            vlm_media_sys_t *p_media = vlm->media[i];

            for( int j = 0; j < p_media->i_instance; )
            {
                vlm_media_instance_sys_t *p_instance = p_media->instance[j];

                vlc_player_Lock(p_instance->player);
                if (!vlc_player_IsStarted(p_instance->player))
                {
                    vlc_player_Unlock(p_instance->player);
                    int i_new_input_index;

                    /* */
                    i_new_input_index = p_instance->i_index + 1;
                    if( p_media->cfg.broadcast.b_loop && i_new_input_index >= p_media->cfg.i_input )
                        i_new_input_index = 0;

                    if( i_new_input_index >= p_media->cfg.i_input )
                        vlm_ControlInternal( vlm, VLM_STOP_MEDIA_INSTANCE, p_media->cfg.id, p_instance->psz_name );
                    else
                        vlm_ControlInternal( vlm, VLM_START_MEDIA_BROADCAST_INSTANCE, p_media->cfg.id, p_instance->psz_name, i_new_input_index );

                    j = 0;
                }
                else
                {
                    vlc_player_Unlock(p_instance->player);
                    j++;
                }
            }
        }

        /* scheduling */
        time_t now, nextschedule = 0;

        time(&now);

        for( int i = 0; i < vlm->i_schedule; i++ )
        {
            time_t real_date = vlm->schedule[i]->date;

            if( vlm->schedule[i]->b_enabled )
            {
                bool b_now = false;
                if( vlm->schedule[i]->date == 0 ) // now !
                {
                    vlm->schedule[i]->date = now;
                    real_date = now;
                    b_now = true;
                }
                else if( vlm->schedule[i]->period != 0 )
                {
                    int j = 0;
                    while( ((vlm->schedule[i]->date + j *
                             vlm->schedule[i]->period) <= lastcheck) &&
                           ( vlm->schedule[i]->i_repeat > j ||
                             vlm->schedule[i]->i_repeat < 0 ) )
                    {
                        j++;
                    }

                    real_date = vlm->schedule[i]->date + j *
                        vlm->schedule[i]->period;
                }

                if( real_date <= now )
                {
                    if( real_date > lastcheck || b_now )
                    {
                        for( int j = 0; j < vlm->schedule[i]->i_command; j++ )
                        {
                            TAB_APPEND( i_scheduled_commands,
                                        ppsz_scheduled_commands,
                                        strdup(vlm->schedule[i]->command[j] ) );
                        }
                    }
                }
                else if( nextschedule == 0 || real_date < nextschedule )
                {
                    nextschedule = real_date;
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

        lastcheck = now;
        vlc_mutex_unlock( &vlm->lock );

        vlc_mutex_lock( &vlm->lock_manage );

        while( !vlm->input_state_changed && !(exiting = vlm->exiting) )
        {
            if( nextschedule )
            {
                if( vlc_cond_timedwait_daytime( &vlm->wait_manage,
                                                &vlm->lock_manage,
                                                nextschedule ) )
                    break;
            }
            else
                vlc_cond_wait( &vlm->wait_manage, &vlm->lock_manage );
        }
        vlm->input_state_changed = false;
        vlc_mutex_unlock( &vlm->lock_manage );
    }
    while( !exiting );

    return NULL;
}

/* New API
 */
/*
typedef struct
{
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
    for( int i = 0; i < p_vlm->i_media; i++ )
    {
        if( p_vlm->media[i]->cfg.id == id )
            return p_vlm->media[i];
    }
    return NULL;
}
static vlm_media_sys_t *vlm_ControlMediaGetByName( vlm_t *p_vlm, const char *psz_name )
{
    for( int i = 0; i < p_vlm->i_media; i++ )
    {
        if( !strcmp( p_vlm->media[i]->cfg.psz_name, psz_name ) )
            return p_vlm->media[i];
    }
    return NULL;
}
static int vlm_MediaDescriptionCheck( vlm_t *p_vlm, vlm_media_t *p_cfg )
{
    if( !p_cfg || !p_cfg->psz_name ||
        !strcmp( p_cfg->psz_name, "all" ) || !strcmp( p_cfg->psz_name, "media" ) || !strcmp( p_cfg->psz_name, "schedule" ) )
        return VLC_EGENERIC;

    for( int i = 0; i < p_vlm->i_media; i++ )
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

    /* TODO start media if needed */

    /* TODO add support of var vlm_media_broadcast */

    vlm_SendEventMediaChanged( p_vlm, p_cfg->id, p_cfg->psz_name );
    return VLC_SUCCESS;
}
static int vlm_ControlMediaChange( vlm_t *p_vlm, vlm_media_t *p_cfg )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, p_cfg->id );

    /* */
    if( !p_media || vlm_MediaDescriptionCheck( p_vlm, p_cfg ) )
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
    char *header;

    if( vlm_MediaDescriptionCheck( p_vlm, p_cfg ) || vlm_ControlMediaGetByName( p_vlm, p_cfg->psz_name ) )
    {
        msg_Err( p_vlm, "invalid media description" );
        return VLC_EGENERIC;
    }

    p_media = vlc_custom_create( VLC_OBJECT(p_vlm), sizeof( *p_media ),
                                 "media" );
    if( !p_media )
        return VLC_ENOMEM;

    if( asprintf( &header, _("Media: %s"), p_cfg->psz_name ) == -1 )
    {
        vlc_object_delete(p_media);
        return VLC_ENOMEM;
    }

    p_media->obj.logger = vlc_LogHeaderCreate( p_media->obj.logger, header );
    free( header );

    if( p_media->obj.logger == NULL )
    {
        vlc_object_delete(p_media);
        return VLC_ENOMEM;
    }

    vlm_media_Copy( &p_media->cfg, p_cfg );
    p_media->cfg.id = p_vlm->i_id++;
    /* FIXME do we do something here if enabled is true ? */

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

    /* */
    vlm_SendEventMediaRemoved( p_vlm, id, p_media->cfg.psz_name );

    vlm_media_Clean( &p_media->cfg );

    TAB_REMOVE( p_vlm->i_media, p_vlm->media, p_media );
    vlc_LogDestroy( p_media->obj.logger );
    vlc_object_delete(p_media);

    return VLC_SUCCESS;
}

static int vlm_ControlMediaGets( vlm_t *p_vlm, vlm_media_t ***ppp_dsc, int *pi_dsc )
{
    vlm_media_t **pp_dsc;
    int                     i_dsc;

    TAB_INIT( i_dsc, pp_dsc );
    for( int i = 0; i < p_vlm->i_media; i++ )
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
    for( int i = 0; i < p_media->i_instance; i++ )
    {
        const char *psz = p_media->instance[i]->psz_name;
        if( ( psz == NULL && psz_id == NULL ) ||
            ( psz && psz_id && !strcmp( psz, psz_id ) ) )
            return p_media->instance[i];
    }
    return NULL;
}

static vlm_media_instance_sys_t *vlm_MediaInstanceNew( vlm_media_sys_t *p_media, const char *psz_name )
{
    vlm_media_instance_sys_t *p_instance = calloc( 1, sizeof(vlm_media_instance_sys_t) );
    if( !p_instance )
        return NULL;

    p_instance->psz_name = NULL;
    if( psz_name )
        p_instance->psz_name = strdup( psz_name );

    p_instance->p_item = input_item_New( NULL, NULL );
    if (!p_instance->p_item)
        goto error;

    p_instance->i_index = 0;
    p_instance->p_parent = vlc_object_create( p_media, sizeof (vlc_object_t) );
    if (!p_instance->p_parent)
        goto error;

    p_instance->player = vlc_player_New(p_instance->p_parent,
                                        VLC_PLAYER_LOCK_NORMAL, NULL, NULL);
    if (!p_instance->player)
        goto error;

    static struct vlc_player_cbs cbs = {
        .on_state_changed = player_on_state_changed,
    };
    vlc_player_Lock(p_instance->player);
    p_instance->listener =
        vlc_player_AddListener(p_instance->player, &cbs, p_media);
    vlc_player_Unlock(p_instance->player);

    if (!p_instance->listener)
        goto error;
    return p_instance;

error:
    if (p_instance->player)
        vlc_player_Delete(p_instance->player);
    if (p_instance->p_parent)
        vlc_object_delete(p_instance->p_parent);
    if (p_instance->p_item)
        input_item_Release(p_instance->p_item);
    free(p_instance->psz_name);
    free(p_instance);
    return NULL;
}
static void vlm_MediaInstanceDelete( vlm_t *p_vlm, int64_t id, vlm_media_instance_sys_t *p_instance, vlm_media_sys_t *p_media )
{
    vlc_player_t *player = p_instance->player;

    vlc_player_Lock(player);
    vlc_player_RemoveListener(player, p_instance->listener);
    vlc_player_Stop(player);
    bool had_media = vlc_player_GetCurrentMedia(player);
    vlc_player_Unlock(player);
    vlc_player_Delete(player);

    if (had_media)
        vlm_SendEventMediaInstanceStopped( p_vlm, id, p_media->cfg.psz_name );
    vlc_object_delete(p_instance->p_parent);

    TAB_REMOVE( p_media->i_instance, p_media->instance, p_instance );
    input_item_Release( p_instance->p_item );
    free( p_instance->psz_name );
    free( p_instance );
}


static int vlm_ControlMediaInstanceStart( vlm_t *p_vlm, int64_t id, const char *psz_id, int i_input_index )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;

    if( !p_media || !p_media->cfg.b_enabled || p_media->cfg.i_input <= 0 )
        return VLC_EGENERIC;

    if( i_input_index < 0 || i_input_index >= p_media->cfg.i_input )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance )
    {
        vlm_media_t *p_cfg = &p_media->cfg;

        p_instance = vlm_MediaInstanceNew( p_media, psz_id );
        if( !p_instance )
            return VLC_ENOMEM;

        if( p_cfg->psz_output != NULL )
        {
            char *psz_buffer;
            if( asprintf( &psz_buffer, "sout=%s", p_cfg->psz_output ) != -1 )
            {
                input_item_AddOption( p_instance->p_item, psz_buffer, VLC_INPUT_OPTION_TRUSTED );
                free( psz_buffer );
            }
        }

        for( int i = 0; i < p_cfg->i_option; i++ )
            input_item_AddOption( p_instance->p_item, p_cfg->ppsz_option[i], VLC_INPUT_OPTION_TRUSTED );
        TAB_APPEND( p_media->i_instance, p_media->instance, p_instance );
    }

    /* Stop old instance */
    vlc_player_t *player = p_instance->player;
    vlc_player_Lock(player);
    if (vlc_player_GetCurrentMedia(player))
    {
        if( p_instance->i_index == i_input_index )
        {
            if (vlc_player_IsPaused(player))
                vlc_player_Resume(player);
            return VLC_SUCCESS;
        }

        vlc_player_Stop(player);
        vlc_player_Unlock(player);
        vlm_SendEventMediaInstanceStopped( p_vlm, id, p_media->cfg.psz_name );
        vlc_player_Lock(player);
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

    vlc_player_SetCurrentMedia(player, p_instance->p_item);
    vlc_player_Start(player);
    vlc_player_Unlock(player);

    vlm_SendEventMediaInstanceStarted( p_vlm, id, p_media->cfg.psz_name );

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
    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance )
        return VLC_EGENERIC;

    vlc_player_Lock(p_instance->player);
    vlc_player_TogglePause(p_instance->player);
    vlc_player_Unlock(p_instance->player);

    return VLC_SUCCESS;
}
static int vlm_ControlMediaInstanceGetTimePosition( vlm_t *p_vlm, int64_t id, const char *psz_id, int64_t *pi_time, double *pd_position )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;

    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance )
        return VLC_EGENERIC;

    vlc_player_Lock(p_instance->player);
    if( pi_time )
        *pi_time = US_FROM_VLC_TICK(vlc_player_GetTime(p_instance->player)); 
    if( pd_position )
        *pd_position = vlc_player_GetPosition(p_instance->player);
    vlc_player_Unlock(p_instance->player);
    return VLC_SUCCESS;
}
static int vlm_ControlMediaInstanceSetTimePosition( vlm_t *p_vlm, int64_t id, const char *psz_id, int64_t i_time, double d_position )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_sys_t *p_instance;

    if( !p_media )
        return VLC_EGENERIC;

    p_instance = vlm_ControlMediaInstanceGetByName( p_media, psz_id );
    if( !p_instance )
        return VLC_EGENERIC;

    vlc_player_Lock(p_instance->player);
    if( i_time >= 0 )
        vlc_player_SetTime(p_instance->player, VLC_TICK_FROM_US(i_time));
    else if( d_position >= 0 && d_position <= 100 )
        vlc_player_SetPosition(p_instance->player, d_position);
    vlc_player_Unlock(p_instance->player);
    return VLC_SUCCESS;
}

static int vlm_ControlMediaInstanceGets( vlm_t *p_vlm, int64_t id, vlm_media_instance_t ***ppp_idsc, int *pi_instance )
{
    vlm_media_sys_t *p_media = vlm_ControlMediaGetById( p_vlm, id );
    vlm_media_instance_t **pp_idsc;
    int                              i_idsc;

    if( !p_media )
        return VLC_EGENERIC;

    TAB_INIT( i_idsc, pp_idsc );
    for( int i = 0; i < p_media->i_instance; i++ )
    {
        vlm_media_instance_sys_t *p_instance = p_media->instance[i];
        vlm_media_instance_t *p_idsc = vlm_media_instance_New();
        if( p_instance->psz_name )
            p_idsc->psz_name = strdup( p_instance->psz_name );
        vlc_player_Lock(p_instance->player);
        p_idsc->i_time = US_FROM_VLC_TICK(vlc_player_GetTime(p_instance->player));
        p_idsc->i_length = US_FROM_VLC_TICK(vlc_player_GetLength(p_instance->player));
        p_idsc->d_position = vlc_player_GetPosition(p_instance->player);
        p_idsc->b_paused = vlc_player_IsPaused(p_instance->player);
        p_idsc->f_rate = vlc_player_GetRate(p_instance->player);
        vlc_player_Unlock(p_instance->player);

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
        return vlm_ControlMediaInstanceStart( p_vlm, id, psz_id, i_int );

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

