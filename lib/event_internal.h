/*****************************************************************************
 * event_internal.h : Definition of opaque structures for libvlc exported API
 * Also contains some internal utility functions
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef _LIBVLC_EVENT_H
#define _LIBVLC_EVENT_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/libvlc_structures.h>
#include <vlc/libvlc.h>
#include <vlc/libvlc_events.h>

#include <vlc_common.h>


/*
 * Event Handling
 */

/* Example usage
 *
 * struct libvlc_cool_object_t
 * {
 *        ...
 *        libvlc_event_manager_t * p_event_manager;
 *        ...
 * }
 *
 * libvlc_my_cool_object_new()
 * {
 *        ...
 *        p_self->p_event_manager = libvlc_event_manager_new( p_self,
 *                                                   p_self->p_libvlc_instance, p_e);
 *        libvlc_event_manager_register_event_type(p_self->p_event_manager,
 *                libvlc_MyCoolObjectDidSomething, p_e)
 *        ...
 * }
 *
 * libvlc_my_cool_object_release()
 * {
 *         ...
 *         libvlc_event_manager_release( p_self->p_event_manager );
 *         ...
 * }
 *
 * libvlc_my_cool_object_do_something()
 * {
 *        ...
 *        libvlc_event_t event;
 *        event.type = libvlc_MyCoolObjectDidSomething;
 *        event.u.my_cool_object_did_something.what_it_did = kSomething;
 *        libvlc_event_send( p_self->p_event_manager, &event );
 * }
 * */

typedef struct libvlc_event_listener_t
{
    libvlc_event_type_t event_type;
    void *              p_user_data;
    libvlc_callback_t   pf_callback;
    bool                is_asynchronous;
} libvlc_event_listener_t;

typedef struct libvlc_event_manager_t
{
    void * p_obj;
    struct libvlc_instance_t * p_libvlc_instance;
    vlc_array_t listeners_groups;
    vlc_mutex_t object_lock;
    vlc_mutex_t event_sending_lock;
    struct libvlc_event_async_queue * async_event_queue;
} libvlc_event_sender_t;


static inline bool
listeners_are_equal( libvlc_event_listener_t * listener1,
                    libvlc_event_listener_t * listener2 )
{
    return listener1->event_type  == listener2->event_type &&
    listener1->pf_callback == listener2->pf_callback &&
    listener1->p_user_data == listener2->p_user_data &&
    listener1->is_asynchronous == listener2->is_asynchronous;
}

/* event_async.c */
void libvlc_event_async_fini(libvlc_event_manager_t * p_em);
void libvlc_event_async_dispatch(libvlc_event_manager_t * p_em, libvlc_event_listener_t * listener, libvlc_event_t * event);
void libvlc_event_async_ensure_listener_removal(libvlc_event_manager_t * p_em, libvlc_event_listener_t * listener);

#endif
