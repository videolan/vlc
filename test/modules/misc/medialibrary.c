/*****************************************************************************
 * medialibrary.c: test for the medialibrary code
 *****************************************************************************
 * Copyright (C) 2023 Videolabs
 *
 * Authors: Alexandre Janniaux <ajanni@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* Define a builtin module for mocked parts */
#define MODULE_NAME test_misc_medialibrary
#undef VLC_DYNAMIC_PLUGIN

#include "../../libvlc/test.h"

#include <vlc/vlc.h>

#include <vlc_common.h>
#include <vlc_image.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_block.h>
#include <vlc_codec.h>
#include <vlc_filter.h>

#include <vlc_media_library.h>
#include <ftw.h>

const char vlc_module_name[] = MODULE_STRING;

static int exitcode = 0;

static void ValidateThumbnail(void *data, const vlc_ml_event_t *event)
{
    if (event->i_type != VLC_ML_EVENT_MEDIA_THUMBNAIL_GENERATED)
        return;

    assert(event->media_thumbnail_generated.b_success);
    vlc_sem_t *sem = data;
    vlc_sem_post(sem);
}

static int OpenIntf(vlc_object_t *root)
{
    vlc_medialibrary_t *ml = vlc_ml_instance_get(root);
    if (ml == NULL)
    {
        exitcode = 77;
        return VLC_SUCCESS;
    }

    #define MOCK_URL "mock://video_track_count=1;length=100000000;" \
                     "video_width=800;video_height=600"
    vlc_ml_media_t *media = vlc_ml_new_external_media(ml, MOCK_URL);

    vlc_sem_t sem;
    vlc_sem_init(&sem, 0);

    vlc_ml_event_callback_t *listener =
        vlc_ml_event_register_callback(ml, ValidateThumbnail, &sem);
    vlc_ml_media_generate_thumbnail(ml, media->i_id,
            VLC_ML_THUMBNAIL_SMALL, 800, 600, 0.f);

    vlc_sem_wait(&sem);
    vlc_ml_event_unregister_callback(ml, listener);

    vlc_ml_media_release(media);

    return VLC_SUCCESS;
}

/** Inject the mocked modules as a static plugin: **/
vlc_module_begin()
    set_callback(OpenIntf)
    set_capability("interface", 0)
vlc_module_end()

VLC_EXPORT const vlc_plugin_cb vlc_static_modules[] = {
    VLC_SYMBOL(vlc_entry),
    NULL
};

static int cleanup_tmpdir(const char *dirpath, const struct stat *sb,
                          int typeflag, struct FTW *ftwbuf)
{
    (void)sb; (void)typeflag; (void)ftwbuf;
    return remove(dirpath);
}

int main()
{
    char template[] = "/tmp/vlc.test." MODULE_STRING ".XXXXXX";
    const char *tempdir = mkdtemp(template);
    if (tempdir == NULL)
    {
        assert(tempdir != NULL);
        return -1;
    }
    fprintf(stderr, "Using XDG_DATA_HOME directory %s\n", tempdir);
    setenv("XDG_DATA_HOME", tempdir, 1);

    test_init();

    const char * const args[] = {
        "-vvv", "--vout=dummy", "--aout=dummy", "--text-renderer=dummy",
        "--no-auto-preparse", "--media-library"
    };

    libvlc_instance_t *vlc = libvlc_new(ARRAY_SIZE(args), args);

    libvlc_add_intf(vlc, MODULE_STRING);
    libvlc_playlist_play(vlc);

    libvlc_release(vlc);

    /* Remove temporary directory */
    nftw(tempdir, cleanup_tmpdir, FOPEN_MAX, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
    return exitcode;
}
