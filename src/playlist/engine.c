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

static int RandomCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval, void *a )
{
    (void)psz_cmd; (void)oldval; (void)newval; (void)a;
    playlist_t *p_playlist = (playlist_t*)p_this;

    PL_LOCK;

    pl_priv(p_playlist)->b_reset_currently_playing = true;
    vlc_cond_signal( &pl_priv(p_playlist)->signal );

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
    vlc_object_attach( p_playlist, p_parent );
    TAB_INIT( pl_priv(p_playlist)->i_sds, pl_priv(p_playlist)->pp_sds );

    libvlc_priv(p_parent->p_libvlc)->p_playlist = p_playlist;

    VariablesInit( p_playlist );
    vlc_mutex_init( &p->lock );
    vlc_cond_init( &p->signal );

    /* Initialise data structures */
    pl_priv(p_playlist)->i_last_playlist_id = 0;
    pl_priv(p_playlist)->p_input = NULL;

    ARRAY_INIT( p_playlist->items );
    ARRAY_INIT( p_playlist->all_items );
    ARRAY_INIT( pl_priv(p_playlist)->items_to_delete );
    ARRAY_INIT( p_playlist->current );

    p_playlist->i_current_index = 0;
    pl_priv(p_playlist)->b_reset_currently_playing = true;
    pl_priv(p_playlist)->last_rebuild_date = 0;

    pl_priv(p_playlist)->b_tree = var_InheritBool( p_parent, "playlist-tree" );

    pl_priv(p_playlist)->b_doing_ml = false;

    pl_priv(p_playlist)->b_auto_preparse =
        var_InheritBool( p_parent, "auto-preparse" );

    /* Fetcher */
    p->p_fetcher = playlist_fetcher_New( p_playlist );
    if( unlikely(p->p_fetcher == NULL) )
    {
        msg_Err( p_playlist, "cannot create fetcher" );
        p->p_preparser = NULL;
    }
    else
    {   /* Preparse */
        p->p_preparser = playlist_preparser_New( p_playlist, p->p_fetcher );
        if( unlikely(p->p_preparser == NULL) )
            msg_Err( p_playlist, "cannot create preparser" );
    }

    /* Create the root node */
    PL_LOCK;
    p_playlist->p_root = playlist_NodeCreate( p_playlist, NULL, NULL,
                                    PLAYLIST_END, 0, NULL );
    PL_UNLOCK;
    if( !p_playlist->p_root ) return NULL;

    /* Create currently playing items node */
    PL_LOCK;
    p_playlist->p_playing = playlist_NodeCreate(
        p_playlist, _( "Playlist" ), p_playlist->p_root,
        PLAYLIST_END, PLAYLIST_RO_FLAG, NULL );

    PL_UNLOCK;

    if( !p_playlist->p_playing ) return NULL;

    /* Create media library node */
    const bool b_ml = var_InheritBool( p_parent, "media-library");
    if( b_ml )
    {
        PL_LOCK;
        p_playlist->p_media_library = playlist_NodeCreate(
            p_playlist, _( "Media Library" ), p_playlist->p_root,
            PLAYLIST_END, PLAYLIST_RO_FLAG, NULL );
        PL_UNLOCK;

        if(!p_playlist->p_media_library ) return NULL;
    }
    else
    {
        p_playlist->p_media_library = NULL;
    }

    p_playlist->p_root_category = p_playlist->p_root;
    p_playlist->p_root_onelevel = p_playlist->p_root;
    p_playlist->p_local_category = p_playlist->p_playing;
    p_playlist->p_local_onelevel = p_playlist->p_playing;
    p_playlist->p_ml_category = p_playlist->p_media_library;
    p_playlist->p_ml_onelevel = p_playlist->p_media_library;;

    /* Initial status */
    pl_priv(p_playlist)->status.p_item = NULL;
    pl_priv(p_playlist)->status.p_node = p_playlist->p_playing;
    pl_priv(p_playlist)->request.b_request = false;
    pl_priv(p_playlist)->status.i_status = PLAYLIST_STOPPED;

    if(b_ml)
    {
        const bool b_auto_preparse = pl_priv(p_playlist)->b_auto_preparse;
        pl_priv(p_playlist)->b_auto_preparse = false;
        playlist_MLLoad( p_playlist );
        pl_priv(p_playlist)->b_auto_preparse = b_auto_preparse;
    }

    return p_playlist;
}

/**
 * Destroy playlist.
 * This is not thread-safe. Any reference to the playlist is assumed gone.
 * (In particular, all interface and services threads must have been joined).
 *
 * \param p_playlist the playlist object
 */
void playlist_Destroy( playlist_t *p_playlist )
{
    playlist_private_t *p_sys = pl_priv(p_playlist);

    msg_Dbg( p_playlist, "destroying" );
    if( p_sys->p_preparser )
        playlist_preparser_Delete( p_sys->p_preparser );
    if( p_sys->p_fetcher )
        playlist_fetcher_Delete( p_sys->p_fetcher );

    /* Already cleared when deactivating (if activated anyway) */
    assert( !p_sys->p_input );
    assert( !p_sys->p_input_resource );

    vlc_cond_destroy( &p_sys->signal );
    vlc_mutex_destroy( &p_sys->lock );

    /* Remove all remaining items */
    FOREACH_ARRAY( playlist_item_t *p_del, p_playlist->all_items )
        free( p_del->pp_children );
        vlc_gc_decref( p_del->p_input );
        free( p_del );
    FOREACH_END();
    ARRAY_RESET( p_playlist->all_items );
    FOREACH_ARRAY( playlist_item_t *p_del, p_sys->items_to_delete )
        free( p_del->pp_children );
        vlc_gc_decref( p_del->p_input );
        free( p_del );
    FOREACH_END();
    ARRAY_RESET( p_sys->items_to_delete );

    ARRAY_RESET( p_playlist->items );
    ARRAY_RESET( p_playlist->current );

    vlc_object_release( p_playlist );
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

static input_thread_t *playlist_FindInput( vlc_object_t *object )
{
    assert( object == VLC_OBJECT(pl_Get(object)) );
    return playlist_CurrentInput( (playlist_t *)object );
}

static void VariablesInit( playlist_t *p_playlist )
{
    /* These variables control updates */
    var_Create( p_playlist, "intf-change", VLC_VAR_BOOL );
    var_SetBool( p_playlist, "intf-change", true );

    var_Create( p_playlist, "item-change", VLC_VAR_ADDRESS );
    var_Create( p_playlist, "leaf-to-parent", VLC_VAR_ADDRESS );

    var_Create( p_playlist, "playlist-item-deleted", VLC_VAR_INTEGER );
    var_SetInteger( p_playlist, "playlist-item-deleted", -1 );

    var_Create( p_playlist, "playlist-item-append", VLC_VAR_ADDRESS );

    var_Create( p_playlist, "item-current", VLC_VAR_ADDRESS );
    var_Create( p_playlist, "input-current", VLC_VAR_ADDRESS );

    var_Create( p_playlist, "activity", VLC_VAR_INTEGER );
    var_SetInteger( p_playlist, "activity", 0 );

    /* Variables to control playback */
    var_Create( p_playlist, "play-and-stop", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "play-and-exit", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "random", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "repeat", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "loop", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    var_AddCallback( p_playlist, "random", RandomCallback, NULL );

    /* */
    var_Create( p_playlist, "album-art", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* Variables to preserve video output parameters */
    var_Create( p_playlist, "fullscreen", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "video-on-top", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Audio output parameters */
    var_Create( p_playlist, "volume-muted", VLC_VAR_BOOL );
    var_Create( p_playlist, "saved-volume", VLC_VAR_INTEGER );
    var_Create( p_playlist, "volume-change", VLC_VAR_VOID );
    /* FIXME: horrible hack for audio output interface code */
    var_Create( p_playlist, "find-input-callback", VLC_VAR_ADDRESS );
    var_SetAddress( p_playlist, "find-input-callback", playlist_FindInput );
}

playlist_item_t * playlist_CurrentPlayingItem( playlist_t * p_playlist )
{
    PL_ASSERT_LOCKED;

    return pl_priv(p_playlist)->status.p_item;
}

int playlist_Status( playlist_t * p_playlist )
{
    PL_ASSERT_LOCKED;

    return pl_priv(p_playlist)->status.i_status;
}

