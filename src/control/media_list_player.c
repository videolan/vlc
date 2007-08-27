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

/*
 * Private functions
 */

/**************************************************************************
 *       get_next_index (private)
 *
 * Simple next item fetcher.
 **************************************************************************/
static int get_next_index( libvlc_media_list_player_t * p_mlp )
{
	/* We are entered with libvlc_media_list_lock( p_mlp->p_list ) */
	
	int next = p_mlp->i_current_playing_index + 1;

	if( next >= libvlc_media_list_count( p_mlp->p_mlist, NULL ) )
		return -1; /* no more to play */

	return next;
}

/**************************************************************************
 *       media_instance_reached_end (private) (Event Callback)
 **************************************************************************/
static void 
media_instance_reached_end( const libvlc_event_t * p_event,
                            void * p_user_data )
{
	libvlc_media_list_player_t * p_mlp = p_user_data;
	libvlc_media_instance_t * p_mi = p_event->p_obj;
    libvlc_media_descriptor_t *p_md, * p_current_md;
    p_md = libvlc_media_instance_get_media_descriptor( p_mi, NULL );
    /* XXX: need if p_mlp->p_current_playing_index is beyond */
    p_current_md = libvlc_media_list_item_at_index(
                        p_mlp->p_mlist,
                        p_mlp->i_current_playing_index,
                        NULL );
	if( p_md != p_current_md )
	{
		msg_Warn( p_mlp->p_libvlc_instance->p_libvlc_int,
				  "We are not sync-ed with the media instance" );
        libvlc_media_descriptor_release( p_md );
        libvlc_media_descriptor_release( p_current_md );
		return;
	}
    libvlc_media_descriptor_release( p_md );
    libvlc_media_descriptor_release( p_current_md );
	libvlc_media_list_player_next( p_mlp, NULL );
}

/**************************************************************************
 *       playlist_item_deleted (private) (Event Callback)
 **************************************************************************/
static void 
mlist_item_deleted( const libvlc_event_t * p_event, void * p_user_data )
{
    libvlc_media_descriptor_t * p_current_md;    
	libvlc_media_list_player_t * p_mlp = p_user_data;
	libvlc_media_list_t * p_emitting_mlist = p_event->p_obj;
    /* XXX: need if p_mlp->p_current_playing_index is beyond */
    p_current_md = libvlc_media_list_item_at_index(
                        p_mlp->p_mlist,
                        p_mlp->i_current_playing_index,
                        NULL );

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
    libvlc_event_detach( libvlc_media_list_event_manager( p_mlp->p_mlist, NULL ),
            libvlc_MediaListItemDeleted, mlist_item_deleted, p_mlp, NULL );
}

/**************************************************************************
 *       install_media_instance_observer (private)
 **************************************************************************/
static void
install_media_instance_observer( libvlc_media_list_player_t * p_mlp )
{
	libvlc_event_attach( libvlc_media_instance_event_manager( p_mlp->p_mi, NULL ),
                         libvlc_MediaInstanceReachedEnd,
					      media_instance_reached_end, p_mlp, NULL );
}


/**************************************************************************
 *       uninstall_media_instance_observer (private)
 **************************************************************************/
static void
uninstall_media_instance_observer( libvlc_media_list_player_t * p_mlp )
{
	libvlc_event_detach( libvlc_media_instance_event_manager( p_mlp->p_mi, NULL ),
                         libvlc_MediaInstanceReachedEnd,
					     media_instance_reached_end, p_mlp, NULL );
}

/**************************************************************************
 *       Stop (Public)
 **************************************************************************/
static vlc_bool_t
libvlc_media_list_player_is_playing( libvlc_media_list_player_t * p_mlp,
                                    libvlc_exception_t * p_e )
{
    libvlc_exception_raise( p_e, "Unimplemented" );
    return 0;
}

/**************************************************************************
 *       Next (private)
 *
 * Playlist lock should be held
 **************************************************************************/
static void
media_list_player_set_next( libvlc_media_list_player_t * p_mlp, int index,
                            libvlc_exception_t * p_e )
{
	libvlc_media_descriptor_t * p_md;
	
	p_md = libvlc_media_list_item_at_index( p_mlp->p_mlist, index, p_e );
	if( !p_md )
	{
		libvlc_media_list_unlock( p_mlp->p_mlist );		
		if( !libvlc_exception_raised( p_e ) )
			libvlc_exception_raise( p_e, "Can't obtain a media" );
		return;
	}

    vlc_mutex_lock( &p_mlp->object_lock );
	
    p_mlp->i_current_playing_index = index;

	/* We are not interested in getting media_descriptor stop event now */
	uninstall_media_instance_observer( p_mlp );
    libvlc_media_instance_set_media_descriptor( p_mlp->p_mi, p_md, NULL );
//	wait_playing_state(); /* If we want to be synchronous */
	install_media_instance_observer( p_mlp );

    vlc_mutex_unlock( &p_mlp->object_lock );

	libvlc_media_list_unlock( p_mlp->p_mlist );		
	
	libvlc_media_descriptor_release( p_md ); /* for libvlc_media_list_item_at_index */
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
	p_mlp->i_current_playing_index = -1;
    p_mlp->p_mi = NULL;
    p_mlp->p_mlist = NULL;
    vlc_mutex_init( p_instance->p_libvlc_int, &p_mlp->object_lock );
    p_mlp->p_event_manager = libvlc_event_manager_new( p_mlp,
                                                       p_instance,
                                                       p_e );
    libvlc_event_manager_register_event_type( p_mlp->p_event_manager,
            libvlc_MediaListPlayerNextItemSet, p_e );
    libvlc_event_manager_register_event_type( p_mlp->p_event_manager,
            libvlc_MediaListPlayerNextItemSet, p_e );

	return p_mlp;
}

/**************************************************************************
 *         release (Public)
 **************************************************************************/
void libvlc_media_list_player_release( libvlc_media_list_player_t * p_mlp )
{
    libvlc_event_manager_release( p_mlib->p_event_manager );
    free(p_mlp);
}

/**************************************************************************
 *        set_media_instance (Public)
 **************************************************************************/
void libvlc_media_list_player_set_media_instance(
                                     libvlc_media_list_player_t * p_mlp,
                                     libvlc_media_instance_t * p_mi,
                                     libvlc_exception_t * p_e )
{
    vlc_mutex_lock( &p_mlp->object_lock );

	if( p_mlp->p_mi )
	{
		uninstall_media_instance_observer( p_mlp );
		libvlc_media_instance_release( p_mlp->p_mi );
	}
	libvlc_media_instance_retain( p_mi );
	p_mlp->p_mi = p_mi;

    install_media_instance_observer( p_mlp );

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
		libvlc_media_list_player_stop( p_mlp, p_e );

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
    if( p_mlp->i_current_playing_index < 0 )
    {
        libvlc_media_list_player_next( p_mlp, p_e );
        return; /* Will set to play */
    }

	libvlc_media_instance_play( p_mlp->p_mi, p_e );
}

/**************************************************************************
 *        Play item at index (Public)
 *
 * Playlist lock should be help
 **************************************************************************/
void libvlc_media_list_player_play_item_at_index(
                        libvlc_media_list_player_t * p_mlp,
                        int i_index,
                        libvlc_exception_t * p_e )
{
	media_list_player_set_next( p_mlp, i_index, p_e );

	if( libvlc_exception_raised( p_e ) )
		return;

    /* Send the next item event */
    libvlc_event_t event;
    event.type = libvlc_MediaListPlayerNextItemSet;
    libvlc_event_send( p_mlp->p_event_manager, &event );

	libvlc_media_instance_play( p_mlp->p_mi, p_e );
}


/**************************************************************************
 *       Stop (Public)
 **************************************************************************/
void libvlc_media_list_player_stop( libvlc_media_list_player_t * p_mlp,
                                    libvlc_exception_t * p_e )
{
	libvlc_media_instance_stop( p_mlp->p_mi, p_e );

    vlc_mutex_lock( &p_mlp->object_lock );
	p_mlp->i_current_playing_index = -1;
    vlc_mutex_unlock( &p_mlp->object_lock );
}

/**************************************************************************
 *       Next (Public)
 **************************************************************************/
void libvlc_media_list_player_next( libvlc_media_list_player_t * p_mlp,
                                    libvlc_exception_t * p_e )
{	
	int index;
	
	libvlc_media_list_lock( p_mlp->p_mlist );

	index = get_next_index( p_mlp );

	if( index < 0 )
	{
		libvlc_media_list_unlock( p_mlp->p_mlist );
		libvlc_exception_raise( p_e, "No more element to play" );
		libvlc_media_list_player_stop( p_mlp, p_e );
		return;
	}

	media_list_player_set_next( p_mlp, index, p_e );
	
    libvlc_media_instance_play( p_mlp->p_mi, p_e );

    libvlc_media_list_unlock( p_mlp->p_mlist );

    /* Send the next item event */
    libvlc_event_t event;
    event.type = libvlc_MediaListPlayerNextItemSet;
    libvlc_event_send( p_mlp->p_event_manager, &event);
}

