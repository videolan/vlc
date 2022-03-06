/*****************************************************************************
 * buffers.c: Video4Linux2 input module for vlc
 *****************************************************************************
 * Copyright (C) 2002-2009 VLC authors and VideoLAN
 * Copyright (C) 2011-2012 Rémi Denis-Courmont
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
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <vlc_common.h>
#include <vlc_block.h>

#include "v4l2.h"

vlc_tick_t GetBufferPTS(const struct v4l2_buffer *buf)
{
    vlc_tick_t pts;

    switch (buf->flags & V4L2_BUF_FLAG_TIMESTAMP_MASK)
    {
        case V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC:
            pts = vlc_tick_from_timeval(&buf->timestamp);
            break;
        case V4L2_BUF_FLAG_TIMESTAMP_UNKNOWN:
        default:
            pts = vlc_tick_now();
            break;
    }
    return pts;
}

block_t *GrabVideo(vlc_object_t *demux, int fd,
                   struct vlc_v4l2_buffers *restrict pool)
{
    struct v4l2_buffer buf_req = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    /* Wait for next frame */
    if (v4l2_ioctl(fd, VIDIOC_DQBUF, &buf_req) < 0)
    {
        switch (errno)
        {
            case EAGAIN:
                return NULL;
            case EIO:
                /* Could ignore EIO, see spec. */
                /* fall through */
            default:
                msg_Err(demux, "dequeue error: %s", vlc_strerror_c(errno));
                return NULL;
        }
    }

    assert(buf_req.index < pool->count);

    /* Copy frame */
    struct vlc_v4l2_buffer *buf = pool->bufs + buf_req.index;
    block_t *block = block_Alloc(buf_req.bytesused);
    if (unlikely(block == NULL))
        return NULL;
    block->i_pts = block->i_dts = GetBufferPTS(&buf_req);
    assert(buf_req.bytesused <= buf->length);
    memcpy(block->p_buffer, buf->base, buf_req.bytesused);

    /* Unlock */
    if (v4l2_ioctl(fd, VIDIOC_QBUF, &buf_req) < 0)
    {
        msg_Err(demux, "queue error: %s", vlc_strerror_c(errno));
        block_Release(block);
        block = NULL;
    }
    return block;
}

/**
 * Allocates memory-mapped buffers, queues them and start streaming.
 * @param n requested buffers count
 * @return array of allocated buffers (use free()), or NULL on error.
 */
struct vlc_v4l2_buffers *StartMmap(vlc_object_t *obj, int fd, unsigned int n)
{
    struct vlc_v4l2_buffers *pool;
    struct v4l2_requestbuffers req = {
        .count = n,
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };

    if (v4l2_ioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    {
        msg_Err(obj, "cannot allocate buffers: %s", vlc_strerror_c(errno));
        return NULL;
    }

    if (req.count < 2)
    {
        msg_Err(obj, "cannot allocate enough buffers");
        return NULL;
    }

    pool = malloc(sizeof (*pool) + req.count * sizeof (pool->bufs[0]));
    if (unlikely(pool == NULL))
        return NULL;

    pool->count = 0;

    while (pool->count < req.count)
    {
        struct vlc_v4l2_buffer *const buf = pool->bufs + pool->count;
        struct v4l2_buffer buf_req = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_MMAP,
            .index = pool->count,
        };

        if (v4l2_ioctl(fd, VIDIOC_QUERYBUF, &buf_req) < 0)
        {
            msg_Err(obj, "cannot query buffer %zu: %s", pool->count,
                    vlc_strerror_c(errno));
            goto error;
        }

        buf->length = buf_req.length;
        buf->base = v4l2_mmap(NULL, buf->length, PROT_READ | PROT_WRITE,
                              MAP_SHARED, fd, buf_req.m.offset);
        if (buf->base == MAP_FAILED)
        {
            msg_Err(obj, "cannot map buffer %"PRIu32": %s", buf_req.index,
                    vlc_strerror_c(errno));
            goto error;
        }

        pool->count++;

        /* Some drivers refuse to queue buffers before they are mapped. Bug? */
        if (v4l2_ioctl(fd, VIDIOC_QBUF, &buf_req) < 0)
        {
            msg_Err(obj, "cannot queue buffer %"PRIu32": %s", buf_req.index,
                     vlc_strerror_c(errno));
            goto error;
        }
    }

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v4l2_ioctl(fd, VIDIOC_STREAMON, &type) < 0)
    {
        msg_Err (obj, "cannot start streaming: %s", vlc_strerror_c(errno));
        goto error;
    }
    return pool;
error:
    StopMmap(fd, pool);
    return NULL;
}

void StopMmap(int fd, struct vlc_v4l2_buffers *pool)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    /* STREAMOFF implicitly dequeues all buffers */
    v4l2_ioctl(fd, VIDIOC_STREAMOFF, &type);

    for (size_t i = 0; i < pool->count; i++)
        v4l2_munmap(pool->bufs[i].base, pool->bufs[i].length);

    free(pool);
}

/**
 * Allocates user pointer buffers, and start streaming.
 */
int StartUserPtr(vlc_object_t *obj, int fd)
{
    struct v4l2_requestbuffers reqbuf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_USERPTR,
        .count = 2,
    };

    if (v4l2_ioctl(fd, VIDIOC_REQBUFS, &reqbuf) < 0)
    {
        if (errno != EINVAL)
            msg_Err(obj, "cannot reserve user buffers: %s",
                    vlc_strerror_c(errno));
        else
            msg_Dbg(obj, "user buffers not supported");
        return -1;
    }
    if (v4l2_ioctl(fd, VIDIOC_STREAMON, &reqbuf.type) < 0)
    {
        msg_Err(obj, "cannot start streaming: %s", vlc_strerror_c(errno));
        return -1;
    }
    return 0;
}
