/*****************************************************************************
 * vlc_executor.h
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

#ifndef VLC_EXECUTOR_H
#define VLC_EXECUTOR_H

#include <vlc_common.h>
#include <vlc_list.h>

# ifdef __cplusplus
extern "C" {
# endif

/** Executor type (opaque) */
typedef struct vlc_executor vlc_executor_t;

/**
 * A Runnable encapsulates a task to be run from an executor thread.
 */
struct vlc_runnable {

    /**
     * This function is to be executed by a vlc_executor_t.
     *
     * It must implement the actions (arbitrarily long) to execute from an
     * executor thread, synchronously. As soon as run() returns, the execution
     * of this runnable is complete.
     *
     * After the runnable is submitted to an executor via
     * vlc_executor_Submit(), the run() function is executed at most once (zero
     * if the execution is canceled before it was started).
     *
     * It must not be NULL.
     *
     * \param userdata the userdata provided to vlc_executor_Submit()
     */
    void (*run)(void *userdata);

    /**
     * Userdata passed back to run().
     */
    void *userdata;

    /* Private data used by the vlc_executor_t (do not touch) */
    struct vlc_list node;
};

/**
 * Create a new executor.
 *
 * \param max_threads the maximum number of threads used to execute runnables
 * \return a pointer to a new executor, or NULL if an error occurred
 */
VLC_API vlc_executor_t *
vlc_executor_New(unsigned max_threads);

/**
 * Delete an executor.
 *
 * Wait for all the threads to complete, and delete the executor instance.
 *
 * All submitted tasks must be either started or explicitly canceled. To wait
 * for all tasks to complete, use vlc_executor_WaitIdle().
 *
 * It is an error to submit a new runnable after vlc_executor_Delete() is
 * called. In particular, a running task must not submit a new runnable once
 * deletion has been requested.
 *
 * \param executor the executor
 */
VLC_API void
vlc_executor_Delete(vlc_executor_t *executor);

/**
 * Submit a runnable for execution.
 *
 * The struct vlc_runnable is not copied, it must exist until the end of the
 * execution (the user is expected to embed it in its own task structure).
 *
 * Here is a simple example:
 *
 * \code{c}
 *  struct my_task {
 *      char *str;
 *      struct vlc_runnable runnable;
 *  };
 *
 *  static void Run(void *userdata)
 *  {
 *      struct my_task *task = userdata;
 *
 *      printf("start of %s\n", task->str);
 *      vlc_tick_sleep(VLC_TICK_FROM_SEC(3)); // long action
 *      printf("end of %s\n", task->str);
 *
 *      free(task->str);
 *      free(task);
 *  }
 *
 *  void foo(vlc_executor_t *executor, const char *str)
 *  {
 *      // no error handling for brevity
 *      struct my_task *task = malloc(sizeof(*task));
 *      task->str = strdup(str);
 *      task->runnable.run = Run;
 *      task->runnable.userdata = task;
 *      vlc_executor_Submit(executor, &task->runnable);
 *  }
 * \endcode
 *
 * A runnable instance is intended to be submitted at most once. The caller is
 * expected to allocate a new task structure (embedding the runnable) for every
 * submission.
 *
 * More precisely, it is incorrect to submit a runnable already submitted that
 * is still in the pending queue (i.e. not canceled or started). This is due to
 * the intrusive linked list of runnables.
 *
 * It is strongly discouraged to submit a runnable that is currently running on
 * the executor (unless you are prepared for the run() callback to be run
 * several times in parallel).
 *
 * For simplicity, it is discouraged to submit a runnable previously submitted.
 *
 * \param executor the executor
 * \param runnable the task to run
 */
VLC_API void
vlc_executor_Submit(vlc_executor_t *executor, struct vlc_runnable *runnable);

/**
 * Cancel a runnable previously submitted.
 *
 * If this runnable is still queued (i.e. it has not be run yet), then dequeue
 * it so that it will never be run, and return true.
 *
 * Otherwise, this runnable has already been taken by an executor thread (it is
 * still running or is complete). In that case, do nothing, and return false.
 *
 * This is an error to pass a runnable not submitted to this executor (the
 * result is undefined in that case).
 *
 * Note that the runnable instance is owned by the caller, so the executor will
 * never attempt to free it.
 *
 * \param executor the executor
 * \param runnable the task to cancel
 * \retval true if the runnable has been canceled before execution
 * \retval false if the runnable has not been canceled
 */
VLC_API bool
vlc_executor_Cancel(vlc_executor_t *executor, struct vlc_runnable *runnable);

/**
 * Wait until all submitted tasks are completed or canceled.
 *
 * \param executor the executor
 */
VLC_API void
vlc_executor_WaitIdle(vlc_executor_t *executor);

# ifdef __cplusplus
}
# endif

 #endif
