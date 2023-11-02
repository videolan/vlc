/*****************************************************************************
 * dynamicoverlay_list.c : dynamic overlay list
 *****************************************************************************
 * Copyright (C) 2008-2009 VLC authors and VideoLAN
 *
 * Author: Søren Bøg <avacore@videolan.org>
 *         Jean-Paul Saman <jpsaman@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_arrays.h>

#include "dynamicoverlay.h"

/*****************************************************************************
 * list_t: Command queue
 *****************************************************************************/

int do_ListInit( list_t *p_list )
{
    vlc_vector_init( p_list );
    return VLC_SUCCESS;
}

int do_ListDestroy( list_t *p_list )
{
    overlay_t *p_cur;
    vlc_vector_foreach(p_cur, p_list)
    {
        OverlayDestroy( p_cur );
        free( p_cur );
    }
    vlc_vector_destroy( p_list );

    return VLC_SUCCESS;
}

ssize_t ListAdd( list_t *p_list, overlay_t *p_new )
{
    if (!vlc_vector_push(p_list, p_new))
        return -1;

    return p_list->size - 1;
}

int ListRemove( list_t *p_list, size_t i_idx )
{
    int ret;

    if ( i_idx >= p_list->size)
        return VLC_EINVAL;

    ret = OverlayDestroy( p_list->data[i_idx] );
    vlc_vector_remove(p_list, i_idx);

    return ret;
}

overlay_t *ListGet( list_t *p_list, size_t i_idx )
{
    if (i_idx >= p_list->size)
        return NULL;
    return p_list->data[i_idx];
}
