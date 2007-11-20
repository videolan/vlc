/*****************************************************************************
 * flat_media_list_view.c: libvlc flat media list view functions.
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id: flat_media_list.c 21287 2007-08-20 01:28:12Z pdherbemont $
 *
 * Authors: Pierre d'Herbemont <pdherbemont # videolan.org>
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
#include <assert.h>
#include "vlc_arrays.h"

//#define DEBUG_FLAT_VIEW

#ifdef DEBUG_FLAT_VIEW
# define trace( fmt, ... ) printf( "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__ )
#else
# define trace( ... )
#endif

struct libvlc_media_list_view_private_t
{
    vlc_array_t array;
};

/*
 * Private functions
 */

/**************************************************************************
 *       ml_item_added  (private) (Callback from media_list_view item_added)
 **************************************************************************/
static void
ml_item_added( const libvlc_event_t * p_event, libvlc_media_list_view_t * p_mlv )
{
    int index = vlc_array_count( &p_mlv->p_this_view_data->array );
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_added.item;
    libvlc_media_descriptor_retain( p_md );
    libvlc_media_list_view_will_add_item( p_mlv, p_md, index );
    vlc_array_append( &p_mlv->p_this_view_data->array, p_md );
    libvlc_media_list_view_item_added( p_mlv, p_md, index );
}

/**************************************************************************
 *       ml_item_removed  (private) (Callback from media_list_view)
 **************************************************************************/
static void
ml_item_removed( const libvlc_event_t * p_event, libvlc_media_list_view_t * p_mlv )
{
    libvlc_media_descriptor_t * p_md = p_event->u.media_list_item_deleted.item;
    int i = vlc_array_index_of_item( &p_mlv->p_this_view_data->array, p_md );
    if( i >= 0 )
    {
        libvlc_media_list_view_will_delete_item( p_mlv, p_md, i );
        vlc_array_remove( &p_mlv->p_this_view_data->array, i );
        libvlc_media_list_view_item_deleted( p_mlv, p_md, i );
        libvlc_media_descriptor_release( p_md );
    }
}

/**************************************************************************
 *       flat_media_list_view_count  (private)
 * (called by media_list_view_count)
 **************************************************************************/
static int
flat_media_list_view_count( libvlc_media_list_view_t * p_mlv,
                            libvlc_exception_t * p_e )
{
    (void)p_e;
    return vlc_array_count( &p_mlv->p_this_view_data->array );
}

/**************************************************************************
 *       flat_media_list_view_item_at_index  (private)
 * (called by flat_media_list_view_item_at_index)
 **************************************************************************/
static libvlc_media_descriptor_t *
flat_media_list_view_item_at_index( libvlc_media_list_view_t * p_mlv,
                                    int index,
                                    libvlc_exception_t * p_e )
{
    libvlc_media_descriptor_t * p_md;
    (void)p_e;
    p_md = vlc_array_item_at_index( &p_mlv->p_this_view_data->array, index );
    libvlc_media_descriptor_retain( p_md );
    return p_md;
}

/**************************************************************************
 *       flat_media_list_view_item_at_index  (private)
 * (called by flat_media_list_view_item_at_index)
 **************************************************************************/
static libvlc_media_list_view_t *
flat_media_list_view_children_at_index( libvlc_media_list_view_t * p_mlv,
                                        int index,
                                        libvlc_exception_t * p_e )
{
    return NULL;
}

/**************************************************************************
 *       flat_media_list_view_release (private)
 * (called by media_list_view_release)
 **************************************************************************/
static void
flat_media_list_view_release( libvlc_media_list_view_t * p_mlv )
{
    vlc_array_clear( &p_mlv->p_this_view_data->array );
    free( p_mlv->p_this_view_data );
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       libvlc_media_list_flat_view (Public)
 **************************************************************************/
libvlc_media_list_view_t *
libvlc_media_list_flat_view( libvlc_media_list_t * p_mlist,
                             libvlc_exception_t * p_e )
{
    trace("\n");
    libvlc_media_list_view_t * p_mlv;
    libvlc_media_list_lock( p_mlist );
    struct libvlc_media_list_view_private_t * p_this_view_data;
    p_this_view_data = malloc(sizeof(struct libvlc_media_list_view_private_t));
    vlc_array_init( &p_this_view_data->array );
    p_mlv = libvlc_media_list_view_new( p_mlist,
                                        flat_media_list_view_count,
                                        flat_media_list_view_item_at_index,
                                        flat_media_list_view_children_at_index,
                                        flat_media_list_view_release,
                                        p_this_view_data,
                                        p_e );
    libvlc_media_list_view_set_ml_notification_callback( p_mlv,
        ml_item_added,
        ml_item_removed );

    return p_mlv;
}
