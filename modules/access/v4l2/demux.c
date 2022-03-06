/*****************************************************************************
 * demux.c : V4L2 raw video demux module for vlc
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

#include <math.h>
#include <errno.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif
#include <poll.h>

#include <vlc_common.h>
#include <vlc_demux.h>

#include "v4l2.h"

typedef struct
{
    int fd;
    vlc_thread_t thread;

    struct vlc_v4l2_buffers *pool;
    uint32_t blocksize;
    uint32_t block_flags;

    es_out_id_t *es;
    vlc_v4l2_ctrl_t *controls;
    vlc_tick_t start;
    vlc_tick_t interval;

#ifdef ZVBI_COMPILED
    vlc_v4l2_vbi_t *vbi;
#endif
} demux_sys_t;

/** Allocates and queue a user buffer using mmap(). */
static block_t *UserPtrQueue(vlc_object_t *obj, int fd, size_t length)
{
    void *ptr = mmap(NULL, length, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED)
    {
        msg_Err(obj, "cannot allocate %zu-bytes buffer: %s", length,
                vlc_strerror_c(errno));
        return NULL;
    }

    block_t *block = block_mmap_Alloc(ptr, length);
    if (unlikely(block == NULL))
    {
        munmap(ptr, length);
        return NULL;
    }

    struct v4l2_buffer buf = {
        .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_USERPTR,
        .m = {
            .userptr = (uintptr_t)ptr,
        },
        .length = length,
    };

    if (v4l2_ioctl(fd, VIDIOC_QBUF, &buf) < 0)
    {
        msg_Err(obj, "cannot queue buffer: %s", vlc_strerror_c(errno));
        block_Release(block);
        return NULL;
    }
    return block;
}

static void *UserPtrThread(void *data)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;
    int fd = sys->fd;
    struct pollfd ufd[2];
    nfds_t numfds = 1;

    ufd[0].fd = fd;
    ufd[0].events = POLLIN;

    int canc = vlc_savecancel();
    for (;;)
    {
        struct v4l2_buffer buf = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
            .memory = V4L2_MEMORY_USERPTR,
        };
        block_t *block = UserPtrQueue(VLC_OBJECT(demux), fd, sys->blocksize);
        if (block == NULL)
            break;

        /* Wait for data */
        vlc_restorecancel(canc);
        block_cleanup_push(block);
        while (poll(ufd, numfds, -1) == -1)
           if (errno != EINTR)
               msg_Err(demux, "poll error: %s", vlc_strerror_c(errno));
        vlc_cleanup_pop();
        canc = vlc_savecancel();

        if (v4l2_ioctl(fd, VIDIOC_DQBUF, &buf) < 0)
        {
            msg_Err(demux, "cannot dequeue buffer: %s",
                    vlc_strerror_c(errno));
            block_Release(block);
            continue;
        }

        assert(block->p_buffer == (void *)buf.m.userptr);
        block->i_buffer = buf.length;
        block->i_pts = block->i_dts = GetBufferPTS(&buf);
        block->i_flags |= sys->block_flags;
        es_out_SetPCR(demux->out, block->i_pts);
        es_out_Send(demux->out, sys->es, block);
    }
    vlc_restorecancel(canc); /* <- hmm, this is purely cosmetic */
    return NULL;
}

static void *MmapThread(void *data)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;
    int fd = sys->fd;
    struct pollfd ufd[2];
    nfds_t numfds = 1;

    ufd[0].fd = fd;
    ufd[0].events = POLLIN;

#ifdef ZVBI_COMPILED
    if (sys->vbi != NULL)
    {
        ufd[1].fd = GetFdVBI(sys->vbi);
        ufd[1].events = POLLIN;
        numfds++;
    }
#endif

    for (;;)
    {
        /* Wait for data */
        if (poll(ufd, numfds, -1) == -1)
        {
           if (errno != EINTR)
               msg_Err(demux, "poll error: %s", vlc_strerror_c(errno));
           continue;
        }

        if (ufd[0].revents)
        {
            int canc = vlc_savecancel();
            block_t *block = GrabVideo(VLC_OBJECT(demux), sys->pool);
            if (block != NULL)
            {
                block->i_flags |= sys->block_flags;
                es_out_SetPCR(demux->out, block->i_pts);
                es_out_Send(demux->out, sys->es, block);
            }
            vlc_restorecancel(canc);
        }
#ifdef ZVBI_COMPILED
        if (sys->vbi != NULL && ufd[1].revents)
            GrabVBI(demux, sys->vbi);
#endif
    }

    vlc_assert_unreachable();
}

static void *ReadThread(void *data)
{
    demux_t *demux = data;
    demux_sys_t *sys = demux->p_sys;
    int fd = sys->fd;
    struct pollfd ufd[2];
    nfds_t numfds = 1;

    ufd[0].fd = fd;
    ufd[0].events = POLLIN;

#ifdef ZVBI_COMPILED
    if (sys->vbi != NULL)
    {
        ufd[1].fd = GetFdVBI(sys->vbi);
        ufd[1].events = POLLIN;
        numfds++;
    }
#endif

    for (;;)
    {
        /* Wait for data */
        if (poll(ufd, numfds, -1) == -1)
        {
           if (errno != EINTR)
               msg_Err(demux, "poll error: %s", vlc_strerror_c(errno));
           continue;
        }

        if (ufd[0].revents)
        {
            block_t *block = block_Alloc(sys->blocksize);
            if (unlikely(block == NULL))
            {
                msg_Err(demux, "read error: %s", vlc_strerror_c(errno));
                v4l2_read(fd, NULL, 0); /* discard frame */
                continue;
            }
            block->i_pts = block->i_dts = vlc_tick_now();
            block->i_flags |= sys->block_flags;

            int canc = vlc_savecancel();
            ssize_t val = v4l2_read(fd, block->p_buffer, block->i_buffer);
            if (val != -1)
            {
                block->i_buffer = val;
                es_out_SetPCR(demux->out, block->i_pts);
                es_out_Send(demux->out, sys->es, block);
            }
            else
                block_Release(block);
            vlc_restorecancel(canc);
        }
#ifdef ZVBI_COMPILED
        if (sys->vbi != NULL && ufd[1].revents)
            GrabVBI(demux, sys->vbi);
#endif
    }
    vlc_assert_unreachable();
}

static int DemuxControl( demux_t *demux, int query, va_list args )
{
    demux_sys_t *sys = demux->p_sys;

    switch( query )
    {
        /* Special for access_demux */
        case DEMUX_CAN_PAUSE:
        case DEMUX_CAN_SEEK:
        case DEMUX_CAN_CONTROL_PACE:
            *va_arg( args, bool * ) = false;
            return VLC_SUCCESS;

        case DEMUX_GET_PTS_DELAY:
        {
            vlc_tick_t *pd = va_arg(args, vlc_tick_t *);

            *pd = VLC_TICK_FROM_MS(var_InheritInteger(demux, "live-caching"));
            if (*pd > sys->interval)
                *pd = sys->interval; /* cap at one frame, more than enough */
            return VLC_SUCCESS;
        }

        case DEMUX_GET_TIME:
            *va_arg (args, vlc_tick_t *) = vlc_tick_now() - sys->start;
            return VLC_SUCCESS;

        /* TODO implement others */
        default:
            return VLC_EGENERIC;
    }

    return VLC_EGENERIC;
}

static int InitVideo (demux_t *demux, int fd, uint32_t caps)
{
    demux_sys_t *sys = demux->p_sys;
    es_format_t es_fmt;

    if (SetupVideo(VLC_OBJECT(demux), fd, caps, &es_fmt,
                   &sys->blocksize, &sys->block_flags))
        return -1;
    if (es_fmt.i_codec == 0)
        return -1; /* defer to access */

    sys->interval = vlc_tick_from_samples(es_fmt.video.i_frame_rate,
                                          es_fmt.video.i_frame_rate_base);
    sys->es = es_out_Add (demux->out, &es_fmt);

    /* Init I/O method */
    void *(*entry) (void *);
    if (caps & V4L2_CAP_STREAMING)
    {
        if (StartUserPtr(VLC_OBJECT(demux), fd) == 0)
        {
            /* In principles, mmap() will pad the length to a multiple of the
             * page size, so there is no need to care. Nevertheless with the
             * page size, block->i_size can be set optimally. */
            const long pagemask = sysconf (_SC_PAGE_SIZE) - 1;

            sys->pool = NULL;
            sys->blocksize = (sys->blocksize + pagemask) & ~pagemask;
            entry = UserPtrThread;
            msg_Dbg (demux, "streaming with %"PRIu32"-bytes user buffers",
                     sys->blocksize);
        }
        else /* fall back to memory map */
        {
            sys->pool = StartMmap(VLC_OBJECT(demux), fd, 16);
            if (sys->pool == NULL)
                return -1;
            entry = MmapThread;
            msg_Dbg(demux, "streaming with %zu memory-mapped buffers",
                    sys->pool->count);
        }
    }
    else if (caps & V4L2_CAP_READWRITE)
    {
        sys->pool = NULL;
        entry = ReadThread;
        msg_Dbg (demux, "reading %"PRIu32" bytes at a time", sys->blocksize);
    }
    else
    {
        msg_Err (demux, "no supported capture method");
        return -1;
    }

#ifdef ZVBI_COMPILED
    {
        char *vbi_path = var_InheritString (demux, CFG_PREFIX"vbidev");
        if (vbi_path != NULL)
            sys->vbi = OpenVBI (demux, vbi_path);
        free(vbi_path);
    }
#endif

    if (vlc_clone (&sys->thread, entry, demux, VLC_THREAD_PRIORITY_INPUT))
    {
#ifdef ZVBI_COMPILED
        if (sys->vbi != NULL)
            CloseVBI (sys->vbi);
#endif
        if (sys->pool != NULL)
            StopMmap(sys->pool);
        return -1;
    }
    return 0;
}

void DemuxClose( vlc_object_t *obj )
{
    demux_t *demux = (demux_t *)obj;
    demux_sys_t *sys = demux->p_sys;

    vlc_cancel (sys->thread);
    vlc_join (sys->thread, NULL);
    if (sys->pool != NULL)
        StopMmap(sys->pool);
    ControlsDeinit(vlc_object_parent(obj), sys->controls);
    v4l2_close (sys->fd);

#ifdef ZVBI_COMPILED
    if (sys->vbi != NULL)
        CloseVBI (sys->vbi);
#endif

    free( sys );
}

int DemuxOpen( vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    if (demux->out == NULL)
        return VLC_EGENERIC;

    demux_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;
    demux->p_sys = sys;
#ifdef ZVBI_COMPILED
    sys->vbi = NULL;
#endif

    ParseMRL(obj, demux->psz_location);

    char *path = var_InheritString(obj, CFG_PREFIX"dev");
    if (unlikely(path == NULL))
        goto error; /* probably OOM */

    uint32_t caps;
    int fd = OpenDevice(obj, path, &caps);
    free(path);
    if (fd == -1)
        goto error;
    sys->fd = fd;

    if (InitVideo(demux, fd, caps))
    {
        v4l2_close(fd);
        goto error;
    }

    sys->controls = ControlsInit(vlc_object_parent(obj), fd);
    sys->start = vlc_tick_now();
    demux->pf_demux = NULL;
    demux->pf_control = DemuxControl;
    return VLC_SUCCESS;
error:
    free(sys);
    return VLC_EGENERIC;
}
