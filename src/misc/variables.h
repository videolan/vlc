/*****************************************************************************
 * variables.h: object variables typedefs
 *****************************************************************************
 * Copyright (C) 1999-2012 VLC authors and VideoLAN
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
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

#ifndef LIBVLC_VARIABLES_H
# define LIBVLC_VARIABLES_H 1

# include <vlc_atomic.h>

/**
 * Private LibVLC data for each object.
 */
typedef struct vlc_object_internals vlc_object_internals_t;

struct vlc_object_internals
{
    char           *psz_name; /* given name */

    /* Object variables */
    void           *var_root;
    vlc_mutex_t     var_lock;
    vlc_cond_t      var_wait;

    /* Objects management */
    atomic_uint     refs;
    vlc_destructor_t pf_destructor;

    /* Objects tree structure */
    vlc_object_internals_t *next;  /* next sibling */
    vlc_object_internals_t *prev;  /* previous sibling */
    vlc_object_internals_t *first; /* first child */
};

# define vlc_internals( obj ) (((vlc_object_internals_t*)(VLC_OBJECT(obj)))-1)
# define vlc_externals( priv ) ((vlc_object_t *)((priv) + 1))

void DumpVariables(vlc_object_t *obj);

extern void var_DestroyAll( vlc_object_t * );

#endif
