/*****************************************************************************
 * item.h
 *****************************************************************************
 * Copyright (C) 2008 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef LIBVLC_INPUT_ITEM_H
#define LIBVLC_INPUT_ITEM_H 1

#include "input_interface.h"
#include <vlc_atomic.h>

void input_item_UpdateTracksInfo( input_item_t *item, const es_format_t *fmt,
                                  const char *es_id, bool stable );
/**
 * Set the libvlc owner (libvlc_media_t *) with a release callback
 *
 * Can only be set after the item is just created.
 */
VLC_API void input_item_SetLibvlcOwner(input_item_t *, void *,
                                       void (*release)(void *));
/**
 * Get the libvlc owner (libvlc_media_t *)
 */
VLC_API void *input_item_GetLibvlcOwner(input_item_t *);

typedef struct input_item_owner
{
    input_item_t item;
    void *libvlc_owner;
    void (*libvlc_owner_release)(void *libvlc_owner);
    vlc_atomic_rc_t rc;
} input_item_owner_t;

# define item_owner(item) ((struct input_item_owner *)(item))

#endif
