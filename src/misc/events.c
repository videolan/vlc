/*****************************************************************************
 * events.c: events interface
 * This library provides an interface to the send and receive events.
 * It is more lightweight than variable based callback.
 *****************************************************************************
 * Copyright (C) 1998-2005 VLC authors and VideoLAN
 * $Id$
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

static bool
listeners_are_equal( vlc_event_listener_t * listener1,
                     vlc_event_listener_t * listener2 )
{
    return listener1->pf_callback == listener2->pf_callback &&
           listener1->p_user_data == listener2->p_user_data;
}

static bool
group_contains_listener( vlc_event_listeners_group_t * group,
                         vlc_event_listener_t * searched_listener )
{
    vlc_event_listener_t * listener;
    FOREACH_ARRAY( listener, group->listeners )
        if( listeners_are_equal(searched_listener, listener) )
            return true;
    FOREACH_END()
    return false;
}

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
int vlc_event_manager_init( vlc_event_manager_t * p_em, void * p_obj )
{
    p_em->p_obj = p_obj;
    vlc_mutex_init( &p_em->object_lock );

    /* We need a recursive lock here, because we need to be able
     * to call libvlc_event_detach even if vlc_event_send is in
     * the call frame.
     * This ensures that after libvlc_event_detach, the callback
     * will never gets triggered.
     * */
    vlc_mutex_init_recursive( &p_em->event_sending_lock );

    for( size_t i = 0; i < ARRAY_SIZE(p_em->events); i++ )
       ARRAY_INIT( p_em->events[i].listeners );

    return VLC_SUCCESS;
}

/**
 * Destroy the event manager
 */
void vlc_event_manager_fini( vlc_event_manager_t * p_em )
{
    struct vlc_event_listener_t * listener;

    vlc_mutex_destroy( &p_em->object_lock );
    vlc_mutex_destroy( &p_em->event_sending_lock );

    for( size_t i = 0; i < ARRAY_SIZE(p_em->events); i++ )
    {
        struct vlc_event_listeners_group_t *slot = p_em->events + i;

        FOREACH_ARRAY( listener, slot->listeners )
            free( listener );
        FOREACH_END()
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
    vlc_event_listener_t * array_of_cached_listeners = NULL;
    vlc_event_listener_t * cached_listener;

    int i, i_cached_listeners = 0;

    /* Fill event with the sending object now */
    p_event->p_obj = p_em->p_obj;

    vlc_mutex_lock( &p_em->event_sending_lock ) ;
    vlc_mutex_lock( &p_em->object_lock );

    if( slot->listeners.i_size <= 0 )
    {
        vlc_mutex_unlock( &p_em->object_lock );
        vlc_mutex_unlock( &p_em->event_sending_lock ) ;
        return;
    }

    /* Save the function to call */
    i_cached_listeners = slot->listeners.i_size;
    array_of_cached_listeners = malloc(
                    sizeof(vlc_event_listener_t)*i_cached_listeners );
    if( unlikely(!array_of_cached_listeners) )
        abort();

    cached_listener = array_of_cached_listeners;
    FOREACH_ARRAY( listener, slot->listeners )
        memcpy( cached_listener, listener, sizeof(vlc_event_listener_t) );
        cached_listener++;
    FOREACH_END()

    /* Track item removed from *this* thread, with a simple flag. Indeed
     * event_sending_lock is a recursive lock. This has the advantage of
     * allowing to remove an event listener from within a callback */
    slot->b_sublistener_removed = false;

    vlc_mutex_unlock( &p_em->object_lock );

    /* Call the function attached */
    cached_listener = array_of_cached_listeners;

    for( i = 0; i < i_cached_listeners; i++ )
    {
        if( slot->b_sublistener_removed )
        {
            /* If a callback was removed inside one of our callback, this gets
             * called */
            bool valid_listener;
            vlc_mutex_lock( &p_em->object_lock );
            valid_listener = group_contains_listener( slot, cached_listener );
            vlc_mutex_unlock( &p_em->object_lock );
            if( !valid_listener )
            {
                cached_listener++;
                continue;
            }
        }
        cached_listener->pf_callback( p_event, cached_listener->p_user_data );
        cached_listener++;
    }
    vlc_mutex_unlock( &p_em->event_sending_lock );

    free( array_of_cached_listeners );
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

    vlc_mutex_lock( &p_em->object_lock );
    ARRAY_APPEND( slot->listeners, listener );
    vlc_mutex_unlock( &p_em->object_lock );
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
    struct vlc_event_listener_t * listener;

    vlc_mutex_lock( &p_em->event_sending_lock );
    vlc_mutex_lock( &p_em->object_lock );

    FOREACH_ARRAY( listener, slot->listeners )
        if( listener->pf_callback == pf_callback &&
            listener->p_user_data == p_user_data )
        {
            /* Tell vlc_event_send, we did remove an item from that group,
               in case vlc_event_send is in our caller stack  */
            slot->b_sublistener_removed = true;

            /* that's our listener */
            ARRAY_REMOVE( slot->listeners,
                          fe_idx /* This comes from the macro (and that's why
                                    I hate macro) */ );
            free( listener );
            vlc_mutex_unlock( &p_em->event_sending_lock );
            vlc_mutex_unlock( &p_em->object_lock );
            return;
        }
    FOREACH_END()

    vlc_assert_unreachable();
}
