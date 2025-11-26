// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * attachments.c: attachments player test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

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

int
main(void)
{
    struct ctx ctx;
    ctx_init(&ctx, 0);
    test_attachments(&ctx);
    ctx_destroy(&ctx);
    return 0;
}
