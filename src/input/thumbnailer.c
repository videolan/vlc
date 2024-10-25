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
    vlc_cond_t cond_ended;

    vlc_thumbnailer_req_id current_id;
    vlc_tick_t timeout;

    struct vlc_list submitted_tasks; /**< list of struct task */
};

typedef struct task
{
    vlc_thumbnailer_t *thumbnailer;

    struct vlc_thumbnailer_seek_arg seek_arg;
    input_item_t *item;
    const struct vlc_thumbnailer_cbs *cbs;
    void* userdata;

    enum
    {
        RUNNING,
        INTERRUPTED,
        ENDED,
    } status;
    picture_t *pic;

    vlc_thumbnailer_req_id id;

    struct vlc_runnable runnable; /**< to be passed to the executor */
    struct vlc_list node; /**< node of vlc_thumbnailer_t.submitted_tasks */
} task_t;

static void RunnableRun(void *);

static task_t *
TaskNew(vlc_thumbnailer_t *thumbnailer, input_item_t *item,
        const struct vlc_thumbnailer_seek_arg *seek_arg,
        const struct vlc_thumbnailer_cbs *cbs, void *userdata )
{
    task_t *task = malloc(sizeof(*task));
    if (!task)
        return NULL;

    task->thumbnailer = thumbnailer;
    task->item = item;
    if (seek_arg == NULL)
        task->seek_arg = (struct vlc_thumbnailer_seek_arg) {
            .type = VLC_THUMBNAILER_SEEK_NONE,
        };
    else
        task->seek_arg = *seek_arg;

    task->cbs = cbs;
    task->userdata = userdata;

    task->status = RUNNING;
    task->pic = NULL;

    task->runnable.run = RunnableRun;
    task->runnable.userdata = task;

    input_item_Hold(item);

    return task;
}

static void
TaskDestroy(task_t *task)
{
    input_item_Release(task->item);
    free(task);
}

static void NotifyThumbnail(task_t *task, picture_t *pic)
{
    task->cbs->on_ended(pic, task->userdata);
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
    vlc_thumbnailer_t *thumbnailer = task->thumbnailer;

    vlc_mutex_lock(&thumbnailer->lock);
    if (task->status != RUNNING)
    {
        /* We may receive a THUMBNAIL_READY event followed by an
         * INPUT_EVENT_STATE (end of stream), we must only consider the first
         * one. */
        vlc_mutex_unlock(&thumbnailer->lock);
        return;
    }

    task->status = ENDED;

    if (event->type == INPUT_EVENT_THUMBNAIL_READY)
        task->pic = picture_Hold(event->thumbnail);

    vlc_cond_signal(&thumbnailer->cond_ended);
    vlc_mutex_unlock(&thumbnailer->lock);
}

static void
RunnableRun(void *userdata)
{
    vlc_thread_set_name("vlc-run-thumb");

    task_t *task = userdata;
    vlc_thumbnailer_t *thumbnailer = task->thumbnailer;

    vlc_tick_t now = vlc_tick_now();

    static const struct vlc_input_thread_callbacks cbs = {
        .on_event = on_thumbnailer_input_event,
    };

    const struct vlc_input_thread_cfg cfg = {
        .type = INPUT_TYPE_THUMBNAILING,
        .cbs = &cbs,
        .cbs_data = task,
    };

    input_thread_t* input =
            input_Create( thumbnailer->parent, task->item, &cfg );
    if (!input)
        goto error;

    assert(task->seek_arg.speed == VLC_THUMBNAILER_SEEK_PRECISE
        || task->seek_arg.speed == VLC_THUMBNAILER_SEEK_FAST);
    bool fast_seek = task->seek_arg.speed == VLC_THUMBNAILER_SEEK_FAST;

    switch (task->seek_arg.type)
    {
        case VLC_THUMBNAILER_SEEK_NONE:
            break;
        case VLC_THUMBNAILER_SEEK_TIME:
            input_SetTime(input, task->seek_arg.time, fast_seek);
            break;
        case VLC_THUMBNAILER_SEEK_POS:
            input_SetPosition(input, task->seek_arg.pos, fast_seek);
            break;
        default:
            vlc_assert_unreachable();
    }

    int ret = input_Start(input);
    if (ret != VLC_SUCCESS)
    {
        input_Close(input);
        goto error;
    }

    vlc_mutex_lock(&thumbnailer->lock);
    if (thumbnailer->timeout == VLC_TICK_INVALID)
    {
        while (task->status == RUNNING)
            vlc_cond_wait(&thumbnailer->cond_ended, &thumbnailer->lock);
    }
    else
    {
        vlc_tick_t deadline = now + thumbnailer->timeout;
        int timeout = 0;
        while (task->status == RUNNING && timeout == 0)
            timeout =
                vlc_cond_timedwait(&thumbnailer->cond_ended, &thumbnailer->lock, deadline);
    }
    picture_t* pic = task->pic;
    task->pic = NULL;

    bool notify = task->status != INTERRUPTED;

    vlc_list_remove(&task->node);
    vlc_mutex_unlock(&thumbnailer->lock);

    if (notify)
        NotifyThumbnail(task, pic);

    if (pic)
        picture_Release(pic);

    input_Stop(input);
    input_Close(input);

error:
    TaskDestroy(task);
}

vlc_thumbnailer_req_id
vlc_thumbnailer_Request( vlc_thumbnailer_t *thumbnailer,
                         input_item_t *item,
                         const struct vlc_thumbnailer_seek_arg *seek_arg,
                         const struct vlc_thumbnailer_cbs *cbs, void *userdata )
{
    assert( cbs != NULL && cbs->on_ended != NULL );

    task_t *task = TaskNew(thumbnailer, item, seek_arg, cbs, userdata);
    if (!task)
        return 0;

    vlc_mutex_lock(&thumbnailer->lock);
    vlc_thumbnailer_req_id id = task->id = thumbnailer->current_id++;
    static_assert(VLC_THUMBNAILER_REQ_ID_INVALID == 0, "Invalid id should be 0");
    if (unlikely(thumbnailer->current_id == 0)) /* unsigned wrapping */
        ++thumbnailer->current_id;
    vlc_list_append(&task->node, &thumbnailer->submitted_tasks);
    vlc_mutex_unlock(&thumbnailer->lock);

    vlc_executor_Submit(thumbnailer->executor, &task->runnable);

    return id;
}

size_t vlc_thumbnailer_Cancel( vlc_thumbnailer_t* thumbnailer, vlc_thumbnailer_req_id id )
{
    vlc_mutex_lock(&thumbnailer->lock);

    task_t *task;
    size_t count = 0;
    vlc_list_foreach(task, &thumbnailer->submitted_tasks, node)
    {
        if (id == VLC_THUMBNAILER_REQ_ID_INVALID || task->id == id)
        {
            count++;
            bool canceled =
                vlc_executor_Cancel(thumbnailer->executor, &task->runnable);
            if (canceled)
            {
                vlc_list_remove(&task->node);
                TaskDestroy(task);
            }
            else
            {
                /* The task will be finished and destroyed after run() */
                task->status = INTERRUPTED;
                vlc_cond_signal(&thumbnailer->cond_ended);
            }
        }
    }

    vlc_mutex_unlock(&thumbnailer->lock);

    return count;
}

vlc_thumbnailer_t *vlc_thumbnailer_Create( vlc_object_t* parent, vlc_tick_t timeout )
{
    assert(timeout >= 0);

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
    thumbnailer->current_id = 1;
    thumbnailer->timeout = timeout;
    vlc_mutex_init(&thumbnailer->lock);
    vlc_cond_init(&thumbnailer->cond_ended);
    vlc_list_init(&thumbnailer->submitted_tasks);

    return thumbnailer;
}

void vlc_thumbnailer_SetTimeout( vlc_thumbnailer_t *thumbnailer,
                                 vlc_tick_t timeout )
{
    thumbnailer->timeout = timeout;
}

void vlc_thumbnailer_Delete( vlc_thumbnailer_t *thumbnailer )
{
    vlc_executor_Delete(thumbnailer->executor);
    free( thumbnailer );
}
