/*****************************************************************************
 * access.c : V4L2 compressed byte stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2011 VLC authors and VideoLAN
 *
 * Authors: Benjamin Pracht <bigben at videolan dot org>
 *          Richard Hosking <richard at hovis dot net>
 *          Antoine Cellerier <dionoea at videolan d.t org>
 *          Dennis Lou <dlou99 at yahoo dot com>
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
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <poll.h>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_es.h>
#include <vlc_interrupt.h>

#include "v4l2.h"

typedef struct
{
    int fd;
    struct vlc_v4l2_buffers *pool;
    uint32_t blocksize;
    uint32_t block_flags;
    vlc_v4l2_ctrl_t *controls;
} access_sys_t;

/* Wait for data */
static int AccessPoll(stream_t *access)
{
    access_sys_t *sys = access->p_sys;
    struct pollfd ufd;

    ufd.fd = sys->fd;
    ufd.events = POLLIN;

    return vlc_poll_i11e(&ufd, 1, -1);
}

static block_t *MMapBlock(stream_t *access, bool *restrict eof)
{
    access_sys_t *sys = access->p_sys;

    if (AccessPoll(access))
        return NULL;

    block_t *block = GrabVideo(VLC_OBJECT(access), sys->pool);
    if (block != NULL)
    {
        block->i_pts = block->i_dts = vlc_tick_now();
        block->i_flags |= sys->block_flags;
    }
    (void) eof;
    return block;
}

static block_t *ReadBlock(stream_t *access, bool *restrict eof)
{
    access_sys_t *sys = access->p_sys;

    if (AccessPoll(access))
        return NULL;

    block_t *block = block_Alloc(sys->blocksize);
    if (unlikely(block == NULL))
        return NULL;

    ssize_t val = v4l2_read(sys->fd, block->p_buffer, block->i_buffer);
    if (val < 0)
    {
        block_Release(block);
        msg_Err(access, "cannot read buffer: %s", vlc_strerror_c(errno));
        *eof = true;
        return NULL;
    }

    block->i_buffer = val;
    return block;
}

static int AccessControl(stream_t *access, int query, va_list args)
{
    switch (query)
    {
        case STREAM_CAN_SEEK:
        case STREAM_CAN_FASTSEEK:
        case STREAM_CAN_PAUSE:
        case STREAM_CAN_CONTROL_PACE:
            *va_arg(args, bool *) = false;
            break;

        case STREAM_GET_PTS_DELAY:
            *va_arg(args,vlc_tick_t *) = VLC_TICK_FROM_MS(
                var_InheritInteger(access, "live-caching"));
            break;

        case STREAM_SET_PAUSE_STATE:
            /* Nothing to do */
            break;

        default:
            return VLC_EGENERIC;

    }
    return VLC_SUCCESS;
}

void AccessClose(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    access_sys_t *sys = access->p_sys;

    if (sys->pool != NULL)
        StopMmap(sys->pool);
    ControlsDeinit(vlc_object_parent(obj), sys->controls);
    v4l2_close(sys->fd);
    free(sys);
}

static int InitVideo(stream_t *access, int fd, uint32_t caps)
{
    access_sys_t *sys = access->p_sys;
    es_format_t es_fmt;

    if (SetupVideo(VLC_OBJECT(access), fd, caps, &es_fmt, &sys->blocksize,
                   &sys->block_flags))
        return -1;

    /* Init I/O method */
    if (caps & V4L2_CAP_STREAMING)
    {
        sys->pool = StartMmap (VLC_OBJECT(access), fd);
        if (sys->pool == NULL)
            return -1;
        access->pf_block = MMapBlock;
    }
    else if (caps & V4L2_CAP_READWRITE)
    {
        access->pf_block = ReadBlock;
    }
    else
    {
        msg_Err (access, "no supported capture method");
        return -1;
    }

    return 0;
}

int AccessOpen(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;

    if (access->b_preparsing)
        return VLC_EGENERIC;

    access_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    access->p_sys = sys;

    ParseMRL(obj, access->psz_location);

    char *path = var_InheritString(obj, CFG_PREFIX"dev");
    if (unlikely(path == NULL))
        goto error; /* probably OOM */

    uint32_t caps;
    int fd = OpenDevice(obj, path, &caps);
    free(path);
    if (fd == -1)
        goto error;
    sys->fd = fd;

    if (InitVideo(access, fd, caps))
    {
        v4l2_close(fd);
        goto error;
    }

    sys->controls = ControlsInit(vlc_object_parent(obj), fd);
    access->pf_seek = NULL;
    access->pf_control = AccessControl;
    return VLC_SUCCESS;
error:
    free(sys);
    return VLC_EGENERIC;
}
