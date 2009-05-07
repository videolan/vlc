/**
 * @file common.c
 * @brief Common code for XCB video output plugins
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/shm.h>

#include <xcb/xcb.h>
#include <xcb/shm.h>

#include <vlc_common.h>
#include <vlc_vout.h>
#include <vlc_window.h>

#include "xcb_vlc.h"

/**
 * Connect to the X server.
 */
xcb_connection_t *Connect (vlc_object_t *obj)
{
    char *display = var_CreateGetNonEmptyString (obj, "x11-display");
    xcb_connection_t *conn = xcb_connect (display, NULL);

    free (display);
    if (xcb_connection_has_error (conn) /*== NULL*/)
    {
        msg_Err (obj, "cannot connect to X server");
        xcb_disconnect (conn);
        return NULL;
    }
    return conn;
}


/**
 * Create a VLC video X window object, find the corresponding X server screen,
 * and probe the MIT-SHM extension.
 */
vout_window_t *GetWindow (vout_thread_t *obj,
                          xcb_connection_t *conn,
                          const xcb_screen_t **restrict pscreen,
                          bool *restrict pshm)
{
    /* Get window */
    xcb_window_t root;
    vout_window_t *wnd = vout_RequestXWindow (obj, &(int){ 0 }, &(int){ 0 },
                                        &(unsigned){ 0 }, &(unsigned){ 0 });
    if (wnd == NULL)
    {
        msg_Err (obj, "parent window not available");
        return NULL;
    }
    else
    {
        xcb_get_geometry_reply_t *geo;
        xcb_get_geometry_cookie_t ck;

        ck = xcb_get_geometry (conn, wnd->handle.xid);
        geo = xcb_get_geometry_reply (conn, ck, NULL);
        if (geo == NULL)
        {
            msg_Err (obj, "parent window not valid");
            goto error;
        }
        root = geo->root;
        free (geo);

        /* Subscribe to parent window resize events */
        const uint32_t value = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
        xcb_change_window_attributes (conn, wnd->handle.xid,
                                      XCB_CW_EVENT_MASK, &value);
    }

    /* Find the selected screen */
    const xcb_setup_t *setup = xcb_get_setup (conn);
    xcb_screen_t *screen = NULL;
    for (xcb_screen_iterator_t i = xcb_setup_roots_iterator (setup);
         i.rem > 0 && screen == NULL; xcb_screen_next (&i))
    {
        if (i.data->root == root)
            screen = i.data;
    }

    if (screen == NULL)
    {
        msg_Err (obj, "parent window screen not found");
        goto error;
    }
    msg_Dbg (obj, "using screen 0x%"PRIx32, root);

    /* Check MIT-SHM shared memory support */
    bool shm = var_CreateGetBool (obj, "x11-shm") > 0;
    if (shm)
    {
        xcb_shm_query_version_cookie_t ck;
        xcb_shm_query_version_reply_t *r;

        ck = xcb_shm_query_version (conn);
        r = xcb_shm_query_version_reply (conn, ck, NULL);
        if (!r)
        {
            msg_Err (obj, "shared memory (MIT-SHM) not available");
            msg_Warn (obj, "display will be slow");
            shm = false;
        }
        free (r);
    }

    *pscreen = screen;
    *pshm = shm;
    return wnd;

error:
    vout_ReleaseWindow (wnd);
    return NULL;
}

/**
 * Gets the size of an X window.
 */
int GetWindowSize (struct vout_window_t *wnd, xcb_connection_t *conn,
                   unsigned *restrict width, unsigned *restrict height)
{
    xcb_get_geometry_cookie_t ck = xcb_get_geometry (conn, wnd->handle.xid);
    xcb_get_geometry_reply_t *geo = xcb_get_geometry_reply (conn, ck, NULL);

    if (!geo)
        return -1;

    *width = geo->width;
    *height = geo->height;
    free (geo);
    return 0;
}

/**
 * Initialize a picture buffer as shared memory, according to the video output
 * format. If a XCB connection pointer is supplied, the segment is attached to
 * the X server (MIT-SHM extension).
 */
int PictureAlloc (vout_thread_t *vout, picture_t *pic, size_t size,
                  xcb_connection_t *conn)
{
    assert (pic->i_status == FREE_PICTURE);

    /* Allocate shared memory segment */
    int id = shmget (IPC_PRIVATE, size, IPC_CREAT | 0700);
    if (id == -1)
    {
        msg_Err (vout, "shared memory allocation error: %m");
        return VLC_EGENERIC;
    }

    /* Attach the segment to VLC */
    void *shm = shmat (id, NULL, 0 /* read/write */);
    if (-1 == (intptr_t)shm)
    {
        msg_Err (vout, "shared memory attachment error: %m");
        shmctl (id, IPC_RMID, 0);
        return VLC_EGENERIC;
    }

    xcb_shm_seg_t segment;
    if (conn != NULL)
    {
        /* Attach the segment to X */
        xcb_void_cookie_t ck;

        segment = xcb_generate_id (conn);
        ck = xcb_shm_attach_checked (conn, segment, id, 1);

        if (CheckError (vout, "shared memory server-side error", ck))
        {
            msg_Info (vout, "using buggy X11 server - SSH proxying?");
            segment = 0;
        }
    }
    else
        segment = 0;

    shmctl (id, IPC_RMID, 0);
    pic->p_sys = (void *)(uintptr_t)segment;
    pic->p->p_pixels = shm;
    pic->i_status = DESTROYED_PICTURE;
    pic->i_type = DIRECT_PICTURE;
    return VLC_SUCCESS;
}

/**
 * Release picture private data: detach the shared memory segment.
 */
void PictureFree (picture_t *pic, xcb_connection_t *conn)
{
    xcb_shm_seg_t segment = (uintptr_t)pic->p_sys;

    if (segment != 0)
    {
        assert (conn != NULL);
        xcb_shm_detach (conn, segment);
    }
    shmdt (pic->p->p_pixels);
}

/**
 * Video output thread management stuff.
 * FIXME: Much of this should move to core
 */
void CommonManage (vout_thread_t *vout)
{
    if (vout->i_changes & VOUT_SCALE_CHANGE)
    {
        vout->b_autoscale = var_GetBool (vout, "autoscale");
        vout->i_zoom = ZOOM_FP_FACTOR;
        vout->i_changes &= ~VOUT_SCALE_CHANGE;
        vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    if (vout->i_changes & VOUT_ZOOM_CHANGE)
    {
        vout->b_autoscale = false;
        vout->i_zoom = var_GetFloat (vout, "scale") * ZOOM_FP_FACTOR;
        vout->i_changes &= ~VOUT_ZOOM_CHANGE;
        vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    if (vout->i_changes & VOUT_CROP_CHANGE)
    {
        vout->fmt_out.i_x_offset = vout->fmt_in.i_x_offset;
        vout->fmt_out.i_y_offset = vout->fmt_in.i_y_offset;
        vout->fmt_out.i_visible_width = vout->fmt_in.i_visible_width;
        vout->fmt_out.i_visible_height = vout->fmt_in.i_visible_height;
        vout->i_changes &= ~VOUT_CROP_CHANGE;
        vout->i_changes |= VOUT_SIZE_CHANGE;
    }

    if (vout->i_changes & VOUT_ASPECT_CHANGE)
    {
        vout->fmt_out.i_aspect = vout->fmt_in.i_aspect;
        vout->fmt_out.i_sar_num = vout->fmt_in.i_sar_num;
        vout->fmt_out.i_sar_den = vout->fmt_in.i_sar_den;
        vout->output.i_aspect = vout->fmt_in.i_aspect;
        vout->i_changes &= ~VOUT_ASPECT_CHANGE;
        vout->i_changes |= VOUT_SIZE_CHANGE;
    }
}
