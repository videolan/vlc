// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * pause.c: pause player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"
#include "timers.h"

static struct report_timer *
wait_timer_report(vlc_player_t *player, struct timer_state *timer,
                  unsigned type)
{
    struct report_timer *r_found = NULL;
    player_lock_timer(player, timer);
    for (;;)
    {
        struct report_timer *r = timer_state_wait_next_report(timer);
        if (r->type == type)
        {
            r_found = r;
            break;
        }
    }
    assert(r_found != NULL);
    player_unlock_timer(player, timer);
    return r_found;
}

static struct report_timer *
get_timer_report(vlc_player_t *player, struct timer_state *timer)
{
    player_lock_timer(player, timer);
    struct report_timer *r = &timer->vec.data[timer->vec.size -1];
    player_unlock_timer(player, timer);
    return r;
}

static void
test_pause(struct ctx *ctx)
{
    test_log("pause\n");
    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(10));
    player_set_next_mock_media(ctx, "media1", &params);
    struct timer_state video_timer, timer;
    player_add_timer(player, &video_timer, true, VLC_TICK_INVALID);
    player_add_timer(player, &timer, false, VLC_TICK_INVALID);

    /* Start paused */
    vlc_player_SetStartPaused(player, true);
    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_PAUSED);

    {
        vec_on_position_changed *vec = &ctx->report.on_position_changed;
        assert(vec->size == 0);
    }

    /* Ensure all timers are paused */
    wait_timer_report(player, &video_timer, REPORT_TIMER_PAUSED);
    wait_timer_report(player, &timer, REPORT_TIMER_PAUSED);

    /* Resume */
    vlc_player_Resume(player);

    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);

    {
        vec_on_position_changed *vec = &ctx->report.on_position_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
    }

    /* Ensure we got a video/audio point updated */
    wait_timer_report(player, &video_timer, REPORT_TIMER_TC);
    wait_timer_report(player, &timer, REPORT_TIMER_POINT);

    /* Pause again (while playing) */
    vlc_player_Pause(player);

    wait_state(ctx, VLC_PLAYER_STATE_PAUSED);

    /* Ensure all timers are paused */
    struct report_timer *r_video_paused, *r_paused;
    r_video_paused = wait_timer_report(player, &video_timer, REPORT_TIMER_PAUSED);
    r_paused = wait_timer_report(player, &timer, REPORT_TIMER_PAUSED);

    /* Ensure we stay paused */
    vlc_tick_sleep(VLC_TICK_FROM_MS(100));

    /* Ensure the last video timer report is the paused one (and that no ouput
     * are updated after) */
    assert(get_timer_report(player, &video_timer) == r_video_paused);
    assert(get_timer_report(player, &timer) == r_paused);

    test_end(ctx);
    player_remove_timer(player, &video_timer);
    player_remove_timer(player, &timer);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_pause(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
