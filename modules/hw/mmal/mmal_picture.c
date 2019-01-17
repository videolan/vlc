/*****************************************************************************
 * mmal_picture.c: MMAL picture related shared functionality
 *****************************************************************************
 * Copyright © 2014 jusst technologies GmbH
 *
 * Authors: Julian Scheel <julian@jusst.de>
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

#include <vlc_common.h>
#include <vlc_picture.h>
#include <interface/mmal/mmal.h>

#include "mmal_picture.h"

int mmal_picture_lock(picture_t *picture)
{
    picture_sys_t *pic_sys = picture->p_sys;
    MMAL_BUFFER_HEADER_T *buffer = pic_sys->buffer;

    int offset = 0;
    picture->p[0].p_pixels = buffer->data;
    for (int i = 1; i < picture->i_planes; i++) {
        offset = offset + picture->p[i - 1].i_pitch * picture->p[i - 1].i_lines;
        picture->p[i].p_pixels = (ptrdiff_t)buffer->data + offset;
    }

    pic_sys->displayed = false;

    return VLC_SUCCESS;
}
