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

#include <sys/types.h>
#ifdef HAVE_SYS_SHM_H
# include <sys/shm.h>
# include <sys/stat.h>
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
 * Initialize a picture buffer as shared memory, according to the video output
 * format. If a attach is true, the segment is attached to
 * the X server (MIT-SHM extension).
 */
int XCB_pictures_Alloc (vout_display_t *vd, picture_resource_t *res,
                        size_t size, xcb_connection_t *conn, bool attach)
{
    res->p_sys = malloc (sizeof(*res->p_sys));
    if (!res->p_sys)
        return VLC_EGENERIC;

#ifdef HAVE_SYS_SHM_H
    /* Allocate shared memory segment */
    int id = shmget (IPC_PRIVATE, size, IPC_CREAT | S_IRWXU);
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

        switch (XCB_error_Check (vd, conn, "shared memory server-side error",
                                 ck))
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
                if (XCB_error_Check (vd, conn, "same error on retry", ck) == 0)
                    break;
                /* fall through */
            }

            default:
                msg_Info (vd, "using buggy X11 server - SSH proxying?");
                segment = 0;
        }
    }
    else
        segment = 0;

    shmctl (id, IPC_RMID, NULL);
    res->p_sys->segment = segment;
    res->p->p_pixels = shm;
#else
    assert (!attach);
    res->p_sys->segment = 0;

    /* XXX: align on 32 bytes for VLC chroma filters */
    res->p->p_pixels = malloc (size);
    if (unlikely(res->p->p_pixels == NULL))
    {
        free (res->p_sys);
        return VLC_EGENERIC;
    }
#endif
    return VLC_SUCCESS;
}

/**
 * Release picture private data: detach the shared memory segment.
 */
void XCB_pictures_Free (picture_resource_t *res, xcb_connection_t *conn)
{
#ifdef HAVE_SYS_SHM_H
    xcb_shm_seg_t segment = res->p_sys->segment;

    if (conn != NULL && segment != 0)
        xcb_shm_detach (conn, segment);
    shmdt (res->p->p_pixels);
#else
    free (res->p->p_pixels);
#endif
}

