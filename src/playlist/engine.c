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
#include <vlc_arrays.h>
#include <vlc_sout.h>
#include <vlc_playlist.h>
#include <vlc_interface.h>
#include <vlc_http.h>
#include <vlc_renderer_discovery.h>
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

    if( !var_InheritBool( obj, "playlist-cork" ) )
        return VLC_SUCCESS;

    if( cur.i_int )
    {
        msg_Dbg( obj, "corked" );
        playlist_Pause( pl );
    }
    else
    {
        msg_Dbg( obj, "uncorked" );
        playlist_Resume( pl );
    }

    (void) var; (void) dummy;
    return VLC_SUCCESS;
}

static int RateCallback( vlc_object_t *p_this, char const *psz_cmd,
                         vlc_value_t oldval, vlc_value_t newval, void *p )
{
    (void)psz_cmd; (void)oldval;(void)p;
    playlist_t *p_playlist = (playlist_t*)p_this;

    PL_LOCK;

    if( pl_priv(p_playlist)->p_input )
        var_SetFloat( pl_priv( p_playlist )->p_input, "rate", newval.f_float );

    PL_UNLOCK;
    return VLC_SUCCESS;
}

static int RateOffsetCallback( vlc_object_t *obj, char const *psz_cmd,
                               vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    playlist_t *p_playlist = (playlist_t *)obj;
    VLC_UNUSED(oldval); VLC_UNUSED(p_data); VLC_UNUSED(newval);

    static const float rates[] = {
        1.0/64, 1.0/32, 1.0/16, 1.0/8, 1.0/4, 1.0/3, 1.0/2, 2.0/3,
        1.0/1,
        3.0/2, 2.0/1, 3.0/1, 4.0/1, 8.0/1, 16.0/1, 32.0/1, 64.0/1,
    };

    PL_LOCK;
    input_thread_t *input = pl_priv( p_playlist )->p_input;
    float current_rate = var_GetFloat( input ? VLC_OBJECT( input ) : obj, "rate" );
    PL_UNLOCK;

    const bool faster = !strcmp( psz_cmd, "rate-faster" );
    float rate = current_rate * ( faster ? 1.1f : 0.9f );

    /* find closest rate (if any) in the desired direction */
    for( size_t i = 0; i < ARRAY_SIZE( rates ); ++i )
    {
        if( ( faster && rates[i] > rate ) ||
            (!faster && rates[i] >= rate && i ) )
        {
            rate = faster ? rates[i] : rates[i-1];
            break;
        }
    }

    msg_Dbg( p_playlist, "adjusting rate from %f to %f (%s)",
        current_rate, rate, faster ? "faster" : "slower" );

    return var_SetFloat( p_playlist, "rate", rate );
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
playlist_t *playlist_Create( vlc_object_t *p_parent )
{
    playlist_t *p_playlist;
    playlist_private_t *p;

    /* Allocate structure */
    p = vlc_custom_create( p_parent, sizeof( *p ), "playlist" );
    if( !p )
        return NULL;

    p_playlist = &p->public_data;

    p->input_tree = NULL;
    p->id_tree = NULL;

    TAB_INIT( pl_priv(p_playlist)->i_sds, pl_priv(p_playlist)->pp_sds );

    VariablesInit( p_playlist );
    vlc_mutex_init( &p->lock );
    vlc_cond_init( &p->signal );
    p->killed = false;

    /* Initialise data structures */
    pl_priv(p_playlist)->i_last_playlist_id = 0;
    pl_priv(p_playlist)->p_input = NULL;

    ARRAY_INIT( p_playlist->items );
    ARRAY_INIT( p_playlist->current );

    p_playlist->i_current_index = 0;
    pl_priv(p_playlist)->b_reset_currently_playing = true;

    pl_priv(p_playlist)->b_tree = var_InheritBool( p_parent, "playlist-tree" );
    pl_priv(p_playlist)->b_preparse = var_InheritBool( p_parent, "auto-preparse" );

    p_playlist->root.p_input = NULL;
    p_playlist->root.pp_children = NULL;
    p_playlist->root.i_children = 0;
    p_playlist->root.i_nb_played = 0;
    p_playlist->root.i_id = 0;
    p_playlist->root.i_flags = 0;

    /* Create the root, playing items and meida library nodes */
    playlist_item_t *playing, *ml;

    PL_LOCK;
    playing = playlist_NodeCreate( p_playlist, _( "Playlist" ),
                                   &p_playlist->root, PLAYLIST_END,
                                   PLAYLIST_RO_FLAG|PLAYLIST_NO_INHERIT_FLAG );
    if( var_InheritBool( p_parent, "media-library") )
        ml = playlist_NodeCreate( p_playlist, _( "Media Library" ),
                                  &p_playlist->root, PLAYLIST_END,
                                  PLAYLIST_RO_FLAG|PLAYLIST_NO_INHERIT_FLAG );
    else
        ml = NULL;
    PL_UNLOCK;

    if( unlikely(playing == NULL) )
        abort();

    p_playlist->p_playing = playing;
    p_playlist->p_media_library = ml;

    /* Initial status */
    pl_priv(p_playlist)->status.p_item = NULL;
    pl_priv(p_playlist)->status.p_node = p_playlist->p_playing;
    pl_priv(p_playlist)->request.b_request = false;
    p->request.input_dead = false;

    if (ml != NULL)
        playlist_MLLoad( p_playlist );

    /* Input resources */
    p->p_input_resource = input_resource_New( VLC_OBJECT( p_playlist ) );
    if( unlikely(p->p_input_resource == NULL) )
        abort();

    /* Audio output (needed for volume and device controls). */
    audio_output_t *aout = input_resource_GetAout( p->p_input_resource );
    if( aout != NULL )
        input_resource_PutAout( p->p_input_resource, aout );

    /* Initialize the shared HTTP cookie jar */
    vlc_value_t cookies;
    cookies.p_address = vlc_http_cookies_new();
    if ( likely(cookies.p_address) )
    {
        var_Create( p_playlist, "http-cookies", VLC_VAR_ADDRESS );
        var_SetChecked( p_playlist, "http-cookies", VLC_VAR_ADDRESS, cookies );
    }

    /* Thread */
    playlist_Activate (p_playlist);

    /* Add service discovery modules */
    char *mods = var_InheritString( p_playlist, "services-discovery" );
    if( mods != NULL )
    {
        char *s = mods, *m;
        while( (m = strsep( &s, " :," )) != NULL )
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

    /* Release input resources */
    assert( p_sys->p_input == NULL );
    input_resource_Release( p_sys->p_input_resource );
    if( p_sys->p_renderer )
        vlc_renderer_item_release( p_sys->p_renderer );

    if( p_playlist->p_media_library != NULL )
        playlist_MLDump( p_playlist );

    PL_LOCK;
    /* Release the current node */
    set_current_status_node( p_playlist, NULL );
    /* Release the current item */
    set_current_status_item( p_playlist, NULL );

    /* Destroy arrays completely - faster than one item at a time */
    ARRAY_RESET( p_playlist->items );
    ARRAY_RESET( p_playlist->current );

    /* Remove all remaining items */
    if( p_playlist->p_media_library != NULL )
    {
        playlist_NodeDeleteExplicit( p_playlist, p_playlist->p_media_library,
            PLAYLIST_DELETE_FORCE );
    }

    playlist_NodeDeleteExplicit( p_playlist, p_playlist->p_playing,
        PLAYLIST_DELETE_FORCE );

    assert( p_playlist->root.i_children <= 0 );
    PL_UNLOCK;

    vlc_cond_destroy( &p_sys->signal );
    vlc_mutex_destroy( &p_sys->lock );

    vlc_http_cookie_jar_t *cookies = var_GetAddress( p_playlist, "http-cookies" );
    if ( cookies )
    {
        var_Destroy( p_playlist, "http-cookies" );
        vlc_http_cookies_destroy( cookies );
    }

    vlc_object_release( p_playlist );
}

/** Get current playing input.
 */
input_thread_t *playlist_CurrentInputLocked( playlist_t *p_playlist )
{
    PL_ASSERT_LOCKED;

    input_thread_t *p_input = pl_priv(p_playlist)->p_input;
    if( p_input != NULL )
        vlc_object_hold( p_input );
    return p_input;
}


/** Get current playing input.
 */
input_thread_t * playlist_CurrentInput( playlist_t * p_playlist )
{
    input_thread_t * p_input;
    PL_LOCK;
    p_input = playlist_CurrentInputLocked( p_playlist );
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

    pl_priv(p_playlist)->status.p_item = p_item;
}

void set_current_status_node( playlist_t * p_playlist,
    playlist_item_t * p_node )
{
    PL_ASSERT_LOCKED;

    pl_priv(p_playlist)->status.p_node = p_node;
}

static void VariablesInit( playlist_t *p_playlist )
{
    /* These variables control updates */
    var_Create( p_playlist, "item-change", VLC_VAR_ADDRESS );
    var_Create( p_playlist, "leaf-to-parent", VLC_VAR_INTEGER );

    var_Create( p_playlist, "playlist-item-append", VLC_VAR_ADDRESS );
    var_Create( p_playlist, "playlist-item-deleted", VLC_VAR_ADDRESS );

    var_Create( p_playlist, "input-current", VLC_VAR_ADDRESS );

    /* Variables to control playback */
    var_Create( p_playlist, "playlist-autostart", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
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

    var_Create( p_playlist, "video-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "sub-source", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "sub-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    /* sout variables */
    var_Create( p_playlist, "sout", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "demux-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );

    /* */
    var_Create( p_playlist, "metadata-network-access", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Variables to preserve video output parameters */
    var_Create( p_playlist, "fullscreen", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "video-on-top", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "video-wallpaper", VLC_VAR_BOOL | VLC_VAR_DOINHERIT );

    /* Audio output parameters */
    var_Create( p_playlist, "audio-filter", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Create( p_playlist, "audio-device", VLC_VAR_STRING );
    var_Create( p_playlist, "mute", VLC_VAR_BOOL );
    var_Create( p_playlist, "volume", VLC_VAR_FLOAT );
    var_SetFloat( p_playlist, "volume", -1.f );

    var_Create( p_playlist, "sub-text-scale",
               VLC_VAR_INTEGER | VLC_VAR_DOINHERIT | VLC_VAR_ISCOMMAND );
}

playlist_item_t * playlist_CurrentPlayingItem( playlist_t * p_playlist )
{
    PL_ASSERT_LOCKED;

    return pl_priv(p_playlist)->status.p_item;
}

int playlist_Status( playlist_t * p_playlist )
{
    input_thread_t *p_input = pl_priv(p_playlist)->p_input;

    PL_ASSERT_LOCKED;

    if( p_input == NULL )
        return PLAYLIST_STOPPED;
    if( var_GetInteger( p_input, "state" ) == PAUSE_S )
        return PLAYLIST_PAUSED;
    return PLAYLIST_RUNNING;
}

