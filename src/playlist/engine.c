/*****************************************************************************
 * engine.c : Run the playlist and handle its control
 *****************************************************************************
 * Copyright (C) 1999-2008 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *          Clément Stenac <zorglub@videolan.org>
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
#include "input/resource.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void VariablesInit( playlist_t *p_playlist );

static int RandomCallback( vlc_object_t *p_this, char const *psz_cmd,
                           vlc_value_t oldval, vlc_value_t newval, void *a )
{
    (void)psz_cmd; (void)oldval; (void)newval; (void)a;
    playlist_t *p_playlist = (playlist_t*)p_this;
    bool random = newval.b_bool;

    PL_LOCK;

    if( !random ) {
        pl_priv(p_playlist)->b_reset_currently_playing = true;
        vlc_cond_signal( &pl_priv(p_playlist)->signal );
    } else {
        /* Shuffle and sync the playlist on activation of random mode.
         * This preserves the current playing item, so that the user
         * can return to it if needed. (See #4472)
         */
        playlist_private_t *p_sys = pl_priv(p_playlist);
        playlist_item_t *p_new = p_sys->status.p_item;
        ResetCurrentlyPlaying( p_playlist, NULL );
        if( p_new )
            ResyncCurrentIndex( p_playlist, p_new );
    }

    PL_UNLOCK;
    return VLC_SUCCESS;
}

/**
 * When there are one or more pending corks, playback should be paused.
 * This is used for audio policy.
 * \warning Always add and remove a cork with var_IncInteger() and var_DecInteger().
 * var_Get() and var_Set() are prone to race conditions.
 */
static int CorksCallback( vlc_object_t *obj, char const *var,
                          vlc_value_t old, vlc_value_t cur, void *dummy )
{
    playlist_t *pl = (playlist_t *)obj;

    msg_Dbg( obj, "corks count: %"PRId64" -> %"PRId64, old.i_int, cur.i_int );
    if( !old.i_int == !cur.i_int )
        return VLC_SUCCESS; /* nothing to do */

    if( cur.i_int )
    {
        if( var_InheritBool( obj, "playlist-cork" ) )
        {
            msg_Dbg( obj, "corked" );
            playlist_Pause( pl );
        }
        else
            msg_Dbg( obj, "not corked" );
    }
    else
        msg_Dbg( obj, "uncorked" );

    (void) var; (void) dummy;
    return VLC_SUCCESS;
}

static int RateCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p )
{
    (void)psz_cmd; (void)oldval;(void)p;
    playlist_t *p_playlist = (playlist_t*)p_this;

    PL_LOCK;

    if( pl_priv(p_playlist)->p_input == NULL )
    {
        PL_UNLOCK;
        return VLC_SUCCESS;
    }

    var_SetFloat( pl_priv( p_playlist )->p_input, "rate", newval.f_float );
    PL_UNLOCK;
    return VLC_SUCCESS;
}

static int RateOffsetCallback( vlc_object_t *obj, char const *psz_cmd,
                               vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    playlist_t *p_playlist = (playlist_t *)obj;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data); VLC_UNUSED(newval);

    static const float pf_rate[] = {
        1.0/64, 1.0/32, 1.0/16, 1.0/8, 1.0/4, 1.0/3, 1.0/2, 2.0/3,
        1.0/1,
        3.0/2, 2.0/1, 3.0/1, 4.0/1, 8.0/1, 16.0/1, 32.0/1, 64.0/1,
    };
    const size_t i_rate_count = sizeof(pf_rate)/sizeof(*pf_rate);

    float f_rate;
    struct input_thread_t *input;

    PL_LOCK;
    input = pl_priv( p_playlist )->p_input;
    f_rate = var_GetFloat( input ? (vlc_object_t *)input : obj, "rate" );
    PL_UNLOCK;

    if( !strcmp( psz_cmd, "rate-faster" ) )
    {
        /* compensate for input rounding errors */
        float r = f_rate * 1.1;
        for( size_t i = 0; i < i_rate_count; i++ )
            if( r < pf_rate[i] )
            {
                f_rate = pf_rate[i];
                break;
            }
    }
    else
    {
        /* compensate for input rounding errors */
        float r = f_rate * .9;
        for( size_t i = 1; i < i_rate_count; i++ )
            if( r <= pf_rate[i] )
            {
                f_rate = pf_rate[i - 1];
                break;
            }
    }

    var_SetFloat( p_playlist, "rate", f_rate );
    return VLC_SUCCESS;
}

static int VideoSplitterCallback( vlc_object_t *p_this, char const *psz_cmd,
                                  vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    playlist_t *p_playlist = (playlist_t*)p_this;
    VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(p_data); VLC_UNUSED(newval);

    PL_LOCK;

    /* Force the input to restart the video ES to force a vout recreation */
    input_thread_t *p_input = pl_priv( p_playlist )->p_input;
    if( p_input )
    {
        const double f_position = var_GetFloat( p_input, "position" );
        input_Control( p_input, INPUT_RESTART_ES, -VIDEO_ES );
        var_SetFloat( p_input, "position", f_position );
    }

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
static playlist_t *playlist_Create( vlc_object_t *p_parent )
{
    playlist_t *p_playlist;
    playlist_private_t *p;

    /* Allocate structure */
    p = vlc_custom_create( p_parent, sizeof( *p ), "playlist" );
    if( !p )
        return NULL;

    assert( offsetof( playlist_private_t, public_data ) == 0 );
    p_playlist = &p->public_data;
    TAB_INIT( pl_priv(p_playlist)->i_sds, pl_priv(p_playlist)->pp_sds );

    VariablesInit( p_playlist );
    vlc_mutex_init( &p->lock );
    vlc_cond_init( &p->signal );
    p->killed = false;

    /* Initialise data structures */
    pl_priv(p_playlist)->i_last_playlist_id = 0;
    pl_priv(p_playlist)->p_input = NULL;

    ARRAY_INIT( p_playlist->items );
    ARRAY_INIT( p_playlist->all_items );
    ARRAY_INIT( pl_priv(p_playlist)->items_to_delete );
    ARRAY_INIT( p_playlist->current );

    p_playlist->i_current_index = 0;
    pl_priv(p_playlist)->b_reset_currently_playing = true;

    pl_priv(p_playlist)->b_tree = var_InheritBool( p_parent, "playlist-tree" );

    pl_priv(p_playlist)->b_doing_ml = false;

    pl_priv(p_playlist)->b_auto_preparse =
        var_InheritBool( p_parent, "auto-preparse" );

    /* Fetcher */
    p->p_fetcher = playlist_fetcher_New( VLC_OBJECT(p_playlist) );
    if( unlikely(p->p_fetcher == NULL) )
        msg_Err( p_playlist, "cannot create fetcher" );
   /* Preparser */
   p->p_preparser = playlist_preparser_New( VLC_OBJECT(p_playlist),
                                            p->p_fetcher );
   if( unlikely(p->p_preparser == NULL) )
       msg_Err( p_playlist, "cannot create preparser" );

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

    /* Input resources */
    p->p_input_resource = input_resource_New( VLC_OBJECT( p_playlist ) );
    if( unlikely(p->p_input_resource == NULL) )
        abort();

    /* Audio output (needed for volume and device controls). */
    audio_output_t *aout = input_resource_GetAout( p->p_input_resource );
    if( aout != NULL )
        input_resource_PutAout( p->p_input_resource, aout );

    /* Thread */
    playlist_Activate (p_playlist);

    /* Add service discovery modules */
    char *mods = var_InheritString( p_playlist, "services-discovery" );
    if( mods != NULL )
    {
        char *p = mods, *m;
        while( (m = strsep( &p, " :," )) != NULL )
            playlist_ServicesDiscoveryAdd( p_playlist, m );
        free( mods );
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

    /* Remove all services discovery */
    playlist_ServicesDiscoveryKillAll( p_playlist );

    msg_Dbg( p_playlist, "destroying" );

    playlist_Deactivate( p_playlist );
    if( p_sys->p_preparser )
        playlist_preparser_Delete( p_sys->p_preparser );
    if( p_sys->p_fetcher )
        playlist_fetcher_Delete( p_sys->p_fetcher );

    /* Release input resources */
    assert( p_sys->p_input == NULL );
    input_resource_Release( p_sys->p_input_resource );

    if( p_playlist->p_media_library != NULL )
        playlist_MLDump( p_playlist );

    PL_LOCK;
    /* Release the current node */
    set_current_status_node( p_playlist, NULL );
    /* Release the current item */
    set_current_status_item( p_playlist, NULL );
    PL_UNLOCK;

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

#undef pl_Get
playlist_t *pl_Get (vlc_object_t *obj)
{
    static vlc_mutex_t lock = VLC_STATIC_MUTEX;
    libvlc_int_t *p_libvlc = obj->p_libvlc;
    playlist_t *pl;

    vlc_mutex_lock (&lock);
    pl = libvlc_priv (p_libvlc)->p_playlist;
    if (unlikely(pl == NULL))
    {
        pl = playlist_Create (VLC_OBJECT(p_libvlc));
        if (unlikely(pl == NULL))
            abort();
        libvlc_priv (p_libvlc)->p_playlist = pl;
    }
    vlc_mutex_unlock (&lock);
    return pl;
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

static void VariablesInit( playlist_t *p_playlist )
{
    /* These variables control updates */
    var_Create( p_playlist, "intf-change", VLC_VAR_BOOL );
    var_SetBool( p_playlist, "intf-change", true );

    var_Create( p_playlist, "item-change", VLC_VAR_ADDRESS );
    var_Create( p_playlist, "leaf-to-parent", VLC_VAR_INTEGER );

    var_Create( p_playlist, "playlist-item-deleted", VLC_VAR_INTEGER );
    var_SetInteger( p_playlist, "playlist-item-deleted", -1 );

    var_Create( p_playlist, "playlist-item-append", VLC_VAR_ADDRESS );

    var_Create( p_playlist, "input-current", VLC_VAR_ADDRESS );

    var_Create( p_playlist, "activity", VLC_VAR_VOID );

    /* Variables to control playback */
    var_Create( p_playlist, "playlist-autostart", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "play-and-stop", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "play-and-exit", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "random", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_AddCallback( p_playlist, "random", RandomCallback, NULL );
    var_Create( p_playlist, "repeat", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "loop", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "corks", VLC_VAR_INTEGER );
    var_AddCallback( p_playlist, "corks", CorksCallback, NULL );

    var_Create( p_playlist, "rate", VLC_VAR_FLOAT | VLC_VAR_DOINHERIT );
    var_AddCallback( p_playlist, "rate", RateCallback, NULL );
    var_Create( p_playlist, "rate-slower", VLC_VAR_VOID );
    var_AddCallback( p_playlist, "rate-slower", RateOffsetCallback, NULL );
    var_Create( p_playlist, "rate-faster", VLC_VAR_VOID );
    var_AddCallback( p_playlist, "rate-faster", RateOffsetCallback, NULL );

    var_Create( p_playlist, "video-splitter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_AddCallback( p_playlist, "video-splitter", VideoSplitterCallback, NULL );

    /* */
    var_Create( p_playlist, "album-art", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );

    /* Variables to preserve video output parameters */
    var_Create( p_playlist, "fullscreen", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "video-on-top", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Audio output parameters */
    var_Create( p_playlist, "mute", VLC_VAR_BOOL );
    var_Create( p_playlist, "volume", VLC_VAR_FLOAT );
    var_SetFloat( p_playlist, "volume", -1.f );
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

