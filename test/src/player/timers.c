// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * timers.c: timers player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"
#include "timers.h"

static void
test_timers_assert_smpte(struct timer_state *timer,
                         vlc_tick_t duration, unsigned fps, bool drop_frame,
                         unsigned frame_resolution)
{
    /* This test doesn't support drop frame handling */
    assert(duration < VLC_TICK_FROM_SEC(60));

    vec_report_timer *vec = &timer->vec;

    /* Check that we didn't miss any update points */
    assert(vec->data[0].tc.frames == 0);
    struct report_timer *prev_report = NULL;

    for (size_t i = 0; i < vec->size; ++i)
    {
        struct report_timer *report = &vec->data[i];
        if (report->type != REPORT_TIMER_TC)
            continue;

        assert(report->tc.seconds == (i / fps));
        if (prev_report)
        {
            if (i % fps == 0)
            {
                assert(prev_report->tc.frames == fps - 1);
                assert(report->tc.frames == 0);
            }
            else
                assert(report->tc.frames == prev_report->tc.frames + 1);
        }

        assert(report->tc.drop_frame == drop_frame);
        assert(report->tc.frame_resolution == frame_resolution);
        prev_report = report;
    }
    assert(prev_report->tc.frames + 1 == fps * duration / VLC_TICK_FROM_SEC(1));
}

static void
test_timers_assert_smpte_dropframe(struct timer_state *timer, unsigned minute,
                                   unsigned fps)
{
    assert(fps == 30 || fps == 60);
    assert(minute > 0);

    vec_report_timer *vec = &timer->vec;

    bool last_second_seen = false, minute_seen = false;
    struct report_timer *prev_report = NULL;
    for (size_t i = 0; i < vec->size; ++i)
    {
        struct report_timer *report = &vec->data[i];
        if (report->type != REPORT_TIMER_TC)
            continue;
        if (prev_report == NULL)
        {
            prev_report = report;
            continue;
        }

        assert(report->tc.drop_frame == true);
        assert(report->tc.frame_resolution == 2);

        if (prev_report->tc.frames == fps - 1)
        {
            if (report->tc.seconds == 59)
            {
                /* Last second before the new minute */
                assert(prev_report->tc.minutes == minute - 1);
                assert(prev_report->tc.seconds == 58);

                assert(report->tc.minutes == minute - 1);
                assert(report->tc.frames == 0);

                last_second_seen = true;
            }
            else if (report->tc.seconds == 0)
            {
                /* The minute just reached, check that 2 or 4 frames are
                 * dropped every minutes, except every 10 minutes */

                assert(prev_report->tc.minutes == minute - 1);
                assert(prev_report->tc.seconds == 59);

                assert(report->tc.minutes == minute);
                if (minute % 10 == 0)
                    assert(report->tc.frames == 0);
                else
                    assert(report->tc.frames == (fps / 30 * 2) /* drop frame */);

                minute_seen = true;
            }

        }
        else if (prev_report->tc.minutes != 0 && prev_report->tc.seconds != 0
              && prev_report->tc.frames != 0)
            assert(report->tc.frames == prev_report->tc.frames + 1);
        prev_report = report;
    }

    /* Assert that we've seen the full last second and the new minute */
    assert(last_second_seen && minute_seen);
}

#define REGULAR_TIMER_IDX 0
#define REGULAR_DELAY_TIMER_IDX 1
#define SMPTE_TIMER_IDX 2
#define TIMER_COUNT 3
#define SOURCE_DELAY_TIMER_VALUE (VLC_TICK_FROM_MS(2))

static void
test_timers_playback(struct ctx *ctx, struct timer_state timers[],
                     size_t track_count, vlc_tick_t length, unsigned fps,
                     unsigned rate)
{
#define SAMPLE_LENGTH VLC_TICK_FROM_MS(1)
#define MAX_UPDATE_COUNT (size_t)(length / SAMPLE_LENGTH)

    struct media_params params = DEFAULT_MEDIA_PARAMS(length);

    params.track_count[VIDEO_ES] = track_count;
    params.track_count[AUDIO_ES] = track_count;
    params.track_count[SPU_ES] = track_count;
    params.audio_sample_length = SAMPLE_LENGTH;
    params.video_frame_rate = fps;
    params.video_frame_rate_base = 1;

    player_set_current_mock_media(ctx, "media1", &params, false);
    player_set_rate(ctx, rate);
    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STARTED);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    /* Common for regular timers */
    for (size_t timer_idx = 0; timer_idx < SMPTE_TIMER_IDX; ++timer_idx)
    {
        struct timer_state *timer = &timers[timer_idx];
        vec_report_timer *vec = &timer->vec;

        assert(vec->size > 1);

        for (size_t i = 1; i < vec->size; ++i)
        {
            struct report_timer *prev_report = &vec->data[i - 1];
            struct report_timer *report = &vec->data[i];

            /* Only the last event should be a discontinuity. We can assume
             * that since we are not seeking and playing a fake content */
            if (i < vec->size - 1)
            {
                if (i == 1)
                    assert(prev_report->point.system_date == INT64_MAX);

                assert(report->type == REPORT_TIMER_POINT);
                /* ts/position should increase, rate should stay to 1.f */
                assert(report->point.ts >= prev_report->point.ts);
                assert(report->point.system_date != VLC_TICK_INVALID);
                assert(report->point.position >= prev_report->point.position);
                assert(report->point.rate == rate);
                assert(report->point.length == length);
            }
            else
            {
                assert(report->type == REPORT_TIMER_PAUSED);
                assert(report->paused_date == VLC_TICK_INVALID);
            }
        }
    }

    /* If there is no master source, we can't known which sources (audio or
     * video) will feed the timer. Indeed the first source that trigger a clock
     * update will be used as a timer source (and audio/video goes through
     * decoder threads and output threads, adding more uncertainty). */
    if ((ctx->flags & CLOCK_MASTER_MONOTONIC) == 0)
    {
        /* Assertions for the regular timer that received all update points */
        if (track_count != 0)
        {
            struct timer_state *timer = &timers[REGULAR_TIMER_IDX];
            vec_report_timer *vec = &timer->vec;

            /* Check that we didn't miss any update points */
            assert(vec->size > 1);
            size_t point_count = 1;
            for (size_t i = 1; i < vec->size - 1; ++i)
            {
                struct report_timer *prev_report = &vec->data[i - 1];
                struct report_timer *report = &vec->data[i];

                /* Don't count forced points */
                if (report->point.ts != prev_report->point.ts)
                {
                    assert(report->point.ts ==
                           prev_report->point.ts + SAMPLE_LENGTH);
                    point_count++;
                }
            }
            assert(vec->data[vec->size - 2].point.ts
                == length - SAMPLE_LENGTH + VLC_TICK_0);
            assert(point_count == MAX_UPDATE_COUNT);
        }

        /* Assertions for the regular filtered timer */
        {
            struct timer_state *timer = &timers[REGULAR_DELAY_TIMER_IDX];
            vec_report_timer *vec = &timer->vec;

            /* It should not receive all update points */
            assert(vec->size < MAX_UPDATE_COUNT);
            assert(vec->size > 1);

            for (size_t i = 1; i < vec->size; ++i)
            {
                struct report_timer *prev_report = &vec->data[i - 1];
                struct report_timer *report = &vec->data[i];
                if (i < vec->size - 1)
                {
                    if (i == 1)
                        assert(prev_report->point.system_date == INT64_MAX);
                    else
                        assert(report->point.system_date -
                               prev_report->point.system_date >= timer->delay);
                }
            }
        }
    }

    if (track_count > 0)
        test_timers_assert_smpte(&timers[SMPTE_TIMER_IDX], length, fps, false, 3);
    else
    {
        struct timer_state *timer = &timers[SMPTE_TIMER_IDX];
        vec_report_timer *vec = &timer->vec;
        assert(vec->size == 1);
        assert(timer->vec.data[0].type == REPORT_TIMER_PAUSED);
    }
    test_end(ctx);

    for (size_t i = 0; i < TIMER_COUNT; ++i)
    {
        struct timer_state *timer = &timers[i];
        vlc_vector_clear(&timer->vec);
    }
}

static void
test_timers(struct ctx *ctx)
{
    test_log("timers%s\n",
        (ctx->flags & CLOCK_MASTER_MONOTONIC) ? " (monotonic)" : "");

    vlc_player_t *player = ctx->player;

    /* Configure timers */
    struct timer_state timers[TIMER_COUNT];

    /* Receive all clock update points */
    timers[REGULAR_TIMER_IDX].delay = VLC_TICK_INVALID;

    /* Filter some points in order to not be flooded */
    timers[REGULAR_DELAY_TIMER_IDX].delay = SOURCE_DELAY_TIMER_VALUE;
    timers[SMPTE_TIMER_IDX].delay = VLC_TICK_INVALID;

    /* Create all timers */
    for (size_t i = 0; i < ARRAY_SIZE(timers); ++i)
    {
        vlc_vector_init(&timers[i].vec);
        bool smpte = i == SMPTE_TIMER_IDX;
        player_add_timer(player, &timers[i], smpte, timers[i].delay);
    }

    /* Test all timers using valid tracks */
    test_timers_playback(ctx, timers, 1, VLC_TICK_FROM_MS(200), 120, 1);

    /* Test all timers without valid tracks */
    test_timers_playback(ctx, timers, 0, VLC_TICK_FROM_MS(5000), 24, 16);

    /* Test SMPTE 29.97DF and 59.94DF arround 1 minute and 10 minutes to check
     * if timecodes are dropped every minutes */
    static const unsigned df_fps_list[] = { 30, 60 };
    static const unsigned df_min_test_list[] = { 1, 10 };

    for (size_t i = 0; i < ARRAY_SIZE(df_fps_list); ++i)
    {
        unsigned fps = df_fps_list[i];
        for (size_t j = 0; j < ARRAY_SIZE(df_min_test_list); ++j)
        {
            unsigned minute = df_min_test_list[j];
            vlc_tick_t check_duration = VLC_TICK_FROM_SEC(2);

            struct media_params params =
                DEFAULT_MEDIA_PARAMS(minute * VLC_TICK_FROM_SEC(60)
                                     + VLC_TICK_FROM_MS(400));
            params.track_count[VIDEO_ES] = 1;
            params.track_count[AUDIO_ES] = 0;
            params.track_count[SPU_ES] = 0;
            params.video_frame_rate = fps * 1000;
            params.video_frame_rate_base = 1001;

            /* This will prevent a RESET_PCR and ensure we receive all outputs
             * points. */
            params.pts_delay = check_duration;

            player_set_current_mock_media(ctx, "media1", &params, false);
            player_set_rate(ctx, 24);

            vlc_player_SetTime(player, params.length - check_duration);

            player_start(ctx);

            wait_state(ctx, VLC_PLAYER_STATE_STARTED);
            wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

            test_timers_assert_smpte_dropframe(&timers[SMPTE_TIMER_IDX], minute,
                                               fps);

            test_end(ctx);

            vlc_vector_clear(&timers[SMPTE_TIMER_IDX].vec);
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(timers); ++i)
    {
        struct timer_state *timer = &timers[i];
        player_remove_timer(player, timer);
    }
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_timers(&ctx);
    ctx_destroy(&ctx);
    ctx_init(&ctx, CLOCK_MASTER_MONOTONIC);
    test_timers(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
