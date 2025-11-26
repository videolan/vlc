// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * titles.c: titles player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

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

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_titles(&ctx, true);
    test_titles(&ctx, false);
    ctx_destroy(&ctx);
    return 0;
}
