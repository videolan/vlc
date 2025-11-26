
// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * discontinuities.c: clock discontinuities player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

static void
test_clock_discontinuities(struct ctx *ctx)
{
    test_log("discontinuities%s\n",
        (ctx->flags & CLOCK_MASTER_MONOTONIC) ? " (monotonic)" : "");

    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(20));
    params.pts_delay = VLC_TICK_FROM_MS(50);
    params.discontinuities = "(400000,2)(400000,2500000)(3000000,10000000)";
    player_set_next_mock_media(ctx, "media1", &params);

    player_start(ctx);

    vec_on_aout_first_pts *vec = &ctx->report.on_aout_first_pts;
    while (vec->size != 4)
        vlc_player_CondWait(player, &ctx->wait);

    assert(vec->data[0] == VLC_TICK_0); /* Initial PTS */
    assert(vec->data[1] == VLC_TICK_0 + 2); /* 1st discontinuity */
    assert(vec->data[2] == VLC_TICK_0 + 2500000); /* 2nd discontinuity */
    assert(vec->data[3] == VLC_TICK_0 + 10000000); /* 3rd discontinuity */

    test_end(ctx);
}

int
main(void)
{
    struct ctx ctx;
    /* Test with instantaneous audio drain */
    ctx_init(&ctx, AUDIO_INSTANT_DRAIN);
    test_clock_discontinuities(&ctx);
    ctx_destroy(&ctx);
    ctx_init(&ctx, CLOCK_MASTER_MONOTONIC|AUDIO_INSTANT_DRAIN);
    test_clock_discontinuities(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
