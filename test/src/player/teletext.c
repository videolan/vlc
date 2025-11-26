// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * teletext.c: teletext player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

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

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_teletext(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
