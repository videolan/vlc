/*****************************************************************************
 * thread.c : Playlist management functions
 *****************************************************************************
 * Copyright © 1999-2008 the VideoLAN team
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc_es.h>
#include <vlc_input.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include "playlist_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RunControlThread ( playlist_t * );
static void RunPreparse( playlist_preparse_t * );
static void RunFetcher( playlist_fetcher_t * );
static void PreparseDestructor( vlc_object_t * );
static void FetcherDestructor( vlc_object_t * );

/*****************************************************************************
 * Main functions for the global thread
 *****************************************************************************/

/**
 * Create the main playlist thread
 * Additionally to the playlist, this thread controls :
 *    - Statistics
 *    - VLM
 * \param p_parent
 * \return an object with a started thread
 */
void __playlist_ThreadCreate( vlc_object_t *p_parent )
{
    playlist_t *p_playlist = playlist_Create( p_parent );
    if( !p_playlist ) return;

    // Preparse
    p_playlist->p_preparse = vlc_object_create( p_playlist,
                                  sizeof( playlist_preparse_t ) );
    p_playlist->p_preparse->psz_object_name = "preparser";
    if( !p_playlist->p_preparse )
    {
        msg_Err( p_playlist, "unable to create preparser" );
        vlc_object_release( p_playlist );
        return;
    }
    p_playlist->p_preparse->i_waiting = 0;
    p_playlist->p_preparse->pp_waiting = NULL;

    vlc_object_set_destructor( p_playlist->p_preparse, PreparseDestructor );

    vlc_object_attach( p_playlist->p_preparse, p_playlist );
    if( vlc_thread_create( p_playlist->p_preparse, "preparser",
                           RunPreparse, VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
    {
        msg_Err( p_playlist, "cannot spawn preparse thread" );
        vlc_object_release( p_playlist->p_preparse );
        return;
    }

    // Secondary Preparse
    p_playlist->p_fetcher = vlc_object_create( p_playlist,
                              sizeof( playlist_fetcher_t ) );
    p_playlist->p_fetcher->psz_object_name = "fetcher";
    if( !p_playlist->p_fetcher )
    {
        msg_Err( p_playlist, "unable to create secondary preparser" );
        vlc_object_release( p_playlist );
        return;
    }
    p_playlist->p_fetcher->i_waiting = 0;
    p_playlist->p_fetcher->p_waiting = NULL;
    p_playlist->p_fetcher->b_fetch_meta = var_CreateGetInteger( p_playlist,
                                                                 "fetch-meta" );
    p_playlist->p_fetcher->i_art_policy = var_CreateGetInteger( p_playlist,
                                                                "album-art" );

    vlc_object_set_destructor( p_playlist->p_fetcher, FetcherDestructor );

    vlc_object_attach( p_playlist->p_fetcher, p_playlist );
    if( vlc_thread_create( p_playlist->p_fetcher,
                           "fetcher",
                           RunFetcher,
                           VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
    {
        msg_Err( p_playlist, "cannot spawn secondary preparse thread" );
        vlc_object_release( p_playlist->p_fetcher );
        return;
    }

    // Start the thread
    if( vlc_thread_create( p_playlist, "playlist", RunControlThread,
                           VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
    {
        msg_Err( p_playlist, "cannot spawn playlist thread" );
        vlc_object_release( p_playlist );
        return;
    }

    /* The object has been initialized, now attach it */
    vlc_object_attach( p_playlist, p_parent );

    return;
}

/**
 * Destroy the playlist global thread.
 *
 * Deinits all things controlled by the playlist global thread
 * \param p_playlist the playlist thread to destroy
 * \return VLC_SUCCESS or an error
 */
int playlist_ThreadDestroy( playlist_t * p_playlist )
{
    playlist_Destroy( p_playlist );
    return VLC_SUCCESS;
}

/**
 * Run the main control thread itself
 */
static void RunControlThread ( playlist_t *p_playlist )
{
    int i_loops = 0;

    /* Tell above that we're ready */
    vlc_thread_ready( p_playlist );
    while( !p_playlist->b_die )
    {
        i_loops++;

        playlist_MainLoop( p_playlist );
        if( p_playlist->b_cant_sleep )
        {
            /* 100 ms is an acceptable delay for playlist operations */
            msleep( INTF_IDLE_SLEEP*2 );
        }
        else
        {
            PL_LOCK;
            vlc_cond_wait( &p_playlist->object_wait, &p_playlist->object_lock );
            PL_UNLOCK;
        }
    }
    playlist_LastLoop( p_playlist );
}

/*****************************************************************************
 * Preparse-specific functions
 *****************************************************************************/
static void RunPreparse ( playlist_preparse_t *p_obj )
{
    /* Tell above that we're ready */
    vlc_thread_ready( p_obj );
    playlist_PreparseLoop( p_obj );
}

static void RunFetcher( playlist_fetcher_t *p_obj )
{
    /* Tell above that we're ready */
    vlc_thread_ready( p_obj );
    playlist_FetcherLoop( p_obj );
}

static void PreparseDestructor( vlc_object_t * p_this )
{
    playlist_preparse_t * p_preparse = (playlist_preparse_t *)p_this;
    free( p_preparse->pp_waiting );
}

static void FetcherDestructor( vlc_object_t * p_this )
{
    playlist_fetcher_t * p_fetcher = (playlist_fetcher_t *)p_this;
    free( p_fetcher->p_waiting );
}
