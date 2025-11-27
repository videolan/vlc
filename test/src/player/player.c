/*****************************************************************************
 * player.c: test vlc_player_t API
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#include "common.h"
#include "timers.h"

static void
test_programs(struct ctx *ctx)
{
    test_log("programs\n");

    vlc_player_t *player = ctx->player;
    /* Long duration but this test doesn't wait for EOS */
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(100));
    params.program_count = 3;
    player_set_next_mock_media(ctx, "media1", &params);

    player_start(ctx);

    {
        vec_on_program_list_changed *vec = &ctx->report.on_program_list_changed;
        while (vec_on_program_list_get_action_count(vec, VLC_PLAYER_LIST_ADDED)
               != params.program_count)
            vlc_player_CondWait(player, &ctx->wait);
    }
    assert(vlc_player_GetProgramCount(player) == params.program_count);

    /* Select every programs ! */
    while (true)
    {
        const struct vlc_player_program *new_prgm = NULL;
        const struct vlc_player_program *old_prgm;
        for (size_t i = 0; i < params.program_count; ++i)
        {
            old_prgm = vlc_player_GetProgramAt(player, i);
            assert(old_prgm);
            assert(old_prgm == vlc_player_GetProgram(player, old_prgm->group_id));
            if (old_prgm->selected)
            {
                if (i + 1 != params.program_count)
                    new_prgm = vlc_player_GetProgramAt(player, i + 1);
                break;
            }
        }
        if (!new_prgm)
            break;
        const int old_id = old_prgm->group_id;
        const int new_id = new_prgm->group_id;
        vlc_player_SelectProgram(player, new_id);

        vec_on_program_selection_changed *vec =
            &ctx->report.on_program_selection_changed;

        size_t vec_oldsize = vec->size;
        while (!vec_on_program_selection_has_event(vec, vec_oldsize, old_id,
                                                   new_id))
            vlc_player_CondWait(player, &ctx->wait);
        ctx->program_switch_count++; /* test_end_poststop_tracks check */
    }

    test_prestop(ctx);
    test_end(ctx);
}

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
static void
test_titles(struct ctx *ctx, bool null_names)
{
    test_log("titles (null_names: %d)\n", null_names);
    vlc_player_t *player = ctx->player;

    /* Long duration but this test seeks to the last chapter, so the test
     * duration is 100sec / 2000 */
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(100));
    params.title_count = 5;
    params.chapter_count = 2000;
    params.null_names = null_names;
    player_set_next_mock_media(ctx, "media1", &params);

    player_start(ctx);

    /* Wait for the title list */
    vlc_player_title_list *titles;
    {
        vec_on_titles_changed *vec = &ctx->report.on_titles_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
        titles = vec->data[0];
        assert(titles != NULL && titles == vlc_player_GetTitleList(player));
    }

    /* Select a new title and a new chapter */
    const size_t last_chapter_idx = params.chapter_count - 1;
    {
        vec_on_title_selection_changed *vec =
            &ctx->report.on_title_selection_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
        assert(vec->data[0] == 0);

        const struct vlc_player_title *title =
            vlc_player_title_list_GetAt(titles, 4);
        vlc_player_SelectTitle(player, title);

        while (vec->size == 1)
            vlc_player_CondWait(player, &ctx->wait);
        assert(vec->data[1] == 4);

        assert(title->chapter_count == params.chapter_count);
        vlc_player_SelectChapter(player, title, last_chapter_idx);
    }

    /* Wait for the chapter selection */
    {
        vec_on_chapter_selection_changed *vec =
            &ctx->report.on_chapter_selection_changed;

        while (vec->size == 0 || VEC_LAST(vec).chapter_idx != last_chapter_idx)
            vlc_player_CondWait(player, &ctx->wait);
        assert(VEC_LAST(vec).title_idx == 4);
    }

    test_prestop(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);
    assert_normal_state(ctx);
    test_end(ctx);
}

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

static void
test_seeks(struct ctx *ctx)
{
    test_log("seeks\n");
    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(10));
    player_set_current_mock_media(ctx, "media1", &params, false);

    /* only the last one will be taken into account before start */
    vlc_player_SetTimeFast(player, 0);
    vlc_player_SetTimeFast(player, VLC_TICK_FROM_SEC(100));
    vlc_player_SetTimeFast(player, 10);

    vlc_tick_t seek_time = VLC_TICK_FROM_SEC(5);
    vlc_player_SetTimeFast(player, seek_time);
    player_start(ctx);

    {
        vec_on_position_changed *vec = &ctx->report.on_position_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);

        assert(VEC_LAST(vec).time >= seek_time);
        assert_position(ctx, &VEC_LAST(vec));

        vlc_tick_t last_time = VEC_LAST(vec).time;

        vlc_tick_t jump_time = -VLC_TICK_FROM_SEC(2);
        vlc_player_JumpTime(player, jump_time);

        while (VEC_LAST(vec).time >= last_time)
            vlc_player_CondWait(player, &ctx->wait);

        assert(VEC_LAST(vec).time >= last_time + jump_time);
        assert_position(ctx, &VEC_LAST(vec));
    }

    vlc_player_SetPosition(player, 2.0f);

    test_prestop(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);
    assert_normal_state(ctx);

    test_end(ctx);
}

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

static void
test_delete_while_playback(vlc_object_t *obj, bool start)
{
    test_log("delete_while_playback (start: %d)\n", start);
    vlc_player_t *player = vlc_player_New(obj, VLC_PLAYER_LOCK_NORMAL);

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(10));
    input_item_t *media = create_mock_media("media1", &params);
    assert(media);

    vlc_player_Lock(player);
    int ret = vlc_player_SetCurrentMedia(player, media);
    assert(ret == VLC_SUCCESS);
    input_item_Release(media);

    if (start)
    {
        ret = vlc_player_Start(player);
        assert(ret == VLC_SUCCESS);
    }

    vlc_player_Unlock(player);

    vlc_player_Delete(player);
}

static void
test_no_outputs(struct ctx *ctx)
{
    test_log("test_no_outputs\n");
    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_MS(10));
    player_set_current_mock_media(ctx, "media1", &params, false);
    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STOPPING);
    {
        vec_on_vout_changed *vec = &ctx->report.on_vout_changed;
        assert(vec->size == 0);
    }

    audio_output_t *aout = vlc_player_aout_Hold(player);
    assert(!aout);

    test_end(ctx);
}

static void
test_outputs(struct ctx *ctx)
{
    test_log("test_outputs\n");
    vlc_player_t *player = ctx->player;

    /* Test that the player has a valid aout and vout, even before first
     * playback */
    audio_output_t *aout = vlc_player_aout_Hold(player);
    assert(aout);

    vout_thread_t *vout = vlc_player_vout_Hold(player);
    assert(vout);

    size_t vout_count;
    vout_thread_t **vout_list = vlc_player_vout_HoldAll(player, &vout_count);
    assert(vout_count == 1 && vout_list[0] == vout);
    vout_Release(vout_list[0]);
    free(vout_list);
    vout_Release(vout);

    /* Test that the player keep the same aout and vout during playback */
    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_MS(10));

    player_set_current_mock_media(ctx, "media1", &params, false);
    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STOPPING);

    {
        vec_on_vout_changed *vec = &ctx->report.on_vout_changed;
        assert(vec->size >= 1);
        assert(vec->data[0].action == VLC_PLAYER_VOUT_STARTED);

        vout_thread_t *same_vout = vlc_player_vout_Hold(player);
        assert(vec->data[0].vout == same_vout);
        vout_Release(same_vout);
    }

    audio_output_t *same_aout = vlc_player_aout_Hold(player);
    assert(same_aout == aout);
    aout_Release(same_aout);

    aout_Release(aout);
    test_end(ctx);
}

static void
test_timers_assert_smpte(struct timer_state *timer,
                         vlc_tick_t duration, unsigned fps, bool drop_frame,
                         unsigned frame_resolution)
{
    /* This test doesn't support drop frame handling */
    assert(duration < VLC_TICK_FROM_SEC(60));

    vec_report_timer *vec = &timer->vec;

    /* Check that we didn't miss any update points */
    assert(vec->data[0].tc.frames == 0);
    for (size_t i = 0; i < vec->size; ++i)
    {
        struct report_timer *prev_report = i > 0 ? &vec->data[i - 1] : NULL;
        struct report_timer *report = &vec->data[i];

        assert(report->tc.seconds == (i / fps));
        if (prev_report)
        {
            if (i % fps == 0)
            {
                assert(prev_report->tc.frames == fps - 1);
                assert(report->tc.frames == 0);
            }
            else
                assert(report->tc.frames == prev_report->tc.frames + 1);
        }

        assert(report->type == REPORT_TIMER_TC);
        assert(report->tc.drop_frame == drop_frame);
        assert(report->tc.frame_resolution == frame_resolution);
    }
    assert(VEC_LAST(vec).tc.frames + 1 == fps * duration / VLC_TICK_FROM_SEC(1));
}

static void
test_timers_assert_smpte_dropframe(struct timer_state *timer, unsigned minute,
                                   unsigned fps)
{
    assert(fps == 30 || fps == 60);
    assert(minute > 0);

    vec_report_timer *vec = &timer->vec;

    bool last_second_seen = false, minute_seen = false;
    for (size_t i = 1; i < vec->size; ++i)
    {
        struct report_timer *prev_report = &vec->data[i - 1];
        struct report_timer *report = &vec->data[i];

        assert(report->tc.drop_frame == true);
        assert(report->tc.frame_resolution == 2);

        if (prev_report->tc.frames == fps - 1)
        {
            if (report->tc.seconds == 59)
            {
                /* Last second before the new minute */
                assert(prev_report->tc.minutes == minute - 1);
                assert(prev_report->tc.seconds == 58);

                assert(report->tc.minutes == minute - 1);
                assert(report->tc.frames == 0);

                last_second_seen = true;
            }
            else if (report->tc.seconds == 0)
            {
                /* The minute just reached, check that 2 or 4 frames are
                 * dropped every minutes, except every 10 minutes */

                assert(prev_report->tc.minutes == minute - 1);
                assert(prev_report->tc.seconds == 59);

                assert(report->tc.minutes == minute);
                if (minute % 10 == 0)
                    assert(report->tc.frames == 0);
                else
                    assert(report->tc.frames == (fps / 30 * 2) /* drop frame */);

                minute_seen = true;
            }

        }
        else if (prev_report->tc.minutes != 0 && prev_report->tc.seconds != 0
              && prev_report->tc.frames != 0)
            assert(report->tc.frames == prev_report->tc.frames + 1);
    }

    /* Assert that we've seen the full last second and the new minute */
    assert(last_second_seen && minute_seen);
}

#define REGULAR_TIMER_IDX 0
#define REGULAR_DELAY_TIMER_IDX 1
#define SMPTE_TIMER_IDX 2
#define TIMER_COUNT 3
#define SOURCE_DELAY_TIMER_VALUE (VLC_TICK_FROM_MS(2))

static void
test_timers_playback(struct ctx *ctx, struct timer_state timers[],
                     size_t track_count, vlc_tick_t length, unsigned fps,
                     unsigned rate)
{
#define SAMPLE_LENGTH VLC_TICK_FROM_MS(1)
#define MAX_UPDATE_COUNT (size_t)(length / SAMPLE_LENGTH)

    struct media_params params = DEFAULT_MEDIA_PARAMS(length);

    params.track_count[VIDEO_ES] = track_count;
    params.track_count[AUDIO_ES] = track_count;
    params.track_count[SPU_ES] = track_count;
    params.audio_sample_length = SAMPLE_LENGTH;
    params.video_frame_rate = fps;
    params.video_frame_rate_base = 1;

    player_set_current_mock_media(ctx, "media1", &params, false);
    player_set_rate(ctx, rate);
    player_start(ctx);

    wait_state(ctx, VLC_PLAYER_STATE_STARTED);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    /* Common for regular timers */
    for (size_t timer_idx = 0; timer_idx < SMPTE_TIMER_IDX; ++timer_idx)
    {
        struct timer_state *timer = &timers[timer_idx];
        vec_report_timer *vec = &timer->vec;

        assert(vec->size > 1);

        for (size_t i = 1; i < vec->size; ++i)
        {
            struct report_timer *prev_report = &vec->data[i - 1];
            struct report_timer *report = &vec->data[i];

            /* Only the last event should be a discontinuity. We can assume
             * that since we are not seeking and playing a fake content */
            if (i < vec->size - 1)
            {
                if (i == 1)
                    assert(prev_report->point.system_date == INT64_MAX);

                assert(report->type == REPORT_TIMER_POINT);
                /* ts/position should increase, rate should stay to 1.f */
                assert(report->point.ts >= prev_report->point.ts);
                assert(report->point.system_date != VLC_TICK_INVALID);
                assert(report->point.position >= prev_report->point.position);
                assert(report->point.rate == rate);
                assert(report->point.length == length);
            }
            else
            {
                assert(report->type == REPORT_TIMER_PAUSED);
                assert(report->paused_date == VLC_TICK_INVALID);
            }
        }
    }

    /* If there is no master source, we can't known which sources (audio or
     * video) will feed the timer. Indeed the first source that trigger a clock
     * update will be used as a timer source (and audio/video goes through
     * decoder threads and output threads, adding more uncertainty). */
    if ((ctx->flags & CLOCK_MASTER_MONOTONIC) == 0)
    {
        /* Assertions for the regular timer that received all update points */
        if (track_count != 0)
        {
            struct timer_state *timer = &timers[REGULAR_TIMER_IDX];
            vec_report_timer *vec = &timer->vec;

            /* Check that we didn't miss any update points */
            assert(vec->size > 1);
            size_t point_count = 1;
            for (size_t i = 1; i < vec->size - 1; ++i)
            {
                struct report_timer *prev_report = &vec->data[i - 1];
                struct report_timer *report = &vec->data[i];

                /* Don't count forced points */
                if (report->point.ts != prev_report->point.ts)
                {
                    assert(report->point.ts ==
                           prev_report->point.ts + SAMPLE_LENGTH);
                    point_count++;
                }
            }
            assert(vec->data[vec->size - 2].point.ts
                == length - SAMPLE_LENGTH + VLC_TICK_0);
            assert(point_count == MAX_UPDATE_COUNT);
        }

        /* Assertions for the regular filtered timer */
        {
            struct timer_state *timer = &timers[REGULAR_DELAY_TIMER_IDX];
            vec_report_timer *vec = &timer->vec;

            /* It should not receive all update points */
            assert(vec->size < MAX_UPDATE_COUNT);
            assert(vec->size > 1);

            for (size_t i = 1; i < vec->size; ++i)
            {
                struct report_timer *prev_report = &vec->data[i - 1];
                struct report_timer *report = &vec->data[i];
                if (i < vec->size - 1)
                {
                    if (i == 1)
                        assert(prev_report->point.system_date == INT64_MAX);
                    else
                        assert(report->point.system_date -
                               prev_report->point.system_date >= timer->delay);
                }
            }
        }
    }

    if (track_count > 0)
        test_timers_assert_smpte(&timers[SMPTE_TIMER_IDX], length, fps, false, 3);
    else
    {
        struct timer_state *timer = &timers[SMPTE_TIMER_IDX];
        vec_report_timer *vec = &timer->vec;
        assert(vec->size == 0);
    }
    test_end(ctx);

    for (size_t i = 0; i < TIMER_COUNT; ++i)
    {
        struct timer_state *timer = &timers[i];
        vlc_vector_clear(&timer->vec);
    }
}

static void
test_timers(struct ctx *ctx)
{
    test_log("timers%s\n",
             (ctx->flags & CLOCK_MASTER_MONOTONIC) ? " (monotonic)" : "");

    vlc_player_t *player = ctx->player;

    static const struct vlc_player_timer_cbs cbs =
    {
        .on_update = timers_on_update,
        .on_paused = timers_on_paused,
    };
    static const struct vlc_player_timer_smpte_cbs smpte_cbs =
    {
        .on_update = timers_smpte_on_update,
    };

    /* Configure timers */
    struct timer_state timers[TIMER_COUNT];

    /* Receive all clock update points */
    timers[REGULAR_TIMER_IDX].delay = VLC_TICK_INVALID;

    /* Filter some points in order to not be flooded */
    timers[REGULAR_DELAY_TIMER_IDX].delay = SOURCE_DELAY_TIMER_VALUE;

    /* Create all timers */
    for (size_t i = 0; i < ARRAY_SIZE(timers); ++i)
    {
        vlc_vector_init(&timers[i].vec);
        if (i == SMPTE_TIMER_IDX)
            timers[i].id = vlc_player_AddSmpteTimer(player, &smpte_cbs,
                                                    &timers[i]);
        else
            timers[i].id = vlc_player_AddTimer(player, timers[i].delay, &cbs,
                                               &timers[i]);
        assert(timers[i].id);
    }

    /* Test all timers using valid tracks */
    test_timers_playback(ctx, timers, 1, VLC_TICK_FROM_MS(200), 120, 1);

    /* Test all timers without valid tracks */
    test_timers_playback(ctx, timers, 0, VLC_TICK_FROM_MS(5000), 24, 16);

    /* Test SMPTE 29.97DF and 59.94DF arround 1 minute and 10 minutes to check
     * if timecodes are dropped every minutes */
    static const unsigned df_fps_list[] = { 30, 60 };
    static const unsigned df_min_test_list[] = { 1, 10 };

    for (size_t i = 0; i < ARRAY_SIZE(df_fps_list); ++i)
    {
        unsigned fps = df_fps_list[i];
        for (size_t j = 0; j < ARRAY_SIZE(df_min_test_list); ++j)
        {
            unsigned minute = df_min_test_list[j];
            vlc_tick_t check_duration = VLC_TICK_FROM_SEC(2);

            struct media_params params =
                DEFAULT_MEDIA_PARAMS(minute * VLC_TICK_FROM_SEC(60)
                                     + VLC_TICK_FROM_MS(400));
            params.track_count[VIDEO_ES] = 1;
            params.track_count[AUDIO_ES] = 0;
            params.track_count[SPU_ES] = 0;
            params.video_frame_rate = fps * 1000;
            params.video_frame_rate_base = 1001;

            /* This will prevent a RESET_PCR and ensure we receive all outputs
             * points. */
            params.pts_delay = check_duration;

            player_set_current_mock_media(ctx, "media1", &params, false);
            player_set_rate(ctx, 24);

            vlc_player_SetTime(player, params.length - check_duration);

            player_start(ctx);

            wait_state(ctx, VLC_PLAYER_STATE_STARTED);
            wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

            test_timers_assert_smpte_dropframe(&timers[SMPTE_TIMER_IDX], minute,
                                               fps);

            test_end(ctx);

            vlc_vector_clear(&timers[SMPTE_TIMER_IDX].vec);
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(timers); ++i)
    {
        struct timer_state *timer = &timers[i];
        vlc_vector_clear(&timer->vec);
        vlc_player_RemoveTimer(player, timer->id);
    }
}

static void
test_teletext(struct ctx *ctx)
{
#if defined(ZVBI_COMPILED) || defined(TELX_COMPILED)
    test_log("teletext with "TELETEXT_DECODER"\n");

    vlc_player_t *player = ctx->player;
    const struct vlc_player_track *track;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(1));
    params.track_count[AUDIO_ES] = 0;
    params.track_count[SPU_ES] = 3;
    params.config = "sub[1]{format=telx,page=888}+sub[2]{format=telx,page=889}";
    player_set_next_mock_media(ctx, "media1", &params);

    player_start(ctx);

    /* Wait that all tracks are added */
    {
        vec_on_track_list_changed *vec = &ctx->report.on_track_list_changed;
        while (vec_on_track_list_get_action_count(vec, VLC_PLAYER_LIST_ADDED)
               != 4)
            vlc_player_CondWait(player, &ctx->wait);
    }

    {
        vec_on_teletext_menu_changed *vec = &ctx->report.on_teletext_menu_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
        assert(vec->size == 1);
        assert(vec->data[0]);
        assert(ctx->report.on_teletext_enabled_changed.size == 0);
        assert(ctx->report.on_teletext_page_changed.size == 0);
        assert(ctx->report.on_teletext_transparency_changed.size == 0);
    }
    assert(vlc_player_HasTeletextMenu(player));

    /* Wait that video is selected */
    {
        vec_on_track_selection_changed *vec =
            &ctx->report.on_track_selection_changed;
        while (vec->size < 1)
            vlc_player_CondWait(player, &ctx->wait);
        assert(vec->size == 1);
        track = vlc_player_GetTrack(player, vec->data[0].selected_id);
        assert(track);
        assert(track->fmt.i_cat == VIDEO_ES);
    }

    track = vlc_player_GetTrackAt(player, SPU_ES, 0);
    assert(track);
    vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);

    /* Wait for first subtitle to be selected */
    {
        vec_on_track_selection_changed *vec =
                &ctx->report.on_track_selection_changed;
        while (vec->size < 2)
            vlc_player_CondWait(player, &ctx->wait);
        assert(vec->size == 2);
        track = vlc_player_GetTrack(player, vec->data[1].selected_id);
        assert(track);
        assert(track->fmt.i_cat == SPU_ES);
        assert(track->fmt.i_codec != VLC_CODEC_TELETEXT);
    }

    assert(!vlc_player_IsTeletextEnabled(player));
    vlc_player_SetTeletextEnabled(player, true);

    /* Wait that video and sub are selected */
    {
        vec_on_track_selection_changed *vec =
                &ctx->report.on_track_selection_changed;
        while (vec->size < 4)
            vlc_player_CondWait(player, &ctx->wait);
        assert(vec->size == 4);
        assert(vec->data[3].selected_id);
        track = vlc_player_GetTrack(player, vec->data[3].selected_id);
        assert(track);
        assert(track->fmt.i_codec == VLC_CODEC_TELETEXT);
    }

    assert(vlc_player_IsTeletextEnabled(player));
    track = vlc_player_GetSelectedTrack(player, SPU_ES);
    assert(track);
    assert(track && track->fmt.i_codec == VLC_CODEC_TELETEXT);

    /* Wait for reselection on teletext ES */
    {
        vec_on_teletext_enabled_changed *vec = &ctx->report.on_teletext_enabled_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
        assert(vec->size == 1);
        assert(VEC_LAST(vec));
    }

    /* Check page change event on selection */
    {
        vec_on_teletext_page_changed *vec = &ctx->report.on_teletext_page_changed;
        while (vec->size == 0)
            vlc_player_CondWait(player, &ctx->wait);
        assert(VEC_LAST(vec) == 888);
    }

    /* Change sub track to other teletext */
    {
        vec_on_track_selection_changed *vec =
                &ctx->report.on_track_selection_changed;
        size_t prevsize = vec->size;
        track = vlc_player_GetTrackAt(player, SPU_ES, 2);
        assert(track);
        assert(track->fmt.i_codec == VLC_CODEC_TELETEXT);
        vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
        while(!vec_on_track_selection_has_event(vec, prevsize,
                                                NULL, track->es_id))
            vlc_player_CondWait(player, &ctx->wait);
    }

    /* Check new ES page */
    {
        vec_on_teletext_page_changed *vec = &ctx->report.on_teletext_page_changed;
        while (vec->size == 1)
            vlc_player_CondWait(player, &ctx->wait);
        assert(VEC_LAST(vec) == 889);
        assert(vlc_player_GetTeletextPage(player) == 889);
    }

    /* Check direct page selection */
#ifdef ZVBI_COMPILED
    {
        vec_on_teletext_page_changed *vec = &ctx->report.on_teletext_page_changed;
        size_t prevsize = vec->size;
        vlc_player_SelectTeletextPage(player, 111);
        do
        {
            while (vec->size == prevsize)
                vlc_player_CondWait(player, &ctx->wait);
            prevsize = vec->size;
        } while(VEC_LAST(vec) != 111);
        assert(vlc_player_GetTeletextPage(player) == 111);
    }
#endif

    /* Check disabling teletext through es re-selection */
    {
        vec_on_track_selection_changed *selvec =
                &ctx->report.on_track_selection_changed;
        vec_on_teletext_enabled_changed *vec =
                &ctx->report.on_teletext_enabled_changed;
        size_t prevselsize = selvec->size;
        size_t prevsize = vec->size;
        track = vlc_player_GetTrackAt(player, SPU_ES, 0);
        assert(track);
        assert(track->fmt.i_codec != VLC_CODEC_TELETEXT);
        vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
        /* Wait for re-selection */
        while(!vec_on_track_selection_has_event(selvec, prevselsize,
                                                NULL, track->es_id))
            vlc_player_CondWait(player, &ctx->wait);
        /* Now check changed events */
        while (vec->size == prevsize)
            vlc_player_CondWait(player, &ctx->wait);
        assert(!VEC_LAST(vec));
        assert(!vlc_player_IsTeletextEnabled(player));
        assert(vlc_player_HasTeletextMenu(player));
    }

    /* Check re-enabling teletext through es re-selection */
    {
        vec_on_track_selection_changed *selvec =
                &ctx->report.on_track_selection_changed;
        vec_on_teletext_enabled_changed *vec =
                &ctx->report.on_teletext_enabled_changed;
        size_t prevselsize = selvec->size;
        size_t prevsize = vec->size;
        track = vlc_player_GetTrackAt(player, SPU_ES, 1);
        assert(track);
        assert(track->fmt.i_codec == VLC_CODEC_TELETEXT);
        vlc_player_SelectTrack(player, track, VLC_PLAYER_SELECT_EXCLUSIVE);
        /* Wait for re-selection */
        while(!vec_on_track_selection_has_event(selvec, prevselsize,
                                                NULL, track->es_id))
            vlc_player_CondWait(player, &ctx->wait);
        /* Now check changed events */
        while (vec->size == prevsize)
            vlc_player_CondWait(player, &ctx->wait);
        assert(VEC_LAST(vec));
        assert(vlc_player_IsTeletextEnabled(player));
        assert(vlc_player_HasTeletextMenu(player));
    }

#ifdef ZVBI_COMPILED
    /* Toggle Transparency tests */
    {
        vec_on_teletext_transparency_changed *vec =
                &ctx->report.on_teletext_transparency_changed;
        size_t prevsize = vec->size;
        assert(!vlc_player_IsTeletextTransparent(player));
        vlc_player_SetTeletextTransparency(player, true);
        do
        {
            while (vec->size == prevsize)
                vlc_player_CondWait(player, &ctx->wait);
            prevsize = vec->size;
        } while(!VEC_LAST(vec));
        prevsize = vec->size;
        vlc_player_SetTeletextTransparency(player, false);
        do
        {
            while (vec->size == prevsize)
                vlc_player_CondWait(player, &ctx->wait);
            prevsize = vec->size;
        } while(VEC_LAST(vec));
        assert(!vlc_player_IsTeletextTransparent(player));
        assert(!VEC_LAST(vec));
    }
#endif

    /* Check disabling teletext through API */
    {
        assert(track); /* from previous sel test */
        vec_on_track_selection_changed *selvec =
                &ctx->report.on_track_selection_changed;
        vec_on_teletext_enabled_changed *vec =
                &ctx->report.on_teletext_enabled_changed;
        size_t prevselsize = selvec->size;
        size_t prevsize = vec->size;
        vlc_player_SetTeletextEnabled(player, false);
        /* Wait for deselection */
        while(!vec_on_track_selection_has_event(selvec, prevselsize,
                                                track->es_id, NULL))
            vlc_player_CondWait(player, &ctx->wait);
        /* Now check changed events */
        while (vec->size == prevsize)
            vlc_player_CondWait(player, &ctx->wait);
        assert(!VEC_LAST(vec));
        assert(!vlc_player_IsTeletextEnabled(player));
        assert(vlc_player_HasTeletextMenu(player));
    }

    test_end(ctx);
#else
    VLC_UNUSED(ctx);
    test_log("teletext skipped\n");
#endif
}

static void
test_attachments(struct ctx *ctx)
{
    test_log("attachments\n");

    vlc_player_t *player = ctx->player;

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(1));
    params.attachment_count = 99;
    player_set_next_mock_media(ctx, "media1", &params);

    player_start(ctx);

    vec_on_media_attachments_added *vec = &ctx->report.on_media_attachments_added;

    while (vec->size == 0)
        vlc_player_CondWait(player, &ctx->wait);

    input_attachment_t **array = vec->data[0].array;
    size_t count = vec->data[0].count;

    assert(count == params.attachment_count);

    for (size_t i = 0; i < count; ++i)
    {
        input_attachment_t *attach = array[i];
        assert(strcmp(attach->psz_mime, "image/bmp") == 0);
        assert(strcmp(attach->psz_description, "Mock Attach Desc") == 0);

        char *name;
        int ret = asprintf(&name, "Mock Attach %zu", i);
        assert(ret > 0);
        assert(strcmp(attach->psz_name, name) == 0);
        free(name);
    }

    test_end(ctx);
}

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

static void
test_audio_loudness_meter_cb(vlc_tick_t date, double momentary_loudness,
                             void *data)
{
    (void) date;
    assert(momentary_loudness>= -23 -0.1 && momentary_loudness <= -23 +0.1);

    bool *loudness_meter_received = data;
    *loudness_meter_received = true;
}

static void
test_audio_loudness_meter(struct ctx *ctx)
{
    test_log("test_audio_loudness_meter\n");

    vlc_player_t *player = ctx->player;

    static const union vlc_player_metadata_cbs cbs = {
        test_audio_loudness_meter_cb,
    };

    if (!module_exists("ebur128"))
    {
        test_log("audio loudness meter test skipped\n");
        return;
    }

    struct media_params params = DEFAULT_MEDIA_PARAMS(VLC_TICK_FROM_SEC(1));
    params.track_count[AUDIO_ES] = 1;
    params.track_count[VIDEO_ES] = 0;
    params.track_count[SPU_ES] = 0;

    /* cf. EBU TECH 3341, Table 1. A 1000Hz stereo sine wave, with an amplitude
     * of -23dbFS (0.0707) should have a momentary loudness of -23LUFS */
    params.config = "audio[0]{sinewave=true,sinewave_frequency=1000"
                    ",sinewave_amplitude=0.0707}";
    player_set_next_mock_media(ctx, "media1", &params);

    bool loudness_meter_received = false;
    vlc_player_metadata_listener_id *listener_id =
        vlc_player_AddMetadataListener(player, VLC_PLAYER_METADATA_LOUDNESS_MOMENTARY,
                                       &cbs, &loudness_meter_received);
    assert(listener_id);

    player_start(ctx);
    wait_state(ctx, VLC_PLAYER_STATE_STARTED);
    wait_state(ctx, VLC_PLAYER_STATE_STOPPED);

    assert(loudness_meter_received);

    vlc_player_RemoveMetadataListener(player, listener_id);
    test_end(ctx);
}

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

    /* Test with --aout=none --vout=none */
    ctx_init(&ctx, DISABLE_VIDEO_OUTPUT | DISABLE_AUDIO_OUTPUT);
    test_no_outputs(&ctx);
    ctx_destroy(&ctx);
    ctx_init(&ctx, 0);

    test_outputs(&ctx); /* Must be the first test */

    test_same_media(&ctx);
    test_media_stopped(&ctx);
    test_set_current_media(&ctx);
    test_next_media(&ctx);
    test_seeks(&ctx);
    test_pause(&ctx);
    test_capabilities_pause(&ctx);
    test_capabilities_seek(&ctx);
    test_error(&ctx);
    test_unknown_uri(&ctx);
    test_titles(&ctx, true);
    test_titles(&ctx, false);
    test_tracks(&ctx, true);
    test_tracks(&ctx, false);
    test_tracks_ids(&ctx);
    test_programs(&ctx);
    test_teletext(&ctx);
    test_attachments(&ctx);

    test_delete_while_playback(VLC_OBJECT(ctx.vlc->p_libvlc_int), true);
    test_delete_while_playback(VLC_OBJECT(ctx.vlc->p_libvlc_int), false);

    test_timers(&ctx);
    ctx_destroy(&ctx);
    ctx_init(&ctx, CLOCK_MASTER_MONOTONIC);
    test_timers(&ctx);
    ctx_destroy(&ctx);

    /* Test with instantaneous audio drain */
    ctx_init(&ctx, AUDIO_INSTANT_DRAIN);
    test_clock_discontinuities(&ctx);
    ctx_destroy(&ctx);
    ctx_init(&ctx, CLOCK_MASTER_MONOTONIC|AUDIO_INSTANT_DRAIN);
    test_clock_discontinuities(&ctx);
    ctx_destroy(&ctx);

    /* Test with --no-video */
    ctx_init(&ctx, DISABLE_VIDEO);
    test_es_selection_override(&ctx);
    test_audio_loudness_meter(&ctx);

    ctx_destroy(&ctx);
    return 0;
}

#define MODULE_NAME test_src_player
#undef VLC_DYNAMIC_PLUGIN
#include <vlc_plugin.h>
/* Define a builtin module for mocked parts */
const char vlc_module_name[] = MODULE_STRING;

struct aout_sys
{
    vlc_tick_t first_pts;
    vlc_tick_t first_play_date;
    vlc_tick_t pos;

    struct ctx *ctx;
};

static void aout_Play(audio_output_t *aout, block_t *block, vlc_tick_t date)
{
    struct aout_sys *sys = aout->sys;

    if (sys->first_play_date == VLC_TICK_INVALID)
    {
        assert(sys->first_pts == VLC_TICK_INVALID);
        sys->first_play_date = date;
        sys->first_pts = block->i_pts;

        struct ctx *ctx = sys->ctx;
        VEC_PUSH(on_aout_first_pts, sys->first_pts);
    }

    aout_TimingReport(aout, sys->first_play_date + sys->pos - VLC_TICK_0,
                      sys->first_pts + sys->pos);
    sys->pos += block->i_length;
    block_Release(block);
}

static void aout_Flush(audio_output_t *aout)
{
    struct aout_sys *sys = aout->sys;
    sys->pos = 0;
    sys->first_pts = sys->first_play_date = VLC_TICK_INVALID;
}

static void aout_InstantDrain(audio_output_t *aout)
{
    aout_DrainedReport(aout);
}

static int aout_Start(audio_output_t *aout, audio_sample_format_t *restrict fmt)
{
    (void) aout;
    return AOUT_FMT_LINEAR(fmt) ? VLC_SUCCESS : VLC_EGENERIC;
}

static void
aout_Close(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;
    free(aout->sys);
}

static int aout_Open(vlc_object_t *obj)
{
    audio_output_t *aout = (audio_output_t *)obj;

    aout->start = aout_Start;
    aout->play = aout_Play;
    aout->pause = NULL;
    aout->flush = aout_Flush;
    aout->stop = aout_Flush;
    aout->volume_set = NULL;
    aout->mute_set = NULL;

    struct aout_sys *sys = aout->sys = malloc(sizeof(*sys));
    assert(sys != NULL);

    sys->ctx = var_InheritAddress(aout, "test-ctx");
    assert(sys->ctx != NULL);

    if (sys->ctx->flags & AUDIO_INSTANT_DRAIN)
        aout->drain = aout_InstantDrain;

    aout_Flush(aout);

    return VLC_SUCCESS;
}

static block_t *resampler_Resample(filter_t *filter, block_t *in)
{
    VLC_UNUSED(filter);
    return in;
}

static void resampler_Close(filter_t *filter)
{
    VLC_UNUSED(filter);
}

static int resampler_Open(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *)obj;
    static const struct vlc_filter_operations filter_ops =
    { .filter_audio = resampler_Resample, .close = resampler_Close, };
    filter->ops = &filter_ops;
    return VLC_SUCCESS;
}

vlc_module_begin()
    /* This aout module will report audio timings perfectly, but without any
     * delay, in order to be usable for player tests. Indeed, this aout will
     * report timings immediately from Play(), but points will be in the
     * future (like when aout->time_get() is used). */
    set_capability("audio output", 0)
    set_callbacks(aout_Open, aout_Close)
    add_submodule ()
    /* aout will insert a resampler that can have samples delay, even for 1:1
     * Insert our own resampler that keeps blocks and pts untouched. */
        set_capability ("audio resampler", 9999)
        set_callback (resampler_Open)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};
