/**
 * @file pictures.c
 * @brief Pictures management code for XCB video output plugins
 */
/*****************************************************************************
 * Copyright © 2009-2013 Rémi Denis-Courmont
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#ifdef HAVE_SYS_SHM_H
# include <sys/shm.h>
# include <sys/stat.h>
#else
# define shmdt(mem) free(mem)
#endif

#include <xcb/xcb.h>
#include <xcb/shm.h>

#include <vlc_common.h>
#include <vlc_vout_display.h>

#include "pictures.h"
#include "events.h"

/** Check MIT-SHM shared memory support */
bool XCB_shm_Check (vlc_object_t *obj, xcb_connection_t *conn)
{
#ifdef HAVE_SYS_SHM_H
    xcb_shm_query_version_cookie_t ck;
    xcb_shm_query_version_reply_t *r;

    ck = xcb_shm_query_version (conn);
    r = xcb_shm_query_version_reply (conn, ck, NULL);
    if (r != NULL)
    {
        free (r);
        return true;
    }
    msg_Err (obj, "shared memory (MIT-SHM) not available");
    msg_Warn (obj, "display will be slow");
#else
    msg_Warn (obj, "shared memory (MIT-SHM) not implemented");
    (void) conn;
#endif
    return false;
}

/**
 * Release picture private data: detach the shared memory segment.
 */
static void XCB_picture_Destroy (picture_t *pic)
{
    shmdt (pic->p[0].p_pixels);
    free (pic);
}

/**
 * Initialize a picture buffer as shared memory, according to the video output
 * format. If a attach is true, the segment is attached to
 * the X server (MIT-SHM extension).
 */
int XCB_picture_Alloc (vout_display_t *vd, picture_resource_t *res,
                       size_t size, xcb_connection_t *conn,
                       xcb_shm_seg_t segment)
{
#ifdef HAVE_SYS_SHM_H
    /* Allocate shared memory segment */
    int id = shmget (IPC_PRIVATE, size, IPC_CREAT | S_IRWXU);
    if (id == -1)
    {
        msg_Err (vd, "shared memory allocation error: %s",
                 vlc_strerror_c(errno));
        return -1;
    }

    /* Attach the segment to VLC */
    void *shm = shmat (id, NULL, 0 /* read/write */);
    if (-1 == (intptr_t)shm)
    {
        msg_Err (vd, "shared memory attachment error: %s",
                 vlc_strerror_c(errno));
        shmctl (id, IPC_RMID, 0);
        return -1;
    }

    if (segment != 0)
    {   /* Attach the segment to X */
        xcb_void_cookie_t ck = xcb_shm_attach_checked (conn, segment, id, 1);
        switch (vlc_xcb_error_Check(vd, conn,
                                    "shared memory server-side error", ck))
        {
            case 0:
                break;

            case XCB_ACCESS:
            {
                struct shmid_ds buf;
                /* Retry with promiscuous permissions */
                shmctl (id, IPC_STAT, &buf);
                buf.shm_perm.mode |= S_IRGRP|S_IROTH;
                shmctl (id, IPC_SET, &buf);
                ck = xcb_shm_attach_checked (conn, segment, id, 1);
                if (vlc_xcb_error_Check(vd, conn, "same error on retry",
                                        ck) == 0)
                    break;
            }   /* fall through */

            default:
                msg_Info (vd, "using buggy X11 server - SSH proxying?");
                segment = 0;
        }
    }

    shmctl (id, IPC_RMID, NULL);
#else
    assert (segment == 0);

    /* XXX: align on 32 bytes for VLC chroma filters */
    void *shm = malloc (size);
    if (unlikely(shm == NULL))
        return -1;
#endif
    res->p_sys = (void *)(uintptr_t)segment;
    res->pf_destroy = XCB_picture_Destroy;
    res->p[0].p_pixels = shm;
    return 0;
}

picture_t *XCB_picture_NewFromResource (const video_format_t *restrict fmt,
                                        const picture_resource_t *restrict res,
                                        xcb_connection_t *conn)
{
    picture_t *pic = picture_NewFromResource (fmt, res);
    if (unlikely(pic == NULL))
    {
        xcb_shm_seg_t seg = (uintptr_t)res->p_sys;

        if (seg != 0)
            xcb_shm_detach (conn, seg);
        shmdt (res->p[0].p_pixels);
    }
    return pic;
}
