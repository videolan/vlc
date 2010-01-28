/*****************************************************************************
 * libvlc_internal.h : Definition of opaque structures for libvlc exported API
 * Also contains some internal utility functions
 *****************************************************************************
 * Copyright (C) 2005-2009 the VideoLAN team
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

#ifndef LIBVLC_MEDIA_LIST_VIEW_INTERNAL_H
#define LIBVLC_MEDIA_LIST_VIEW_INTERNAL_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include <vlc/libvlc_structures.h>
#include <vlc/libvlc_media_list.h>

#include <vlc_common.h>

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

    libvlc_media_list_view_constructor_func_t        pf_constructor;
    libvlc_media_list_view_release_func_t            pf_release;

    /* Notification callback */
    void (*pf_ml_item_added)(const libvlc_event_t *, libvlc_media_list_view_t *);
    void (*pf_ml_item_removed)(const libvlc_event_t *, libvlc_media_list_view_t *);
};

/* Media List View */
libvlc_media_list_view_t * libvlc_media_list_view_new(
        libvlc_media_list_t * p_mlist,
        libvlc_media_list_view_count_func_t pf_count,
        libvlc_media_list_view_item_at_index_func_t pf_item_at_index,
        libvlc_media_list_view_children_at_index_func_t pf_children_at_index,
        libvlc_media_list_view_constructor_func_t pf_constructor,
        libvlc_media_list_view_release_func_t pf_release,
        void * this_view_data );

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

#endif
