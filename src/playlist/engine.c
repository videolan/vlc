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

#include <stddef.h>
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

static int RandomCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval, void *a )
{
    (void)psz_cmd; (void)oldval; (void)newval; (void)a;
    playlist_t *p_playlist = (playlist_t*)p_this;

    PL_LOCK;

    pl_priv(p_playlist)->b_reset_currently_playing = true;
    vlc_object_signal_unlocked( p_playlist );

    PL_UNLOCK;
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
    playlist_private_t *p;

    /* Allocate structure */
    p = vlc_custom_create( p_parent, sizeof( *p ),
                           VLC_OBJECT_GENERIC, playlist_name );
    if( !p )
        return NULL;

    assert( offsetof( playlist_private_t, public_data ) == 0 );
    p_playlist = &p->public_data;
    TAB_INIT( pl_priv(p_playlist)->i_sds, pl_priv(p_playlist)->pp_sds );

    libvlc_priv(p_parent->p_libvlc)->p_playlist = p_playlist;

    VariablesInit( p_playlist );

    /* Initialise data structures */
    pl_priv(p_playlist)->i_last_playlist_id = 0;
    pl_priv(p_playlist)->p_input = NULL;

    pl_priv(p_playlist)->gc_date = 0;
    pl_priv(p_playlist)->b_cant_sleep = false;

    ARRAY_INIT( p_playlist->items );
    ARRAY_INIT( p_playlist->all_items );
    ARRAY_INIT( pl_priv(p_playlist)->items_to_delete );
    ARRAY_INIT( p_playlist->current );

    p_playlist->i_current_index = 0;
    pl_priv(p_playlist)->b_reset_currently_playing = true;
    pl_priv(p_playlist)->last_rebuild_date = 0;

    pl_priv(p_playlist)->b_tree = var_CreateGetBool( p_playlist, "playlist-tree" );

    pl_priv(p_playlist)->b_doing_ml = false;

    pl_priv(p_playlist)->b_auto_preparse =
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
    pl_priv(p_playlist)->status.p_item = NULL;
    pl_priv(p_playlist)->status.p_node = p_playlist->p_local_onelevel;
    pl_priv(p_playlist)->request.b_request = false;
    pl_priv(p_playlist)->status.i_status = PLAYLIST_STOPPED;

    pl_priv(p_playlist)->b_auto_preparse = false;
    playlist_MLLoad( p_playlist );
    pl_priv(p_playlist)->b_auto_preparse = true;

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

    /* Destroy the item preparser */
    playlist_preparse_t *p_preparse = &pl_priv(p_playlist)->preparse;
    if (p_preparse->up)
    {
        vlc_cancel (p_preparse->thread);
        vlc_join (p_preparse->thread, NULL);
    }
    while (p_preparse->i_waiting > 0)
    {   /* Any left-over unparsed item? */
        vlc_gc_decref (p_preparse->pp_waiting[0]);
        REMOVE_ELEM (p_preparse->pp_waiting, p_preparse->i_waiting, 0);
    }
    vlc_cond_destroy (&p_preparse->wait);
    vlc_mutex_destroy (&p_preparse->lock);

    /* Destroy the item meta-infos fetcher */
    playlist_fetcher_t *p_fetcher = &pl_priv(p_playlist)->fetcher;
    if (p_fetcher->up)
    {
        vlc_cancel (p_fetcher->thread);
        vlc_join (p_fetcher->thread, NULL);
    }
    while (p_fetcher->i_waiting > 0)
    {   /* Any left-over unparsed item? */
        vlc_gc_decref (p_fetcher->pp_waiting[0]);
        REMOVE_ELEM (p_fetcher->pp_waiting, p_fetcher->i_waiting, 0);
    }
    vlc_cond_destroy (&p_fetcher->wait);
    vlc_mutex_destroy (&p_fetcher->lock);

    msg_Dbg( p_this, "Destroyed" );
}

/* Destroy remaining objects */
static void ObjectGarbageCollector( playlist_t *p_playlist, bool b_force )
{
    if( !b_force )
    {
        if( mdate() - pl_priv(p_playlist)->gc_date < 1000000 )
        {
           pl_priv(p_playlist)->b_cant_sleep = true;
            return;
        }
        else if( pl_priv(p_playlist)->gc_date == 0 )
            return;
    }

    pl_priv(p_playlist)->b_cant_sleep = false;
}

/* Input Callback */
static int InputEvent( vlc_object_t *p_this, char const *psz_cmd,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    VLC_UNUSED(p_this); VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval);
    playlist_t *p_playlist = p_data;

    if( newval.i_int != INPUT_EVENT_STATE &&
        newval.i_int != INPUT_EVENT_ES )
        return VLC_SUCCESS;

    PL_LOCK;

    if( newval.i_int == INPUT_EVENT_ES )
        pl_priv(p_playlist)->gc_date = mdate();

    vlc_object_signal_unlocked( p_playlist );

    PL_UNLOCK;
    return VLC_SUCCESS;
}

/* Internals */
void playlist_release_current_input( playlist_t * p_playlist )
{
    PL_ASSERT_LOCKED;

    if( !pl_priv(p_playlist)->p_input ) return;

    input_thread_t * p_input = pl_priv(p_playlist)->p_input;

    var_DelCallback( p_input, "intf-event", InputEvent, p_playlist );
    pl_priv(p_playlist)->p_input = NULL;

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
        vlc_object_hold( p_input );
        pl_priv(p_playlist)->p_input = p_input;

        var_AddCallback( p_input, "intf-event", InputEvent, p_playlist );
    }
}

/** Get current playing input.
 */
input_thread_t * playlist_CurrentInput( playlist_t * p_playlist )
{
    input_thread_t * p_input;
    PL_LOCK;
    p_input = pl_priv(p_playlist)->p_input;
    if( p_input ) vlc_object_hold( p_input );
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

    return pl_priv(p_playlist)->status.p_item;
}

playlist_item_t * get_current_status_node( playlist_t * p_playlist )
{
    PL_ASSERT_LOCKED;

    return pl_priv(p_playlist)->status.p_node;
}

void set_current_status_item( playlist_t * p_playlist,
    playlist_item_t * p_item )
{
    PL_ASSERT_LOCKED;

    if( pl_priv(p_playlist)->status.p_item &&
        pl_priv(p_playlist)->status.p_item->i_flags & PLAYLIST_REMOVE_FLAG &&
        pl_priv(p_playlist)->status.p_item != p_item )
    {
        /* It's unsafe given current design to delete a playlist item :(
        playlist_ItemDelete( pl_priv(p_playlist)->status.p_item ); */
    }
    pl_priv(p_playlist)->status.p_item = p_item;
}

void set_current_status_node( playlist_t * p_playlist,
    playlist_item_t * p_node )
{
    PL_ASSERT_LOCKED;

    if( pl_priv(p_playlist)->status.p_node &&
        pl_priv(p_playlist)->status.p_node->i_flags & PLAYLIST_REMOVE_FLAG &&
        pl_priv(p_playlist)->status.p_node != p_node )
    {
        /* It's unsafe given current design to delete a playlist item :(
        playlist_ItemDelete( pl_priv(p_playlist)->status.p_node ); */
    }
    pl_priv(p_playlist)->status.p_node = p_node;
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

    if( pl_priv(p_playlist)->b_reset_currently_playing &&
        mdate() - pl_priv(p_playlist)->last_rebuild_date > 30000 ) // 30 ms
    {
        ResetCurrentlyPlaying( p_playlist, var_GetBool( p_playlist, "random" ),
                               get_current_status_item( p_playlist ) );
        pl_priv(p_playlist)->last_rebuild_date = mdate();
    }

check_input:
    /* If there is an input, check that it doesn't need to die. */
    if( pl_priv(p_playlist)->p_input )
    {
        if( pl_priv(p_playlist)->request.b_request && !pl_priv(p_playlist)->p_input->b_die )
        {
            PL_DEBUG( "incoming request - stopping current input" );
            input_StopThread( pl_priv(p_playlist)->p_input );
        }

        /* This input is dead. Remove it ! */
        if( pl_priv(p_playlist)->p_input->b_dead )
        {
            int i_activity;
            input_thread_t *p_input;
            sout_instance_t **pp_sout = &pl_priv(p_playlist)->p_sout;

            PL_DEBUG( "dead input" );

            p_input = pl_priv(p_playlist)->p_input;

            assert( *pp_sout == NULL );
            if( var_CreateGetBool( p_input, "sout-keep" ) )
                *pp_sout = input_DetachSout( p_input );

            /* Destroy input */
            playlist_release_current_input( p_playlist );

            pl_priv(p_playlist)->gc_date = mdate();
            pl_priv(p_playlist)->b_cant_sleep = true;

            i_activity= var_GetInteger( p_playlist, "activity" );
            var_SetInteger( p_playlist, "activity", i_activity -
                            DEFAULT_INPUT_ACTIVITY );

            goto check_input;
        }
        /* This input is dying, let it do */
        else if( pl_priv(p_playlist)->p_input->b_die )
        {
            PL_DEBUG( "dying input" );
            PL_UNLOCK;
            msleep( INTF_IDLE_SLEEP );
            PL_LOCK;
            goto check_input;
        }
        /* This input has finished, ask it to die ! */
        else if( pl_priv(p_playlist)->p_input->b_error
                  || pl_priv(p_playlist)->p_input->b_eof )
        {
            PL_DEBUG( "finished input" );
            input_StopThread( pl_priv(p_playlist)->p_input );
            /* No need to wait here, we'll wait in the p_input->b_die case */
            goto check_input;
        }
        else if( pl_priv(p_playlist)->p_input->i_state != INIT_S )
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
        int i_status = pl_priv(p_playlist)->request.b_request ?
            pl_priv(p_playlist)->request.i_status : pl_priv(p_playlist)->status.i_status;
        if( i_status != PLAYLIST_STOPPED )
        {
            msg_Dbg( p_playlist, "starting new item" );
            p_item = playlist_NextItem( p_playlist );

            if( p_item == NULL )
            {
                msg_Dbg( p_playlist, "nothing to play" );
                pl_priv(p_playlist)->status.i_status = PLAYLIST_STOPPED;

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
            const bool b_gc_forced = pl_priv(p_playlist)->status.i_status != PLAYLIST_STOPPED;

            pl_priv(p_playlist)->status.i_status = PLAYLIST_STOPPED;

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
        if( pl_priv(p_playlist)->p_input == NULL )
        {
            PL_UNLOCK;
            break;
        }

        if( pl_priv(p_playlist)->p_input->b_dead )
        {
            /* remove input */
            playlist_release_current_input( p_playlist );

            /* sout-keep: no need to anything here.
             * The last input will destroy its sout, if any, by itself */

            PL_UNLOCK;
            continue;
        }
        else if( pl_priv(p_playlist)->p_input->b_die )
        {
            /* This input is dying, leave it alone */
            ;
        }
        else if( pl_priv(p_playlist)->p_input->b_error || pl_priv(p_playlist)->p_input->b_eof )
        {
            input_StopThread( pl_priv(p_playlist)->p_input );
            PL_UNLOCK;
            continue;
        }
        else
        {
            pl_priv(p_playlist)->p_input->b_eof = 1;
        }
        PL_UNLOCK;

        msleep( INTF_IDLE_SLEEP );
    }

#ifdef ENABLE_SOUT
    /* close the remaining sout-keep (if there was no input atm) */
    sout_instance_t *p_sout = pl_priv(p_playlist)->p_sout;
    if (p_sout)
        sout_DeleteInstance( p_sout );
#endif

    /* Core should have terminated all SDs before the playlist */
    /* TODO: It fails to do so when not playing anything -- Courmisch */
    playlist_ServicesDiscoveryKillAll( p_playlist );
    playlist_MLDump( p_playlist );

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
    FOREACH_ARRAY( playlist_item_t *p_del, pl_priv(p_playlist)->items_to_delete )
        free( p_del->pp_children );
        vlc_gc_decref( p_del->p_input );
        free( p_del );
    FOREACH_END();
    ARRAY_RESET( pl_priv(p_playlist)->items_to_delete );

    ARRAY_RESET( p_playlist->items );
    ARRAY_RESET( p_playlist->current );

    PL_UNLOCK;
}

/**
 * Preparse queue loop
 *
 * @param p_obj preparse structure
 * @return never
 */
void *playlist_PreparseLoop( void *data )
{
    playlist_preparse_t *p_preparse = data;
    playlist_t *p_playlist = &((playlist_private_t *)(((char *)p_preparse)
             - offsetof(playlist_private_t, preparse)))->public_data;

    for( ;; )
    {
        input_item_t *p_current;

        vlc_mutex_lock( &p_preparse->lock );
        mutex_cleanup_push( &p_preparse->lock );

        while( p_preparse->i_waiting == 0 )
            vlc_cond_wait( &p_preparse->wait, &p_preparse->lock );

        p_current = p_preparse->pp_waiting[0];
        REMOVE_ELEM( p_preparse->pp_waiting, p_preparse->i_waiting, 0 );
        vlc_cleanup_run( );

        if( p_current )
        {
            int canc = vlc_savecancel ();
            PL_LOCK;
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
            playlist_fetcher_t *p_fetcher = &pl_priv(p_playlist)->fetcher;
            if( p_fetcher->i_art_policy == ALBUM_ART_ALL &&
                ( !psz_arturl || strncmp( psz_arturl, "file://", 7 ) ) )
            {
                PL_DEBUG("meta ok for %s, need to fetch art", psz_name );
                vlc_mutex_lock( &p_fetcher->lock );
                INSERT_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting,
                             p_fetcher->i_waiting, p_current);
                vlc_cond_signal( &p_fetcher->wait );
                vlc_mutex_unlock( &p_fetcher->lock );
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
            vlc_restorecancel( canc );
        }

        int i_activity = var_GetInteger( p_playlist, "activity" );
        if( i_activity < 0 ) i_activity = 0;
        /* Sleep at least 1ms */
        msleep( (i_activity+1) * 1000 );
    }

    assert( 0 );
    return NULL;
}

/**
 * Fetcher loop
 *
 * \return never
 */
void *playlist_FetcherLoop( void *data )
{
    playlist_fetcher_t *p_fetcher = data;
    playlist_t *p_playlist = &((playlist_private_t *)(((char *)p_fetcher)
             - offsetof(playlist_private_t, fetcher)))->public_data;

    for( ;; )
    {
        input_item_t *p_item;

        vlc_mutex_lock( &p_fetcher->lock );
        mutex_cleanup_push( &p_fetcher->lock );

        while( p_fetcher->i_waiting == 0 )
            vlc_cond_wait( &p_fetcher->wait, &p_fetcher->lock );

        p_item = p_fetcher->pp_waiting[0];
        REMOVE_ELEM( p_fetcher->pp_waiting, p_fetcher->i_waiting, 0 );
        vlc_cleanup_run( );

        int canc = vlc_savecancel();
        if( p_item )
        {
            int i_ret;

            /* Check if it is not yet preparsed and if so wait for it
             * (at most 0.5s)
             * (This can happen if we fetch art on play)
             * FIXME this doesn't work if we need to fetch meta before art...
             */
            for( i_ret = 0; i_ret < 10 && !input_item_IsPreparsed( p_item ); i_ret++ )
            {
                bool b_break;
                PL_LOCK;
                b_break = ( !pl_priv(p_playlist)->p_input || input_GetItem(pl_priv(p_playlist)->p_input) != p_item  ||
                            pl_priv(p_playlist)->p_input->b_die || pl_priv(p_playlist)->p_input->b_eof || pl_priv(p_playlist)->p_input->b_error );
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
        vlc_restorecancel( canc );

        int i_activity = var_GetInteger( p_playlist, "activity" );
        if( i_activity < 0 ) i_activity = 0;
        /* Sleep at least 1ms */
        msleep( (i_activity+1) * 1000 );
    }

    assert( 0 );
    return NULL;
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
    var_Create( p_playlist, "play-and-stop", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "play-and-exit", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "random", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "repeat", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "loop", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    var_AddCallback( p_playlist, "random", RandomCallback, NULL );
}

int playlist_CurrentId( playlist_t * p_playlist )
{
    return pl_priv(p_playlist)->status.p_item->i_id;
}

bool playlist_IsPlaying( playlist_t * p_playlist )
{
    return ( pl_priv(p_playlist)->status.i_status == PLAYLIST_RUNNING &&
            !(pl_priv(p_playlist)->request.b_request && pl_priv(p_playlist)->request.i_status == PLAYLIST_STOPPED) );
}

playlist_item_t * playlist_CurrentPlayingItem( playlist_t * p_playlist )
{
    return pl_priv(p_playlist)->status.p_item;
}

int playlist_Status( playlist_t * p_playlist )
{
    return pl_priv(p_playlist)->status.i_status;
}
