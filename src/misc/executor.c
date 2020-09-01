/*****************************************************************************
 * misc/executor.c
 *****************************************************************************
 * Copyright (C) 2020 Videolabs, VLC authors and VideoLAN
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

#include <vlc_executor.h>

#include <vlc_atomic.h>
#include <vlc_list.h>
#include <vlc_threads.h>
#include "libvlc.h"

/**
 * An executor can spawn several threads.
 *
 * This structure contains the data specific to one thread.
 */
struct vlc_executor_thread {
    /** Node of vlc_executor.threads list */
    struct vlc_list node;

    /** The executor owning the thread */
    vlc_executor_t *owner;

    /** The system thread */
    vlc_thread_t thread;

    /** The current task executed by the thread, NULL if none */
    struct vlc_runnable *current_task;
};

/**
 * The executor (also vlc_executor_t, exposed as opaque type in the public
 * header).
 */
struct vlc_executor {
    vlc_mutex_t lock;

    /** Maximum number of threads to run the tasks */
    unsigned max_threads;

    /** List of active vlc_executor_thread */
    struct vlc_list threads;

    /** Thread count (in a separate field to quickly compare to max_threads) */
    unsigned nthreads;

    /* Number of tasks requested but not finished. */
    unsigned unfinished;

    /** Wait for the executor to be idle (i.e. unfinished == 0) */
    vlc_cond_t idle_wait;

    /** Queue of vlc_runnable */
    struct vlc_list queue;

    /** Wait for the queue to be non-empty */
    vlc_cond_t queue_wait;

    /** True if executor deletion is requested */
    bool closing;
};

static void
QueuePush(vlc_executor_t *executor, struct vlc_runnable *runnable)
{
    vlc_mutex_assert(&executor->lock);

    vlc_list_append(&runnable->node, &executor->queue);
    vlc_cond_signal(&executor->queue_wait);
}

static struct vlc_runnable *
QueueTake(vlc_executor_t *executor)
{
    vlc_mutex_assert(&executor->lock);

    while (!executor->closing && vlc_list_is_empty(&executor->queue))
        vlc_cond_wait(&executor->queue_wait, &executor->lock);

    if (executor->closing)
        return NULL;

    struct vlc_runnable *runnable =
        vlc_list_first_entry_or_null(&executor->queue, struct vlc_runnable,
                                     node);
    assert(runnable);
    vlc_list_remove(&runnable->node);

    /* Set links to NULL to know that it has been taken by a thread in
     * vlc_executor_Cancel() */
    runnable->node.prev = runnable->node.next = NULL;

    return runnable;
}

static void *
ThreadRun(void *userdata)
{
    struct vlc_executor_thread *thread = userdata;
    vlc_executor_t *executor = thread->owner;

    vlc_mutex_lock(&executor->lock);

    struct vlc_runnable *runnable;
    /* When the executor is closing, QueueTake() returns NULL */
    while ((runnable = QueueTake(executor)))
    {
        thread->current_task = runnable;
        vlc_mutex_unlock(&executor->lock);

        /* Execute the user-provided runnable, without the executor lock */
        runnable->run(runnable->userdata);

        vlc_mutex_lock(&executor->lock);
        thread->current_task = NULL;

        assert(executor->unfinished > 0);
        --executor->unfinished;
        if (!executor->unfinished)
            vlc_cond_signal(&executor->idle_wait);
    }

    vlc_mutex_unlock(&executor->lock);

    return NULL;
}

static int
SpawnThread(vlc_executor_t *executor)
{
    assert(executor->nthreads < executor->max_threads);

    struct vlc_executor_thread *thread = malloc(sizeof(*thread));
    if (!thread)
        return VLC_ENOMEM;

    thread->owner = executor;
    thread->current_task = NULL;

    if (vlc_clone(&thread->thread, ThreadRun, thread, VLC_THREAD_PRIORITY_LOW))
    {
        free(thread);
        return VLC_EGENERIC;
    }

    executor->nthreads++;
    vlc_list_append(&thread->node, &executor->threads);

    return VLC_SUCCESS;
}

vlc_executor_t *
vlc_executor_New(unsigned max_threads)
{
    assert(max_threads);
    vlc_executor_t *executor = malloc(sizeof(*executor));
    if (!executor)
        return NULL;

    vlc_mutex_init(&executor->lock);

    executor->max_threads = max_threads;
    executor->nthreads = 0;
    executor->unfinished = 0;

    vlc_list_init(&executor->threads);
    vlc_list_init(&executor->queue);

    vlc_cond_init(&executor->idle_wait);
    vlc_cond_init(&executor->queue_wait);

    executor->closing = false;

    /* Create one thread on init so that vlc_executor_Submit() may never fail */
    int ret = SpawnThread(executor);
    if (ret != VLC_SUCCESS)
    {
        free(executor);
        return NULL;
    }

    return executor;
}

void
vlc_executor_Submit(vlc_executor_t *executor, struct vlc_runnable *runnable)
{
    vlc_mutex_lock(&executor->lock);

    assert(!executor->closing);

    QueuePush(executor, runnable);

    if (++executor->unfinished > executor->nthreads
            && executor->nthreads < executor->max_threads)
        /* If it fails, this is not an error, there is at least one thread */
        SpawnThread(executor);

    vlc_mutex_unlock(&executor->lock);
}

bool
vlc_executor_Cancel(vlc_executor_t *executor, struct vlc_runnable *runnable)
{
    vlc_mutex_lock(&executor->lock);

    /* Either both prev and next are set, either both are NULL */
    assert(!runnable->node.prev == !runnable->node.next);

    bool in_queue = runnable->node.prev;
    if (in_queue)
    {
        vlc_list_remove(&runnable->node);

        assert(executor->unfinished > 0);
        --executor->unfinished;
        if (!executor->unfinished)
            vlc_cond_signal(&executor->idle_wait);
    }

    vlc_mutex_unlock(&executor->lock);

    return in_queue;
}

void
vlc_executor_WaitIdle(vlc_executor_t *executor)
{
    vlc_mutex_lock(&executor->lock);
    while (executor->unfinished)
        vlc_cond_wait(&executor->idle_wait, &executor->lock);
    vlc_mutex_unlock(&executor->lock);
}

void
vlc_executor_Delete(vlc_executor_t *executor)
{
    vlc_mutex_lock(&executor->lock);

    executor->closing = true;

    /* All the tasks must be canceled on delete */
    assert(vlc_list_is_empty(&executor->queue));

    vlc_mutex_unlock(&executor->lock);

    /* "closing" is now true, this will wake up threads */
    vlc_cond_broadcast(&executor->queue_wait);

    /* The threads list may not be written at this point, so it is safe to read
     * it without mutex locked (the mutex must be released to join the
     * threads). */

    struct vlc_executor_thread *thread;
    vlc_list_foreach(thread, &executor->threads, node)
    {
        vlc_join(thread->thread, NULL);
        free(thread);
    }

    /* The queue must still be empty (no runnable submitted a new runnable) */
    assert(vlc_list_is_empty(&executor->queue));

    /* There are no tasks anymore */
    assert(!executor->unfinished);

    free(executor);
}
