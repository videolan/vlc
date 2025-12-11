// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * tracks_ids.c: tracks IDs player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"
#include "timers.h"

struct np_ctx
{
    struct ctx *ctx;
    struct timer_state timers[2];
    size_t timers_count;
    size_t prev_status_idx;
    size_t next_status_idx;
    struct vlc_player_timer_smpte_timecode tc;
};

static unsigned
get_fps(struct ctx *ctx)
{
    return ceil(ctx->params.video_frame_rate /
                (float) ctx->params.video_frame_rate_base);
}


static void
wait_prev_frame_status(struct np_ctx *np_ctx, size_t count, int status)
{
    struct ctx *ctx = np_ctx->ctx;
    size_t idx = np_ctx->prev_status_idx;
    vec_on_prev_frame_status *vec = &ctx->report.on_prev_frame_status;
    while (vec->size < idx + count)
        vlc_player_CondWait(ctx->player, &ctx->wait);
    for (size_t i = idx; i < idx + count; ++i)
        assert(vec->data[i] == status);
    idx += count;
    np_ctx->prev_status_idx = idx;
}

static void
wait_next_frame_status(struct np_ctx *np_ctx, size_t count, int status)
{
    struct ctx *ctx = np_ctx->ctx;
    size_t idx = np_ctx->next_status_idx;
    vec_on_next_frame_status *vec = &ctx->report.on_next_frame_status;
    while (vec->size < idx + count)
        vlc_player_CondWait(ctx->player, &ctx->wait);
    for (size_t i = idx; i < idx + count; ++i)
        assert(vec->data[i] == status);
    idx += count;
    np_ctx->next_status_idx = idx;
}

static void
decrease_tc(struct np_ctx *np_ctx)
{
    unsigned fps = get_fps(np_ctx->ctx);
    struct vlc_player_timer_smpte_timecode *tc = &np_ctx->tc;

    if (tc->frames == 0)
    {
        if (tc->seconds > 0)
        {
            tc->seconds--;
            tc->frames = fps - 1;
        }
    }
    else
        tc->frames--;
}

static void
increase_tc(struct np_ctx *np_ctx)
{
    unsigned fps = get_fps(np_ctx->ctx);
    struct vlc_player_timer_smpte_timecode *tc = &np_ctx->tc;

    tc->frames++;
    if (tc->frames == fps)
    {
        tc->frames = 0;
        tc->seconds++;
    }
}

static void
wait_type_timer(struct np_ctx *np_ctx, unsigned type)
{
    struct ctx *ctx = np_ctx->ctx;
    vlc_player_t *player = ctx->player;

    for (size_t i = 0; i < np_ctx->timers_count; ++i)
    {
        struct timer_state *timer = &np_ctx->timers[i];
        /* Seek events come with 2 reports (start/end) */
        unsigned nb_reports = type == REPORT_TIMER_SEEK ? 2 : 1;
        player_lock_timer(player, timer);
        for (;;)
        {
            struct report_timer *r = timer_state_wait_next_report(timer);
            if (r->type == type)
            {
                nb_reports--;
                if (nb_reports == 0)
                {
                    player_unlock_timer(player, timer);
                    break;
                }
            }
            if (type != REPORT_TIMER_SEEK && r->type == REPORT_TIMER_TC)
                np_ctx->tc = r->tc;
        }
    }
}

static void
wait_next_tc(struct np_ctx *np_ctx)
{
    struct ctx *ctx = np_ctx->ctx;
    vlc_player_t *player = ctx->player;
    struct timer_state *timer = &np_ctx->timers[0];
    struct vlc_player_timer_smpte_timecode *tc = &np_ctx->tc;

    player_lock_timer(player, timer);
    struct report_timer *r = timer_state_wait_next_report(timer);
    assert(r->type == REPORT_TIMER_TC);
    *tc = r->tc;
    player_unlock_timer(player, timer);

    if (np_ctx->timers_count > 1)
    {
        struct timer_state *normal_timer = &np_ctx->timers[1];
        player_lock_timer(player, normal_timer);
        r = timer_state_wait_next_report(normal_timer);
        assert(r->type == REPORT_TIMER_POINT);
        player_unlock_timer(player, normal_timer);
    }
}

static void
check_resumed_timer(struct np_ctx *np_ctx)
{
    struct ctx *ctx = np_ctx->ctx;

    /* Check that the player can be resumed and paused again after
     * next/prev frame */
    vlc_player_t *player = ctx->player;
    assert_state(ctx, VLC_PLAYER_STATE_PAUSED);
    vlc_player_Resume(player);
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);

    /* Wait that a video frame is rendered */
    wait_next_tc(np_ctx);

    vlc_player_Pause(player);

    wait_state(ctx, VLC_PLAYER_STATE_PAUSED);

    wait_type_timer(np_ctx, REPORT_TIMER_PAUSED);
}

static void
check_seek_timer(struct np_ctx *np_ctx)
{
    /* Check that the player can be resumed and paused again after
     * next/prev frame */
    struct ctx *ctx = np_ctx->ctx;
    vlc_player_t *player = ctx->player;

    assert_state(ctx, VLC_PLAYER_STATE_PAUSED);
    vlc_player_SetTime(player, ctx->params.length / 2);
    assert_state(ctx, VLC_PLAYER_STATE_PAUSED);

    /* Wait for the seek */
    wait_type_timer(np_ctx, REPORT_TIMER_SEEK);

    /* Wait that a video frame is rendered */
    wait_next_tc(np_ctx);

    assert_state(ctx, VLC_PLAYER_STATE_PAUSED);
}

static void
check_next_frame_timer(struct np_ctx *np_ctx)
{
    /* Check that the player can go next frame after a prev frame */
    struct ctx *ctx = np_ctx->ctx;
    struct timer_state *timer = &np_ctx->timers[0];
    vlc_player_t *player = ctx->player;
    struct vlc_player_timer_smpte_timecode *tc = &np_ctx->tc;

    assert_state(ctx, VLC_PLAYER_STATE_PAUSED);

    vlc_player_NextVideoFrame(player);
    wait_next_frame_status(np_ctx, 1, 0);

    increase_tc(np_ctx);

    /* Wait that a video frame is rendered */
    struct report_timer *r = NULL;
    player_lock_timer(player, timer);
    r = timer_state_wait_next_report(timer);
    assert(r->type == REPORT_TIMER_TC);
    assert(r->tc.seconds == tc->seconds);
    assert(r->tc.frames == tc->frames);
    player_unlock_timer(player, timer);

    if (np_ctx->timers_count > 1)
    {
        struct timer_state *normal_timer = &np_ctx->timers[1];
        player_lock_timer(player, normal_timer);
        r = timer_state_wait_next_report(normal_timer);
        assert(r->type == REPORT_TIMER_POINT);
        player_unlock_timer(player, normal_timer);
    }
}

static size_t
get_frame_count_to_start(struct np_ctx *np_ctx)
{
    unsigned fps = get_fps(np_ctx->ctx);

    return np_ctx->tc.frames + VLC_TICK_FROM_SEC(np_ctx->tc.seconds)
        / vlc_tick_rate_duration(fps);
}

static void check_normal_timer(struct np_ctx *np_ctx, size_t count, bool next)
{
    if (np_ctx->timers_count < 2)
        return;
    struct ctx *ctx = np_ctx->ctx;
    struct timer_state *normal_timer = &np_ctx->timers[1];
    vlc_player_t *player = ctx->player;

    player_lock_timer(player, normal_timer);
    vlc_tick_t last_ts = next ? VLC_TICK_0 : VLC_TICK_MAX;
    for (size_t i = 0; i < count; ++i)
    {
        struct report_timer *r = timer_state_wait_next_report(normal_timer);
        assert(r->type == REPORT_TIMER_POINT);
        if (next)
            assert(r->point.ts > last_ts);
        else
            assert(last_ts > r->point.ts);
        last_ts = r->point.ts;
    }
    player_unlock_timer(player, normal_timer);
}

static void
go_start(struct np_ctx *np_ctx, bool extra_checks)
{
    struct ctx *ctx = np_ctx->ctx;
    struct timer_state *timer = &np_ctx->timers[0];
    vlc_player_t *player = ctx->player;
    struct vlc_player_timer_smpte_timecode *tc = &np_ctx->tc;
    struct report_timer *r = NULL;

    size_t frame_prev_count_total = get_frame_count_to_start(np_ctx);
    unsigned burst = frame_prev_count_total > 4 ? frame_prev_count_total / 4
                   : frame_prev_count_total;
    if (burst > 100)
        burst = 100;

    bool check_resume, check_seek, check_next;
    check_resume = check_seek = check_next = extra_checks;

    decrease_tc(np_ctx);
    /* Send prev-frame requests in burst until we reach start of file */
    while (frame_prev_count_total > 0)
    {
        size_t frame_prev_count = frame_prev_count_total > burst ? burst
                                : frame_prev_count_total;
        frame_prev_count_total -= frame_prev_count;

        /* Send burst */
        for (size_t i = 0; i < frame_prev_count; ++i)
            vlc_player_PreviousVideoFrame(player);

        /* Wait for all prev-frame status */
        wait_prev_frame_status(np_ctx, frame_prev_count, 0);

        /* Check that all video timecodes are decreasing */
        player_lock_timer(player, timer);
        for (size_t i = 0; i < frame_prev_count; ++i)
        {
            r = timer_state_wait_next_report(timer);
            assert(r->type == REPORT_TIMER_TC);
            assert(r->tc.seconds == tc->seconds);
            assert(r->tc.frames == tc->frames);

            decrease_tc(np_ctx);
        }
        player_unlock_timer(player, timer);

        /* Ensure the normal timer is moving */
        check_normal_timer(np_ctx, frame_prev_count, false);

        /* extra checks */
        if (check_seek)
        {
            /* Check seek is not broken after prev-frame */
            check_seek_timer(np_ctx);
            check_seek = false;
            /* And continue prev-frame */
            frame_prev_count_total = get_frame_count_to_start(np_ctx);
            decrease_tc(np_ctx);
        }
        else if (check_resume)
        {
            /* Check resume is not broken after prev-frame */
            check_resumed_timer(np_ctx);
            check_resume = false;
            /* And continue prev-frame */
            frame_prev_count_total = get_frame_count_to_start(np_ctx);
            decrease_tc(np_ctx);
        }
        else if (check_next)
        {
            /* Check next-frame is not broken after prev-frame*/
            /* Cancel last decrease_tc() from for loop */
            increase_tc(np_ctx);
            check_next_frame_timer(np_ctx);
            check_next = false;
            frame_prev_count_total = get_frame_count_to_start(np_ctx);
            decrease_tc(np_ctx);
        }
    }
    assert(!check_resume);
    assert(!check_seek);
    assert(!check_next);

    assert(tc->seconds == 0);
    assert(tc->frames == 0);

    /* Ensure start of File is handled */
    vlc_player_PreviousVideoFrame(player);
    wait_prev_frame_status(np_ctx, 1, -EAGAIN);
}

static void
go_eof(struct np_ctx *np_ctx)
{
    struct ctx *ctx = np_ctx->ctx;
    struct timer_state *timer = &np_ctx->timers[0];
    vlc_player_t *player = ctx->player;
    struct vlc_player_timer_smpte_timecode *tc = &np_ctx->tc;
    struct report_timer *r = NULL;

    unsigned fps = get_fps(ctx);

    increase_tc(np_ctx);

    size_t current_frame_count = tc->seconds * fps + tc->frames;
    size_t end_frame_count = ctx->params.length * fps / CLOCK_FREQ;
    assert(end_frame_count > current_frame_count);
    size_t frame_next_count_total = end_frame_count - current_frame_count;
    unsigned burst = 100;

    /* Send prev-frame requests in burst until we reach start of file */
    while (frame_next_count_total > 0)
    {
        size_t frame_next_count = frame_next_count_total > burst ? burst
                                : frame_next_count_total;

        frame_next_count_total -= frame_next_count;

        /* Send burst */
        for (size_t i = 0; i < frame_next_count; ++i)
            vlc_player_NextVideoFrame(player);

        /* Wait for all next-frame status */
        wait_next_frame_status(np_ctx, frame_next_count, 0);

        /* Check that all video timecodes are increasing */
        player_lock_timer(player, timer);
        for (size_t i = 0; i < frame_next_count; ++i)
        {
            r = timer_state_wait_next_report(timer);
            assert(r->type == REPORT_TIMER_TC);
            assert(r->tc.seconds == tc->seconds);
            assert(r->tc.frames == tc->frames);

            increase_tc(np_ctx);
        }
        player_unlock_timer(player, timer);

        /* Ensure the normal timer is moving */
        check_normal_timer(np_ctx, frame_next_count, true);
    }

    vlc_player_NextVideoFrame(player);
    wait_next_frame_status(np_ctx, 1, -EAGAIN);

    decrease_tc(np_ctx);
}

static void
resume(struct np_ctx *np_ctx)
{
    struct ctx *ctx = np_ctx->ctx;
    vlc_player_t *player = ctx->player;

    /* Resume player */
    vlc_player_Resume(player);
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);

    /* Wait for the forced frame after resume and next/prev frame */
    wait_next_tc(np_ctx);

    /* Wait for the normal frame after resume */
    wait_next_tc(np_ctx);
}

static void
burst_unpaused(struct np_ctx *np_ctx)
{
    struct ctx *ctx = np_ctx->ctx;
    vlc_player_t *player = ctx->player;
    struct timer_state *timer = &np_ctx->timers[0];
    struct vlc_player_timer_smpte_timecode *tc = &np_ctx->tc;
    struct report_timer *r = NULL;
    const unsigned burst = 6;

    assert_state(ctx, VLC_PLAYER_STATE_PLAYING);

    for (size_t i = 0; i < burst + 1 /* pause */; i++)
        vlc_player_NextVideoFrame(player);

    /* First request is pausing the video */
    wait_next_frame_status(np_ctx, 1, -EAGAIN);
    assert_state(ctx, VLC_PLAYER_STATE_PAUSED);
    wait_type_timer(np_ctx, REPORT_TIMER_PAUSED);

    /* Wait all status events */
    wait_next_frame_status(np_ctx, burst, 0);

    /* Check updated timecodes */
    increase_tc(np_ctx);
    player_lock_timer(player, timer);
    for (size_t i = 0; i < burst; ++i)
    {
        r = timer_state_wait_next_report(timer);

        assert(r->type == REPORT_TIMER_TC);
        assert(r->tc.seconds == tc->seconds);
        assert(r->tc.frames == tc->frames);

        increase_tc(np_ctx);
    }
    player_unlock_timer(player, timer);

    /* Ensure the normal timer is moving */
    check_normal_timer(np_ctx, burst, true);

    /* Resume player */
    resume(np_ctx);

    for (size_t i = 0; i < burst + 1 /* pause */; i++)
        vlc_player_PreviousVideoFrame(player);

    /* First request is pausing the video */
    wait_prev_frame_status(np_ctx, 1, -EAGAIN);
    assert_state(ctx, VLC_PLAYER_STATE_PAUSED);
    wait_type_timer(np_ctx, REPORT_TIMER_PAUSED);

    /* Wait all status events */
    wait_prev_frame_status(np_ctx, burst, 0);

    /* Check updated timecodes */
    decrease_tc(np_ctx);
    player_lock_timer(player, timer);
    for (size_t i = 0; i < burst; ++i)
    {
        r = timer_state_wait_next_report(timer);

        assert(r->type == REPORT_TIMER_TC);
        assert(r->tc.seconds == tc->seconds);
        assert(r->tc.frames == tc->frames);

        decrease_tc(np_ctx);
    }
    player_unlock_timer(player, timer);

    /* Ensure the normal timer is moving */
    check_normal_timer(np_ctx, burst, false);

    /* Resume player */
    resume(np_ctx);
}

static void
test_prev(struct ctx *ctx, const struct media_params *params, bool extra_checks)
{
    test_log("prev-frame (fps: %u/%u pts-delay: %"PRId64" with_audio: %zu)\n",
             params->video_frame_rate, params->video_frame_rate_base,
             params->pts_delay, params->track_count[AUDIO_ES]);
    vlc_player_t *player = ctx->player;
    struct np_ctx np_ctx = {
        .ctx = ctx,
        .prev_status_idx = 0,
        .next_status_idx = 0,
    };

    player_set_current_mock_media(ctx, "media1", params, false);

    player_add_timer(player, &np_ctx.timers[0], true, VLC_TICK_INVALID);
    np_ctx.timers_count = 1;

    /* Normal timer can't be tested reliably with audio as it will take the
     * lead */
    if (params->track_count[AUDIO_ES] == 0)
    {
        player_add_timer(player, &np_ctx.timers[1], false, VLC_TICK_INVALID);
        np_ctx.timers_count++;
    }

    player_start(ctx);

    /* Wait for a vout */
    {
        vec_on_vout_changed *vec = &ctx->report.on_vout_changed;
        while (vec->size == 0)
            vlc_player_CondWait(ctx->player, &ctx->wait);
        assert(vec->data[0].action == VLC_PLAYER_VOUT_STARTED);
    }

    /* Wait for a first frame */
    wait_next_tc(&np_ctx);

    /* Check that it behaves correctly when sending 1st request and next ones
     * in a burst */
    burst_unpaused(&np_ctx);

    vlc_player_SetTime(player, params->length / 2);

    /* Wait for a first frame */
    wait_type_timer(&np_ctx, REPORT_TIMER_SEEK);

    /* Wait for a first frame after seek */
    wait_next_tc(&np_ctx);

    vlc_player_NextVideoFrame(player);

    /* First request is pausing the video */
    wait_next_frame_status(&np_ctx, 1, -EAGAIN);
    assert_state(ctx, VLC_PLAYER_STATE_PAUSED);
    wait_type_timer(&np_ctx, REPORT_TIMER_PAUSED);

    /* Go EOF, via next-frame */
    go_eof(&np_ctx);

    /* Go start via prev-frame */
    go_start(&np_ctx, extra_checks);

    /* Check start of file a second time */
    vlc_player_PreviousVideoFrame(player);
    wait_prev_frame_status(&np_ctx, 1, -EAGAIN);

    if (extra_checks)
    {
        /* Check playback can be resumed */
        check_resumed_timer(&np_ctx);

        /* Go back to start */
        go_start(&np_ctx, false);

        /* Check we can seek again */
        check_seek_timer(&np_ctx);

        /* Go back to start */
        go_start(&np_ctx, false);
    }

    test_end(ctx);
    player_remove_timer(player, &np_ctx.timers[0]);
    if (np_ctx.timers_count > 1)
        player_remove_timer(player, &np_ctx.timers[1]);
}

static void
test_fail(struct ctx *ctx, const struct media_params *params, int error)
{
    test_log("prev-frame fail (can_seek: %d can_pause: %d)\n",
             params->can_seek, params->can_pause);

    vlc_player_t *player = ctx->player;

    player_set_current_mock_media(ctx, "media1", params, false);

    struct np_ctx np_ctx = {
        .ctx = ctx,
        .prev_status_idx = 0,
        .next_status_idx = 0,
    };
    player_add_timer(player, &np_ctx.timers[0], true, VLC_TICK_INVALID);
    np_ctx.timers_count = 1;
    struct timer_state *timer = &np_ctx.timers[0];

    player_start(ctx);

    {
        vec_on_vout_changed *vec = &ctx->report.on_vout_changed;
        while (vec->size == 0)
            vlc_player_CondWait(ctx->player, &ctx->wait);
        assert(vec->data[0].action == VLC_PLAYER_VOUT_STARTED);
    }

    struct report_timer *r = NULL;
    player_lock_timer(player, timer);
    r = timer_state_wait_next_report(timer);
    assert(r->type == REPORT_TIMER_TC);
    player_unlock_timer(player, timer);

    vlc_player_PreviousVideoFrame(player);

    wait_prev_frame_status(&np_ctx, 1, error);

    test_end(ctx);
    player_remove_timer(player, timer);
}

static void
test_vout_fail(struct ctx *ctx, const struct media_params *params)
{
    test_log("prev-frame fail (no vout)\n");

    vlc_player_t *player = ctx->player;

    player_set_current_mock_media(ctx, "media1", params, false);

    struct np_ctx np_ctx = {
        .ctx = ctx,
        .prev_status_idx = 0,
        .next_status_idx = 0,
    };
    player_add_timer(player, &np_ctx.timers[0], false, VLC_TICK_INVALID);
    np_ctx.timers_count = 1;
    struct timer_state *timer = &np_ctx.timers[0];

    player_start(ctx);

    struct report_timer *r = NULL;
    player_lock_timer(player, timer);
    r = timer_state_wait_next_report(timer);
    assert(r->type == REPORT_TIMER_POINT);
    player_unlock_timer(player, timer);

    vlc_player_PreviousVideoFrame(player);
    wait_prev_frame_status(&np_ctx, 1, -EAGAIN);

    vlc_player_PreviousVideoFrame(player);
    wait_prev_frame_status(&np_ctx, 1, -EBUSY);

    test_end(ctx);
    player_remove_timer(player, timer);
}

int
main(void)
{
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(5));
    params.track_count[VIDEO_ES] = 1;
    params.track_count[AUDIO_ES] = 1;
    params.track_count[SPU_ES] = 1;
    params.video_frame_rate_base = 1;

    struct ctx ctx;
    ctx_init(&ctx, 0);

    /* 25 fps + pts-delay lower than frame duration */
    params.pts_delay = VLC_TICK_FROM_MS(20);
    params.video_frame_rate = 25;
    test_prev(&ctx, &params, true);
    params.pts_delay = DEFAULT_PTS_DELAY;

    /* 29.97fps: Playing less than a minute so no drop frames */
    params.video_frame_rate = 30000;
    params.video_frame_rate_base = 1001;
    test_prev(&ctx, &params, true);
    params.video_frame_rate_base = 1;

    /* Now, disable audio to also test normal timer */
    params.track_count[AUDIO_ES] = 0;
    params.track_count[SPU_ES] = 0;

    /* 60 fps */
    params.video_frame_rate = 60;
    test_prev(&ctx, &params, true);

    /* 1 fps */
    params.length = VLC_TICK_FROM_SEC(20);
    params.video_frame_rate = 1;
    /* Don't do extra checks for 1fps, as the check_resumed_timer() might take
     * a longer time than expected due to a bug on the clock: resuming a 1fps
     * video after prev/frame next might result on using bad reference points. */
    test_prev(&ctx, &params, false);

    /* Fail because can't seek */
    params.video_frame_rate = 30;
    params.can_seek = false;
    test_fail(&ctx, &params, -ENOTSUP);
    params.can_seek = true;

    /* Fail because can't pause */
    params.can_pause = false;
    test_fail(&ctx, &params, -ENOTSUP);
    params.can_pause = true;

    ctx_destroy(&ctx);

    /* Fail because no vout */
    ctx_init(&ctx, DISABLE_VIDEO_OUTPUT);
    test_vout_fail(&ctx, &params);
    ctx_destroy(&ctx);

    return 0;
}
