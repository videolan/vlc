/*****************************************************************************
 * downloader.c:  libvlc downloader API sample usage
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

/*
 * Program usage:
 *   Usage: <total number of media to download> <MRLtodownload1> [MRLtodownload2]...
 *
 * Example:
 *   ./doc/samples_libvlc_downloader 2 https://example.org/a.mp4 file:///home/user/b.mp3
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <semaphore.h>
#include <inttypes.h>

#include <vlc/vlc.h>
#include <vlc/libvlc_downloader.h>

struct request_ctx
{
    int index;
    sem_t *done_sem;
};

static const char *dl_status_to_string(libvlc_downloader_status_t st)
{
    switch (st)
    {
        case libvlc_downloader_status_pending:   return "pending";
        case libvlc_downloader_status_running:   return "running";
        case libvlc_downloader_status_paused:    return "paused";
        case libvlc_downloader_status_finished:  return "finished";
        case libvlc_downloader_status_cancelled: return "cancelled";
        case libvlc_downloader_status_error:     return "error";
        default: return "unknown";
    }
}

static ptrdiff_t on_buffer(void *opaque, libvlc_downloader_task *task, const uint8_t *buf,
                           size_t len, uint64_t position, uint64_t total)
{
    (void)buf;
    struct request_ctx *ctx = (struct request_ctx *)opaque;
    /* Buffer received for media. User can process or save 'buf' here. */
    fprintf(stdout, "[req %d] received buffer: %zu bytes\n", ctx->index, len);
    double pct = (total > 0) ? (100.0 * (double)position / (double)total) : 0.0;
    fprintf(stdout, "[req %d, id=%p] progress: %" PRIu64 "/%" PRIu64 " (%.1f%%)\n",
            ctx->index, task, position, total, pct);
    fflush(stdout);
    return (ptrdiff_t)len;
}

static void on_state_update(void *opaque, libvlc_downloader_task *task,
                            libvlc_downloader_status_t status)
{
    struct request_ctx *ctx = (struct request_ctx *)opaque;
    fprintf(stdout, "[req %d, id=%p] state: %s\n",
            ctx->index, task, dl_status_to_string(status));
    fflush(stdout);

    if (status == libvlc_downloader_status_finished ||
        status == libvlc_downloader_status_cancelled ||
        status == libvlc_downloader_status_error)
    {
        sem_post(ctx->done_sem);
        libvlc_downloader_task_release(task);
    }
}

static void on_subitems(void *opaque, libvlc_downloader_task *task, libvlc_media_list_t *subitems)
{
    (void)task;
    struct request_ctx *ctx = (struct request_ctx *)opaque;
    int count = libvlc_media_list_count(subitems);
    fprintf(stdout, "[req %d] media has %d subitems (download will not proceed for playlists/directories)\n",
            ctx->index, count);
    fflush(stdout);
}

static void on_slaves(void *opaque, libvlc_downloader_task *task, libvlc_media_slave_t **slaves, size_t count)
{
    (void)task;
    struct request_ctx *ctx = (struct request_ctx *)opaque;
    fprintf(stdout, "[req %d] media has %zu slave(s)\n", ctx->index, count);
    (void)slaves;
    fflush(stdout);
}

static libvlc_downloader_t *create_downloader(const struct libvlc_downloader_cfg *cfg)
{
    static const char* const argv[] = {
        "-vvv",
        "--vout=dummy",
        "--aout=dummy",
        "--text-renderer=dummy",
    };
    libvlc_instance_t *inst = libvlc_new((int)(sizeof argv / sizeof *argv), argv);
    if (inst == NULL)
        return NULL;

    libvlc_downloader_t *downloader = libvlc_downloader_new(inst, cfg);
    /* downloader retains the instance, we can release our reference */
    libvlc_release(inst);
    return downloader;
}

static void print_usage(const char *name)
{
    fprintf(stderr,
            "Usage: %s <total number of media to download> <mediatodownload1> [mediatodownload2]...\n",
            name);
}

int main(int argc, const char **argv)
{
    setlocale(LC_ALL, "");

    if (argc < 2)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    char *endp = NULL;
    long nlong = strtol(argv[1], &endp, 10);
    if (endp == argv[1] || nlong <= 0)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    int total = (int)nlong;

    /* expect: prog N media1 ... mediaN */
    if (argc != 2 + total)
    {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char **media_uris = &argv[2];

    const struct libvlc_downloader_cfg cfg = {
        .version = 0,
        .max_parser_threads = 1,
    };
    libvlc_downloader_t *downloader = create_downloader(&cfg);
    if (downloader == NULL)
    {
        fprintf(stderr, "Failed to create downloader\n");
        return EXIT_FAILURE;
    }

    static const struct libvlc_downloader_cbs cbs = {
        .version = 0,
        .on_state_update = on_state_update,
        .on_buffer = on_buffer,
        .on_subitems = on_subitems,
        .on_slaves = on_slaves,
    };

    sem_t done_sem;
    sem_init(&done_sem, 0, 0);

    struct request_ctx *ctxs = calloc((size_t)total, sizeof(*ctxs));
    if (ctxs == NULL)
    {
        fprintf(stderr, "Failed to create context\n");
        libvlc_downloader_destroy(downloader);
        sem_destroy(&done_sem);
        return EXIT_FAILURE;
    }

    int queued = 0;

    for (int i = 0; i < total; ++i)
    {
        const char *url = media_uris[i];
        libvlc_media_t *media = NULL;
        if (strstr(url, "://") != NULL)
            media = libvlc_media_new_location(url);
        else
            media = libvlc_media_new_path(url);

        if (media == NULL)
        {
            fprintf(stderr, "[req %d] failed to create media for '%s'\n", i, url);
            continue;
        }

        ctxs[i].index = i;
        ctxs[i].done_sem = &done_sem;

        const libvlc_downloader_request_t req = {
            .version = 0,
            .media = media,
        };

        libvlc_downloader_task *task = libvlc_downloader_queue(downloader, &req, &cbs, &ctxs[i]);

        libvlc_media_release(media);

        if (task == NULL)
        {
            fprintf(stderr, "[req %d] failed to queue download for '%s'\n", i, url);
            continue;
        }

        fprintf(stdout, "[req %d, id=%p] queued: %s\n",
                i, task, url);
        fflush(stdout);
        queued++;
    }

    for (int i = 0; i < queued; ++i)
        sem_wait(&done_sem);

    fprintf(stdout, "All %d queued download(s) completed (finished/cancelled/error).\n", queued);
    fflush(stdout);

    libvlc_downloader_destroy(downloader);
    sem_destroy(&done_sem);
    free(ctxs);

    return queued > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
