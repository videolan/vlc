/*****************************************************************************
 * prefetch.c: prefetchinging module for VLC
 *****************************************************************************
 * Copyright © 2015 Rémi Denis-Courmont
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
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_MMAP
# include <sys/mman.h>
# ifndef MAP_ANONYMOUS
#  define MAP_ANONYMOUS MAP_ANON
# endif
#elif !defined(__OS2__)
# include <errno.h>
# define MAP_FAILED ((void *)-1)
# define mmap(a,l,p,f,d,o) \
     ((void)(a), (void)(l), (void)(d), (void)(o), errno = ENOMEM, MAP_FAILED)
# define munmap(a,l) \
     ((void)(a), (void)(l), errno = EINVAL, -1)
# define sysconf(a) 1
#endif

#if defined (_WIN32)
# include <windows.h>
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_fs.h>
#include <vlc_interrupt.h>

struct stream_sys_t
{
    vlc_mutex_t  lock;
    vlc_cond_t   wait_data;
    vlc_cond_t   wait_space;
    vlc_thread_t thread;
    vlc_interrupt_t *interrupt;

    bool         eof;
    bool         error;
    bool         paused;

    bool         can_seek;
    bool         can_pace;
    bool         can_pause;
    uint64_t     size;
    int64_t      pts_delay;
    char        *content_type;

    uint64_t     buffer_offset;
    uint64_t     stream_offset;
    size_t       buffer_length;
    size_t       buffer_size;
    char        *buffer;
    size_t       read_size;
    size_t       seek_threshold;
};

static int ThreadRead(stream_t *stream, size_t length)
{
    stream_sys_t *sys = stream->p_sys;
    int canc = vlc_savecancel();

    vlc_mutex_unlock(&sys->lock);
    assert(length > 0);

    char *p = sys->buffer + (sys->buffer_offset % sys->buffer_size)
                          + sys->buffer_length;
    ssize_t val = vlc_stream_ReadPartial(stream->p_source, p, length);

    if (val < 0)
        msg_Err(stream, "cannot read data (at offset %"PRIu64")",
                sys->buffer_offset + sys->buffer_length);
    if (val == 0)
        msg_Dbg(stream, "end of stream");

    vlc_mutex_lock(&sys->lock);
    vlc_restorecancel(canc);

    if (val < 0)
        return -1;

    if (val == 0)
        sys->eof = true;

    assert((size_t)val <= length);
    sys->buffer_length += val;
    assert(sys->buffer_length <= sys->buffer_size);
    //msg_Dbg(stream, "buffer: %zu/%zu", sys->buffer_length, sys->buffer_size);
    return 0;
}

static int ThreadSeek(stream_t *stream, uint64_t seek_offset)
{
    stream_sys_t *sys = stream->p_sys;
    int canc = vlc_savecancel();

    vlc_mutex_unlock(&sys->lock);

    int val = vlc_stream_Seek(stream->p_source, seek_offset);
    if (val != VLC_SUCCESS)
        msg_Err(stream, "cannot seek (to offset %"PRIu64")", seek_offset);

    vlc_mutex_lock(&sys->lock);
    vlc_restorecancel(canc);

    if (val != VLC_SUCCESS)
        return -1;

    sys->buffer_offset = seek_offset;
    sys->buffer_length = 0;
    sys->eof = false;
    return 0;
}

static int ThreadControl(stream_t *stream, int query, ...)
{
    stream_sys_t *sys = stream->p_sys;
    int canc = vlc_savecancel();

    vlc_mutex_unlock(&sys->lock);

    va_list ap;
    int ret;

    va_start(ap, query);
    ret = vlc_stream_vaControl(stream->p_source, query, ap);
    va_end(ap);

    vlc_mutex_lock(&sys->lock);
    vlc_restorecancel(canc);
    return ret;
}

#define MAX_READ 65536
#define SEEK_THRESHOLD MAX_READ

static void *Thread(void *data)
{
    stream_t *stream = data;
    stream_sys_t *sys = stream->p_sys;
    bool paused = false;

    vlc_interrupt_set(sys->interrupt);

    vlc_mutex_lock(&sys->lock);
    mutex_cleanup_push(&sys->lock);
    for (;;)
    {
        if (paused)
        {
            if (sys->paused)
            {   /* Wait for resumption */
                vlc_cond_wait(&sys->wait_space, &sys->lock);
                continue;
            }

            /* Resume the underlying stream */
            msg_Dbg(stream, "resuming");
            ThreadControl(stream, STREAM_SET_PAUSE_STATE, false);
            paused = false;
            continue;
        }

        if (sys->stream_offset < sys->buffer_offset)
        {   /* Need to seek backward */
            if (ThreadSeek(stream, sys->stream_offset))
                break;
            continue;
        }

        if (sys->eof)
        {   /* At EOF, wait for backward seek */
            vlc_cond_wait(&sys->wait_space, &sys->lock);
            continue;
        }

        assert(sys->stream_offset >= sys->buffer_offset);

        /* As long as there is space, the buffer will retain already read
         * ("historical") data. The data can be used if/when seeking backward.
         * Unread data is however given precedence if the buffer is full. */
        uint64_t history = sys->stream_offset - sys->buffer_offset;

        if (sys->can_seek
         && history >= (sys->buffer_length + sys->seek_threshold))
        {   /* Large skip: seek forward */
            if (ThreadSeek(stream, sys->stream_offset))
                break;
            continue;
        }

        assert(sys->buffer_size >= sys->buffer_length);

        size_t unused = sys->buffer_size - sys->buffer_length;
        if (unused == 0)
        {   /* Buffer is full */
            if (history == 0)
            {
                if (sys->paused)
                {   /* Pause the stream once the buffer is full
                     * (and assuming pause was actually requested) */
                    msg_Dbg(stream, "pausing");
                    ThreadControl(stream, STREAM_SET_PAUSE_STATE, true);
                    paused = true;
                    continue;
                }

                /* Wait for data to be read */
                vlc_cond_wait(&sys->wait_space, &sys->lock);
                continue;
            }

            /* Discard some historical data to make room. */
            size_t discard = sys->read_size;
            if (discard > history)
                discard = history;

            /* discard <= sys->read_size <= sys->buffer_size = ...
             * ... unused + sys->buffer_length = 0 + sys->buffer_length */
            assert(discard <= sys->buffer_length);
            sys->buffer_offset += discard;
            sys->buffer_length -= discard;
            history -= discard;
            unused = discard;
        }

        /* Some streams cannot return a short data count and just wait for all
         * requested data to become available (e.g. regular files). So we have
         * to limit the data read in a single operation to avoid blocking for
         * too long. */
        if (unused > sys->read_size)
            unused = sys->read_size;

        if (ThreadRead(stream, unused))
            break;

        vlc_cond_signal(&sys->wait_data);
    }
    vlc_cleanup_pop();

    sys->error = true;
    vlc_cond_signal(&sys->wait_data);
    vlc_mutex_unlock(&sys->lock);
    return NULL;
}

static int Seek(stream_t *stream, uint64_t offset)
{
    stream_sys_t *sys = stream->p_sys;

    vlc_mutex_lock(&sys->lock);
    if (sys->stream_offset != offset)
    {
        sys->stream_offset = offset;
        vlc_cond_signal(&sys->wait_space);
    }
    vlc_mutex_unlock(&sys->lock);
    return 0;
}

static size_t BufferLevel(const stream_t *stream, bool *eof)
{
    stream_sys_t *sys = stream->p_sys;

    *eof = false;

    if (sys->stream_offset < sys->buffer_offset)
        return 0;
    if ((sys->stream_offset - sys->buffer_offset) >= sys->buffer_length)
    {
        *eof = sys->eof;
        return 0;
    }
    return sys->buffer_offset + sys->buffer_length - sys->stream_offset;
}

static ssize_t Read(stream_t *stream, void *buf, size_t buflen)
{
    stream_sys_t *sys = stream->p_sys;
    size_t copy;
    bool eof;

    if (buflen == 0)
        return buflen;
    if (buf == NULL)
    {
        Seek(stream, sys->stream_offset + buflen);
        return buflen;
    }

    vlc_mutex_lock(&sys->lock);
    if (sys->paused)
    {
        msg_Err(stream, "reading while paused (buggy demux?)");
        sys->paused = false;
        vlc_cond_signal(&sys->wait_space);
    }

    while ((copy = BufferLevel(stream, &eof)) == 0 && !eof)
    {
        void *data[2];

        if (sys->error)
        {
            vlc_mutex_unlock(&sys->lock);
            return 0;
        }

        vlc_interrupt_forward_start(sys->interrupt, data);
        vlc_cond_wait(&sys->wait_data, &sys->lock);
        vlc_interrupt_forward_stop(data);
    }

    char *p = sys->buffer + (sys->stream_offset % sys->buffer_size);
    if (copy > buflen)
        copy = buflen;
    memcpy(buf, p, copy);
    sys->stream_offset += copy;
    vlc_cond_signal(&sys->wait_space);
    vlc_mutex_unlock(&sys->lock);
    return copy;
}

static int ReadDir(stream_t *stream, input_item_node_t *node)
{
    (void) stream; (void) node;
    return VLC_EGENERIC;
}

static int Control(stream_t *stream, int query, va_list args)
{
    stream_sys_t *sys = stream->p_sys;

    switch (query)
    {
        case STREAM_CAN_SEEK:
            *va_arg(args, bool *) = sys->can_seek;
            break;
        case STREAM_CAN_FASTSEEK:
            *va_arg(args, bool *) = false;
            break;
        case STREAM_CAN_PAUSE:
             *va_arg(args, bool *) = sys->can_pause;
            break;
        case STREAM_CAN_CONTROL_PACE:
            *va_arg (args, bool *) = sys->can_pace;
            break;
        case STREAM_IS_DIRECTORY:
            return VLC_EGENERIC;
        case STREAM_GET_SIZE:
            if (sys->size == (uint64_t)-1)
                return VLC_EGENERIC;
            *va_arg(args, uint64_t *) = sys->size;
            break;
        case STREAM_GET_PTS_DELAY:
            *va_arg(args, int64_t *) = sys->pts_delay;
            break;
        case STREAM_GET_TITLE_INFO:
        case STREAM_GET_TITLE:
        case STREAM_GET_SEEKPOINT:
        case STREAM_GET_META:
            return VLC_EGENERIC;
        case STREAM_GET_CONTENT_TYPE:
            if (sys->content_type == NULL)
                return VLC_EGENERIC;
            *va_arg(args, char **) = strdup(sys->content_type);
            return VLC_SUCCESS;
        case STREAM_GET_SIGNAL:
            return VLC_EGENERIC;
        case STREAM_SET_PAUSE_STATE:
        {
            bool paused = va_arg(args, unsigned);

            vlc_mutex_lock(&sys->lock);
            sys->paused = paused;
            vlc_cond_signal(&sys->wait_space);
            vlc_mutex_unlock (&sys->lock);
            break;
        }
        case STREAM_SET_TITLE:
        case STREAM_SET_SEEKPOINT:
        case STREAM_SET_PRIVATE_ID_STATE:
        case STREAM_SET_PRIVATE_ID_CA:
        case STREAM_GET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;
        default:
            msg_Err(stream, "unimplemented query (%d) in control", query);
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static int Open(vlc_object_t *obj)
{
    stream_t *stream = (stream_t *)obj;

    bool fast_seek;
    /* For local files, the operating system is likely to do a better work at
     * caching/prefetching. Also, prefetching with this module could cause
     * undesirable high load at start-up. Lastly, local files may require
     * support for title/seekpoint and meta control requests. */
    vlc_stream_Control(stream->p_source, STREAM_CAN_FASTSEEK, &fast_seek);
    if (fast_seek)
        return VLC_EGENERIC;

    /* PID-filtered streams are not suitable for prefetching, as they would
     * suffer excessive latency to enable a PID. DVB would also require support
     * for the signal level and Conditional Access controls.
     * TODO? For seekable streams, a forced could work around the problem. */
    if (vlc_stream_Control(stream->p_source, STREAM_GET_PRIVATE_ID_STATE, 0,
                           &(bool){ false }) == VLC_SUCCESS)
        return VLC_EGENERIC;

    stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    stream->pf_read = Read;
    stream->pf_seek = Seek;
    stream->pf_control = Control;

    vlc_stream_Control(stream->p_source, STREAM_CAN_SEEK, &sys->can_seek);
    vlc_stream_Control(stream->p_source, STREAM_CAN_PAUSE, &sys->can_pause);
    vlc_stream_Control(stream->p_source, STREAM_CAN_CONTROL_PACE,
                       &sys->can_pace);
    if (vlc_stream_Control(stream->p_source, STREAM_GET_SIZE, &sys->size))
        sys->size = -1;
    vlc_stream_Control(stream->p_source, STREAM_GET_PTS_DELAY,
                       &sys->pts_delay);
    if (vlc_stream_Control(stream->p_source, STREAM_GET_CONTENT_TYPE,
                           &sys->content_type))
        sys->content_type = NULL;

    sys->eof = false;
    sys->error = false;
    sys->paused = false;
    sys->buffer_offset = 0;
    sys->stream_offset = 0;
    sys->buffer_length = 0;
    sys->buffer_size = var_InheritInteger(obj, "prefetch-buffer-size") << 10u;
    sys->read_size = var_InheritInteger(obj, "prefetch-read-size");
    sys->seek_threshold = var_InheritInteger(obj, "prefetch-seek-threshold");

    uint64_t size = stream_Size(stream->p_source);
    if (size > 0)
    {   /* No point allocating a buffer larger than the source stream */
        if (sys->buffer_size > size)
            sys->buffer_size = size;
        if (sys->read_size > size)
            sys->read_size = size;
    }
    if (sys->buffer_size < sys->read_size)
        sys->buffer_size = sys->read_size;

#if !defined(_WIN32) && !defined(__OS2__)
    /* Round up to a multiple of the page size */
    long page_size = sysconf(_SC_PAGESIZE);

    sys->buffer_size += page_size - 1;
    sys->buffer_size &= ~(page_size - 1);

    sys->buffer = mmap(NULL, 2 * sys->buffer_size, PROT_NONE,
                       MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (sys->buffer == MAP_FAILED)
        goto error;

    int fd = vlc_memfd();
    if (fd == -1)
        goto error;

    if (ftruncate(fd, sys->buffer_size)
     || mmap(sys->buffer, sys->buffer_size,
             PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0) == MAP_FAILED
     || mmap(sys->buffer + sys->buffer_size, sys->buffer_size,
             PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0) == MAP_FAILED)
    {
        vlc_close(fd);
        goto error;
    }
    vlc_close(fd);
#elif defined(__OS2__)
    /* On OS/2 Warp, page size is 4K, but the smallest chunk size is 64K */
    int page_size = 64 * 1024;

    sys->buffer_size += page_size - 1;
    sys->buffer_size &= ~(page_size - 1);
    sys->buffer = NULL;

    char *buffer;

    if (DosAllocMem(&buffer, 2 * sys->buffer_size, fALLOC))
        goto error;

    struct buffer_list
    {
        char *buffer;
        struct buffer_list *next;
    } *buffer_list_start = NULL;

    for (;;)
    {
        char *buf;
        if (DosAllocMem(&buf, sys->buffer_size, fPERM | OBJ_TILE))
            break;

        struct buffer_list *new_buffer = calloc(1, sizeof(*new_buffer));

        if (!new_buffer)
        {
            DosFreeMem(buf);
            break;
        }

        new_buffer->buffer = buf;
        new_buffer->next = buffer_list_start;

        buffer_list_start = new_buffer;

        if (buf > buffer)
        {
            /* Disable thread switching */
            DosEnterCritSec();

            char *addr = buffer;

            DosFreeMem(buffer);
            buffer = NULL;

            if (DosAllocMem(&buf, sys->buffer_size, fALLOC))
                goto exitcritsec;

            /* Was hole ? */
            if (buf < addr)
            {
                DosFreeMem(buf);
                buf = NULL;

                char *tmp;

                /* Try to fill the hole out */
                if (DosAllocMem(&tmp, addr - buf, fALLOC))
                    goto exitcritsec;

                DosAllocMem(&buf, sys->buffer_size, fALLOC);

                DosFreeMem(tmp);
            }

            if (buf != addr)
            {
                DosFreeMem(buf);
                goto exitcritsec;
            }

            char *alias = NULL;

            if (!DosAliasMem(buf, sys->buffer_size, &alias, 0)
             && alias == buf + sys->buffer_size)
                sys->buffer = addr;
            else
            {
                DosFreeMem(buf);
                DosFreeMem(alias);
            }

exitcritsec:
            /* Enable thread switching */
            DosExitCritSec();
            break;
        }
    }

    DosFreeMem(buffer);

    for (struct buffer_list *l = buffer_list_start, *next; l; l = next)
    {
        next = l->next;

        DosFreeMem(l->buffer);
        free(l);
    }

    if (sys->buffer == NULL)
        goto error;
#else
    SYSTEM_INFO info;
    GetSystemInfo(&info);

    sys->buffer_size += info.dwPageSize - 1;
    sys->buffer_size &= ~(info.dwPageSize - 1);
    sys->buffer = NULL;

    HANDLE map = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
                                   0, sys->buffer_size, NULL);
    if (map == NULL)
        goto error;

    for (;;)
    {
        char *buffer = VirtualAlloc(NULL, 2 * sys->buffer_size, MEM_RESERVE,
                                    PAGE_NOACCESS);
        if (buffer == NULL)
            break;

        VirtualFree(buffer, 2 * sys->buffer_size, MEM_RELEASE);

        char *a = MapViewOfFileEx(map, FILE_MAP_ALL_ACCESS, 0, 0,
                                  sys->buffer_size, buffer);
        char *b = MapViewOfFileEx(map, FILE_MAP_ALL_ACCESS, 0, 0,
                                  sys->buffer_size, buffer + sys->buffer_size);

        if (a == buffer && b == buffer + sys->buffer_size)
        {
            sys->buffer = buffer;
            break;
        }
        if (b != NULL)
            UnmapViewOfFile(b);
        if (a != NULL)
            UnmapViewOfFile(a);
        if (a == NULL || b == NULL)
            break; /* ENOMEM */
    }

    CloseHandle(map);
    if (sys->buffer == NULL)
        goto error;
#endif /* _WIN32 */

    sys->interrupt = vlc_interrupt_create();
    if (unlikely(sys->interrupt == NULL))
        goto error;

    vlc_mutex_init(&sys->lock);
    vlc_cond_init(&sys->wait_data);
    vlc_cond_init(&sys->wait_space);

    stream->p_sys = sys;

    if (vlc_clone(&sys->thread, Thread, stream, VLC_THREAD_PRIORITY_LOW))
    {
        vlc_cond_destroy(&sys->wait_space);
        vlc_cond_destroy(&sys->wait_data);
        vlc_mutex_destroy(&sys->lock);
        vlc_interrupt_destroy(sys->interrupt);
        goto error;
    }

    msg_Dbg(stream, "using %zu bytes buffer, %zu bytes read",
            sys->buffer_size, sys->read_size);
    stream->pf_read = Read;
    stream->pf_readdir = ReadDir;
    stream->pf_control = Control;
    return VLC_SUCCESS;

error:
#if !defined(_WIN32) && !defined(__OS2__)
    if (sys->buffer != MAP_FAILED)
        munmap(sys->buffer, 2 * sys->buffer_size);
#elif defined(__OS2__)
    if (sys->buffer != NULL)
    {
        DosFreeMem(sys->buffer + sys->buffer_size);
        DosFreeMem(sys->buffer);
    }
#else
    if (sys->buffer != NULL)
    {
        UnmapViewOfFile(sys->buffer + sys->buffer_size);
        UnmapViewOfFile(sys->buffer);
    }
#endif
    free(sys);
    return VLC_ENOMEM;
}


/**
 * Releases allocate resources.
 */
static void Close (vlc_object_t *obj)
{
    stream_t *stream = (stream_t *)obj;
    stream_sys_t *sys = stream->p_sys;

    vlc_cancel(sys->thread);
    vlc_interrupt_kill(sys->interrupt);
    vlc_join(sys->thread, NULL);
    vlc_interrupt_destroy(sys->interrupt);
    vlc_cond_destroy(&sys->wait_space);
    vlc_cond_destroy(&sys->wait_data);
    vlc_mutex_destroy(&sys->lock);

#if !defined(_WIN32) && !defined(__OS2__)
    munmap(sys->buffer, 2 * sys->buffer_size);
#elif defined(__OS2__)
    DosFreeMem(sys->buffer + sys->buffer_size);
    DosFreeMem(sys->buffer);
#else
    UnmapViewOfFile(sys->buffer + sys->buffer_size);
    UnmapViewOfFile(sys->buffer);
#endif
    free(sys->content_type);
    free(sys);
}

vlc_module_begin()
    set_category(CAT_INPUT)
    set_subcategory(SUBCAT_INPUT_STREAM_FILTER)
    set_capability("stream_filter", 0)

    set_description(N_("Stream prefetch filter"))
    set_callbacks(Open, Close)

    add_integer("prefetch-buffer-size", 1 << 14, N_("Buffer size"),
                N_("Prefetch buffer size (KiB)"), false)
        change_integer_range(4, 1 << 20)
    add_integer("prefetch-read-size", 1 << 14, N_("Read size"),
                N_("Prefetch background read size (bytes)"), true)
        change_integer_range(1, 1 << 29)
    add_integer("prefetch-seek-threshold", 1 << 14, N_("Seek threshold"),
                N_("Prefetch forward seek threshold (bytes)"), true)
        change_integer_range(0, UINT64_C(1) << 60)
vlc_module_end()
