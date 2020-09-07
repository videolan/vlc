/*****************************************************************************
 * src/test/executor.c
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

#undef NDEBUG

#include <assert.h>

#include <vlc_common.h>
#include <vlc_executor.h>
#include <vlc_tick.h>

struct data
{
    vlc_mutex_t lock;
    vlc_cond_t cond;
    int started;
    int ended;
    vlc_tick_t delay;
};

static void InitData(struct data *data)
{
    vlc_mutex_init(&data->lock);
    vlc_cond_init(&data->cond);
    data->started = 0;
    data->ended = 0;
    data->delay = 0;
}

static void RunIncrement(void *userdata)
{
    struct data *data = userdata;

    vlc_mutex_lock(&data->lock);
    ++data->started;
    vlc_mutex_unlock(&data->lock);

    if (data->delay > 0)
        vlc_tick_sleep(data->delay);

    vlc_mutex_lock(&data->lock);
    ++data->ended;
    vlc_mutex_unlock(&data->lock);

    vlc_cond_signal(&data->cond);
}

static void test_single_runnable(void)
{
    vlc_executor_t *executor = vlc_executor_New(1);
    assert(executor);

    struct data data;
    InitData(&data);

    struct vlc_runnable runnable = {
        .run = RunIncrement,
        .userdata = &data,
    };

    vlc_executor_Submit(executor, &runnable);

    vlc_mutex_lock(&data.lock);
    while (data.ended == 0)
        vlc_cond_wait(&data.cond, &data.lock);
    vlc_mutex_unlock(&data.lock);

    assert(data.ended == 1);

    vlc_executor_Delete(executor);
}

static void test_multiple_runnables(void)
{
    vlc_executor_t *executor = vlc_executor_New(3);
    assert(executor);

    struct data shared_data;
    InitData(&shared_data);

    struct vlc_runnable runnables[300];
    for (int i = 0; i < 300; ++i)
    {
        struct vlc_runnable *runnable = &runnables[i];
        runnable->run = RunIncrement;
        runnable->userdata = &shared_data;
        vlc_executor_Submit(executor, runnable);
    }

    vlc_mutex_lock(&shared_data.lock);
    while (shared_data.ended < 300)
        vlc_cond_wait(&shared_data.cond, &shared_data.lock);
    vlc_mutex_unlock(&shared_data.lock);

    assert(shared_data.ended == 300);

    vlc_executor_Delete(executor);
}

static void test_blocking_delete(void)
{
    vlc_executor_t *executor = vlc_executor_New(1);
    assert(executor);

    struct data data;
    InitData(&data);

    data.delay = VLC_TICK_FROM_MS(100);

    struct vlc_runnable runnable = {
        .run = RunIncrement,
        .userdata = &data,
    };

    vlc_executor_Submit(executor, &runnable);

    /* Wait for the runnable to be started */
    vlc_mutex_lock(&data.lock);
    while (data.started == 0)
        vlc_cond_wait(&data.cond, &data.lock);
    vlc_mutex_unlock(&data.lock);

    /* The runnable sleeps for about 100ms */

    vlc_executor_Delete(executor);

    vlc_mutex_lock(&data.lock);
    /* The executor must wait the end of running tasks on delete */
    assert(data.ended == 1);
    vlc_mutex_unlock(&data.lock);
}

static void test_cancel(void)
{
    vlc_executor_t *executor = vlc_executor_New(4);
    assert(executor);

    struct data shared_data;
    InitData(&shared_data);

    shared_data.delay = VLC_TICK_FROM_MS(100);

    /* Submit 40 tasks taking at least 100ms on an executor with at most 4
     * threads */

    struct vlc_runnable runnables[40];
    for (int i = 0; i < 40; ++i)
    {
        struct vlc_runnable *runnable = &runnables[i];
        runnable->run = RunIncrement;
        runnable->userdata = &shared_data;
        vlc_executor_Submit(executor, runnable);
    }

    /* Wait a bit (in two lines to avoid harmful_delay() warning) */
    vlc_tick_t delay = VLC_TICK_FROM_MS(150);
    vlc_tick_sleep(delay);

    int canceled = 0;
    for (int i = 0; i < 40; ++i)
    {
        if (vlc_executor_Cancel(executor, &runnables[i]))
            ++canceled;
    }

    vlc_mutex_lock(&shared_data.lock);
    /* All started must not be canceled (but some non-started-yet may not be
     * canceled either) */
    assert(canceled + shared_data.started <= 40);
    vlc_mutex_unlock(&shared_data.lock);

    vlc_executor_Delete(executor);

    /* Every started task must also be ended */
    assert(shared_data.started == shared_data.ended);
    /* Every task is either canceled or ended */
    assert(canceled + shared_data.ended == 40);
}

struct doubler_task
{
    vlc_executor_t *executor;
    int *array;
    size_t count;
    struct vlc_runnable runnable;
};

static void DoublerRun(void *);

static bool
SpawnDoublerTask(vlc_executor_t *executor, int *array, size_t count)
{
    struct doubler_task *task = malloc(sizeof(*task));
    if (!task)
        return false;

    task->executor = executor;
    task->array = array;
    task->count = count;
    task->runnable.run = DoublerRun;
    task->runnable.userdata = task;

    vlc_executor_Submit(executor, &task->runnable);

    return true;
}

static void DoublerRun(void *userdata)
{
    struct doubler_task *task = userdata;

    if (task->count == 1)
        task->array[0] *= 2; /* double the value */
    else
    {
        /* Spawn tasks doubling halves of the array recursively */
        bool ok;

        ok = SpawnDoublerTask(task->executor, task->array, task->count / 2);
        assert(ok);

        ok = SpawnDoublerTask(task->executor, task->array + task->count / 2,
                                              task->count - task->count / 2);
        assert(ok);
    }

    free(task);
}

static void test_task_chain(void)
{
    vlc_executor_t *executor = vlc_executor_New(4);
    assert(executor);

    /* Numbers from 0 to 99 */
    int array[100];
    for (int i = 0; i < 100; ++i)
        array[i] = i;

    /* Double all values in the array from tasks spawning smaller tasks
     * recursively, until the array has size 1, where the single value is
     * doubled */
    SpawnDoublerTask(executor, array, 100);

    vlc_executor_WaitIdle(executor);
    vlc_executor_Delete(executor);

    /* All values must have been doubled */
    for (int i = 0; i < 100; ++i)
        assert(array[i] == 2 * i);
}

int main(void)
{
    test_single_runnable();
    test_multiple_runnables();
    test_blocking_delete();
    test_cancel();
    test_task_chain();
    return 0;
}
