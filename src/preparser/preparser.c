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
#include <vlc_preparser.h>

#include "input/input_interface.h"
#include "input/input_internal.h"
#include "fetcher.h"

struct vlc_preparser_t
{
    vlc_object_t* owner;
    input_fetcher_t* fetcher;
    vlc_executor_t *parser;
    vlc_tick_t timeout;
    atomic_bool deactivated;

    vlc_mutex_t lock;
    vlc_preparser_req_id current_id;
    struct vlc_list submitted_tasks; /**< list of struct task */
};

struct task
{
    vlc_preparser_t *preparser;
    input_item_t *item;
    int options;
    const input_item_parser_cbs_t *cbs;
    void *userdata;
    vlc_preparser_req_id id;

    vlc_sem_t preparse_ended;
    int preparse_status;
    atomic_bool interrupted;

    struct vlc_runnable runnable; /**< to be passed to the executor */

    struct vlc_list node; /**< node of vlc_preparser_t.submitted_tasks */
};

static struct task *
TaskNew(vlc_preparser_t *preparser, void (*run)(void *), input_item_t *item,
        int options, const input_item_parser_cbs_t *cbs, void *userdata)
{
    struct task *task = malloc(sizeof(*task));
    if (!task)
        return NULL;

    task->preparser = preparser;
    task->item = item;
    task->options = options;
    task->cbs = cbs;
    task->userdata = userdata;

    input_item_Hold(item);

    vlc_sem_init(&task->preparse_ended, 0);
    task->preparse_status = VLC_EGENERIC;
    atomic_init(&task->interrupted, false);

    task->runnable.run = run;
    task->runnable.userdata = task;

    return task;
}

static void
TaskDelete(struct task *task)
{
    input_item_Release(task->item);
    free(task);
}

static vlc_preparser_req_id
PreparserGetNextTaskIdLocked(vlc_preparser_t *preparser, struct task *task)
{
    vlc_preparser_req_id id = task->id = preparser->current_id++;
    static_assert(VLC_PREPARSER_REQ_ID_INVALID == 0, "Invalid id should be 0");
    if (unlikely(preparser->current_id == 0)) /* unsigned wrapping */
        ++preparser->current_id;
    return id;
}

static vlc_preparser_req_id
PreparserAddTask(vlc_preparser_t *preparser, struct task *task)
{
    vlc_mutex_lock(&preparser->lock);
    vlc_preparser_req_id id = PreparserGetNextTaskIdLocked(preparser, task);
    vlc_list_append(&task->node, &preparser->submitted_tasks);
    vlc_mutex_unlock(&preparser->lock);
    return id;
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
    if (atomic_load(&task->interrupted))
        task->preparse_status = -EINTR;
    else if (task->preparse_status == VLC_SUCCESS)
        input_item_SetPreparsed(task->item);

    task->cbs->on_ended(task->item, task->preparse_status, task->userdata);
}

static void
OnParserEnded(input_item_t *item, int status, void *task_)
{
    VLC_UNUSED(item);
    struct task *task = task_;

    task->preparse_status = status;
    vlc_sem_post(&task->preparse_ended);
}

static void
OnParserSubtreeAdded(input_item_t *item, input_item_node_t *subtree,
                     void *task_)
{
    VLC_UNUSED(item);
    struct task *task = task_;

    if (atomic_load(&task->interrupted))
        return;

    if (task->cbs->on_subtree_added)
        task->cbs->on_subtree_added(task->item, subtree, task->userdata);
}

static void
OnParserAttachmentsAdded(input_item_t *item,
                         input_attachment_t *const *array,
                         size_t count, void *task_)
{
    VLC_UNUSED(item);
    struct task *task = task_;

    if (atomic_load(&task->interrupted))
        return;

    if (task->cbs->on_attachments_added)
        task->cbs->on_attachments_added(task->item, array, count, task->userdata);
}

static void
OnArtFetchEnded(input_item_t *item, bool fetched, void *userdata)
{
    VLC_UNUSED(item);
    VLC_UNUSED(fetched);

    struct task *task = userdata;

    NotifyPreparseEnded(task);
    TaskDelete(task);
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
        .on_attachments_added = OnParserAttachmentsAdded,
    };

    vlc_object_t *obj = task->preparser->owner;
    const struct input_item_parser_cfg cfg = {
        .cbs = &cbs,
        .cbs_data = task,
        .subitems = task->options & VLC_PREPARSER_OPTION_SUBITEMS,
        .interact = task->options & VLC_PREPARSER_OPTION_INTERACT,
    };
    input_item_parser_id_t *parser =
        input_item_Parse(obj, task->item, &cfg);
    if (parser == NULL)
    {
        task->preparse_status = VLC_EGENERIC;
        return;
    }

    /* Wait until the end of parsing */
    if (deadline == VLC_TICK_INVALID)
        vlc_sem_wait(&task->preparse_ended);
    else
        if (vlc_sem_timedwait(&task->preparse_ended, deadline))
        {
            input_item_parser_id_Release(parser);
            task->preparse_status = VLC_ETIMEOUT;
            return;
        }

    /* This call also interrupts the parsing if it is still running */
    input_item_parser_id_Release(parser);
}

static int
Fetch(struct task *task)
{
    input_fetcher_t *fetcher = task->preparser->fetcher;
    if (!fetcher || !(task->options & VLC_PREPARSER_TYPE_FETCHMETA_ALL))
        return VLC_ENOENT;

    return input_fetcher_Push(fetcher, task->item,
                              task->options & VLC_PREPARSER_TYPE_FETCHMETA_ALL,
                              &input_fetcher_callbacks, task);
}

static void
ParserRun(void *userdata)
{
    vlc_thread_set_name("vlc-run-prepars");

    struct task *task = userdata;
    vlc_preparser_t *preparser = task->preparser;

    vlc_tick_t deadline = preparser->timeout ? vlc_tick_now() + preparser->timeout
                                             : VLC_TICK_INVALID;

    if (task->options & VLC_PREPARSER_TYPE_PARSE)
    {
        if (atomic_load(&task->interrupted))
        {
            PreparserRemoveTask(preparser, task);
            goto end;
        }

        Parse(task, deadline);
    }

    PreparserRemoveTask(preparser, task);

    if (task->preparse_status == VLC_ETIMEOUT || atomic_load(&task->interrupted))
        goto end;

    int ret = Fetch(task);

    if (ret == VLC_SUCCESS)
        return; /* Remove the task and notify from the fetcher callback */

end:
    NotifyPreparseEnded(task);
    TaskDelete(task);
}

static void
Interrupt(struct task *task)
{
    atomic_store(&task->interrupted, true);

    vlc_sem_post(&task->preparse_ended);
}

vlc_preparser_t* vlc_preparser_New( vlc_object_t *parent, unsigned max_threads,
                                    vlc_tick_t timeout, int request_type )
{
    assert(max_threads >= 1);
    assert(timeout >= 0);
    assert(request_type & (VLC_PREPARSER_TYPE_FETCHMETA_ALL|VLC_PREPARSER_TYPE_PARSE));

    vlc_preparser_t* preparser = malloc( sizeof *preparser );
    if (!preparser)
        return NULL;

    preparser->timeout = timeout;
    preparser->owner = parent;

    if (request_type & VLC_PREPARSER_TYPE_PARSE)
    {
        preparser->parser = vlc_executor_New(max_threads);
        if (!preparser->parser)
            goto error_parser;
    }
    else
        preparser->parser = NULL;

    if (request_type & VLC_PREPARSER_TYPE_FETCHMETA_ALL)
    {
        preparser->fetcher = input_fetcher_New(parent, request_type);
        if (unlikely(preparser->fetcher == NULL))
            goto error_fetcher;
    }
    else
        preparser->fetcher = NULL;

    atomic_init( &preparser->deactivated, false );

    vlc_mutex_init(&preparser->lock);
    vlc_list_init(&preparser->submitted_tasks);
    preparser->current_id = 1;

    return preparser;

error_fetcher:
    if (preparser->parser != NULL)
        vlc_executor_Delete(preparser->parser);
error_parser:
    free(preparser);
    return NULL;
}

vlc_preparser_req_id vlc_preparser_Push( vlc_preparser_t *preparser, input_item_t *item,
                                         int type_options,
                                         const input_item_parser_cbs_t *cbs,
                                         void *cbs_userdata )
{
    if( atomic_load( &preparser->deactivated ) )
        return VLC_PREPARSER_REQ_ID_INVALID;

    assert(type_options & VLC_PREPARSER_TYPE_PARSE
        || type_options & VLC_PREPARSER_TYPE_FETCHMETA_ALL);

    assert(!(type_options & VLC_PREPARSER_TYPE_PARSE)
        || preparser->parser != NULL);
    assert(!(type_options & VLC_PREPARSER_TYPE_FETCHMETA_ALL)
        || preparser->fetcher != NULL);

    assert(cbs != NULL && cbs->on_ended != NULL);

    struct task *task =
        TaskNew(preparser, ParserRun, item, type_options, cbs, cbs_userdata);
    if( !task )
        return VLC_PREPARSER_REQ_ID_INVALID;

    if (preparser->parser != NULL)
    {
        vlc_preparser_req_id id = PreparserAddTask(preparser, task);

        vlc_executor_Submit(preparser->parser, &task->runnable);

        return id;
    }

    /* input_fetcher is not cancellable (for now) but we need to generate a new
     * id anyway. */
    vlc_mutex_lock(&preparser->lock);
    vlc_preparser_req_id id = PreparserGetNextTaskIdLocked(preparser, task);
    vlc_mutex_unlock(&preparser->lock);

    int ret = Fetch(task);
    return ret == VLC_SUCCESS ? id : 0;
}

size_t vlc_preparser_Cancel( vlc_preparser_t *preparser, vlc_preparser_req_id id )
{
    vlc_mutex_lock(&preparser->lock);

    struct task *task;
    size_t count = 0;
    vlc_list_foreach(task, &preparser->submitted_tasks, node)
    {
        if (id == VLC_PREPARSER_REQ_ID_INVALID || task->id == id)
        {
            count++;
            /* TODO: the fetcher should be cancellable too */
            bool canceled = preparser->parser != NULL
              && vlc_executor_Cancel(preparser->parser, &task->runnable);
            if (canceled)
            {
                vlc_list_remove(&task->node);
                vlc_mutex_unlock(&preparser->lock);
                task->preparse_status = -EINTR;
                task->cbs->parser.on_ended(task->item, task->preparse_status,
                                           task->userdata);
                TaskDelete(task);

                /* Small optimisation in the likely case where the user cancel
                 * only one task */
                if (id != VLC_PREPARSER_REQ_ID_INVALID)
                    return count;
                vlc_mutex_lock(&preparser->lock);
            }
            else
                /* The task will be finished and destroyed after run() */
                Interrupt(task);
        }
    }

    vlc_mutex_unlock(&preparser->lock);

    return count;
}

void vlc_preparser_Deactivate( vlc_preparser_t* preparser )
{
    atomic_store( &preparser->deactivated, true );
    vlc_preparser_Cancel(preparser, 0);
}

void vlc_preparser_SetTimeout( vlc_preparser_t *preparser,
                               vlc_tick_t timeout )
{
    preparser->timeout = timeout;
}

void vlc_preparser_Delete( vlc_preparser_t *preparser )
{
    /* In case vlc_preparser_Deactivate() has not been called */
    vlc_preparser_Cancel(preparser, 0);

    if (preparser->parser != NULL)
        vlc_executor_Delete(preparser->parser);

    if( preparser->fetcher )
        input_fetcher_Delete( preparser->fetcher );

    free( preparser );
}
