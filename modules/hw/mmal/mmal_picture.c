/*****************************************************************************
 * mmal_picture.c: MMAL picture related shared functionality
 *****************************************************************************
 * Copyright © 2014 jusst technologies GmbH
 * $Id$
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
    int ret = VLC_SUCCESS;

    vlc_mutex_lock(pic_sys->mutex);

    MMAL_BUFFER_HEADER_T *buffer = mmal_queue_timedwait(pic_sys->queue, 2);
    if (!buffer) {
        ret = VLC_EGENERIC;
        goto out;
    }

    mmal_buffer_header_reset(buffer);
    buffer->user_data = picture;
    picture->p[0].p_pixels = buffer->data;
    picture->p[1].p_pixels += (ptrdiff_t)buffer->data;
    picture->p[2].p_pixels += (ptrdiff_t)buffer->data;

    pic_sys->buffer = buffer;

    pic_sys->displayed = false;

out:
    vlc_mutex_unlock(pic_sys->mutex);
    return ret;
}

void mmal_picture_unlock(picture_t *picture)
{
    picture_sys_t *pic_sys = picture->p_sys;
    MMAL_BUFFER_HEADER_T *buffer = pic_sys->buffer;

    vlc_mutex_lock(pic_sys->mutex);

    pic_sys->buffer = NULL;
    if (buffer) {
        buffer->user_data = NULL;
        mmal_buffer_header_release(buffer);
    }

    vlc_mutex_unlock(pic_sys->mutex);
}
