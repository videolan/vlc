/**
 * @file common.c
 * @brief Common code for XCB video output plugins
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
#include <vlc_vout_display.h>

#include "xcb_vlc.h"

/**
 * Connect to the X server.
 */
static xcb_connection_t *Connect (vlc_object_t *obj, const char *display)
{
    xcb_connection_t *conn = xcb_connect (display, NULL);
    if (xcb_connection_has_error (conn) /*== NULL*/)
    {
        msg_Err (obj, "cannot connect to X server (%s)",
                 display ? display : "default");
        xcb_disconnect (conn);
        return NULL;
    }

    const xcb_setup_t *setup = xcb_get_setup (conn);
    msg_Dbg (obj, "connected to X%"PRIu16".%"PRIu16" server",
             setup->protocol_major_version, setup->protocol_minor_version);
    char *vendor = strndup (xcb_setup_vendor (setup), setup->vendor_len);
    if (vendor)
    {
        msg_Dbg (obj, " vendor : %s", vendor);
        free (vendor);
    }
    msg_Dbg (obj, " version: %"PRIu32, setup->release_number);
    return conn;
}

/**
 * Find screen matching a given root window.
 */
static const xcb_screen_t *FindScreen (vlc_object_t *obj,
                                       xcb_connection_t *conn,
                                       xcb_window_t root)
{
    /* Find the selected screen */
    const xcb_setup_t *setup = xcb_get_setup (conn);
    const xcb_screen_t *screen = NULL;
    for (xcb_screen_iterator_t i = xcb_setup_roots_iterator (setup);
         i.rem > 0 && screen == NULL; xcb_screen_next (&i))
    {
        if (i.data->root == root)
            screen = i.data;
    }

    if (screen == NULL)
    {
        msg_Err (obj, "parent window screen not found");
        return NULL;
    }
    msg_Dbg (obj, "using screen 0x%"PRIx32, root);
    return screen;
}

static const xcb_screen_t *FindWindow (vlc_object_t *obj,
                                       xcb_connection_t *conn,
                                       xcb_window_t xid,
                                       uint8_t *restrict pdepth)
{
    xcb_get_geometry_reply_t *geo =
        xcb_get_geometry_reply (conn, xcb_get_geometry (conn, xid), NULL);
    if (geo == NULL)
    {
        msg_Err (obj, "parent window not valid");
        return NULL;
    }

    const xcb_screen_t *screen = FindScreen (obj, conn, geo->root);
    *pdepth = geo->depth;
    free (geo);
    return screen;
}


/**
 * Create a VLC video X window object, connect to the corresponding X server,
 * find the corresponding X server screen.
 */
vout_window_t *GetWindow (vout_display_t *vd,
                          xcb_connection_t **restrict pconn,
                          const xcb_screen_t **restrict pscreen,
                          uint8_t *restrict pdepth)
{
    /* Get window */
    vout_window_cfg_t wnd_cfg;

    memset( &wnd_cfg, 0, sizeof(wnd_cfg) );
    wnd_cfg.type = VOUT_WINDOW_TYPE_XID;
    wnd_cfg.x = var_InheritInteger (vd, "video-x");
    wnd_cfg.y = var_InheritInteger (vd, "video-y");
    wnd_cfg.width  = vd->cfg->display.width;
    wnd_cfg.height = vd->cfg->display.height;

    vout_window_t *wnd = vout_display_NewWindow (vd, &wnd_cfg);
    if (wnd == NULL)
    {
        msg_Err (vd, "parent window not available");
        return NULL;
    }

    xcb_connection_t *conn = Connect (VLC_OBJECT(vd), wnd->display.x11);
    if (conn == NULL)
        goto error;
    *pconn = conn;

    *pscreen = FindWindow (VLC_OBJECT(vd), conn, wnd->handle.xid, pdepth);
    if (*pscreen == NULL)
    {
        xcb_disconnect (conn);
        goto error;
    }

    RegisterMouseEvents (VLC_OBJECT(vd), conn, wnd->handle.xid);
    return wnd;

error:
    vout_display_DeleteWindow (vd, wnd);
    return NULL;
}

/** Check MIT-SHM shared memory support */
void CheckSHM (vlc_object_t *obj, xcb_connection_t *conn, bool *restrict pshm)
{
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
    *pshm = shm;
}

/**
 * Initialize a picture buffer as shared memory, according to the video output
 * format. If a attach is true, the segment is attached to
 * the X server (MIT-SHM extension).
 */
int PictureResourceAlloc (vout_display_t *vd, picture_resource_t *res, size_t size,
                          xcb_connection_t *conn, bool attach)
{
    res->p_sys = malloc (sizeof(*res->p_sys));
    if (!res->p_sys)
        return VLC_EGENERIC;

    /* Allocate shared memory segment */
    int id = shmget (IPC_PRIVATE, size, IPC_CREAT | 0700);
    if (id == -1)
    {
        msg_Err (vd, "shared memory allocation error: %m");
        free (res->p_sys);
        return VLC_EGENERIC;
    }

    /* Attach the segment to VLC */
    void *shm = shmat (id, NULL, 0 /* read/write */);
    if (-1 == (intptr_t)shm)
    {
        msg_Err (vd, "shared memory attachment error: %m");
        shmctl (id, IPC_RMID, 0);
        free (res->p_sys);
        return VLC_EGENERIC;
    }

    xcb_shm_seg_t segment;
    if (attach)
    {
        /* Attach the segment to X */
        xcb_void_cookie_t ck;

        segment = xcb_generate_id (conn);
        ck = xcb_shm_attach_checked (conn, segment, id, 1);

        if (CheckError (vd, conn, "shared memory server-side error", ck))
        {
            msg_Info (vd, "using buggy X11 server - SSH proxying?");
            segment = 0;
        }
    }
    else
        segment = 0;

    shmctl (id, IPC_RMID, 0);
    res->p_sys->segment = segment;
    res->p->p_pixels = shm;
    return VLC_SUCCESS;
}

/**
 * Release picture private data: detach the shared memory segment.
 */
void PictureResourceFree (picture_resource_t *res, xcb_connection_t *conn)
{
    xcb_shm_seg_t segment = res->p_sys->segment;

    if (conn != NULL && segment != 0)
        xcb_shm_detach (conn, segment);
    shmdt (res->p->p_pixels);
}

