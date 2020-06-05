/*
 * media_player.c - libvlc smoke test
 *
 */

/**********************************************************************
 *  Copyright (C) 2010 Pierre d'Herbemont.                            *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

#include "test.h"
#include "../lib/libvlc_internal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vlc_threads.h>
#include <vlc_fs.h>
#include <vlc_input_item.h>
#include <vlc_events.h>

static void media_parse_ended(const libvlc_event_t *event, void *user_data)
{
    (void)event;
    vlc_sem_t *sem = user_data;
    vlc_sem_post (sem);
}

static void print_media(libvlc_media_t *media)
{
    static const libvlc_track_type_t types[] =
        { libvlc_track_audio, libvlc_track_video, libvlc_track_text,
          libvlc_track_unknown };
    size_t nb_tracks = 0;

    for (size_t i = 0; i < ARRAY_SIZE(types); ++i)
    {
        const libvlc_track_type_t type = types[i];
        libvlc_media_tracklist_t *tracklist =
            libvlc_media_get_tracklist(media, type);
        assert(tracklist);

        for (size_t j = 0; j < libvlc_media_tracklist_count(tracklist); ++j)
        {
            const libvlc_media_track_t *p_track =
                libvlc_media_tracklist_at(tracklist, j);

            assert(p_track);
            assert(p_track->i_type == type);
            nb_tracks ++;

            test_log("\ttrack(%zu/%d): codec: %4.4s/%4.4s, ", j, p_track->i_id,
                (const char *)&p_track->i_codec,
                (const char *)&p_track->i_original_fourcc);
            switch (p_track->i_type)
            {
            case libvlc_track_audio:
                printf("audio: channels: %u, rate: %u\n",
                       p_track->audio->i_channels, p_track->audio->i_rate);
                break;
            case libvlc_track_video:
                printf("video: %ux%u, sar: %u/%u, fps: %u/%u\n",
                       p_track->video->i_width, p_track->video->i_height,
                       p_track->video->i_sar_num, p_track->video->i_sar_den,
                       p_track->video->i_frame_rate_num, p_track->video->i_frame_rate_den);
                break;
            case libvlc_track_text:
                printf("text: %s\n", p_track->subtitle->psz_encoding);
                break;
            case libvlc_track_unknown:
                printf("unknown\n");
                break;
            default:
                vlc_assert_unreachable();
            }
        }
        libvlc_media_tracklist_delete(tracklist);
    }

    if (nb_tracks == 0)
        test_log("\tmedia doesn't have any tracks\n");

    for (enum libvlc_meta_t i = libvlc_meta_Title;
         i <= libvlc_meta_DiscTotal; ++i)
    {
        char *psz_meta = libvlc_media_get_meta(media, i);
        if (psz_meta != NULL)
            test_log("\tmeta(%d): '%s'\n", i, psz_meta);
        free(psz_meta);
    }
}

static void test_media_preparsed(libvlc_instance_t *vlc, const char *path,
                                 const char *location,
                                 libvlc_media_parse_flag_t parse_flags,
                                 libvlc_media_parsed_status_t i_expected_status)
{
    test_log ("test_media_preparsed: %s, expected: %d\n", path ? path : location,
              i_expected_status);

    libvlc_media_t *media;
    if (path != NULL)
        media = libvlc_media_new_path (vlc, path);
    else
        media = libvlc_media_new_location (vlc, location);
    assert (media != NULL);

    vlc_sem_t sem;
    vlc_sem_init (&sem, 0);

    // Check to see if we are properly receiving the event.
    libvlc_event_manager_t *em = libvlc_media_event_manager (media);
    libvlc_event_attach (em, libvlc_MediaParsedChanged, media_parse_ended, &sem);

    // Parse the media. This is synchronous.
    int i_ret = libvlc_media_parse_with_options(media, parse_flags, -1);
    assert(i_ret == 0);

    // Wait for preparsed event
    vlc_sem_wait (&sem);

    // We are good, now check Elementary Stream info.
    assert (libvlc_media_get_parsed_status(media) == i_expected_status);
    if (i_expected_status == libvlc_media_parsed_status_done)
        print_media(media);

    libvlc_media_release (media);
}

static void input_item_preparse_timeout( input_item_t *item,
                                         enum input_item_preparse_status status,
                                         void *user_data )
{
    VLC_UNUSED(item);
    vlc_sem_t *p_sem = user_data;

    assert( status == ITEM_PREPARSE_TIMEOUT );
    vlc_sem_post(p_sem);
}

static void test_input_metadata_timeout(libvlc_instance_t *vlc, int timeout,
                                        int wait_and_cancel)
{
    test_log ("test_input_metadata_timeout: timeout: %d, wait_and_cancel: %d ms\n",
         timeout, wait_and_cancel);

    int i_ret, p_pipe[2];
    i_ret = vlc_pipe(p_pipe);
    assert(i_ret == 0 && p_pipe[1] >= 0);

    char psz_fd_uri[strlen("fd://") + 11];
    sprintf(psz_fd_uri, "fd://%u", (unsigned) p_pipe[1]);
    input_item_t *p_item = input_item_NewFile(psz_fd_uri, "test timeout", 0,
                                              ITEM_LOCAL);
    assert(p_item != NULL);

    vlc_sem_t sem;
    vlc_sem_init (&sem, 0);
    const struct input_preparser_callbacks_t cbs = {
        .on_preparse_ended = input_item_preparse_timeout,
    };
    i_ret = libvlc_MetadataRequest(vlc->p_libvlc_int, p_item,
                                   META_REQUEST_OPTION_SCOPE_LOCAL |
                                   META_REQUEST_OPTION_FETCH_LOCAL,
                                   &cbs, &sem, timeout, vlc);
    assert(i_ret == 0);

    if (wait_and_cancel > 0)
    {
        vlc_tick_sleep( VLC_TICK_FROM_MS(wait_and_cancel) );
        libvlc_MetadataCancel(vlc->p_libvlc_int, vlc);

    }
    vlc_sem_wait(&sem);

    input_item_Release(p_item);
    vlc_close(p_pipe[0]);
    vlc_close(p_pipe[1]);
}

#define TEST_SUBITEMS_COUNT 6
static struct
{
    const char *file;
    libvlc_media_type_t type;
} test_media_subitems_list[TEST_SUBITEMS_COUNT] =
{
    { "directory", libvlc_media_type_directory, },
    { "file.jpg", libvlc_media_type_file },
    { "file.mkv", libvlc_media_type_file },
    { "file.mp3", libvlc_media_type_file },
    { "file.png", libvlc_media_type_file },
    { "file.ts", libvlc_media_type_file },
};

static void subitem_parse_ended(const libvlc_event_t *event, void *user_data)
{
    (void)event;
    vlc_sem_t *sem = user_data;
    vlc_sem_post (sem);
}

static void subitem_added(const libvlc_event_t *event, void *user_data)
{
#ifdef _WIN32
#define FILE_SEPARATOR   '\\'
#else
#define FILE_SEPARATOR   '/'
#endif
    bool *subitems_found = user_data;
    libvlc_media_t *m = event->u.media_subitem_added.new_child;
    assert (m);

    char *mrl = libvlc_media_get_mrl (m);
    assert (mrl);

    const char *file = strrchr (mrl, FILE_SEPARATOR);
    assert (file);
    file++;
    test_log ("subitem_added, file: %s\n", file);

    for (unsigned i = 0; i < TEST_SUBITEMS_COUNT; ++i)
    {
        if (strcmp (test_media_subitems_list[i].file, file) == 0)
        {
            assert (!subitems_found[i]);
            assert (libvlc_media_get_type(m) == test_media_subitems_list[i].type);
            subitems_found[i] = true;
        }
    }
    free (mrl);
#undef FILE_SEPARATOR
}

static void test_media_subitems_media(libvlc_media_t *media, bool play,
                                      bool b_items_expected)
{
    libvlc_media_add_option(media, ":ignore-filetypes= ");
    libvlc_media_add_option(media, ":no-sub-autodetect-file");

    bool subitems_found[TEST_SUBITEMS_COUNT] = { 0 };
    vlc_sem_t sem;
    vlc_sem_init (&sem, 0);

    libvlc_event_manager_t *em = libvlc_media_event_manager (media);
    libvlc_event_attach (em, libvlc_MediaSubItemAdded, subitem_added, subitems_found);

    if (play)
    {
        /* XXX: libvlc_media_parse_with_options won't work with fd, since it
         * won't be preparsed because fd:// is an unknown type, so play the
         * file to force parsing. */
        libvlc_event_attach (em, libvlc_MediaSubItemTreeAdded, subitem_parse_ended, &sem);

        libvlc_media_player_t *mp = libvlc_media_player_new_from_media (media);
        assert (mp);
        assert (libvlc_media_player_play (mp) != -1);
        vlc_sem_wait (&sem);
        libvlc_media_player_release (mp);
    }
    else
    {
        libvlc_event_attach (em, libvlc_MediaParsedChanged, subitem_parse_ended, &sem);

        int i_ret = libvlc_media_parse_with_options(media, libvlc_media_parse_local, -1);
        assert(i_ret == 0);
        vlc_sem_wait (&sem);
    }

    if (!b_items_expected)
        return;

    for (unsigned i = 0; i < TEST_SUBITEMS_COUNT; ++i)
    {
        test_log ("test if %s was added\n", test_media_subitems_list[i].file);
        assert (subitems_found[i]);
    }
}

static void test_media_subitems(libvlc_instance_t *vlc)
{
    const char *subitems_path = SRCDIR"/samples/subitems";

    libvlc_media_t *media;

    test_log ("Testing media_subitems: path: '%s'\n", subitems_path);
    media = libvlc_media_new_path (vlc, subitems_path);
    assert (media != NULL);
    test_media_subitems_media (media, false, true);
    libvlc_media_release (media);

    #define NB_LOCATIONS 2
    char *subitems_realpath = realpath (subitems_path, NULL);
    assert (subitems_realpath != NULL);
    const char *schemes[NB_LOCATIONS] = { "file://", "dir://" };
    for (unsigned i = 0; i < NB_LOCATIONS; ++i)
    {
        char *location;
        assert (asprintf (&location, "%s%s", schemes[i], subitems_realpath) != -1);
        test_log ("Testing media_subitems: location: '%s'\n", location);
        media = libvlc_media_new_location (vlc, location);
        assert (media != NULL);
        test_media_subitems_media (media, false, true);
        free (location);
        libvlc_media_release (media);
    }
    free (subitems_realpath);

#ifdef HAVE_FSTATAT
    /* listing directory via a fd works only if fstatat() exists */
    int fd = open (subitems_path, O_RDONLY);
    test_log ("Testing media_subitems: fd: '%d'\n", fd);
    assert (fd >= 0);
    media = libvlc_media_new_fd (vlc, fd);
    assert (media != NULL);
    test_media_subitems_media (media, true, true);
    libvlc_media_release (media);
    vlc_close (fd);
#else
#warning not testing subitems list via a fd location
#endif

    test_log ("Testing media_subitems failure\n");
    media = libvlc_media_new_location (vlc, "wrongfile://test");
    assert (media != NULL);
    test_media_subitems_media (media, false, false);
    libvlc_media_release (media);
}

int main(int i_argc, char *ppsz_argv[])
{
    test_init();

    libvlc_instance_t *vlc = libvlc_new (test_defaults_nargs,
                                         test_defaults_args);
    assert (vlc != NULL);

    char *psz_test_arg = i_argc > 1 ? ppsz_argv[1] : NULL;
    if (psz_test_arg != NULL)
    {
        alarm(0);
        const char *psz_test_url;
        const char *psz_test_path;
        if (strstr(psz_test_arg, "://") != NULL)
        {
            psz_test_url = psz_test_arg;
            psz_test_path = NULL;
        }
        else
        {
            psz_test_url = NULL;
            psz_test_path = psz_test_arg;
        }
        test_media_preparsed (vlc, psz_test_path, psz_test_url,
                              libvlc_media_parse_network,
                              libvlc_media_parsed_status_done);
        return 0;
    }

    test_media_preparsed (vlc, SRCDIR"/samples/image.jpg", NULL,
                          libvlc_media_parse_local,
                          libvlc_media_parsed_status_done);
    test_media_preparsed (vlc, NULL, "http://parsing_should_be_skipped.org/video.mp4",
                          libvlc_media_parse_local,
                          libvlc_media_parsed_status_skipped);
    test_media_preparsed (vlc, NULL, "unknown://parsing_should_be_skipped.org/video.mp4",
                          libvlc_media_parse_local,
                          libvlc_media_parsed_status_skipped);
    test_media_subitems (vlc);

    /* Testing libvlc_MetadataRequest timeout and libvlc_MetadataCancel. For
     * that, we need to create a local input_item_t based on a pipe. There is
     * no way to do that with a libvlc_media_t, that's why we don't use
     * libvlc_media_parse*() */

    test_input_metadata_timeout (vlc, 100, 0);
    test_input_metadata_timeout (vlc, 0, 100);

    libvlc_release (vlc);

    return 0;
}
