/*****************************************************************************
 * events.c: events interface
 * This library provides an interface to the send and receive events.
 * It is more lightweight than variable based callback.
 * Methode
 *****************************************************************************
 * Copyright (C) 1998-2005 the VideoLAN team
 * $Id$
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org >
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <vlc/vlc.h>

#include <stdio.h>                                               /* required */
#include <stdlib.h>                                              /* malloc() */

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
} vlc_event_listeners_group_t;

/*****************************************************************************
 * 
 *****************************************************************************/

/**
 * Initialize event manager object
 * p_this is the object that contains the event manager. But not
 * necessarily a vlc_object_t (an input_item_t is not a vlc_object_t
 * for instance).
 */
int event_manager_Create( vlc_event_manager_t * p_em, void * p_obj )
{
    p_em->p_obj = p_obj;
    ARRAY_INIT( p_em->listeners_groups );
    return VLC_SUCCESS;
}

/**
 * Destroy the event manager
 */
void event_manager_Destroy( vlc_event_manager_t * p_em )
{
    struct vlc_event_listeners_group_t * listeners_group;
    struct vlc_event_listener_t * listener;

    FOREACH_ARRAY( listeners_group, p_em->listeners_groups )
        FOREACH_ARRAY( listener, listeners_group->listeners )
            free( listener );
        FOREACH_END()
        free( listeners_group );
    FOREACH_END()
    free( p_em );
}

/**
 * Destroy the event manager
 */
int event_manager_RegisterEventType(
        vlc_event_manager_t * p_em,
        vlc_event_type_t event_type )
{
    vlc_event_listeners_group_t * listeners_group;
    listeners_group = malloc(sizeof(vlc_event_listeners_group_t));

    if( !listeners_group )
        return VLC_ENOMEM;

    listeners_group->event_type = event_type;
    ARRAY_INIT( listeners_group->listeners );

    ARRAY_APPEND( p_em->listeners_groups, listeners_group );

    return VLC_SUCCESS;
}

/**
 * Send an event to the listener attached to this p_em.
 */
void event_Send( vlc_event_manager_t * p_em,
                 vlc_event_t * p_event )
{
    vlc_event_listeners_group_t * listeners_group;
    vlc_event_listener_t * listener;

    /* Fill event with the sending object now */
    p_event->p_obj = p_em->p_obj;

    FOREACH_ARRAY( listeners_group, p_em->listeners_groups )
        if( listeners_group->event_type == p_event->type )
        {
            /* We found the group, now send every one the event */
            FOREACH_ARRAY( listener, listeners_group->listeners )
                listener->pf_callback( p_event, listener->p_user_data );
            FOREACH_END()
            break;
        }
    FOREACH_END()
}

/**
 * Add a callback for an event.
 */
int event_Attach( vlc_event_manager_t * p_event_manager,
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
    
    FOREACH_ARRAY( listeners_group, p_event_manager->listeners_groups )
        if( listeners_group->event_type == event_type )
        {
            ARRAY_APPEND( listeners_group->listeners, listener );
            return VLC_SUCCESS;
        }
    FOREACH_END()
    
    free(listener);
    return VLC_EGENERIC;
}

/**
 * Remove a callback for an event.
 */
int event_Detach( vlc_event_manager_t *p_event_manager,
                   vlc_event_type_t event_type,
                   vlc_event_callback_t pf_callback,
                   void *p_user_data )
{
    vlc_event_listeners_group_t * listeners_group;
    struct vlc_event_listener_t * listener;
    FOREACH_ARRAY( listeners_group, p_event_manager->listeners_groups )
        if( listeners_group->event_type == event_type )
        {
            FOREACH_ARRAY( listener, listeners_group->listeners )
                if( listener->pf_callback == pf_callback &&
                    listener->p_user_data == p_user_data )
                {
                    /* that's our listener */
                    free( listener );
                    ARRAY_REMOVE( listeners_group->listeners,
                        fe_idx /* This comes from the macro (and that's why
                                  I hate macro) */ );
                    return VLC_SUCCESS;
                }
            FOREACH_END()
        }
    FOREACH_END()

    return VLC_EGENERIC;
}


