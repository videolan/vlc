/*****************************************************************************
 * libvlc_internal.h : Definition of opaque structures for libvlc exported API
 * Also contains some internal utility functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifndef _LIBVLC_INTERNAL_H
#define _LIBVLC_INTERNAL_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc/libvlc_structures.h>

#include <vlc_common.h>
#include <vlc_arrays.h>
#include <vlc_input.h>

/***************************************************************************
 * Internal creation and destruction functions
 ***************************************************************************/
VLC_EXPORT (libvlc_int_t *, libvlc_InternalCreate, ( void ) );
VLC_EXPORT (int, libvlc_InternalInit, ( libvlc_int_t *, int, const char *ppsz_argv[] ) );
VLC_EXPORT (void, libvlc_InternalCleanup, ( libvlc_int_t * ) );
VLC_EXPORT (void, libvlc_InternalDestroy, ( libvlc_int_t * ) );

VLC_EXPORT (int, libvlc_InternalAddIntf, ( libvlc_int_t *, const char * ) );
VLC_EXPORT (void, libvlc_InternalWait, ( libvlc_int_t * ) );

/***************************************************************************
 * Opaque structures for libvlc API
 ***************************************************************************/

typedef int * libvlc_media_list_path_t; /* (Media List Player Internal) */

typedef enum libvlc_lock_state_t
{
    libvlc_Locked,
    libvlc_UnLocked
} libvlc_lock_state_t;

struct libvlc_instance_t
{
    libvlc_int_t *p_libvlc_int;
    vlm_t        *p_vlm;
    int           b_playlist_locked;
    unsigned      ref_count;
    int           verbosity;
    vlc_mutex_t   instance_lock;
    vlc_mutex_t   event_callback_lock;
    struct libvlc_callback_entry_list_t *p_callback_list;
};

struct libvlc_media_t
{
    libvlc_event_manager_t * p_event_manager;
    int                b_preparsed;
    input_item_t      *p_input_item;
    int                i_refcount;
    libvlc_instance_t *p_libvlc_instance;
    libvlc_state_t     state;
    struct libvlc_media_list_t *p_subitems; /* A media descriptor can have
                                           * Sub item */
    void *p_user_data; /* Allows for VLC.framework to hook into media descriptor without creating a new VLCMedia object. */
};

struct libvlc_media_list_t
{
    libvlc_event_manager_t *    p_event_manager;
    libvlc_instance_t *         p_libvlc_instance;
    int                         i_refcount;
    vlc_mutex_t                 object_lock;
    libvlc_media_t * p_md; /* The media from which the
                                       * mlist comes, if any. */
    vlc_array_t                items;

    /* Other way to see that media list */
    /* Used in flat_media_list.c */
    libvlc_media_list_t *       p_flat_mlist;

    /* This indicates if this media list is read-only
     * from a user point of view */
    bool                  b_read_only;
};

typedef libvlc_media_list_view_t * (*libvlc_media_list_view_constructor_func_t)( libvlc_media_list_t * p_mlist, libvlc_exception_t * p_e ) ;
typedef void (*libvlc_media_list_view_release_func_t)( libvlc_media_list_view_t * p_mlv ) ;

typedef int (*libvlc_media_list_view_count_func_t)( libvlc_media_list_view_t * p_mlv,
        libvlc_exception_t * ) ;

typedef libvlc_media_t *
        (*libvlc_media_list_view_item_at_index_func_t)(
                libvlc_media_list_view_t * p_mlv,
                int index,
                libvlc_exception_t * ) ;

typedef libvlc_media_list_view_t *
        (*libvlc_media_list_view_children_at_index_func_t)(
                libvlc_media_list_view_t * p_mlv,
                int index,
                libvlc_exception_t * ) ;

/* A way to see a media list */
struct libvlc_media_list_view_t
{
    libvlc_event_manager_t *    p_event_manager;
    libvlc_instance_t *         p_libvlc_instance;
    int                         i_refcount;
    vlc_mutex_t                 object_lock;

    libvlc_media_list_t *       p_mlist;

    struct libvlc_media_list_view_private_t * p_this_view_data;

    /* Accessors */
    libvlc_media_list_view_count_func_t              pf_count;
    libvlc_media_list_view_item_at_index_func_t      pf_item_at_index;
    libvlc_media_list_view_children_at_index_func_t  pf_children_at_index;

    libvlc_media_list_view_constructor_func_t         pf_constructor;
    libvlc_media_list_view_release_func_t            pf_release;

    /* Notification callback */
    void (*pf_ml_item_added)(const libvlc_event_t *, libvlc_media_list_view_t *);
    void (*pf_ml_item_removed)(const libvlc_event_t *, libvlc_media_list_view_t *);
};

struct libvlc_media_player_t
{
    int                i_refcount;
    vlc_mutex_t        object_lock;
    input_thread_t *   p_input_thread;
    struct libvlc_instance_t *  p_libvlc_instance; /* Parent instance */
    libvlc_media_t * p_md; /* current media descriptor */
    libvlc_event_manager_t *    p_event_manager;
    struct
    {
        void *hwnd;
        uint32_t xid;
    } drawable;

    bool        b_own_its_input_thread;
};

struct libvlc_media_list_player_t
{
    libvlc_event_manager_t *    p_event_manager;
    libvlc_instance_t *         p_libvlc_instance;
    int                         i_refcount;
    vlc_mutex_t                 object_lock;
    libvlc_media_list_path_t    current_playing_item_path;
    libvlc_media_t * p_current_playing_item;
    libvlc_media_list_t *       p_mlist;
    libvlc_media_player_t *   p_mi;
};

struct libvlc_media_library_t
{
    libvlc_event_manager_t * p_event_manager;
    libvlc_instance_t *      p_libvlc_instance;
    int                      i_refcount;
    libvlc_media_list_t *    p_mlist;
};

struct libvlc_media_discoverer_t
{
    libvlc_event_manager_t * p_event_manager;
    libvlc_instance_t *      p_libvlc_instance;
    services_discovery_t *   p_sd;
    libvlc_media_list_t *    p_mlist;
    bool               running;
    vlc_dictionary_t         catname_to_submedialist;
};

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
} libvlc_event_listener_t;

typedef struct libvlc_event_listeners_group_t
{
    libvlc_event_type_t event_type;
    vlc_array_t listeners;
    bool b_sublistener_removed;
} libvlc_event_listeners_group_t;

typedef struct libvlc_event_manager_t
{
    void * p_obj;
    struct libvlc_instance_t * p_libvlc_instance;
    vlc_array_t listeners_groups;
    vlc_mutex_t object_lock;
    vlc_mutex_t event_sending_lock;
} libvlc_event_sender_t;


/***************************************************************************
 * Other internal functions
 ***************************************************************************/
input_thread_t *libvlc_get_input_thread(
     libvlc_media_player_t *,
    libvlc_exception_t * );

/* Media instance */
libvlc_media_player_t *
libvlc_media_player_new_from_input_thread( libvlc_instance_t *,
                                           input_thread_t *,
                                           libvlc_exception_t * );

void libvlc_media_player_destroy(
        libvlc_media_player_t * );

/* Media Descriptor */
libvlc_media_t * libvlc_media_new_from_input_item(
        libvlc_instance_t *, input_item_t *,
        libvlc_exception_t * );

void libvlc_media_set_state(
        libvlc_media_t *, libvlc_state_t,
        libvlc_exception_t * );

/* Media List */
void _libvlc_media_list_add_media(
        libvlc_media_list_t * p_mlist,
        libvlc_media_t * p_md,
        libvlc_exception_t * p_e );

void _libvlc_media_list_insert_media(
        libvlc_media_list_t * p_mlist,
        libvlc_media_t * p_md, int index,
        libvlc_exception_t * p_e );

void _libvlc_media_list_remove_index(
        libvlc_media_list_t * p_mlist, int index,
        libvlc_exception_t * p_e );

/* Media List View */
libvlc_media_list_view_t * libvlc_media_list_view_new(
        libvlc_media_list_t * p_mlist,
        libvlc_media_list_view_count_func_t pf_count,
        libvlc_media_list_view_item_at_index_func_t pf_item_at_index,
        libvlc_media_list_view_children_at_index_func_t pf_children_at_index,
        libvlc_media_list_view_constructor_func_t pf_constructor,
        libvlc_media_list_view_release_func_t pf_release,
        void * this_view_data,
        libvlc_exception_t * p_e );

void libvlc_media_list_view_set_ml_notification_callback(
        libvlc_media_list_view_t * p_mlv,
        void (*item_added)(const libvlc_event_t *, libvlc_media_list_view_t *),
        void (*item_removed)(const libvlc_event_t *, libvlc_media_list_view_t *) );

void libvlc_media_list_view_will_delete_item(
        libvlc_media_list_view_t * p_mlv,
        libvlc_media_t * p_item, int index );

void libvlc_media_list_view_item_deleted(
        libvlc_media_list_view_t * p_mlv,
        libvlc_media_t * p_item, int index );

void libvlc_media_list_view_will_add_item (
        libvlc_media_list_view_t * p_mlv,
        libvlc_media_t * p_item, int index );

void libvlc_media_list_view_item_added(
        libvlc_media_list_view_t * p_mlv,
        libvlc_media_t * p_item, int index );

/* Events */
libvlc_event_manager_t * libvlc_event_manager_new(
        void * p_obj, libvlc_instance_t * p_libvlc_inst,
        libvlc_exception_t *p_e );

void libvlc_event_manager_release(
        libvlc_event_manager_t * p_em );

void libvlc_event_manager_register_event_type(
        libvlc_event_manager_t * p_em,
        libvlc_event_type_t event_type,
        libvlc_exception_t * p_e );

void libvlc_event_send(
        libvlc_event_manager_t * p_em,
        libvlc_event_t * p_event );

/* Media player - audio, video */
libvlc_track_description_t * libvlc_get_track_description(
        libvlc_media_player_t *p_mi,
        const char *psz_variable,
        libvlc_exception_t *p_e );


/* Exception shorcuts */

#define RAISENULL( ... ) { libvlc_exception_raise( p_e, __VA_ARGS__ ); \
                           return NULL; }
#define RAISEVOID( ... ) { libvlc_exception_raise( p_e, __VA_ARGS__ ); \
                           return; }
#define RAISEZERO( ... ) { libvlc_exception_raise( p_e, __VA_ARGS__ ); \
                           return 0; }

#endif
