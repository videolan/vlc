// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * capabilities.c: capabilities player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

static void
test_capabilities_seek(struct ctx *ctx)
{
    test_log("capabilites_seek\n");
    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(1));
    params.can_seek = false;
    player_set_next_mock_media(ctx, "media1", &params);

    player_start(ctx);

    {
        vec_on_capabilities_changed *vec = &ctx->report.on_capabilities_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
    }

    vlc_player_ChangeRate(player, 4.f);

    /* Ensure that seek back to 0 doesn't work */
    {
        vlc_tick_t last_time = 0;
        vec_on_state_changed *vec = &ctx->report.on_state_changed;
        while (vec->size == 0 || VEC_LAST(vec) != VLC_PLAYER_STATE_STOPPED)
        {
            vec_on_position_changed *posvec = &ctx->report.on_position_changed;
            if (posvec->size > 0 && last_time != VEC_LAST(posvec).time)
            {
                last_time = VEC_LAST(posvec).time;
                vlc_player_SetTime(player, 0);
            }
            vlc_player_CondWait(player, &ctx->wait);
        }
    }

    assert_state(ctx, VLC_PLAYER_STATE_STOPPED);
    test_end(ctx);
}

static void
test_capabilities_pause(struct ctx *ctx)
{
    test_log("capabilites_pause\n");
    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(1));
    params.can_pause = false;
    player_set_next_mock_media(ctx, "media1", &params);

    player_start(ctx);

    {
        vec_on_capabilities_changed *vec = &ctx->report.on_capabilities_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
    }

    /* Ensure that pause doesn't work */
    vlc_player_Pause(player);
    vlc_player_ChangeRate(player, 32.f);

    test_prestop(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);
    assert_normal_state(ctx);

    test_end(ctx);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_capabilities_seek(&ctx);
    test_capabilities_pause(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
