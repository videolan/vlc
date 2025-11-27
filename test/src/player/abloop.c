// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * abloop.c: abloop player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"
#include "timers.h"

struct abloop_scenario
{
    bool can_seek;
    bool after_1st_buferring;
    bool nolength_report;
    bool wait_stopped;
    bool check_prev_ts;
    vlc_tick_t length;
    vlc_tick_t a_time;
    vlc_tick_t b_time;
    double a_pos;
    double b_pos;
    size_t seek_count;
};

static void
test_abloop_scenario(struct ctx *ctx, const struct abloop_scenario *scenario)
{
    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(scenario->length);
    params.can_seek = scenario->can_seek;
    params.track_count[AUDIO_ES] = 1;
    params.track_count[VIDEO_ES] = 0;
    params.track_count[SPU_ES] = 0;
    params.report_length = !scenario->nolength_report;
    player_set_next_mock_media(ctx, "media1", &params);

    struct timer_state timer;
    player_add_timer(player, &timer, false, VLC_TICK_INVALID);

    player_start(ctx);

    size_t expected_buffering_count = scenario->seek_count;
    if (scenario->after_1st_buferring)
    {
        /* Wait for 1st buffering */
        while (get_buffering_count(ctx) < 1)
            vlc_player_CondWait(ctx->player, &ctx->wait);
        expected_buffering_count++;
    }

    if (scenario->a_time != VLC_TICK_INVALID)
    {
        assert(scenario->b_time != VLC_TICK_INVALID);
        int ret = vlc_player_SetAtoBLoopTime(ctx->player, scenario->a_time,
                                             scenario->b_time);
        assert(ret == VLC_SUCCESS);
    }
    else
    {
        int ret = vlc_player_SetAtoBLoopPosition(ctx->player, scenario->a_pos,
                                                 scenario->b_pos);
        assert(ret == VLC_SUCCESS);
    }

    if (!scenario->wait_stopped)
    {
        /* Wait all seek request */
        while (get_buffering_count(ctx) < expected_buffering_count)
            vlc_player_CondWait(ctx->player, &ctx->wait);

        vlc_player_Stop(player);
    }
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    vec_report_timer *vec = &timer.vec;
    size_t seek_count = 0;
    for (size_t i = 0; i < vec->size; ++i)
    {
        struct report_timer *report = &vec->data[i];
        if (report->type != REPORT_TIMER_SEEK)
            continue;
        if (report->seek.finished)
            continue;
        struct vlc_player_timer_point *point = &report->seek.point;

        if (scenario->a_time != VLC_TICK_INVALID)
            assert(point->ts == scenario->a_time);
        else
            assert(point->position == scenario->a_pos);

        if (seek_count > 0 && scenario->check_prev_ts)
        {
            assert(scenario->a_time != VLC_TICK_INVALID);
            assert(i > 0);
            bool checked = false;
            for (size_t j = i - 1; j > 0; --j)
            {
                struct report_timer *time_report = &vec->data[j];
                if (time_report->type != REPORT_TIMER_POINT)
                    continue;
                struct vlc_player_timer_point *time_point = &time_report->point;

                vlc_tick_t end_ts = scenario->b_time;
                if (end_ts > params.length)
                    end_ts = params.length;
                assert(time_point->ts >= scenario->a_time);
                assert(time_point->ts <= end_ts);
                assert(time_point->ts + params.audio_sample_length >= end_ts);
                checked = true;
                break;
            }
            assert(checked);
        }
        seek_count++;
    }
    assert(seek_count == scenario->seek_count);

    test_end(ctx);
    player_remove_timer(player, &timer);
}

static void
test_abloop(struct ctx *ctx)
{
    const struct abloop_scenario scenarios[] =
    {
        {
            /* Check that b_time past length is handled */
            .can_seek = true, .after_1st_buferring = false,
            .check_prev_ts = true,
            .length = VLC_TICK_FROM_MS(20000),
            .a_time = VLC_TICK_FROM_MS(19800), .b_time = VLC_TICK_FROM_MS(30000),
            .seek_count = 2,
        },
        {
            /* Check we have the same result if called after buffering */
            .can_seek = true, .after_1st_buferring = true,
            .check_prev_ts = true,
            .length = VLC_TICK_FROM_MS(20000),
            .a_time = VLC_TICK_FROM_MS(19800), .b_time = VLC_TICK_FROM_MS(30000),
            .seek_count = 2,
        },
        {
            /* Check small A->B loop values */
            .can_seek = true, .after_1st_buferring = true,
            .length = VLC_TICK_FROM_MS(3000),
            .a_time = VLC_TICK_FROM_MS(1), .b_time = VLC_TICK_FROM_MS(2),
            .seek_count = 4,
        },
        {
            /* Check with positions */
            .can_seek = true, .after_1st_buferring = true,
            .length = VLC_TICK_FROM_MS(3000),
            .a_pos = 0.9, .b_pos = 1.0f,
            .seek_count = 1,
        },
        {
            /* Check we have the same result if called after buffering */
            .can_seek = true, .after_1st_buferring = false,
            .length = VLC_TICK_FROM_MS(3000),
            .a_pos = 0.9, .b_pos = 1.0f,
            .seek_count = 2,
        },
        {
            /* Check that seek is triggered by EOF (no reported length) */
            .can_seek = true, .after_1st_buferring = false,
            .nolength_report = true,
            .length = VLC_TICK_FROM_MS(1000),
            .a_pos = 0.9, .b_pos = 1.0f,
            .seek_count = 2,
        },
        {
            /* Check that A->B loop is not triggered */
            .can_seek = false, .after_1st_buferring = false,
            .nolength_report = true, .wait_stopped = true,
            .length = VLC_TICK_FROM_MS(100),
            .a_pos = 0.1, .b_pos = 1.0f,
            .seek_count = 1, /* Caused by the A seek request that fails */
        },
    };

    for (size_t i = 0; i < ARRAY_SIZE(scenarios); ++i)
    {
        test_log("abloop[%zu]\n", i);
        test_abloop_scenario(ctx, &scenarios[i]);
    }
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_abloop(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
