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

#include "../../lib/libvlc_internal.h"

#define DEFAULT_SAMPLE "mock://"

struct check_items_order_data {
    vlc_mutex_t lock;
    vlc_cond_t wait;
    libvlc_state_t state;
    void *current_item;
    unsigned item_count;
};

static void* media_list_add_file_path(libvlc_media_list_t *ml, const char * file_path)
{
    libvlc_media_t *md = libvlc_media_new_location(file_path);
    int ret = libvlc_media_list_add_media (ml, md);
    assert(ret == 0);
    libvlc_media_release (md);
    return md;
}

static void check_data_init(struct check_items_order_data *check)
{
    vlc_mutex_init(&check->lock);
    vlc_cond_init(&check->wait);
    check->current_item = NULL;
    check->item_count = 0;
}

static void wait_item(struct check_items_order_data *check, void *id)
{
    vlc_mutex_lock(&check->lock);
    while (check->current_item != id)
        vlc_cond_wait(&check->wait, &check->lock);
    vlc_mutex_unlock(&check->lock);
}

static void wait_item_count(struct check_items_order_data *check, unsigned count)
{
    vlc_mutex_lock(&check->lock);
    while (check->item_count < count)
        vlc_cond_wait(&check->wait, &check->lock);
    vlc_mutex_unlock(&check->lock);
}

static void wait_playing(struct check_items_order_data *check)
{
    vlc_mutex_lock(&check->lock);
    while (check->state != libvlc_Playing)
        vlc_cond_wait(&check->wait, &check->lock);
    vlc_mutex_unlock(&check->lock);
}

static void wait_stopped(struct check_items_order_data *check)
{
    vlc_mutex_lock(&check->lock);
    while (check->state != libvlc_Stopped)
        vlc_cond_wait(&check->wait, &check->lock);
    vlc_mutex_unlock(&check->lock);
}

static void on_media_changed(void *opaque, libvlc_media_t *md)
{
    struct check_items_order_data *check = opaque;

    vlc_mutex_lock(&check->lock);
    check->current_item = md;
    check->item_count++;
    vlc_cond_signal(&check->wait);
    vlc_mutex_unlock(&check->lock);
}

static void on_state_changed(void *opaque, libvlc_state_t state)
{
    struct check_items_order_data *check = opaque;
    vlc_mutex_lock(&check->lock);
    check->state = state;
    vlc_cond_signal(&check->wait);
    vlc_mutex_unlock(&check->lock);
}

static const struct libvlc_media_player_cbs cbs = {
    .version = 0,
    .on_media_changed = on_media_changed,
    .on_state_changed = on_state_changed,
};

static void test_media_list_player_items_queue(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    static const char * file = "mock://length=10000"; /*10 ms sample for the test */
    static const char * file_node = "mock://node_count=3;length=10000";

    test_log ("Testing media player item queue-ing\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    // Add 3 normal media
    media_list_add_file_path(ml, file);
    media_list_add_file_path(ml, file);
    media_list_add_file_path(ml, file);

    // Add a node with 3 sub media, that is 4 media
    media_list_add_file_path(ml, file_node);

    // Add 1 more media
    media_list_add_file_path(ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml);

    libvlc_media_list_player_play(mlp);

    // Wait until all items are read
    wait_item_count(&check, 8);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_release (vlc);
}

static void test_media_list_player_previous(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    int ret;
    const char * file = DEFAULT_SAMPLE;

    test_log ("Testing media player previous()\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location(file);
    assert(md);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);

    assert(mlp);

    // Add three media
    void *id0 = media_list_add_file_path(ml, file);
    void *id1 = media_list_add_file_path(ml, file);
    void *id2 = media_list_add_file_path(ml, file);

    libvlc_media_list_add_media (ml, md);

    libvlc_media_list_player_set_media_list (mlp, ml);

    ret = libvlc_media_list_player_play_item (mlp, md);
    assert(ret == 0);

    wait_playing (&check);

    libvlc_media_release (md);

    ret = libvlc_media_list_player_previous (mlp);
    assert(ret == 0);

    /* don't wait playing since playback was not interrupted */
    wait_item (&check, id2);

    libvlc_media_list_player_pause (mlp);
    ret = libvlc_media_list_player_previous (mlp);
    assert(ret == 0);

    wait_playing (&check);
    wait_item (&check, id1);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    ret = libvlc_media_list_player_previous (mlp);
    assert(ret == 0);

    wait_playing (&check);
    wait_item (&check, id0);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_release (vlc);
}

static void test_media_list_player_next(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = DEFAULT_SAMPLE;

    test_log ("Testing media player next()\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location(file);
    assert(md);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    libvlc_media_list_add_media (ml, md);
    void *id0 = md;

    // Add three media
    void *id1 = media_list_add_file_path(ml, file);
    void *id2 = media_list_add_file_path(ml, file);
    void *id3 = media_list_add_file_path(ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml);

    libvlc_media_list_player_play_item (mlp, md);

    libvlc_media_release (md);

    wait_playing (&check);

    libvlc_media_list_player_next (mlp);

    /* don't wait playing since playback was not interrupted */
    wait_item (&check, id1);

    libvlc_media_list_player_pause (mlp);
    libvlc_media_list_player_next (mlp);

    wait_playing (&check);
    wait_item (&check, id2);

    libvlc_media_list_player_next (mlp);

    wait_playing (&check);
    wait_item (&check, id3);

    int ret = libvlc_media_list_player_next (mlp);
    assert(ret == -1); /* no next items */

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    /* Go back to start after a stop */
    libvlc_media_list_player_next (mlp);
    wait_playing (&check);
    wait_item (&check, id0);

    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_release (vlc);
}

static void test_media_list_player_pause_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = DEFAULT_SAMPLE;

    test_log ("Testing play and pause of %s using the media list.\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location(file);
    assert(md);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    libvlc_media_list_add_media( ml, md);

    libvlc_media_list_player_set_media_list( mlp, ml );

    libvlc_media_list_player_play_item( mlp, md );

    wait_playing (&check);

    libvlc_media_list_player_pause (mlp);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    libvlc_media_release (md);
    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_release (vlc);
}

static void test_media_list_player_play_item_at_index(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = DEFAULT_SAMPLE;

    test_log ("Testing play_item_at_index of %s using the media list.\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_location(file);
    assert(md);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    for (unsigned i = 0; i < 5; i++)
        libvlc_media_list_add_media( ml, md );

    libvlc_media_list_player_set_media_list( mlp, ml );
    libvlc_media_list_player_play_item_at_index( mlp, 0 );

    wait_playing (&check);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    libvlc_media_release (md);
    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
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
