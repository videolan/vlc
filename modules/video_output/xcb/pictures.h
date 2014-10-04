/**
 * @file pictures.h
 * @brief XCB pictures allocation header
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
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

#ifdef WORDS_BIGENDIAN
# define ORDER XCB_IMAGE_ORDER_MSB_FIRST
#else
# define ORDER XCB_IMAGE_ORDER_LSB_FIRST
#endif

#include <vlc_picture.h>
#include <vlc_vout_display.h>
#include <xcb/shm.h>

bool XCB_shm_Check (vlc_object_t *obj, xcb_connection_t *conn);
int XCB_picture_Alloc (vout_display_t *, picture_resource_t *, size_t size,
                       xcb_connection_t *, xcb_shm_seg_t);
picture_t *XCB_picture_NewFromResource (const video_format_t *,
                                        const picture_resource_t *,
                                        xcb_connection_t *);

static inline xcb_shm_seg_t XCB_picture_GetSegment(const picture_t *pic)
{
    return (uintptr_t)pic->p_sys;
}
