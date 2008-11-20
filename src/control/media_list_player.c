/*****************************************************************************
 * media_list_player.c: libvlc new API media_list player functions
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include "libvlc_internal.h"
#include <vlc/libvlc.h>
#include "media_list_path.h"

/*
 * Private functions
 */

/**************************************************************************
 *       get_next_index (private)
 *
 * Simple next item fetcher.
 **************************************************************************/
static libvlc_media_list_path_t
get_next_path( libvlc_media_list_player_t * p_mlp )
{
    /* We are entered with libvlc_media_list_lock( p_mlp->p_list ) */
    libvlc_media_list_path_t ret;
    libvlc_media_list_t * p_parent_of_playing_item;
    libvlc_media_list_t * p_sublist_of_playing_item;

    if ( !p_mlp->current_playing_item_path )
    {
        if( !libvlc_media_list_count( p_mlp->p_mlist, NULL ) )
            return NULL;
        return libvlc_media_list_path_with_root_index(0);
    }
    
    p_sublist_of_playing_item = libvlc_media_list_sublist_at_path(
                            p_mlp->p_mlist,
                            p_mlp->current_playing_item_path );
 
    /* If item just gained a sublist just play it */
    if( p_sublist_of_playing_item )
    {
        libvlc_media_list_release( p_sublist_of_playing_item );
        return libvlc_media_list_path_copy_by_appending( p_mlp->current_playing_item_path, 0 );
    }

    /* Try to catch next element */
    p_parent_of_playing_item = libvlc_media_list_parentlist_at_path(
                            p_mlp->p_mlist,
                            p_mlp->current_playing_item_path );

    int deepness = libvlc_media_list_path_deepness( p_mlp->current_playing_item_path );
    if( deepness < 1 || !p_parent_of_playing_item )
        return NULL;

    ret = libvlc_media_list_path_copy( p_mlp->current_playing_item_path );

    while( ret[deepness-1] >= libvlc_media_list_count( p_parent_of_playing_item, NULL ) )
    {
        deepness--;
        if( deepness <= 0 )
        {
            free( ret );
            libvlc_media_list_release( p_parent_of_playing_item );
            return NULL;
        }
        ret[deepness] = -1;
        ret[deepness-1]++;
        p_parent_of_playing_item  = libvlc_media_list_parentlist_at_path(
                                        p_mlp->p_mlist,
                                        ret );
    }
    libvlc_media_list_release( p_parent_of_playing_item );
    return ret;
}

/**************************************************************************
 *       media_player_reached_end (private) (Event Callback)
 **************************************************************************/
static void
media_player_reached_end( const libvlc_event_t * p_event,
                            void * p_user_data )
{
    libvlc_media_list_player_t * p_mlp = p_user_data;
    libvlc_media_player_t * p_mi = p_event->p_obj;
    libvlc_media_t *p_md, * p_current_md;

    p_md = libvlc_media_player_get_media( p_mi, NULL );
    /* XXX: need if p_mlp->p_current_playing_index is beyond */
    p_current_md = libvlc_media_list_item_at_path(
                        p_mlp->p_mlist,
                        p_mlp->current_playing_item_path );
    if( p_md != p_current_md )
    {
        msg_Warn( p_mlp->p_libvlc_instance->p_libvlc_int,
                  "We are not sync-ed with the media instance" );
        libvlc_media_release( p_md );
        libvlc_media_release( p_current_md );
        return;
    }
    libvlc_media_release( p_md );
    libvlc_media_release( p_current_md );
    libvlc_media_list_player_next( p_mlp, NULL );
}

/**************************************************************************
 *       playlist_item_deleted (private) (Event Callback)
 **************************************************************************/
static void
mlist_item_deleted( const libvlc_event_t * p_event, void * p_user_data )
{
    libvlc_media_t * p_current_md;
    libvlc_media_list_player_t * p_mlp = p_user_data;
    libvlc_media_list_t * p_emitting_mlist = p_event->p_obj;
    /* XXX: need if p_mlp->p_current_playing_index is beyond */
    p_current_md = libvlc_media_list_item_at_path(
                        p_mlp->p_mlist,
                        p_mlp->current_playing_item_path );

    if( p_event->u.media_list_item_deleted.item == p_current_md &&
        p_emitting_mlist == p_mlp->p_mlist )
    {
        /* We are playing this item, we choose to stop */
        libvlc_media_list_player_stop( p_mlp, NULL );
    }
}

/**************************************************************************
 *       install_playlist_observer (private)
 **************************************************************************/
static void
install_playlist_observer( libvlc_media_list_player_t * p_mlp )
{
    libvlc_event_attach( libvlc_media_list_event_manager( p_mlp->p_mlist, NULL ),
            libvlc_MediaListItemDeleted, mlist_item_deleted, p_mlp, NULL );
}

/**************************************************************************
 *       uninstall_playlist_observer (private)
 **************************************************************************/
static void
uninstall_playlist_observer( libvlc_media_list_player_t * p_mlp )
{
    if ( !p_mlp->p_mlist )
    {
        return;
    }

    libvlc_event_detach( libvlc_media_list_event_manager( p_mlp->p_mlist, NULL ),
            libvlc_MediaListItemDeleted, mlist_item_deleted, p_mlp, NULL );
}

/**************************************************************************
 *       install_media_player_observer (private)
 **************************************************************************/
static void
install_media_player_observer( libvlc_media_list_player_t * p_mlp )
{
    libvlc_event_attach( libvlc_media_player_event_manager( p_mlp->p_mi, NULL ),
                         libvlc_MediaPlayerEndReached,
                          media_player_reached_end, p_mlp, NULL );
}


/**************************************************************************
 *       uninstall_media_player_observer (private)
 **************************************************************************/
static void
uninstall_media_player_observer( libvlc_media_list_player_t * p_mlp )
{
    if ( !p_mlp->p_mi )
    {
        return;
    }

    libvlc_event_detach( libvlc_media_player_event_manager( p_mlp->p_mi, NULL ),
                         libvlc_MediaPlayerEndReached,
                         media_player_reached_end, p_mlp, NULL );
}

/**************************************************************************
 *       set_current_playing_item (private)
 *
 * Playlist lock should be held
 **************************************************************************/
static void
set_current_playing_item( libvlc_media_list_player_t * p_mlp,
                          libvlc_media_list_path_t path,
                          libvlc_exception_t * p_e )
{
    VLC_UNUSED(p_e);

    libvlc_media_t * p_md;

    p_md = libvlc_media_list_item_at_path( p_mlp->p_mlist, path );
    vlc_mutex_lock( &p_mlp->object_lock );

    if( p_mlp->current_playing_item_path != path )
    {
        free( p_mlp->current_playing_item_path );
        p_mlp->current_playing_item_path = path;
    }

    if( !p_md )
    {
        vlc_mutex_unlock( &p_mlp->object_lock );
        return;
    }

    /* We are not interested in getting media stop event now */
    uninstall_media_player_observer( p_mlp );

    if ( !p_mlp->p_mi )
    {
        p_mlp->p_mi = libvlc_media_player_new_from_media(p_md, p_e);
    }
    
    if( p_md->p_subitems && libvlc_media_list_count( p_md->p_subitems, NULL ) > 0 )
    {
        libvlc_media_t * p_submd;
        p_submd = libvlc_media_list_item_at_index( p_md->p_subitems, 0, NULL );
        libvlc_media_player_set_media( p_mlp->p_mi, p_submd, NULL );
        libvlc_media_release( p_submd );
    }
    else
        libvlc_media_player_set_media( p_mlp->p_mi, p_md, NULL );
//    wait_playing_state(); /* If we want to be synchronous */
    install_media_player_observer( p_mlp );

    vlc_mutex_unlock( &p_mlp->object_lock );

    libvlc_media_release( p_md ); /* for libvlc_media_list_item_at_index */
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *         new (Public)
 **************************************************************************/
libvlc_media_list_player_t *
libvlc_media_list_player_new( libvlc_instance_t * p_instance,
                              libvlc_exception_t * p_e )
{
    (void)p_e;
    libvlc_media_list_player_t * p_mlp;
    p_mlp = malloc(sizeof(libvlc_media_list_player_t));
    if( !p_mlp )
        return NULL;

    p_mlp->current_playing_item_path = NULL;
    p_mlp->p_mi = NULL;
    p_mlp->p_mlist = NULL;
    vlc_mutex_init( &p_mlp->object_lock );
    p_mlp->p_event_manager = libvlc_event_manager_new( p_mlp,
                                                       p_instance,
                                                       p_e );
    libvlc_event_manager_register_event_type( p_mlp->p_event_manager,
            libvlc_MediaListPlayerNextItemSet, p_e );

    return p_mlp;
}

/**************************************************************************
 *         release (Public)
 **************************************************************************/
void libvlc_media_list_player_release( libvlc_media_list_player_t * p_mlp )
{
    free(p_mlp);
}

/**************************************************************************
 *        set_media_player (Public)
 **************************************************************************/
void libvlc_media_list_player_set_media_player(
                                     libvlc_media_list_player_t * p_mlp,
                                     libvlc_media_player_t * p_mi,
                                     libvlc_exception_t * p_e )
{
    VLC_UNUSED(p_e);

    vlc_mutex_lock( &p_mlp->object_lock );

    if( p_mlp->p_mi )
    {
        uninstall_media_player_observer( p_mlp );
        libvlc_media_player_release( p_mlp->p_mi );
    }
    libvlc_media_player_retain( p_mi );
    p_mlp->p_mi = p_mi;

    install_media_player_observer( p_mlp );

    vlc_mutex_unlock( &p_mlp->object_lock );
}

/**************************************************************************
 *       set_media_list (Public)
 **************************************************************************/
void libvlc_media_list_player_set_media_list(
                                     libvlc_media_list_player_t * p_mlp,
                                     libvlc_media_list_t * p_mlist,
                                     libvlc_exception_t * p_e )
{
    vlc_mutex_lock( &p_mlp->object_lock );

    if( libvlc_media_list_player_is_playing( p_mlp, p_e ) )
    {
        libvlc_media_player_stop( p_mlp->p_mi, p_e );
        /* Don't bother if there was an error. */
        libvlc_exception_clear( p_e );
    }

    if( p_mlp->p_mlist )
    {
        uninstall_playlist_observer( p_mlp );
        libvlc_media_list_release( p_mlp->p_mlist );
    }
    libvlc_media_list_retain( p_mlist );
    p_mlp->p_mlist = p_mlist;
 
    install_playlist_observer( p_mlp );

    vlc_mutex_unlock( &p_mlp->object_lock );
}

/**************************************************************************
 *        Play (Public)
 **************************************************************************/
void libvlc_media_list_player_play( libvlc_media_list_player_t * p_mlp,
                                  libvlc_exception_t * p_e )
{
    if( !p_mlp->current_playing_item_path )
    {
        libvlc_media_list_player_next( p_mlp, p_e );
        return; /* Will set to play */
    }

    libvlc_media_player_play( p_mlp->p_mi, p_e );
}


/**************************************************************************
 *        Pause (Public)
 **************************************************************************/
void libvlc_media_list_player_pause( libvlc_media_list_player_t * p_mlp,
                                     libvlc_exception_t * p_e )
{
    if( !p_mlp->p_mi )
        return;
    libvlc_media_player_pause( p_mlp->p_mi, p_e );
}

/**************************************************************************
 *        is_playing (Public)
 **************************************************************************/
int
libvlc_media_list_player_is_playing( libvlc_media_list_player_t * p_mlp,
                                     libvlc_exception_t * p_e )
{
    libvlc_state_t state = libvlc_media_player_get_state( p_mlp->p_mi, p_e );
    return (state == libvlc_Opening) || (state == libvlc_Buffering) ||
           (state == libvlc_Playing);
}

/**************************************************************************
 *        State (Public)
 **************************************************************************/
libvlc_state_t
libvlc_media_list_player_get_state( libvlc_media_list_player_t * p_mlp,
                                    libvlc_exception_t * p_e )
{
    if( !p_mlp->p_mi )
        return libvlc_Ended;
    return libvlc_media_player_get_state( p_mlp->p_mi, p_e );
}

/**************************************************************************
 *        Play item at index (Public)
 **************************************************************************/
void libvlc_media_list_player_play_item_at_index(
                        libvlc_media_list_player_t * p_mlp,
                        int i_index,
                        libvlc_exception_t * p_e )
{
    set_current_playing_item( p_mlp, libvlc_media_list_path_with_root_index(i_index), p_e );

    if( libvlc_exception_raised( p_e ) )
        return;

    /* Send the next item event */
    libvlc_event_t event;
    event.type = libvlc_MediaListPlayerNextItemSet;
    libvlc_event_send( p_mlp->p_event_manager, &event );

    libvlc_media_player_play( p_mlp->p_mi, p_e );
}

/**************************************************************************
 *        Play item (Public)
 **************************************************************************/
void libvlc_media_list_player_play_item(
                        libvlc_media_list_player_t * p_mlp,
                        libvlc_media_t * p_md,
                        libvlc_exception_t * p_e )
{
    libvlc_media_list_path_t path = libvlc_media_list_path_of_item( p_mlp->p_mlist, p_md );
    if( !path )
    {
        libvlc_exception_raise( p_e, "No such item in media list" );
        return;
    }
    set_current_playing_item( p_mlp, path, p_e );

    if( libvlc_exception_raised( p_e ) )
        return;

    libvlc_media_player_play( p_mlp->p_mi, p_e );
}

/**************************************************************************
 *       Stop (Public)
 **************************************************************************/
void libvlc_media_list_player_stop( libvlc_media_list_player_t * p_mlp,
                                    libvlc_exception_t * p_e )
{
    if ( p_mlp->p_mi )
    {
        /* We are not interested in getting media stop event now */
        uninstall_media_player_observer( p_mlp );
        libvlc_media_player_stop( p_mlp->p_mi, p_e );
        install_media_player_observer( p_mlp );
    }

    vlc_mutex_lock( &p_mlp->object_lock );
    free( p_mlp->current_playing_item_path );
    p_mlp->current_playing_item_path = NULL;
    vlc_mutex_unlock( &p_mlp->object_lock );
}

/**************************************************************************
 *       Next (Public)
 **************************************************************************/
void libvlc_media_list_player_next( libvlc_media_list_player_t * p_mlp,
                                    libvlc_exception_t * p_e )
{
    libvlc_media_list_path_t path;

    if (! p_mlp->p_mlist )
    {
        libvlc_exception_raise( p_e, "No more element to play" );
        return;
    }

    libvlc_media_list_lock( p_mlp->p_mlist );

    path = get_next_path( p_mlp );

    if( !path )
    {
        libvlc_media_list_unlock( p_mlp->p_mlist );
        libvlc_exception_raise( p_e, "No more element to play" );
        libvlc_media_list_player_stop( p_mlp, p_e );
        return;
    }

    set_current_playing_item( p_mlp, path, p_e );

    libvlc_media_player_play( p_mlp->p_mi, p_e );

    libvlc_media_list_unlock( p_mlp->p_mlist );

    /* Send the next item event */
    libvlc_event_t event;
    event.type = libvlc_MediaListPlayerNextItemSet;
    libvlc_event_send( p_mlp->p_event_manager, &event);
}
