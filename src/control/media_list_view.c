/*****************************************************************************
 * flat_media_list.c: libvlc flat media list functions. (extension to
 * media_list.c).
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

//#define DEBUG_FLAT_LIST

#ifdef DEBUG_FLAT_LIST
# define trace( fmt, ... ) printf( "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__ )
#else
# define trace( ... )
#endif

/*
 * Private functions
 */

/*
 * Public libvlc functions
 */

/* Limited to four args, because it should be enough */

#define AN_SELECT( collapser, dec1, dec2, dec3, dec4, p, ...) p
#define ARGS(...) AN_SELECT( collapser, ##__VA_ARGS__, \
                                              (p_mlv, arg1, arg2, arg3, arg4, p_e), \
                                              (p_mlv, arg1, arg2, arg3, p_e), \
                                              (p_mlv, arg1, arg2, p_e), \
                                              (p_mlv, arg1, p_e), (p_mlv, p_e) )

#define MEDIA_LIST_VIEW_FUNCTION( name, ret_type, default_ret_value, /* Params */ ... ) \
    ret_type \
    libvlc_media_list_view_##name( libvlc_media_list_view_t * p_mlv, \
                                  ##__VA_ARGS__, \
                                  libvlc_exception_t * p_e ) \
    { \
        if( p_mlv->pf_##name ) \
            return p_mlv->pf_##name ARGS(__VA_ARGS__) ; \
        libvlc_exception_raise( p_e, "No '" #name "' method in this media_list_view" ); \
        return default_ret_value;\
    }

#define MEDIA_LIST_VIEW_FUNCTION_VOID_RET( name, /* Params */ ... ) \
    void \
    libvlc_media_list_view_##name( libvlc_media_list_view_t * p_mlv, \
                                  ##__VA_ARGS__, \
                                  libvlc_exception_t * p_e ) \
    { \
        if( p_mlv->pf_##name ) \
        { \
            p_mlv->pf_##name ARGS(__VA_ARGS__) ; \
            return; \
        } \
        libvlc_exception_raise( p_e, "No '" #name "' method in this media_list_view" ); \
    }


MEDIA_LIST_VIEW_FUNCTION( count, int, 0 )
MEDIA_LIST_VIEW_FUNCTION( item_at_index, libvlc_media_descriptor_t *, NULL, int arg1 )
MEDIA_LIST_VIEW_FUNCTION( index_of_item, int, -1, libvlc_media_descriptor_t * arg1 )

MEDIA_LIST_VIEW_FUNCTION_VOID_RET( insert_at_index, libvlc_media_descriptor_t * arg1, int arg2 )
MEDIA_LIST_VIEW_FUNCTION_VOID_RET( remove_at_index, int arg1 )
MEDIA_LIST_VIEW_FUNCTION_VOID_RET( add_item, libvlc_media_descriptor_t * arg1 )

