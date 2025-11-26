// SPDX-License-Identifier: LGPL-2.1-or-later
/*****************************************************************************
 * lifecycle.c: player lifecycle test
 *****************************************************************************
 * Copyright (C) 2018-2025 VLC authors and VideoLAN
 *****************************************************************************/

#include "common.h"

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

int
main(void)
{
    test_init();

    libvlc_instance_t *dummy = libvlc_new(0, NULL);
    assert(dummy != NULL);

    test_delete_while_playback(VLC_OBJECT(dummy->p_libvlc_int), true);
    test_delete_while_playback(VLC_OBJECT(dummy->p_libvlc_int), false);

    libvlc_release(dummy);
    return 0;
}
