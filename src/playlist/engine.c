/*****************************************************************************
 * engine.c : Run the playlist and handle its control
 *****************************************************************************
 * Copyright (C) 1999-2008 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>
#include <vlc_sout.h>
#include <vlc_playlist.h>
#include <vlc_interface.h>
#include "playlist_internal.h"
#include "stream_output/stream_output.h" /* sout_DeleteInstance */

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void VariablesInit( playlist_t *p_playlist );
static void playlist_Destructor( vlc_object_t * p_this );
static void playlist_Destructor( vlc_object_t * p_this );

static int RandomCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval, void *a )
{
    (void)psz_cmd; (void)oldval; (void)newval; (void)a;

    ((playlist_t*)p_this)->b_reset_currently_playing = true;
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
    static const char playlist_name[] = "playlist";
    playlist_t *p_playlist;
    bool b_save;

    /* Allocate structure */
    p_playlist = vlc_custom_create( p_parent, sizeof( *p_playlist ),
                                    VLC_OBJECT_GENERIC, playlist_name );
    if( !p_playlist )
        return NULL;

    TAB_INIT( p_playlist->i_sds, p_playlist->pp_sds );

    libvlc_priv(p_parent->p_libvlc)->p_playlist = p_playlist;

    VariablesInit( p_playlist );

    /* Initialise data structures */
    p_playlist->i_last_playlist_id = 0;
    p_playlist->p_input = NULL;

    p_playlist->gc_date = 0;
    p_playlist->b_cant_sleep = false;

    ARRAY_INIT( p_playlist->items );
    ARRAY_INIT( p_playlist->all_items );
    ARRAY_INIT( p_playlist->items_to_delete );
    ARRAY_INIT( p_playlist->current );

    p_playlist->i_current_index = 0;
    p_playlist->b_reset_currently_playing = true;
    p_playlist->last_rebuild_date = 0;

    p_playlist->b_tree = var_CreateGetBool( p_playlist, "playlist-tree" );

    p_playlist->b_doing_ml = false;

    p_playlist->b_auto_preparse =
                        var_CreateGetBool( p_playlist, "auto-preparse" ) ;

    PL_LOCK; /* playlist_NodeCreate will check for it */
    p_playlist->p_root_category = playlist_NodeCreate( p_playlist, NULL, NULL,
                                    0, NULL );
    p_playlist->p_root_onelevel = playlist_NodeCreate( p_playlist, NULL, NULL,
                                    0, p_playlist->p_root_category->p_input );
    PL_UNLOCK;

    if( !p_playlist->p_root_category || !p_playlist->p_root_onelevel )
        return NULL;

    /* Create playlist and media library */
    PL_LOCK; /* playlist_NodesPairCreate will check for it */
    playlist_NodesPairCreate( p_playlist, _( "Playlist" ),
                            &p_playlist->p_local_category,
                            &p_playlist->p_local_onelevel, false );
    PL_UNLOCK;

    p_playlist->p_local_category->i_flags |= PLAYLIST_RO_FLAG;
    p_playlist->p_local_onelevel->i_flags |= PLAYLIST_RO_FLAG;

    if( !p_playlist->p_local_category || !p_playlist->p_local_onelevel ||
        !p_playlist->p_local_category->p_input ||
        !p_playlist->p_local_onelevel->p_input )
        return NULL;

    if( config_GetInt( p_playlist, "media-library") )
    {
        PL_LOCK; /* playlist_NodesPairCreate will check for it */
        playlist_NodesPairCreate( p_playlist, _( "Media Library" ),
                            &p_playlist->p_ml_category,
                            &p_playlist->p_ml_onelevel, false );
        PL_UNLOCK;

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
    p_playlist->request.b_request = false;
    p_playlist->status.i_status = PLAYLIST_STOPPED;

    p_playlist->i_sort = SORT_ID;
    p_playlist->i_order = ORDER_NORMAL;


    b_save = p_playlist->b_auto_preparse;
    p_playlist->b_auto_preparse = false;
    playlist_MLLoad( p_playlist );
    p_playlist->b_auto_preparse = true;

    vlc_object_set_destructor( p_playlist, playlist_Destructor );

    return p_playlist;
}

/**
 * Destroy playlist
 *
 * Destroy a playlist structure.
 * \param p_playlist the playlist object
 * \return nothing
 */

static void playlist_Destructor( vlc_object_t * p_this )
{
    playlist_t * p_playlist = (playlist_t *)p_this;

    if( p_playlist->p_preparse )
    {
        vlc_object_release( p_playlist->p_preparse );
    }

    if( p_playlist->p_fetcher )
    {
        vlc_object_release( p_playlist->p_fetcher );
    }
    msg_Dbg( p_this, "Destroyed" );
}

/* Destroy remaining objects */
static void ObjectGarbageCollector( playlist_t *p_playlist, bool b_force )
{
    if( !b_force )
    {
        if( mdate() - p_playlist->gc_date < 1000000 )
        {
            p_playlist->b_cant_sleep = true;
            return;
        }
        else if( p_playlist->gc_date == 0 )
            return;
    }

    p_playlist->b_cant_sleep = false;
}

/* Input Callback */
static void input_state_changed( const vlc_event_t * event, void * data )
{
    (void)event;
    playlist_t * p_playlist = data;
    playlist_Signal( p_playlist );
}

/* Input Callback */
static void input_selected_stream_changed( const vlc_event_t * event, void * data )
{
    (void)event;
    playlist_t * p_playlist = data;
    PL_LOCK;
    p_playlist->gc_date = mdate();
    vlc_object_signal_unlocked( p_playlist );
    PL_UNLOCK;
}

/* Internals */
void playlist_release_current_input( playlist_t * p_playlist )
{
    PL_ASSERT_LOCKED;

    if( !p_playlist->p_input ) return;

    input_thread_t * p_input = p_playlist->p_input;
    vlc_event_manager_t * p_em = input_get_event_manager( p_input );

    vlc_event_detach( p_em, vlc_InputStateChanged,
                      input_state_changed, p_playlist );
    vlc_event_detach( p_em, vlc_InputSelectedStreamChanged,
                      input_selected_stream_changed, p_playlist );
    p_playlist->p_input = NULL;

    /* Release the playlist lock, because we may get stuck
     * in vlc_object_release() for some time. */
    PL_UNLOCK;
    vlc_thread_join( p_input );
    vlc_object_release( p_input );
    PL_LOCK;
}

void playlist_set_current_input(
    playlist_t * p_playlist, input_thread_t * p_input )
{
    PL_ASSERT_LOCKED;

    playlist_release_current_input( p_playlist );

    if( p_input )
    {
        vlc_object_yield( p_input );
        p_playlist->p_input = p_input;
        vlc_event_manager_t * p_em = input_get_event_manager( p_input );
        vlc_event_attach( p_em, vlc_InputStateChanged,
                          input_state_changed, p_playlist );
        vlc_event_attach( p_em, vlc_InputSelectedStreamChanged,
                          input_selected_stream_changed, p_playlist );
    }
}

/** Get current playing input.
 */
input_thread_t * playlist_CurrentInput( playlist_t * p_playlist )
{
    input_thread_t * p_input;
    PL_LOCK;
    p_input = p_playlist->p_input;
    if( p_input ) vlc_object_yield( p_input );
    PL_UNLOCK;
    return p_input;
}

/**
 * @}
 */

/** Accessor for status item and status nodes.
 */
playlist_item_t * get_current_status_item( playlist_t * p_playlist )
{
    PL_ASSERT_LOCKED;

    return p_playlist->status.p_item;
}

playlist_item_t * get_current_status_node( playlist_t * p_playlist )
{
    PL_ASSERT_LOCKED;

    return p_playlist->status.p_node;
}

void set_current_status_item( playlist_t * p_playlist,
    playlist_item_t * p_item )
{
    PL_ASSERT_LOCKED;

    if( p_playlist->status.p_item &&
        p_playlist->status.p_item->i_flags & PLAYLIST_REMOVE_FLAG &&
        p_playlist->status.p_item != p_item )
    {
        /* It's unsafe given current design to delete a playlist item :(
        playlist_ItemDelete( p_playlist->status.p_item ); */
    }
    p_playlist->status.p_item = p_item;
}

void set_current_status_node( playlist_t * p_playlist,
    playlist_item_t * p_node )
{
    PL_ASSERT_LOCKED;

    if( p_playlist->status.p_node &&
        p_playlist->status.p_node->i_flags & PLAYLIST_REMOVE_FLAG &&
        p_playlist->status.p_node != p_node )
    {
        /* It's unsafe given current design to delete a playlist item :(
        playlist_ItemDelete( p_playlist->status.p_node ); */
    }
    p_playlist->status.p_node = p_node;
}

/**
 * Main loop
 *
 * Main loop for the playlist. It should be entered with the
 * playlist lock (otherwise input event may be lost)
 * \param p_playlist the playlist object
 * \return nothing
 */
void playlist_MainLoop( playlist_t *p_playlist )
{
    playlist_item_t *p_item = NULL;
    bool b_playexit = var_GetBool( p_playlist, "play-and-exit" );

    PL_ASSERT_LOCKED;

    if( p_playlist->b_reset_currently_playing &&
        mdate() - p_playlist->last_rebuild_date > 30000 ) // 30 ms
    {
        ResetCurrentlyPlaying( p_playlist, var_GetBool( p_playlist, "random" ),
                               get_current_status_item( p_playlist ) );
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
            sout_instance_t **pp_sout =
                &libvlc_priv(p_playlist->p_libvlc)->p_sout;

            PL_DEBUG( "dead input" );

            p_input = p_playlist->p_input;

            assert( *pp_sout == NULL );
            if( var_CreateGetBool( p_input, "sout-keep" ) )
                *pp_sout = input_DetachSout( p_input );

            /* Destroy input */
            playlist_release_current_input( p_playlist );

            p_playlist->gc_date = mdate();
            p_playlist->b_cant_sleep = true;

            i_activity= var_GetInteger( p_playlist, "activity" );
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
            ObjectGarbageCollector( p_playlist, false );
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
        int i_status = p_playlist->request.b_request ?
            p_playlist->request.i_status : p_playlist->status.i_status;
        if( i_status != PLAYLIST_STOPPED )
        {
            msg_Dbg( p_playlist, "starting new item" );
            p_item = playlist_NextItem( p_playlist );

            if( p_item == NULL )
            {
                msg_Dbg( p_playlist, "nothing to play" );
                p_playlist->status.i_status = PLAYLIST_STOPPED;

                if( b_playexit == true )
                {
                    msg_Info( p_playlist, "end of playlist, exiting" );
                    vlc_object_kill( p_playlist->p_libvlc );
                }
                ObjectGarbageCollector( p_playlist, true );
                return;
            }
            playlist_PlayItem( p_playlist, p_item );
            /* playlist_PlayItem loose input event, we need to recheck */
            goto check_input;
        }
        else
        {
            const bool b_gc_forced = p_playlist->status.i_status != PLAYLIST_STOPPED;

            p_playlist->status.i_status = PLAYLIST_STOPPED;

            /* Collect garbage */
            ObjectGarbageCollector( p_playlist, b_gc_forced );
        }
    }
}

/**
 * Last loop
 *
 * The playlist is dying so do the last loop
 * \param p_playlist the playlist object
 * \return nothing
*/
void playlist_LastLoop( playlist_t *p_playlist )
{
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
            /* remove input */
            playlist_release_current_input( p_playlist );

            /* sout-keep: no need to anything here.
             * The last input will destroy its sout, if any, by itself */

            PL_UNLOCK;
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

#ifdef ENABLE_SOUT
    /* close the remaining sout-keep (if there was no input atm) */
    sout_instance_t *p_sout = libvlc_priv (p_playlist->p_libvlc)->p_sout;
    if (p_sout)
        sout_DeleteInstance( p_sout );
#endif

    /* Core should have terminated all SDs before the playlist */
    /* TODO: It fails to do so when not playing anything -- Courmisch */
    playlist_ServicesDiscoveryKillAll( p_playlist );
    playlist_MLDump( p_playlist );

    vlc_object_kill( p_playlist->p_preparse );
    vlc_thread_join( p_playlist->p_preparse );
    vlc_object_kill( p_playlist->p_fetcher );
    vlc_thread_join( p_playlist->p_fetcher );

    PL_LOCK;

    /* Release the current node */
    set_current_status_node( p_playlist, NULL );

    /* Release the current item */
    set_current_status_item( p_playlist, NULL );

    FOREACH_ARRAY( playlist_item_t *p_del, p_playlist->all_items )
        free( p_del->pp_children );
        vlc_gc_decref( p_del->p_input );
        free( p_del );
    FOREACH_END();
    ARRAY_RESET( p_playlist->all_items );
    FOREACH_ARRAY( playlist_item_t *p_del, p_playlist->items_to_delete )
        free( p_del->pp_children );
        vlc_gc_decref( p_del->p_input );
        free( p_del );
    FOREACH_END();
    ARRAY_RESET( p_playlist->items_to_delete );

    ARRAY_RESET( p_playlist->items );
    ARRAY_RESET( p_playlist->current );

    PL_UNLOCK;
}

/**
 * Preparse loop
 *
 * Main loop for preparser queue
 * \param p_obj items to preparse
 * \return nothing
 */
void playlist_PreparseLoop( playlist_preparse_t *p_obj )
{
    playlist_t *p_playlist = (playlist_t *)p_obj->p_parent;
    input_item_t *p_current;
    int i_activity;

    vlc_object_lock( p_obj );

    while( vlc_object_alive( p_obj ) )
    {
        if( p_obj->i_waiting == 0 )
        {
            vlc_object_wait( p_obj );
            continue;
        }

        p_current = p_obj->pp_waiting[0];
        REMOVE_ELEM( p_obj->pp_waiting, p_obj->i_waiting, 0 );
        vlc_object_unlock( p_obj );

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
                input_item_SetPreparsed( p_current, true );
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
            if( p_playlist->p_fetcher->i_art_policy == ALBUM_ART_ALL &&
                        ( !psz_arturl || strncmp( psz_arturl, "file://", 7 ) ) )
            {
                PL_DEBUG("meta ok for %s, need to fetch art", psz_name );
                vlc_object_lock( p_playlist->p_fetcher );
                if( vlc_object_alive( p_playlist->p_fetcher ) )
                {
                    INSERT_ELEM( p_playlist->p_fetcher->pp_waiting,
                        p_playlist->p_fetcher->i_waiting,
                        p_playlist->p_fetcher->i_waiting, p_current);
                    vlc_object_signal_unlocked( p_playlist->p_fetcher );
                }
                else
                    vlc_gc_decref( p_current );
                vlc_object_unlock( p_playlist->p_fetcher );
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

        vlc_object_lock( p_obj );
        i_activity = var_GetInteger( p_playlist, "activity" );
        if( i_activity < 0 ) i_activity = 0;
        vlc_object_unlock( p_obj );
        /* Sleep at least 1ms */
        msleep( (i_activity+1) * 1000 );
        vlc_object_lock( p_obj );
    }

    while( p_obj->i_waiting > 0 )
    {
        vlc_gc_decref( p_obj->pp_waiting[0] );
        REMOVE_ELEM( p_obj->pp_waiting, p_obj->i_waiting, 0 );
    }

    vlc_object_unlock( p_obj );
}

/**
 * Fetcher loop
 *
 * Main loop for secondary preparser queue
 * \param p_obj items to preparse
 * \return nothing
 */
void playlist_FetcherLoop( playlist_fetcher_t *p_obj )
{
    playlist_t *p_playlist = (playlist_t *)p_obj->p_parent;
    input_item_t *p_item;
    int i_activity;

    vlc_object_lock( p_obj );

    while( vlc_object_alive( p_obj ) )
    {
        if( p_obj->i_waiting == 0 )
        {
            vlc_object_wait( p_obj );
            continue;
        }

        p_item = p_obj->pp_waiting[0];
        REMOVE_ELEM( p_obj->pp_waiting, p_obj->i_waiting, 0 );
        vlc_object_unlock( p_obj );
        if( p_item )
        {
            int i_ret;

            /* Check if it is not yet preparsed and if so wait for it (at most 0.5s)
             * (This can happen if we fetch art on play)
             * FIXME this doesn't work if we need to fetch meta before art ... */
            for( i_ret = 0; i_ret < 10 && !input_item_IsPreparsed( p_item ); i_ret++ )
            {
                bool b_break;
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
                PL_DEBUG( "downloading art for %s", p_item->psz_name );
                if( input_DownloadAndCacheArt( p_playlist, p_item ) )
                    input_item_SetArtNotFound( p_item, true );
                else {
                    input_item_SetArtFetched( p_item, true );
                    var_SetInteger( p_playlist, "item-change",
                                    p_item->i_id );
                }
            }
            else if( i_ret == 0 ) /* Was in cache */
            {
                PL_DEBUG( "found art for %s in cache", p_item->psz_name );
                input_item_SetArtFetched( p_item, true );
                var_SetInteger( p_playlist, "item-change", p_item->i_id );
            }
            else
            {
                PL_DEBUG( "art not found for %s", p_item->psz_name );
                input_item_SetArtNotFound( p_item, true );
            }
            vlc_gc_decref( p_item );
        }
        vlc_object_lock( p_obj );
        i_activity = var_GetInteger( p_playlist, "activity" );
        if( i_activity < 0 ) i_activity = 0;
        vlc_object_unlock( p_obj );
        /* Sleep at least 1ms */
        msleep( (i_activity+1) * 1000 );
        vlc_object_lock( p_obj );
    }

    while( p_obj->i_waiting > 0 )
    {
        vlc_gc_decref( p_obj->pp_waiting[0] );
        REMOVE_ELEM( p_obj->pp_waiting, p_obj->i_waiting, 0 );
    }

    vlc_object_unlock( p_obj );
}

static void VariablesInit( playlist_t *p_playlist )
{
    vlc_value_t val;
    /* These variables control updates */
    var_Create( p_playlist, "intf-change", VLC_VAR_BOOL );
    val.b_bool = true;
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
