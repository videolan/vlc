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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>
#include <vlc_fs.h>
#include <vlc_interrupt.h>

struct stream_ctrl
{
    struct stream_ctrl *next;
    int query;
    union
    {
        struct
        {
            int id;
            bool state;
        } id_state;
    };
};

typedef struct
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
    vlc_tick_t   pts_delay;
    char        *content_type;

    uint64_t     buffer_offset;
    uint64_t     stream_offset;
    size_t       buffer_length;
    size_t       buffer_size;
    char        *buffer;
    size_t       seek_threshold;

    struct stream_ctrl *controls;
} stream_sys_t;

static ssize_t ThreadRead(stream_t *stream, void *buf, size_t length)
{
    stream_sys_t *sys = stream->p_sys;

    vlc_mutex_unlock(&sys->lock);
    assert(length > 0);

    ssize_t val = vlc_stream_ReadPartial(stream->s, buf, length);

    vlc_mutex_lock(&sys->lock);
    return val;
}

static int ThreadSeek(stream_t *stream, uint64_t seek_offset)
{
    stream_sys_t *sys = stream->p_sys;

    vlc_mutex_unlock(&sys->lock);

    int val = vlc_stream_Seek(stream->s, seek_offset);
    if (val != VLC_SUCCESS)
        msg_Err(stream, "cannot seek (to offset %"PRIu64")", seek_offset);

    vlc_mutex_lock(&sys->lock);

    return (val == VLC_SUCCESS) ? 0 : -1;
}

static int ThreadControl(stream_t *stream, int query, ...)
{
    stream_sys_t *sys = stream->p_sys;

    vlc_mutex_unlock(&sys->lock);

    va_list ap;
    int ret;

    va_start(ap, query);
    ret = vlc_stream_vaControl(stream->s, query, ap);
    va_end(ap);

    vlc_mutex_lock(&sys->lock);
    return ret;
}

static void *Thread(void *data)
{
    stream_t *stream = data;
    stream_sys_t *sys = stream->p_sys;
    bool paused = false;

    vlc_interrupt_set(sys->interrupt);

    vlc_mutex_lock(&sys->lock);
    while (!vlc_killed())
    {
        struct stream_ctrl *ctrl = sys->controls;

        if (unlikely(ctrl != NULL))
        {
            sys->controls = ctrl->next;
            ThreadControl(stream, ctrl->query, ctrl->id_state.id,
                          ctrl->id_state.state);
            free(ctrl);
            continue;
        }

        if (sys->paused != paused)
        {   /* Update pause state */
            msg_Dbg(stream, paused ? "resuming" : "pausing");
            paused = sys->paused;
            ThreadControl(stream, STREAM_SET_PAUSE_STATE, paused);
            continue;
        }

        if (paused || sys->error)
        {   /* Wait for not paused and not failed */
            vlc_cond_wait(&sys->wait_space, &sys->lock);
            continue;
        }

        uint_fast64_t stream_offset = sys->stream_offset;

        if (stream_offset < sys->buffer_offset)
        {   /* Need to seek backward */
            if (ThreadSeek(stream, stream_offset) == 0)
            {
                sys->buffer_offset = stream_offset;
                sys->buffer_length = 0;
                assert(!sys->error);
                sys->eof = false;
            }
            else
            {
                sys->error = true;
                vlc_cond_signal(&sys->wait_data);
            }
            continue;
        }

        if (sys->eof)
        {   /* Do not attempt to read at EOF - would busy loop */
            vlc_cond_wait(&sys->wait_space, &sys->lock);
            continue;
        }

        assert(stream_offset >= sys->buffer_offset);

        /* As long as there is space, the buffer will retain already read
         * ("historical") data. The data can be used if/when seeking backward.
         * Unread data is however given precedence if the buffer is full. */
        uint64_t history = stream_offset - sys->buffer_offset;

        /* If upstream supports seeking and if the downstream offset is far
         * beyond the upstream offset, then attempt to skip forward.
         * If it fails, assume upstream is well-behaved such that the failed
         * seek is a no-op, and continue as if seeking was not supported.
         * WARNING: Except problems with misbehaving access plug-ins. */
        if (sys->can_seek
         && history >= (sys->buffer_length + sys->seek_threshold))
        {
            if (ThreadSeek(stream, stream_offset) == 0)
            {
                sys->buffer_offset = stream_offset;
                sys->buffer_length = 0;
                assert(!sys->error);
                assert(!sys->eof);
            }
            else
            {   /* Seek failure is not necessarily fatal here. We could read
                 * data instead until the desired seek offset. But in practice,
                 * not all upstream accesses handle reads after failed seek
                 * correctly. Furthermore, sys->stream_offset and/or
                 * sys->paused might have changed in the mean time. */
                sys->error = true;
                vlc_cond_signal(&sys->wait_data);
            }
            continue;
        }

        assert(sys->buffer_size >= sys->buffer_length);

        size_t len = sys->buffer_size - sys->buffer_length;
        if (len == 0)
        {   /* Buffer is full */
            if (history == 0)
            {   /* Wait for data to be read */
                vlc_cond_wait(&sys->wait_space, &sys->lock);
                continue;
            }

            /* Discard some historical data to make room. */
            len = history > sys->buffer_length ? sys->buffer_length : history;

            sys->buffer_offset += len;
            sys->buffer_length -= len;
        }

        size_t offset = (sys->buffer_offset + sys->buffer_length)
                        % sys->buffer_size;
         /* Do not step past the sharp edge of the circular buffer */
        if (offset + len > sys->buffer_size)
            len = sys->buffer_size - offset;

        ssize_t val = ThreadRead(stream, sys->buffer + offset, len);
        if (val < 0)
            continue;
        if (val == 0)
        {
            assert(len > 0);
            msg_Dbg(stream, "end of stream");
            sys->eof = true;
        }

        assert((size_t)val <= len);
        sys->buffer_length += val;
        assert(sys->buffer_length <= sys->buffer_size);
        //msg_Dbg(stream, "buffer: %zu/%zu", sys->buffer_length,
        //        sys->buffer_size);
        vlc_cond_signal(&sys->wait_data);
    }

    sys->error = true;
    vlc_cond_signal(&sys->wait_data);
    vlc_mutex_unlock(&sys->lock);
    return NULL;
}

static int Seek(stream_t *stream, uint64_t offset)
{
    stream_sys_t *sys = stream->p_sys;

    vlc_mutex_lock(&sys->lock);
    sys->stream_offset = offset;
    sys->error = false;
    vlc_cond_signal(&sys->wait_space);
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
    size_t copy, offset;
    bool eof;

    if (buflen == 0)
        return buflen;

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

    offset = sys->stream_offset % sys->buffer_size;
    if (copy > buflen)
        copy = buflen;
    /* Do not step past the sharp edge of the circular buffer */
    if (offset + copy > sys->buffer_size)
        copy = sys->buffer_size - offset;

    memcpy(buf, sys->buffer + offset, copy);
    sys->stream_offset += copy;
    vlc_cond_signal(&sys->wait_space);
    vlc_mutex_unlock(&sys->lock);
    return copy;
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
        case STREAM_GET_SIZE:
            if (sys->size == (uint64_t)-1)
                return VLC_EGENERIC;
            *va_arg(args, uint64_t *) = sys->size;
            break;
        case STREAM_GET_PTS_DELAY:
            *va_arg(args, vlc_tick_t *) = sys->pts_delay;
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
        case STREAM_GET_TAGS:
        case STREAM_GET_TYPE:
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
            return VLC_EGENERIC;
        case STREAM_SET_PRIVATE_ID_STATE:
        {
            struct stream_ctrl *ctrl = malloc(sizeof (*ctrl)), **pp;
            if (unlikely(ctrl == NULL))
                return VLC_ENOMEM;

            ctrl->next = NULL;
            ctrl->query = query;
            ctrl->id_state.id = va_arg(args, int);
            ctrl->id_state.state = va_arg(args, int);
            vlc_mutex_lock(&sys->lock);
            for (pp = &sys->controls; *pp != NULL; pp = &((*pp)->next));
            *pp = ctrl;
            vlc_cond_signal(&sys->wait_space);
            vlc_mutex_unlock(&sys->lock);
            break;
        }
        case STREAM_SET_PRIVATE_ID_CA:
        case STREAM_GET_PRIVATE_ID_STATE:
            return VLC_EGENERIC;

        case STREAM_CLEAR_BUFFER:
            vlc_mutex_lock(&sys->lock);

            sys->buffer_offset = 0;
            sys->stream_offset = 0;
            sys->buffer_length = 0;

            /* clear buffer */
            vlc_stream_Control(stream->s, STREAM_CLEAR_BUFFER);
            vlc_cond_signal(&sys->wait_space);
            vlc_mutex_unlock (&sys->lock);
            return VLC_SUCCESS;
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

    if (vlc_stream_Control(stream->s, STREAM_CAN_FASTSEEK, &fast_seek))
        return VLC_EGENERIC; /* not a byte stream */
    /* For local files, the operating system is likely to do a better work at
     * caching/prefetching. Also, prefetching with this module could cause
     * undesirable high load at start-up. Lastly, local files may require
     * support for title/seekpoint and meta control requests. */
    if (fast_seek)
        return VLC_EGENERIC;

    /* PID-filtered streams are not suitable for prefetching, as they would
     * suffer excessive latency to enable a PID. DVB would also require support
     * for the signal level and Conditional Access controls.
     * TODO? For seekable streams, a forced could work around the problem. */
    if (vlc_stream_Control(stream->s, STREAM_GET_PRIVATE_ID_STATE, 0,
                           &(bool){ false }) == VLC_SUCCESS)
        return VLC_EGENERIC;

    stream_sys_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    vlc_stream_Control(stream->s, STREAM_CAN_SEEK, &sys->can_seek);
    vlc_stream_Control(stream->s, STREAM_CAN_PAUSE, &sys->can_pause);
    vlc_stream_Control(stream->s, STREAM_CAN_CONTROL_PACE, &sys->can_pace);
    if (vlc_stream_Control(stream->s, STREAM_GET_SIZE, &sys->size))
        sys->size = -1;
    vlc_stream_Control(stream->s, STREAM_GET_PTS_DELAY, &sys->pts_delay);
    if (vlc_stream_Control(stream->s, STREAM_GET_CONTENT_TYPE,
                           &sys->content_type))
        sys->content_type = NULL;

    sys->eof = false;
    sys->error = false;
    sys->paused = false;
    sys->buffer_offset = 0;
    sys->stream_offset = 0;
    sys->buffer_length = 0;
    sys->buffer_size = var_InheritInteger(obj, "prefetch-buffer-size") << 10u;
    sys->seek_threshold = var_InheritInteger(obj, "prefetch-seek-threshold");
    sys->controls = NULL;

    uint64_t size = stream_Size(stream->s);
    if (size > 0)
    {   /* No point allocating a buffer larger than the source stream */
        if (sys->buffer_size > size)
            sys->buffer_size = size;
    }

    sys->buffer = malloc(sys->buffer_size);
    if (sys->buffer == NULL)
        goto error;

    sys->interrupt = vlc_interrupt_create();
    if (unlikely(sys->interrupt == NULL))
        goto error;

    vlc_mutex_init(&sys->lock);
    vlc_cond_init(&sys->wait_data);
    vlc_cond_init(&sys->wait_space);

    stream->p_sys = sys;

    if (vlc_clone(&sys->thread, Thread, stream, VLC_THREAD_PRIORITY_LOW))
    {
        vlc_interrupt_destroy(sys->interrupt);
        goto error;
    }

    msg_Dbg(stream, "using %zu bytes buffer", sys->buffer_size);
    stream->pf_read = Read;
    stream->pf_seek = Seek;
    stream->pf_control = Control;
    return VLC_SUCCESS;

error:
    free(sys->buffer);
    free(sys->content_type);
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

    vlc_mutex_lock(&sys->lock);
    vlc_interrupt_kill(sys->interrupt);
    vlc_cond_signal(&sys->wait_space);
    vlc_mutex_unlock(&sys->lock);

    vlc_join(sys->thread, NULL);
    vlc_interrupt_destroy(sys->interrupt);

    while(sys->controls)
    {
        struct stream_ctrl *ctrl = sys->controls;
        sys->controls = ctrl->next;
        free(ctrl);
    }
    free(sys->buffer);
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
                N_("Prefetch buffer size (KiB)"))
        change_integer_range(4, 1 << 20)
    add_obsolete_integer("prefetch-read-size") /* since 4.0.0 */
    add_integer("prefetch-seek-threshold", 1 << 14, N_("Seek threshold"),
                N_("Prefetch forward seek threshold (bytes)"))
        change_integer_range(0, UINT64_C(1) << 60)
vlc_module_end()
