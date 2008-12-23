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
#include "stream_output/stream_output.h"
#include "playlist_internal.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void* RunControlThread   ( vlc_object_t * );

/*****************************************************************************
 * Main functions for the global thread
 *****************************************************************************/

/**
 * Create the main playlist threads.
 * Additionally to the playlist, this thread controls :
 *    - Statistics
 *    - VLM
 * \param p_parent
 * \return an object with a started thread
 */
void playlist_Activate( playlist_t *p_playlist )
{
    /* */
    playlist_private_t *p_sys = pl_priv(p_playlist);

    /* Fetcher */
    p_sys->p_fetcher = playlist_fetcher_New( p_playlist );
    if( !p_sys->p_fetcher )
        msg_Err( p_playlist, "cannot create playlist fetcher" );

    /* Preparse */
    p_sys->p_preparser = playlist_preparser_New( p_playlist, p_sys->p_fetcher );
    if( !p_sys->p_preparser )
        msg_Err( p_playlist, "cannot create playlist preparser" );

    /* Start the playlist thread */
    if( vlc_thread_create( p_playlist, "playlist", RunControlThread,
                           VLC_THREAD_PRIORITY_LOW, false ) )
    {
        msg_Err( p_playlist, "cannot spawn playlist thread" );
    }
    msg_Err( p_playlist, "Activated" );
}

void playlist_Deactivate( playlist_t *p_playlist )
{
    /* */
    playlist_private_t *p_sys = pl_priv(p_playlist);

    msg_Err( p_playlist, "Deactivate" );
    vlc_object_kill( p_playlist );
    vlc_thread_join( p_playlist );

    if( p_sys->p_preparser )
        playlist_preparser_Delete( p_sys->p_preparser );
    if( p_sys->p_fetcher )
        playlist_fetcher_Delete( p_sys->p_fetcher );

    /* close the remaining sout-keep */
    if( p_sys->p_sout )
        sout_DeleteInstance( p_sys->p_sout );

    /* */
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

    /* The NULL are there only to assert in playlist destructor */
    p_sys->p_sout = NULL;
    p_sys->p_preparser = NULL;
    p_sys->p_fetcher = NULL;
    msg_Err( p_playlist, "Deactivated" );
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
        if( !vlc_object_alive( p_playlist ) && !pl_priv(p_playlist)->p_input )
            break;

        vlc_object_wait( p_playlist );
    }
    vlc_object_unlock( p_playlist );

    vlc_restorecancel (canc);
    return NULL;
}

