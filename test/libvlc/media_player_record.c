/*****************************************************************************
 * media_player_record.c - libvlc record test with ENABLE_SOUT
 **********************************************************************
 * Copyright (C) 2023 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
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

#include "test.h"
#include "media_player.h"
#include <vlc_common.h>

static void test_media_player_record(const char** argv, int argc)
{
    test_log ("Testing record\n");

    const char file[] = "mock://video_track_count=1;audio_track_count=1;can_record=0";

    /* Avoid leaks from various dlopen... */
    const char *new_argv[argc+1];
    for (int i = 0; i < argc; ++i)
        new_argv[i] = argv[i];
    new_argv[argc++] = "--codec=araw,rawvideo,subsdec,none";

    /* Load the mock media */
    libvlc_instance_t *vlc = libvlc_new (argc, new_argv);
    assert (vlc != NULL);
    libvlc_media_t *md = libvlc_media_new_location(file);
    assert (md != NULL);
    libvlc_media_player_t *mp = libvlc_media_player_new (vlc);
    assert (mp != NULL);
    libvlc_media_player_set_media (mp, md);
    libvlc_media_release (md);

    libvlc_event_manager_t *em = libvlc_media_player_event_manager(mp);
    struct mp_event_ctx ctx;
    mp_event_ctx_init(&ctx);

    int res;
    res = libvlc_event_attach(em, libvlc_MediaPlayerRecordChanged, mp_event_ctx_on_event, &ctx);
    assert(!res);

    libvlc_media_player_play (mp);

    const char path[] = "./";
    char *filepath;

    libvlc_media_player_record(mp, true, path);

    /* Enabling */
    {
        const struct libvlc_event_t *ev = mp_event_ctx_wait_event(&ctx);
        assert(ev->u.media_player_record_changed.recording);
        mp_event_ctx_release(&ctx);
    }

    /* Disabling */
    {
        libvlc_media_player_record(mp, false, path);
        const struct libvlc_event_t *ev = mp_event_ctx_wait_event(&ctx);
        assert(!ev->u.media_player_record_changed.recording);
        assert(ev->u.media_player_record_changed.recorded_file_path != NULL);
        filepath = strdup(ev->u.media_player_record_changed.recorded_file_path);
        assert(filepath != NULL);
        mp_event_ctx_release(&ctx);
    }

    libvlc_media_player_stop_async (mp);
    libvlc_media_player_release (mp);
    libvlc_release (vlc);

    res = unlink(filepath);
    /** TODO:
     * We should check assert(res == 0);, but the record is currently
     * creating a stream output pipeline instance for recording with
     * a specific mux after the end of the Open() of the recording
     * module, with a file{no-overwrite} option. It means that the
     * file might not exist yet. */
    free(filepath);
}

int main(void)
{
#ifndef ENABLE_SOUT
    (void)test_media_player_record;
    return 77;
#endif

    test_init();
    test_media_player_record (test_defaults_args, test_defaults_nargs);
    return 0;
}
