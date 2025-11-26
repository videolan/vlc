// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * es_selection.c: ES selection override player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

static void
test_es_selection_override(struct ctx *ctx)
{
    test_log("test_es_selection_override\n");

    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(2));
    params.track_count[VIDEO_ES] = 1;
    params.track_count[AUDIO_ES] = 1;
    params.track_count[SPU_ES] = 0;


    player_set_next_mock_media(ctx, "media1", &params);

    player_start(ctx);

    /* Wait that all tracks are added */
    {
        vec_on_track_list_changed *vec = &ctx->report.on_track_list_changed;
        while (vec_on_track_list_get_action_count(vec, VLC_PLAYER_LIST_ADDED)
               != 2)
            vlc_player_CondWait(player, &ctx->wait);
    }

    /* Wait that all tracks are selected */
    {
        vec_on_track_selection_changed *vec =
                &ctx->report.on_track_selection_changed;
        while (vec->size < 1)
            vlc_player_CondWait(player, &ctx->wait);
    }

    assert(vlc_player_GetTrackCount(player, VIDEO_ES) == 1);
    assert(vlc_player_GetTrackCount(player, AUDIO_ES) == 1);
    const struct vlc_player_track *track = vlc_player_GetTrackAt(player, AUDIO_ES, 0);
    assert(track);
    assert(track->selected);
    track = vlc_player_GetTrackAt(player, VIDEO_ES, 0);
    assert(track);
    assert(!track->selected);

    /* Select video track */
    vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
    {
        vec_on_track_selection_changed *vec =
                &ctx->report.on_track_selection_changed;
        while (vec->size < 2)
            vlc_player_CondWait(player, &ctx->wait);
    }
    track = vlc_player_GetTrackAt(player, VIDEO_ES, 0);
    assert(track);
    assert(track->selected);

    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);
    assert_normal_state(ctx);

    test_end(ctx);
}

int
main(void)
{
    struct ctx ctx;
    /* Test with --no-video */
    ctx_init(&ctx, DISABLE_VIDEO);
    test_es_selection_override(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
