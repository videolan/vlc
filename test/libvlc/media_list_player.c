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
    check->state = libvlc_NothingSpecial;
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

/* Long enough to never reach EOS on its own during a test: the tests below
 * must control when the current media changes or ends, whatever the load. */
#define LONG_SAMPLE "mock://length=100000000"

static void test_media_list_player_set_media_list_while_playing(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_list_t *ml;
    libvlc_media_list_t *ml_shuffled;
    libvlc_media_list_player_t *mlp;

    const char * file = LONG_SAMPLE;

    test_log ("Testing set_media_list() while playing, current media "
              "still present (shuffle case)\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    void *id0 = media_list_add_file_path(ml, file);
    void *id1 = media_list_add_file_path(ml, file);
    void *id2 = media_list_add_file_path(ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml);

    int ret = libvlc_media_list_player_play_item (mlp, id1);
    assert(ret == 0);

    wait_item (&check, id1);

    vlc_mutex_lock(&check.lock);
    unsigned item_count_before = check.item_count;
    vlc_mutex_unlock(&check.lock);

    /* A shuffled copy of the same list: same media_t identities as ml,
     * but reordered, with id1 (currently playing) moved to the middle. */
    ml_shuffled = libvlc_media_list_new ();
    assert (ml_shuffled != NULL);
    libvlc_media_list_add_media (ml_shuffled, id2);
    libvlc_media_list_add_media (ml_shuffled, id1);
    libvlc_media_list_add_media (ml_shuffled, id0);

    libvlc_media_list_player_set_media_list (mlp, ml_shuffled);

    /* Playback must not have been interrupted/restarted by the swap */
    libvlc_media_player_t *mp = libvlc_media_list_player_get_media_player (mlp);
    libvlc_media_t *md = libvlc_media_player_get_media (mp);
    assert(md == id1);
    libvlc_media_release (md);

    /* The next media must follow ml_shuffled's order (id0), not ml's
     * stale order (which would have been id2) */
    md = libvlc_media_player_get_next_media (mp);
    assert(md == id0);
    libvlc_media_release (md);
    libvlc_media_player_release (mp);

    vlc_mutex_lock(&check.lock);
    assert(check.current_item == id1);
    assert(check.item_count == item_count_before);
    vlc_mutex_unlock(&check.lock);

    /* So must next(): it is the only media change that can happen from
     * now on. */
    ret = libvlc_media_list_player_next (mlp);
    assert(ret == 0);
    wait_item_count (&check, item_count_before + 1);

    vlc_mutex_lock(&check.lock);
    assert(check.current_item == id0);
    vlc_mutex_unlock(&check.lock);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_media_list_release (ml_shuffled);
    libvlc_release (vlc);
}

static void test_media_list_player_set_media_list_after_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_list_t *ml;
    libvlc_media_list_player_t *mlp;

    const char * file = LONG_SAMPLE;

    test_log ("Testing set_media_list() after an explicit stop doesn't "
              "resurrect the stopped position\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    void *id0 = media_list_add_file_path(ml, file);
    media_list_add_file_path(ml, file);
    media_list_add_file_path(ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml);
    libvlc_media_list_player_play (mlp);

    wait_playing (&check);
    wait_item (&check, id0);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    /* Forget about the first playback, so that the waits below can only
     * be satisfied by the playback started by next() */
    vlc_mutex_lock(&check.lock);
    check.state = libvlc_NothingSpecial;
    check.current_item = NULL;
    unsigned item_count_before = check.item_count;
    vlc_mutex_unlock(&check.lock);

    /* Re-installing the list after a completed stop must not carry the
     * stopped position or its queued next media over. */
    libvlc_media_list_player_set_media_list (mlp, ml);

    int ret = libvlc_media_list_player_next (mlp);
    assert(ret == 0);
    wait_playing (&check);
    wait_item_count (&check, item_count_before + 1);

    vlc_mutex_lock(&check.lock);
    assert(check.current_item == id0);
    vlc_mutex_unlock(&check.lock);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_release (vlc);
}

static void test_media_list_player_set_media_list_not_found(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_list_t *ml;
    libvlc_media_list_t *ml2;
    libvlc_media_list_player_t *mlp;

    const char * file = LONG_SAMPLE;

    test_log ("Testing set_media_list() when the currently playing media "
              "can't be identified in the new list\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    void *id0 = media_list_add_file_path(ml, file);
    media_list_add_file_path(ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml);
    int ret = libvlc_media_list_player_play_item_at_index (mlp, 0);
    assert(ret == 0);

    wait_item (&check, id0);

    /* A brand new list built from different media_t objects: id0 can't
     * be identified in it by identity. */
    ml2 = libvlc_media_list_new ();
    assert (ml2 != NULL);
    void *id2_0 = media_list_add_file_path(ml2, file);

    libvlc_media_list_player_set_media_list (mlp, ml2);

    /* Nothing that followed id0 is part of the new list: playback stops */
    wait_stopped (&check);

    /* Must not get stuck on the stale index: play() falls back to the
     * start of the new list. */
    libvlc_media_list_player_play (mlp);
    wait_item (&check, id2_0);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_media_list_release (ml2);
    libvlc_release (vlc);
}

static void test_media_list_player_set_media_list_not_found_repeat(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_list_t *ml;
    libvlc_media_list_t *ml2;
    libvlc_media_list_player_t *mlp;

    const char * file = LONG_SAMPLE;

    test_log ("Testing set_media_list() in repeat mode when the currently "
              "playing media can't be identified in the new list\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    libvlc_media_list_player_set_playback_mode (mlp, libvlc_playback_mode_repeat);

    void *id0 = media_list_add_file_path(ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml);
    int ret = libvlc_media_list_player_play_item_at_index (mlp, 0);
    assert(ret == 0);

    wait_playing (&check);
    wait_item (&check, id0);

    /* In repeat mode, id0 is queued as its own next media */
    libvlc_media_player_t *mp = libvlc_media_list_player_get_media_player (mlp);
    libvlc_media_t *md = libvlc_media_player_get_next_media (mp);
    assert(md == id0);
    libvlc_media_release (md);
    libvlc_media_player_release (mp);

    vlc_mutex_lock(&check.lock);
    unsigned item_count_before = check.item_count;
    vlc_mutex_unlock(&check.lock);

    /* Unrelated new list: id0 can't be identified in it, so playback must
     * stop instead of resurrecting id0 via the queued next media. */
    ml2 = libvlc_media_list_new ();
    assert (ml2 != NULL);
    media_list_add_file_path(ml2, file);

    libvlc_media_list_player_set_media_list (mlp, ml2);
    wait_stopped (&check);

    vlc_mutex_lock(&check.lock);
    assert(check.item_count == item_count_before);
    vlc_mutex_unlock(&check.lock);

    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_media_list_release (ml2);
    libvlc_release (vlc);
}

static void test_media_list_player_set_media_list_not_found_hierarchical(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_list_t *ml;
    libvlc_media_list_t *ml2;
    libvlc_media_list_player_t *mlp;

    static const char * file = LONG_SAMPLE;
    static const char * file_node = "mock://node_count=3;length=100000000";

    test_log ("Testing set_media_list() when the current position is a "
              "multi-level path into a node, and the new list doesn't "
              "contain the currently playing media\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    /* item 0: flat media, item 1: a node with 3 sub-media */
    media_list_add_file_path(ml, file);
    media_list_add_file_path(ml, file_node);

    libvlc_media_list_player_set_media_list (mlp, ml);

    /* Play the node directly: it is expanded, then its first sub-media
     * plays, so that current_playing_item_path is a multi-level path */
    int ret = libvlc_media_list_player_play_item_at_index (mlp, 1);
    assert(ret == 0);
    wait_item_count (&check, 2);

    /* An unrelated, shorter list: the currently playing sub-item can't
     * be found in it, and its top-level index is out of range. */
    ml2 = libvlc_media_list_new ();
    assert (ml2 != NULL);
    void *id2_0 = media_list_add_file_path(ml2, file);

    libvlc_media_list_player_set_media_list (mlp, ml2);
    wait_stopped (&check);

    /* Must not crash, and must recover cleanly */
    ret = libvlc_media_list_player_next (mlp);
    assert(ret == 0);
    wait_item (&check, id2_0);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_media_list_release (ml2);
    libvlc_release (vlc);
}

static void test_media_list_player_set_media_list_current_removed(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_list_t *ml;
    libvlc_media_list_t *ml2;
    libvlc_media_list_player_t *mlp;

    const char * file = LONG_SAMPLE;

    test_log ("Testing set_media_list() when the currently playing media "
              "was removed from the list\n");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    void *id0 = media_list_add_file_path(ml, file);
    void *id1 = media_list_add_file_path(ml, file);
    void *id2 = media_list_add_file_path(ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml);

    int ret = libvlc_media_list_player_play_item (mlp, id1);
    assert(ret == 0);
    wait_item (&check, id1);

    vlc_mutex_lock(&check.lock);
    unsigned item_count_before = check.item_count;
    vlc_mutex_unlock(&check.lock);

    /* [id0, id1, id2] -> [id0, id2] while id1 is playing */
    ml2 = libvlc_media_list_new ();
    assert (ml2 != NULL);
    libvlc_media_list_add_media (ml2, id0);
    libvlc_media_list_add_media (ml2, id2);

    libvlc_media_list_player_set_media_list (mlp, ml2);

    /* id1 must be stopped right away, continuing with the media that
     * followed it: id2 */
    wait_item_count (&check, item_count_before + 1);

    vlc_mutex_lock(&check.lock);
    assert(check.current_item == id2);
    vlc_mutex_unlock(&check.lock);

    libvlc_media_list_player_stop_async (mlp);
    wait_stopped (&check);

    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_media_list_release (ml2);
    libvlc_release (vlc);
}

static void test_media_list_player_set_media_list_last_removed(const char** argv, int argc,
                                                               libvlc_playback_mode_t mode)
{
    libvlc_instance_t *vlc;
    libvlc_media_list_t *ml;
    libvlc_media_list_t *ml2;
    libvlc_media_list_player_t *mlp;

    const char * file = LONG_SAMPLE;
    bool b_loop = mode == libvlc_playback_mode_loop;

    test_log ("Testing set_media_list() when the currently playing media "
              "was the last one and was removed from the list (%s)\n",
              b_loop ? "loop" : "default");

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    ml = libvlc_media_list_new ();
    assert (ml != NULL);

    struct check_items_order_data check;
    check_data_init(&check);

    mlp = libvlc_media_list_player_new (vlc, &cbs, &check);
    assert(mlp);

    libvlc_media_list_player_set_playback_mode (mlp, mode);

    void *id0 = media_list_add_file_path(ml, file);
    void *id1 = media_list_add_file_path(ml, file);
    void *id2 = media_list_add_file_path(ml, file);

    libvlc_media_list_player_set_media_list (mlp, ml);

    int ret = libvlc_media_list_player_play_item (mlp, id2);
    assert(ret == 0);
    wait_item (&check, id2);

    vlc_mutex_lock(&check.lock);
    unsigned item_count_before = check.item_count;
    vlc_mutex_unlock(&check.lock);

    /* [id0, id1, id2] -> [id1, id0] while id2 is playing */
    ml2 = libvlc_media_list_new ();
    assert (ml2 != NULL);
    libvlc_media_list_add_media (ml2, id1);
    libvlc_media_list_add_media (ml2, id0);

    libvlc_media_list_player_set_media_list (mlp, ml2);

    if (b_loop)
    {
        /* Wrapping around the old list, id0 followed id2 */
        wait_item_count (&check, item_count_before + 1);

        vlc_mutex_lock(&check.lock);
        assert(check.current_item == id0);
        vlc_mutex_unlock(&check.lock);

        libvlc_media_list_player_stop_async (mlp);
    }

    /* Without looping, nothing followed id2: playback stops */
    wait_stopped (&check);

    vlc_mutex_lock(&check.lock);
    assert(check.item_count == item_count_before + b_loop);
    vlc_mutex_unlock(&check.lock);

    libvlc_media_list_player_release (mlp);
    libvlc_media_list_release (ml);
    libvlc_media_list_release (ml2);
    libvlc_release (vlc);
}

int main (void)
{
    test_init();

    // There are 13 tests. And they take some times.
    alarm(13 * 5);

    test_media_list_player_pause_stop (test_defaults_args, test_defaults_nargs);
    test_media_list_player_play_item_at_index (test_defaults_args, test_defaults_nargs);
    test_media_list_player_previous (test_defaults_args, test_defaults_nargs);
    test_media_list_player_next (test_defaults_args, test_defaults_nargs);
    test_media_list_player_items_queue (test_defaults_args, test_defaults_nargs);
    test_media_list_player_set_media_list_while_playing (test_defaults_args, test_defaults_nargs);
    test_media_list_player_set_media_list_after_stop (test_defaults_args, test_defaults_nargs);
    test_media_list_player_set_media_list_not_found (test_defaults_args, test_defaults_nargs);
    test_media_list_player_set_media_list_not_found_repeat (test_defaults_args, test_defaults_nargs);
    test_media_list_player_set_media_list_not_found_hierarchical (test_defaults_args, test_defaults_nargs);
    test_media_list_player_set_media_list_current_removed (test_defaults_args, test_defaults_nargs);
    test_media_list_player_set_media_list_last_removed (test_defaults_args, test_defaults_nargs, libvlc_playback_mode_default);
    test_media_list_player_set_media_list_last_removed (test_defaults_args, test_defaults_nargs, libvlc_playback_mode_loop);
    return 0;
}
