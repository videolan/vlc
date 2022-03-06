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

static void ReleaseBuffer(block_t *block)
{
    struct vlc_v4l2_buffer *buf = container_of(block, struct vlc_v4l2_buffer,
                                               block);
    struct vlc_v4l2_buffers *pool = buf->pool;
    uint32_t index = buf - pool->bufs;
    struct v4l2_buffer buf_req = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
        .index = index,
    };
    uint32_t mask;
    int fd;

    vlc_mutex_lock(&pool->lock);
    mask = atomic_fetch_and_explicit(&pool->inflight, ~(1U << index),
                                     memory_order_relaxed);
    fd = pool->fd;
    vlc_mutex_unlock(&pool->lock);
    assert(mask & (1U << index));

    assert(mask & (1U << index));

    if (likely(fd >= 0)) {
        /* Requeue the freed buffer */
        v4l2_ioctl(pool->fd, VIDIOC_QBUF, &buf_req);
        return;
    }

    v4l2_munmap(block->p_start, block->i_size);

    if (vlc_popcount(mask) == 1) /* last active buffer? */
        free(pool);
}

static const struct vlc_block_callbacks vlc_v4l2_buffer_cbs = {
    ReleaseBuffer,
};

block_t *GrabVideo(vlc_object_t *demux, struct vlc_v4l2_buffers *restrict pool)
{
    int fd = pool->fd;
    struct v4l2_buffer buf_req = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    uint32_t mask;

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
    mask = atomic_fetch_or_explicit(&pool->inflight, 1U << buf_req.index,
                                    memory_order_relaxed);

    struct vlc_v4l2_buffer *buf = pool->bufs + buf_req.index;
    block_t *block = &buf->block;
    /* Reinitialise the buffer */
    block->p_buffer = block->p_start;
    assert(buf_req.bytesused <= block->i_size);
    block->i_buffer = buf_req.bytesused;
    block->p_next = NULL;

    if ((size_t)vlc_popcount(mask) == pool->count - 1) {
        /* Running out of buffers! Memory copy forced. */
        block = block_Duplicate(block);
        block_Release(&buf->block);
    }

    block->i_pts = block->i_dts = GetBufferPTS(&buf_req);
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

    pool->fd = fd;
    pool->inflight = 0;
    pool->count = 0;
    vlc_mutex_init(&pool->lock);

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

        void *base = v4l2_mmap(NULL, buf_req.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, buf_req.m.offset);
        if (base == MAP_FAILED)
        {
            msg_Err(obj, "cannot map buffer %"PRIu32": %s", buf_req.index,
                    vlc_strerror_c(errno));
            goto error;
        }

        block_Init(&buf->block, &vlc_v4l2_buffer_cbs, base, buf_req.length);
        buf->pool = pool;
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
    StopMmap(pool);
    return NULL;
}

void StopMmap(struct vlc_v4l2_buffers *pool)
{
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    const size_t count = pool->count;
    uint32_t unused;

    /* STREAMOFF implicitly dequeues all buffers */
    v4l2_ioctl(pool->fd, VIDIOC_STREAMOFF, &type);

    vlc_mutex_lock(&pool->lock);
    pool->fd = -1;
    unused = (~atomic_load_explicit(&pool->inflight, memory_order_relaxed))
             & ((UINT64_C(1) << count) - 1);
    atomic_fetch_or_explicit(&pool->inflight, unused, memory_order_relaxed);
    vlc_mutex_unlock(&pool->lock);

    for (size_t i = 0; i < count; i++)
        if (unused & (1u << i))
            block_Release(&pool->bufs[i].block);
    /* Pool is freed whence all buffers are released (possibly here) */
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
