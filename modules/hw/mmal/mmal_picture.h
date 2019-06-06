/*****************************************************************************
 * mmal_picture.h: Shared header for MMAL pictures
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

#ifndef VLC_MMAL_MMAL_PICTURE_H_
#define VLC_MMAL_MMAL_PICTURE_H_

#include <vlc_common.h>
#include <interface/mmal/mmal.h>

/* Think twice before changing this. Incorrect values cause havoc. */
#define NUM_ACTUAL_OPAQUE_BUFFERS 30

typedef struct
{
    vlc_object_t *owner;

    MMAL_BUFFER_HEADER_T *buffer;
    bool displayed;
} picture_sys_t;

int mmal_picture_lock(picture_t *picture);

#endif
