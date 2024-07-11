/*****************************************************************************
 * vlc_pipewire.c: common PipeWire code
 *****************************************************************************
 * Copyright (C) 2022 Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <pipewire/pipewire.h>
#include <vlc_common.h>
#include <vlc_tick.h>
#include "vlc_pipewire.h"

struct vlc_pw_context {
    struct pw_thread_loop *loop;
    struct pw_context *context;
    struct pw_core *core;
    struct pw_registry *registry;
    struct vlc_logger *logger;
    const char *type;
};

static
void vlc_pw_vlog(struct vlc_pw_context *ctx, int prio,
                 const char *file, unsigned int line, const char *func,
                 const char *fmt, va_list ap)
{
    vlc_vaLog(&ctx->logger, prio, ctx->type, "pipewire", file, line, func, fmt,
              ap);
}

void (vlc_pw_log)(struct vlc_pw_context *ctx, int prio,
                  const char *file, unsigned int line, const char *func,
                  const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vlc_pw_vlog(ctx, prio, file, line, func, fmt, ap);
    va_end(ap);
}

int (vlc_pw_perror)(struct vlc_pw_context *ctx, const char *file,
                    unsigned int line, const char *func, const char *desc)
{
    int err = errno;
    (vlc_pw_log)(ctx, VLC_MSG_ERR, file, line, func,
                 "PipeWire %s error: %s", desc, vlc_strerror_c(err));
    errno = err;
    return err;
}

void vlc_pw_lock(struct vlc_pw_context *ctx)
{
    pw_thread_loop_lock(ctx->loop);
}

void vlc_pw_unlock(struct vlc_pw_context *ctx)
{
    pw_thread_loop_unlock(ctx->loop);
}

void vlc_pw_signal(struct vlc_pw_context *ctx)
{
    pw_thread_loop_signal(ctx->loop, false);
}

void vlc_pw_wait(struct vlc_pw_context *ctx)
{
    pw_thread_loop_wait(ctx->loop);
}

struct pw_stream *vlc_pw_stream_new(struct vlc_pw_context *ctx,
                                    const char *name,
                                    struct pw_properties *props)
{
    return pw_stream_new(ctx->core, name, props);
}

struct vlc_pw_rt {
    struct vlc_pw_context *context;
    int seq;
    bool done;
};

static void roundtrip_done(void *data, uint32_t id, int seq)
{
    struct vlc_pw_rt *rt = data;

    if (id == PW_ID_CORE && seq == rt->seq) {
        rt->done = true;
        vlc_pw_signal(rt->context);
    }
}

void vlc_pw_roundtrip_unlocked(struct vlc_pw_context *ctx)
{
    static const struct pw_core_events events = {
        PW_VERSION_CORE_EVENTS,
        .done = roundtrip_done,
    };
    struct spa_hook listener = { };
    struct vlc_pw_rt rt;

    rt.context = ctx;
    rt.done = false;

    pw_core_add_listener(ctx->core, &listener, &events, &rt);
    rt.seq = pw_core_sync(ctx->core, PW_ID_CORE, 0);
    while (!rt.done)
        vlc_pw_wait(ctx);
    spa_hook_remove(&listener);
}

int vlc_pw_registry_listen(struct vlc_pw_context *ctx, struct spa_hook *hook,
                           const struct pw_registry_events *evts, void *data)
{
    if (ctx->registry == NULL) {
        ctx->registry = pw_core_get_registry(ctx->core, PW_VERSION_REGISTRY,
                                             0);
        if (unlikely(ctx->registry == NULL))
            return -errno;
    }

    *hook = (struct spa_hook){ };
    pw_registry_add_listener(ctx->registry, hook, evts, data);
    return 0;
}

void vlc_pw_disconnect(struct vlc_pw_context *ctx)
{
    pw_thread_loop_stop(ctx->loop);

    if (ctx->registry != NULL)
        pw_proxy_destroy((struct pw_proxy *)ctx->registry);

    pw_core_disconnect(ctx->core);
    pw_context_destroy(ctx->context);
    pw_thread_loop_destroy(ctx->loop);
    pw_deinit();
    free(ctx);
}

struct vlc_pw_context *vlc_pw_connect(vlc_object_t *obj, const char *name)
{
    struct vlc_logger *logger = obj->logger;
    const char *version = pw_get_library_version();
    int err;

    vlc_debug(logger, "using PipeWire run-time v%s (built v%s)", version,
              pw_get_headers_version());

    /* Safe use of pw_deinit() requires 0.3.49 */
    if (strverscmp(version, "0.3.49") < 0) {
        vlc_error(logger, "PipeWire version %s required, %s detected",
                  "0.3.49", version);
        errno = ENOSYS;
        return NULL;
    }

    struct vlc_pw_context *ctx = malloc(sizeof (*ctx));
    if (unlikely(ctx == NULL))
        return NULL;

    pw_init(NULL, NULL);

    ctx->logger = logger;
    ctx->type = name;
    ctx->loop = pw_thread_loop_new(name, NULL);
    ctx->registry = NULL;

    if (likely(ctx->loop != NULL)) {
        ctx->context = pw_context_new(pw_thread_loop_get_loop(ctx->loop),
                                      NULL, 0);

        if (likely(ctx->context != NULL)) {
            ctx->core = pw_context_connect(ctx->context, NULL, 0);

            if (ctx->core != NULL) {
                if (likely(pw_thread_loop_start(ctx->loop) == 0))
                    return ctx;

                err = errno;
                pw_core_disconnect(ctx->core);
            } else {
                err = errno;
                vlc_pw_perror(ctx, "context connection");
            }

            pw_context_destroy(ctx->context);
        } else
            err = errno;
        pw_thread_loop_destroy(ctx->loop);
    } else
        err = errno;

    pw_deinit();
    errno = err;
    free(ctx);
    return NULL;
}
