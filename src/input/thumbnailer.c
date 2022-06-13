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

    vlc_mutex_t lock;
    struct vlc_list submitted_tasks; /**< list of struct thumbnailer_task */
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
        float pos;
    };
};

/* We may not rename vlc_thumbnailer_request_t because it is exposed in the
 * public API */
typedef struct vlc_thumbnailer_request_t task_t;

struct vlc_thumbnailer_request_t
{
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
    bool ended;
    picture_t *pic;

    struct vlc_runnable runnable; /**< to be passed to the executor */

    struct vlc_list node; /**< node of vlc_thumbnailer_t.submitted_tasks */
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

    task->thumbnailer = thumbnailer;
    task->item = item;
    task->seek_target = seek_target;
    task->fast_seek = fast_seek;
    task->cb = cb;
    task->userdata = userdata;
    task->timeout = timeout;

    vlc_mutex_init(&task->lock);
    vlc_cond_init(&task->cond_ended);
    task->ended = false;
    task->pic = NULL;

    task->runnable.run = RunnableRun;
    task->runnable.userdata = task;

    input_item_Hold(item);

    return task;
}

static void
TaskDelete(task_t *task)
{
    input_item_Release(task->item);
    free(task);
}

static void
ThumbnailerAddTask(vlc_thumbnailer_t *thumbnailer, task_t *task)
{
    vlc_mutex_lock(&thumbnailer->lock);
    vlc_list_append(&task->node, &thumbnailer->submitted_tasks);
    vlc_mutex_unlock(&thumbnailer->lock);
}

static void
ThumbnailerRemoveTask(vlc_thumbnailer_t *thumbnailer, task_t *task)
{
    vlc_mutex_lock(&thumbnailer->lock);
    vlc_list_remove(&task->node);
    vlc_mutex_unlock(&thumbnailer->lock);
}

static void NotifyThumbnail(task_t *task, picture_t *pic)
{
    assert(task->cb);
    task->cb(task->userdata, pic);
    if (pic)
        picture_Release(pic);
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
    if (task->ended)
    {
        /* We may receive a THUMBNAIL_READY event followed by an
         * INPUT_EVENT_STATE (end of stream), we must only consider the first
         * one. */
        vlc_mutex_unlock(&task->lock);
        return;
    }

    task->ended = true;

    if (event->type == INPUT_EVENT_THUMBNAIL_READY)
        task->pic = picture_Hold(event->thumbnail);

    vlc_mutex_unlock(&task->lock);

    vlc_cond_signal(&task->cond_ended);
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
        goto end;

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
        goto end;
    }

    vlc_mutex_lock(&task->lock);
    if (task->timeout == VLC_TICK_INVALID)
    {
        while (!task->ended)
            vlc_cond_wait(&task->cond_ended, &task->lock);
    }
    else
    {
        vlc_tick_t deadline = now + task->timeout;
        bool timeout = false;
        while (!task->ended && !timeout)
            timeout =
                vlc_cond_timedwait(&task->cond_ended, &task->lock, deadline);
    }
    picture_t* pic = task->pic;
    task->pic = NULL;
    vlc_mutex_unlock(&task->lock);

    NotifyThumbnail(task, pic);

    input_Stop(input);
    input_Close(input);

end:
    ThumbnailerRemoveTask(thumbnailer, task);
    TaskDelete(task);
}

static void
Interrupt(task_t *task)
{
    /* Wake up RunnableRun() which will call input_Stop() */
    vlc_mutex_lock(&task->lock);
    task->ended = true;
    vlc_mutex_unlock(&task->lock);
    vlc_cond_signal(&task->cond_ended);
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

    ThumbnailerAddTask(thumbnailer, task);

    vlc_executor_Submit(thumbnailer->executor, &task->runnable);

    /* XXX In theory, "task" might already be invalid here (if it is already
     * executed and deleted). This is consistent with the API documentation and
     * the previous implementation, but it is not very convenient for the user.
     */
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
                              float pos, enum vlc_thumbnailer_seek_speed speed,
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

void vlc_thumbnailer_Cancel( vlc_thumbnailer_t* thumbnailer, task_t* task )
{
    (void) thumbnailer;
    /* The API documentation requires that task is valid */
    Interrupt(task);
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
    vlc_mutex_init(&thumbnailer->lock);
    vlc_list_init(&thumbnailer->submitted_tasks);

    return thumbnailer;
}

static void
CancelAllTasks(vlc_thumbnailer_t *thumbnailer)
{
    vlc_mutex_lock(&thumbnailer->lock);

    task_t *task;
    vlc_list_foreach(task, &thumbnailer->submitted_tasks, node)
    {
        bool canceled = vlc_executor_Cancel(thumbnailer->executor,
                                            &task->runnable);
        if (canceled)
        {
            NotifyThumbnail(task, NULL);
            vlc_list_remove(&task->node);
            TaskDelete(task);
        }
        /* Otherwise, the task will be finished and destroyed after run() */
    }

    vlc_mutex_unlock(&thumbnailer->lock);
}

void vlc_thumbnailer_Release( vlc_thumbnailer_t *thumbnailer )
{
    CancelAllTasks(thumbnailer);

    vlc_executor_Delete(thumbnailer->executor);
    free( thumbnailer );
}
