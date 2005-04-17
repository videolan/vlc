/*****************************************************************************
 * playlist.c : Playlist management functions
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/
#include <stdlib.h>                                      /* free(), strtol() */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */

#include <vlc/vlc.h>
#include <vlc/vout.h>
#include <vlc/sout.h>
#include <vlc/input.h>

#include "vlc_playlist.h"

#define TITLE_CATEGORY N_( "By category" )
#define TITLE_SIMPLE   N_( "Manually added" )
#define TITLE_ALL      N_( "All items, unsorted" )

#undef PLAYLIST_PROFILE
#undef PLAYLIST_DEBUG

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void RunThread ( playlist_t * );
static void RunPreparse( playlist_preparse_t * );
static playlist_item_t * NextItem  ( playlist_t * );
static int PlayItem  ( playlist_t *, playlist_item_t * );

static int ItemChange( vlc_object_t *, const char *,
                       vlc_value_t, vlc_value_t, void * );

int playlist_vaControl( playlist_t * p_playlist, int i_query, va_list args );


/**
 * Create playlist
 *
 * Create a playlist structure.
 * \param p_parent the vlc object that is to be the parent of this playlist
 * \return a pointer to the created playlist, or NULL on error
 */
playlist_t * __playlist_Create ( vlc_object_t *p_parent )
{
    playlist_t *p_playlist;
    playlist_view_t *p_view;
    vlc_value_t     val;

    /* Allocate structure */
    p_playlist = vlc_object_create( p_parent, VLC_OBJECT_PLAYLIST );
    if( !p_playlist )
    {
        msg_Err( p_parent, "out of memory" );
        return NULL;
    }

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


    /* Variables to control playback */
    var_CreateGetBool( p_playlist, "play-and-stop" );
    var_CreateGetBool( p_playlist, "random" );
    var_CreateGetBool( p_playlist, "repeat" );
    var_CreateGetBool( p_playlist, "loop" );

    /* Initialise data structures */
    p_playlist->i_last_id = 0;
    p_playlist->b_go_next = VLC_TRUE;
    p_playlist->p_input = NULL;

    p_playlist->request_date = 0;

    p_playlist->i_views = 0;
    p_playlist->pp_views = NULL;

    p_playlist->i_index = -1;
    p_playlist->i_size = 0;
    p_playlist->pp_items = NULL;
    p_playlist->i_all_size = 0;
    p_playlist->pp_all_items = 0;

    playlist_ViewInsert( p_playlist, VIEW_CATEGORY, TITLE_CATEGORY );
    playlist_ViewInsert( p_playlist, VIEW_ALL, TITLE_ALL );

    p_view = playlist_ViewFind( p_playlist, VIEW_CATEGORY );

    p_playlist->p_general = playlist_NodeCreate( p_playlist, VIEW_CATEGORY,
                                        _( "General" ), p_view->p_root );
    p_playlist->p_general->i_flags |= PLAYLIST_RO_FLAG;

    /* Set startup status
     * We set to simple view on startup for interfaces that don't do
     * anything */
    p_view = playlist_ViewFind( p_playlist, VIEW_CATEGORY );
    p_playlist->status.i_view = VIEW_CATEGORY;
    p_playlist->status.p_item = NULL;
    p_playlist->status.p_node = p_view->p_root;
    p_playlist->request.b_request = VLC_FALSE;
    p_playlist->status.i_status = PLAYLIST_STOPPED;


    p_playlist->i_sort = SORT_ID;
    p_playlist->i_order = ORDER_NORMAL;

    /* Finally, launch the thread ! */
    if( vlc_thread_create( p_playlist, "playlist", RunThread,
                           VLC_THREAD_PRIORITY_LOW, VLC_TRUE ) )
    {
        msg_Err( p_playlist, "cannot spawn playlist thread" );
        vlc_object_destroy( p_playlist );
        return NULL;
    }

    /* Preparsing stuff */
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

    /* The object has been initialized, now attach it */
    vlc_object_attach( p_playlist, p_parent );

    return p_playlist;
}

/**
 * Destroy the playlist.
 *
 * Delete all items in the playlist and free the playlist structure.
 * \param p_playlist the playlist structure to destroy
 * \return VLC_SUCCESS or an error
 */
int playlist_Destroy( playlist_t * p_playlist )
{
    int i;
    p_playlist->b_die = 1;

    for( i = 0 ; i< p_playlist->i_sds ; i++ )
    {
        playlist_ServicesDiscoveryRemove( p_playlist,
                                          p_playlist->pp_sds[i]->psz_module );
    }

    vlc_thread_join( p_playlist->p_preparse );
    vlc_thread_join( p_playlist );

    vlc_object_detach( p_playlist->p_preparse );

    var_Destroy( p_playlist, "intf-change" );
    var_Destroy( p_playlist, "item-change" );
    var_Destroy( p_playlist, "playlist-current" );
    var_Destroy( p_playlist, "intf-popmenu" );
    var_Destroy( p_playlist, "intf-show" );
    var_Destroy( p_playlist, "play-and-stop" );
    var_Destroy( p_playlist, "random" );
    var_Destroy( p_playlist, "repeat" );
    var_Destroy( p_playlist, "loop" );

    playlist_Clear( p_playlist );

    for( i = p_playlist->i_views - 1; i >= 0 ; i-- )
    {
        playlist_view_t *p_view = p_playlist->pp_views[i];
        if( p_view->psz_name )
            free( p_view->psz_name );
        playlist_ItemDelete( p_view->p_root );
        REMOVE_ELEM( p_playlist->pp_views, p_playlist->i_views, i );
        free( p_view );
    }

    vlc_object_destroy( p_playlist->p_preparse );
    vlc_object_destroy( p_playlist );

    return VLC_SUCCESS;
}


/**
 * Do a playlist action.
 *
 * If there is something in the playlist then you can do playlist actions.
 *
 * Playlist lock must not be taken when calling this function
 *
 * \param p_playlist the playlist to do the command on
 * \param i_query the command to do
 * \param variable number of arguments
 * \return VLC_SUCCESS or an error
 */
int playlist_LockControl( playlist_t * p_playlist, int i_query, ... )
{
    va_list args;
    int i_result;
    va_start( args, i_query );
    vlc_mutex_lock( &p_playlist->object_lock );
    i_result = playlist_vaControl( p_playlist, i_query, args );
    va_end( args );
    vlc_mutex_unlock( &p_playlist->object_lock );
    return i_result;
}

/**
 * Do a playlist action.
 *
 * If there is something in the playlist then you can do playlist actions.
 *
 * Playlist lock must be taken when calling this function
 *
 * \param p_playlist the playlist to do the command on
 * \param i_query the command to do
 * \param variable number of arguments
 * \return VLC_SUCCESS or an error
 */
int playlist_Control( playlist_t * p_playlist, int i_query, ... )
{
    va_list args;
    int i_result;
    va_start( args, i_query );
    i_result = playlist_vaControl( p_playlist, i_query, args );
    va_end( args );

    return i_result;
}

int playlist_vaControl( playlist_t * p_playlist, int i_query, va_list args )
{
    playlist_view_t *p_view;
    vlc_value_t val;

#ifdef PLAYLIST_PROFILE
    p_playlist->request_date = mdate();
#endif

    if( p_playlist->i_size <= 0 )
    {
        return VLC_EGENERIC;
    }

    switch( i_query )
    {
    case PLAYLIST_STOP:
        p_playlist->status.i_status = PLAYLIST_STOPPED;
        p_playlist->request.b_request = VLC_TRUE;
        break;

    case PLAYLIST_ITEMPLAY:
        p_playlist->status.i_status = PLAYLIST_RUNNING;
        p_playlist->request.i_skip = 0;
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.p_item = (playlist_item_t *)va_arg( args,
                                                   playlist_item_t *);
        p_playlist->request.i_view = p_playlist->status.i_view;
        p_view = playlist_ViewFind( p_playlist, p_playlist->status.i_view );
        if( p_view )
        {
            p_playlist->request.p_node = p_view->p_root;
        }
        else
        {
            p_playlist->request.p_node = NULL;
        }
        break;

    case PLAYLIST_VIEWPLAY:
        p_playlist->status.i_status = PLAYLIST_RUNNING;
        p_playlist->request.i_skip = 0;
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.i_view = (int)va_arg( args,int );
        p_playlist->request.p_node = (playlist_item_t *)va_arg( args,
                                                        playlist_item_t *);
        p_playlist->request.p_item = (playlist_item_t *)va_arg( args,
                                                        playlist_item_t *);

        /* If we select a node, play only it.
         * If we select an item, continue */
        if( p_playlist->request.p_item == NULL ||
            ! p_playlist->request.p_node->i_flags & PLAYLIST_SKIP_FLAG )
        {
            p_playlist->b_go_next = VLC_FALSE;
        }
        else
        {
            p_playlist->b_go_next = VLC_TRUE;
        }
        break;

    case PLAYLIST_PLAY:
        p_playlist->status.i_status = PLAYLIST_RUNNING;

        if( p_playlist->p_input )
        {
            val.i_int = PLAYING_S;
            var_Set( p_playlist->p_input, "state", val );
            break;
        }

        /* FIXME : needed ? */
        p_playlist->request.b_request = VLC_TRUE;
        p_playlist->request.i_view = p_playlist->status.i_view;
        p_playlist->request.p_node = p_playlist->status.p_node;
        p_playlist->request.p_item = p_playlist->status.p_item;
        p_playlist->request.i_skip = 0;
        p_playlist->request.i_goto = -1;
        break;

    case PLAYLIST_AUTOPLAY:
        p_playlist->status.i_status = PLAYLIST_RUNNING;

        p_playlist->request.b_request = VLC_FALSE;
        break;

    case PLAYLIST_PAUSE:
        val.i_int = 0;
        if( p_playlist->p_input )
            var_Get( p_playlist->p_input, "state", &val );

        if( val.i_int == PAUSE_S )
        {
            p_playlist->status.i_status = PLAYLIST_RUNNING;
            if( p_playlist->p_input )
            {
                val.i_int = PLAYING_S;
                var_Set( p_playlist->p_input, "state", val );
            }
        }
        else
        {
            p_playlist->status.i_status = PLAYLIST_PAUSED;
            if( p_playlist->p_input )
            {
                val.i_int = PAUSE_S;
                var_Set( p_playlist->p_input, "state", val );
            }
        }
        break;

    case PLAYLIST_SKIP:
        if( p_playlist->status.i_view > -1 )
        {
            p_playlist->request.i_view = p_playlist->status.i_view;
            p_playlist->request.p_node = p_playlist->status.p_node;
            p_playlist->request.p_item = p_playlist->status.p_item;
        }
        p_playlist->request.i_skip = (int) va_arg( args, int );
        p_playlist->request.b_request = VLC_TRUE;
        break;

    case PLAYLIST_GOTO:
        p_playlist->status.i_status = PLAYLIST_RUNNING;
        p_playlist->request.p_node = NULL;
        p_playlist->request.p_item = NULL;
        p_playlist->request.i_view = -1;
        p_playlist->request.i_goto = (int) va_arg( args, int );
        p_playlist->request.b_request = VLC_TRUE;
        break;

    default:
        msg_Err( p_playlist, "unimplemented playlist query" );
        return VLC_EBADVAR;
        break;
    }

    return VLC_SUCCESS;
}

int playlist_PreparseEnqueue( playlist_t *p_playlist,
                              input_item_t *p_item )
{
    vlc_mutex_lock( &p_playlist->p_preparse->object_lock );
    INSERT_ELEM( p_playlist->p_preparse->pp_waiting,
                 p_playlist->p_preparse->i_waiting,
                 p_playlist->p_preparse->i_waiting,
                 p_item );
    vlc_mutex_unlock( &p_playlist->p_preparse->object_lock );
    return VLC_SUCCESS;
}


/* Destroy remaining objects */
static mtime_t ObjectGarbageCollector( playlist_t *p_playlist, int i_type,
                                       mtime_t destroy_date )
{
    vlc_object_t *p_obj;

    if( destroy_date > mdate() ) return destroy_date;

    if( destroy_date == 0 )
    {
        /* give a little time */
        return mdate() + I64C(1000000);
    }
    else
    {
        while( ( p_obj = vlc_object_find( p_playlist, i_type, FIND_CHILD ) ) )
        {
            if( p_obj->p_parent != (vlc_object_t*)p_playlist )
            {
                /* only first child (ie unused) */
                vlc_object_release( p_obj );
                break;
            }
            if( i_type == VLC_OBJECT_VOUT )
            {
                msg_Dbg( p_playlist, "garbage collector destroying 1 vout" );
                vlc_object_detach( p_obj );
                vlc_object_release( p_obj );
                vout_Destroy( (vout_thread_t *)p_obj );
            }
            else if( i_type == VLC_OBJECT_SOUT )
            {
                vlc_object_release( p_obj );
                sout_DeleteInstance( (sout_instance_t*)p_obj );
            }
        }
        return 0;
    }
}

/*****************************************************************************
 * RunThread: main playlist thread
 *****************************************************************************/
static void RunThread ( playlist_t *p_playlist )
{
    vlc_object_t *p_obj;
    playlist_item_t *p_item = NULL;

    mtime_t    i_vout_destroyed_date = 0;
    mtime_t    i_sout_destroyed_date = 0;

    playlist_item_t *p_autodelete_item = NULL;

    /* Tell above that we're ready */
    vlc_thread_ready( p_playlist );

    while( !p_playlist->b_die )
    {
        vlc_mutex_lock( &p_playlist->object_lock );

        /* First, check if we have something to do */
        /* FIXME : this can be called several times */
        if( p_playlist->request.b_request )
        {
#ifdef PLAYLIST_PROFILE
            msg_Dbg(p_playlist, "beginning processing of request, "
                         I64Fi" us ", mdate() - p_playlist->request_date );
#endif
            /* Stop the existing input */
            if( p_playlist->p_input )
            {
                input_StopThread( p_playlist->p_input );
            }
            /* The code below will start the next input for us */
            if( p_playlist->status.i_status == PLAYLIST_STOPPED )
            {
                p_playlist->request.b_request = VLC_FALSE;
            }
        }

        /* If there is an input, check that it doesn't need to die. */
        if( p_playlist->p_input )
        {
            /* This input is dead. Remove it ! */
            if( p_playlist->p_input->b_dead )
            {
                input_thread_t *p_input;

                p_input = p_playlist->p_input;
                p_playlist->p_input = NULL;

                /* Release the playlist lock, because we may get stuck
                 * in input_DestroyThread() for some time. */
                vlc_mutex_unlock( &p_playlist->object_lock );

                /* Destroy input */
                input_DestroyThread( p_input );

                /* Unlink current input
                 * (_after_ input_DestroyThread for vout garbage collector) */
                vlc_object_detach( p_input );

                /* Destroy object */
                vlc_object_destroy( p_input );

                i_vout_destroyed_date = 0;
                i_sout_destroyed_date = 0;

                if( p_playlist->status.p_item->i_flags
                    & PLAYLIST_REMOVE_FLAG )
                {
                     playlist_ItemDelete( p_item );
                     p_playlist->status.p_item = NULL;
                }

                continue;
            }
            /* This input is dying, let him do */
            else if( p_playlist->p_input->b_die )
            {
                ;
            }
            /* This input has finished, ask him to die ! */
            else if( p_playlist->p_input->b_error
                      || p_playlist->p_input->b_eof )
            {
                /* TODO FIXME XXX TODO FIXME XXX */
                /* Check for autodeletion */

                if( p_playlist->status.p_item->i_flags & PLAYLIST_DEL_FLAG )
                {
                    p_autodelete_item = p_playlist->status.p_item;
                }
                input_StopThread( p_playlist->p_input );
                /* Select the next playlist item */
                vlc_mutex_unlock( &p_playlist->object_lock );
                continue;
            }
            else if( p_playlist->p_input->i_state != INIT_S )
            {
                vlc_mutex_unlock( &p_playlist->object_lock );
                i_vout_destroyed_date =
                    ObjectGarbageCollector( p_playlist, VLC_OBJECT_VOUT,
                                            i_vout_destroyed_date );
                i_sout_destroyed_date =
                    ObjectGarbageCollector( p_playlist, VLC_OBJECT_SOUT,
                                            i_sout_destroyed_date );
                vlc_mutex_lock( &p_playlist->object_lock );
            }
        }
        else if( p_playlist->status.i_status != PLAYLIST_STOPPED )
        {
            /* Start another input.
             * Get the next item to play */
            p_item = NextItem( p_playlist );


            /* We must stop */
            if( p_item == NULL )
            {
                if( p_autodelete_item )
                {
                    playlist_Delete( p_playlist,
                                     p_autodelete_item->input.i_id );
                    p_autodelete_item = NULL;
                }
                p_playlist->status.i_status = PLAYLIST_STOPPED;
                vlc_mutex_unlock( &p_playlist->object_lock );
                continue;
            }

            PlayItem( p_playlist, p_item );

            if( p_autodelete_item )
            {
                playlist_Delete( p_playlist, p_autodelete_item->input.i_id );
                p_autodelete_item = NULL;
            }
        }
        else if( p_playlist->status.i_status == PLAYLIST_STOPPED )
        {
            /* Collect garbage */
            vlc_mutex_unlock( &p_playlist->object_lock );
            i_sout_destroyed_date =
                ObjectGarbageCollector( p_playlist, VLC_OBJECT_SOUT, mdate() );
            i_vout_destroyed_date =
                ObjectGarbageCollector( p_playlist, VLC_OBJECT_VOUT, mdate() );
            vlc_mutex_lock( &p_playlist->object_lock );
        }
        vlc_mutex_unlock( &p_playlist->object_lock );

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

    /* Playlist dying */

    /* If there is an input, kill it */
    while( 1 )
    {
        vlc_mutex_lock( &p_playlist->object_lock );

        if( p_playlist->p_input == NULL )
        {
            vlc_mutex_unlock( &p_playlist->object_lock );
            break;
        }

        if( p_playlist->p_input->b_dead )
        {
            input_thread_t *p_input;

            /* Unlink current input */
            p_input = p_playlist->p_input;
            p_playlist->p_input = NULL;
            vlc_mutex_unlock( &p_playlist->object_lock );

            /* Destroy input */
            input_DestroyThread( p_input );
            /* Unlink current input (_after_ input_DestroyThread for vout
             * garbage collector)*/
            vlc_object_detach( p_input );

            /* Destroy object */
            vlc_object_destroy( p_input );
            continue;
        }
        else if( p_playlist->p_input->b_die )
        {
            /* This input is dying, leave him alone */
            ;
        }
        else if( p_playlist->p_input->b_error || p_playlist->p_input->b_eof )
        {
            input_StopThread( p_playlist->p_input );
            vlc_mutex_unlock( &p_playlist->object_lock );
            continue;
        }
        else
        {
            p_playlist->p_input->b_eof = 1;
        }

        vlc_mutex_unlock( &p_playlist->object_lock );

        msleep( INTF_IDLE_SLEEP );
    }

    /* close all remaining sout */
    while( ( p_obj = vlc_object_find( p_playlist,
                                      VLC_OBJECT_SOUT, FIND_CHILD ) ) )
    {
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
}

/* Queue for items to preparse */
static void RunPreparse ( playlist_preparse_t *p_obj )
{
    playlist_t *p_playlist = (playlist_t *)p_obj->p_parent;
    vlc_bool_t b_sleep;

    /* Tell above that we're ready */
    vlc_thread_ready( p_obj );

    while( !p_playlist->b_die )
    {
        vlc_mutex_lock( &p_obj->object_lock );

        if( p_obj->i_waiting > 0 )
        {
            input_item_t *p_current = p_obj->pp_waiting[0];
            REMOVE_ELEM( p_obj->pp_waiting, p_obj->i_waiting, 0 );
            vlc_mutex_unlock( &p_obj->object_lock );
            input_Preparse( p_playlist, p_current );
            var_SetInteger( p_playlist, "item-change", p_current->i_id );
            vlc_mutex_lock( &p_obj->object_lock );
        }
        b_sleep = ( p_obj->i_waiting == 0 );

        vlc_mutex_unlock( &p_obj->object_lock );

        if( p_obj->i_waiting == 0 )
        {
            msleep( INTF_IDLE_SLEEP );
        }
    }
}

/*****************************************************************************
 * NextItem
 *****************************************************************************
 * This function calculates the next playlist item, depending
 * on the playlist course mode (forward, backward, random, view,...).
 *****************************************************************************/
static playlist_item_t * NextItem( playlist_t *p_playlist )
{
    playlist_item_t *p_new = NULL;
    int i_skip,i_goto,i, i_new, i_count ;
    playlist_view_t *p_view;

    vlc_bool_t b_loop = var_GetBool( p_playlist, "loop");
    vlc_bool_t b_random = var_GetBool( p_playlist, "random" );
    vlc_bool_t b_repeat = var_GetBool( p_playlist, "repeat" );
    vlc_bool_t b_playstop = var_GetBool( p_playlist, "play-and-stop" );

#ifdef PLAYLIST_PROFILE
    /* Calculate time needed */
    int64_t start = mdate();
#endif


    /* Handle quickly a few special cases */

    /* No items to play */
    if( p_playlist->i_size == 0 )
    {
        msg_Info( p_playlist, "playlist is empty" );
        return NULL;
    }
    /* Nothing requested */
    if( !p_playlist->request.b_request && p_playlist->status.p_item == NULL )
    {
        msg_Dbg( p_playlist,"nothing requested, starting" );
    }

    /* Repeat and play/stop */
    if( !p_playlist->request.b_request && b_repeat == VLC_TRUE )
    {
        msg_Dbg( p_playlist,"repeating item" );
        return p_playlist->status.p_item;
    }

    if( !p_playlist->request.b_request && b_playstop == VLC_TRUE )
    {
        msg_Dbg( p_playlist,"stopping (play and stop)");
        return NULL;
    }

    if( !p_playlist->request.b_request && p_playlist->status.p_item &&
        !( p_playlist->status.p_item->i_flags & PLAYLIST_SKIP_FLAG ) )
    {
        msg_Dbg( p_playlist, "no-skip mode, stopping") ;
        return NULL;
    }

    /* TODO: improve this (only use current node) */
    /* TODO: use the "shuffled view" internally ? */
    /* Random case. This is an exception: if request, but request is skip +- 1
     * we don't go to next item but select a new random one. */
    if( b_random && (!p_playlist->request.b_request ||
        p_playlist->request.i_skip == 1 || p_playlist->request.i_skip == -1 ) )
    {
        srand( (unsigned int)mdate() );
        i_new = 0;
        for( i_count = 0; i_count < p_playlist->i_size - 1 ; i_count ++ )
        {
            i_new =
                (int)((float)p_playlist->i_size * rand() / (RAND_MAX+1.0));
            /* Check if the item has not already been played */
            if( p_playlist->pp_items[i_new]->i_nb_played == 0 )
                break;
        }
        if( i_count == p_playlist->i_size )
        {
            /* The whole playlist has been played: reset the counters */
            while( i_count > 0 )
            {
                p_playlist->pp_items[--i_count]->i_nb_played = 0;
            }
           if( !b_loop )
            {
                return NULL;
            }
        }
        p_playlist->request.i_skip = 0;
        p_playlist->request.b_request = VLC_FALSE;
        return p_playlist->pp_items[i_new];
   }

    /* Start the real work */
    if( p_playlist->request.b_request )
    {
#ifdef PLAYLIST_DEBUG
        msg_Dbg( p_playlist,"processing request" );
#endif
        /* We are not playing from a view */
        if(  p_playlist->request.i_view == -1  )
        {
#ifdef PLAYLIST_DEBUG
            msg_Dbg( p_playlist, "non-view mode request");
#endif
            /* Directly select the item, just like now */
            i_skip = p_playlist->request.i_skip;
            i_goto = p_playlist->request.i_goto;

            if( p_playlist->i_index == -1 ) p_playlist->i_index = 0;
            p_new = p_playlist->pp_items[p_playlist->i_index];

            if( i_goto >= 0  && i_goto < p_playlist->i_size )
            {
                p_playlist->i_index = i_goto;
                p_new = p_playlist->pp_items[p_playlist->i_index];
                p_playlist->request.i_goto = -1;
            }

            if( i_skip != 0 )
            {
                if( p_playlist->i_index + i_skip < p_playlist->i_size &&
                    p_playlist->i_index + i_skip >=  0 )
                {
                    p_playlist->i_index += i_skip;
                    p_new = p_playlist->pp_items[p_playlist->i_index];
                }
                p_playlist->request.i_skip = 0;
            }
        }
        else
        {
#ifdef PLAYLIST_DEBUG
            msg_Dbg( p_playlist, "view mode request" );
#endif
            p_new = p_playlist->request.p_item;
            i_skip = p_playlist->request.i_skip;

            /* If we are asked for a node, take its first item */
            if( p_playlist->request.p_item == NULL && i_skip == 0 )
            {
                i_skip++;
            }

            p_view = playlist_ViewFind( p_playlist,p_playlist->request.i_view );
            p_playlist->status.p_node = p_playlist->request.p_node;
            p_playlist->status.i_view = p_playlist->request.i_view;
            if( !p_view )
            {
                msg_Err( p_playlist, "p_view is NULL and should not! (requested view is %i", p_playlist->request.i_view );
            }
            else if( i_skip > 0 )
            {
                for( i = i_skip; i > 0 ; i-- )
                {
                    p_new = playlist_FindNextFromParent( p_playlist,
                                    p_playlist->request.i_view,
                                    p_view->p_root,
                                    p_playlist->request.p_node,
                                    p_new );
                    if( p_new == NULL )
                    {
                        if( b_loop )
                        {
                            p_new = playlist_FindNextFromParent( p_playlist,
                                      p_playlist->request.i_view,
                                      p_view->p_root,
                                      p_playlist->request.p_node,
                                      NULL );
                            if( p_new == NULL ) break;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
            else if( i_skip < 0 )
            {
                for( i = i_skip; i < 0 ; i++ )
                {
                    p_new = playlist_FindPrevFromParent( p_playlist,
                                    p_playlist->request.i_view,
                                    p_view->p_root,
                                    p_playlist->request.p_node,
                                    p_new );
                    if( p_new == NULL ) break;
                }

            }
        }
        /* Clear the request */
        p_playlist->request.b_request = VLC_FALSE;
    }
    /* "Automatic" item change ( next ) */
    else
    {
        p_playlist->request_date = 0;

        if( p_playlist->status.i_view == -1 )
        {
            if( p_playlist->i_index + 1 < p_playlist->i_size )
            {
                p_playlist->i_index++;
                p_new = p_playlist->pp_items[p_playlist->i_index];
                if( !( p_new->i_flags & PLAYLIST_SKIP_FLAG ) )
                {
                    return NULL;
                }
            }
            else
            {
                if( b_loop && p_playlist->i_size > 0)
                {
                    p_playlist->i_index = 0;
                    p_new = p_playlist->pp_items[0];
                }
                else
                    p_new = NULL;
            }
        }
        /* We are playing with a view */
        else
        {
            playlist_view_t *p_view =
                    playlist_ViewFind( p_playlist,
                                   p_playlist->status.i_view );
            if( !p_view )
            {
                msg_Err( p_playlist, "p_view is NULL and should not! (FIXME)" );
            }
            else
            {
                p_new = playlist_FindNextFromParent( p_playlist,
                            p_playlist->status.i_view,
                            p_view->p_root,
                            p_playlist->status.p_node,
                            p_playlist->status.p_item );
                if( p_new == NULL && b_loop )
                {
                    p_new = playlist_FindNextFromParent( p_playlist,
                                   p_playlist->status.i_view,
                                   p_view->p_root,
                                   p_playlist->status.p_node,
                                   NULL );
                }
                if( p_new != NULL && !(p_new->i_flags & PLAYLIST_SKIP_FLAG) )
                    return NULL;
            }
        }
    }

    /* Reset index */
    if( p_playlist->i_index >= 0 && p_new != NULL &&
            p_playlist->pp_items[p_playlist->i_index] != p_new )
    {
        p_playlist->i_index = playlist_GetPositionById( p_playlist,
                                                        p_new->input.i_id );
    }

#ifdef PLAYLIST_PROFILE
    msg_Dbg(p_playlist,"next item found in "I64Fi " us", mdate()-start );
#endif

    if( p_new == NULL )
    {
        msg_Info( p_playlist, "nothing to play" );
    }
    return p_new;
}

/*****************************************************************************
 * PlayItem: start the input thread for an item
 ****************************************************************************/
static int PlayItem( playlist_t *p_playlist, playlist_item_t *p_item )
{
    vlc_value_t val;

    msg_Dbg( p_playlist, "creating new input thread" );

    p_item->i_nb_played++;
    p_playlist->status.p_item = p_item;

    p_playlist->i_index = playlist_GetPositionById( p_playlist,
                                                    p_item->input.i_id );

#ifdef PLAYLIST_PROFILE
    if( p_playlist->request_date != 0 )
    {
        msg_Dbg( p_playlist, "request processed after "I64Fi " us",
                  mdate() - p_playlist->request_date );
    }
#endif

    p_playlist->p_input = input_CreateThread( p_playlist, &p_item->input );

    var_AddCallback( p_playlist->p_input, "item-change",
                         ItemChange, p_playlist );

    val.i_int = p_item->input.i_id;
    /* unlock the playlist to set the var...mmm */
    vlc_mutex_unlock( &p_playlist->object_lock);
    var_Set( p_playlist, "playlist-current", val);
    vlc_mutex_lock( &p_playlist->object_lock);

    return VLC_SUCCESS;

}

/* Forward item change from input */
static int ItemChange( vlc_object_t *p_obj, const char *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *param )
{
    playlist_t *p_playlist = (playlist_t *)param;

    //p_playlist->b_need_update = VLC_TRUE;
    var_SetInteger( p_playlist, "item-change", newval.i_int );

    /* Update view */
    /* FIXME: Make that automatic */
//    playlist_ViewUpdate( p_playlist, VIEW_S_AUTHOR );

    return VLC_SUCCESS;
}
