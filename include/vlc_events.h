/*****************************************************************************
 * vlc_events.h: events definitions
 * Interface used to send events.
 *****************************************************************************
 * Copyright (C) 2007 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Pierre d'Herbemont
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

#ifndef VLC_EVENTS_H
# define VLC_EVENTS_H

#include <vlc_arrays.h>
#include <vlc_meta.h>

/**
 * \file
 * This file is the interface definition for events
 * (implementation in src/misc/events.c)
 */

/*****************************************************************************
 * Documentation
 *****************************************************************************/
/*
 **** Background
 *
 * This implements a way to send and receive event for an object (which can be
 * a simple C struct or less).
 *
 * This is in direct concurrency with the Variable based Callback
 * (see src/misc/variables.c).
 *
 * It has the following advantages over Variable based Callback:
 * - No need to implement the whole VLC_COMMON_MEMBERS in the object,
 * thus it reduce it size. This is especially true for input_item_t which
 * doesn't have VLC_COMMON_MEMBERS. This is the first reason of existence of
 * this implementation.
 * - Libvlc can easily be based upon that.
 * - Existing event are clearly declared (in include/vlc_events.h)
 *
 *
 **** Example usage
 *
 * (vlc_cool_object_t doesn't need to have the VLC_COMMON_MEMBERS.)
 *
 * struct vlc_cool_object_t
 * {
 *        ...
 *        vlc_event_manager_t p_event_manager;
 *        ...
 * }
 *
 * vlc_my_cool_object_new()
 * {
 *        ...
 *        vlc_event_manager_init( &p_self->p_event_manager, p_self, p_a_libvlc_object );
 *        vlc_event_manager_register_event_type(p_self->p_event_manager,
 *                vlc_MyCoolObjectDidSomething, p_e)
 *        ...
 * }
 *
 * vlc_my_cool_object_release()
 * {
 *         ...
 *         vlc_event_manager_fini( &p_self->p_event_manager );
 *         ...
 * }
 *
 * vlc_my_cool_object_do_something()
 * {
 *        ...
 *        vlc_event_t event;
 *        event.type = vlc_MyCoolObjectDidSomething;
 *        event.u.my_cool_object_did_something.what_it_did = kSomething;
 *        vlc_event_send( &p_self->p_event_manager, &event );
 * }
 * */

  /*****************************************************************************
 * Event Type
 *****************************************************************************/

/* Private structure defined in misc/events.c */
struct vlc_event_listeners_group_t;

/* Event manager type */
typedef struct vlc_event_manager_t
{
    void * p_obj;
    vlc_mutex_t object_lock;
    vlc_mutex_t event_sending_lock;
    DECL_ARRAY(struct vlc_event_listeners_group_t *) listeners_groups;
} vlc_event_manager_t;

/* List of event */
typedef enum vlc_event_type_t {
    /* Input (thread) events */
    vlc_InputStateChanged,
    vlc_InputSelectedStreamChanged,

    /* Input item events */
    vlc_InputItemMetaChanged,
    vlc_InputItemSubItemAdded,
    vlc_InputItemSubItemTreeAdded,
    vlc_InputItemDurationChanged,
    vlc_InputItemPreparsedChanged,
    vlc_InputItemNameChanged,
    vlc_InputItemInfoChanged,
    vlc_InputItemErrorWhenReadingChanged,

    /* Service Discovery event */
    vlc_ServicesDiscoveryItemAdded,
    vlc_ServicesDiscoveryItemRemoved,
    vlc_ServicesDiscoveryItemRemoveAll,
    vlc_ServicesDiscoveryStarted,
    vlc_ServicesDiscoveryEnded
} vlc_event_type_t;

/* Event definition */
typedef struct vlc_event_t
{
    vlc_event_type_t type;
    void * p_obj; /* Sender object, automatically filled by vlc_event_send() */
    union vlc_event_type_specific
    {
        /* Input (thread) events */
        struct vlc_input_state_changed
        {
            int new_state;
        } input_state_changed;
        struct vlc_input_selected_stream_changed
        {
            void * unused;
        } input_selected_stream_changed;

        /* Input item events */
        struct vlc_input_item_meta_changed
        {
            vlc_meta_type_t meta_type;
        } input_item_meta_changed;
        struct vlc_input_item_subitem_added
        {
            input_item_t * p_new_child;
        } input_item_subitem_added;
        struct vlc_input_item_subitem_tree_added
        {
            input_item_node_t * p_root;
        } input_item_subitem_tree_added;
        struct vlc_input_item_duration_changed
        {
            mtime_t new_duration;
        } input_item_duration_changed;
        struct vlc_input_item_preparsed_changed
        {
            int new_status;
        } input_item_preparsed_changed;
        struct vlc_input_item_name_changed
        {
            const char * new_name;
        } input_item_name_changed;
        struct vlc_input_item_info_changed
        {
            void * unused;
        } input_item_info_changed;
        struct input_item_error_when_reading_changed
        {
            bool new_value;
        } input_item_error_when_reading_changed;

        /* Service discovery events */
        struct vlc_services_discovery_item_added
        {
            input_item_t * p_new_item;
            const char * psz_category;
        } services_discovery_item_added;
        struct vlc_services_discovery_item_removed
        {
            input_item_t * p_item;
        } services_discovery_item_removed;
        struct vlc_services_discovery_started
        {
            void * unused;
        } services_discovery_started;
        struct vlc_services_discovery_ended
        {
            void * unused;
        } services_discovery_ended;

    } u;
} vlc_event_t;

/* Event callback type */
typedef void ( *vlc_event_callback_t )( const vlc_event_t *, void * );

 /*****************************************************************************
 * Event manager
 *****************************************************************************/

/*
 * p_obj points to the object that owns the event manager, and from
 * which events are sent
 */
VLC_API int vlc_event_manager_init( vlc_event_manager_t * p_em, void * p_obj );

/*
 * Destroy
 */
VLC_API void vlc_event_manager_fini( vlc_event_manager_t * p_em );

/*
 * Tells a specific event manager that it will handle event_type object
 */
VLC_API int vlc_event_manager_register_event_type( vlc_event_manager_t * p_em,
                                                   vlc_event_type_t );

/*
 * Send an event to the listener attached to this p_em.
 */
VLC_API void vlc_event_send( vlc_event_manager_t * p_em, vlc_event_t * );

/*
 * Add a callback for an event.
 */
VLC_API int vlc_event_attach( vlc_event_manager_t * p_event_manager,
                              vlc_event_type_t event_type,
                              vlc_event_callback_t pf_callback,
                              void *p_user_data );

/*
 * Remove a callback for an event.
 */
VLC_API void vlc_event_detach( vlc_event_manager_t *p_event_manager,
                               vlc_event_type_t event_type,
                               vlc_event_callback_t pf_callback,
                               void *p_user_data );

#endif /* VLC_EVENTS_H */
