/*****************************************************************************
 * media_source/test.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>
#include <vlc_vector.h>
#include "media_source.h"
#include "media_tree.h"

static void
test_media_tree(void)
{
    vlc_media_tree_t *tree = vlc_media_tree_New();
    vlc_media_tree_Lock(tree);

    assert(!tree->root.p_item);
    assert(tree->root.i_children == 0);

    input_item_t *media = input_item_New("vlc://item", "aaa");
    assert(media);
    input_item_node_t *node = vlc_media_tree_Add(tree, &tree->root, media);
    assert(node);
    input_item_Release(media); /* there is still 1 ref after that */

    assert(tree->root.i_children == 1);
    assert(tree->root.pp_children[0] == node);
    assert(node->p_item == media);
    assert(node->i_children == 0);

    input_item_t *media2 = input_item_New("vlc://child", "bbb");
    assert(media2);
    input_item_node_t *node2 = vlc_media_tree_Add(tree, node, media2);
    assert(node2);
    input_item_Release(media2);

    assert(node->i_children == 1);
    assert(node->pp_children[0] == node2);
    assert(node2->p_item == media2);
    assert(node2->i_children == 0);

    input_item_t *media3 = input_item_New("vlc://child2", "ccc");
    assert(media3);
    input_item_node_t *node3 = vlc_media_tree_Add(tree, node, media3);
    assert(node3);
    input_item_Release(media3);

    assert(node->i_children == 2);
    assert(node->pp_children[0] == node2);
    assert(node->pp_children[1] == node3);
    assert(node3->p_item == media3);
    assert(node3->i_children == 0);

    bool removed = vlc_media_tree_Remove(tree, media2);
    assert(removed);
    assert(node->i_children == 1);
    assert(node->pp_children[0] == node3);

    vlc_media_tree_Unlock(tree);
    vlc_media_tree_Release(tree);
}

struct children_reset_report
{
    input_item_node_t *node;
};

struct children_added_report
{
    input_item_node_t *node;
    input_item_t *first_media;
    size_t count;
};

struct children_removed_report
{
    input_item_node_t *node;
    input_item_t *first_media;
    size_t count;
};

struct callback_ctx
{
    struct VLC_VECTOR(struct children_reset_report) vec_children_reset;
    struct VLC_VECTOR(struct children_added_report) vec_children_added;
    struct VLC_VECTOR(struct children_removed_report) vec_children_removed;
};

#define CALLBACK_CTX_INITIALIZER \
{ \
    VLC_VECTOR_INITIALIZER, \
    VLC_VECTOR_INITIALIZER, \
    VLC_VECTOR_INITIALIZER, \
}

static inline void
callback_ctx_destroy_reports(struct callback_ctx *ctx)
{
    for (size_t i = 0; i < ctx->vec_children_added.size; ++i)
        input_item_Release(ctx->vec_children_added.data[i].first_media);
    for (size_t i = 0; i < ctx->vec_children_removed.size; ++i)
        input_item_Release(ctx->vec_children_removed.data[i].first_media);
}

static inline void
callback_ctx_reset(struct callback_ctx *ctx)
{
    callback_ctx_destroy_reports(ctx);
    vlc_vector_clear(&ctx->vec_children_reset);
    vlc_vector_clear(&ctx->vec_children_added);
    vlc_vector_clear(&ctx->vec_children_removed);
}

static inline void
callback_ctx_destroy(struct callback_ctx *ctx)
{
    callback_ctx_destroy_reports(ctx);
    vlc_vector_destroy(&ctx->vec_children_reset);
    vlc_vector_destroy(&ctx->vec_children_added);
    vlc_vector_destroy(&ctx->vec_children_removed);
}

static void
on_children_reset(vlc_media_tree_t *tree, input_item_node_t *node,
                  void *userdata)
{
    VLC_UNUSED(tree);

    struct callback_ctx *ctx = userdata;

    struct children_reset_report report;
    report.node = node;
    bool ok = vlc_vector_push(&ctx->vec_children_reset, report);
    assert(ok);
}

static void
on_children_added(vlc_media_tree_t *tree, input_item_node_t *node,
                  input_item_node_t *const children[], size_t count,
                  void *userdata)
{
    VLC_UNUSED(tree);

    struct callback_ctx *ctx = userdata;

    struct children_added_report report;
    report.node = node;
    report.first_media = input_item_Hold(children[0]->p_item);
    report.count = count;
    bool ok = vlc_vector_push(&ctx->vec_children_added, report);
    assert(ok);
}

static void
on_children_removed(vlc_media_tree_t *tree, input_item_node_t *node,
                    input_item_node_t *const children[], size_t count,
                    void *userdata)
{
    VLC_UNUSED(tree);

    struct callback_ctx *ctx = userdata;

    struct children_removed_report report;
    report.node = node;
    report.first_media = input_item_Hold(children[0]->p_item);
    report.count = count;
    bool ok = vlc_vector_push(&ctx->vec_children_removed, report);
    assert(ok);
}

static void test_media_tree_callbacks(void)
{
    struct vlc_media_tree_callbacks cbs = {
        .on_children_reset = on_children_reset,
        .on_children_added = on_children_added,
        .on_children_removed = on_children_removed,
    };

    vlc_media_tree_t *tree = vlc_media_tree_New();
    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_media_tree_listener_id *listener =
            vlc_media_tree_AddListener(tree, &cbs, &ctx, false);
    assert(listener);

    vlc_media_tree_Lock(tree);

    input_item_t *media = input_item_New("vlc://item", "aaa");
    assert(media);
    input_item_node_t *node = vlc_media_tree_Add(tree, &tree->root, media);
    assert(node);
    input_item_Release(media); /* there is still 1 ref after that */

    assert(ctx.vec_children_reset.size == 0);
    assert(ctx.vec_children_added.size == 1);
    assert(ctx.vec_children_added.data[0].node == &tree->root);
    assert(ctx.vec_children_added.data[0].first_media == media);
    assert(ctx.vec_children_removed.size == 0);

    callback_ctx_reset(&ctx);

    input_item_t *media2 = input_item_New("vlc://child", "bbb");
    assert(media2);
    input_item_node_t *node2 = vlc_media_tree_Add(tree, node, media2);
    assert(node2);
    input_item_Release(media2);

    assert(ctx.vec_children_reset.size == 0);
    assert(ctx.vec_children_added.size == 1);
    assert(ctx.vec_children_added.data[0].node == node);
    assert(ctx.vec_children_added.data[0].first_media == media2);
    assert(ctx.vec_children_removed.size == 0);

    callback_ctx_reset(&ctx);

    input_item_t *media3 = input_item_New("vlc://child2", "ccc");
    assert(media3);
    input_item_node_t *node3 = vlc_media_tree_Add(tree, node, media3);
    assert(node3);
    input_item_Release(media3);

    assert(ctx.vec_children_reset.size == 0);
    assert(ctx.vec_children_added.size == 1);
    assert(ctx.vec_children_added.data[0].node == node);
    assert(ctx.vec_children_added.data[0].first_media == media3);
    assert(ctx.vec_children_removed.size == 0);

    callback_ctx_reset(&ctx);

    bool removed = vlc_media_tree_Remove(tree, media2);
    assert(removed);
    assert(node->i_children == 1);
    assert(node->pp_children[0] == node3);

    assert(ctx.vec_children_reset.size == 0);
    assert(ctx.vec_children_added.size == 0);
    assert(ctx.vec_children_removed.size == 1);
    assert(ctx.vec_children_removed.data[0].node == node);
    assert(ctx.vec_children_removed.data[0].first_media == media2);

    vlc_media_tree_Unlock(tree);

    vlc_media_tree_RemoveListener(tree, listener);
    callback_ctx_destroy(&ctx);

    vlc_media_tree_Release(tree);
}

static void
test_media_tree_callbacks_on_add_listener(void)
{
    struct vlc_media_tree_callbacks cbs = {
        .on_children_reset = on_children_reset,
    };


    vlc_media_tree_t *tree = vlc_media_tree_New();

    vlc_media_tree_Lock(tree);

    input_item_t *media = input_item_New("vlc://item", "aaa");
    assert(media);
    input_item_node_t *node = vlc_media_tree_Add(tree, &tree->root, media);
    assert(node);
    input_item_Release(media);

    input_item_t *media2 = input_item_New("vlc://child", "bbb");
    assert(media2);
    input_item_node_t *node2 = vlc_media_tree_Add(tree, node, media2);
    assert(node2);
    input_item_Release(media2);

    vlc_media_tree_Unlock(tree);

    struct callback_ctx ctx = CALLBACK_CTX_INITIALIZER;
    vlc_media_tree_listener_id *listener =
            vlc_media_tree_AddListener(tree, &cbs, &ctx, true);
    assert(listener);

    assert(ctx.vec_children_reset.size == 1);
    assert(ctx.vec_children_reset.data[0].node == &tree->root);
    assert(ctx.vec_children_reset.data[0].node->i_children == 1);
    assert(ctx.vec_children_reset.data[0].node->pp_children[0] == node);

    vlc_media_tree_RemoveListener(tree, listener);
    callback_ctx_destroy(&ctx);

    vlc_media_tree_Release(tree);
}

int main(void)
{
    test_media_tree();
    test_media_tree_callbacks();
    test_media_tree_callbacks_on_add_listener();
    return 0;
}
