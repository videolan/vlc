/*****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
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
#  include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>
#include <vlc_list.h>
#include <vlc_threads.h>

#include "libvlc.h"
#include "background_worker.h"

struct task {
    struct vlc_list node;
    void* id; /**< id associated with entity */
    void* entity; /**< the entity to process */
    vlc_tick_t timeout; /**< timeout duration in vlc_tick_t */
};

struct background_worker;

struct background_thread {
    struct background_worker *owner;
    vlc_cond_t probe_cancel_wait; /**< wait for probe request or cancelation */
    bool probe; /**< true if a probe is requested */
    bool cancel; /**< true if a cancel is requested */
    struct task *task; /**< current task */
    struct vlc_list node;
};

struct background_worker {
    void* owner;
    struct background_worker_config conf;

    vlc_mutex_t lock;

    int uncompleted; /**< number of tasks requested but not completed */
    int nthreads; /**< number of threads in the threads list */
    struct vlc_list threads; /**< list of active background_thread instances */

    struct vlc_list queue; /**< queue of tasks */
    vlc_cond_t queue_wait; /**< wait for the queue to be non-empty */

    vlc_cond_t nothreads_wait; /**< wait for nthreads == 0 */
    bool closing; /**< true if background worker deletion is requested */
};

static struct task *task_Create(struct background_worker *worker, void *id,
                                void *entity, int timeout)
{
    struct task *task = malloc(sizeof(*task));
    if (unlikely(!task))
        return NULL;

    task->id = id;
    task->entity = entity;
    task->timeout = timeout < 0 ? worker->conf.default_timeout : VLC_TICK_FROM_MS(timeout);
    worker->conf.pf_hold(task->entity);
    return task;
}

static void task_Destroy(struct background_worker *worker, struct task *task)
{
    worker->conf.pf_release(task->entity);
    free(task);
}

static struct task *QueueTake(struct background_worker *worker, int timeout_ms)
{
    vlc_mutex_assert(&worker->lock);

    vlc_tick_t deadline = vlc_tick_now() + VLC_TICK_FROM_MS(timeout_ms);
    bool timeout = false;
    while (!timeout && !worker->closing && vlc_list_is_empty(&worker->queue))
        timeout = vlc_cond_timedwait(&worker->queue_wait,
                                     &worker->lock, deadline) != 0;

    if (worker->closing || timeout)
        return NULL;

    struct task *task = vlc_list_first_entry_or_null(&worker->queue,
                                                     struct task, node);
    assert(task);
    vlc_list_remove(&task->node);

    return task;
}

static void QueuePush(struct background_worker *worker, struct task *task)
{
    vlc_mutex_assert(&worker->lock);
    vlc_list_append(&task->node, &worker->queue);
    vlc_cond_signal(&worker->queue_wait);
}

static void QueueRemoveAll(struct background_worker *worker, void *id)
{
    vlc_mutex_assert(&worker->lock);
    struct task *task;
    vlc_list_foreach(task, &worker->queue, node)
    {
        if (!id || task->id == id)
        {
            vlc_list_remove(&task->node);
            task_Destroy(worker, task);
        }
    }
}

static struct background_thread *
background_thread_Create(struct background_worker *owner)
{
    struct background_thread *thread = malloc(sizeof(*thread));
    if (!thread)
        return NULL;

    vlc_cond_init(&thread->probe_cancel_wait);
    thread->probe = false;
    thread->cancel = false;
    thread->task = NULL;
    thread->owner = owner;
    return thread;
}

static void background_thread_Destroy(struct background_thread *thread)
{
    free(thread);
}

static struct background_worker *background_worker_Create(void *owner,
                                         struct background_worker_config *conf)
{
    struct background_worker* worker = malloc(sizeof(*worker));
    if (unlikely(!worker))
        return NULL;

    worker->conf = *conf;
    worker->owner = owner;

    vlc_mutex_init(&worker->lock);
    worker->uncompleted = 0;
    worker->nthreads = 0;
    vlc_list_init(&worker->threads);
    vlc_list_init(&worker->queue);
    vlc_cond_init(&worker->queue_wait);
    vlc_cond_init(&worker->nothreads_wait);
    worker->closing = false;
    return worker;
}

static void background_worker_Destroy(struct background_worker *worker)
{
    free(worker);
}

static void TerminateTask(struct background_thread *thread, struct task *task)
{
    struct background_worker *worker = thread->owner;

    vlc_mutex_lock(&worker->lock);
    thread->task = NULL;
    worker->uncompleted--;
    assert(worker->uncompleted >= 0);
    vlc_mutex_unlock(&worker->lock);

    task_Destroy(worker, task);
}

static void RemoveThread(struct background_thread *thread)
{
    struct background_worker *worker = thread->owner;

    vlc_mutex_lock(&worker->lock);

    vlc_list_remove(&thread->node);
    worker->nthreads--;
    assert(worker->nthreads >= 0);
    if (!worker->nthreads)
        vlc_cond_signal(&worker->nothreads_wait);

    vlc_mutex_unlock(&worker->lock);

    background_thread_Destroy(thread);
}

static void* Thread( void* data )
{
    struct background_thread *thread = data;
    struct background_worker *worker = thread->owner;

    for (;;)
    {
        vlc_mutex_lock(&worker->lock);
        struct task *task = QueueTake(worker, 5000);
        if (!task)
        {
            vlc_mutex_unlock(&worker->lock);
            /* terminate this thread */
            break;
        }

        thread->task = task;
        thread->cancel = false;
        thread->probe = false;
        vlc_tick_t deadline;
        if (task->timeout > 0)
            deadline = vlc_tick_now() + task->timeout;
        else
            deadline = INT64_MAX; /* no deadline */
        vlc_mutex_unlock(&worker->lock);

        void *handle;
        if (worker->conf.pf_start(worker->owner, task->entity, &handle))
        {
            TerminateTask(thread, task);
            continue;
        }

        for (;;)
        {
            vlc_mutex_lock(&worker->lock);
            bool timeout = false;
            while (!timeout && !thread->probe && !thread->cancel)
                /* any non-zero return value means timeout */
                timeout = vlc_cond_timedwait(&thread->probe_cancel_wait,
                                             &worker->lock, deadline) != 0;

            bool cancel = thread->cancel;
            thread->cancel = false;
            thread->probe = false;
            vlc_mutex_unlock(&worker->lock);

            if (timeout || cancel
                    || worker->conf.pf_probe(worker->owner, handle))
            {
                worker->conf.pf_stop(worker->owner, handle);
                TerminateTask(thread, task);
                break;
            }
        }
    }

    RemoveThread(thread);

    return NULL;
}

static bool SpawnThread(struct background_worker *worker)
{
    vlc_mutex_assert(&worker->lock);

    struct background_thread *thread = background_thread_Create(worker);
    if (!thread)
        return false;

    if (vlc_clone_detach(NULL, Thread, thread, VLC_THREAD_PRIORITY_LOW))
    {
        free(thread);
        return false;
    }
    worker->nthreads++;
    vlc_list_append(&thread->node, &worker->threads);

    return true;
}

struct background_worker* background_worker_New( void* owner,
    struct background_worker_config* conf )
{
    return background_worker_Create(owner, conf);
}

int background_worker_Push( struct background_worker* worker, void* entity,
                        void* id, int timeout )
{
    struct task *task = task_Create(worker, id, entity, timeout);
    if (unlikely(!task))
        return VLC_ENOMEM;

    vlc_mutex_lock(&worker->lock);
    QueuePush(worker, task);
    if (++worker->uncompleted > worker->nthreads
            && worker->nthreads < worker->conf.max_threads)
        SpawnThread(worker);
    vlc_mutex_unlock(&worker->lock);

    return VLC_SUCCESS;
}

static void BackgroundWorkerCancelLocked(struct background_worker *worker,
                                         void *id)
{
    vlc_mutex_assert(&worker->lock);

    QueueRemoveAll(worker, id);

    struct background_thread *thread;
    vlc_list_foreach(thread, &worker->threads, node)
    {
        if (!id || (thread->task && thread->task->id == id && !thread->cancel))
        {
            thread->cancel = true;
            vlc_cond_signal(&thread->probe_cancel_wait);
        }
    }
}

void background_worker_Cancel( struct background_worker* worker, void* id )
{
    vlc_mutex_lock(&worker->lock);
    BackgroundWorkerCancelLocked(worker, id);
    vlc_mutex_unlock(&worker->lock);
}

void background_worker_RequestProbe( struct background_worker* worker )
{
    vlc_mutex_lock(&worker->lock);

    struct background_thread *thread;
    vlc_list_foreach(thread, &worker->threads, node)
    {
        thread->probe = true;
        vlc_cond_signal(&thread->probe_cancel_wait);
    }

    vlc_mutex_unlock(&worker->lock);
}

void background_worker_Delete( struct background_worker* worker )
{
    vlc_mutex_lock(&worker->lock);

    worker->closing = true;
    BackgroundWorkerCancelLocked(worker, NULL);
    /* closing is now true, this will wake up any QueueTake() */
    vlc_cond_broadcast(&worker->queue_wait);

    while (worker->nthreads)
        vlc_cond_wait(&worker->nothreads_wait, &worker->lock);

    vlc_mutex_unlock(&worker->lock);

    /* no threads use the worker anymore, we can destroy it */
    background_worker_Destroy(worker);
}
