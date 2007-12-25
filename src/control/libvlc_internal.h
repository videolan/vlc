/*****************************************************************************
 * libvlc_internal.h : Definition of opaque structures for libvlc exported API
 * Also contains some internal utility functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: control_structures.h 13752 2005-12-15 10:14:42Z oaubert $
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

#include <vlc/vlc.h>
#include <vlc/libvlc_structures.h>

#include <vlc_arrays.h>
#include <vlc_input.h>

# ifdef __cplusplus
extern "C" {
# endif

/***************************************************************************
 * Internal creation and destruction functions
 ***************************************************************************/
VLC_EXPORT (libvlc_int_t *, libvlc_InternalCreate, ( void ) );
VLC_EXPORT (int, libvlc_InternalInit, ( libvlc_int_t *, int, const char *ppsz_argv[] ) );
VLC_EXPORT (int, libvlc_InternalCleanup, ( libvlc_int_t * ) );
VLC_EXPORT (int, libvlc_InternalDestroy, ( libvlc_int_t *, vlc_bool_t ) );

VLC_EXPORT (int, libvlc_InternalAddIntf, ( libvlc_int_t *, const char *, vlc_bool_t,
                            vlc_bool_t, int, const char *const * ) );

VLC_EXPORT (void, libvlc_event_init, ( libvlc_instance_t *, libvlc_exception_t * ) );
VLC_EXPORT (void, libvlc_event_fini, ( libvlc_instance_t * ) );


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
    vlc_mutex_t   instance_lock;
    vlc_mutex_t   event_callback_lock;
    struct libvlc_callback_entry_list_t *p_callback_list;
};

struct libvlc_tags_storage_t
{
    char ** ppsz_tags;
    int i_count;
};

struct libvlc_media_descriptor_t
{
    libvlc_event_manager_t * p_event_manager;
    int                b_preparsed;
    input_item_t      *p_input_item;
    int                i_refcount;
    libvlc_instance_t *p_libvlc_instance;
    vlc_dictionary_t   tags; /* To be merged with core's meta soon */
    libvlc_state_t     state;
    struct libvlc_media_list_t *p_subitems; /* A media descriptor can have
                                           * Sub item */
    void *p_user_data; /* Allows for VLC.framework to hook into media descriptor without creating a new VLCMedia object. */
};

struct libvlc_tag_query_t
{
    struct libvlc_instance_t  *p_libvlc_instance; /* Parent instance */
    int                i_refcount;
    libvlc_tag_t       tag;
    char *             psz_tag_key;
};

struct libvlc_media_list_t
{
    libvlc_event_manager_t *    p_event_manager;
    libvlc_instance_t *         p_libvlc_instance;
    int                         i_refcount;
    vlc_mutex_t                 object_lock;
    libvlc_media_descriptor_t * p_md; /* The media_descriptor from which the
                                       * mlist comes, if any. */
    vlc_array_t                items;
 
    /* Other way to see that media list */
    /* Used in flat_media_list.c */
    libvlc_media_list_t *       p_flat_mlist;

    /* This indicates if this media list is read-only
     * from a user point of view */
    vlc_bool_t                  b_read_only;
};

typedef void (*libvlc_media_list_view_release_func_t)( libvlc_media_list_view_t * p_mlv ) ;

typedef int (*libvlc_media_list_view_count_func_t)( libvlc_media_list_view_t * p_mlv,
        libvlc_exception_t * ) ;

typedef libvlc_media_descriptor_t *
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

    libvlc_media_list_view_release_func_t            pf_release;

    /* Notification callback */
    void (*pf_ml_item_added)(const libvlc_event_t *, libvlc_media_list_view_t *);
    void (*pf_ml_item_removed)(const libvlc_event_t *, libvlc_media_list_view_t *);
};

struct libvlc_dynamic_media_list_t
{
    libvlc_instance_t *     p_libvlc_instance;
    int                     i_refcount;
    libvlc_media_list_t *   p_media_provider;
    libvlc_tag_query_t *    p_query;
    char *                  psz_tag_key;
    libvlc_tag_t            tag;
    struct libvlc_media_list_t *  p_mlist;
    struct libvlc_media_list_t *  p_provider;
};

struct libvlc_media_instance_t
{
    int                i_refcount;
    vlc_mutex_t        object_lock;
    int i_input_id;  /* Input object id. We don't use a pointer to
                        avoid any crash */
    struct libvlc_instance_t *  p_libvlc_instance; /* Parent instance */
    libvlc_media_descriptor_t * p_md; /* current media descriptor */
    libvlc_event_manager_t *    p_event_manager;
    libvlc_drawable_t           drawable;
    
    vlc_bool_t        b_own_its_input_thread;
};

struct libvlc_media_list_player_t
{
    libvlc_event_manager_t *    p_event_manager;
    libvlc_instance_t *         p_libvlc_instance;
    int                         i_refcount;
    vlc_mutex_t                 object_lock;
    libvlc_media_list_path_t    current_playing_item_path;
    libvlc_media_descriptor_t * p_current_playing_item;
    libvlc_media_list_t *       p_mlist;
    libvlc_media_instance_t *   p_mi;
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
    vlc_bool_t               running;
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
 *        p_self->p_event_manager = libvlc_event_manager_init( p_self,
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
    DECL_ARRAY(libvlc_event_listener_t *) listeners;
} libvlc_event_listeners_group_t;

typedef struct libvlc_event_manager_t
{
    void * p_obj;
    struct libvlc_instance_t * p_libvlc_instance;
    DECL_ARRAY(libvlc_event_listeners_group_t *) listeners_groups;
    vlc_mutex_t object_lock;
    vlc_mutex_t event_sending_lock;
} libvlc_event_sender_t;


/***************************************************************************
 * Other internal functions
 ***************************************************************************/
VLC_EXPORT (input_thread_t *, libvlc_get_input_thread,
                        ( struct libvlc_media_instance_t *, libvlc_exception_t * ) );

/* Media instance */
VLC_EXPORT (libvlc_media_instance_t *, libvlc_media_instance_new_from_input_thread,
                        ( struct libvlc_instance_t *, input_thread_t *, libvlc_exception_t * ) );

VLC_EXPORT (void, libvlc_media_instance_destroy,
                        ( libvlc_media_instance_t * ) );

/* Media Descriptor */
VLC_EXPORT (libvlc_media_descriptor_t *, libvlc_media_descriptor_new_from_input_item,
                        ( struct libvlc_instance_t *, input_item_t *, libvlc_exception_t * ) );

VLC_EXPORT (libvlc_media_descriptor_t *, libvlc_media_descriptor_duplicate,
                        ( libvlc_media_descriptor_t * ) );

VLC_EXPORT (void, libvlc_media_descriptor_set_state,
                        ( libvlc_media_descriptor_t *, libvlc_state_t, libvlc_exception_t * ) );

/* Media List */
VLC_EXPORT ( void, _libvlc_media_list_add_media_descriptor,
                        ( libvlc_media_list_t * p_mlist,
                          libvlc_media_descriptor_t * p_md,
                          libvlc_exception_t * p_e ) );

VLC_EXPORT ( void, _libvlc_media_list_insert_media_descriptor,
                        ( libvlc_media_list_t * p_mlist,
                          libvlc_media_descriptor_t * p_md,
                          int index,
                          libvlc_exception_t * p_e ) );

VLC_EXPORT ( void, _libvlc_media_list_remove_index,
                        ( libvlc_media_list_t * p_mlist,
                          int index,
                          libvlc_exception_t * p_e ) );

/* Media List View */
VLC_EXPORT ( libvlc_media_list_view_t *, libvlc_media_list_view_new,
                          ( libvlc_media_list_t * p_mlist,
                            libvlc_media_list_view_count_func_t pf_count,
                            libvlc_media_list_view_item_at_index_func_t pf_item_at_index,
                            libvlc_media_list_view_children_at_index_func_t pf_children_at_index,
                            libvlc_media_list_view_release_func_t pf_release,
                            void * this_view_data,
                            libvlc_exception_t * p_e ) );

VLC_EXPORT ( void, libvlc_media_list_view_set_ml_notification_callback, (
                libvlc_media_list_view_t * p_mlv,
                void (*item_added)(const libvlc_event_t *, libvlc_media_list_view_t *),
                void (*item_removed)(const libvlc_event_t *, libvlc_media_list_view_t *) ));

VLC_EXPORT ( void, libvlc_media_list_view_will_delete_item, ( libvlc_media_list_view_t * p_mlv,
                                                              libvlc_media_descriptor_t * p_item,
                                                              int index ));
VLC_EXPORT ( void, libvlc_media_list_view_item_deleted, ( libvlc_media_list_view_t * p_mlv,
                                                          libvlc_media_descriptor_t * p_item,
                                                          int index ));
VLC_EXPORT ( void, libvlc_media_list_view_will_add_item, ( libvlc_media_list_view_t * p_mlv,
                                                           libvlc_media_descriptor_t * p_item,
                                                           int index ));
VLC_EXPORT ( void, libvlc_media_list_view_item_added, ( libvlc_media_list_view_t * p_mlv,
                                                        libvlc_media_descriptor_t * p_item,
                                                        int index ));

/* Events */
VLC_EXPORT (libvlc_event_manager_t *, libvlc_event_manager_new, ( void * p_obj, libvlc_instance_t * p_libvlc_inst, libvlc_exception_t *p_e ) );

VLC_EXPORT (void, libvlc_event_manager_release, ( libvlc_event_manager_t * p_em ) );

VLC_EXPORT (void, libvlc_event_manager_register_event_type, ( libvlc_event_manager_t * p_em, libvlc_event_type_t event_type, libvlc_exception_t * p_e ) );
VLC_EXPORT (void, libvlc_event_detach_lock_state, ( libvlc_event_manager_t *p_event_manager, libvlc_event_type_t event_type, libvlc_callback_t pf_callback,
                                                    void *p_user_data, libvlc_lock_state_t lockstate, libvlc_exception_t *p_e ) );

VLC_EXPORT (void, libvlc_event_send, ( libvlc_event_manager_t * p_em, libvlc_event_t * p_event ) );


/* Exception shorcuts */

#define RAISENULL( psz,a... ) { libvlc_exception_raise( p_e, psz,##a ); \
                                return NULL; }
#define RAISEVOID( psz,a... ) { libvlc_exception_raise( p_e, psz,##a ); \
                                return; }
#define RAISEZERO( psz,a... ) { libvlc_exception_raise( p_e, psz,##a ); \
                                return 0; }

# ifdef __cplusplus
}
# endif

#endif
