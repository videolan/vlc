/*****************************************************************************
 * downloader.c: test for the LibVLC Downloader API
 *****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_downloader
#undef VLC_DYNAMIC_PLUGIN

#include "./test.h"
#include <vlc_common.h>
#include <vlc_demux.h>
#include <vlc_plugin.h>
#include <vlc_stream.h>

#include <vlc/libvlc.h>

#include <limits.h>

const char vlc_module_name[] = MODULE_STRING;

#define TEST_TOTAL_READS 20
#define TEST_BUFFER_SIZE 65536
#define TEST_TOTAL_BYTES ((uint64_t)TEST_TOTAL_READS * (uint64_t)TEST_BUFFER_SIZE)

struct test_ctx_t
{
    vlc_sem_t terminated_sem;
    vlc_sem_t progress_sem;
    vlc_sem_t paused_sem;
    int state_counts[libvlc_downloader_status_error + 1];
    int buffer_cb_calls;
    uint64_t total_bytes;
    bool buffer_data_ok;
};

/* Dummy access module read counter */
struct vlctest_access_sys_t
{
    int read_counter;
};

/* Dummy Read callback: fills buffer with 0xAB until EOF (simulated) */
static ssize_t Read(stream_t *access, void *buf, size_t len)
{
    struct vlctest_access_sys_t *p_sys = access->p_sys;
    assert(len <= SSIZE_MAX);
    if (p_sys->read_counter >= TEST_TOTAL_READS)
        return 0; /* EOF */

    /* simulate some delay in reading */
    vlc_tick_sleep(VLC_TICK_FROM_MS(10));
    memset(buf, 0xAB, len);
    p_sys->read_counter++;
    return len;
}

static int Control(stream_t *access, int query, va_list args)
{
    (void)access;
    switch (query)
    {
        case STREAM_CAN_DOWNLOAD:
        {
            bool *pb_bool = va_arg(args, bool *);
            *pb_bool = true;
            break;
        }
        case STREAM_GET_SIZE:
        {
            uint64_t *p_size = va_arg(args, uint64_t *);
            *p_size = TEST_TOTAL_BYTES;
            break;
        }
        default:
            return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}

static void AccessClose(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    free(access->p_sys);
}

static int AccessOpen(vlc_object_t *obj)
{
    stream_t *access = (stream_t *)obj;
    struct vlctest_access_sys_t *p_sys = malloc(sizeof(*p_sys));
    if (p_sys == NULL)
        return VLC_ENOMEM;

    p_sys->read_counter = 0;
    access->p_sys = p_sys;
    access->pf_read = Read;
    access->pf_control = Control;
    access->pf_seek = NULL;
    return VLC_SUCCESS;
}

/* Dummy demux that opens any vlctest_:// stream and immediately reports EOF.
   Selected only when explicitly forced via ":demux=vlctest_" on the media,
   so the preparser succeeds without needing real demuxer probing. */
static int DemuxDemux(demux_t *demux)
{
    (void)demux;
    return VLC_DEMUXER_EOF;
}

static int DemuxControl(demux_t *demux, int query, va_list args)
{
    (void)demux;
    (void)query;
    (void)args;
    return VLC_EGENERIC;
}

static int DemuxOpen(vlc_object_t *obj)
{
    demux_t *demux = (demux_t *)obj;
    demux->pf_demux = DemuxDemux;
    demux->pf_control = DemuxControl;
    return VLC_SUCCESS;
}

static void DemuxClose(vlc_object_t *obj)
{
    (void)obj;
}

vlc_module_begin()
    set_capability("access", 0)
    add_shortcut("vlctest_")
    set_callbacks(AccessOpen, AccessClose)

    add_submodule()
        set_capability("demux", 0)
        add_shortcut("vlctest_")
        set_callbacks(DemuxOpen, DemuxClose)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

static ptrdiff_t on_buffer(void *opaque, libvlc_downloader_task *task, const uint8_t *buf,
                           size_t len, uint64_t position, uint64_t total)
{
    (void)task;
    (void)total;
    assert(len <= SSIZE_MAX);
    struct test_ctx_t *ctx = opaque;
    ctx->buffer_cb_calls++;
    ctx->total_bytes += len;
    /* check that all bytes are 0xAB as per Read() */
    for (size_t i = 0; i < len; i++)
    {
        if (buf[i] != 0xAB)
            ctx->buffer_data_ok = false;
    }
    /* progress should match total bytes downloaded so far */
    assert(position == ctx->total_bytes);

    /* notify main thread for synchronization */
    vlc_sem_post(&ctx->progress_sem);
    return (ptrdiff_t)len;
}

static void on_state_update(void *opaque, libvlc_downloader_task *task, libvlc_downloader_status_t status)
{
    struct test_ctx_t *ctx = opaque;
    ctx->state_counts[status]++;
    if (status == libvlc_downloader_status_paused)
        vlc_sem_post(&ctx->paused_sem);
    if (status == libvlc_downloader_status_finished ||
        status == libvlc_downloader_status_cancelled ||
        status == libvlc_downloader_status_error)
    {
        vlc_sem_post(&ctx->terminated_sem);
        libvlc_downloader_task_release(task);
    }
}

static void reset_ctx(struct test_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->buffer_data_ok = true;
    vlc_sem_init(&ctx->terminated_sem, 0);
    vlc_sem_init(&ctx->progress_sem, 0);
    vlc_sem_init(&ctx->paused_sem, 0);
}

static const struct libvlc_downloader_cbs cbs = {
    .version = 0,
    .on_buffer = on_buffer,
    .on_state_update = on_state_update,
    .on_subitems = NULL,
    .on_slaves = NULL,
};

static void test_basic_download(libvlc_instance_t *vlc)
{
    fprintf(stderr, "test: 1/ checking basic download of two medias with one downloader\n");
    struct test_ctx_t ctx1, ctx2;
    reset_ctx(&ctx1);
    reset_ctx(&ctx2);

    const struct libvlc_downloader_cfg cfg = {
        .version = 0,
        .max_parser_threads = 1,
    };
    libvlc_downloader_t *downloader = libvlc_downloader_new(vlc, &cfg);
    assert(downloader);

    libvlc_media_t *media1 = libvlc_media_new_location("vlctest_://dummyone");
    libvlc_media_add_option(media1, ":demux=vlctest_");
    libvlc_media_t *media2 = libvlc_media_new_location("vlctest_://dummytwo");
    libvlc_media_add_option(media2, ":demux=vlctest_");
    assert(media1 && media2);

    const libvlc_downloader_request_t req1 = {
        .version = 0,
        .media = media1,
    };
    const libvlc_downloader_request_t req2 = {
        .version = 0,
        .media = media2,
    };

    libvlc_downloader_task *task1 = libvlc_downloader_queue(downloader, &req1, &cbs, &ctx1);
    libvlc_downloader_task *task2 = libvlc_downloader_queue(downloader, &req2, &cbs, &ctx2);

    assert(task1 != NULL);
    assert(task2 != NULL);
    assert(task1 != task2);

    /* wait for both downloads to reach a terminal state */
    vlc_sem_wait(&ctx1.terminated_sem);
    vlc_sem_wait(&ctx2.terminated_sem);

    /* check state transitions for both */
    assert(ctx1.state_counts[libvlc_downloader_status_running] > 0);
    assert(ctx1.state_counts[libvlc_downloader_status_finished] == 1);
    assert(ctx2.state_counts[libvlc_downloader_status_running] > 0);
    assert(ctx2.state_counts[libvlc_downloader_status_finished] == 1);

    /* check buffer and progress */
    assert(ctx1.buffer_cb_calls > 0);
    assert(ctx1.buffer_data_ok);
    assert(ctx2.buffer_cb_calls > 0);
    assert(ctx2.buffer_data_ok);

    /* check total bytes per download */
    assert(ctx1.total_bytes == TEST_TOTAL_BYTES);
    assert(ctx2.total_bytes == TEST_TOTAL_BYTES);

    libvlc_downloader_destroy(downloader);
    libvlc_media_release(media1);
    libvlc_media_release(media2);
}

static void test_pause_resume(libvlc_instance_t *vlc)
{
    fprintf(stderr, "test: 2/ checking pause and resume\n");
    struct test_ctx_t ctx;
    reset_ctx(&ctx);

    const struct libvlc_downloader_cfg cfg = {
        .version = 0,
        .max_parser_threads = 1,
    };
    libvlc_downloader_t *downloader = libvlc_downloader_new(vlc, &cfg);
    assert(downloader);

    libvlc_media_t *media = libvlc_media_new_location("vlctest_://dummy");
    assert(media);
    libvlc_media_add_option(media, ":demux=vlctest_");

    const libvlc_downloader_request_t req = {
        .version = 0,
        .media = media,
    };

    libvlc_downloader_task *task = libvlc_downloader_queue(downloader, &req, &cbs, &ctx);
    assert(task != NULL);

    /* wait for a couple of buffer callbacks before pausing */
    for (int i = 0; i < 2; ++i)
        vlc_sem_wait(&ctx.progress_sem);
    int progress_before_pause = ctx.buffer_cb_calls;

    libvlc_downloader_set_pause(downloader, task, true);

    /* wait until paused state is reported */
    vlc_sem_wait(&ctx.paused_sem);
    assert(ctx.state_counts[libvlc_downloader_status_paused] >= 1);

    /* resume and wait for terminal state */
    libvlc_downloader_set_pause(downloader, task, false);
    vlc_sem_wait(&ctx.terminated_sem);

    assert(ctx.state_counts[libvlc_downloader_status_running] >= 1);
    assert(ctx.state_counts[libvlc_downloader_status_finished] == 1);

    /* ensure progress increased after resume */
    assert(ctx.buffer_cb_calls > progress_before_pause);

    /* buffer integrity and total size checks */
    assert(ctx.buffer_data_ok);
    assert(ctx.total_bytes == TEST_TOTAL_BYTES);

    libvlc_downloader_destroy(downloader);
    libvlc_media_release(media);
}

static void test_cancel_download(libvlc_instance_t *vlc)
{
    fprintf(stderr, "test: 3/ checking download cancellation\n");
    struct test_ctx_t ctx;
    reset_ctx(&ctx);

    const struct libvlc_downloader_cfg cfg = {
        .version = 0,
        .max_parser_threads = 1,
    };
    libvlc_downloader_t *downloader = libvlc_downloader_new(vlc, &cfg);
    assert(downloader);

    libvlc_media_t *media = libvlc_media_new_location("vlctest_://dummy");
    assert(media);
    libvlc_media_add_option(media, ":demux=vlctest_");

    const libvlc_downloader_request_t req = {
        .version = 0,
        .media = media,
    };

    libvlc_downloader_task *task = libvlc_downloader_queue(downloader, &req, &cbs, &ctx);
    assert(task != NULL);

    int progress_for_cancel = 2; /* cancel after 2 calls of buffer callback */

    /* wait for the desired progress count, then cancel from main thread */
    for (int i = 0; i < progress_for_cancel; ++i)
        vlc_sem_wait(&ctx.progress_sem);

    size_t cancelled = libvlc_downloader_cancel(downloader, task);
    assert(cancelled == 1);

    vlc_sem_wait(&ctx.terminated_sem);

    assert(ctx.state_counts[libvlc_downloader_status_cancelled] == 1);

    libvlc_downloader_destroy(downloader);
    libvlc_media_release(media);
}

/* context for partial-read */
struct partial_ctx_t
{
    vlc_sem_t terminated_sem; /* signal download termination */
    vlc_sem_t first_buffer_sem; /* signal first buffer callback */
    vlc_sem_t paused_sem; /* signal paused state */
    int state_counts[libvlc_downloader_status_error + 1]; /* count of each state reported */
    int buffer_cb_calls; /* count of buffer callback calls */
    size_t first_len; /* length of the buffer in the first on_buffer callback invocation */
    size_t second_len; /* length of the buffer in the second on_buffer callback invocation (residual) */
};

static void partial_ctx_init(struct partial_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    vlc_sem_init(&ctx->terminated_sem, 0);
    vlc_sem_init(&ctx->first_buffer_sem, 0);
    vlc_sem_init(&ctx->paused_sem, 0);
}

static void partial_on_state(void *opaque, libvlc_downloader_task *task,
                             libvlc_downloader_status_t status)
{
    struct partial_ctx_t *ctx = opaque;
    ctx->state_counts[status]++;
    if (status == libvlc_downloader_status_paused)
        vlc_sem_post(&ctx->paused_sem);
    if (status == libvlc_downloader_status_finished ||
        status == libvlc_downloader_status_cancelled ||
        status == libvlc_downloader_status_error)
    {
        vlc_sem_post(&ctx->terminated_sem);
        libvlc_downloader_task_release(task);
    }
}

static ptrdiff_t partial_on_buffer(void *opaque, libvlc_downloader_task *task,
                                   const uint8_t *buf, size_t len,
                                   uint64_t position, uint64_t total)
{
    (void)task; (void)total; (void)buf; (void)position;
    struct partial_ctx_t *ctx = opaque;
    ctx->buffer_cb_calls++;

    if (ctx->buffer_cb_calls == 1)
    {
        /* accept half to trigger auto-pause as backpressure */
        ctx->first_len = len;
        vlc_sem_post(&ctx->first_buffer_sem);
        return (ptrdiff_t)(len / 2);
    }

    if (ctx->buffer_cb_calls == 2)
        ctx->second_len = len;

    return (ptrdiff_t)len;
}

static void test_partial_read(libvlc_instance_t *vlc)
{
    fprintf(stderr, "test: 4/ checking partial read triggers pause and redelivers residual\n");
    struct partial_ctx_t ctx;
    partial_ctx_init(&ctx);

    const struct libvlc_downloader_cfg cfg = {
        .version = 0,
        .max_parser_threads = 1,
    };
    libvlc_downloader_t *downloader = libvlc_downloader_new(vlc, &cfg);
    assert(downloader);

    static const struct libvlc_downloader_cbs cbs = {
        .version = 0,
        .on_buffer = partial_on_buffer,
        .on_state_update = partial_on_state,
        .on_subitems = NULL,
        .on_slaves = NULL,
    };

    libvlc_media_t *media = libvlc_media_new_location("vlctest_://partial");
    assert(media);
    libvlc_media_add_option(media, ":demux=vlctest_");

    const libvlc_downloader_request_t req = {
        .version = 0,
        .media = media,
    };

    libvlc_downloader_task *task = libvlc_downloader_queue(downloader, &req, &cbs, &ctx);
    assert(task != NULL);

    /* wait for the first (partial-accept) callback, then for auto-pause */
    vlc_sem_wait(&ctx.first_buffer_sem);
    vlc_sem_wait(&ctx.paused_sem);

    assert(ctx.state_counts[libvlc_downloader_status_paused] >= 1);
    /* no further on_buffer call should have happened while paused */
    assert(ctx.buffer_cb_calls == 1);

    /* resume, next on_buffer must deliver the residual of the same chunk */
    libvlc_downloader_set_pause(downloader, task, false);

    vlc_sem_wait(&ctx.terminated_sem);

    assert(ctx.state_counts[libvlc_downloader_status_finished] == 1);

    /* check that the second buffer contains the expected residual */
    size_t expected_residual = ctx.first_len - (ctx.first_len / 2);
    assert(ctx.second_len == expected_residual);

    libvlc_downloader_destroy(downloader);
    libvlc_media_release(media);
}

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    test_init();

    const char * const vlc_argv[] = {
        "-vvv", "--vout=dummy", "--aout=dummy", "--text-renderer=dummy"
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(vlc_argv), vlc_argv);
    assert(vlc);

    test_basic_download(vlc);
    test_pause_resume(vlc);
    test_cancel_download(vlc);
    test_partial_read(vlc);

    libvlc_release(vlc);
    return 0;
}
