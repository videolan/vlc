/**
 * @file xcb_vlc.h
 * @brief X C Bindings VLC module common header
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

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

int ManageEvent (vout_display_t *vd, xcb_connection_t *conn, bool *);

/* keys.c */
typedef struct key_handler_t key_handler_t;
key_handler_t *CreateKeyHandler (vlc_object_t *, xcb_connection_t *);
void DestroyKeyHandler (key_handler_t *);
int ProcessKeyEvent (key_handler_t *, xcb_generic_event_t *);

/* common.c */
struct vout_window_t *GetWindow (vout_display_t *obj,
                                 xcb_connection_t **restrict pconn,
                                 const xcb_screen_t **restrict pscreen,
                                 uint8_t *restrict pdepth);
int GetWindowSize (struct vout_window_t *wnd, xcb_connection_t *conn,
                   unsigned *restrict width, unsigned *restrict height);
void CheckSHM (vlc_object_t *obj, xcb_connection_t *conn, bool *restrict pshm);
xcb_cursor_t CreateBlankCursor (xcb_connection_t *, const xcb_screen_t *);
void RegisterMouseEvents (vlc_object_t *, xcb_connection_t *, xcb_window_t);

int CheckError (vout_display_t *, xcb_connection_t *conn,
                const char *str, xcb_void_cookie_t);

/* FIXME
 * maybe it would be better to split this header in 2 */
#include <xcb/shm.h>
struct picture_sys_t
{
    xcb_shm_seg_t segment;
};
int PictureResourceAlloc (vout_display_t *vd, picture_resource_t *res, size_t size,
                          xcb_connection_t *conn, bool attach);
void PictureResourceFree (picture_resource_t *res, xcb_connection_t *conn);

