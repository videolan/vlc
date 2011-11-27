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

typedef struct vlc_event_listeners_group_t
{
    vlc_event_type_t    event_type;
    DECL_ARRAY(struct vlc_event_listener_t *) listeners;

   /* Used in vlc_event_send() to make sure to behave
      Correctly when vlc_event_detach was called during
      a callback */
    bool          b_sublistener_removed;
                                         
} vlc_event_listeners_group_t;

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
    ARRAY_INIT( p_em->listeners_groups );
    return VLC_SUCCESS;
}

/**
 * Destroy the event manager
 */
void vlc_event_manager_fini( vlc_event_manager_t * p_em )
{
    struct vlc_event_listeners_group_t * listeners_group;
    struct vlc_event_listener_t * listener;

    vlc_mutex_destroy( &p_em->object_lock );
    vlc_mutex_destroy( &p_em->event_sending_lock );

    FOREACH_ARRAY( listeners_group, p_em->listeners_groups )
        FOREACH_ARRAY( listener, listeners_group->listeners )
            free( listener );
        FOREACH_END()
        ARRAY_RESET( listeners_group->listeners );
        free( listeners_group );
    FOREACH_END()
    ARRAY_RESET( p_em->listeners_groups );
}

/**
 * Register the event manager
 */
int vlc_event_manager_register_event_type(
        vlc_event_manager_t * p_em,
        vlc_event_type_t event_type )
{
    vlc_event_listeners_group_t * listeners_group;
    listeners_group = malloc(sizeof(vlc_event_listeners_group_t));

    if( !listeners_group )
        return VLC_ENOMEM;

    listeners_group->event_type = event_type;
    ARRAY_INIT( listeners_group->listeners );
 
    vlc_mutex_lock( &p_em->object_lock );
    ARRAY_APPEND( p_em->listeners_groups, listeners_group );
    vlc_mutex_unlock( &p_em->object_lock );

    return VLC_SUCCESS;
}

/**
 * Send an event to the listener attached to this p_em.
 */
void vlc_event_send( vlc_event_manager_t * p_em,
                     vlc_event_t * p_event )
{
    vlc_event_listeners_group_t * listeners_group = NULL;
    vlc_event_listener_t * listener;
    vlc_event_listener_t * array_of_cached_listeners = NULL;
    vlc_event_listener_t * cached_listener;
    int i, i_cached_listeners = 0;

    /* Fill event with the sending object now */
    p_event->p_obj = p_em->p_obj;

    vlc_mutex_lock( &p_em->event_sending_lock ) ;
    vlc_mutex_lock( &p_em->object_lock );

    FOREACH_ARRAY( listeners_group, p_em->listeners_groups )
        if( listeners_group->event_type == p_event->type )
        {
            if( listeners_group->listeners.i_size <= 0 )
                break;

            /* Save the function to call */
            i_cached_listeners = listeners_group->listeners.i_size;
            array_of_cached_listeners = malloc(
                    sizeof(vlc_event_listener_t)*i_cached_listeners );
            if( !array_of_cached_listeners )
            {
                vlc_mutex_unlock( &p_em->object_lock );
                vlc_mutex_unlock( &p_em->event_sending_lock ) ;
                return;
            }

            cached_listener = array_of_cached_listeners;
            FOREACH_ARRAY( listener, listeners_group->listeners )
                memcpy( cached_listener, listener, sizeof(vlc_event_listener_t));
                cached_listener++;
            FOREACH_END()

            break;
        }
    FOREACH_END()

    /* Track item removed from *this* thread, with a simple flag. Indeed
     * event_sending_lock is a recursive lock. This has the advantage of
     * allowing to remove an event listener from within a callback */
    listeners_group->b_sublistener_removed = false;

    vlc_mutex_unlock( &p_em->object_lock );

    /* Call the function attached */
    cached_listener = array_of_cached_listeners;

    if( !listeners_group || !array_of_cached_listeners )
    {
        free( array_of_cached_listeners );
        vlc_mutex_unlock( &p_em->event_sending_lock );
        return;
    }


    for( i = 0; i < i_cached_listeners; i++ )
    {
        if( listeners_group->b_sublistener_removed )
        {
            /* If a callback was removed inside one of our callback, this gets
	     * called */
            bool valid_listener;
            vlc_mutex_lock( &p_em->object_lock );
            valid_listener = group_contains_listener( listeners_group, cached_listener );
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
    vlc_event_listeners_group_t * listeners_group;
    vlc_event_listener_t * listener;
    listener = malloc(sizeof(vlc_event_listener_t));
    if( !listener )
        return VLC_ENOMEM;
 
    listener->p_user_data = p_user_data;
    listener->pf_callback = pf_callback;

    vlc_mutex_lock( &p_em->object_lock );
    FOREACH_ARRAY( listeners_group, p_em->listeners_groups )
        if( listeners_group->event_type == event_type )
        {
            ARRAY_APPEND( listeners_group->listeners, listener );
            vlc_mutex_unlock( &p_em->object_lock );
            return VLC_SUCCESS;
        }
    FOREACH_END()
    /* Unknown event = BUG */
    assert( 0 );
}

/**
 * Remove a callback for an event.
 */

void vlc_event_detach( vlc_event_manager_t *p_em,
                       vlc_event_type_t event_type,
                       vlc_event_callback_t pf_callback,
                       void *p_user_data )
{
    vlc_event_listeners_group_t * listeners_group;
    struct vlc_event_listener_t * listener;

    vlc_mutex_lock( &p_em->event_sending_lock );
    vlc_mutex_lock( &p_em->object_lock );
    FOREACH_ARRAY( listeners_group, p_em->listeners_groups )
        if( listeners_group->event_type == event_type )
        {
            FOREACH_ARRAY( listener, listeners_group->listeners )
                if( listener->pf_callback == pf_callback &&
                    listener->p_user_data == p_user_data )
                {
                    /* Tell vlc_event_send, we did remove an item from that group,
                       in case vlc_event_send is in our caller stack  */
                    listeners_group->b_sublistener_removed = true;

                    /* that's our listener */
                    ARRAY_REMOVE( listeners_group->listeners,
                        fe_idx /* This comes from the macro (and that's why
                                  I hate macro) */ );
                    free( listener );
                    vlc_mutex_unlock( &p_em->event_sending_lock );
                    vlc_mutex_unlock( &p_em->object_lock );
                    return;
                }
            FOREACH_END()
        }
    FOREACH_END()

    assert( 0 );
}
