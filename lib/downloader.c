/*****************************************************************************
 * downloader.c: LibVLC Downloader API
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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
#include "config.h"
#endif

#include <vlc/libvlc_downloader.h>

#include <vlc_access.h>
#include <vlc_interrupt.h>
#include <vlc_stream.h>
#include <vlc_threads.h>

#include "libvlc_internal.h"

#define BUFFER_SIZE 65536

struct libvlc_downloader_thread
{
    /* Node of libvlc_downloader_t.threads list */
    struct vlc_list node;

    /* The task thread */
    vlc_thread_t thread;

    /* Thread termination state */
    bool terminated;
};

struct libvlc_downloader_t
{
    libvlc_instance_t *libvlc;
    vlc_mutex_t lock;
    libvlc_parser_t *parser;

    /* list of ongoing tasks (terminated tasks are removed) */
    struct vlc_list submitted_tasks;

    /* list of downloader threads (dead threads joined at libvlc_downloader_queue,
       all threads are joined at libvlc_downloader_destroy) */
    struct vlc_list threads;
};

struct libvlc_downloader_task
{
    libvlc_downloader_t *downloader;
    struct vlc_list node;
    libvlc_downloader_status_t status;

    libvlc_parser_task *parser_task;

    libvlc_media_t *media;
    stream_t *s;

    const struct libvlc_downloader_cbs *cbs;
    void *cbs_opaque;

    /* interrupt context (to interrupt blocking read) */
    vlc_interrupt_t *interrupt;

    /* required for pause/resume */
    bool pause_requested;
    vlc_cond_t interrupt_cond;

    /* required for ongoing task abortion */
    bool interrupted;

    vlc_atomic_rc_t rc;

    struct libvlc_downloader_thread *thread;
};

/* Create a new downloader task */
static struct libvlc_downloader_task *
DownloaderTaskNew(libvlc_downloader_t *downloader, libvlc_media_t *media,
                  const struct libvlc_downloader_cbs *cbs, void *cbs_opaque)
{
    struct libvlc_downloader_task *task = malloc(sizeof(*task));
    if (task == NULL)
        return NULL;

    task->interrupt = vlc_interrupt_create();
    if (task->interrupt == NULL)
    {
        free(task);
        return NULL;
    }

    task->downloader = downloader;
    task->media = libvlc_media_retain(media);
    task->s = NULL;
    task->cbs = cbs;
    task->cbs_opaque = cbs_opaque;
    task->pause_requested = false;
    vlc_cond_init(&task->interrupt_cond);
    task->interrupted = false;
    task->status = libvlc_downloader_status_pending;
    task->parser_task = NULL;
    task->thread = NULL;

    vlc_atomic_rc_init(&task->rc);

    return task;
}

/* Delete a downloader task */
static void
DownloaderTaskDestroy(struct libvlc_downloader_task *task)
{
    libvlc_media_release(task->media);
    vlc_interrupt_destroy(task->interrupt);

    if (task->parser_task)
        libvlc_parser_task_release(task->parser_task);

    if (task->s)
        vlc_stream_Delete(task->s);

    free(task);
}

/* Interrupt a running downloader task */
static void
DownloaderTaskInterruptLocked(struct libvlc_downloader_task *task)
{
    task->interrupted = true;
    vlc_interrupt_kill(task->interrupt);
    vlc_cond_signal(&task->interrupt_cond);
}

/* report downloader state changes */
static void libvlc_downloader_task_ReportStateLocked(struct libvlc_downloader_task *task,
                                                     libvlc_downloader_status_t new_status)
{
    bool status_changed = false;

    if (task->interrupted)
        new_status = libvlc_downloader_status_cancelled;

    libvlc_downloader_status_t old_status = task->status;
    if (old_status != new_status)
    {
        task->status = new_status;
        status_changed = true;
    }

    /* report on_state_update only when the state changed */
    if (status_changed)
        task->cbs->on_state_update(task->cbs_opaque, task, new_status);
}

static void *
downloaderThread(void *data)
{
    vlc_thread_set_name("downloader");
    struct libvlc_downloader_task *task = data;
    libvlc_downloader_t *downloader = task->downloader;
    libvlc_downloader_status_t status = libvlc_downloader_status_error;
    uint8_t *buf = NULL;

    vlc_interrupt_set(task->interrupt);

    char *mrl = libvlc_media_get_mrl(task->media);
    if (mrl == NULL)
        goto cleanup;

    task->s = vlc_access_NewMRL(VLC_OBJECT(downloader->libvlc->p_libvlc_int), mrl);
    free(mrl);

    if (task->s == NULL || !vlc_stream_CanDownload(task->s))
        goto cleanup;

    uint64_t stream_size = 0;
    /* only files with valid size allowed */
    if (vlc_stream_GetSize(task->s, &stream_size) != VLC_SUCCESS)
        goto cleanup;

    buf = malloc(BUFFER_SIZE);
    if (buf == NULL)
        goto cleanup;

    uint64_t total_read = 0;
    size_t buf_len = 0;
    size_t consumed_offset = 0;
    status = libvlc_downloader_status_cancelled;

    while (true)
    {
        vlc_mutex_lock(&downloader->lock);
        if (task->interrupted)
            goto cleanup_locked;

        /* Pause handling */
        if (task->pause_requested)
        {
            libvlc_downloader_task_ReportStateLocked(task, libvlc_downloader_status_paused);
            vlc_cond_wait(&task->interrupt_cond, &downloader->lock);
            vlc_mutex_unlock(&downloader->lock);
            continue;
        }

        libvlc_downloader_task_ReportStateLocked(task, libvlc_downloader_status_running);

        /* fetch next chunk only when the previous one is fully drained */
        if (consumed_offset == buf_len)
        {
            vlc_mutex_unlock(&downloader->lock);

            ssize_t read = vlc_stream_ReadPartial(task->s, buf, BUFFER_SIZE);

            if (read < 0)
                continue;

            vlc_mutex_lock(&downloader->lock);
            /* check whether task was interrupted during blocking read operation */
            if (task->interrupted)
                goto cleanup_locked;

            if (read == 0) /* EOF */
            {
                status = libvlc_downloader_status_finished;
                goto cleanup_locked;
            }

            buf_len = (size_t)read;
            consumed_offset = 0;
            total_read += (uint64_t)read;

            /* update size in case it changed; a valid size is required throughout */
            if (vlc_stream_GetSize(task->s, &stream_size) != VLC_SUCCESS)
            {
                status = libvlc_downloader_status_error;
                goto cleanup_locked;
            }

            /* check whether the user requested pause during the blocking read. In that case,
               the data is buffered and download is paused. */
            if (task->pause_requested)
            {
                vlc_mutex_unlock(&downloader->lock);
                continue;
            }
        }

        /* deliver the residual (either just fetched or left over after a partial read) */
        const uint8_t *chunk = buf + consumed_offset;
        size_t remaining = buf_len - consumed_offset;

        ptrdiff_t consumed = task->cbs->on_buffer(task->cbs_opaque, task, chunk,
                                                  remaining, total_read, stream_size);

        if (consumed == LIBVLC_DOWNLOADER_CB_CANCEL)
            goto cleanup_locked;

        if (consumed < 0 || (size_t)consumed > remaining)
        {
            status = libvlc_downloader_status_error;
            goto cleanup_locked;
        }

        consumed_offset += (size_t)consumed;

        /* partial read: auto-pause as backpressure */
        if ((size_t)consumed < remaining)
            task->pause_requested = true;

        vlc_mutex_unlock(&downloader->lock);
    }

cleanup:
    vlc_mutex_lock(&downloader->lock);
cleanup_locked:
    libvlc_downloader_task_ReportStateLocked(task, status);
    vlc_list_remove(&task->node);
    task->thread->terminated = true;
    vlc_mutex_unlock(&downloader->lock);
    free(buf);
    libvlc_downloader_task_release(task);
    return NULL;
}

libvlc_downloader_t *
libvlc_downloader_new(libvlc_instance_t *inst, const struct libvlc_downloader_cfg *cfg)
{
    assert(inst != NULL);
    assert(cfg != NULL);

    /* No different versions to handle for now */
    assert(cfg->version <= 0);

    libvlc_downloader_t *downloader = malloc(sizeof(*downloader));
    if (downloader == NULL)
        return NULL;

    const struct libvlc_parser_cfg parser_cfg = {
        .version = 0,
        .max_parser_threads = cfg->max_parser_threads,
        .timeout = 0,
    };

    downloader->parser = libvlc_parser_new(inst, &parser_cfg);
    if (downloader->parser == NULL)
    {
        free(downloader);
        return NULL;
    }

    downloader->libvlc = libvlc_retain(inst);
    vlc_mutex_init(&downloader->lock);
    vlc_list_init(&downloader->submitted_tasks);
    vlc_list_init(&downloader->threads);
    return downloader;
}

static void notify_subitems(const struct libvlc_downloader_cbs *cbs, void *cbs_opaque,
                            libvlc_downloader_task *task)
{
    libvlc_media_list_t *mlist = libvlc_media_subitems(task->media);
    if (mlist == NULL)
        return;

    cbs->on_subitems(cbs_opaque, task, mlist);
    libvlc_media_list_release(mlist);
}

static void notify_slaves(const struct libvlc_downloader_cbs *cbs, void *cbs_opaque,
                          libvlc_downloader_task *task)
{
    libvlc_media_slave_t **slaves = NULL;
    unsigned count = libvlc_media_slaves_get(task->media, &slaves);
    if (count > 0 && slaves)
    {
        cbs->on_slaves(cbs_opaque, task, slaves, count);
        libvlc_media_slaves_release(slaves, count);
    }
}

static void parse_ended(void *opaque, libvlc_parser_task *parser_task, libvlc_parser_status_t status)
{
    (void) parser_task;
    struct libvlc_downloader_task *task = opaque;
    const struct libvlc_downloader_cbs *cbs = task->cbs;
    void *cbs_opaque = task->cbs_opaque;
    libvlc_downloader_t *downloader = task->downloader;
    libvlc_downloader_status_t downloader_status = libvlc_downloader_status_error;
    struct libvlc_downloader_thread *thread = NULL;

    if (status == libvlc_parser_status_cancelled)
    {
        downloader_status = libvlc_downloader_status_cancelled;
        goto cleanup;
    }

    if (status != libvlc_parser_status_done)
        goto cleanup;

    if (cbs->on_subitems)
        notify_subitems(cbs, cbs_opaque, task);

    if (cbs->on_slaves)
        notify_slaves(cbs, cbs_opaque, task);

    libvlc_media_type_t type = libvlc_media_get_type(task->media);
    if (type != libvlc_media_type_file)
        goto cleanup;

    thread = malloc(sizeof(*thread));
    if (thread == NULL)
        goto cleanup;

    vlc_mutex_lock(&downloader->lock);
    /* interruption check before spawning downloader thread */
    if (task->interrupted)
        goto cleanup_locked;

    thread->terminated = false;
    vlc_list_append(&thread->node, &downloader->threads);
    if (vlc_clone(&thread->thread, downloaderThread, task))
    {
        vlc_list_remove(&thread->node);
        goto cleanup_locked;
    }
    task->thread = thread;
    vlc_mutex_unlock(&downloader->lock);

    return;

cleanup:
    vlc_mutex_lock(&downloader->lock);
cleanup_locked:
    libvlc_downloader_task_ReportStateLocked(task, downloader_status);
    vlc_list_remove(&task->node);
    vlc_mutex_unlock(&downloader->lock);
    free(thread);
    libvlc_downloader_task_release(task);
}

static const struct libvlc_parser_cbs parser_cbs = {
    .version = 0,
    .on_parsed = parse_ended,
    .on_attachments_added = NULL,
};

libvlc_downloader_task *
libvlc_downloader_queue(libvlc_downloader_t *downloader, const libvlc_downloader_request_t *req,
                        const struct libvlc_downloader_cbs *cbs, void *cbs_opaque)
{
    assert(downloader != NULL);
    assert(req != NULL && req->media != NULL);
    assert(cbs != NULL && cbs->on_buffer != NULL && cbs->on_state_update != NULL);

    /* No different versions to handle for now */
    assert(req->version <= 0);
    assert(cbs->version <= 0);

    struct libvlc_downloader_task *task = DownloaderTaskNew(downloader, req->media,
                                                            cbs, cbs_opaque);

    if (task == NULL)
        return NULL;

    vlc_mutex_lock(&downloader->lock);
    vlc_list_append(&task->node, &downloader->submitted_tasks);

    /* clean up terminated threads */
    struct libvlc_downloader_thread *thread;
    vlc_list_foreach(thread, &downloader->threads, node)
    {
        if (thread->terminated)
        {
            vlc_join(thread->thread, NULL);
            vlc_list_remove(&thread->node);
            free(thread);
        }
    }
    vlc_mutex_unlock(&downloader->lock);

    libvlc_parser_request_t request = {
        .version = 0,
        .media = req->media,
        .parse_flags = 0, /* default VLC_PREPARSER_TYPE_PARSE | VLC_PREPARSER_OPTION_SUBITEMS */
    };

    vlc_atomic_rc_inc(&task->rc);

    libvlc_parser_task *parser_task = libvlc_parser_queue(downloader->parser, &request,
                                                          &parser_cbs, task);
    if (parser_task == NULL)
    {
        vlc_mutex_lock(&downloader->lock);
        vlc_list_remove(&task->node);
        vlc_mutex_unlock(&downloader->lock);
        DownloaderTaskDestroy(task);
        return NULL;
    }

    task->parser_task = parser_task;
    return task;
}

size_t libvlc_downloader_cancel(libvlc_downloader_t *downloader, libvlc_downloader_task *task)
{
    assert(downloader != NULL);
    size_t cancelled = 0;

    vlc_mutex_lock(&downloader->lock);

    struct libvlc_downloader_task *task_itr;
    vlc_list_foreach(task_itr, &downloader->submitted_tasks, node)
    {
        if (task != NULL && task_itr != task)
            continue;

        cancelled++;
        DownloaderTaskInterruptLocked(task_itr);

        /* small optimisation in the likely case where the user cancel
           only one task */
        if (task != NULL)
            break;
    }

    vlc_mutex_unlock(&downloader->lock);

    if (task != NULL)
        libvlc_parser_cancel_request(downloader->parser, task->parser_task);
    else
        libvlc_parser_cancel_request(downloader->parser, NULL);

    return cancelled;
}

void libvlc_downloader_set_pause(libvlc_downloader_t *downloader, libvlc_downloader_task *task,
                                 bool paused)
{
    assert(downloader != NULL);
    assert(task != NULL);

    vlc_mutex_lock(&downloader->lock);
    task->pause_requested = paused;
    if (!paused)
        vlc_cond_signal(&task->interrupt_cond);
    vlc_mutex_unlock(&downloader->lock);
}

void libvlc_downloader_destroy(libvlc_downloader_t *downloader)
{
    assert(downloader != NULL);
    libvlc_downloader_cancel(downloader, NULL);

    /* drain the parser. libvlc_parser_destroy() waits for all in-flight
       parse_ended callbacks to complete. Every running task was interrupted
       above, so none of the parse_ended callback can spawn a new download
       thread and the threads list is stable once this returns.
       A thread spawned just before the interruption is already in that list and
       observes the interruption at its next iteration, so joining it below returns
       promptly. */
    libvlc_parser_destroy(downloader->parser);

    struct libvlc_downloader_thread *thread;
    vlc_list_foreach(thread, &downloader->threads, node)
    {
        vlc_join(thread->thread, NULL);
        vlc_list_remove(&thread->node);
        free(thread);
    }

    /* both lists must be empty by now */
    assert(vlc_list_is_empty(&downloader->submitted_tasks));
    assert(vlc_list_is_empty(&downloader->threads));

    libvlc_release(downloader->libvlc);
    free(downloader);
}

libvlc_media_t *
libvlc_downloader_task_get_media(libvlc_downloader_task *task)
{
    assert(task != NULL);
    return task->media;
}

void libvlc_downloader_task_release(libvlc_downloader_task *task)
{
    assert(task != NULL);
    if (!vlc_atomic_rc_dec(&task->rc))
        return;

    DownloaderTaskDestroy(task);
}
