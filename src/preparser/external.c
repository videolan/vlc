// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * external.c: preparser with external process
 *****************************************************************************
 * Copyright © 2025 Videolabs, VideoLAN and VLC authors
 *
 * Authors: Gabriel Lafond Thenaille <gabriel@videolabs.io>
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_POLL_H
# include <poll.h>
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_configuration.h>
#include <vlc_fs.h>
#include <vlc_input_item.h>
#include <vlc_preparser.h>
#include <vlc_preparser_ipc.h>
#include <vlc_spawn.h>
#include <vlc_threads.h>
#include <vlc_interrupt.h>
#include <vlc_list.h>
#include <vlc_memstream.h>
#include <vlc_vector.h>
#include <vlc_arrays.h>
#include <vlc_executor.h>
#include <vlc_tick.h>
#include <vlc_process.h>

#include "preparser.h"

#define VLC_PREPARSER_PATH "vlc-preparser"

/*****************************************************************************
 * Preparser serdes callbacks functions
 *****************************************************************************/

struct preparser_serdes_cbs_ctx {
    vlc_tick_t timeout;
    vlc_tick_t start;
    struct vlc_process *process;
};

static ssize_t
write_cbs(const void *data, size_t size, void *userdata)
{
    struct preparser_serdes_cbs_ctx *ctx = userdata;
    assert(ctx->start != VLC_TICK_INVALID);
    assert(ctx->process != NULL);

    int timeout_ms = -1;
    if (ctx->timeout != VLC_TICK_INVALID) {
        vlc_tick_t time_elapsed = (vlc_tick_now() - ctx->start);
        if (time_elapsed >= ctx->timeout) {
            errno = ETIMEDOUT;
            return -1;
        }
        timeout_ms = MS_FROM_VLC_TICK(ctx->timeout - time_elapsed);
    }

    int ret = vlc_process_fd_Write(ctx->process, data, size, timeout_ms);
    return ret;
}

static ssize_t
read_cbs(void *data, size_t size, void *userdata)
{
    struct preparser_serdes_cbs_ctx *ctx = userdata;
    assert(ctx->start != VLC_TICK_INVALID);
    assert(ctx->process != NULL);

    int timeout_ms = -1;
    if (ctx->timeout != VLC_TICK_INVALID) {
        vlc_tick_t time_elapsed = (vlc_tick_now() - ctx->start);
        if (time_elapsed >= ctx->timeout) {
            errno = ETIMEDOUT;
            return -1;
        }
        timeout_ms = MS_FROM_VLC_TICK(ctx->timeout - time_elapsed);
    }

    int ret = vlc_process_fd_Read(ctx->process, data, size, timeout_ms);
    return ret;
}

/*****************************************************************************
 * preparser_sys structure
 *****************************************************************************/

struct preparser_sys {
    vlc_object_t *parent;

    /* shared data */
    atomic_size_t current_id; 

    /* process pool for the preparser */
    struct preparser_process_pool *pool_preparser;

    /* process pool for the thumbnailer */
    struct preparser_process_pool *pool_thumbnailer;
};

/*****************************************************************************
 * Preparser process thread structure
 *****************************************************************************/

struct preparser_process_thread {
    /* Node of the process_pool.thread list */
    struct vlc_list node;

    /* The system thread */
    vlc_thread_t thread;

    /* The process_pool owning the thread */
    struct preparser_process_pool *owner;

    /* The current process running */
    struct vlc_process *process;
    bool process_running;

    /* The current task */
    struct preparser_task *task;

    /* The thread serializer */
    struct vlc_preparser_msg_serdes *serdes;
};

/*****************************************************************************
 * Task functions
 *****************************************************************************/


union preparser_task_cbs
{
    const struct vlc_preparser_cbs *parser;
    const struct vlc_thumbnailer_cbs *thumbnailer;
    const struct vlc_thumbnailer_to_files_cbs *thumbnailer_to_files;
};

struct preparser_task {
    /** Request message */
    struct vlc_preparser_msg req_msg;

    /** Response message */
    struct vlc_preparser_msg res_msg;

    /** The preparser request */
    struct vlc_preparser_req req;
    vlc_atomic_rc_t rc;

    /** Input item used for the request */
    input_item_t *item;

    /* Thread interrupt */
    vlc_interrupt_t *interrupt;

    /** Preparser callbacks */
    union preparser_task_cbs cbs;
    void *cbs_userdata;

    struct preparser_sys *owner;

    struct vlc_list node; /**< node of vlc_preparser_t.submitted_tasks */
};

static struct preparser_task *
preparser_task_get_req_owner(struct vlc_preparser_req *req)
{
    return container_of(req, struct preparser_task, req);
}

/**
 * Execute task callbacks
 */
static void
preparser_task_ExecCallback(struct preparser_task *task, int status)
{
    assert(task != NULL);
    assert(task->item != NULL);
    assert(task->req_msg.type == VLC_PREPARSER_MSG_TYPE_REQ);
    struct vlc_preparser_msg *res_msg = &task->res_msg;
    if (status != 0) {
        vlc_preparser_msg_Init(res_msg, VLC_PREPARSER_MSG_TYPE_RES,
                               task->req_msg.req_type);
        res_msg->res.item = input_item_Hold(task->item);
        res_msg->res.status = status;
    } else {
        assert(task->req_msg.req_type == task->res_msg.req_type);
        input_item_Update(task->item, res_msg->res.item);
    }

    switch (task->req_msg.req_type) {
        case VLC_PREPARSER_MSG_REQ_TYPE_PARSE:
            assert(task->cbs.parser != NULL);
            if (task->cbs.parser->on_subtree_added != NULL
                    && res_msg->res.subtree != NULL) {
                task->cbs.parser->on_subtree_added(&task->req,
                                                   res_msg->res.subtree,
                                                   task->cbs_userdata);
                res_msg->res.subtree = NULL;
            }
            if (task->cbs.parser->on_attachments_added != NULL
                    && res_msg->res.attachments.size != 0) {
                task->cbs.parser->on_attachments_added(&task->req,
                                        res_msg->res.attachments.data,
                                        res_msg->res.attachments.size,
                                        task->cbs_userdata);
            }
            assert(task->cbs.parser->on_ended != NULL);
            task->cbs.parser->on_ended(&task->req, res_msg->res.status,
                                       task->cbs_userdata);
            break;
        case VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL:
            assert(task->cbs.thumbnailer->on_ended != NULL);
            task->cbs.thumbnailer->on_ended(&task->req,
                                            res_msg->res.status,
                                            res_msg->res.pic,
                                            task->cbs_userdata);
            break;
        case VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES:
            assert(task->cbs.thumbnailer_to_files->on_ended != NULL);
            task->cbs.thumbnailer_to_files->on_ended(&task->req,
                                                res_msg->res.status,
                                                res_msg->res.result.data,
                                                res_msg->res.result.size,
                                                task->cbs_userdata);
            break;
        default:
            vlc_assert_unreachable();
    }
}

/**
 * Execute a task: Send a request and wait for a response then call callbacks.
 */
static int
preparser_task_Run(struct preparser_process_thread *thread, vlc_tick_t timeout)
{
    vlc_thread_set_name("vlc-task-run");

    assert(thread != NULL);
    assert(thread->process != NULL);
    assert(thread->task != NULL);

    struct preparser_task *task = thread->task;
    assert(task->item != NULL);

    struct preparser_serdes_cbs_ctx serdes_ctx = {
        .start = vlc_tick_now(),
        .timeout = timeout,
        .process = thread->process,
    };

    int status = VLC_SUCCESS;
    int ret = vlc_preparser_msg_serdes_Serialize(thread->serdes,
                                                 &task->req_msg, &serdes_ctx);
    if (ret != VLC_SUCCESS) {
        status = ret;
        goto end;
    }

    ret = vlc_preparser_msg_serdes_Deserialize(thread->serdes, &task->res_msg,
                                               &serdes_ctx);
    if (ret != VLC_SUCCESS) {
        status = ret;
    }

end:
    preparser_task_ExecCallback(task, status);
    return status;
}

static struct vlc_preparser_req *
preparser_task_req_Hold(struct vlc_preparser_req *req)
{
    assert(req != NULL);
    struct preparser_task *task = preparser_task_get_req_owner(req);
    vlc_atomic_rc_inc(&task->rc);
    return req;
}

static input_item_t *
preparser_task_req_GetItem(struct vlc_preparser_req *req)
{
    assert(req != NULL);
    struct preparser_task *task = preparser_task_get_req_owner(req);
    return task->item;
}

static void
preparser_task_req_Release(struct vlc_preparser_req *req)
{
    assert(req != NULL);
    struct preparser_task *task = preparser_task_get_req_owner(req);
    assert(task->item != NULL);

    if (!vlc_atomic_rc_dec(&task->rc)) {
        return;
    }

    vlc_interrupt_destroy(task->interrupt);
    vlc_preparser_msg_Clean(&task->req_msg);
    vlc_preparser_msg_Clean(&task->res_msg);
    input_item_Release(task->item);

    free(task);
}

/**
 * Create a new task.
 */
static struct preparser_task*
preparser_task_New(input_item_t *item,
                   enum vlc_preparser_msg_req_type req_type)
{
    assert(item != NULL);
    assert(req_type == VLC_PREPARSER_MSG_REQ_TYPE_PARSE ||
           req_type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL ||
           req_type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES);

    struct preparser_task *task = malloc(sizeof(*task));
    if (task == NULL) {
        return NULL;
    }
    vlc_preparser_msg_Init(&task->req_msg, VLC_PREPARSER_MSG_TYPE_REQ,
                           req_type);

    vlc_preparser_msg_Init(&task->res_msg, VLC_PREPARSER_MSG_TYPE_RES,
                           req_type);

    task->interrupt = vlc_interrupt_create();
    if (task->interrupt == NULL) {
        vlc_preparser_msg_Clean(&task->req_msg);
        vlc_preparser_msg_Clean(&task->res_msg);
        free(task);
        return NULL;
    }

    if (item->psz_uri != NULL) {
        task->req_msg.req.uri = strdup(item->psz_uri);
        if (task->req_msg.req.uri == NULL) {
            vlc_interrupt_destroy(task->interrupt);
            vlc_preparser_msg_Clean(&task->req_msg);
            vlc_preparser_msg_Clean(&task->res_msg);
            free(task);
            return NULL;
        }
    }

    task->item = input_item_Hold(item);
    task->cbs_userdata = NULL;
    vlc_list_init(&task->node);

    static const struct vlc_preparser_req_operations ops = {
        .get_item = preparser_task_req_GetItem,
        .release = preparser_task_req_Release,
    };
    task->req.ops = &ops;
    vlc_atomic_rc_init(&task->rc);

    return task;
}

/**
 * Delete a task and its message.
 */
static void
preparser_task_Delete(struct preparser_task *task)
{
    assert(task != NULL);
    assert(task->item != NULL);

    vlc_interrupt_destroy(task->interrupt);
    vlc_preparser_msg_Clean(&task->req_msg);
    vlc_preparser_msg_Clean(&task->res_msg);
    input_item_Release(task->item);

    free(task);
}

/**
 * Init a task for a Push request.
 */
static void
preparser_task_InitPush(struct preparser_task *task, int options,
                        const union preparser_task_cbs *cbs, void *userdata)
{
    assert(task != NULL);
    struct vlc_preparser_msg *msg = &task->req_msg;
    assert(msg != NULL);
    assert(msg->type == VLC_PREPARSER_MSG_TYPE_REQ);
    assert(msg->req_type == VLC_PREPARSER_MSG_REQ_TYPE_PARSE);
    assert(cbs != NULL);

    msg->req.options = options;

    task->cbs = *cbs;
    task->cbs_userdata = userdata;
}

/**
 * Init a task for a Generate Thumbnail request.
 */
static void
preparser_task_InitThumbnail(struct preparser_task *task,
                             const struct vlc_thumbnailer_arg *arg,
                             const union preparser_task_cbs *cbs,
                             void *userdata)
{
    assert(task != NULL);
    struct vlc_preparser_msg *msg = &task->req_msg;
    assert(msg != NULL);
    assert(msg->type == VLC_PREPARSER_MSG_TYPE_REQ);
    assert(msg->req_type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL);
    assert(cbs != NULL);

    if (arg == NULL) {
        msg->req.arg.seek.type = VLC_THUMBNAILER_SEEK_NONE;
        msg->req.arg.hw_dec = false;
    } else {
        msg->req.arg = *arg;
    }

    task->cbs = *cbs;
    task->cbs_userdata = userdata;
}

/**
 * Init a task for a Generate Thumbnail To Files request.
 */
static void
preparser_task_InitThumbnailToFile(struct preparser_task *task,
                                const struct vlc_thumbnailer_arg *arg,
                                const struct vlc_thumbnailer_output *outputs,
                                size_t output_count,
                                const union preparser_task_cbs *cbs,
                                void *userdata)
{
    assert(task != NULL);
    struct vlc_preparser_msg *msg = &task->req_msg;
    assert(msg != NULL);
    assert(msg->type == VLC_PREPARSER_MSG_TYPE_REQ);
    assert(msg->req_type == VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES);
    assert(output_count == 0 || outputs != NULL);
    assert(cbs != NULL);

    if (arg == NULL) {
        msg->req.arg.seek.type = VLC_THUMBNAILER_SEEK_NONE;
        msg->req.arg.hw_dec = false;
    } else {
        msg->req.arg = *arg;
    }
    if (output_count != 0) {
        vlc_vector_push_all(&msg->req.outputs, outputs, output_count);
    }

    task->cbs = *cbs;
    task->cbs_userdata = userdata;
}

/*****************************************************************************
 * Process Pool functions
 *****************************************************************************/

struct preparser_process_pool {
    vlc_mutex_t lock;

    /** Maximum number of threads to run the tasks */
    unsigned max_threads;

    /** List of active vlc_process_executor_thread */
    struct vlc_list threads;

    /** Number of running thread */
    size_t nthreads;

    /** Unfinished task */
    size_t unfinished;

    /** List of running preparser_task */
    struct vlc_list running;

    /** Queue of preparser_task */
    struct vlc_list queue;

    /** Wait for the queue to be non-empty */
    vlc_cond_t queue_wait;

    /** True if pool deletion is requested */
    bool closing;

    /** Preparser process arguments */
    vlc_tick_t timeout;
    int types;

    char **argv;
    int argc;

    /** Parent object */
    vlc_object_t *parent;
};

/**
 * Add a task to the process pool queue and trigger the `queue_wait`.
 */
static void
preparser_pool_QueuePush(struct preparser_process_pool *pool,
                         struct preparser_task *task)
{
    assert(pool != NULL);
    vlc_mutex_assert(&pool->lock);
    assert(task != NULL);

    vlc_list_append(&task->node, &pool->queue);
    vlc_cond_signal(&pool->queue_wait);
}

/**
 * Take a task on the queue or wait for a new one to be added.
 */
static struct preparser_task*
preparser_pool_QueueTake(struct preparser_process_pool *pool)
{
    assert(pool != NULL);
    vlc_mutex_assert(&pool->lock);

    while (!pool->closing && vlc_list_is_empty(&pool->queue)) {
        vlc_cond_wait(&pool->queue_wait, &pool->lock);
    }

    if (pool->closing) {
        return NULL;
    }

    struct preparser_task *task = NULL;
    task = vlc_list_first_entry_or_null(&pool->queue, struct preparser_task,
                                        node);
    assert(task != NULL);
    vlc_list_remove(&task->node);

    return task;
}

static int
preparser_pool_SpawnProcess(struct preparser_process_thread *thread)
{
    assert(thread != NULL);
    vlc_mutex_assert(&thread->owner->lock);
    struct preparser_process_pool *pool = thread->owner;

    char *str_timeout = NULL;
    if (asprintf(&str_timeout, "%" PRId64, pool->timeout) < 0) {
        return VLC_ENOMEM;
    }
    char *str_types = NULL;
    if (asprintf(&str_types, "%" PRId32, pool->types) < 0) {
        free(str_timeout);
        return VLC_ENOMEM;
    }

    const char *argv[] = {
        "--timeout-tick",
        str_timeout,
        "--types",
        str_types,
        "--daemon",
        NULL,
    };
    int argc = ARRAY_SIZE(argv);

    char *path = NULL;
#ifdef _WIN32
    const char *name = VLC_PREPARSER_PATH ".exe";
    const char *static_name = VLC_PREPARSER_PATH "-static.exe";
#else
    const char *name = VLC_PREPARSER_PATH;
    const char *static_name = VLC_PREPARSER_PATH "-static";
#endif

    /* When running from build directory (vlc-static), VLC_LIB_PATH points to
     * TOP_BUILDDIR/modules, so we need to construct TOP_BUILDDIR/bin to
     * start vlc-preparser-static */
    const char *lib_path = getenv("VLC_LIB_PATH");
    if (lib_path != NULL) {
        size_t len = strlen(lib_path);
        if (len > 8 && strcmp(lib_path + len - 8, "/modules") == 0) {
            if (asprintf(&path, "%.*s/bin/%s", (int)(len - 8),
                         lib_path, static_name) < 0) {
                free(str_timeout);
                free(str_types);
                return VLC_ENOMEM;
            }
        }
    }

    /* If not in build directory, use the standard installed location */
    if (path == NULL) {
        path = config_GetSysPath(VLC_PKG_LIBEXEC_DIR, name);
    }

    if (path == NULL) {
        thread->process = vlc_process_Spawn(name, argc, argv);
    } else {
        thread->process = vlc_process_Spawn(path, argc, argv);
    }
    free(path);
    free(str_timeout);
    free(str_types);

    if (thread->process == NULL) {
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

/**
 * Process thread loop.
 * Take a task on the queue, then execute the task and delete it.
 * Check that the external process has not crashed otherwise start a new one.
 */
static void*
preparser_pool_Run(void *data)
{
    struct preparser_process_thread *thread = data;
    assert(thread != NULL);
    assert(thread->owner != NULL);
    assert(thread->process != NULL);

    //struct preparser_process_pool *pool = thread->owner;

    vlc_thread_set_name("vlc-pool-runner");


    vlc_mutex_lock(&thread->owner->lock);
    struct preparser_task *task = NULL;
    while ((task = preparser_pool_QueueTake(thread->owner))) {
        thread->task = task;
        vlc_list_append(&task->node, &thread->owner->running);

        vlc_tick_t timeout = thread->owner->timeout;
        vlc_interrupt_t *old = vlc_interrupt_set(task->interrupt);

        vlc_mutex_unlock(&thread->owner->lock);
        int status = preparser_task_Run(thread, timeout);
        vlc_mutex_lock(&thread->owner->lock);

        vlc_thread_set_name("vlc-pool-runner");
        vlc_interrupt_set(old);

        vlc_list_remove(&task->node);
        preparser_task_Delete(task);
        thread->task = NULL;

        assert(thread->owner->unfinished > 0);
        --thread->owner->unfinished;

        if (thread->owner->closing) {
            break;
        }

        if (status == VLC_SUCCESS) {
            continue;
        }

        /* If preparser_task_Run fails, it may indicate that the preparser is
         * stuck or not functioning correctly. In this case, the process is
         * stopped and deleted. Since the thread is not supposed to exit on
         * its own, it repeatedly attempts to restart the process every second
         * until it succeeds or the pool is shutting down. */
        vlc_process_Terminate(thread->process, true);
        thread->process_running = false;
        while (preparser_pool_SpawnProcess(thread) != VLC_SUCCESS) {
            vlc_mutex_unlock(&thread->owner->lock);
            sleep(1);
            vlc_mutex_lock(&thread->owner->lock);
            if (thread->owner->closing) {
                goto end;
            }
        }
        thread->process_running = true;
    }
end:
    vlc_mutex_unlock(&thread->owner->lock);
    return NULL;
}

/**
 * Start a new thread process with its external process.
 */
static int
preparser_pool_SpawnThread(struct preparser_process_pool *pool)
{
    assert(pool != NULL);
    assert(pool->nthreads < pool->max_threads);

    struct preparser_process_thread *thread = malloc(sizeof(*thread));
    if (thread == NULL) {
        return -ENOMEM;
    }

    thread->owner = pool;
    thread->task = NULL;

    static struct vlc_preparser_msg_serdes_cbs cbs = {
        .write = write_cbs,
        .read = read_cbs,
    };
    thread->serdes = vlc_preparser_msg_serdes_Create(pool->parent, &cbs, true);
    if (thread->serdes == NULL) {
        free(thread);
        return VLC_EGENERIC;
    }

    vlc_mutex_lock(&pool->lock);
    if (preparser_pool_SpawnProcess(thread) != VLC_SUCCESS) {
        msg_Err(pool->parent, "Fail to create Process in process_pool");
        vlc_mutex_unlock(&pool->lock);
        vlc_preparser_msg_serdes_Delete(thread->serdes);
        free(thread);
        return VLC_EGENERIC;
    }
    thread->process_running = true;

    int ret = vlc_clone(&thread->thread, preparser_pool_Run, thread);
    if (ret != 0) {
        vlc_mutex_unlock(&pool->lock);
        vlc_process_Terminate(thread->process, false);
        vlc_preparser_msg_serdes_Delete(thread->serdes);
        free(thread);
        return VLC_EGENERIC;
    }
    pool->nthreads++;
    vlc_list_append(&thread->node, &pool->threads);
    vlc_mutex_unlock(&pool->lock);

    return VLC_SUCCESS;
}

/**
 * Push a new task in the queue and check if a new process thread can be spawn.
 * If there is more unfinished task than spawned process thread and that the
 * max number of process thread is not reached, then a new thread is spwaned.
 */
static void
preparser_pool_Submit(struct preparser_process_pool *pool,
                      struct preparser_task *task)
{
    assert(pool != NULL);
    assert(task != NULL);

    vlc_mutex_lock(&pool->lock);

    assert(!pool->closing);

    preparser_pool_QueuePush(pool, task);

    bool need_new_thread = ++pool->unfinished > pool->nthreads &&
                           pool->nthreads < pool->max_threads;
    if (need_new_thread) {
        /* If it fails, this is not an error, there is at least one thread */
        preparser_pool_SpawnThread(pool);
    }

    vlc_mutex_unlock(&pool->lock);
}

/**
 * Cancel a request. If `NULL` is given, all request are canceled.
 */
static size_t
preparser_pool_Cancel(struct preparser_process_pool *pool,
                      struct vlc_preparser_req *req)
{
    assert(pool != NULL);

    vlc_mutex_lock(&pool->lock);
    size_t count = 0;
    struct preparser_task *task = NULL;
    vlc_list_foreach(task, &pool->queue, node) {
        if (req == NULL || req == &task->req) {
            count++;
            --pool->unfinished;
            vlc_list_remove(&task->node);
            preparser_task_ExecCallback(task, -EINTR);
            preparser_task_Delete(task);

            if (req != NULL) {
                vlc_mutex_unlock(&pool->lock);
                return count;
            }
        }
    }
    vlc_list_foreach(task, &pool->running, node) {
        if (req == NULL || req == &task->req) {
            count++;
            if (task->interrupt != NULL) {
                vlc_interrupt_raise(task->interrupt);
            }

            if (req != NULL) {
                vlc_mutex_unlock(&pool->lock);
                return count;
            }
        }
    }
    vlc_mutex_unlock(&pool->lock);
    return count;
}

/**
 * Clear and delete the process pool.
 */
static void
preparser_pool_Delete(struct preparser_process_pool *pool)
{
    assert(pool != NULL);

    vlc_mutex_lock(&pool->lock);

    /* All the tasks must be canceled on delete */
    assert(vlc_list_is_empty(&pool->queue));

    pool->closing = true;
    /* "closing" is now true, this will wake up threads */
    vlc_cond_broadcast(&pool->queue_wait);

    vlc_mutex_unlock(&pool->lock);

    /* The threads list may not be written at this point, so it is safe to read
     * it without mutex locked (the mutex must be released to join the
     * threads). */
    struct preparser_process_thread *thread = NULL;
    vlc_list_foreach(thread, &pool->threads, node) {
        vlc_join(thread->thread, NULL);
        if (thread->process != NULL) {
            vlc_process_Terminate(thread->process, false);
        }
        vlc_preparser_msg_serdes_Delete(thread->serdes);
        free(thread);
    }

    /* The queue must still be empty (no runnable submitted a new runnable) */
    assert(vlc_list_is_empty(&pool->queue));

    /* There are no tasks anymore */
    assert(!pool->unfinished);

    free(pool);
}

/**
 * Create a new process pool with `max` maximum process.
 */
static struct preparser_process_pool*
preparser_pool_New(vlc_object_t *obj, size_t max, vlc_tick_t timeout,
                   int types)
{
    assert(obj != NULL);
    assert(max != 0);

    struct preparser_process_pool *pool = malloc(sizeof(*pool));
    if (pool == NULL) {
        return NULL;
    }

    vlc_mutex_init(&pool->lock);

    pool->max_threads = max;
    pool->nthreads = 0;
    pool->unfinished = 0;
    pool->timeout = timeout;
    pool->types = types;

    vlc_list_init(&pool->threads);
    vlc_list_init(&pool->queue);
    vlc_list_init(&pool->running);

    vlc_cond_init(&pool->queue_wait);

    pool->closing = false;
    pool->parent = obj;

    /* Spawn one thread so sumbit will never fail */
    int ret = preparser_pool_SpawnThread(pool);
    if (ret != VLC_SUCCESS) {
        free(pool);
        return NULL;
    }

    return pool;
}



/*****************************************************************************
 * Preparser operations
 *****************************************************************************/

/**
 * Preparser push operation. (see `vlc_preparser_Push`)
 */
static struct vlc_preparser_req *
preparser_Push(void *opaque, input_item_t *item, int options,
               const struct vlc_preparser_cbs *cbs, void *cbs_userdata)
{
    struct preparser_sys *sys = opaque;

    assert(sys != NULL);
    assert(sys->pool_preparser != NULL);
    assert(item != NULL);
    assert(cbs != NULL);

    const union preparser_task_cbs task_cbs = {
        .parser = cbs,
    };

    struct preparser_task *task = NULL;
    task = preparser_task_New(item, VLC_PREPARSER_MSG_REQ_TYPE_PARSE);
    if (task == NULL) {
        return NULL;
    }
    preparser_task_InitPush(task, options, &task_cbs, cbs_userdata);

    struct vlc_preparser_req *req = preparser_task_req_Hold(&task->req);
    preparser_pool_Submit(sys->pool_preparser, task);
    return req;
}

/**
 * Preparser GenerateThumbnail operation.
 * (see `vlc_preparser_GenerateThumbnail`)
 */
static struct vlc_preparser_req *
preparser_GenerateThumbnail(void *opaque, input_item_t *item,
                            const struct vlc_thumbnailer_arg *thumb_arg,
                            const struct vlc_thumbnailer_cbs *cbs,
                            void *cbs_userdata)
{
    struct preparser_sys *sys = opaque;

    assert(sys != NULL);
    assert(sys->pool_thumbnailer != NULL);
    assert(item != NULL);
    assert(cbs != NULL);

    const union preparser_task_cbs task_cbs = {
        .thumbnailer = cbs,
    };

    struct preparser_task *task = NULL;
    task = preparser_task_New(item, VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL);
    if (task == NULL) {
        return NULL;
    }
    preparser_task_InitThumbnail(task, thumb_arg, &task_cbs, cbs_userdata);

    struct vlc_preparser_req *req = preparser_task_req_Hold(&task->req);
    preparser_pool_Submit(sys->pool_thumbnailer, task);
    return req;
}

/**
 * Preparser GenerateThumbnailToFiles operation.
 * (see `vlc_preparser_GenerateThumbnailToFiles`)
 */
static struct vlc_preparser_req *
preparser_GenerateThumbnailToFiles(void *opaque, input_item_t *item,
                                const struct vlc_thumbnailer_arg *thumb_arg,
                                const struct vlc_thumbnailer_output *outputs,
                                size_t output_count,
                                const struct vlc_thumbnailer_to_files_cbs *cbs,
                                void *cbs_userdata)
{
    struct preparser_sys *sys = opaque;

    assert(sys != NULL);
    assert(sys->pool_thumbnailer != NULL);
    assert(item != NULL);
    assert(cbs != NULL);

    const union preparser_task_cbs task_cbs = {
        .thumbnailer_to_files = cbs,
    };

    struct preparser_task *task = NULL;
    task = preparser_task_New(item,
                              VLC_PREPARSER_MSG_REQ_TYPE_THUMBNAIL_TO_FILES);
    if (task == NULL) {
        return NULL;
    }
    preparser_task_InitThumbnailToFile(task, thumb_arg, outputs, output_count,
                                       &task_cbs, cbs_userdata);

    struct vlc_preparser_req *req = preparser_task_req_Hold(&task->req);
    preparser_pool_Submit(sys->pool_thumbnailer, task);
    return req;
}

/**
 * Preparser Cancel operation.
 * (see `vlc_preparser_Cancel`)
 */
static size_t preparser_Cancel(void *opaque, struct vlc_preparser_req *req)
{
    struct preparser_sys *sys = opaque;
    assert(sys != NULL);

    size_t count = 0;
    if (sys->pool_preparser != NULL) {
        count = preparser_pool_Cancel(sys->pool_preparser, req);
    }

    if (sys->pool_thumbnailer != NULL) {
        count = preparser_pool_Cancel(sys->pool_thumbnailer, req);
    }

    return count;
}

/**
 * Preparser SetTimeout operation.
 * (see `vlc_preparser_SetTimeout`)
 */
static void preparser_SetTimeout(void *opaque, vlc_tick_t timeout)
{
    struct preparser_sys *sys = opaque;
    assert(sys != NULL);

    if (sys->pool_preparser != NULL) {
        vlc_mutex_lock(&sys->pool_preparser->lock);
        sys->pool_preparser->timeout = timeout;
        vlc_mutex_unlock(&sys->pool_preparser->lock);
    }
    if (sys->pool_thumbnailer != NULL) {
        vlc_mutex_lock(&sys->pool_thumbnailer->lock);
        sys->pool_thumbnailer->timeout = timeout;
        vlc_mutex_unlock(&sys->pool_thumbnailer->lock);
    }
}

/**
 * Preparser Delete operation.
 * (see `vlc_preparser_Delete`)
 */
static void preparser_Delete(void *opaque)
{
    struct preparser_sys *sys = opaque;
    assert(sys != NULL);

    if (sys->pool_preparser != NULL) {
        preparser_pool_Cancel(sys->pool_preparser, NULL);
        preparser_pool_Delete(sys->pool_preparser);
        sys->pool_preparser = NULL;
    }

    if (sys->pool_thumbnailer != NULL) {
        preparser_pool_Cancel(sys->pool_thumbnailer, NULL);
        preparser_pool_Delete(sys->pool_thumbnailer);
        sys->pool_thumbnailer = NULL;
    }

    free(sys);
}

/*****************************************************************************
 * New function
 *****************************************************************************/

/**
 * Create a new preparser with an external process.
 * (see `vlc_preparser_New`)
 */
void *vlc_preparser_external_New(vlc_preparser_t *owner, vlc_object_t *parent,
                                 const struct vlc_preparser_cfg *cfg)
{
    assert(owner != NULL);
    assert(parent != NULL);

    struct preparser_sys *sys = malloc(sizeof(*sys));
    if (sys == NULL) {
        return NULL;
    }

    sys->parent = parent;
    atomic_init(&sys->current_id, 1);
    sys->pool_preparser = NULL;
    sys->pool_thumbnailer = NULL;

    if (cfg->types & VLC_PREPARSER_TYPE_PARSE) {
        size_t nprocess = cfg->max_parser_threads;
        if (nprocess == 0) {
            nprocess = 1;
        }
        sys->pool_preparser = preparser_pool_New(parent, nprocess,
                                                 cfg->timeout,
                                                 cfg->types);
        if (sys->pool_preparser == NULL) {
            goto end;
        }
    }

    if (cfg->types & (VLC_PREPARSER_TYPE_THUMBNAIL|
                      VLC_PREPARSER_TYPE_THUMBNAIL_TO_FILES)) {
        size_t nprocess = cfg->max_thumbnailer_threads;
        if (nprocess == 0) {
            nprocess = 1;
        }
        sys->pool_thumbnailer = preparser_pool_New(parent, nprocess,
                                                   cfg->timeout,
                                                   cfg->types);
        if (sys->pool_thumbnailer == NULL) {
            goto end;
        }
    }

    static const struct vlc_preparser_operations ops = {
        .push = preparser_Push,
        .generate_thumbnail = preparser_GenerateThumbnail,
        .generate_thumbnail_to_files = preparser_GenerateThumbnailToFiles,
        .cancel = preparser_Cancel,
        .delete = preparser_Delete,
        .set_timeout = preparser_SetTimeout,
    };
    owner->ops = &ops;

    return sys;
end:
    preparser_Delete(sys);
    return NULL;
}
