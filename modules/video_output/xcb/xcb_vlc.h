/**
 * @file xcb_vlc.h
 * @brief X C Bindings VLC module common header
 */
/*****************************************************************************
 * Copyright © 2009 Rémi Denis-Courmont
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2.0
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 ****************************************************************************/

#ifdef WORDS_BIGENDIAN
# define ORDER XCB_IMAGE_ORDER_MSB_FIRST
#else
# define ORDER XCB_IMAGE_ORDER_LSB_FIRST
#endif

int CheckError (vout_thread_t *, const char *str, xcb_void_cookie_t);
int ProcessEvent (vout_thread_t *, xcb_connection_t *, xcb_window_t,
                  xcb_generic_event_t *);
void HandleParentStructure (vout_thread_t *vout, xcb_connection_t *conn,
                          xcb_window_t xid, xcb_configure_notify_event_t *ev);


/* keys.c */
typedef struct key_handler_t key_handler_t;
key_handler_t *CreateKeyHandler (vlc_object_t *, xcb_connection_t *);
void DestroyKeyHandler (key_handler_t *);
int ProcessKeyEvent (key_handler_t *, xcb_generic_event_t *);

/* common.c */
struct vout_window_t;

xcb_connection_t *Connect (vlc_object_t *obj);
struct vout_window_t *GetWindow (vout_thread_t *obj,
                                 xcb_connection_t *pconn,
                                 const xcb_screen_t **restrict pscreen,
                                 bool *restrict pshm);
int GetWindowSize (struct vout_window_t *wnd, xcb_connection_t *conn,
                   unsigned *restrict width, unsigned *restrict height);
int PictureAlloc (vout_thread_t *, picture_t *, size_t, xcb_connection_t *);
void PictureFree (picture_t *pic, xcb_connection_t *conn);
void CommonManage (vout_thread_t *);
