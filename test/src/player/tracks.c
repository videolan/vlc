// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * tracks.c: tracks player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

static bool
player_select_next_unselected_track(struct ctx *ctx,
                                    enum es_format_category_e cat)
{
    vlc_player_t *player = ctx->player;

    const struct vlc_player_track *new_track = NULL;
    const struct vlc_player_track *old_track = NULL;
    bool has_selected_track = false;
    vlc_es_id_t *new_id, *old_id;

    /* Find the next track to select (selected +1) */
    const size_t count = vlc_player_GetTrackCount(player, cat);
    for (size_t i = 0; i < count; ++i)
    {
        old_track = vlc_player_GetTrackAt(player, cat, i);
        assert(old_track);
        if (old_track->selected)
        {
            has_selected_track = true;
            if (i + 1 != count)
                new_track = vlc_player_GetTrackAt(player, cat, i + 1);
            /* else: trigger UnselectTrack path */
            break;
        }
    }

    if (!has_selected_track)
    {
        /* subs are not selected by default */
        assert(cat == SPU_ES);
        old_track = NULL;
        new_track = vlc_player_GetTrackAt(player, cat, 0);
    }
    new_id = new_track ? vlc_es_id_Hold(new_track->es_id) : NULL;
    old_id = old_track ? vlc_es_id_Hold(old_track->es_id) : NULL;

    if (new_id)
        vlc_player_SelectEsId(player, new_id, VLC_PLAYER_SELECT_EXCLUSIVE);
    else
    {
        assert(old_id);
        vlc_player_UnselectEsId(player, old_id);
    }

    {
        vec_on_track_selection_changed *vec =
            &ctx->report.on_track_selection_changed;

        size_t vec_oldsize = vec->size;
        while (!vec_on_track_selection_has_event(vec, vec_oldsize, old_id,
                                                 new_id))
            vlc_player_CondWait(player, &ctx->wait);
    }
    if (new_id)
        vlc_es_id_Release(new_id);
    if (old_id)
        vlc_es_id_Release(old_id);

    return !!new_track;
}

static void
test_tracks(struct ctx *ctx, bool packetized)
{
    test_log("tracks (packetized: %d)\n", packetized);

    vlc_player_t *player = ctx->player;
    /* Long duration but this test doesn't wait for EOS */
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(100));
    params.track_count[VIDEO_ES] = 1;
    params.track_count[AUDIO_ES] = 9;
    params.track_count[SPU_ES] = 9;
    params.video_packetized = params.audio_packetized = params.sub_packetized
                            = packetized;
    player_set_next_mock_media(ctx, "media1", &params);
    const size_t track_count = params.track_count[VIDEO_ES] +
                               params.track_count[AUDIO_ES] +
                               params.track_count[SPU_ES];

    player_start(ctx);

    /* Wait that all tracks are added */
    {
        vec_on_track_list_changed *vec = &ctx->report.on_track_list_changed;
        while (vec_on_track_list_get_action_count(vec, VLC_PLAYER_LIST_ADDED)
               != track_count)
            vlc_player_CondWait(player, &ctx->wait);
    }

    /* Wait that video and audio are selected */
    {
        vec_on_track_selection_changed *vec =
            &ctx->report.on_track_selection_changed;
        while (vec->size != 2)
            vlc_player_CondWait(player, &ctx->wait);
        for (size_t i = 0; i < vec->size; ++i)
        {
            assert(!vec->data[i].unselected_id);
            assert(vec->data[i].selected_id);
            const struct vlc_player_track *track =
                vlc_player_GetTrack(player, vec->data[i].selected_id);
            assert(track);
            assert(track->fmt.i_cat == VIDEO_ES || track->fmt.i_cat == AUDIO_ES);
            assert(track == vlc_player_GetTrackAt(player, track->fmt.i_cat, 0));
        }
    }

    static const enum es_format_category_e cats[] = {
        SPU_ES, VIDEO_ES, AUDIO_ES /* Test SPU before the vout is disabled */
    };
    for (size_t i = 0; i < ARRAY_SIZE(cats); ++i)
    {
        /* Select every possible tracks with getters and setters */
        enum es_format_category_e cat = cats[i];
        assert(params.track_count[cat] == vlc_player_GetTrackCount(player, cat));
        while (player_select_next_unselected_track(ctx, cat));

        /* All tracks are unselected now */
        assert(vlc_player_GetSelectedTrack(player, cat) == NULL);

        if (cat == VIDEO_ES)
            continue;

        vec_on_track_selection_changed *vec =
            &ctx->report.on_track_selection_changed;
        size_t vec_oldsize = vec->size;

        /* Select all track via next calls */
        for (size_t j = 0; j < params.track_count[cat]; ++j)
        {
            vlc_player_SelectNextTrack(player, cat, VLC_VOUT_ORDER_PRIMARY);

            /* Wait that the next track is selected */
            const struct vlc_player_track *track =
                vlc_player_GetTrackAt(player, cat, j);
            while (!vec_on_track_selection_has_event(vec, vec_oldsize,
                                                     NULL, track->es_id))
                vlc_player_CondWait(player, &ctx->wait);
            vec_oldsize = vec->size;
        }

        /* Select all track via previous calls */
        for (size_t j = params.track_count[cat] - 1; j > 0; --j)
        {
            vlc_player_SelectPrevTrack(player, cat, VLC_VOUT_ORDER_PRIMARY);

            const struct vlc_player_track *track =
                vlc_player_GetTrackAt(player, cat, j - 1);

            /* Wait that the previous track is selected */
            while (!vec_on_track_selection_has_event(vec, vec_oldsize,
                                                     NULL, track->es_id))
                vlc_player_CondWait(player, &ctx->wait);
            vec_oldsize = vec->size;

        }
        /* Current track index is 0, a previous will unselect the track */
        vlc_player_SelectPrevTrack(player, cat, VLC_VOUT_ORDER_PRIMARY);
        const struct vlc_player_track *track =
            vlc_player_GetTrackAt(player, cat, 0);
        /* Wait that the track is unselected */
        while (!vec_on_track_selection_has_event(vec, vec_oldsize,
                                                 track->es_id, NULL))
            vlc_player_CondWait(player, &ctx->wait);

        assert(vlc_player_GetSelectedTrack(player, cat) == NULL);
    }

    test_prestop(ctx);
    test_end(ctx);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_tracks(&ctx, true);
    test_tracks(&ctx, false);
    ctx_destroy(&ctx);
    return 0;
}
