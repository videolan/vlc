/*****************************************************************************
 * event.c: New libvlc event control API
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id $
 *
 * Authors: Filippo Carone <filippo@carone.org>
 *          Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include <vlc_playlist.h>

/*
 * Private functions
 */

static bool
listeners_are_equal( libvlc_event_listener_t * listener1,
                     libvlc_event_listener_t * listener2 )
{
    return listener1->event_type  == listener2->event_type &&
           listener1->pf_callback == listener2->pf_callback &&
           listener1->p_user_data == listener2->p_user_data;
}

static bool
group_contains_listener( libvlc_event_listeners_group_t * group,
                         libvlc_event_listener_t * searched_listener )
{
    int i;
    for( i = 0; i < vlc_array_count(&group->listeners); i++ )
    {
        if( listeners_are_equal(searched_listener, vlc_array_item_at_index(&group->listeners, i)) )
            return true;
    }
    return false;
}

/*
 * Internal libvlc functions
 */

/**************************************************************************
 *       libvlc_event_manager_init (internal) :
 *
 * Init an object's event manager.
 **************************************************************************/
libvlc_event_manager_t *
libvlc_event_manager_new( void * p_obj, libvlc_instance_t * p_libvlc_inst,
                           libvlc_exception_t *p_e )
{
    libvlc_event_manager_t * p_em;

    p_em = malloc(sizeof( libvlc_event_manager_t ));
    if( !p_em )
    {
        libvlc_exception_raise( p_e, "No Memory left" );
        return NULL;
    }

    p_em->p_obj = p_obj;
    p_em->p_libvlc_instance = p_libvlc_inst;
    libvlc_retain( p_libvlc_inst );
    vlc_array_init( &p_em->listeners_groups );
    vlc_mutex_init( &p_em->object_lock );
    vlc_mutex_init_recursive( &p_em->event_sending_lock );
    return p_em;
}

/**************************************************************************
 *       libvlc_event_manager_release (internal) :
 *
 * Init an object's event manager.
 **************************************************************************/
void libvlc_event_manager_release( libvlc_event_manager_t * p_em )
{
    libvlc_event_listeners_group_t * p_lg;
    int i,j ;

    vlc_mutex_destroy( &p_em->event_sending_lock );
    vlc_mutex_destroy( &p_em->object_lock );

    for( i = 0; i < vlc_array_count(&p_em->listeners_groups); i++)
    {
        p_lg = vlc_array_item_at_index( &p_em->listeners_groups, i );

        for( j = 0; j < vlc_array_count(&p_lg->listeners); j++)
            free( vlc_array_item_at_index( &p_lg->listeners, j ) );

        vlc_array_clear( &p_lg->listeners );
        free( p_lg );
    }
    vlc_array_clear( &p_em->listeners_groups );
    libvlc_release( p_em->p_libvlc_instance );
    free( p_em );
}

/**************************************************************************
 *       libvlc_event_manager_register_event_type (internal) :
 *
 * Init an object's event manager.
 **************************************************************************/
void libvlc_event_manager_register_event_type(
        libvlc_event_manager_t * p_em,
        libvlc_event_type_t event_type,
        libvlc_exception_t * p_e )
{
    libvlc_event_listeners_group_t * listeners_group;
    listeners_group = malloc(sizeof(libvlc_event_listeners_group_t));
    if( !listeners_group )
    {
        libvlc_exception_raise( p_e, "No Memory left" );
        return;
    }

    listeners_group->event_type = event_type;
    vlc_array_init( &listeners_group->listeners );

    vlc_mutex_lock( &p_em->object_lock );
    vlc_array_append( &p_em->listeners_groups, listeners_group );
    vlc_mutex_unlock( &p_em->object_lock );
}

/**************************************************************************
 *       libvlc_event_send (internal) :
 *
 * Send a callback.
 **************************************************************************/
void libvlc_event_send( libvlc_event_manager_t * p_em,
                        libvlc_event_t * p_event )
{
    libvlc_event_listeners_group_t * listeners_group = NULL;
    libvlc_event_listener_t * listener_cached;
    libvlc_event_listener_t * listener;
    libvlc_event_listener_t * array_listeners_cached = NULL;
    int i, i_cached_listeners = 0;

    /* Fill event with the sending object now */
    p_event->p_obj = p_em->p_obj;

    /* Here a read/write lock would be nice */

    vlc_mutex_lock( &p_em->object_lock );
    for( i = 0; i < vlc_array_count(&p_em->listeners_groups); i++)
    {
        listeners_group = vlc_array_item_at_index(&p_em->listeners_groups, i);
        if( listeners_group->event_type == p_event->type )
        {
            if( vlc_array_count( &listeners_group->listeners ) <= 0 )
                break;

            /* Cache a copy of the listener to avoid locking issues */
            i_cached_listeners = vlc_array_count(&listeners_group->listeners);
            array_listeners_cached = malloc(sizeof(libvlc_event_listener_t)*(i_cached_listeners));
            if( !array_listeners_cached )
            {
                printf( "Can't alloc memory in libvlc_event_send" );
                break;
            }

            listener_cached = array_listeners_cached;
            for( i = 0; i < vlc_array_count(&listeners_group->listeners); i++)
            {
                listener = vlc_array_item_at_index(&listeners_group->listeners, i);
                memcpy( listener_cached, listener, sizeof(libvlc_event_listener_t) );
                listener_cached++;
            }
            break;
        }
    }

    if( !listeners_group )
    {
        free( array_listeners_cached );
        return;
    }

    vlc_mutex_unlock( &p_em->object_lock );

    vlc_mutex_lock( &p_em->event_sending_lock );
    listener_cached = array_listeners_cached;
    listeners_group->b_sublistener_removed = false;
    for( i = 0; i < i_cached_listeners; i++ )
    {
        if( listeners_group->b_sublistener_removed )
        {
            /* If a callback was removed, this gets called */
            bool valid_listener;
            vlc_mutex_lock( &p_em->object_lock );
            valid_listener = group_contains_listener( listeners_group, listener_cached );
            vlc_mutex_unlock( &p_em->object_lock );
            if( !valid_listener )
            {
                listener_cached++;
                continue;
            }
        }

        listener_cached->pf_callback( p_event, listener_cached->p_user_data );
        listener_cached++;
    }
    vlc_mutex_unlock( &p_em->event_sending_lock );

    free( array_listeners_cached );
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       libvlc_event_type_name (public) :
 *
 * Get the char * name of an event type.
 **************************************************************************/
static const char event_type_to_name[][35] =
{
#define EVENT(a) [a]=#a
    EVENT(libvlc_MediaMetaChanged),
    EVENT(libvlc_MediaSubItemAdded),
    EVENT(libvlc_MediaDurationChanged),
    EVENT(libvlc_MediaPreparsedChanged),
    EVENT(libvlc_MediaFreed),
    EVENT(libvlc_MediaStateChanged),

    EVENT(libvlc_MediaPlayerNothingSpecial),
    EVENT(libvlc_MediaPlayerOpening),
    EVENT(libvlc_MediaPlayerBuffering),
    EVENT(libvlc_MediaPlayerPlaying),
    EVENT(libvlc_MediaPlayerPaused),
    EVENT(libvlc_MediaPlayerStopped),
    EVENT(libvlc_MediaPlayerForward),
    EVENT(libvlc_MediaPlayerBackward),
    EVENT(libvlc_MediaPlayerEndReached),
    EVENT(libvlc_MediaPlayerTimeChanged),
    EVENT(libvlc_MediaPlayerPositionChanged),
    EVENT(libvlc_MediaPlayerSeekableChanged),
    EVENT(libvlc_MediaPlayerPausableChanged),

    EVENT(libvlc_MediaListItemAdded),
    EVENT(libvlc_MediaListWillAddItem),
    EVENT(libvlc_MediaListItemDeleted),
    EVENT(libvlc_MediaListWillDeleteItem),

    EVENT(libvlc_MediaListViewItemAdded),
    EVENT(libvlc_MediaListViewWillAddItem),
    EVENT(libvlc_MediaListViewItemDeleted),
    EVENT(libvlc_MediaListViewWillDeleteItem),

    EVENT(libvlc_MediaListPlayerPlayed),
    EVENT(libvlc_MediaListPlayerNextItemSet),
    EVENT(libvlc_MediaListPlayerStopped),

    EVENT(libvlc_MediaDiscovererStarted),
    EVENT(libvlc_MediaDiscovererEnded)
#undef EVENT
};

static const char unkwown_event_name[] = "Unknown Event";

const char * libvlc_event_type_name( libvlc_event_type_t event_type )
{
    if( event_type >= sizeof(event_type_to_name)/sizeof(event_type_to_name[0]))
        return unkwown_event_name;
    return event_type_to_name[event_type];
}

/**************************************************************************
 *       libvlc_event_attach (public) :
 *
 * Add a callback for an event.
 **************************************************************************/
void libvlc_event_attach( libvlc_event_manager_t * p_event_manager,
                          libvlc_event_type_t event_type,
                          libvlc_callback_t pf_callback,
                          void *p_user_data,
                          libvlc_exception_t *p_e )
{
    libvlc_event_listeners_group_t * listeners_group;
    libvlc_event_listener_t * listener;
    int i;

    listener = malloc(sizeof(libvlc_event_listener_t));
    if( !listener )
    {
        libvlc_exception_raise( p_e, "No Memory left" );
        return;
    }

    listener->event_type = event_type;
    listener->p_user_data = p_user_data;
    listener->pf_callback = pf_callback;

    vlc_mutex_lock( &p_event_manager->object_lock );
    for( i = 0; i < vlc_array_count(&p_event_manager->listeners_groups); i++ )
    {
        listeners_group = vlc_array_item_at_index(&p_event_manager->listeners_groups, i);
        if( listeners_group->event_type == listener->event_type )
        {
            vlc_array_append( &listeners_group->listeners, listener );
            vlc_mutex_unlock( &p_event_manager->object_lock );
            return;
        }
    }
    vlc_mutex_unlock( &p_event_manager->object_lock );

    free(listener);
    libvlc_exception_raise( p_e,
            "This object event manager doesn't know about '%s' events",
            libvlc_event_type_name(event_type));
}

/**************************************************************************
 *       libvlc_event_detach (public) :
 *
 * Remove a callback for an event.
 **************************************************************************/
void libvlc_event_detach( libvlc_event_manager_t *p_event_manager,
                                     libvlc_event_type_t event_type,
                                     libvlc_callback_t pf_callback,
                                     void *p_user_data,
                                     libvlc_exception_t *p_e )
{
    libvlc_event_listeners_group_t * listeners_group;
    libvlc_event_listener_t * listener;
    int i, j;

    vlc_mutex_lock( &p_event_manager->event_sending_lock );
    vlc_mutex_lock( &p_event_manager->object_lock );
    for( i = 0; i < vlc_array_count(&p_event_manager->listeners_groups); i++)
    {
        listeners_group = vlc_array_item_at_index(&p_event_manager->listeners_groups, i);
        if( listeners_group->event_type == event_type )
        {
            for( j = 0; j < vlc_array_count(&listeners_group->listeners); j++)
            {
                listener = vlc_array_item_at_index(&listeners_group->listeners, j);
                if( listener->event_type == event_type &&
                    listener->pf_callback == pf_callback &&
                    listener->p_user_data == p_user_data )
                {
                    /* that's our listener */

                    /* Mark this group as edited so that libvlc_event_send
                     * will recheck what listener to call */
                    listeners_group->b_sublistener_removed = false;

                    free( listener );
                    vlc_array_remove( &listeners_group->listeners, j );
                    vlc_mutex_unlock( &p_event_manager->object_lock );
                    vlc_mutex_unlock( &p_event_manager->event_sending_lock );
                    return;
                }
            }
        }
    }
    vlc_mutex_unlock( &p_event_manager->object_lock );
    vlc_mutex_unlock( &p_event_manager->event_sending_lock );

    libvlc_exception_raise( p_e,
            "This object event manager doesn't know about '%s,%p,%p' event observer",
            libvlc_event_type_name(event_type), pf_callback, p_user_data );
}
