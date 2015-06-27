/*****************************************************************************
 * picture.h: picture internals
 *****************************************************************************
 * Copyright (C) 2015 Remi Denis-Courmont
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

#include <vlc_picture.h>
#include <vlc_atomic.h>

typedef struct
{
    picture_t picture;
    struct
    {
        atomic_uintptr_t refs;
        void (*destroy)(picture_t *);
        void *opaque;
    } gc;
} picture_priv_t;
