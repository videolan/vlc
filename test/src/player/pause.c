// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * pause.c: pause player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

static void
test_pause(struct ctx *ctx)
{
    test_log("pause\n");
    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(10));
    player_set_next_mock_media(ctx, "media1", &params);

    /* Start paused */
    vlc_player_SetStartPaused(player, true);
    player_start(ctx);
    {
        vec_on_state_changed *vec = &ctx->report.on_state_changed;
        while (vec->size == 0 || VEC_LAST(vec) != VLC_PLAYER_STATE_PAUSED)
            vlc_player_CondWait(player, &ctx->wait);
        assert(vec->size == 3);
        assert(vec->data[0] == VLC_PLAYER_STATE_STARTED);
        assert(vec->data[1] == VLC_PLAYER_STATE_PLAYING);
        assert(vec->data[2] == VLC_PLAYER_STATE_PAUSED);
    }

    {
        vec_on_position_changed *vec = &ctx->report.on_position_changed;
        assert(vec->size == 0);
    }

    /* Resume */
    vlc_player_Resume(player);

    {
        vec_on_state_changed *vec = &ctx->report.on_state_changed;
        while (VEC_LAST(vec) != VLC_PLAYER_STATE_PLAYING)
            vlc_player_CondWait(player, &ctx->wait);
        assert(vec->size == 4);
    }

    {
        vec_on_position_changed *vec = &ctx->report.on_position_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
    }

    /* Pause again (while playing) */
    vlc_player_Pause(player);

    {
        vec_on_state_changed *vec = &ctx->report.on_state_changed;
        while (VEC_LAST(vec) != VLC_PLAYER_STATE_PAUSED)
            vlc_player_CondWait(player, &ctx->wait);
        assert(vec->size == 5);
    }

    test_end(ctx);
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
