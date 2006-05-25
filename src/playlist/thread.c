/*****************************************************************************
 * playlist.c : Playlist management functions
 *****************************************************************************
 * Copyright (C) 1999-2004 the VideoLAN team
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
#include <vlc_es.h>
#include <vlc_input.h>
#include "vlc_playlist.h"
#include "vlc_interaction.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RunControlThread ( playlist_t * );
static void RunPreparse( playlist_preparse_t * );

static playlist_t * CreatePlaylist( vlc_object_t *p_parent );
static void HandlePlaylist( playlist_t * );
static void EndPlaylist( playlist_t * );
static void DestroyPlaylist( playlist_t * );

static void HandleStats( playlist_t *, int );

static void HandleInteraction( playlist_t * );
static void DestroyInteraction( playlist_t * );

/*****************************************************************************
 * Main functions for the global thread
 *****************************************************************************/

/**
 * Create the main playlist thread
 * Additionally to the playlist, this thread controls :
 *    - Interaction
 *    - Statistics
 *    - VLM
 * \param p_parent
 * \return an object with a started thread
 */
playlist_t * __playlist_ThreadCreate( vlc_object_t *p_parent )
{
    playlist_t *p_playlist;
    p_playlist = CreatePlaylist( p_parent );

    if( !p_playlist ) return NULL;

    // Stats
    p_playlist->p_stats = (global_stats_t *)malloc( sizeof( global_stats_t ) );
    vlc_mutex_init( p_playlist, &p_playlist->p_stats->lock );

    // Interaction
    p_playlist->p_interaction = NULL;

    // Preparse
    p_playlist->p_preparse = vlc_object_create( p_playlist,
                                  sizeof( playlist_preparse_t ) );
    if( !p_playlist->p_preparse )
    {
        msg_Err( p_playlist, "unable to create preparser" );
        vlc_object_destroy( p_playlist );
        return NULL;
    }
    p_playlist->p_preparse->i_waiting = 0;
    p_playlist->p_preparse->pp_waiting = NULL;

    vlc_object_attach( p_playlist->p_preparse, p_playlist );
    if( vlc_thread_create( p_playlist->p_preparse, "preparser",
                           RunPreparse, VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
    {
        msg_Err( p_playlist, "cannot spawn preparse thread" );
        vlc_object_detach( p_playlist->p_preparse );
        vlc_object_destroy( p_playlist->p_preparse );
        return NULL;
    }

    // Start the thread
    if( vlc_thread_create( p_playlist, "playlist", RunControlThread,
                           VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
    {
        msg_Err( p_playlist, "cannot spawn playlist thread" );
        vlc_object_destroy( p_playlist );
        return NULL;
    }

    /* The object has been initialized, now attach it */
    vlc_object_attach( p_playlist, p_parent );

    return p_playlist;
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
    p_playlist->b_die = 1;

    DestroyInteraction( p_playlist );
    DestroyPlaylist( p_playlist );

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

        HandleInteraction( p_playlist );
        HandleStats( p_playlist, i_loops );

        HandlePlaylist( p_playlist );

        msleep( INTF_IDLE_SLEEP / 2 );

        /* Stop sleeping earlier if we have work */
        /* TODO : statistics about this */
        if ( p_playlist->request.b_request &&
                        p_playlist->status.i_status == PLAYLIST_RUNNING )
        {
            continue;
        }

        msleep( INTF_IDLE_SLEEP / 2 );
    }

    EndPlaylist( p_playlist );
}


/*****************************************************************************
 * Playlist-specific functions
 *****************************************************************************/
static playlist_t * CreatePlaylist( vlc_object_t *p_parent )
{
    return playlist_Create( p_parent );
}

static void DestroyPlaylist( playlist_t *p_playlist )
{
    playlist_Destroy( p_playlist );
}

static void HandlePlaylist( playlist_t *p_playlist )
{
    playlist_MainLoop( p_playlist );
}

static void EndPlaylist( playlist_t *p_playlist )
{
    playlist_LastLoop( p_playlist );
}

/*****************************************************************************
 * Preparse-specific functions
 *****************************************************************************/
static void RunPreparse ( playlist_preparse_t *p_obj )
{
    playlist_t *p_playlist = p_obj->p_parent;
    /* Tell above that we're ready */
    vlc_thread_ready( p_obj );

    while( !p_playlist->b_die )
    {
        playlist_PreparseLoop(  p_obj );
        if( p_obj->i_waiting == 0 )
        {
            msleep( INTF_IDLE_SLEEP );
        }
    }
}

/*****************************************************************************
 * Interaction functions
 *****************************************************************************/
static void DestroyInteraction( playlist_t *p_playlist )
{
    if( p_playlist->p_interaction )
    {
        intf_InteractionDestroy( p_playlist->p_interaction );
    }
}

static void HandleInteraction( playlist_t *p_playlist )
{
    if( p_playlist->p_interaction )
    {
        stats_TimerStart( p_playlist, "Interaction thread",
                          STATS_TIMER_INTERACTION );
        intf_InteractionManage( p_playlist );
        stats_TimerStop( p_playlist, STATS_TIMER_INTERACTION );
    }
}


/*****************************************************************************
 * Stats functions
 *****************************************************************************/
static void HandleStats( playlist_t *p_playlist, int i_loops )
{
    if( i_loops %5 == 0 && p_playlist->p_stats )
    {
        stats_ComputeGlobalStats( p_playlist, p_playlist->p_stats );
        if( p_playlist->p_input )
        {
            stats_ComputeInputStats( p_playlist->p_input,
                                p_playlist->p_input->input.p_item->p_stats );
        }
    }
}
