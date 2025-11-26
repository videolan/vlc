// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * programs.c: programs player
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

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

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_programs(&ctx);
    ctx_destroy(&ctx);
    return 0;
}