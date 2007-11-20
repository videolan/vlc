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

#include "libvlc_internal.h"
#include <vlc/libvlc.h>
#include <assert.h>
#include "vlc_arrays.h"

//#define DEBUG_HIERARCHICAL_VIEW

#ifdef DEBUG_HIERARCHICAL_VIEW
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
static libvlc_media_descriptor_t *
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
    libvlc_media_descriptor_t * p_md;
    libvlc_media_list_t * p_submlist;
    p_md = libvlc_media_list_item_at_index( p_mlv->p_mlist, index, p_e );
    if( !p_md ) return NULL;
    p_submlist = libvlc_media_descriptor_subitems( p_md, p_e );
    if( !p_submlist ) return NULL;
    return libvlc_media_list_hierarchical_view( p_submlist, p_e );
}

/**************************************************************************
 *       flat_media_list_view_release (private)
 * (called by media_list_view_release)
 **************************************************************************/
static void
hierarch_media_list_view_release( libvlc_media_list_view_t * p_mlv )
{
}

/*
 * Public libvlc functions
 */

/**************************************************************************
 *       libvlc_media_list_flat_view (Public)
 **************************************************************************/
libvlc_media_list_view_t *
libvlc_media_list_hierarchical_view( libvlc_media_list_t * p_mlist,
                                     libvlc_exception_t * p_e )
{
    trace("\n");
    libvlc_media_list_view_t * p_mlv;
    libvlc_media_list_lock( p_mlist );
    p_mlv = libvlc_media_list_view_new( p_mlist,
                                        hierarch_media_list_view_count,
                                        hierarch_media_list_view_item_at_index,
                                        hierarch_media_list_view_children_at_index,
                                        hierarch_media_list_view_release,
                                        NULL,
                                        p_e );
    libvlc_media_list_unlock( p_mlist );
    return p_mlv;
}
