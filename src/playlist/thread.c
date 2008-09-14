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

#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_input.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include "playlist_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void* RunControlThread   ( vlc_object_t * );

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
    playlist_preparse_t *p_preparse = &pl_priv(p_playlist)->preparse;
    vlc_mutex_init (&p_preparse->lock);
    vlc_cond_init (&p_preparse->wait);
    p_preparse->i_waiting = 0;
    p_preparse->pp_waiting = NULL;

    if( vlc_clone( &p_preparse->thread, playlist_PreparseLoop, p_preparse,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_playlist, "cannot spawn preparse thread" );
        p_preparse->up = false;
        return;
    }
    p_preparse->up = true;

    // Secondary Preparse
    playlist_fetcher_t *p_fetcher = &pl_priv(p_playlist)->fetcher;
    vlc_mutex_init (&p_fetcher->lock);
    vlc_cond_init (&p_fetcher->wait);
    p_fetcher->i_waiting = 0;
    p_fetcher->pp_waiting = NULL;
    p_fetcher->i_art_policy = var_CreateGetInteger( p_playlist, "album-art" );

    if( vlc_clone( &p_fetcher->thread, playlist_FetcherLoop, p_fetcher,
                   VLC_THREAD_PRIORITY_LOW ) )
    {
        msg_Err( p_playlist, "cannot spawn secondary preparse thread" );
        p_fetcher->up = false;
        return;
    }
    p_fetcher->up = true;

    // Start the thread
    if( vlc_thread_create( p_playlist, "playlist", RunControlThread,
                           VLC_THREAD_PRIORITY_LOW, false ) )
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
 * Run the main control thread itself
 */
static void* RunControlThread ( vlc_object_t *p_this )
{
    playlist_t *p_playlist = (playlist_t*)p_this;

    int canc = vlc_savecancel ();
    vlc_object_lock( p_playlist );
    while( vlc_object_alive( p_playlist ) )
    {
        playlist_MainLoop( p_playlist );

        /* The playlist lock has been unlocked, so we can't tell if
         * someone has killed us in the meantime. Check now. */
        if( !vlc_object_alive( p_playlist ) )
            break;

        if( p_playlist->b_cant_sleep )
        {
            /* 100 ms is an acceptable delay for playlist operations */
            vlc_object_unlock( p_playlist );

            msleep( INTF_IDLE_SLEEP*2 );

            vlc_object_lock( p_playlist );
        }
        else
        {
            vlc_object_wait( p_playlist );
        }
    }
    vlc_object_unlock( p_playlist );

    playlist_LastLoop( p_playlist );
    vlc_restorecancel (canc);
    return NULL;
}
