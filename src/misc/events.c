/*****************************************************************************
 * events.c: events interface
 * This library provides an interface to the send and receive events.
 * It is more lightweight than variable based callback.
 *****************************************************************************
 * Copyright (C) 1998-2005 VLC authors and VideoLAN
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org >
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include <assert.h>

#include <vlc_events.h>
#include <vlc_arrays.h>

/*****************************************************************************
 * Documentation : Read vlc_events.h
 *****************************************************************************/

/*****************************************************************************
 *  Private types.
 *****************************************************************************/

typedef struct vlc_event_listener_t
{
    void *               p_user_data;
    vlc_event_callback_t pf_callback;
} vlc_event_listener_t;

/*****************************************************************************
 *
 *****************************************************************************/

#undef vlc_event_manager_init
/**
 * Initialize event manager object
 * p_obj is the object that contains the event manager. But not
 * necessarily a vlc_object_t (an input_item_t is not a vlc_object_t
 * for instance).
 */
void vlc_event_manager_init( vlc_event_manager_t * p_em, void * p_obj )
{
    p_em->p_obj = p_obj;
    /* This is an unsafe work-around for a long-standing playlist bug.
     * Do not rely on this. */
    vlc_mutex_init_recursive( &p_em->lock );

    for( size_t i = 0; i < ARRAY_SIZE(p_em->events); i++ )
       ARRAY_INIT( p_em->events[i].listeners );
}

/**
 * Destroy the event manager
 */
void vlc_event_manager_fini( vlc_event_manager_t * p_em )
{
    struct vlc_event_listener_t * listener;

    for( size_t i = 0; i < ARRAY_SIZE(p_em->events); i++ )
    {
        struct vlc_event_listeners_group_t *slot = p_em->events + i;

        ARRAY_FOREACH( listener, slot->listeners )
            free( listener );

        ARRAY_RESET( slot->listeners );
    }
}

/**
 * Send an event to the listener attached to this p_em.
 */
void vlc_event_send( vlc_event_manager_t * p_em,
                     vlc_event_t * p_event )
{
    vlc_event_listeners_group_t *slot = &p_em->events[p_event->type];
    vlc_event_listener_t * listener;

    /* Fill event with the sending object now */
    p_event->p_obj = p_em->p_obj;

    vlc_mutex_lock( &p_em->lock ) ;

    ARRAY_FOREACH( listener, slot->listeners )
        listener->pf_callback( p_event, listener->p_user_data );

    vlc_mutex_unlock( &p_em->lock );
}

#undef vlc_event_attach
/**
 * Add a callback for an event.
 */
int vlc_event_attach( vlc_event_manager_t * p_em,
                      vlc_event_type_t event_type,
                      vlc_event_callback_t pf_callback,
                      void *p_user_data )
{
    vlc_event_listener_t * listener;
    vlc_event_listeners_group_t *slot = &p_em->events[event_type];

    listener = malloc(sizeof(vlc_event_listener_t));
    if( !listener )
        return VLC_ENOMEM;

    listener->p_user_data = p_user_data;
    listener->pf_callback = pf_callback;

    vlc_mutex_lock( &p_em->lock );
    ARRAY_APPEND( slot->listeners, listener );
    vlc_mutex_unlock( &p_em->lock );
    return VLC_SUCCESS;
}

/**
 * Remove a callback for an event.
 */

void vlc_event_detach( vlc_event_manager_t *p_em,
                       vlc_event_type_t event_type,
                       vlc_event_callback_t pf_callback,
                       void *p_user_data )
{
    vlc_event_listeners_group_t *slot = &p_em->events[event_type];

    vlc_mutex_lock( &p_em->lock );

    for (int i = 0; i < slot->listeners.i_size; ++i)
    {
        struct vlc_event_listener_t *listener = slot->listeners.p_elems[i];
        if( listener->pf_callback == pf_callback &&
            listener->p_user_data == p_user_data )
        {
            /* that's our listener */
            ARRAY_REMOVE( slot->listeners, i );
            vlc_mutex_unlock( &p_em->lock );
            free( listener );
            return;
        }
    }

    vlc_assert_unreachable();
}
