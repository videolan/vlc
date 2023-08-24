/*****************************************************************************
 * thumbnailer.c - libvlc thumbnailing smoke test
 *****************************************************************************
 * Copyright (C) 2023 VideoLabs
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
# include "config.h"
#endif

#include "test.h"
#include "../lib/libvlc_internal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vlc_common.h>
#include <vlc_threads.h>
#include <vlc_modules.h>

#include <vlc/vlc.h>

#ifndef TEST_THUMB_TYPE
# error Define TEST_THUMB_TYPE to a libvlc_picture_type_t value
#endif

static void thumbnail_generated(const libvlc_event_t *event, void *user_data)
{
    (void)event;
    assert(event->u.media_thumbnail_generated.p_thumbnail != NULL);
    vlc_sem_t *sem = user_data;
    vlc_sem_post(sem);
}

static void test_media_thumbnail(libvlc_instance_t *vlc, const char *location,
                                 unsigned width, unsigned height,
                                 enum libvlc_picture_type_t picture_type)
{
#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)
    test_log ("Testing media_thumbnail: width=%u height=%u type=" STRINGIFY(TEST_THUMB_TYPE) "'\n", width, height);
    libvlc_media_t *media = libvlc_media_new_location(location);
    assert (media != NULL);

    vlc_sem_t sem;
    vlc_sem_init (&sem, 0);

    libvlc_event_manager_t *em = libvlc_media_event_manager(media);
    libvlc_event_attach(em, libvlc_MediaThumbnailGenerated, thumbnail_generated, &sem);

    libvlc_media_thumbnail_request_t *request =
        libvlc_media_thumbnail_request_by_pos(
            vlc, media, 0.f, libvlc_media_thumbnail_seek_precise,
            width, height, false, picture_type, 0);

    vlc_sem_wait(&sem);
    libvlc_media_thumbnail_request_destroy(request);

    libvlc_event_detach(em, libvlc_MediaThumbnailGenerated, thumbnail_generated, &sem);
    libvlc_media_release(media);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    test_init();

#ifndef ENABLE_SOUT
    (void)thumbnail_generated;
    (void)test_media_thumbnail;
    test_log("Stream output has been disabled (see ENABLE_SOUT), skipping...")
    return 77;
#else

    libvlc_instance_t *vlc = libvlc_new (test_defaults_nargs,
                                         test_defaults_args);
    assert (vlc != NULL);

    const char *module_deps[] = {
        "demux_mock", "swscale",
        TEST_REQUIRED_MODULES
    };

    for (size_t i=0; i<ARRAY_SIZE(module_deps); ++i)
    {
        test_log(" - Checking that module %s exists...\n", module_deps[i]);
        /* Skip test if one of the dependencies doesn't exist */
        if (!module_exists(module_deps[i]))
        {
            test_log("   Module %s is required and wasn't found, skipping...\n", module_deps[i]);
            libvlc_release(vlc);
            exit(77);
        }
    }

    /* Check that thumbnails are correctly generated. */
    const char *mock_addr =
        "mock://video_track_count=1;length=100000000;"
        "video_width=800;video_height=600";

    test_media_thumbnail(vlc, mock_addr, 800, 600, TEST_THUMB_TYPE);
    test_media_thumbnail(vlc, mock_addr, 400, 300, TEST_THUMB_TYPE);

    libvlc_release (vlc);

    return 0;
#endif
}
