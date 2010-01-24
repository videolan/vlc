/*****************************************************************************
 * hierarchical_media_list_view.c: libvlc hierarchical media list view functs.
 *****************************************************************************
 * Copyright (C) 2007 the VideoLAN team
 * $Id$
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/libvlc.h>
#include <vlc/libvlc_media.h>
#include <vlc/libvlc_media_list.h>
#include <vlc/libvlc_media_list_view.h>

#include "media_list_internal.h"
#include "media_list_view_internal.h"

//#define DEBUG_HIERARCHICAL_VIEW

#ifdef DEBUG_HIERARCHICAL_VIEW
# define trace( fmt, ... ) printf( "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__ )
#else
# define trace( ... )
#endif

/*
 * Private functions
 */

/**************************************************************************
 *       flat_media_list_view_count  (private)
 * (called by media_list_view_count)
 **************************************************************************/
static int
hierarch_media_list_view_count( libvlc_media_list_view_t * p_mlv,
                                libvlc_exception_t * p_e )
{
    return libvlc_media_list_count( p_mlv->p_mlist, p_e );
}

/**************************************************************************
 *       flat_media_list_view_item_at_index  (private)
 * (called by flat_media_list_view_item_at_index)
 **************************************************************************/
static libvlc_media_t *
hierarch_media_list_view_item_at_index( libvlc_media_list_view_t * p_mlv,
                                    int index,
                                    libvlc_exception_t * p_e )
{
    return libvlc_media_list_item_at_index( p_mlv->p_mlist, index, p_e );
}

/**************************************************************************
 *       flat_media_list_view_item_at_index  (private)
 * (called by flat_media_list_view_item_at_index)
 **************************************************************************/
static libvlc_media_list_view_t *
hierarch_media_list_view_children_at_index( libvlc_media_list_view_t * p_mlv,
                                        int index,
                                        libvlc_exception_t * p_e )
{
    libvlc_media_t * p_md;
    libvlc_media_list_t * p_submlist;
    libvlc_media_list_view_t * p_ret;
    p_md = libvlc_media_list_item_at_index( p_mlv->p_mlist, index, p_e );
    if( !p_md ) return NULL;
    p_submlist = libvlc_media_subitems( p_md );
    libvlc_media_release( p_md );
    if( !p_submlist ) return NULL;
    p_ret = libvlc_media_list_hierarchical_view( p_submlist, p_e );
    libvlc_media_list_release( p_submlist );

    return p_ret;
}

/**************************************************************************
 *       media_list_(item|will)_* (private) (Event callback)
 **************************************************************************/
static void
media_list_item_added( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_t * p_md;
    libvlc_media_list_view_t * p_mlv = user_data;
    int index = p_event->u.media_list_item_added.index;
    p_md = p_event->u.media_list_item_added.item;
    libvlc_media_list_view_item_added( p_mlv, p_md, index );
}
static void
media_list_will_add_item( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_t * p_md;
    libvlc_media_list_view_t * p_mlv = user_data;
    int index = p_event->u.media_list_will_add_item.index;
    p_md = p_event->u.media_list_will_add_item.item;
    libvlc_media_list_view_will_add_item( p_mlv, p_md, index );
}
static void
media_list_item_deleted( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_t * p_md;
    libvlc_media_list_view_t * p_mlv = user_data;
    int index = p_event->u.media_list_item_deleted.index;
    p_md = p_event->u.media_list_item_deleted.item;
    libvlc_media_list_view_item_deleted( p_mlv, p_md, index );
}
static void
media_list_will_delete_item( const libvlc_event_t * p_event, void * user_data )
{
    libvlc_media_t * p_md;
    libvlc_media_list_view_t * p_mlv = user_data;
    int index = p_event->u.media_list_will_delete_item.index;
    p_md = p_event->u.media_list_will_delete_item.item;
    libvlc_media_list_view_will_delete_item( p_mlv, p_md, index );
}

/*
 * Public libvlc functions
 */


/**************************************************************************
 *       flat_media_list_view_release (private)
 * (called by media_list_view_release)
 **************************************************************************/
static void
hierarch_media_list_view_release( libvlc_media_list_view_t * p_mlv )
{
    libvlc_event_detach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListItemAdded,
                         media_list_item_added, p_mlv );
    libvlc_event_detach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListWillAddItem,
                         media_list_will_add_item, p_mlv );
    libvlc_event_detach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListItemDeleted,
                         media_list_item_deleted, p_mlv );
    libvlc_event_detach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListWillDeleteItem,
                         media_list_will_delete_item, p_mlv );
}

/**************************************************************************
 *       libvlc_media_list_flat_view (Public)
 **************************************************************************/
libvlc_media_list_view_t *
libvlc_media_list_hierarchical_view( libvlc_media_list_t * p_mlist,
                                     libvlc_exception_t * p_e )
{
    trace("\n");
    libvlc_media_list_view_t * p_mlv;
    p_mlv = libvlc_media_list_view_new( p_mlist,
                                        hierarch_media_list_view_count,
                                        hierarch_media_list_view_item_at_index,
                                        hierarch_media_list_view_children_at_index,
                                        libvlc_media_list_hierarchical_view,
                                        hierarch_media_list_view_release,
                                        NULL,
                                        p_e );
    libvlc_media_list_lock( p_mlist );
    libvlc_event_attach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListItemAdded,
                         media_list_item_added, p_mlv, NULL );
    libvlc_event_attach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListWillAddItem,
                         media_list_will_add_item, p_mlv, NULL );
    libvlc_event_attach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListItemDeleted,
                         media_list_item_deleted, p_mlv, NULL );
    libvlc_event_attach( p_mlv->p_mlist->p_event_manager,
                         libvlc_MediaListWillDeleteItem,
                         media_list_will_delete_item, p_mlv, NULL );
    libvlc_media_list_unlock( p_mlist );
    return p_mlv;
}
