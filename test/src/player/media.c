// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * media.c: media-related player tests
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

#define assert_media_name(media, name) do { \
    assert(media); \
    char *media_name = input_item_GetName(media); \
    assert(media_name && strcmp(media_name, name) == 0); \
    free(media_name); \
} while(0)

static void
test_next_media(struct ctx *ctx)
{
    test_log("next_media\n");
    const char *media_names[] = { "media1", "media2", "media3" };
    const size_t media_count = ARRAY_SIZE(media_names);

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_MS(100));

    /* This media should be overiden by the first call of
     * player_set_next_mock_media() and no events should be sent regarding this
     * media. */
    input_item_t *media = create_mock_media("ignored", &params);
    assert(media);
    vlc_player_SetNextMedia(ctx->player, media);

    /* Check vlc_player_GetNextMedia() */
    assert(vlc_player_GetNextMedia(ctx->player) == media);
    input_item_Release(media);

    for (size_t i = 0; i < media_count; ++i)
        player_set_next_mock_media(ctx, media_names[i], &params);
    player_set_rate(ctx, 4.f);
    player_start(ctx);

    test_prestop(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);
    assert_normal_state(ctx);

    {
        vec_on_current_media_changed *vec = &ctx->report.on_current_media_changed;

        assert(vec->size == media_count);
        assert(ctx->next_medias.size == 0);
        for (size_t i = 0; i < ctx->played_medias.size; ++i)
            assert_media_name(vec->data[i], media_names[i]);
    }

    test_end(ctx);
}

static void
test_same_media(struct ctx *ctx)
{
    test_log("same_media\n");

    vlc_player_t *player = ctx->player;
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_MS(10));

    player_set_current_mock_media(ctx, "media", &params, false);

    input_item_t *media = vlc_player_GetCurrentMedia(player);
    assert(media);

    input_item_Hold(media);
    vlc_player_SetCurrentMedia(player, media);
    bool success = vlc_vector_push(&ctx->added_medias, media);
    assert(success);

    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STARTED);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    test_end(ctx);
}

static void
test_media_stopped(struct ctx *ctx)
{
    test_log("media_stopped\n");

    vlc_player_t *player = ctx->player;
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_MS(10));

    /* This test checks for player_start() success, and doesn't care about the
     * stopped state. */
    //params.error = true;

    player_set_current_mock_media(ctx, "media1", &params, true);
    player_set_current_mock_media(ctx, "media2", &params, false);

    {
        vec_on_current_media_changed *vec = &ctx->report.on_current_media_changed;
        while (vec->size != 2)
            vlc_player_CondWait(player, &ctx->wait);
    }
    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STARTED);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    test_end(ctx);
}

static void
test_set_current_media(struct ctx *ctx)
{
    test_log("current_media\n");
    const char *media_names[] = { "media1", "media2", "media3" };
    const size_t media_count = ARRAY_SIZE(media_names);

    vlc_player_t *player = ctx->player;
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_MS(100));

    /* Ensure that this media is not played */
    player_set_current_mock_media(ctx, "ignored", &params, true);

    player_set_current_mock_media(ctx, media_names[0], &params, false);
    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_PLAYING);

    /* Call vlc_player_SetCurrentMedia for the remaining medias interrupting
     * the player and without passing by the next_media provider. */
    {
        vec_on_current_media_changed *vec = &ctx->report.on_current_media_changed;
        assert(vec->size == 2);

        for (size_t i = 1; i <= media_count; ++i)
        {
            while (vec->size - 1 /* ignored */!= i)
                vlc_player_CondWait(player, &ctx->wait);

            input_item_t *last_media = VEC_LAST(vec);
            assert(last_media);
            assert(last_media == vlc_player_GetCurrentMedia(player));
            assert(last_media == VEC_LAST(&ctx->added_medias));
            assert_media_name(last_media, media_names[i - 1]);

            if (i < media_count)
            {
                /* Next vlc_player_SetCurrentMedia() call should be
                 * asynchronous since we are still playing. Therefore,
                 * vlc_player_GetCurrentMedia() should return the last one. */
                input_item_t *ignored = create_mock_media("ignored", &params);
                assert(ignored);
                int ret = vlc_player_SetCurrentMedia(ctx->player, ignored);
                assert(ret == VLC_SUCCESS);
                assert(vlc_player_GetCurrentMedia(player) == last_media);
                input_item_Release(ignored);

                /* The previous media is ignored due to this call */
                player_set_current_mock_media(ctx, media_names[i], &params, false);
            }
        }
    }

    test_prestop(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);
    assert_normal_state(ctx);

    /* Test that the player can be played again with the same media */
    player_start(ctx);
    ctx->extra_start_count++; /* Since we play the same media  */

    /* Current state is already stopped, wait first for started then */
    wait_state(ctx, VLC_PLAYER_STATE_STARTED);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    assert_normal_state(ctx);

    /* Playback is stopped: vlc_player_SetCurrentMedia should be synchronous */
    player_set_current_mock_media(ctx, media_names[0], &params, false);
    assert(vlc_player_GetCurrentMedia(player) == VEC_LAST(&ctx->added_medias));

    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STARTED);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    test_end(ctx);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_same_media(&ctx);
    test_media_stopped(&ctx);
    test_set_current_media(&ctx);
    test_next_media(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
