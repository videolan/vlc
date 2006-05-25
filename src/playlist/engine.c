/*****************************************************************************
 * engine.c : Run the playlist and handle its control
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
 * $Id: /local/vlc/0.8.6-playlist-vlm/src/playlist/playlist.c 13741 2006-03-21T19:29:39.792444Z zorglub  $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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
#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/sout.h>
#include <vlc/input.h>
#include "vlc_playlist.h"
#include "vlc_interaction.h"

#undef PLAYLIST_DEBUG

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void VariablesInit( playlist_t *p_playlist );

/**
 * Create playlist
 *
 * Create a playlist structure.
 * \param p_parent the vlc object that is to be the parent of this playlist
 * \return a pointer to the created playlist, or NULL on error
 */
playlist_t * playlist_Create( vlc_object_t *p_parent )
{
    playlist_t *p_playlist;

    /* Allocate structure */
    p_playlist = vlc_object_create( p_parent, VLC_OBJECT_PLAYLIST );
    if( !p_playlist )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    VariablesInit( p_playlist );

    /* Initialise data structures */
    vlc_mutex_init( p_playlist, &p_playlist->gc_lock );
    p_playlist->i_last_playlist_id = 0;
    p_playlist->i_last_input_id = 0;
    p_playlist->p_input = NULL;

    p_playlist->i_size = 0;
    p_playlist->pp_items = NULL;
    p_playlist->i_all_size = 0;
    p_playlist->pp_all_items = NULL;

    p_playlist->i_input_items = 0;
    p_playlist->pp_input_items = NULL;

    p_playlist->p_root_category = playlist_NodeCreate( p_playlist, NULL, NULL);
    p_playlist->p_root_onelevel = playlist_NodeCreate( p_playlist, NULL, NULL);

    /* Create playlist and media library */
    p_playlist->p_local_category = playlist_NodeCreate( p_playlist,
                                 _( "Playlist" ),p_playlist->p_root_category );
    p_playlist->p_ml_category =   playlist_NodeCreate( p_playlist,
                           _( "Media Library" ), p_playlist->p_root_category );
    p_playlist->p_local_onelevel =  playlist_NodeCreate( p_playlist,
                                _( "Playlist" ), p_playlist->p_root_onelevel );
    p_playlist->p_ml_onelevel =  playlist_NodeCreate( p_playlist,
                           _( "Media Library" ), p_playlist->p_root_onelevel );

    /* This is a hack to find it later. Quite ugly, but I haven't found a
     * better way */
    p_playlist->p_local_onelevel->p_input->i_id =
        p_playlist->p_local_category->p_input->i_id;
    p_playlist->p_ml_onelevel->p_input->i_id =
        p_playlist->p_ml_category->p_input->i_id;

    /* Initial status */
    p_playlist->status.p_item = NULL;
    p_playlist->status.p_node = p_playlist->p_root_onelevel;
    p_playlist->request.b_request = VLC_FALSE;
    p_playlist->status.i_status = PLAYLIST_STOPPED;

    p_playlist->i_sort = SORT_ID;
    p_playlist->i_order = ORDER_NORMAL;

    vlc_object_attach( p_playlist, p_parent );
    return p_playlist;
}

void playlist_Destroy( playlist_t *p_playlist )
{
    while( p_playlist->i_sds )
    {
        playlist_ServicesDiscoveryRemove( p_playlist,
                                          p_playlist->pp_sds[0]->psz_module );
    }
    vlc_thread_join( p_playlist->p_preparse );
    vlc_thread_join( p_playlist );

    vlc_object_detach( p_playlist->p_preparse );

    var_Destroy( p_playlist, "intf-change" );
    var_Destroy( p_playlist, "item-change" );
    var_Destroy( p_playlist, "playlist-current" );
    var_Destroy( p_playlist, "intf-popmenu" );
    var_Destroy( p_playlist, "intf-show" );
    var_Destroy( p_playlist, "play-and-stop" );
    var_Destroy( p_playlist, "random" );
    var_Destroy( p_playlist, "repeat" );
    var_Destroy( p_playlist, "loop" );
    var_Destroy( p_playlist, "activity" );

    playlist_LockClear( p_playlist );

    if( p_playlist->p_stats )
        free( p_playlist->p_stats );

    vlc_mutex_destroy( &p_playlist->gc_lock );
    vlc_object_destroy( p_playlist->p_preparse );
    vlc_object_destroy( p_playlist );

}
/* Destroy remaining objects */
static mtime_t ObjectGarbageCollector( playlist_t *p_playlist, int i_type,
                                       mtime_t destroy_date )
{
    vlc_object_t *p_obj;

    if( destroy_date > mdate() ) return destroy_date;

    if( destroy_date == 0 )
    {
        /* give a little time */
        return mdate() + I64C(1000000);
    }
    else
    {
        vlc_mutex_lock( &p_playlist->gc_lock );
        while( ( p_obj = vlc_object_find( p_playlist, i_type, FIND_CHILD ) ) )
        {
            if( p_obj->p_parent != (vlc_object_t*)p_playlist )
            {
                /* only first child (ie unused) */
                vlc_object_release( p_obj );
                break;
            }
            if( i_type == VLC_OBJECT_VOUT )
            {
                msg_Dbg( p_playlist, "garbage collector destroying 1 vout" );
                vlc_object_detach( p_obj );
                vlc_object_release( p_obj );
                vout_Destroy( (vout_thread_t *)p_obj );
            }
            else if( i_type == VLC_OBJECT_SOUT )
            {
                vlc_object_release( p_obj );
                sout_DeleteInstance( (sout_instance_t*)p_obj );
            }
        }
        vlc_mutex_unlock( &p_playlist->gc_lock );
        return 0;
    }
}

/** Main loop for the playlist */
void playlist_MainLoop( playlist_t *p_playlist )
{
    playlist_item_t *p_item = NULL;

    mtime_t    i_vout_destroyed_date = 0;
    mtime_t    i_sout_destroyed_date = 0;

    PL_LOCK

    /* First, check if we have something to do */
    /* FIXME : this can be called several times */
    if( p_playlist->request.b_request )
    {
#ifdef PLAYLIST_DEBUG
        msg_Dbg(p_playlist, "incoming request - stopping current input" );
#endif
        /* Stop the existing input */
        if( p_playlist->p_input )
        {
            input_StopThread( p_playlist->p_input );
        }
    }

    /* If there is an input, check that it doesn't need to die. */
    if( p_playlist->p_input )
    {
        /* This input is dead. Remove it ! */
        if( p_playlist->p_input->b_dead )
        {
            int i_activity;
            input_thread_t *p_input;

            p_input = p_playlist->p_input;
            p_playlist->p_input = NULL;

            /* Release the playlist lock, because we may get stuck
             * in input_DestroyThread() for some time. */
            PL_UNLOCK

            /* Destroy input */
            input_DestroyThread( p_input );

            /* Unlink current input
             * (_after_ input_DestroyThread for vout garbage collector) */
            vlc_object_detach( p_input );

            /* Destroy object */
            vlc_object_destroy( p_input );

            i_vout_destroyed_date = 0;
            i_sout_destroyed_date = 0;

            if( p_playlist->status.p_item->i_flags
                & PLAYLIST_REMOVE_FLAG )
            {
                 playlist_ItemDelete( p_playlist->status.p_item );
                 p_playlist->status.p_item = NULL;
            }

            i_activity= var_GetInteger( p_playlist, "activity") ;
            var_SetInteger( p_playlist, "activity", i_activity -
                            DEFAULT_INPUT_ACTIVITY );

            return;
        }
        /* This input is dying, let it do */
        else if( p_playlist->p_input->b_die )
        {
            ;
        }
        /* This input has finished, ask it to die ! */
        else if( p_playlist->p_input->b_error
                  || p_playlist->p_input->b_eof )
        {
            input_StopThread( p_playlist->p_input );
            /* Select the next playlist item */
            PL_UNLOCK
            return;
        }
        else if( p_playlist->p_input->i_state != INIT_S )
        {
            PL_UNLOCK
            i_vout_destroyed_date =
                ObjectGarbageCollector( p_playlist, VLC_OBJECT_VOUT,
                                        i_vout_destroyed_date );
            i_sout_destroyed_date =
                ObjectGarbageCollector( p_playlist, VLC_OBJECT_SOUT,
                                        i_sout_destroyed_date );
            PL_LOCK
        }
    }
    else
    {
        /* No input. Several cases
         *  - No request, running status -> start new item
         *  - No request, stopped status -> collect garbage
         *  - Request, running requested -> start new item
         *  - Request, stopped requested -> collect garbage
         */
         if( (!p_playlist->request.b_request &&
              p_playlist->status.i_status != PLAYLIST_STOPPED) ||
              ( p_playlist->request.b_request &&
                p_playlist->request.i_status != PLAYLIST_STOPPED ) )
         {
             msg_Dbg( p_playlist, "Starting new item" );
             stats_TimerStart( p_playlist, "Playlist walk",
                                  STATS_TIMER_PLAYLIST_WALK );
             p_item = playlist_NextItem( p_playlist );
             stats_TimerStop( p_playlist, STATS_TIMER_PLAYLIST_WALK );

             if( p_item == NULL )
             {
                msg_Dbg( p_playlist, "nothing to play" );
                p_playlist->status.i_status = PLAYLIST_STOPPED;
                PL_UNLOCK
                return;
             }
             playlist_PlayItem( p_playlist, p_item );
         }
         else
         {
             if( p_item && p_playlist->status.p_item &&
                 p_playlist->status.p_item->i_flags & PLAYLIST_REMOVE_FLAG )
             {
                 playlist_ItemDelete( p_item );
                 p_playlist->status.p_item = NULL;
             }

             /* Collect garbage */
             PL_UNLOCK
             i_sout_destroyed_date =
             ObjectGarbageCollector( p_playlist, VLC_OBJECT_SOUT, mdate() );
             i_vout_destroyed_date =
             ObjectGarbageCollector( p_playlist, VLC_OBJECT_VOUT, mdate() );
             PL_LOCK
         }
    }
    PL_UNLOCK
}

/** Playlist dying last loop */
void playlist_LastLoop( playlist_t *p_playlist )
{
    vlc_object_t *p_obj;

    /* If there is an input, kill it */
    while( 1 )
    {
        PL_LOCK

        if( p_playlist->p_input == NULL )
        {
            PL_UNLOCK
            break;
        }

        if( p_playlist->p_input->b_dead )
        {
            input_thread_t *p_input;

            /* Unlink current input */
            p_input = p_playlist->p_input;
            p_playlist->p_input = NULL;
            PL_UNLOCK

            /* Destroy input */
            input_DestroyThread( p_input );
            /* Unlink current input (_after_ input_DestroyThread for vout
             * garbage collector)*/
            vlc_object_detach( p_input );

            /* Destroy object */
            vlc_object_destroy( p_input );
            continue;
        }
        else if( p_playlist->p_input->b_die )
        {
            /* This input is dying, leave it alone */
            ;
        }
        else if( p_playlist->p_input->b_error || p_playlist->p_input->b_eof )
        {
            input_StopThread( p_playlist->p_input );
            PL_UNLOCK
            continue;
        }
        else
        {
            p_playlist->p_input->b_eof = 1;
        }

        PL_UNLOCK

        msleep( INTF_IDLE_SLEEP );
    }

    /* close all remaining sout */
    while( ( p_obj = vlc_object_find( p_playlist,
                                      VLC_OBJECT_SOUT, FIND_CHILD ) ) )
    {
        vlc_object_release( p_obj );
        sout_DeleteInstance( (sout_instance_t*)p_obj );
    }

    /* close all remaining vout */
    while( ( p_obj = vlc_object_find( p_playlist,
                                      VLC_OBJECT_VOUT, FIND_CHILD ) ) )
    {
        vlc_object_detach( p_obj );
        vlc_object_release( p_obj );
        vout_Destroy( (vout_thread_t *)p_obj );
    }
}

/** Main loop for preparser queue */
void playlist_PreparseLoop( playlist_preparse_t *p_obj )
{
    playlist_t *p_playlist = (playlist_t *)p_obj->p_parent;
    int i_activity;

    vlc_mutex_lock( &p_obj->object_lock );

    if( p_obj->i_waiting > 0 )
    {
        input_item_t *p_current = p_playlist->p_preparse->pp_waiting[0];
        REMOVE_ELEM( p_obj->pp_waiting, p_obj->i_waiting, 0 );
        vlc_mutex_unlock( &p_obj->object_lock );
        vlc_mutex_lock( &p_playlist->object_lock );
        if( p_current )
        {
            vlc_bool_t b_preparsed = VLC_FALSE;
            if( strncmp( p_current->psz_uri, "http:", 5 ) &&
                strncmp( p_current->psz_uri, "rtsp:", 5 ) &&
                strncmp( p_current->psz_uri, "udp:", 4 ) &&
                strncmp( p_current->psz_uri, "mms:", 4 ) &&
                strncmp( p_current->psz_uri, "cdda:", 4 ) &&
                strncmp( p_current->psz_uri, "dvd:", 4 ) &&
                strncmp( p_current->psz_uri, "v4l:", 4 ) &&
                strncmp( p_current->psz_uri, "dshow:", 6 ) )
            {
                b_preparsed = VLC_TRUE;
                stats_TimerStart( p_playlist, "Preparse run",
                                  STATS_TIMER_PREPARSE );
                input_Preparse( p_playlist, p_current );
                stats_TimerStop( p_playlist, STATS_TIMER_PREPARSE );
            }
            vlc_mutex_unlock( &p_playlist->object_lock );
            if( b_preparsed )
            {
                var_SetInteger( p_playlist, "item-change",
                                p_current->i_id );
            }
            vlc_gc_decref( p_current );
        }
        else
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
        }
        vlc_mutex_lock( &p_obj->object_lock );
        i_activity=  var_GetInteger( p_playlist, "activity" );
        if( i_activity < 0 ) i_activity = 0;
        vlc_mutex_unlock( &p_obj->object_lock );
        msleep( (i_activity+1) * 1000 );
        return;
    }
    vlc_mutex_unlock( &p_obj->object_lock );
}

static void VariablesInit( playlist_t *p_playlist )
{
    vlc_value_t val;
    /* These variables control updates */
    var_Create( p_playlist, "intf-change", VLC_VAR_BOOL );
    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-change", val );

    var_Create( p_playlist, "item-change", VLC_VAR_INTEGER );
    val.i_int = -1;
    var_Set( p_playlist, "item-change", val );

    var_Create( p_playlist, "item-deleted", VLC_VAR_INTEGER );
    val.i_int = -1;
    var_Set( p_playlist, "item-deleted", val );

    var_Create( p_playlist, "item-append", VLC_VAR_ADDRESS );

    var_Create( p_playlist, "playlist-current", VLC_VAR_INTEGER );
    val.i_int = -1;
    var_Set( p_playlist, "playlist-current", val );

    var_Create( p_playlist, "intf-popupmenu", VLC_VAR_BOOL );

    var_Create( p_playlist, "intf-show", VLC_VAR_BOOL );
    val.b_bool = VLC_TRUE;
    var_Set( p_playlist, "intf-show", val );

    var_Create( p_playlist, "activity", VLC_VAR_INTEGER );
    var_SetInteger( p_playlist, "activity", 0 );

    /* Variables to control playback */
    var_CreateGetBool( p_playlist, "play-and-stop" );
    var_CreateGetBool( p_playlist, "random" );
    var_CreateGetBool( p_playlist, "repeat" );
    var_CreateGetBool( p_playlist, "loop" );
}
