// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * tracks_ids.c: tracks IDs player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

static void
test_tracks_ids(struct ctx *ctx)
{
    test_log("tracks_ids\n");

    vlc_player_t *player = ctx->player;

    /* Long duration but this test doesn't wait for EOS */
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(100));
    params.track_count[VIDEO_ES] = 9;
    params.track_count[AUDIO_ES] = 9;
    params.track_count[SPU_ES] = 9;
    const size_t track_count = params.track_count[VIDEO_ES] +
                               params.track_count[AUDIO_ES] +
                               params.track_count[SPU_ES];
    player_set_current_mock_media(ctx, "media1", &params, false);

    /*
     * Test that tracks can be set before the player is started
     */

    unsigned selected_video_tracks = 3;
    unsigned first_video_track_idx = 4;
    vlc_player_SelectTracksByStringIds(player, VIDEO_ES,
                                       "video/4,video/5,video/6");

    unsigned selected_audio_tracks = 0;
    unsigned first_audio_track_idx = 0;
    vlc_player_SelectTracksByStringIds(player, AUDIO_ES, "invalid");

    unsigned selected_sub_tracks = 2;
    unsigned first_sub_track_idx = 0;
    vlc_player_SelectTracksByStringIds(player, SPU_ES, "spu/0,spu/1");

    unsigned selected_tracks = selected_video_tracks + selected_audio_tracks
                             + selected_sub_tracks;

    player_start(ctx);

    /* Wait that all tracks are added */
    {
        vec_on_track_list_changed *vec = &ctx->report.on_track_list_changed;
        while (vec_on_track_list_get_action_count(vec, VLC_PLAYER_LIST_ADDED)
               != track_count)
            vlc_player_CondWait(player, &ctx->wait);
    }

    /* Wait that video and spu are selected */
    {
        unsigned video_track_idx = first_video_track_idx;
        unsigned sub_track_idx = first_sub_track_idx;
        char cat_id[] = "video/0";

        vec_on_track_selection_changed *vec =
            &ctx->report.on_track_selection_changed;
        while (vec->size != selected_tracks)
            vlc_player_CondWait(player, &ctx->wait);
        for (size_t i = 0; i < vec->size; ++i)
        {
            assert(!vec->data[i].unselected_id);
            assert(vec->data[i].selected_id);

            vlc_es_id_t *es_id = vec->data[i].selected_id;
            const struct vlc_player_track *track =
                vlc_player_GetTrack(player, es_id);
            assert(track);
            assert(track->fmt.i_cat != AUDIO_ES);
            if (track->fmt.i_cat == VIDEO_ES)
            {
                assert(video_track_idx < 10);
                sprintf(cat_id, "video/%u", video_track_idx++);
            }
            else
            {
                assert(sub_track_idx < 10);
                sprintf(cat_id, "spu/%u", sub_track_idx++);
            }
            assert(strcmp(vlc_es_id_GetStrId(es_id), cat_id) == 0);
        }
    }

    /*
     * Test that tracks can be set/unset during playback
     */

    /* Should remove the track preferences but not disable the current tracks */
    selected_video_tracks = 0;
    vlc_player_SelectTracksByStringIds(player, VIDEO_ES, NULL);
    /* Should select the first track */
    selected_audio_tracks = 1;
    first_audio_track_idx = 1;
    vlc_player_SelectTracksByStringIds(player, AUDIO_ES, "audio/1");
    /* Should disable all tracks */
    vlc_player_SelectTracksByStringIds(player, SPU_ES, "");

    unsigned new_selected_tracks = selected_tracks +
                                 + selected_video_tracks + selected_audio_tracks
                                 + selected_sub_tracks;

    /* Wait for the new selection */
    {
        unsigned audio_track_idx = first_audio_track_idx;
        unsigned sub_track_idx = first_sub_track_idx;
        char cat_id[] = "audio/0";

        vec_on_track_selection_changed *vec =
            &ctx->report.on_track_selection_changed;
        while (vec->size != new_selected_tracks)
            vlc_player_CondWait(player, &ctx->wait);
        for (size_t i = selected_tracks; i < vec->size; ++i)
        {
            vlc_es_id_t *es_id = vec->data[i].unselected_id ?
                vec->data[i].unselected_id : vec->data[i].selected_id;
            const struct vlc_player_track *track =
                vlc_player_GetTrack(player, es_id);
            assert(track);

            assert(track->fmt.i_cat != VIDEO_ES);

            if (track->fmt.i_cat == SPU_ES)
            {
                assert(vec->data[i].unselected_id);
                assert(!vec->data[i].selected_id);
            }
            else
            {
                assert(track->fmt.i_cat == AUDIO_ES);
                assert(!vec->data[i].unselected_id);
                assert(vec->data[i].selected_id);
            }

            if (track->fmt.i_cat == AUDIO_ES)
            {
                assert(audio_track_idx < 10);
                sprintf(cat_id, "audio/%u", audio_track_idx++);
            }
            else
            {
                assert(sub_track_idx < 10);
                sprintf(cat_id, "spu/%u", sub_track_idx++);
            }
            assert(strcmp(vlc_es_id_GetStrId(es_id), cat_id) == 0);
        }
    }

    test_prestop(ctx);
    test_end(ctx);

    /*
     * Test that tracks preference are reset for the next media
     */

    player_set_next_mock_media(ctx, "media1", &params);

    player_start(ctx);

    /* Wait that all tracks are added */
    {
        vec_on_track_list_changed *vec = &ctx->report.on_track_list_changed;
        while (vec_on_track_list_get_action_count(vec, VLC_PLAYER_LIST_ADDED)
               != track_count)
            vlc_player_CondWait(player, &ctx->wait);
    }

    /* Wait for the new selection: video/0 and audio/0 */
    {
        unsigned video_track_idx = 0;
        unsigned audio_track_idx = 0;
        char cat_id[] = "audio/0";

        vec_on_track_selection_changed *vec =
            &ctx->report.on_track_selection_changed;
        while (vec->size != 2)
            vlc_player_CondWait(player, &ctx->wait);

        for (size_t i = 0; i < vec->size; ++i)
        {
            assert(!vec->data[i].unselected_id);
            assert(vec->data[i].selected_id);

            vlc_es_id_t *es_id = vec->data[i].selected_id;
            const struct vlc_player_track *track =
                vlc_player_GetTrack(player, es_id);
            assert(track);
            assert(track->fmt.i_cat != SPU_ES);
            if (track->fmt.i_cat == VIDEO_ES)
            {
                assert(video_track_idx < 1);
                sprintf(cat_id, "video/%u", video_track_idx++);
            }
            else
            {
                assert(audio_track_idx < 1);
                sprintf(cat_id, "audio/%u", audio_track_idx++);
            }
            assert(strcmp(vlc_es_id_GetStrId(es_id), cat_id) == 0);
        }
    }

    test_prestop(ctx);
    test_end(ctx);
}

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_tracks_ids(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
