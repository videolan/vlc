/*
 * media_player.c - libvlc smoke test
 *
 * $Id$
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

static void media_parse_ended(const libvlc_event_t *event, void *user_data)
{
    (void)event;
    vlc_sem_t *sem = user_data;
    vlc_sem_post (sem);
}

static void test_media_preparsed(int argc, const char** argv,
                                 const char *path,
                                 const char *location,
                                 libvlc_media_parsed_status_t i_expected_status)
{
    log ("test_media_preparsed: %s, expected: %d\n", path ? path : location,
         i_expected_status);

    libvlc_instance_t *vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

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
    int i_ret = libvlc_media_parse_with_options(media, libvlc_media_parse_local);
    assert(i_ret == 0);

    // Wait for preparsed event
    vlc_sem_wait (&sem);
    vlc_sem_destroy (&sem);

    // We are good, now check Elementary Stream info.
    assert (libvlc_media_get_parsed_status(media) == i_expected_status);

    libvlc_media_release (media);
    libvlc_release (vlc);
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
    log ("subitem_added, file: %s\n", file);

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

        int i_ret = libvlc_media_parse_with_options(media, libvlc_media_parse_local);
        assert(i_ret == 0);
        vlc_sem_wait (&sem);
    }

    vlc_sem_destroy (&sem);

    if (!b_items_expected)
        return;

    for (unsigned i = 0; i < TEST_SUBITEMS_COUNT; ++i)
    {
        log ("test if %s was added\n", test_media_subitems_list[i].file);
        assert (subitems_found[i]);
    }
}

static void test_media_subitems(int argc, const char** argv)
{
    const char *subitems_path = SRCDIR"/samples/subitems";

    libvlc_instance_t *vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);
    libvlc_media_t *media;

    log ("Testing media_subitems: path: '%s'\n", subitems_path);
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
        log ("Testing media_subitems: location: '%s'\n", location);
        media = libvlc_media_new_location (vlc, location);
        assert (media != NULL);
        test_media_subitems_media (media, false, true);
        free (location);
        libvlc_media_release (media);
    }
    free (subitems_realpath);

#ifdef HAVE_OPENAT
    /* listing directory via a fd works only if HAVE_OPENAT is defined */
    int fd = open (subitems_path, O_RDONLY);
    log ("Testing media_subitems: fd: '%d'\n", fd);
    assert (fd >= 0);
    media = libvlc_media_new_fd (vlc, fd);
    assert (media != NULL);
    test_media_subitems_media (media, true, true);
    libvlc_media_release (media);
    vlc_close (fd);
#else
#warning not testing subitems list via a fd location
#endif

    log ("Testing media_subitems failure\n");
    media = libvlc_media_new_location (vlc, "wrongfile://test");
    assert (media != NULL);
    test_media_subitems_media (media, false, false);
    libvlc_media_release (media);

    libvlc_release (vlc);
}

int main (void)
{
    test_init();

    test_media_preparsed (test_defaults_nargs, test_defaults_args,
                          SRCDIR"/samples/image.jpg", NULL,
                          libvlc_media_parsed_status_done);
    test_media_preparsed (test_defaults_nargs, test_defaults_args,
                          NULL, "http://parsing_should_be_skipped.org/video.mp4",
                          libvlc_media_parsed_status_skipped);
    test_media_preparsed (test_defaults_nargs, test_defaults_args,
                          NULL, "unknown://parsing_should_be_skipped.org/video.mp4",
                          libvlc_media_parsed_status_skipped);
    test_media_subitems (test_defaults_nargs, test_defaults_args);

    return 0;
}
