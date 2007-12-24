/*****************************************************************************
 * engine.c : Run the playlist and handle its control
 *****************************************************************************
 * Copyright (C) 1999-2007 the VideoLAN team
 * $Id$
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
#include <vlc_vout.h>
#include <vlc_sout.h>
#include <vlc_playlist.h>
#include <vlc_interface.h>
#include "playlist_internal.h"
#include "stream_output/stream_output.h" /* sout_DeleteInstance */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void VariablesInit( playlist_t *p_playlist );

static int RandomCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval, void *a )
{
    (void)psz_cmd; (void)oldval; (void)newval; (void)a;

    ((playlist_t*)p_this)->b_reset_currently_playing = VLC_TRUE;
    playlist_Signal( ((playlist_t*)p_this) );
    return VLC_SUCCESS;
}

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
    vlc_bool_t b_save;
    int i_tree;

    /* Allocate structure */
    p_playlist = vlc_object_create( p_parent, VLC_OBJECT_PLAYLIST );
    if( !p_playlist )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

    TAB_INIT( p_playlist->i_sds, p_playlist->pp_sds );

    p_parent->p_libvlc->p_playlist = p_playlist;

    VariablesInit( p_playlist );

    /* Initialise data structures */
    vlc_mutex_init( p_playlist, &p_playlist->gc_lock );
    p_playlist->i_last_playlist_id = 0;
    p_playlist->i_last_input_id = 0;
    p_playlist->p_input = NULL;

    p_playlist->gc_date = 0;
    p_playlist->b_cant_sleep = VLC_FALSE;

    ARRAY_INIT( p_playlist->items );
    ARRAY_INIT( p_playlist->all_items );
    ARRAY_INIT( p_playlist->input_items );
    ARRAY_INIT( p_playlist->current );

    p_playlist->i_current_index = 0;
    p_playlist->b_reset_currently_playing = VLC_TRUE;
    p_playlist->last_rebuild_date = 0;

    i_tree = var_CreateGetBool( p_playlist, "playlist-tree" );
    p_playlist->b_always_tree = (i_tree == 1);
    p_playlist->b_never_tree = (i_tree == 2);

    p_playlist->b_doing_ml = VLC_FALSE;

    p_playlist->b_auto_preparse =
                        var_CreateGetBool( p_playlist, "auto-preparse") ;

    p_playlist->p_root_category = playlist_NodeCreate( p_playlist, NULL, NULL,
                                    0, NULL );
    p_playlist->p_root_onelevel = playlist_NodeCreate( p_playlist, NULL, NULL,
                                    0, p_playlist->p_root_category->p_input );

    if( !p_playlist->p_root_category || !p_playlist->p_root_onelevel )
        return NULL;

    /* Create playlist and media library */
    playlist_NodesPairCreate( p_playlist, _( "Playlist" ),
                            &p_playlist->p_local_category,
                            &p_playlist->p_local_onelevel, VLC_FALSE );

    p_playlist->p_local_category->i_flags |= PLAYLIST_RO_FLAG;
    p_playlist->p_local_onelevel->i_flags |= PLAYLIST_RO_FLAG;

    if( !p_playlist->p_local_category || !p_playlist->p_local_onelevel ||
        !p_playlist->p_local_category->p_input ||
        !p_playlist->p_local_onelevel->p_input )
        return NULL;

    if( config_GetInt( p_playlist, "media-library") )
    {
        playlist_NodesPairCreate( p_playlist, _( "Media Library" ),
                            &p_playlist->p_ml_category,
                            &p_playlist->p_ml_onelevel, VLC_FALSE );

        if(!p_playlist->p_ml_category || !p_playlist->p_ml_onelevel)
            return NULL;

        p_playlist->p_ml_category->i_flags |= PLAYLIST_RO_FLAG;
        p_playlist->p_ml_onelevel->i_flags |= PLAYLIST_RO_FLAG;
    }
    else
    {
        p_playlist->p_ml_category = p_playlist->p_ml_onelevel = NULL;
    }

    /* Initial status */
    p_playlist->status.p_item = NULL;
    p_playlist->status.p_node = p_playlist->p_local_onelevel;
    p_playlist->request.b_request = VLC_FALSE;
    p_playlist->status.i_status = PLAYLIST_STOPPED;

    p_playlist->i_sort = SORT_ID;
    p_playlist->i_order = ORDER_NORMAL;

    b_save = p_playlist->b_auto_preparse;
    p_playlist->b_auto_preparse = VLC_FALSE;
    playlist_MLLoad( p_playlist );
    p_playlist->b_auto_preparse = VLC_TRUE;
    return p_playlist;
}

void playlist_Destroy( playlist_t *p_playlist )
{
    var_Destroy( p_playlist, "intf-change" );
    var_Destroy( p_playlist, "item-change" );
    var_Destroy( p_playlist, "playlist-current" );
    var_Destroy( p_playlist, "intf-popupmenu" );
    var_Destroy( p_playlist, "intf-show" );
    var_Destroy( p_playlist, "play-and-stop" );
    var_Destroy( p_playlist, "play-and-exit" );
    var_Destroy( p_playlist, "random" );
    var_Destroy( p_playlist, "repeat" );
    var_Destroy( p_playlist, "loop" );
    var_Destroy( p_playlist, "activity" );

    vlc_mutex_destroy( &p_playlist->gc_lock );
    vlc_object_detach( p_playlist );
    vlc_object_destroy( p_playlist );
}

/* Destroy remaining objects */
static void ObjectGarbageCollector( playlist_t *p_playlist, vlc_bool_t b_force )
{
    vlc_object_t *p_obj;

    if( !b_force )
    {
        if( mdate() - p_playlist->gc_date < 1000000 )
        {
            p_playlist->b_cant_sleep = VLC_TRUE;
            return;
        }
        else if( p_playlist->gc_date == 0 )
            return;
    }

    vlc_mutex_lock( &p_playlist->gc_lock );
    while( ( p_obj = vlc_object_find( p_playlist, VLC_OBJECT_VOUT,
                                                  FIND_CHILD ) ) )
    {
        if( p_obj->p_parent != VLC_OBJECT(p_playlist) )
        {
            vlc_object_release( p_obj );
            break;
        }
        msg_Dbg( p_playlist, "garbage collector destroying 1 vout" );
        vlc_object_detach( p_obj );
        vlc_object_release( p_obj );
        vout_Destroy( (vout_thread_t *)p_obj );
    }
    while( ( p_obj = vlc_object_find( p_playlist, VLC_OBJECT_SOUT,
                                                  FIND_CHILD ) ) )
    {
        if( p_obj->p_parent != VLC_OBJECT(p_playlist) )
        {
            vlc_object_release( p_obj );
            break;
        }
        msg_Dbg( p_playlist, "garbage collector destroying 1 sout" );
        vlc_object_detach( p_obj );
        vlc_object_release( p_obj );
        sout_DeleteInstance( (sout_instance_t*)p_obj );
    }
    p_playlist->b_cant_sleep = VLC_FALSE;
    vlc_mutex_unlock( &p_playlist->gc_lock );
}

/** Main loop for the playlist */
void playlist_MainLoop( playlist_t *p_playlist )
{
    playlist_item_t *p_item = NULL;
    vlc_bool_t b_playexit = var_GetBool( p_playlist, "play-and-exit" );
    PL_LOCK;

    if( p_playlist->b_reset_currently_playing &&
        mdate() - p_playlist->last_rebuild_date > 30000 ) // 30 ms
    {
        ResetCurrentlyPlaying( p_playlist, var_GetBool( p_playlist, "random"),
                             p_playlist->status.p_item );
        p_playlist->last_rebuild_date = mdate();
    }

check_input:
    /* If there is an input, check that it doesn't need to die. */
    if( p_playlist->p_input )
    {
        if( p_playlist->request.b_request && !p_playlist->p_input->b_die )
        {
            PL_DEBUG( "incoming request - stopping current input" );
            input_StopThread( p_playlist->p_input );
        }

        /* This input is dead. Remove it ! */
        if( p_playlist->p_input->b_dead )
        {
            int i_activity;
            input_thread_t *p_input;
            PL_DEBUG( "dead input" );

            p_input = p_playlist->p_input;
            p_playlist->p_input = NULL;

            /* Release the playlist lock, because we may get stuck
             * in input_DestroyThread() for some time. */
            PL_UNLOCK;

            /* Destroy input */
            input_DestroyThread( p_input );

            PL_LOCK;

            p_playlist->gc_date = mdate();
            p_playlist->b_cant_sleep = VLC_TRUE;

            if( p_playlist->status.p_item->i_flags
                & PLAYLIST_REMOVE_FLAG )
            {
                 PL_DEBUG( "%s was marked for deletion, deleting",
                                 PLI_NAME( p_playlist->status.p_item  ) );
                 playlist_ItemDelete( p_playlist->status.p_item );
                 if( p_playlist->request.p_item == p_playlist->status.p_item )
                     p_playlist->request.p_item = NULL;
                 p_playlist->status.p_item = NULL;
            }

            i_activity= var_GetInteger( p_playlist, "activity") ;
            var_SetInteger( p_playlist, "activity", i_activity -
                            DEFAULT_INPUT_ACTIVITY );
            goto check_input;
        }
        /* This input is dying, let it do */
        else if( p_playlist->p_input->b_die )
        {
            PL_DEBUG( "dying input" );
            PL_UNLOCK;
            msleep( INTF_IDLE_SLEEP );
            PL_LOCK;
            goto check_input;
        }
        /* This input has finished, ask it to die ! */
        else if( p_playlist->p_input->b_error
                  || p_playlist->p_input->b_eof )
        {
            PL_DEBUG( "finished input" );
            input_StopThread( p_playlist->p_input );
            /* No need to wait here, we'll wait in the p_input->b_die case */
            goto check_input;
        }
        else if( p_playlist->p_input->i_state != INIT_S )
        {
            PL_UNLOCK;
            ObjectGarbageCollector( p_playlist, VLC_FALSE );
            PL_LOCK;
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
             msg_Dbg( p_playlist, "starting new item" );
             p_item = playlist_NextItem( p_playlist );

             if( p_item == NULL )
             {
                msg_Dbg( p_playlist, "nothing to play" );
                p_playlist->status.i_status = PLAYLIST_STOPPED;
                PL_UNLOCK;

                if( b_playexit == VLC_TRUE )
                {
                    msg_Info( p_playlist, "end of playlist, exiting" );
                    vlc_object_kill( p_playlist->p_libvlc );
                }
                ObjectGarbageCollector( p_playlist, VLC_TRUE );
                return;
             }
             playlist_PlayItem( p_playlist, p_item );
         }
         else
         {
            const vlc_bool_t b_gc_forced = p_playlist->status.i_status != PLAYLIST_STOPPED;

            p_playlist->status.i_status = PLAYLIST_STOPPED;
            if( p_playlist->status.p_item &&
                p_playlist->status.p_item->i_flags & PLAYLIST_REMOVE_FLAG )
            {
                PL_DEBUG( "deleting item marked for deletion" );
                playlist_ItemDelete( p_playlist->status.p_item );
                p_playlist->status.p_item = NULL;
            }

            /* Collect garbage */
            PL_UNLOCK;
            ObjectGarbageCollector( p_playlist, b_gc_forced );
            PL_LOCK;
        }
    }
    PL_UNLOCK;
}

/** Playlist dying last loop */
void playlist_LastLoop( playlist_t *p_playlist )
{
    vlc_object_t *p_obj;

    /* If there is an input, kill it */
    while( 1 )
    {
        PL_LOCK;
        if( p_playlist->p_input == NULL )
        {
            PL_UNLOCK;
            break;
        }

        if( p_playlist->p_input->b_dead )
        {
            input_thread_t *p_input;

            /* Unlink current input */
            p_input = p_playlist->p_input;
            p_playlist->p_input = NULL;
            PL_UNLOCK;

            /* Destroy input */
            input_DestroyThread( p_input );
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
            PL_UNLOCK;
            continue;
        }
        else
        {
            p_playlist->p_input->b_eof = 1;
        }
        PL_UNLOCK;

        msleep( INTF_IDLE_SLEEP );
    }

    /* close all remaining sout */
    while( ( p_obj = vlc_object_find( p_playlist,
                                      VLC_OBJECT_SOUT, FIND_CHILD ) ) )
    {
        vlc_object_detach( p_obj );
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

    while( p_playlist->i_sds )
    {
        playlist_ServicesDiscoveryRemove( p_playlist,
                                          p_playlist->pp_sds[0]->p_sd->psz_module );
    }

    playlist_MLDump( p_playlist );

    PL_LOCK;
    /* Go through all items, and simply free everything without caring
     * about the tree structure. Do not decref, it will be done by doing
     * the same thing on the input items array */
    FOREACH_ARRAY( playlist_item_t *p_del, p_playlist->all_items )
        free( p_del->pp_children );
        free( p_del );
    FOREACH_END();
    ARRAY_RESET( p_playlist->all_items );

    FOREACH_ARRAY( input_item_t *p_del, p_playlist->input_items )
        input_ItemClean( p_del );
        free( p_del );
    FOREACH_END();
    ARRAY_RESET( p_playlist->input_items );

    ARRAY_RESET( p_playlist->items );
    ARRAY_RESET( p_playlist->current );

    PL_UNLOCK;
}

/** Main loop for preparser queue */
void playlist_PreparseLoop( playlist_preparse_t *p_obj )
{
    playlist_t *p_playlist = (playlist_t *)p_obj->p_parent;
    input_item_t *p_current;
    int i_activity;
    uint32_t i_m, i_o;

    while( !p_playlist->b_die )
    {
        vlc_mutex_lock( &p_obj->object_lock );
        while( p_obj->i_waiting == 0 )
        {
            vlc_cond_wait( &p_obj->object_wait, &p_obj->object_lock );
            if( p_playlist->b_die )
            {
                vlc_mutex_unlock( &p_obj->object_lock );
                return;
            }
        }

        p_current = p_obj->pp_waiting[0];
        REMOVE_ELEM( p_obj->pp_waiting, p_obj->i_waiting, 0 );
        vlc_mutex_unlock( &p_obj->object_lock );

        PL_LOCK;
        if( p_current )
        {
            if( p_current->i_type == ITEM_TYPE_FILE )
            {
                stats_TimerStart( p_playlist, "Preparse run",
                                  STATS_TIMER_PREPARSE );
                /* Do not preparse if it is already done (like by playing it) */
                if( !input_item_IsPreparsed( p_current ) )
                {
                    PL_UNLOCK;
                    input_Preparse( p_playlist, p_current );
                    PL_LOCK;
                }
                stats_TimerStop( p_playlist, STATS_TIMER_PREPARSE );
                PL_UNLOCK;
                input_item_SetPreparsed( p_current, VLC_TRUE );
                var_SetInteger( p_playlist, "item-change", p_current->i_id );
                PL_LOCK;
            }
            /* If we haven't retrieved enough meta, add to secondary queue
             * which will run the "meta fetchers".
             * This only checks for meta, not for art
             * \todo don't do this for things we won't get meta for, like vids
             */
            char *psz_arturl = input_item_GetArtURL( p_current );
            char *psz_name = input_item_GetName( p_current );
            if( !input_MetaSatisfied( p_playlist, p_current, &i_m, &i_o ) )
            {
                preparse_item_t p;
                PL_DEBUG("need to fetch meta for %s", p_current->psz_name );
                p.p_item = p_current;
                p.b_fetch_art = VLC_FALSE;
                vlc_mutex_lock( &p_playlist->p_fetcher->object_lock );
                INSERT_ELEM( p_playlist->p_fetcher->p_waiting,
                             p_playlist->p_fetcher->i_waiting,
                             p_playlist->p_fetcher->i_waiting, p);
                vlc_cond_signal( &p_playlist->p_fetcher->object_wait );
                vlc_mutex_unlock( &p_playlist->p_fetcher->object_lock );
            }
            /* We already have all needed meta, but we need art right now */
            else if( p_playlist->p_fetcher->i_art_policy == ALBUM_ART_ALL &&
                        ( !psz_arturl || strncmp( psz_arturl, "file://", 7 ) ) )
            {
                preparse_item_t p;
                PL_DEBUG("meta ok for %s, need to fetch art", psz_name );
                p.p_item = p_current;
                p.b_fetch_art = VLC_TRUE;
                vlc_mutex_lock( &p_playlist->p_fetcher->object_lock );
                INSERT_ELEM( p_playlist->p_fetcher->p_waiting,
                             p_playlist->p_fetcher->i_waiting,
                             p_playlist->p_fetcher->i_waiting, p);
                vlc_cond_signal( &p_playlist->p_fetcher->object_wait );
                vlc_mutex_unlock( &p_playlist->p_fetcher->object_lock );
            }
            else
            {
                PL_DEBUG( "no fetch required for %s (art currently %s)",
                          psz_name, psz_arturl );
                vlc_gc_decref( p_current );
            }
            free( psz_name );
            free( psz_arturl );
            PL_UNLOCK;
        }
        else
            PL_UNLOCK;

        vlc_mutex_lock( &p_obj->object_lock );
        i_activity = var_GetInteger( p_playlist, "activity" );
        if( i_activity < 0 ) i_activity = 0;
        vlc_mutex_unlock( &p_obj->object_lock );
        /* Sleep at least 1ms */
        msleep( (i_activity+1) * 1000 );
    }
}

/** Main loop for secondary preparser queue */
void playlist_FetcherLoop( playlist_fetcher_t *p_obj )
{
    playlist_t *p_playlist = (playlist_t *)p_obj->p_parent;
    vlc_bool_t b_fetch_art;
    input_item_t *p_item;
    int i_activity;

    while( !p_playlist->b_die )
    {
        vlc_mutex_lock( &p_obj->object_lock );
        while( p_obj->i_waiting == 0 )
        {
            vlc_cond_wait( &p_obj->object_wait, &p_obj->object_lock );
            if( p_playlist->b_die )
            {
                vlc_mutex_unlock( &p_obj->object_lock );
                return;
            }
        }

        b_fetch_art = p_obj->p_waiting->b_fetch_art;
        p_item = p_obj->p_waiting->p_item;
        REMOVE_ELEM( p_obj->p_waiting, p_obj->i_waiting, 0 );
        vlc_mutex_unlock( &p_obj->object_lock );
        if( p_item )
        {
            if( !b_fetch_art )
            {
                /* If the user doesn't want us to fetch meta automatically 
                 * abort here. */
                if( p_playlist->p_fetcher->b_fetch_meta )
                {
                    input_MetaFetch( p_playlist, p_item );
                    var_SetInteger( p_playlist, "item-change", p_item->i_id );
                }

                /*  Fetch right now */
                if( p_playlist->p_fetcher->i_art_policy == ALBUM_ART_ALL )
                {
                    vlc_mutex_lock( &p_obj->object_lock );
                    preparse_item_t p;
                    p.p_item = p_item;
                    p.b_fetch_art = VLC_TRUE;
                    INSERT_ELEM( p_playlist->p_fetcher->p_waiting,
                                 p_playlist->p_fetcher->i_waiting,
                                 0, p );
                    PL_DEBUG("meta fetched for %s, get art", p_item->psz_name);
                    vlc_mutex_unlock( &p_obj->object_lock );
                    continue;
                }
                else
                    vlc_gc_decref( p_item );
            }
            else
            {
                int i_ret;

                /* Check if it is not yet preparsed and if so wait for it (at most 0.5s)
                 * (This can happen if we fetch art on play)
                 * FIXME this doesn't work if we need to fetch meta before art ... */
                for( i_ret = 0; i_ret < 10 && !input_item_IsPreparsed( p_item ); i_ret++ )
                {
                    vlc_bool_t b_break;
                    PL_LOCK;
                    b_break = ( !p_playlist->p_input || input_GetItem(p_playlist->p_input) != p_item  ||
                                p_playlist->p_input->b_die || p_playlist->p_input->b_eof || p_playlist->p_input->b_error );
                    PL_UNLOCK;
                    if( b_break )
                        break;
                    msleep( 50000 );
                }

                i_ret = input_ArtFind( p_playlist, p_item );
                if( i_ret == 1 )
                {
                    PL_DEBUG("downloading art for %s", p_item->psz_name );
                    if( input_DownloadAndCacheArt( p_playlist, p_item ) )
                        input_item_SetArtNotFound( p_item, VLC_TRUE );
                    else {
                        input_item_SetArtFetched( p_item, VLC_TRUE );
                        var_SetInteger( p_playlist, "item-change",
                                        p_item->i_id );
                    }
                }
                else if( i_ret == 0 ) /* Was in cache */
                {
                    PL_DEBUG("found art for %s in cache", p_item->psz_name );
                    input_item_SetArtFetched( p_item, VLC_TRUE );
                    var_SetInteger( p_playlist, "item-change", p_item->i_id );
                }
                else
                {
                    PL_DEBUG("art not found for %s", p_item->psz_name );
                    input_item_SetArtNotFound( p_item, VLC_TRUE );
                }
                vlc_gc_decref( p_item );
           }
        }
        vlc_mutex_lock( &p_obj->object_lock );
        i_activity = var_GetInteger( p_playlist, "activity" );
        if( i_activity < 0 ) i_activity = 0;
        vlc_mutex_unlock( &p_obj->object_lock );
        /* Sleep at least 1ms */
        msleep( (i_activity+1) * 1000 );
    }
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
    var_CreateGetBool( p_playlist, "play-and-exit" );
    var_CreateGetBool( p_playlist, "random" );
    var_CreateGetBool( p_playlist, "repeat" );
    var_CreateGetBool( p_playlist, "loop" );

    var_AddCallback( p_playlist, "random", RandomCallback, NULL );
}
