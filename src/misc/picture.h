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

#include <stdatomic.h>
#include <stddef.h>

#include <vlc_picture.h>

typedef struct
{
    picture_t picture;
    struct
    {
        void (*destroy)(picture_t *);
        void *opaque;
    } gc;
} picture_priv_t;

void *picture_Allocate(int *, size_t);
void picture_Deallocate(int, void *, size_t);

picture_t * picture_InternalClone(picture_t *, void (*pf_destroy)(picture_t *), void *);
