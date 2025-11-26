// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * errors.c: error handling player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

static void
test_error(struct ctx *ctx)
{
    test_log("error\n");
    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(1));
    params.error = true;
    player_set_next_mock_media(ctx, "media1", &params);

    player_start(ctx);

    {
        vec_on_error_changed *vec = &ctx->report.on_error_changed;
        while (vec->size == 0 || VEC_LAST(vec) == VLC_PLAYER_ERROR_NONE)
            vlc_player_CondWait(player, &ctx->wait);
    }
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    test_end(ctx);
}

static void
test_unknown_uri(struct ctx *ctx)
{
    test_log("unknown_uri\n");
    vlc_player_t *player = ctx->player;

    input_item_t *media = input_item_New("unknownuri://foo", "fail");
    assert(media);
    int ret = vlc_player_SetCurrentMedia(player, media);
    assert(ret == VLC_SUCCESS);

    ctx->params.error = true;
    bool success = vlc_vector_push(&ctx->added_medias, media);
    assert(success);
    success = vlc_vector_push(&ctx->played_medias, media);
    assert(success);

    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STARTED);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);
    {
        vec_on_error_changed *vec = &ctx->report.on_error_changed;
        assert(vec->size == 1);
        assert(vec->data[0] != VLC_PLAYER_ERROR_NONE);
    }

    test_end(ctx);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_error(&ctx);
    test_unknown_uri(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
