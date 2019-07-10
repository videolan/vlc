/*****************************************************************************
 * playlist/test.c
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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

#ifndef DOC

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include "item.h"
#include "playlist.h"
#include "preparse.h"

/* the playlist lock is the one of the player */
# define vlc_playlist_Lock(p) VLC_UNUSED(p);
# define vlc_playlist_Unlock(p) VLC_UNUSED(p);

static input_item_t *
CreateDummyMedia(int num)
{
    char *url;
    char *name;

    int res = asprintf(&url, "vlc://item-%d", num);
    if (res == -1)
        return NULL;

    res = asprintf(&name, "item-%d", num);
    if (res == -1)
        return NULL;

    input_item_t *media = input_item_New(url, name);
    free(url);
    free(name);
    return media;
}

static void
CreateDummyMediaArray(input_item_t *out[], size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        out[i] = CreateDummyMedia(i);
        assert(out[i]);
    }
}

static void
DestroyMediaArray(input_item_t *const array[], size_t count)
{
    for (size_t i = 0; i < count; ++i)
        input_item_Release(array[i]);
}

#define EXPECT_AT(index, id) \
    assert(vlc_playlist_Get(playlist, index)->media == media[id])

static void
test_append(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* append one by one */
    for (int i = 0; i < 5; ++i)
    {
        int ret = vlc_playlist_AppendOne(playlist, media[i]);
        assert(ret == VLC_SUCCESS);
    }

    /* append several at once */
    int ret = vlc_playlist_Append(playlist, &media[5], 5);
    assert(ret == VLC_SUCCESS);

    assert(vlc_playlist_Count(playlist) == 10);
    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 2);
    EXPECT_AT(3, 3);
    EXPECT_AT(4, 4);
    EXPECT_AT(5, 5);
    EXPECT_AT(6, 6);
    EXPECT_AT(7, 7);
    EXPECT_AT(8, 8);
    EXPECT_AT(9, 9);

    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_insert(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[15];
    CreateDummyMediaArray(media, 15);

    /* initial playlist with 5 items */
    int ret = vlc_playlist_Append(playlist, media, 5);
    assert(ret == VLC_SUCCESS);

    /* insert one by one */
    for (int i = 0; i < 5; ++i)
    {
        ret = vlc_playlist_InsertOne(playlist, 2, media[i + 5]);
        assert(ret == VLC_SUCCESS);
    }

    /* insert several at once */
    ret = vlc_playlist_Insert(playlist, 6, &media[10], 5);
    assert(ret == VLC_SUCCESS);

    assert(vlc_playlist_Count(playlist) == 15);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);

    EXPECT_AT(2, 9);
    EXPECT_AT(3, 8);
    EXPECT_AT(4, 7);
    EXPECT_AT(5, 6);

    EXPECT_AT(6, 10);
    EXPECT_AT(7, 11);
    EXPECT_AT(8, 12);
    EXPECT_AT(9, 13);
    EXPECT_AT(10, 14);

    EXPECT_AT(11, 5);
    EXPECT_AT(12, 2);
    EXPECT_AT(13, 3);
    EXPECT_AT(14, 4);

    DestroyMediaArray(media, 15);
    vlc_playlist_Delete(playlist);
}

static void
test_move(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    /* move slice {3, 4, 5, 6} so that its new position is 5 */
    vlc_playlist_Move(playlist, 3, 4, 5);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 2);
    EXPECT_AT(3, 7);
    EXPECT_AT(4, 8);
    EXPECT_AT(5, 3);
    EXPECT_AT(6, 4);
    EXPECT_AT(7, 5);
    EXPECT_AT(8, 6);
    EXPECT_AT(9, 9);

    /* move it back to its original position */
    vlc_playlist_Move(playlist, 5, 4, 3);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 2);
    EXPECT_AT(3, 3);
    EXPECT_AT(4, 4);
    EXPECT_AT(5, 5);
    EXPECT_AT(6, 6);
    EXPECT_AT(7, 7);
    EXPECT_AT(8, 8);
    EXPECT_AT(9, 9);

    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_remove(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    /* remove one by one */
    for (int i = 0; i < 3; ++i)
        vlc_playlist_RemoveOne(playlist, 2);

    /* remove several at once */
    vlc_playlist_Remove(playlist, 3, 2);

    assert(vlc_playlist_Count(playlist) == 5);
    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 5);
    EXPECT_AT(3, 8);
    EXPECT_AT(4, 9);

    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_clear(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    assert(vlc_playlist_Count(playlist) == 10);
    vlc_playlist_Clear(playlist);
    assert(vlc_playlist_Count(playlist) == 0);

    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_expand_item(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[16];
    CreateDummyMediaArray(media, 16);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    /* create a subtree for item 8 with 4 children */
    input_item_t *item_to_expand = playlist->items.data[8]->media;
    input_item_node_t *root = input_item_node_Create(item_to_expand);
    for (int i = 0; i < 4; ++i)
    {
        input_item_node_t *node = input_item_node_AppendItem(root,
                                                             media[i + 10]);
        assert(node);
    }

    /* on the 3rd children, add 2 grand-children */
    input_item_node_t *parent = root->pp_children[2];
    for (int i = 0; i < 2; ++i)
    {
        input_item_node_t *node = input_item_node_AppendItem(parent,
                                                             media[i + 14]);
        assert(node);
    }

    playlist->current = 8;
    playlist->has_prev = true;
    playlist->has_next = true;

    ret = vlc_playlist_ExpandItem(playlist, 8, root);
    assert(ret == VLC_SUCCESS);
    assert(vlc_playlist_Count(playlist) == 15);
    EXPECT_AT(7, 7);

    EXPECT_AT(8, 10);
    EXPECT_AT(9, 11);
    EXPECT_AT(10, 12);

    EXPECT_AT(11, 14);
    EXPECT_AT(12, 15);

    EXPECT_AT(13, 13);

    EXPECT_AT(14, 9);

    /* item 8 will be replaced, the current must stay the same */
    assert(playlist->current == 8);

    input_item_node_Delete(root);
    DestroyMediaArray(media, 16);
    vlc_playlist_Delete(playlist);
}

struct playlist_state
{
    size_t playlist_size;
    ssize_t current;
    bool has_prev;
    bool has_next;
};

static void
playlist_state_init(struct playlist_state *state, vlc_playlist_t *playlist)
{
    state->playlist_size = vlc_playlist_Count(playlist);
    state->current = vlc_playlist_GetCurrentIndex(playlist);
    state->has_prev = vlc_playlist_HasPrev(playlist);
    state->has_next = vlc_playlist_HasNext(playlist);
}

struct items_reset_report
{
    size_t count;
    struct playlist_state state;
};

struct items_added_report
{
    size_t index;
    size_t count;
    struct playlist_state state;
};

struct items_moved_report
{
    size_t index;
    size_t count;
    size_t target;
    struct playlist_state state;
};

struct items_removed_report
{
    size_t index;
    size_t count;
    struct playlist_state state;
};

struct playback_repeat_changed_report
{
    enum vlc_playlist_playback_repeat repeat;
};

struct playback_order_changed_report
{
    enum vlc_playlist_playback_order order;
};

struct current_index_changed_report
{
    ssize_t current;
};

struct has_prev_changed_report
{
    bool has_prev;
};

struct has_next_changed_report
{
    bool has_next;
};

struct callback_ctx
{
    struct VLC_VECTOR(struct items_reset_report)           vec_items_reset;
    struct VLC_VECTOR(struct items_added_report)           vec_items_added;
    struct VLC_VECTOR(struct items_moved_report)           vec_items_moved;
    struct VLC_VECTOR(struct items_removed_report)         vec_items_removed;
    struct VLC_VECTOR(struct playback_order_changed_report)
                                                  vec_playback_order_changed;
    struct VLC_VECTOR(struct playback_repeat_changed_report)
                                                  vec_playback_repeat_changed;
    struct VLC_VECTOR(struct current_index_changed_report)
                                                  vec_current_index_changed;
    struct VLC_VECTOR(struct has_prev_changed_report)      vec_has_prev_changed;
    struct VLC_VECTOR(struct has_next_changed_report)      vec_has_next_changed;
};

#define CALLBACK_CTX_INITIALIZER \
{ \
    VLC_VECTOR_INITIALIZER, \
    VLC_VECTOR_INITIALIZER, \
    VLC_VECTOR_INITIALIZER, \
    VLC_VECTOR_INITIALIZER, \
    VLC_VECTOR_INITIALIZER, \
    VLC_VECTOR_INITIALIZER, \
    VLC_VECTOR_INITIALIZER, \
    VLC_VECTOR_INITIALIZER, \
    VLC_VECTOR_INITIALIZER, \
}

static inline void
callback_ctx_reset(struct callback_ctx *ctx)
{
    vlc_vector_clear(&ctx->vec_items_reset);
    vlc_vector_clear(&ctx->vec_items_added);
    vlc_vector_clear(&ctx->vec_items_moved);
    vlc_vector_clear(&ctx->vec_items_removed);
    vlc_vector_clear(&ctx->vec_playback_repeat_changed);
    vlc_vector_clear(&ctx->vec_playback_order_changed);
    vlc_vector_clear(&ctx->vec_current_index_changed);
    vlc_vector_clear(&ctx->vec_has_prev_changed);
    vlc_vector_clear(&ctx->vec_has_next_changed);
};

static inline void
callback_ctx_destroy(struct callback_ctx *ctx)
{
    vlc_vector_destroy(&ctx->vec_items_reset);
    vlc_vector_destroy(&ctx->vec_items_added);
    vlc_vector_destroy(&ctx->vec_items_moved);
    vlc_vector_destroy(&ctx->vec_items_removed);
    vlc_vector_destroy(&ctx->vec_playback_repeat_changed);
    vlc_vector_destroy(&ctx->vec_playback_order_changed);
    vlc_vector_destroy(&ctx->vec_current_index_changed);
    vlc_vector_destroy(&ctx->vec_has_prev_changed);
    vlc_vector_destroy(&ctx->vec_has_next_changed);
};

static void
callback_on_items_reset(vlc_playlist_t *playlist,
                        vlc_playlist_item_t *const items[], size_t count,
                        void *userdata)
{
    VLC_UNUSED(items);
    struct callback_ctx *ctx = userdata;

    struct items_reset_report report;
    report.count = count;
    playlist_state_init(&report.state, playlist);
    vlc_vector_push(&ctx->vec_items_reset, report);
}

static void
callback_on_items_added(vlc_playlist_t *playlist, size_t index,
                        vlc_playlist_item_t *const items[], size_t count,
                        void *userdata)
{
    VLC_UNUSED(items);
    struct callback_ctx *ctx = userdata;

    struct items_added_report report;
    report.index = index;
    report.count = count;
    playlist_state_init(&report.state, playlist);
    vlc_vector_push(&ctx->vec_items_added, report);
}

static void
callback_on_items_moved(vlc_playlist_t *playlist, size_t index, size_t count,
                        size_t target, void *userdata)
{
    struct callback_ctx *ctx = userdata;

    struct items_moved_report report;
    report.index = index;
    report.count = count;
    report.target = target;
    playlist_state_init(&report.state, playlist);
    vlc_vector_push(&ctx->vec_items_moved, report);
}

static void
callback_on_items_removed(vlc_playlist_t *playlist, size_t index, size_t count,
                          void *userdata)
{
    struct callback_ctx *ctx = userdata;

    struct items_removed_report report;
    report.index = index;
    report.count = count;
    playlist_state_init(&report.state, playlist);
    vlc_vector_push(&ctx->vec_items_removed, report);
}

static void
callback_on_playback_repeat_changed(vlc_playlist_t *playlist,
                                    enum vlc_playlist_playback_repeat repeat,
                                    void *userdata)
{
    VLC_UNUSED(playlist);
    struct callback_ctx *ctx = userdata;

    struct playback_repeat_changed_report report;
    report.repeat = repeat;
    vlc_vector_push(&ctx->vec_playback_repeat_changed, report);
}

static void
callback_on_playback_order_changed(vlc_playlist_t *playlist,
                                   enum vlc_playlist_playback_order order,
                                   void *userdata)
{
    VLC_UNUSED(playlist);
    struct callback_ctx *ctx = userdata;

    struct playback_order_changed_report report;
    report.order = order;
    vlc_vector_push(&ctx->vec_playback_order_changed, report);
}

static void
callback_on_current_index_changed(vlc_playlist_t *playlist, ssize_t index,
                                  void *userdata)
{
    VLC_UNUSED(playlist);
    struct callback_ctx *ctx = userdata;

    struct current_index_changed_report report;
    report.current = index;
    vlc_vector_push(&ctx->vec_current_index_changed, report);
}

static void
callback_on_has_prev_changed(vlc_playlist_t *playlist, bool has_prev,
                             void *userdata)
{
    VLC_UNUSED(playlist);
    struct callback_ctx *ctx = userdata;

    struct has_prev_changed_report report;
    report.has_prev = has_prev;
    vlc_vector_push(&ctx->vec_has_prev_changed, report);
}

static void
callback_on_has_next_changed(vlc_playlist_t *playlist, bool has_next,
                             void *userdata)
{
    VLC_UNUSED(playlist);
    struct callback_ctx *ctx = userdata;

    struct has_next_changed_report report;
    report.has_next = has_next;
    vlc_vector_push(&ctx->vec_has_next_changed, report);
}

static void
test_items_added_callbacks(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    struct vlc_playlist_callbacks cbs = {
        .on_items_added = callback_on_items_added,
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    int ret = vlc_playlist_AppendOne(playlist, media[0]);
    assert(ret == VLC_SUCCESS);

    /* the callbacks must be called with *all* values up to date */
    assert(ctx.vec_items_added.size == 1);
    assert(ctx.vec_items_added.data[0].index == 0);
    assert(ctx.vec_items_added.data[0].count == 1);
    assert(ctx.vec_items_added.data[0].state.playlist_size == 1);
    assert(ctx.vec_items_added.data[0].state.current == -1);
    assert(!ctx.vec_items_added.data[0].state.has_prev);
    assert(ctx.vec_items_added.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 0);

    assert(ctx.vec_has_prev_changed.size == 0);

    assert(ctx.vec_has_next_changed.size == 1);
    assert(ctx.vec_has_next_changed.data[0].has_next);

    callback_ctx_reset(&ctx);

    /* set the only item as current */
    playlist->current = 0;
    playlist->has_prev = false;
    playlist->has_next = false;

    /* insert before the current item */
    ret = vlc_playlist_Insert(playlist, 0, &media[1], 4);
    assert(ret == VLC_SUCCESS);

    assert(ctx.vec_items_added.size == 1);
    assert(ctx.vec_items_added.data[0].index == 0);
    assert(ctx.vec_items_added.data[0].count == 4);
    assert(ctx.vec_items_added.data[0].state.playlist_size == 5);
    assert(ctx.vec_items_added.data[0].state.current == 4); /* shifted */
    assert(ctx.vec_items_added.data[0].state.has_prev);
    assert(!ctx.vec_items_added.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 4);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_reset(&ctx);

    /* append (after the current item) */
    ret = vlc_playlist_Append(playlist, &media[5], 5);
    assert(ret == VLC_SUCCESS);

    assert(ctx.vec_items_added.size == 1);
    assert(ctx.vec_items_added.data[0].index == 5);
    assert(ctx.vec_items_added.data[0].count == 5);
    assert(ctx.vec_items_added.data[0].state.playlist_size == 10);
    assert(ctx.vec_items_added.data[0].state.current == 4);
    assert(ctx.vec_items_added.data[0].state.has_prev);
    assert(ctx.vec_items_added.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 0);

    assert(ctx.vec_has_prev_changed.size == 0);

    assert(ctx.vec_has_next_changed.size == 1);
    assert(ctx.vec_has_next_changed.data[0].has_next);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_items_moved_callbacks(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_moved = callback_on_items_moved,
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_Move(playlist, 2, 3, 5);

    assert(ctx.vec_items_moved.size == 1);
    assert(ctx.vec_items_moved.data[0].index == 2);
    assert(ctx.vec_items_moved.data[0].count == 3);
    assert(ctx.vec_items_moved.data[0].target == 5);
    assert(ctx.vec_items_moved.data[0].state.playlist_size == 10);
    assert(ctx.vec_items_moved.data[0].state.current == -1);
    assert(!ctx.vec_items_moved.data[0].state.has_prev);
    assert(ctx.vec_items_moved.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 0);
    assert(ctx.vec_has_prev_changed.size == 0);
    assert(ctx.vec_has_next_changed.size == 0);

    playlist->current = 3;
    playlist->has_prev = true;
    playlist->has_next = true;

    callback_ctx_reset(&ctx);

    /* the current index belongs to the moved slice */
    vlc_playlist_Move(playlist, 1, 3, 5);

    assert(ctx.vec_items_moved.size == 1);
    assert(ctx.vec_items_moved.data[0].index == 1);
    assert(ctx.vec_items_moved.data[0].count == 3);
    assert(ctx.vec_items_moved.data[0].target == 5);
    assert(ctx.vec_items_moved.data[0].state.playlist_size == 10);
    assert(ctx.vec_items_moved.data[0].state.current == 7);
    assert(ctx.vec_items_moved.data[0].state.has_prev);
    assert(ctx.vec_items_moved.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 7);

    assert(ctx.vec_has_prev_changed.size == 0);
    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_reset(&ctx);

    /* as a result of this move, the current item (7) will be at index 0 */
    vlc_playlist_Move(playlist, 0, 7, 1);

    assert(ctx.vec_items_moved.size == 1);
    assert(ctx.vec_items_moved.data[0].index == 0);
    assert(ctx.vec_items_moved.data[0].count == 7);
    assert(ctx.vec_items_moved.data[0].target == 1);
    assert(ctx.vec_items_moved.data[0].state.playlist_size == 10);
    assert(ctx.vec_items_moved.data[0].state.current == 0);
    assert(!ctx.vec_items_moved.data[0].state.has_prev);
    assert(ctx.vec_items_moved.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 0);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(!ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_reset(&ctx);

    playlist->current = 5;
    playlist->has_prev = true;
    playlist->has_next = true;

    vlc_playlist_Move(playlist, 6, 2, 3);

    assert(ctx.vec_items_moved.size == 1);
    assert(ctx.vec_items_moved.data[0].index == 6);
    assert(ctx.vec_items_moved.data[0].count == 2);
    assert(ctx.vec_items_moved.data[0].target == 3);
    assert(ctx.vec_items_moved.data[0].state.playlist_size == 10);
    assert(ctx.vec_items_moved.data[0].state.current == 7);
    assert(ctx.vec_items_moved.data[0].state.has_prev);
    assert(ctx.vec_items_moved.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 7);

    assert(ctx.vec_has_prev_changed.size == 0);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_items_removed_callbacks(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_removed = callback_on_items_removed,
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_RemoveOne(playlist, 4);

    assert(ctx.vec_items_removed.size == 1);
    assert(ctx.vec_items_removed.data[0].index == 4);
    assert(ctx.vec_items_removed.data[0].count == 1);
    assert(ctx.vec_items_removed.data[0].state.playlist_size == 9);
    assert(ctx.vec_items_removed.data[0].state.current == -1);
    assert(!ctx.vec_items_removed.data[0].state.has_prev);
    assert(ctx.vec_items_removed.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 0);
    assert(ctx.vec_has_prev_changed.size == 0);
    assert(ctx.vec_has_next_changed.size == 0);

    playlist->current = 7;
    playlist->has_prev = true;
    playlist->has_next = true;

    callback_ctx_reset(&ctx);

    /* remove items before the current */
    vlc_playlist_Remove(playlist, 2, 4);

    assert(ctx.vec_items_removed.size == 1);
    assert(ctx.vec_items_removed.data[0].index == 2);
    assert(ctx.vec_items_removed.data[0].count == 4);
    assert(ctx.vec_items_removed.data[0].state.playlist_size == 5);
    assert(ctx.vec_items_removed.data[0].state.current == 3); /* shifted */
    assert(ctx.vec_items_removed.data[0].state.has_prev);
    assert(ctx.vec_items_removed.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 3);

    assert(ctx.vec_has_prev_changed.size == 0);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_reset(&ctx);

    /* remove the remaining items (without Clear) */
    vlc_playlist_Remove(playlist, 0, 5);

    assert(ctx.vec_items_removed.size == 1);
    assert(ctx.vec_items_removed.data[0].index == 0);
    assert(ctx.vec_items_removed.data[0].count == 5);
    assert(ctx.vec_items_removed.data[0].state.playlist_size == 0);
    assert(ctx.vec_items_removed.data[0].state.current == -1);
    assert(!ctx.vec_items_removed.data[0].state.has_prev);
    assert(!ctx.vec_items_removed.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == -1);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(!ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 1);
    assert(!ctx.vec_has_next_changed.data[0].has_next);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_items_reset_callbacks(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_reset = callback_on_items_reset,
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    callback_ctx_reset(&ctx);

    playlist->current = 9; /* last item */
    playlist->has_prev = true;
    playlist->has_next = false;

    vlc_playlist_Clear(playlist);

    assert(ctx.vec_items_reset.size == 1);
    assert(ctx.vec_items_reset.data[0].count == 0);
    assert(ctx.vec_items_reset.data[0].state.playlist_size == 0);
    assert(ctx.vec_items_reset.data[0].state.current == -1);
    assert(!ctx.vec_items_reset.data[0].state.has_prev);
    assert(!ctx.vec_items_reset.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == -1);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(!ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_playback_repeat_changed_callbacks(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    playlist->repeat = VLC_PLAYLIST_PLAYBACK_REPEAT_NONE;

    struct vlc_playlist_callbacks cbs = {
        .on_playback_repeat_changed = callback_on_playback_repeat_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_SetPlaybackRepeat(playlist, VLC_PLAYLIST_PLAYBACK_REPEAT_ALL);

    assert(vlc_playlist_GetPlaybackRepeat(playlist) ==
                                            VLC_PLAYLIST_PLAYBACK_REPEAT_ALL);

    assert(ctx.vec_playback_repeat_changed.size == 1);
    assert(ctx.vec_playback_repeat_changed.data[0].repeat ==
                                            VLC_PLAYLIST_PLAYBACK_REPEAT_ALL);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    vlc_playlist_Delete(playlist);
}

static void
test_playback_order_changed_callbacks(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    playlist->order = VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL;

    struct vlc_playlist_callbacks cbs = {
        .on_playback_order_changed = callback_on_playback_order_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_SetPlaybackOrder(playlist, VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM);

    assert(vlc_playlist_GetPlaybackOrder(playlist) ==
                                            VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM);

    assert(ctx.vec_playback_order_changed.size == 1);
    assert(ctx.vec_playback_order_changed.data[0].order ==
                                            VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    vlc_playlist_Delete(playlist);
}

static void
test_callbacks_on_add_listener(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    vlc_playlist_SetPlaybackRepeat(playlist, VLC_PLAYLIST_PLAYBACK_REPEAT_ALL);
    vlc_playlist_SetPlaybackOrder(playlist, VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL);

    ret = vlc_playlist_GoTo(playlist, 5);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_reset = callback_on_items_reset,
        .on_playback_repeat_changed = callback_on_playback_repeat_changed,
        .on_playback_order_changed = callback_on_playback_order_changed,
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, true);
    assert(listener);

    assert(ctx.vec_items_reset.size == 1);
    assert(ctx.vec_items_reset.data[0].count == 10);

    assert(ctx.vec_playback_repeat_changed.size == 1);
    assert(ctx.vec_playback_repeat_changed.data[0].repeat ==
                                            VLC_PLAYLIST_PLAYBACK_REPEAT_ALL);

    assert(ctx.vec_playback_order_changed.size == 1);
    assert(ctx.vec_playback_order_changed.data[0].order ==
                                            VLC_PLAYLIST_PLAYBACK_ORDER_NORMAL);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 5);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 1);
    assert(ctx.vec_has_next_changed.data[0].has_next);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_index_of(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 9 items (1 is not added) */
    int ret = vlc_playlist_Append(playlist, media, 9);
    assert(ret == VLC_SUCCESS);

    assert(vlc_playlist_IndexOfMedia(playlist, media[4]) == 4);
    /* only items 0 to 8 were added */
    assert(vlc_playlist_IndexOfMedia(playlist, media[9]) == -1);

    vlc_playlist_item_t *item = vlc_playlist_Get(playlist, 4);
    assert(vlc_playlist_IndexOf(playlist, item) == 4);

    vlc_playlist_item_Hold(item);
    vlc_playlist_RemoveOne(playlist, 4);
    assert(vlc_playlist_IndexOf(playlist, item) == -1);
    vlc_playlist_item_Release(item);

    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_prev(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[4];
    CreateDummyMediaArray(media, 4);

    /* initial playlist with 3 items */
    int ret = vlc_playlist_Append(playlist, media, 3);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    playlist->current = 2; /* last item */
    playlist->has_prev = true;
    playlist->has_next = false;

    /* go to the previous item (at index 1) */
    assert(vlc_playlist_HasPrev(playlist));
    ret = vlc_playlist_Prev(playlist);
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 1);
    assert(playlist->has_prev);
    assert(playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 1);

    assert(ctx.vec_has_prev_changed.size == 0);

    assert(ctx.vec_has_next_changed.size == 1);
    assert(ctx.vec_has_next_changed.data[0].has_next);

    callback_ctx_reset(&ctx);

    /* go to the previous item (at index 0) */
    assert(vlc_playlist_HasPrev(playlist));
    ret = vlc_playlist_Prev(playlist);
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 0);
    assert(!playlist->has_prev);
    assert(playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 0);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(!ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    /* no more previous item */
    assert(!vlc_playlist_HasPrev(playlist));

    /* returns an error, but does not crash */
    assert(vlc_playlist_Prev(playlist) == VLC_EGENERIC);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 4);
    vlc_playlist_Delete(playlist);
}

static void
test_next(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[3];
    CreateDummyMediaArray(media, 3);

    /* initial playlist with 3 items */
    int ret = vlc_playlist_Append(playlist, media, 3);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    playlist->current = 0; /* first item */
    playlist->has_prev = false;
    playlist->has_next = true;

    /* go to the next item (at index 1) */
    assert(vlc_playlist_HasNext(playlist));
    ret = vlc_playlist_Next(playlist);
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 1);
    assert(playlist->has_prev);
    assert(playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 1);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_reset(&ctx);

    /* go to the next item (at index 2) */
    assert(vlc_playlist_HasNext(playlist));
    ret = vlc_playlist_Next(playlist);
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 2);
    assert(playlist->has_prev);
    assert(!playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 2);

    assert(ctx.vec_has_prev_changed.size == 0);

    assert(ctx.vec_has_next_changed.size == 1);
    assert(!ctx.vec_has_next_changed.data[0].has_next);

    /* no more next item */
    assert(!vlc_playlist_HasNext(playlist));

    /* returns an error, but does not crash */
    assert(vlc_playlist_Next(playlist) == VLC_EGENERIC);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 3);
    vlc_playlist_Delete(playlist);
}

static void
test_goto(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    /* go to an item in the middle */
    ret = vlc_playlist_GoTo(playlist, 4);
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 4);
    assert(playlist->has_prev);
    assert(playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 4);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_reset(&ctx);

    /* go to the same item */
    ret = vlc_playlist_GoTo(playlist, 4);
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 4);
    assert(playlist->has_prev);
    assert(playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 0);
    assert(ctx.vec_has_prev_changed.size == 0);
    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_reset(&ctx);

    /* go to the first item */
    ret = vlc_playlist_GoTo(playlist, 0);
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 0);
    assert(!playlist->has_prev);
    assert(playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 0);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(!ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_reset(&ctx);

    /* go to the last item */
    ret = vlc_playlist_GoTo(playlist, 9);
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 9);
    assert(playlist->has_prev);
    assert(!playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 9);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 1);
    assert(!ctx.vec_has_next_changed.data[0].has_next);

    callback_ctx_reset(&ctx);

    /* deselect current */
    ret = vlc_playlist_GoTo(playlist, -1);
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == -1);
    assert(!playlist->has_prev);
    assert(playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == -1);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(!ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 1);
    assert(ctx.vec_has_next_changed.data[0].has_next);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_request_insert(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[5];
    CreateDummyMediaArray(media, 5);

    /* initial playlist with 3 items */
    int ret = vlc_playlist_Append(playlist, media, 3);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_added = callback_on_items_added,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    /* insert 5 items at index 10 (out-of-bounds) */
    ret = vlc_playlist_RequestInsert(playlist, 10, &media[3], 2);
    assert(ret == VLC_SUCCESS);

    assert(vlc_playlist_Count(playlist) == 5);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 2);
    EXPECT_AT(3, 3);
    EXPECT_AT(4, 4);

    assert(ctx.vec_items_added.size == 1);
    assert(ctx.vec_items_added.data[0].index == 3); /* index was changed */
    assert(ctx.vec_items_added.data[0].count == 2);
    assert(ctx.vec_items_added.data[0].state.playlist_size == 5);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 5);
    vlc_playlist_Delete(playlist);
}

static void
test_request_remove_with_matching_hint(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_removed = callback_on_items_removed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_item_t *items_to_remove[] = {
        vlc_playlist_Get(playlist, 3),
        vlc_playlist_Get(playlist, 4),
        vlc_playlist_Get(playlist, 5),
        vlc_playlist_Get(playlist, 6),
    };

    ret = vlc_playlist_RequestRemove(playlist, items_to_remove, 4, 3);
    assert(ret == VLC_SUCCESS);

    assert(vlc_playlist_Count(playlist) == 6);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 2);
    EXPECT_AT(3, 7);
    EXPECT_AT(4, 8);
    EXPECT_AT(5, 9);

    assert(ctx.vec_items_removed.size == 1);
    assert(ctx.vec_items_removed.data[0].index == 3);
    assert(ctx.vec_items_removed.data[0].count == 4);
    assert(ctx.vec_items_removed.data[0].state.playlist_size == 6);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_request_remove_without_hint(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_removed = callback_on_items_removed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_item_t *items_to_remove[] = {
        vlc_playlist_Get(playlist, 3),
        vlc_playlist_Get(playlist, 4),
        vlc_playlist_Get(playlist, 5),
        vlc_playlist_Get(playlist, 6),
    };

    ret = vlc_playlist_RequestRemove(playlist, items_to_remove, 4, -1);
    assert(ret == VLC_SUCCESS);

    assert(vlc_playlist_Count(playlist) == 6);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 2);
    EXPECT_AT(3, 7);
    EXPECT_AT(4, 8);
    EXPECT_AT(5, 9);

    assert(ctx.vec_items_removed.size == 1);
    assert(ctx.vec_items_removed.data[0].index == 3);
    assert(ctx.vec_items_removed.data[0].count == 4);
    assert(ctx.vec_items_removed.data[0].state.playlist_size == 6);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_request_remove_adapt(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[11];
    CreateDummyMediaArray(media, 11);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_removed = callback_on_items_removed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_item_t *dummy = vlc_playlist_item_New(media[10], 0);
    assert(dummy);

    /* remove items in a wrong order at wrong position, as if the playlist had
     * been sorted/shuffled before the request were applied */
    vlc_playlist_item_t *items_to_remove[] = {
        vlc_playlist_Get(playlist, 3),
        vlc_playlist_Get(playlist, 2),
        vlc_playlist_Get(playlist, 6),
        vlc_playlist_Get(playlist, 9),
        vlc_playlist_Get(playlist, 1),
        dummy, /* inexistant */
        vlc_playlist_Get(playlist, 8),
    };

    ret = vlc_playlist_RequestRemove(playlist, items_to_remove, 7, 3);
    assert(ret == VLC_SUCCESS);

    vlc_playlist_item_Release(dummy);

    assert(vlc_playlist_Count(playlist) == 4);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 4);
    EXPECT_AT(2, 5);
    EXPECT_AT(3, 7);

    /* it should notify 3 different slices removed, in descending order for
     * optimization: {8,9}, {6}, {1,2,3}. */

    assert(ctx.vec_items_removed.size == 3);

    assert(ctx.vec_items_removed.data[0].index == 8);
    assert(ctx.vec_items_removed.data[0].count == 2);
    assert(ctx.vec_items_removed.data[0].state.playlist_size == 8);

    assert(ctx.vec_items_removed.data[1].index == 6);
    assert(ctx.vec_items_removed.data[1].count == 1);
    assert(ctx.vec_items_removed.data[1].state.playlist_size == 7);

    assert(ctx.vec_items_removed.data[2].index == 1);
    assert(ctx.vec_items_removed.data[2].count == 3);
    assert(ctx.vec_items_removed.data[2].state.playlist_size == 4);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 11);
    vlc_playlist_Delete(playlist);
}

static void
test_request_move_with_matching_hint(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_moved = callback_on_items_moved,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_item_t *items_to_move[] = {
        vlc_playlist_Get(playlist, 5),
        vlc_playlist_Get(playlist, 6),
        vlc_playlist_Get(playlist, 7),
        vlc_playlist_Get(playlist, 8),
    };

    ret = vlc_playlist_RequestMove(playlist, items_to_move, 4, 2, 5);
    assert(ret == VLC_SUCCESS);

    assert(vlc_playlist_Count(playlist) == 10);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 5);
    EXPECT_AT(3, 6);
    EXPECT_AT(4, 7);
    EXPECT_AT(5, 8);
    EXPECT_AT(6, 2);
    EXPECT_AT(7, 3);
    EXPECT_AT(8, 4);
    EXPECT_AT(9, 9);

    assert(ctx.vec_items_moved.size == 1);
    assert(ctx.vec_items_moved.data[0].index == 5);
    assert(ctx.vec_items_moved.data[0].count == 4);
    assert(ctx.vec_items_moved.data[0].state.playlist_size == 10);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_request_move_without_hint(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_moved = callback_on_items_moved,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_item_t *items_to_move[] = {
        vlc_playlist_Get(playlist, 5),
        vlc_playlist_Get(playlist, 6),
        vlc_playlist_Get(playlist, 7),
        vlc_playlist_Get(playlist, 8),
    };

    ret = vlc_playlist_RequestMove(playlist, items_to_move, 4, 2, -1);
    assert(ret == VLC_SUCCESS);

    assert(vlc_playlist_Count(playlist) == 10);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 5);
    EXPECT_AT(3, 6);
    EXPECT_AT(4, 7);
    EXPECT_AT(5, 8);
    EXPECT_AT(6, 2);
    EXPECT_AT(7, 3);
    EXPECT_AT(8, 4);
    EXPECT_AT(9, 9);

    assert(ctx.vec_items_moved.size == 1);
    assert(ctx.vec_items_moved.data[0].index == 5);
    assert(ctx.vec_items_moved.data[0].count == 4);
    assert(ctx.vec_items_moved.data[0].state.playlist_size == 10);

    vlc_playlist_item_t *item = vlc_playlist_Get(playlist, 3);
    /* move it to index 42 (out of bounds) */
    vlc_playlist_RequestMove(playlist, &item, 1, 42, -1);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 5);
    EXPECT_AT(3, 7);
    EXPECT_AT(4, 8);
    EXPECT_AT(5, 2);
    EXPECT_AT(6, 3);
    EXPECT_AT(7, 4);
    EXPECT_AT(8, 9);
    EXPECT_AT(9, 6);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_request_move_adapt(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[16];
    CreateDummyMediaArray(media, 16);

    /* initial playlist with 15 items */
    int ret = vlc_playlist_Append(playlist, media, 15);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_moved = callback_on_items_moved,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_item_t *dummy = vlc_playlist_item_New(media[15], 0);
    assert(dummy);

    /* move items in a wrong order at wrong position, as if the playlist had
     * been sorted/shuffled before the request were applied */
    vlc_playlist_item_t *items_to_move[] = {
        vlc_playlist_Get(playlist, 7),
        vlc_playlist_Get(playlist, 8),
        vlc_playlist_Get(playlist, 5),
        vlc_playlist_Get(playlist, 12),
        dummy, /* inexistant */
        vlc_playlist_Get(playlist, 3),
        vlc_playlist_Get(playlist, 13),
        vlc_playlist_Get(playlist, 14),
        vlc_playlist_Get(playlist, 1),
    };

    vlc_playlist_RequestMove(playlist, items_to_move, 9, 3, 2);

    vlc_playlist_item_Release(dummy);

    assert(vlc_playlist_Count(playlist) == 15);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 2);
    EXPECT_AT(2, 4);

    EXPECT_AT(3, 7);
    EXPECT_AT(4, 8);
    EXPECT_AT(5, 5);
    EXPECT_AT(6, 12);
    EXPECT_AT(7, 3);
    EXPECT_AT(8, 13);
    EXPECT_AT(9, 14);
    EXPECT_AT(10, 1);

    EXPECT_AT(11, 6);
    EXPECT_AT(12, 9);
    EXPECT_AT(13, 10);
    EXPECT_AT(14, 11);

    /* there are 6 slices to move: 7-8, 5, 12, 3, 13-14, 1 */
    assert(ctx.vec_items_moved.size == 6);

    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;
    for (int i = 0; i < 15; ++i)
        vlc_vector_push(&vec, i * 10);

    struct items_moved_report report;
    vlc_vector_foreach(report, &ctx.vec_items_moved)
        /* apply the changes as reported by the callbacks */
        vlc_vector_move_slice(&vec, report.index, report.count, report.target);

    /* the vector items must have been moved the same way as the playlist */
    assert(vec.size == 15);
    assert(vec.data[0] == 0);
    assert(vec.data[1] == 20);
    assert(vec.data[2] == 40);
    assert(vec.data[3] == 70);
    assert(vec.data[4] == 80);
    assert(vec.data[5] == 50);
    assert(vec.data[6] == 120);
    assert(vec.data[7] == 30);
    assert(vec.data[8] == 130);
    assert(vec.data[9] == 140);
    assert(vec.data[10] == 10);
    assert(vec.data[11] == 60);
    assert(vec.data[12] == 90);
    assert(vec.data[13] == 100);
    assert(vec.data[14] == 110);

    vlc_vector_destroy(&vec);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 16);
    vlc_playlist_Delete(playlist);
}

static void
test_request_move_to_end_adapt(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[16];
    CreateDummyMediaArray(media, 16);

    /* initial playlist with 15 items */
    int ret = vlc_playlist_Append(playlist, media, 15);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_moved = callback_on_items_moved,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    vlc_playlist_item_t *dummy = vlc_playlist_item_New(media[15], 0);
    assert(dummy);

    /* move items in a wrong order at wrong position, as if the playlist had
     * been sorted/shuffled before the request were applied */
    vlc_playlist_item_t *items_to_move[] = {
        vlc_playlist_Get(playlist, 7),
        vlc_playlist_Get(playlist, 8),
        vlc_playlist_Get(playlist, 5),
        vlc_playlist_Get(playlist, 12),
        dummy, /* inexistant */
        vlc_playlist_Get(playlist, 3),
        vlc_playlist_Get(playlist, 13),
        vlc_playlist_Get(playlist, 14),
        vlc_playlist_Get(playlist, 1),
    };

    /* target 20 is far beyond the end of the list */
    vlc_playlist_RequestMove(playlist, items_to_move, 9, 20, 2);

    vlc_playlist_item_Release(dummy);

    assert(vlc_playlist_Count(playlist) == 15);

    EXPECT_AT(0, 0);
    EXPECT_AT(1, 2);
    EXPECT_AT(2, 4);
    EXPECT_AT(3, 6);
    EXPECT_AT(4, 9);
    EXPECT_AT(5, 10);
    EXPECT_AT(6, 11);

    EXPECT_AT(7, 7);
    EXPECT_AT(8, 8);
    EXPECT_AT(9, 5);
    EXPECT_AT(10, 12);
    EXPECT_AT(11, 3);
    EXPECT_AT(12, 13);
    EXPECT_AT(13, 14);
    EXPECT_AT(14, 1);

    /* there are 6 slices to move: 7-8, 5, 12, 3, 13-14, 1 */
    assert(ctx.vec_items_moved.size == 6);

    struct VLC_VECTOR(int) vec = VLC_VECTOR_INITIALIZER;
    for (int i = 0; i < 15; ++i)
        vlc_vector_push(&vec, i * 10);

    struct items_moved_report report;
    vlc_vector_foreach(report, &ctx.vec_items_moved)
        /* apply the changes as reported by the callbacks */
        vlc_vector_move_slice(&vec, report.index, report.count, report.target);

    /* the vector items must have been moved the same way as the playlist */
    assert(vec.size == 15);
    assert(vec.data[0] == 0);
    assert(vec.data[1] == 20);
    assert(vec.data[2] == 40);
    assert(vec.data[3] == 60);
    assert(vec.data[4] == 90);
    assert(vec.data[5] == 100);
    assert(vec.data[6] == 110);
    assert(vec.data[7] == 70);
    assert(vec.data[8] == 80);
    assert(vec.data[9] == 50);
    assert(vec.data[10] == 120);
    assert(vec.data[11] == 30);
    assert(vec.data[12] == 130);
    assert(vec.data[13] == 140);
    assert(vec.data[14] == 10);

    vlc_vector_destroy(&vec);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 16);
    vlc_playlist_Delete(playlist);
}

static void
test_request_goto_with_matching_hint(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    /* go to an item in the middle, with incorrect index_hint */
    vlc_playlist_item_t *item = vlc_playlist_Get(playlist, 4);
    ret = vlc_playlist_RequestGoTo(playlist, item, 4);
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 4);
    assert(playlist->has_prev);
    assert(playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 4);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_request_goto_without_hint(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    /* go to an item in the middle, with incorrect index_hint */
    vlc_playlist_item_t *item = vlc_playlist_Get(playlist, 4);
    ret = vlc_playlist_RequestGoTo(playlist, item, -1); /* no hint */
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 4);
    assert(playlist->has_prev);
    assert(playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 4);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_request_goto_adapt(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    /* go to an item in the middle, with incorrect index_hint */
    vlc_playlist_item_t *item = vlc_playlist_Get(playlist, 4);
    ret = vlc_playlist_RequestGoTo(playlist, item, 7); /* wrong index hint */
    assert(ret == VLC_SUCCESS);

    assert(playlist->current == 4);
    assert(playlist->has_prev);
    assert(playlist->has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 4);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

/* this only tests that the randomizer is correctly managed by the playlist,
 * for further tests on randomization properties, see randomizer tests. */
static void
test_random(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[6];
    CreateDummyMediaArray(media, 6);

    /* initial playlist with 5 items (1 is not added immediately) */
    int ret = vlc_playlist_Append(playlist, media, 5);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    assert(!vlc_playlist_HasPrev(playlist));
    assert(vlc_playlist_HasNext(playlist));

    for (int i = 0; i < 3; ++i)
    {
        assert(vlc_playlist_HasNext(playlist));
        ret = vlc_playlist_Next(playlist);
        assert(ret == VLC_SUCCESS);
    }

    assert(vlc_playlist_HasPrev(playlist));
    vlc_playlist_SetPlaybackOrder(playlist, VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM);

    /* in random order, previous uses the history of randomly selected items */
    assert(!vlc_playlist_HasPrev(playlist));

    bool selected[5] = {};
    for (int i = 0; i < 5; ++i)
    {
        assert(vlc_playlist_HasNext(playlist));
        ret = vlc_playlist_Next(playlist);
        assert(ret == VLC_SUCCESS);
        ssize_t index = vlc_playlist_GetCurrentIndex(playlist);
        assert(index != -1);
        assert(!selected[index]); /* not selected twice */
        selected[index] = true;
    }

    assert(!vlc_playlist_HasNext(playlist));

    /* add a new item, it must be taken into account */
    ret = vlc_playlist_AppendOne(playlist, media[5]);
    assert(ret == VLC_SUCCESS);
    assert(vlc_playlist_HasNext(playlist));

    ret = vlc_playlist_Next(playlist);
    assert(ret == VLC_SUCCESS);

    assert(vlc_playlist_GetCurrentIndex(playlist) == 5);
    assert(!vlc_playlist_HasNext(playlist));

    vlc_playlist_RemoveOne(playlist, 5);

    /* enable repeat */
    vlc_playlist_SetPlaybackRepeat(playlist, VLC_PLAYLIST_PLAYBACK_REPEAT_ALL);

    /* now there are more items */
    assert(vlc_playlist_HasNext(playlist));

    /* once again */
    memset(selected, 0, sizeof(selected));
    for (int i = 0; i < 5; ++i)
    {
        assert(vlc_playlist_HasNext(playlist));
        ret = vlc_playlist_Next(playlist);
        assert(ret == VLC_SUCCESS);
        ssize_t index = vlc_playlist_GetCurrentIndex(playlist);
        assert(index != -1);
        assert(!selected[index]); /* not selected twice */
        selected[index] = true;
    }

    /* there are always more items */
    assert(vlc_playlist_HasNext(playlist));

    /* move to the middle of the random array */
    for (int i = 0; i < 3; ++i)
    {
        assert(vlc_playlist_HasNext(playlist));
        ret = vlc_playlist_Next(playlist);
        assert(ret == VLC_SUCCESS);
    }

    memset(selected, 0, sizeof(selected));
    int actual[5]; /* store the selected items (by their index) */

    ssize_t current = vlc_playlist_GetCurrentIndex(playlist);
    assert(current != -1);
    actual[4] = current;

    for (int i = 3; i >= 0; --i)
    {
        assert(vlc_playlist_HasPrev(playlist));
        ret = vlc_playlist_Prev(playlist);
        assert(ret == VLC_SUCCESS);
        ssize_t index = vlc_playlist_GetCurrentIndex(playlist);
        assert(index != -1);
        actual[i] = index;
        assert(!selected[index]); /* not selected twice */
        selected[index] = true;
    }

    /* no more previous, the history may only contain each item once */
    assert(!vlc_playlist_HasPrev(playlist));

    /* we should get the same items in the reverse order going forward */
    for (int i = 1; i < 5; ++i)
    {
        assert(vlc_playlist_HasNext(playlist));
        ret = vlc_playlist_Next(playlist);
        assert(ret == VLC_SUCCESS);
        ssize_t index = vlc_playlist_GetCurrentIndex(playlist);
        assert(index != -1);
        assert(index == actual[i]);
    }

    /* there are always more items */
    assert(vlc_playlist_HasNext(playlist));

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 6);
    vlc_playlist_Delete(playlist);
}

static void
test_shuffle(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    CreateDummyMediaArray(media, 10);

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_reset = callback_on_items_reset,
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    playlist->current = 4;
    playlist->has_prev = true;
    playlist->has_next = true;

    vlc_playlist_Shuffle(playlist);

    ssize_t index = vlc_playlist_IndexOfMedia(playlist, media[4]);
    assert(index != -1);
    assert(index == playlist->current);

    assert(ctx.vec_items_reset.size == 1);
    assert(ctx.vec_items_reset.data[0].count == 10);
    assert(ctx.vec_items_reset.data[0].state.playlist_size == 10);
    assert(ctx.vec_items_reset.data[0].state.current == index);
    assert(ctx.vec_items_reset.data[0].state.has_prev == (index > 0));
    assert(ctx.vec_items_reset.data[0].state.has_next == (index < 9));

    if (index == 4)
        assert(ctx.vec_current_index_changed.size == 0);
    else
    {
        assert(ctx.vec_current_index_changed.size == 1);
        assert(ctx.vec_current_index_changed.data[0].current == index);
    }

    if (index == 0)
    {
        assert(!playlist->has_prev);
        assert(ctx.vec_has_prev_changed.size == 1);
        assert(!ctx.vec_has_prev_changed.data[0].has_prev);
    }
    else
    {
        assert(playlist->has_prev);
        assert(ctx.vec_has_prev_changed.size == 0);
    }

    if (index == 9)
    {
        assert(!playlist->has_next);
        assert(ctx.vec_has_next_changed.size == 1);
        assert(!ctx.vec_has_next_changed.data[0].has_next);
    }
    else
    {
        assert(playlist->has_next);
        assert(ctx.vec_has_next_changed.size == 0);
    }

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

static void
test_sort(void)
{
    vlc_playlist_t *playlist = vlc_playlist_New(NULL);
    assert(playlist);

    input_item_t *media[10];
    media[0] = CreateDummyMedia(4); media[0]->i_duration = 42;
    media[1] = CreateDummyMedia(1); media[1]->i_duration = 5;
    media[2] = CreateDummyMedia(6); media[2]->i_duration = 100;
    media[3] = CreateDummyMedia(2); media[3]->i_duration = 1;
    media[4] = CreateDummyMedia(1); media[4]->i_duration = 8;
    media[5] = CreateDummyMedia(4); media[5]->i_duration = 23;
    media[6] = CreateDummyMedia(3); media[6]->i_duration = 60;
    media[7] = CreateDummyMedia(3); media[7]->i_duration = 40;
    media[8] = CreateDummyMedia(0); media[8]->i_duration = 42;
    media[9] = CreateDummyMedia(5); media[9]->i_duration = 42;

    /* initial playlist with 10 items */
    int ret = vlc_playlist_Append(playlist, media, 10);
    assert(ret == VLC_SUCCESS);

    struct vlc_playlist_callbacks cbs = {
        .on_items_reset = callback_on_items_reset,
        .on_current_index_changed = callback_on_current_index_changed,
        .on_has_prev_changed = callback_on_has_prev_changed,
        .on_has_next_changed = callback_on_has_next_changed,
    };

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_playlist_listener_id *listener =
            vlc_playlist_AddListener(playlist, &cbs, &ctx, false);
    assert(listener);

    playlist->current = 0;
    playlist->has_prev = false;
    playlist->has_next = true;

    struct vlc_playlist_sort_criterion criteria1[] = {
        { VLC_PLAYLIST_SORT_KEY_TITLE, VLC_PLAYLIST_SORT_ORDER_ASCENDING },
        { VLC_PLAYLIST_SORT_KEY_DURATION, VLC_PLAYLIST_SORT_ORDER_ASCENDING },
    };
    vlc_playlist_Sort(playlist, criteria1, 2);

    EXPECT_AT(0, 8);
    EXPECT_AT(1, 1);
    EXPECT_AT(2, 4);
    EXPECT_AT(3, 3);
    EXPECT_AT(4, 7);
    EXPECT_AT(5, 6);
    EXPECT_AT(6, 5);
    EXPECT_AT(7, 0);
    EXPECT_AT(8, 9);
    EXPECT_AT(9, 2);

    ssize_t index = vlc_playlist_IndexOfMedia(playlist, media[0]);
    assert(index == 7);
    assert(playlist->current == 7);

    assert(ctx.vec_items_reset.size == 1);
    assert(ctx.vec_items_reset.data[0].count == 10);
    assert(ctx.vec_items_reset.data[0].state.playlist_size == 10);
    assert(ctx.vec_items_reset.data[0].state.current == 7);
    assert(ctx.vec_items_reset.data[0].state.has_prev);
    assert(ctx.vec_items_reset.data[0].state.has_next);

    assert(ctx.vec_current_index_changed.size == 1);
    assert(ctx.vec_current_index_changed.data[0].current == 7);

    assert(ctx.vec_has_prev_changed.size == 1);
    assert(ctx.vec_has_prev_changed.data[0].has_prev);

    assert(ctx.vec_has_next_changed.size == 0);

    callback_ctx_reset(&ctx);

    struct vlc_playlist_sort_criterion criteria2[] = {
        { VLC_PLAYLIST_SORT_KEY_DURATION, VLC_PLAYLIST_SORT_ORDER_DESCENDING },
        { VLC_PLAYLIST_SORT_KEY_TITLE, VLC_PLAYLIST_SORT_ORDER_ASCENDING },
    };

    vlc_playlist_Sort(playlist, criteria2, 2);

    EXPECT_AT(0, 2);
    EXPECT_AT(1, 6);
    EXPECT_AT(2, 8);
    EXPECT_AT(3, 0);
    EXPECT_AT(4, 9);
    EXPECT_AT(5, 7);
    EXPECT_AT(6, 5);
    EXPECT_AT(7, 4);
    EXPECT_AT(8, 1);
    EXPECT_AT(9, 3);

    assert(ctx.vec_items_reset.size == 1);
    assert(ctx.vec_items_reset.data[0].count == 10);
    assert(ctx.vec_items_reset.data[0].state.playlist_size == 10);

    callback_ctx_destroy(&ctx);
    vlc_playlist_RemoveListener(playlist, listener);
    DestroyMediaArray(media, 10);
    vlc_playlist_Delete(playlist);
}

#undef EXPECT_AT

int main(void)
{
    test_append();
    test_insert();
    test_move();
    test_remove();
    test_clear();
    test_expand_item();
    test_items_added_callbacks();
    test_items_moved_callbacks();
    test_items_removed_callbacks();
    test_items_reset_callbacks();
    test_playback_repeat_changed_callbacks();
    test_playback_order_changed_callbacks();
    test_callbacks_on_add_listener();
    test_index_of();
    test_prev();
    test_next();
    test_goto();
    test_request_insert();
    test_request_remove_with_matching_hint();
    test_request_remove_without_hint();
    test_request_remove_adapt();
    test_request_move_with_matching_hint();
    test_request_move_without_hint();
    test_request_move_adapt();
    test_request_move_to_end_adapt();
    test_request_goto_with_matching_hint();
    test_request_goto_without_hint();
    test_request_goto_adapt();
    test_random();
    test_shuffle();
    test_sort();
    return 0;
}

#endif
