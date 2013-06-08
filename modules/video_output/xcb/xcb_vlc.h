/**
 * @file xcb_vlc.h
 * @brief X C Bindings VLC module common header
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

#ifndef XCB_CURSOR_NONE
# define XCB_CURSOR_NONE ((xcb_cursor_t) 0U)
#endif

#include <vlc_picture.h>
#include <vlc_vout_display.h>

/* keys.c */
typedef struct key_handler_t key_handler_t;
key_handler_t *XCB_keyHandler_Create (vlc_object_t *, xcb_connection_t *);
void XCB_keyHandler_Destroy (key_handler_t *);
int XCB_keyHandler_Process (key_handler_t *, xcb_generic_event_t *);

/* events.c */
int XCB_error_Check (vout_display_t *, xcb_connection_t *conn,
                     const char *str, xcb_void_cookie_t);

struct vout_window_t *XCB_parent_Create (vout_display_t *obj,
                                         xcb_connection_t **,
                                         const xcb_screen_t **,
                                         uint16_t *width, uint16_t *height);
xcb_cursor_t XCB_cursor_Create (xcb_connection_t *, const xcb_screen_t *);

int XCB_Manage (vout_display_t *vd, xcb_connection_t *conn, bool *);

/* common.c */
bool XCB_shm_Check (vlc_object_t *obj, xcb_connection_t *conn);

/* FIXME
 * maybe it would be better to split this header in 2 */
#include <xcb/shm.h>
struct picture_sys_t
{
    xcb_shm_seg_t segment;
};
int XCB_pictures_Alloc (vout_display_t *, picture_resource_t *, size_t size,
                        xcb_connection_t *, bool attach);
void XCB_pictures_Free (picture_resource_t *, xcb_connection_t *);
