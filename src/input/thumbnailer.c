/*****************************************************************************
 * thumbnailer.c: Thumbnailing API
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
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

#include <vlc_thumbnailer.h>
#include <vlc_executor.h>
#include "input_internal.h"

struct vlc_thumbnailer_t
{
    vlc_object_t* parent;
    vlc_executor_t *executor;
};

struct seek_target
{
    enum
    {
        VLC_THUMBNAILER_SEEK_TIME,
        VLC_THUMBNAILER_SEEK_POS,
    } type;
    union
    {
        vlc_tick_t time;
        double pos;
    };
};

/* We may not rename vlc_thumbnailer_request_t because it is exposed in the
 * public API */
typedef struct vlc_thumbnailer_request_t task_t;

struct vlc_thumbnailer_request_t
{
    vlc_atomic_rc_t rc;
    vlc_thumbnailer_t *thumbnailer;

    struct seek_target seek_target;
    bool fast_seek;
    input_item_t *item;
    /**
     * A positive value will be used as the timeout duration
     * VLC_TICK_INVALID means no timeout
     */
    vlc_tick_t timeout;
    vlc_thumbnailer_cb cb;
    void* userdata;

    vlc_mutex_t lock;
    vlc_cond_t cond_ended;
    enum
    {
        RUNNING,
        INTERRUPTED,
        ENDED,
    } status;
    picture_t *pic;

    struct vlc_runnable runnable; /**< to be passed to the executor */
};

static void RunnableRun(void *);

static task_t *
TaskNew(vlc_thumbnailer_t *thumbnailer, input_item_t *item,
        struct seek_target seek_target, bool fast_seek,
        vlc_thumbnailer_cb cb, void *userdata, vlc_tick_t timeout)
{
    task_t *task = malloc(sizeof(*task));
    if (!task)
        return NULL;

    vlc_atomic_rc_init(&task->rc);
    task->thumbnailer = thumbnailer;
    task->item = item;
    task->seek_target = seek_target;
    task->fast_seek = fast_seek;
    task->cb = cb;
    task->userdata = userdata;
    task->timeout = timeout;

    vlc_mutex_init(&task->lock);
    vlc_cond_init(&task->cond_ended);
    task->status = RUNNING;
    task->pic = NULL;

    task->runnable.run = RunnableRun;
    task->runnable.userdata = task;

    input_item_Hold(item);

    return task;
}

static void
TaskRelease(task_t *task)
{
    if (!vlc_atomic_rc_dec(&task->rc))
        return;
    input_item_Release(task->item);
    free(task);
}

static void NotifyThumbnail(task_t *task, picture_t *pic)
{
    assert(task->cb);
    task->cb(task->userdata, pic);
}

static void
on_thumbnailer_input_event( input_thread_t *input,
                            const struct vlc_input_event *event, void *userdata )
{
    VLC_UNUSED(input);
    if ( event->type != INPUT_EVENT_THUMBNAIL_READY &&
         ( event->type != INPUT_EVENT_STATE || ( event->state.value != ERROR_S &&
                                                 event->state.value != END_S ) ) )
         return;

    task_t *task = userdata;

    vlc_mutex_lock(&task->lock);
    if (task->status != RUNNING)
    {
        /* We may receive a THUMBNAIL_READY event followed by an
         * INPUT_EVENT_STATE (end of stream), we must only consider the first
         * one. */
        vlc_mutex_unlock(&task->lock);
        return;
    }

    task->status = ENDED;

    if (event->type == INPUT_EVENT_THUMBNAIL_READY)
        task->pic = picture_Hold(event->thumbnail);

    vlc_cond_signal(&task->cond_ended);
    vlc_mutex_unlock(&task->lock);
}

static void
RunnableRun(void *userdata)
{
    vlc_thread_set_name("vlc-run-thumb");

    task_t *task = userdata;
    vlc_thumbnailer_t *thumbnailer = task->thumbnailer;

    vlc_tick_t now = vlc_tick_now();

    input_thread_t* input =
            input_Create( thumbnailer->parent, on_thumbnailer_input_event, task,
                          task->item, INPUT_TYPE_THUMBNAILING, NULL, NULL );
    if (!input)
        goto error;

    if (task->seek_target.type == VLC_THUMBNAILER_SEEK_TIME)
        input_SetTime(input, task->seek_target.time, task->fast_seek);
    else
    {
        assert(task->seek_target.type == VLC_THUMBNAILER_SEEK_POS);
        input_SetPosition(input, task->seek_target.pos, task->fast_seek);
    }

    int ret = input_Start(input);
    if (ret != VLC_SUCCESS)
    {
        input_Close(input);
        goto error;
    }

    vlc_mutex_lock(&task->lock);
    if (task->timeout == VLC_TICK_INVALID)
    {
        while (task->status == RUNNING)
            vlc_cond_wait(&task->cond_ended, &task->lock);
    }
    else
    {
        vlc_tick_t deadline = now + task->timeout;
        int timeout = 0;
        while (task->status == RUNNING && timeout == 0)
            timeout =
                vlc_cond_timedwait(&task->cond_ended, &task->lock, deadline);
    }
    picture_t* pic = task->pic;
    task->pic = NULL;

    bool notify = task->status != INTERRUPTED;
    vlc_mutex_unlock(&task->lock);

    if (notify)
        NotifyThumbnail(task, pic);

    if (pic != NULL)
        picture_Release(pic);

    input_Stop(input);
    input_Close(input);

error:
    TaskRelease(task);
}

static void
Interrupt(task_t *task)
{
    /* Wake up RunnableRun() which will call input_Stop() */
    vlc_mutex_lock(&task->lock);
    task->status = INTERRUPTED;
    vlc_cond_signal(&task->cond_ended);
    vlc_mutex_unlock(&task->lock);
}

static task_t *
RequestCommon(vlc_thumbnailer_t *thumbnailer, struct seek_target seek_target,
              enum vlc_thumbnailer_seek_speed speed, input_item_t *item,
              vlc_tick_t timeout, vlc_thumbnailer_cb cb, void *userdata)
{
    bool fast_seek = speed == VLC_THUMBNAILER_SEEK_FAST;
    task_t *task = TaskNew(thumbnailer, item, seek_target, fast_seek, cb,
                           userdata, timeout);
    if (!task)
        return NULL;

    /* One ref for the executor */
    vlc_atomic_rc_inc(&task->rc);
    vlc_executor_Submit(thumbnailer->executor, &task->runnable);

    return task;
}

task_t *
vlc_thumbnailer_RequestByTime( vlc_thumbnailer_t *thumbnailer,
                               vlc_tick_t time,
                               enum vlc_thumbnailer_seek_speed speed,
                               input_item_t *item, vlc_tick_t timeout,
                               vlc_thumbnailer_cb cb, void* userdata )
{
    struct seek_target seek_target = {
        .type = VLC_THUMBNAILER_SEEK_TIME,
        .time = time,
    };
    return RequestCommon(thumbnailer, seek_target, speed, item, timeout, cb,
                         userdata);
}

task_t *
vlc_thumbnailer_RequestByPos( vlc_thumbnailer_t *thumbnailer,
                              double pos, enum vlc_thumbnailer_seek_speed speed,
                              input_item_t *item, vlc_tick_t timeout,
                              vlc_thumbnailer_cb cb, void* userdata )
{
    struct seek_target seek_target = {
        .type = VLC_THUMBNAILER_SEEK_POS,
        .pos = pos,
    };
    return RequestCommon(thumbnailer, seek_target, speed, item, timeout, cb,
                         userdata);
}

void vlc_thumbnailer_DestroyRequest( vlc_thumbnailer_t* thumbnailer, task_t* task )
{
    bool canceled = vlc_executor_Cancel(thumbnailer->executor, &task->runnable);
    if (canceled)
    {
        /* Release the executor reference (since it won't run) */
        bool ret = vlc_atomic_rc_dec(&task->rc);
        /* Assert that only the caller got the reference */
        assert(!ret); (void) ret;
    }
    else
        Interrupt(task);

    TaskRelease(task);
}

vlc_thumbnailer_t *vlc_thumbnailer_Create( vlc_object_t* parent)
{
    vlc_thumbnailer_t *thumbnailer = malloc( sizeof( *thumbnailer ) );
    if ( unlikely( thumbnailer == NULL ) )
        return NULL;

    thumbnailer->executor = vlc_executor_New(1);
    if (!thumbnailer->executor)
    {
        free(thumbnailer);
        return NULL;
    }

    thumbnailer->parent = parent;

    return thumbnailer;
}

void vlc_thumbnailer_Release( vlc_thumbnailer_t *thumbnailer )
{
    vlc_executor_Delete(thumbnailer->executor);
    free( thumbnailer );
}
