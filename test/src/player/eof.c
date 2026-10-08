// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * eof.c: EOF player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

static void
test_play_pause(struct ctx *ctx)
{
    test_log("play_pause\n");

    vlc_player_t *player = ctx->player;
    vlc_player_SetPlayAndPause(player, true);

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_MS(100));
    player_set_next_mock_media(ctx, "media1", &params);
    player_start(ctx);

    /* Check the PlayAndPause if pausing at EOF */
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);
    wait_state(ctx, VLC_PLAYER_STATE_PAUSED);

    vlc_player_SetTime(player, 0);
    vlc_player_Resume(player);

    /* Check we can resume the playback and pause again */
    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);
    wait_state(ctx, VLC_PLAYER_STATE_PAUSED);

    /* Check we stay paused */
    vlc_tick_sleep(VLC_TICK_FROM_MS(100));
    assert_state(ctx, VLC_PLAYER_STATE_PAUSED);
    test_prestop(ctx);

    vlc_player_Stop(player);

    test_end(ctx);
}

static void
test_repeat(struct ctx *ctx)
{
    test_log("repeat\n");

    vlc_player_t *player = ctx->player;
    const unsigned repeat_count = 3;
    vlc_player_SetRepeatCount(player, repeat_count);

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_MS(100));
    player_set_next_mock_media(ctx, "media1", &params);
    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    /* Check buffering count match the repeat count */
    assert(get_buffering_count(ctx) ==  repeat_count + 1 /* initial buffering */);

    test_end(ctx);
}

static void
test_audio_end(struct ctx *ctx, float rate)
{
    test_log("audio_end: rate %f\n", rate);

    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(1));
    params.track_count[VIDEO_ES] = 0;
    params.track_count[SPU_ES] = 0;
    player_set_next_mock_media(ctx, "media1", &params);
    player_set_rate(ctx, rate);
    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    /* The test aout has no drain callback: EOF waits for the play date of its
     * last sample, computed by the core from two clock reads. */
    vec_on_aout_stop *vec = &ctx->report.on_aout_stop;
    assert(vec->size > 0);
    struct report_aout_stop report = VEC_LAST(vec);
    assert(report.date >= report.end_date - VLC_TICK_FROM_MS(10));

    test_end(ctx);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_play_pause(&ctx);
    test_repeat(&ctx);
    test_audio_end(&ctx, 2.f);
    test_audio_end(&ctx, 4.f);
    ctx_destroy(&ctx);
    return 0;
}
