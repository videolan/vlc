/*****************************************************************************
 * preparser.c
 *****************************************************************************
 * Copyright © 2017-2017 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_executor.h>

#include "input/input_interface.h"
#include "input/input_internal.h"
#include "preparser.h"
#include "fetcher.h"

struct vlc_preparser_t
{
    vlc_object_t* owner;
    input_fetcher_t* fetcher;
    vlc_executor_t *executor;
    vlc_tick_t default_timeout;
    atomic_bool deactivated;

    vlc_mutex_t lock;
    struct vlc_list submitted_tasks; /**< list of struct task */
};

struct task
{
    vlc_preparser_t *preparser;
    input_item_t *item;
    input_item_meta_request_option_t options;
    const struct vlc_metadata_cbs *cbs;
    void *userdata;
    void *id;
    vlc_tick_t timeout;

    input_item_parser_id_t *parser;

    vlc_sem_t preparse_ended;
    vlc_sem_t fetch_ended;
    atomic_int preparse_status;
    atomic_bool interrupted;
    bool art_fetched;

    struct vlc_runnable runnable; /**< to be passed to the executor */

    struct vlc_list node; /**< node of vlc_preparser_t.submitted_tasks */
};

static void RunnableRun(void *);

static struct task *
TaskNew(vlc_preparser_t *preparser, input_item_t *item,
        input_item_meta_request_option_t options,
        const struct vlc_metadata_cbs *cbs, void *userdata,
        void *id, vlc_tick_t timeout)
{
    assert(timeout >= 0);

    struct task *task = malloc(sizeof(*task));
    if (!task)
        return NULL;

    task->preparser = preparser;
    task->item = item;
    task->options = options;
    task->cbs = cbs;
    task->userdata = userdata;
    task->id = id;
    task->timeout = timeout;
    task->art_fetched = false;

    input_item_Hold(item);

    task->parser = NULL;
    vlc_sem_init(&task->preparse_ended, 0);
    vlc_sem_init(&task->fetch_ended, 0);
    atomic_init(&task->preparse_status, ITEM_PREPARSE_SKIPPED);
    atomic_init(&task->interrupted, false);

    task->runnable.run = RunnableRun;
    task->runnable.userdata = task;

    return task;
}

static void
TaskDelete(struct task *task)
{
    input_item_Release(task->item);
    free(task);
}

static void
PreparserAddTask(vlc_preparser_t *preparser, struct task *task)
{
    vlc_mutex_lock(&preparser->lock);
    vlc_list_append(&task->node, &preparser->submitted_tasks);
    vlc_mutex_unlock(&preparser->lock);
}

static void
PreparserRemoveTask(vlc_preparser_t *preparser, struct task *task)
{
    vlc_mutex_lock(&preparser->lock);
    vlc_list_remove(&task->node);
    vlc_mutex_unlock(&preparser->lock);
}

static void
NotifyPreparseEnded(struct task *task)
{
    if (task->cbs == NULL)
        return;

    if (task->options & META_REQUEST_OPTION_FETCH_ANY
     && task->cbs->on_art_fetch_ended)
        task->cbs->on_art_fetch_ended(task->item, task->art_fetched,
                                      task->userdata);

    if (task->cbs->on_preparse_ended) {
        int status = atomic_load_explicit(&task->preparse_status,
                                          memory_order_relaxed);
        task->cbs->on_preparse_ended(task->item, status, task->userdata);
    }
}

static void
OnParserEnded(input_item_t *item, int status, void *task_)
{
    VLC_UNUSED(item);
    struct task *task = task_;

    if (atomic_load(&task->interrupted))
        /*
         * On interruption, the call to input_item_parser_id_Release() may
         * trigger this "parser ended" callback. Ignore it.
         */
        return;

    atomic_store_explicit(&task->preparse_status,
                          status == VLC_SUCCESS ? ITEM_PREPARSE_DONE
                                                : ITEM_PREPARSE_FAILED,
                          memory_order_relaxed);
    vlc_sem_post(&task->preparse_ended);
}

static void
OnParserSubtreeAdded(input_item_t *item, input_item_node_t *subtree,
                     void *task_)
{
    VLC_UNUSED(item);
    struct task *task = task_;

    if (task->cbs && task->cbs->on_subtree_added)
        task->cbs->on_subtree_added(task->item, subtree, task->userdata);
}

static void
OnArtFetchEnded(input_item_t *item, bool fetched, void *userdata)
{
    VLC_UNUSED(item);
    VLC_UNUSED(fetched);

    struct task *task = userdata;
    task->art_fetched = fetched;

    vlc_sem_post(&task->fetch_ended);
}

static const input_fetcher_callbacks_t input_fetcher_callbacks = {
    .on_art_fetch_ended = OnArtFetchEnded,
};

static void
Parse(struct task *task, vlc_tick_t deadline)
{
    static const input_item_parser_cbs_t cbs = {
        .on_ended = OnParserEnded,
        .on_subtree_added = OnParserSubtreeAdded,
    };

    vlc_object_t *obj = task->preparser->owner;
    task->parser = input_item_Parse(task->item, obj, &cbs, task);
    if (!task->parser)
    {
        atomic_store_explicit(&task->preparse_status, ITEM_PREPARSE_FAILED,
                              memory_order_relaxed);
        return;
    }

    /* Wait until the end of parsing */
    if (deadline == VLC_TICK_INVALID)
        vlc_sem_wait(&task->preparse_ended);
    else
        if (vlc_sem_timedwait(&task->preparse_ended, deadline))
        {
            atomic_store_explicit(&task->preparse_status,
                                  ITEM_PREPARSE_TIMEOUT, memory_order_relaxed);
            atomic_store(&task->interrupted, true);
        }

    /* This call also interrupts the parsing if it is still running */
    input_item_parser_id_Release(task->parser);
}

static void
Fetch(struct task *task)
{
    input_fetcher_t *fetcher = task->preparser->fetcher;
    if (!fetcher || !(task->options & META_REQUEST_OPTION_FETCH_ANY))
        return;

    int ret =
        input_fetcher_Push(fetcher, task->item,
                           task->options & META_REQUEST_OPTION_FETCH_ANY,
                           &input_fetcher_callbacks, task);
    if (ret != VLC_SUCCESS)
        return;

    /* Wait until the end of fetching (fetching is not interruptible) */
    vlc_sem_wait(&task->fetch_ended);
}

static void
RunnableRun(void *userdata)
{
    vlc_thread_set_name("vlc-run-prepars");

    struct task *task = userdata;

    vlc_tick_t deadline = task->timeout ? vlc_tick_now() + task->timeout
                                        : VLC_TICK_INVALID;

    if (task->options & (META_REQUEST_OPTION_SCOPE_ANY|
                         META_REQUEST_OPTION_SCOPE_FORCED))
    {
        if (atomic_load(&task->interrupted))
            goto end;

        Parse(task, deadline);
    }

    if (atomic_load(&task->interrupted))
        goto end;

    Fetch(task);

    if (atomic_load(&task->interrupted))
        goto end;

    input_item_SetPreparsed(task->item, true);

end:
    NotifyPreparseEnded(task);
    vlc_preparser_t *preparser = task->preparser;
    PreparserRemoveTask(preparser, task);
    TaskDelete(task);
}

static void
Interrupt(struct task *task)
{
    atomic_store(&task->interrupted, true);

    /* Wake up the preparser cond_wait */
    atomic_store_explicit(&task->preparse_status, ITEM_PREPARSE_TIMEOUT,
                          memory_order_relaxed);
    vlc_sem_post(&task->preparse_ended);
}

vlc_preparser_t* vlc_preparser_New( vlc_object_t *parent )
{
    vlc_preparser_t* preparser = malloc( sizeof *preparser );
    if (!preparser)
        return NULL;

    int max_threads = var_InheritInteger(parent, "preparse-threads");
    if (max_threads < 1)
        max_threads = 1;

    preparser->executor = vlc_executor_New(max_threads);
    if (!preparser->executor)
    {
        free(preparser);
        return NULL;
    }

    preparser->default_timeout =
        VLC_TICK_FROM_MS(var_InheritInteger(parent, "preparse-timeout"));
    if (preparser->default_timeout < 0)
        preparser->default_timeout = 0;

    preparser->owner = parent;
    preparser->fetcher = input_fetcher_New( parent );
    atomic_init( &preparser->deactivated, false );

    vlc_mutex_init(&preparser->lock);
    vlc_list_init(&preparser->submitted_tasks);

    if( unlikely( !preparser->fetcher ) )
        msg_Warn( parent, "unable to create art fetcher" );

    return preparser;
}

int vlc_preparser_Push( vlc_preparser_t *preparser,
    input_item_t *item, input_item_meta_request_option_t i_options,
    const struct vlc_metadata_cbs *cbs, void *cbs_userdata,
    int timeout_ms, void *id )
{
    if( atomic_load( &preparser->deactivated ) )
        return VLC_EGENERIC;

    vlc_mutex_lock( &item->lock );
    enum input_item_type_e i_type = item->i_type;
    int b_net = item->b_net;
    if( i_options & META_REQUEST_OPTION_DO_INTERACT )
        item->b_preparse_interact = true;
    vlc_mutex_unlock( &item->lock );

    if (!(i_options & META_REQUEST_OPTION_SCOPE_FORCED))
    {
        switch( i_type )
        {
            case ITEM_TYPE_NODE:
            case ITEM_TYPE_FILE:
            case ITEM_TYPE_DIRECTORY:
            case ITEM_TYPE_PLAYLIST:
                if( !b_net || i_options & META_REQUEST_OPTION_SCOPE_NETWORK )
                    break;
                /* fallthrough */
            default:
                if( ( i_options & META_REQUEST_OPTION_FETCH_ANY ) == 0 )
                {
                    /* Nothing to do (no preparse and not fetch), notify it */
                    if (cbs && cbs->on_preparse_ended)
                        cbs->on_preparse_ended(item, ITEM_PREPARSE_SKIPPED,
                                               cbs_userdata);
                    return VLC_SUCCESS;
                }
                /* Continue without parsing (but fetching) */
                i_options &= ~META_REQUEST_OPTION_SCOPE_ANY;
        }
    }

    vlc_tick_t timeout = timeout_ms == -1 ? preparser->default_timeout
                                          : VLC_TICK_FROM_MS(timeout_ms);
    struct task *task =
        TaskNew(preparser, item, i_options, cbs, cbs_userdata, id, timeout);
    if( !task )
        return VLC_ENOMEM;

    PreparserAddTask(preparser, task);

    vlc_executor_Submit(preparser->executor, &task->runnable);
    return VLC_SUCCESS;
}

void vlc_preparser_Cancel( vlc_preparser_t *preparser, void *id )
{
    vlc_mutex_lock(&preparser->lock);

    struct task *task;
    vlc_list_foreach(task, &preparser->submitted_tasks, node)
    {
        if (!id || task->id == id)
        {
            bool canceled =
                vlc_executor_Cancel(preparser->executor, &task->runnable);
            if (canceled)
            {
                NotifyPreparseEnded(task);
                vlc_list_remove(&task->node);
                TaskDelete(task);
            }
            else
                /* The task will be finished and destroyed after run() */
                Interrupt(task);
        }
    }

    vlc_mutex_unlock(&preparser->lock);
}

void vlc_preparser_Deactivate( vlc_preparser_t* preparser )
{
    atomic_store( &preparser->deactivated, true );
    vlc_preparser_Cancel(preparser, NULL);
}

void vlc_preparser_Delete( vlc_preparser_t *preparser )
{
    /* In case vlc_preparser_Deactivate() has not been called */
    vlc_preparser_Cancel(preparser, NULL);

    vlc_executor_Delete(preparser->executor);

    if( preparser->fetcher )
        input_fetcher_Delete( preparser->fetcher );

    free( preparser );
}
