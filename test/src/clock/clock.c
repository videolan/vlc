/*****************************************************************************
 * clock/clock.c: test for the vlc clock
 *****************************************************************************
 * Copyright (C) 2023 VLC authors, VideoLAN and Videolabs SAS
 * Copyright (C) 2024 Videolabs
 *
 * Authors: Thomas Guillem <thomas@gllm.fr>
 *          Alexandre Janniaux <ajanni@videolabs.io>
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
#include <vlc_tick.h>
#include <vlc_es.h>
#include <vlc_tracer.h>

#include "../../../src/clock/clock.h"

#include <vlc/vlc.h>
#include "../../libvlc/test.h"
#include "../../../lib/libvlc_internal.h"

#define MODULE_NAME test_clock_clock
#undef VLC_DYNAMIC_PLUGIN
#include <vlc_plugin.h>
#include <vlc_vector.h>

/* Define a builtin module for mocked parts */
const char vlc_module_name[] = MODULE_STRING;

struct clock_ctx;

struct clock_scenario
{
    const char *name;
    const char *desc;

    enum {
        CLOCK_SCENARIO_UPDATE,
        CLOCK_SCENARIO_RUN,
    } type;

    union {
        struct {
            vlc_tick_t stream_start;
            vlc_tick_t system_start; /* VLC_TICK_INVALID for vlc_tick_now() */
            vlc_tick_t duration;
            vlc_tick_t stream_increment; /* VLC_TICK_INVALID for manual increment */
            unsigned video_fps;
            vlc_tick_t total_drift_duration; /* VLC_TICK_INVALID for non-drift test */

            double coeff_epsilon; /* Valid for lowprecision/burst checks */
            void (*update)(const struct clock_ctx *ctx, size_t index,
                           vlc_tick_t *system, vlc_tick_t stream);
            void (*check)(const struct clock_ctx *ctx, size_t update_count,
                          vlc_tick_t expected_system_end, vlc_tick_t stream_end);
        };
        struct {
            void (*run)(const struct clock_ctx *ctx);
            bool disable_jitter;
        };
    };
};

struct clock_ctx
{
    vlc_clock_main_t *mainclk;
    vlc_clock_t *master;
    vlc_clock_t *slave;

    vlc_tick_t system_start;
    vlc_tick_t stream_start;

    const struct clock_scenario *scenario;
};

enum tracer_event_type {
    TRACER_EVENT_TYPE_UPDATE,
    TRACER_EVENT_TYPE_RENDER_VIDEO,
    TRACER_EVENT_TYPE_STATUS,
};

enum tracer_event_status  {
    TRACER_EVENT_STATUS_RESET_USER,
    TRACER_EVENT_STATUS_RESET_BADSOURCE,
};

struct tracer_event
{
    enum tracer_event_type type;

    union {
        struct {
            double coeff;
            vlc_tick_t offset;
        } update;

        struct {
            vlc_tick_t play_date;
            vlc_tick_t pts;
        } render_video;

        enum tracer_event_status status;
    };
};

struct tracer_ctx
{
    vlc_tick_t forced_ts;

    struct VLC_VECTOR(struct tracer_event) events;
};

static struct tracer_ctx tracer_ctx;

static void tracer_ctx_Reset(struct tracer_ctx *ctx)
{
    ctx->events.size = 0;
    ctx->forced_ts = VLC_TICK_INVALID;
}

static void tracer_ctx_Init(struct tracer_ctx *ctx)
{
    tracer_ctx_Reset(ctx);
    vlc_vector_init(&ctx->events);
};

static void tracer_ctx_Destroy(struct tracer_ctx *ctx)
{
    vlc_vector_destroy(&ctx->events);
}

static void tracer_ctx_PushUpdate(struct tracer_ctx *ctx,
                                  double coeff, vlc_tick_t offset)
{
    struct tracer_event event = {
        .type = TRACER_EVENT_TYPE_UPDATE,
        .update = {
            .coeff = coeff,
            .offset = offset,
        },
    };
    bool ret = vlc_vector_push(&ctx->events, event);
    assert(ret);
}

static void tracer_ctx_PushRenderVideo(struct tracer_ctx *ctx,
                                       vlc_tick_t play_date, vlc_tick_t pts)
{
    struct tracer_event event = {
        .type = TRACER_EVENT_TYPE_RENDER_VIDEO,
        .render_video = {
            .play_date = play_date,
            .pts = pts,
        },
    };
    bool ret = vlc_vector_push(&ctx->events, event);
    assert(ret);
}

static void tracer_ctx_PushStatus(struct tracer_ctx *ctx,
                                  enum tracer_event_status status)
{
    struct tracer_event event = {
        .type = TRACER_EVENT_TYPE_STATUS,
        .status = status,
    };
    bool ret = vlc_vector_push(&ctx->events, event);
    assert(ret);
}

static void TracerTrace(void *opaque, vlc_tick_t ts,
                        const struct vlc_tracer_trace *trace)
{
    struct vlc_tracer *libvlc_tracer = opaque;

    const struct vlc_tracer_entry *entry = trace->entries;

    bool is_render = false, is_render_video = false, is_status = false;
    unsigned nb_update = 0;
    double coeff = 0.0f;
    vlc_tick_t offset = VLC_TICK_INVALID;
    vlc_tick_t render_video_pts = VLC_TICK_INVALID;
    enum tracer_event_status status = 0;

    while (entry->key != NULL)
    {
        switch (entry->type)
        {
            case VLC_TRACER_INT:
                if (!is_render)
                    continue;
                assert(!is_status);

                if (strcmp(entry->key, "offset") == 0)
                {
                    nb_update++;
                    offset = VLC_TICK_FROM_NS(entry->value.integer);
                }
                else if (strcmp(entry->key, "render_pts") == 0)
                    render_video_pts = VLC_TICK_FROM_NS(entry->value.integer);
                break;
            case VLC_TRACER_DOUBLE:
                if (!is_render)
                    continue;
                assert(!is_status);

                if (strcmp(entry->key, "coeff") == 0)
                {
                    nb_update++;
                    coeff = entry->value.double_;
                }
                break;
            case VLC_TRACER_STRING:
                if (strcmp(entry->key, "type") == 0)
                {
                    if (strcmp(entry->value.string, "RENDER") == 0)
                        is_render = true;
                }
                else if (strcmp(entry->key, "id") == 0)
                {
                    if (strcmp(entry->value.string, "video") == 0)
                        is_render_video= true;
                }

                if (!is_render)
                    continue;
                /* Assert that there is no "reset_bad_source" */
                if (strcmp(entry->key, "event") == 0)
                {
                    is_status = true;
                    if (strcmp(entry->value.string, "reset_bad_source") == 0)
                        status = TRACER_EVENT_STATUS_RESET_BADSOURCE;
                    else if (strcmp(entry->value.string, "reset_user") == 0)
                        status = TRACER_EVENT_STATUS_RESET_USER;
                    else
                        vlc_assert_unreachable();
                }

                break;
            default: vlc_assert_unreachable();
        }
        entry++;
    }

    if (libvlc_tracer != NULL)
    {
        /* If the user specified a tracer, forward directly to it after faking
         * the system ts */
        assert(tracer_ctx.forced_ts != VLC_TICK_INVALID);

        /* Use the original ts for the "render_pts" entry */
        vlc_tick_t tracer_ts = render_video_pts != VLC_TICK_INVALID ?
                               ts : tracer_ctx.forced_ts;
        (vlc_tracer_TraceWithTs)(libvlc_tracer, tracer_ts, trace);
    }

    if (!is_render)
        return;

    if (is_status)
        tracer_ctx_PushStatus(&tracer_ctx, status);
    else if (nb_update > 0)
    {
        assert(nb_update == 2);
        tracer_ctx_PushUpdate(&tracer_ctx, coeff, offset);
    }
    else if (is_render_video && render_video_pts != VLC_TICK_INVALID)
        tracer_ctx_PushRenderVideo(&tracer_ctx, ts, render_video_pts);
}

/* Used to check for some trace value and hack the ts to the user tracer */
static const struct vlc_tracer_operations *
OpenTracer(vlc_object_t *obj, void **restrict sysp)
{
    static const struct vlc_tracer_operations ops =
    {
        .trace = TracerTrace,
    };

    *sysp = vlc_object_get_tracer(obj);

    return &ops;
}

vlc_module_begin()
    set_callback(OpenTracer)
    set_capability("tracer", 0)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

static void play_scenario(libvlc_int_t *vlc, struct vlc_tracer *tracer,
                          struct clock_scenario *scenario)
{
    fprintf(stderr, "[%s]: checking that %s\n", scenario->name, scenario->desc);

    assert((scenario->type == CLOCK_SCENARIO_UPDATE && scenario->update != NULL)
        || (scenario->type == CLOCK_SCENARIO_RUN && scenario->run != NULL));

    tracer_ctx_Reset(&tracer_ctx);

    struct vlc_logger *logger = vlc->obj.logger;

    vlc_clock_main_t *mainclk = vlc_clock_main_New(logger, tracer);
    assert(mainclk != NULL);

    vlc_clock_main_Lock(mainclk);
    vlc_clock_t *master = vlc_clock_main_CreateMaster(mainclk, scenario->name,
                                                      NULL, NULL);
    assert(master != NULL);

    char *slave_name;
    int ret = asprintf(&slave_name, "%s/video", scenario->name);
    assert(ret != -1);
    vlc_clock_t *slave = vlc_clock_main_CreateSlave(mainclk, slave_name, VIDEO_ES,
                                                    NULL, NULL);
    assert(slave != NULL);
    if (scenario->type == CLOCK_SCENARIO_RUN && scenario->disable_jitter)
    {
        /* Don't add any delay for the first monotonic ref point  */
        vlc_clock_main_SetInputDejitter(mainclk, 0);
        vlc_clock_main_SetDejitter(mainclk, 0);
    }

    vlc_tick_t system_start = VLC_TICK_0 + VLC_TICK_FROM_MS(15000);
    vlc_tick_t stream_start = VLC_TICK_0 + VLC_TICK_FROM_MS(15000);
    if (scenario->type == CLOCK_SCENARIO_UPDATE)
    {
        system_start = scenario->system_start;
        stream_start = scenario->stream_start;
    }
    vlc_clock_main_Unlock(mainclk);

    const struct clock_ctx ctx = {
        .mainclk = mainclk,
        .master = master,
        .slave = slave,
        .scenario = scenario,
        .system_start = system_start,
        .stream_start = stream_start,
    };

    if (scenario->type == CLOCK_SCENARIO_RUN)
    {
        scenario->run(&ctx);
        goto end;
    }

    vlc_tick_t stream_end = scenario->stream_start + scenario->duration;
    vlc_tick_t stream = scenario->stream_start;
    if (scenario->system_start == VLC_TICK_INVALID)
        scenario->system_start = vlc_tick_now();
    vlc_tick_t system = scenario->system_start;
    vlc_tick_t expected_system = scenario->system_start;

    vlc_tick_t video_increment = 0;
    vlc_tick_t video_ts = scenario->stream_start;
    if (scenario->video_fps != 0)
        video_increment = vlc_tick_rate_duration(scenario->video_fps);

    tracer_ctx.forced_ts = expected_system;
    size_t index = 0;

    for(; stream < stream_end;
        stream += scenario->stream_increment, ++index)
    {
        scenario->update(&ctx, index, &system, stream);

        vlc_tick_t video_system = expected_system;

        if (video_increment > 0)
        {
            while (video_system < expected_system + scenario->stream_increment)
            {
                vlc_clock_Lock(ctx.slave);
                vlc_tick_t play_date =
                    vlc_clock_ConvertToSystem(ctx.slave, video_system, video_ts,
                                              1.0f);
                vlc_clock_Update(ctx.slave, play_date, video_ts, 1.0f);
                vlc_clock_Unlock(ctx.slave);
                video_system += video_increment;
                video_ts += video_increment;
            }
        }

        expected_system += scenario->stream_increment;
        tracer_ctx.forced_ts = expected_system;
    }

    if (scenario->check != NULL)
    {
        assert(expected_system == scenario->system_start + scenario->duration);

        scenario->check(&ctx, index, expected_system, stream_end);
    }

end:
    vlc_clock_Delete(master);
    vlc_clock_Delete(slave);
    free(slave_name);
    vlc_clock_main_Delete(mainclk);
}

static void run_scenarios(int argc, const char *argv[],
                          struct clock_scenario *scenarios, size_t count)
{
    /* Skip argv[0] */
    argc--;
    argv++;

    const char *scenario_name = NULL;
    vlc_tick_t forced_duration = VLC_TICK_INVALID;
    if (argc > 0)
    {
        /* specific test run from the user with custom options */
        scenario_name = argv[0];
        argc--;
        argv++;

        if (argc > 0)
        {
            long long val = atoll(argv[0]);
            if (val > 0)
            {
                forced_duration = val;
                argc--;
                argv++;
            }
        }
    }

    libvlc_instance_t *vlc = libvlc_new(argc, argv);
    assert(vlc != NULL);

    tracer_ctx_Init(&tracer_ctx);

    struct vlc_tracer *tracer = vlc_tracer_Create(VLC_OBJECT(vlc->p_libvlc_int),
                                                  MODULE_STRING);
    assert(tracer != NULL);

    for (size_t i = 0; i < count; ++i)
    {
        if (scenario_name == NULL
         || strcmp(scenario_name, scenarios[i].name) == 0)
        {
            struct clock_scenario *scenario = &scenarios[i];
            if (forced_duration != VLC_TICK_INVALID)
                scenario->duration = forced_duration;
            play_scenario(vlc->p_libvlc_int, tracer, scenario);
        }
    }

    vlc_tracer_Destroy(tracer);

    tracer_ctx_Destroy(&tracer_ctx);

    libvlc_release(vlc);
}

static void normal_update(const struct clock_ctx *ctx, size_t index,
                          vlc_tick_t *system, vlc_tick_t stream)
{
    (void) index;
    const struct clock_scenario *scenario = ctx->scenario;

    vlc_clock_Lock(ctx->master);
    vlc_tick_t drift =
        vlc_clock_Update(ctx->master, *system, stream, 1.0f);
    vlc_clock_Unlock(ctx->master);
    /* The master can't drift */
    assert(drift == VLC_TICK_INVALID);

    *system += scenario->stream_increment;
}

static void check_no_event_error(size_t expected_update_count)
{
    /* assert that there is no error/status */
    assert(tracer_ctx.events.size > 0);

    size_t update_count = 0;
    for (size_t i = 0; i < tracer_ctx.events.size; ++i)
    {
        struct tracer_event event = tracer_ctx.events.data[i];
        switch (event.type)
        {
            case TRACER_EVENT_TYPE_UPDATE:
                update_count++;
                break;
            case TRACER_EVENT_TYPE_RENDER_VIDEO:
                break;
            case TRACER_EVENT_TYPE_STATUS:
                switch (event.status)
                {
                    case TRACER_EVENT_STATUS_RESET_USER:
                    case TRACER_EVENT_STATUS_RESET_BADSOURCE:
                        assert("clock reset not expected" == NULL);
                        break;
                }
                break;
        }
    }

    assert(update_count == expected_update_count);
}

static void normal_check(const struct clock_ctx *ctx, size_t update_count,
                         vlc_tick_t expected_system_end,
                         vlc_tick_t stream_end)
{
    const struct clock_scenario *scenario = ctx->scenario;

    check_no_event_error(update_count);
    vlc_tick_t last_video_date = VLC_TICK_INVALID;
    vlc_tick_t video_increment = vlc_tick_rate_duration(scenario->video_fps);

    for (size_t i = 0; i < tracer_ctx.events.size; ++i)
    {
        struct tracer_event event = tracer_ctx.events.data[i];

        switch (event.type)
        {
            case TRACER_EVENT_TYPE_UPDATE:
                assert(event.update.coeff == 1.0f);
                assert(event.update.offset ==
                       scenario->system_start - scenario->stream_start);
                break;
            case TRACER_EVENT_TYPE_RENDER_VIDEO:
                if (last_video_date != VLC_TICK_INVALID)
                    assert(event.render_video.play_date - last_video_date == video_increment);
                last_video_date = event.render_video.play_date;
                break;
            default:
                break;
        }
    }

    vlc_clock_Lock(ctx->slave);
    vlc_tick_t converted =
        vlc_clock_ConvertToSystem(ctx->slave, expected_system_end,
                                  stream_end, 1.0f);
    vlc_clock_Unlock(ctx->slave);
    assert(converted == expected_system_end);
}

static void lowprecision_update(const struct clock_ctx *ctx, size_t index,
                                vlc_tick_t *system, vlc_tick_t stream)
{
    (void) index;
    const struct clock_scenario *scenario = ctx->scenario;

    vlc_tick_t base_system = stream - scenario->stream_start
                           + scenario->system_start;
    /* random imprecision (seed based on stream) */
    srand(stream);
    vlc_tick_t imprecision = rand() % VLC_TICK_FROM_MS(5);
    *system = base_system + imprecision;

    vlc_clock_Lock(ctx->master);
    vlc_tick_t drift =
        vlc_clock_Update(ctx->master, *system, stream, 1.0f);
    vlc_clock_Unlock(ctx->master);
    /* The master can't drift */
    assert(drift == VLC_TICK_INVALID);
}

static void lowprecision_check(const struct clock_ctx *ctx, size_t update_count,
                               vlc_tick_t expected_system_end,
                               vlc_tick_t stream_end)
{
    const struct clock_scenario *scenario = ctx->scenario;
    (void) expected_system_end; (void) stream_end;

    check_no_event_error(update_count);

    for (size_t i = 0; i < tracer_ctx.events.size; ++i)
    {
        struct tracer_event event = tracer_ctx.events.data[i];
        switch (event.type)
        {
            case TRACER_EVENT_TYPE_UPDATE:
                assert(fabs(event.update.coeff - 1.0f) <= scenario->coeff_epsilon);
                break;
            default:
                break;
        }
    }
}

static void drift_update(const struct clock_ctx *ctx, size_t index,
                         vlc_tick_t *system, vlc_tick_t stream)
{
    (void) index;
    const struct clock_scenario *scenario = ctx->scenario;

    vlc_clock_Lock(ctx->master);
    vlc_tick_t drift =
        vlc_clock_Update(ctx->master, *system, stream, 1.0f);
    vlc_clock_Unlock(ctx->master);
    /* The master can't drift */
    assert(drift == VLC_TICK_INVALID);

    *system += scenario->stream_increment;

    /* Simulate 1us drift every stream_increment */
    if (scenario->total_drift_duration > 0)
        *system += VLC_TICK_FROM_US(1);
    else
        *system -= VLC_TICK_FROM_US(1);
}

static void drift_check(const struct clock_ctx *ctx, size_t update_count,
                        vlc_tick_t expected_system_end,
                        vlc_tick_t stream_end)
{
    const struct clock_scenario *scenario = ctx->scenario;

    check_no_event_error(update_count);

    vlc_clock_Lock(ctx->slave);
    vlc_tick_t converted =
        vlc_clock_ConvertToSystem(ctx->slave, expected_system_end,
                                  stream_end, 1.0f);
    vlc_clock_Unlock(ctx->slave);

    assert(converted - expected_system_end == scenario->total_drift_duration);
}

static void drift_sudden_update(const struct clock_ctx *ctx, size_t index,
                                vlc_tick_t *system, vlc_tick_t stream)
{
    (void) index;
    const struct clock_scenario *scenario = ctx->scenario;

    vlc_clock_Lock(ctx->slave);
    vlc_tick_t drift =
        vlc_clock_Update(ctx->master, *system, stream, 1.0f);
    vlc_clock_Unlock(ctx->slave);
    /* The master can't drift */
    assert(drift == VLC_TICK_INVALID);

    *system += scenario->stream_increment;

    if (stream - scenario->stream_start >= scenario->duration * 3 / 4)
    {
        /* Simulate a sudden high drift */
        *system += VLC_TICK_FROM_US(4);
    }
}

static void pause_common(const struct clock_ctx *ctx, vlc_clock_t *updater)
{
    const vlc_tick_t system_start = ctx->system_start;
    const vlc_tick_t pause_duration = VLC_TICK_FROM_MS(20);
    vlc_tick_t system = system_start;

    vlc_clock_Lock(updater);
    vlc_clock_Update(updater, system, ctx->stream_start, 1.0f);
    vlc_clock_Unlock(updater);

    {
        vlc_clock_Lock(ctx->slave);
        vlc_tick_t converted = vlc_clock_ConvertToSystem(ctx->slave, system, ctx->stream_start, 1.0f);
        assert(converted == system);
        vlc_clock_Unlock(ctx->slave);
    }

    system += VLC_TICK_FROM_MS(10);

    vlc_clock_main_Lock(ctx->mainclk);
    vlc_clock_main_ChangePause(ctx->mainclk, system, true);
    vlc_clock_main_Unlock(ctx->mainclk);

    system += pause_duration;

    vlc_clock_main_Lock(ctx->mainclk);
    vlc_clock_main_ChangePause(ctx->mainclk, system, false);
    vlc_clock_main_Unlock(ctx->mainclk);
    system += 1;

    vlc_clock_Lock(ctx->slave);
    vlc_tick_t converted = vlc_clock_ConvertToSystem(ctx->slave, system, ctx->stream_start, 1.0f);
    vlc_clock_Unlock(ctx->slave);
    assert(converted == system_start + pause_duration);
}

static void master_pause_run(const struct clock_ctx *ctx)
{
    pause_common(ctx, ctx->master);
}

static void monotonic_pause_run(const struct clock_ctx *ctx)
{
    pause_common(ctx, ctx->slave);
}

static void convert_paused_common(const struct clock_ctx *ctx, vlc_clock_t *updater)
{
    const vlc_tick_t system_start = ctx->system_start;
    vlc_tick_t system = system_start;

    vlc_clock_Lock(updater);
    vlc_clock_Update(updater, ctx->system_start, ctx->stream_start, 1.0f);
    vlc_clock_Unlock(updater);

    system += VLC_TICK_FROM_MS(10);

    vlc_clock_main_Lock(ctx->mainclk);
    vlc_clock_main_ChangePause(ctx->mainclk, system, true);
    vlc_clock_main_Unlock(ctx->mainclk);
    system += 1;

    vlc_clock_Lock(ctx->slave);
    vlc_tick_t converted = vlc_clock_ConvertToSystem(ctx->slave, system, ctx->stream_start, 1.0f);
    vlc_clock_Unlock(ctx->slave);
    assert(converted == system_start);
}

static void master_convert_paused_run(const struct clock_ctx *ctx)
{
    convert_paused_common(ctx, ctx->master);
}

static void monotonic_convert_paused_run(const struct clock_ctx *ctx)
{
    convert_paused_common(ctx, ctx->slave);
}

#define VLC_TICK_12H VLC_TICK_FROM_SEC(12 * 60 * 60)
#define VLC_TICK_2H VLC_TICK_FROM_SEC(2 * 60 * 60)
#define DEFAULT_STREAM_INCREMENT VLC_TICK_FROM_MS(100)

#define INIT_SYSTEM_STREAM_TIMING(duration_, increment_, video_fps_) \
    .stream_start = VLC_TICK_0 + VLC_TICK_FROM_MS(31000000), \
    .system_start = VLC_TICK_0, \
    .duration = duration_, \
    .stream_increment = increment_, \
    .video_fps = video_fps_

static struct clock_scenario clock_scenarios[] = {
{
    .name = "normal",
    .desc = "normal update has a coeff of 1.0f",
    .type = CLOCK_SCENARIO_UPDATE,
    INIT_SYSTEM_STREAM_TIMING(VLC_TICK_2H, DEFAULT_STREAM_INCREMENT, 60),
    .update = normal_update,
    .check = normal_check,
},
{
    .name = "lowprecision",
    .desc = "low precision update has a coeff near 1.0f",
    .type = CLOCK_SCENARIO_UPDATE,
    INIT_SYSTEM_STREAM_TIMING(VLC_TICK_2H, DEFAULT_STREAM_INCREMENT, 60),
    .coeff_epsilon = 0.005,
    .update = lowprecision_update,
    .check = lowprecision_check,
},
{
    .name = "drift_72",
    .desc = "a drift of 72ms in 2h is handled",
    .type = CLOCK_SCENARIO_UPDATE,
    INIT_SYSTEM_STREAM_TIMING(VLC_TICK_2H, DEFAULT_STREAM_INCREMENT, 60),
    .total_drift_duration = VLC_TICK_FROM_MS(72),
    .update = drift_update,
    .check = drift_check,
},
{
    .name = "drift_-72",
    .desc = "a drift of -72ms in 2h is handled",
    .type = CLOCK_SCENARIO_UPDATE,
    INIT_SYSTEM_STREAM_TIMING(VLC_TICK_2H, DEFAULT_STREAM_INCREMENT, 60),
    .total_drift_duration = -VLC_TICK_FROM_MS(72),
    .update = drift_update,
    .check = drift_check,
},
{
    .name = "drift_432",
    .desc = "a drift of 432ms in 12h is handled",
    .type = CLOCK_SCENARIO_UPDATE,
    INIT_SYSTEM_STREAM_TIMING(VLC_TICK_12H, DEFAULT_STREAM_INCREMENT, 0),
    .total_drift_duration = VLC_TICK_FROM_MS(432),
    .update = drift_update,
    .check = drift_check,
},
{
    .name = "drift_-432",
    .desc = "a drift of -432ms in 12h is handled",
    .type = CLOCK_SCENARIO_UPDATE,
    INIT_SYSTEM_STREAM_TIMING(VLC_TICK_12H, DEFAULT_STREAM_INCREMENT, 0),
    .total_drift_duration = -VLC_TICK_FROM_MS(432),
    .update = drift_update,
    .check = drift_check,
},
{
    .name = "drift_sudden",
    .desc = "a sudden drift is handled",
    .type = CLOCK_SCENARIO_UPDATE,
    INIT_SYSTEM_STREAM_TIMING(VLC_TICK_12H, DEFAULT_STREAM_INCREMENT, 0),
    .total_drift_duration = VLC_TICK_FROM_MS(432),
    .update = drift_sudden_update,
    .check = drift_check,
},
{
    .name = "master_pause",
    .desc = "pause + resume is delaying the next conversion",
    .type = CLOCK_SCENARIO_RUN,
    .run = master_pause_run,
},
{
    .name = "monotonic_pause",
    .desc = "pause + resume is delaying the next conversion",
    .type = CLOCK_SCENARIO_RUN,
    .run = monotonic_pause_run,
    .disable_jitter = true,
},
{
    .name = "master_convert_paused",
    .desc = "it is possible to convert ts while paused",
    .type = CLOCK_SCENARIO_RUN,
    .run = master_convert_paused_run,
},
{
    .name = "monotonic_convert_paused",
    .desc = "it is possible to convert ts while paused",
    .type = CLOCK_SCENARIO_RUN,
    .run = monotonic_convert_paused_run,
    .disable_jitter = true,
},
};

int main(int argc, const char *argv[])
{
    test_init();
    run_scenarios(argc, argv, clock_scenarios, ARRAY_SIZE(clock_scenarios));

    return EXIT_SUCCESS;
}
