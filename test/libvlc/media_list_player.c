/*
 * media_list_player.c - libvlc smoke test
 *
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
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

 // For vlc_tick_sleep
#include <vlc_common.h>
#include <vlc_tick.h>

#include "libvlc_additions.h"

/*
    HACK - FIX ME
    This allows for the direct addition of subitems in the playback options test.
    This would not be necessary if there were an add subitems function.
*/
#include "../../lib/libvlc_internal.h"

struct check_items_order_data {
    bool done_playing;
    unsigned count;
    unsigned index;
    void * items[16];
};

static inline void check_data_init(struct check_items_order_data *check)
{
    check->index = 0;
    check->count = 0;
    check->done_playing = false;
}

static inline void queue_expected_item(struct check_items_order_data *check, void *item)
{
    assert(check->count < 16);
    check->items[check->count] = item;
    check->count++;
}

static inline void wait_queued_items(struct check_items_order_data *check)
{
    // Wait dummily for check_items_order_callback() to flag 'done_playing':
    while (!check->done_playing)
        sched_yield();
}

static inline void wait_playing(libvlc_media_list_player_t *mlp)
{
    while (!libvlc_media_list_player_is_playing (mlp))
        sched_yield();
}

static inline void wait_stopped(libvlc_media_list_player_t *mlp)
{
    while (libvlc_media_list_player_is_playing (mlp))
        sched_yield();
}

static inline void stop_and_wait(libvlc_media_list_player_t *mlp)
{
    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (mlp);
}

static void check_items_order_callback(const libvlc_event_t * p_event, void * user_data)
{
    struct check_items_order_data *checks = user_data;
    libvlc_media_t *md = p_event->u.media_list_player_next_item_set.item;
    assert(checks->index < checks->count);
    if (checks->items[checks->index] != md)
    {
        char *title = libvlc_media_get_meta(md, libvlc_meta_Title);
        test_log ("Got items %s\n", title);
        free(title);
    }
    assert(checks->items[checks->index] == md);

    char *title = libvlc_media_get_meta(md, libvlc_meta_Title);
    test_log ("Item %d '%s' was correctly queued\n", checks->index, title);
    free(title);

    if (checks->index == (checks->count - 1))
    {
        test_log ("Done playing with success\n");
        checks->done_playing = true;
    }
    checks->index++;
}

static void test_media_list_player_items_queue(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = test_default_sample;

    test_log ("Testing media player item queue-ing\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location(file);
    assert(md);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    mlp = libvlc_media_list_player_new (vlc, NULL, NULL);
    assert(mlp);

    libvlc_media_list_add_media (ml, md);

    static struct check_items_order_data check;
    check_data_init(&check);
    queue_expected_item(&check, md);

    // Add three more media
    queue_expected_item(&check, media_list_add_file_path(ml, file));
    queue_expected_item(&check, media_list_add_file_path(ml, file));
    queue_expected_item(&check, media_list_add_file_path(ml, file));

    // Add a node
    libvlc_media_t *node = libvlc_media_new_as_node("node");
    assert(node);
    libvlc_media_list_add_media(ml, node);
    queue_expected_item(&check, node);

    // Add items to that node
    libvlc_media_list_t *subitems = libvlc_media_subitems(node);
    queue_expected_item(&check, media_list_add_file_path(subitems, file));
    queue_expected_item(&check, media_list_add_file_path(subitems, file));
    queue_expected_item(&check, media_list_add_file_path(subitems, file));
    libvlc_media_list_release(subitems);

    libvlc_media_list_player_set_media_list (mlp, ml);

    libvlc_event_manager_t * em = libvlc_media_list_player_event_manager(mlp);
    int val = libvlc_event_attach(em, libvlc_MediaListPlayerNextItemSet,
                                  check_items_order_callback, &check);
    assert(val == 0);

    libvlc_media_list_player_play(mlp);

    // Wait until all item are read
    wait_queued_items(&check);

    stop_and_wait (mlp);

    libvlc_media_list_player_release (mlp);
    libvlc_release (vlc);
}

static void test_media_list_player_previous(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = test_default_sample;

    test_log ("Testing media player previous()\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location(file);
    assert(md);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    mlp = libvlc_media_list_player_new (vlc, NULL, NULL);
    assert(mlp);

    libvlc_media_list_add_media (ml, md);

    // Add three media
    media_list_add_file_path(ml, file);
    media_list_add_file_path(ml, file);
    media_list_add_file_path(ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml);

    libvlc_media_list_player_play_item (mlp, md);

    wait_playing (mlp);

    libvlc_media_release (md);

    libvlc_media_list_player_previous (mlp);

    wait_playing (mlp);

    libvlc_media_list_player_pause (mlp);
    libvlc_media_list_player_previous (mlp);

    wait_playing (mlp);

    stop_and_wait (mlp);

    libvlc_media_list_player_previous (mlp);

    wait_playing (mlp);

    stop_and_wait (mlp);

    libvlc_media_list_player_release (mlp);
    libvlc_release (vlc);
}

static void test_media_list_player_next(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = test_default_sample;

    test_log ("Testing media player next()\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location(file);
    assert(md);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    mlp = libvlc_media_list_player_new (vlc, NULL, NULL);
    assert(mlp);

    libvlc_media_list_add_media (ml, md);

    // Add three media
    media_list_add_file_path(ml, file);
    media_list_add_file_path(ml, file);
    media_list_add_file_path(ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml);

    libvlc_media_list_player_play_item (mlp, md);

    libvlc_media_release (md);

    wait_playing (mlp);

    libvlc_media_list_player_next (mlp);

    wait_playing (mlp);

    libvlc_media_list_player_pause (mlp);
    libvlc_media_list_player_next (mlp);

    wait_playing (mlp);

    stop_and_wait (mlp);

    libvlc_media_list_player_next (mlp);

    wait_playing (mlp);

    stop_and_wait (mlp);

    libvlc_media_list_player_release (mlp);
    libvlc_release (vlc);
}

static void test_media_list_player_pause_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = test_default_sample;

    test_log ("Testing play and pause of %s using the media list.\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location(file);
    assert(md);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    mlp = libvlc_media_list_player_new (vlc, NULL, NULL);
    assert(mlp);

    libvlc_media_list_add_media( ml, md);

    libvlc_media_list_player_set_media_list( mlp, ml );

    libvlc_media_list_player_play_item( mlp, md );

    wait_playing (mlp);

    libvlc_media_list_player_pause (mlp);

    stop_and_wait (mlp);

    libvlc_media_release (md);
    libvlc_media_list_player_release (mlp);
    libvlc_release (vlc);
}

static void test_media_list_player_play_item_at_index(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = test_default_sample;

    test_log ("Testing play_item_at_index of %s using the media list.\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location(file);
    assert(md);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    mlp = libvlc_media_list_player_new (vlc, NULL, NULL);
    assert(mlp);

    for (unsigned i = 0; i < 5; i++)
        libvlc_media_list_add_media( ml, md );

    libvlc_media_list_player_set_media_list( mlp, ml );
    libvlc_media_list_player_play_item_at_index( mlp, 0 );

    wait_playing (mlp);

    stop_and_wait (mlp);

    libvlc_media_release (md);
    libvlc_media_list_player_release (mlp);
    libvlc_release (vlc);
}

int main (void)
{
    test_init();

    // There are 6 tests. And they take some times.
    alarm(6 * 5);

    test_media_list_player_pause_stop (test_defaults_args, test_defaults_nargs);
    test_media_list_player_play_item_at_index (test_defaults_args, test_defaults_nargs);
    test_media_list_player_previous (test_defaults_args, test_defaults_nargs);
    test_media_list_player_next (test_defaults_args, test_defaults_nargs);
    test_media_list_player_items_queue (test_defaults_args, test_defaults_nargs);
    return 0;
}
