/*****************************************************************************
 * media_tree.c
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

#include "media_tree.h"

#include <assert.h>
#include <vlc_common.h>
#include <vlc_arrays.h>
#include <vlc_atomic.h>
#include <vlc_input_item.h>
#include <vlc_threads.h>
#include "libvlc.h"

struct vlc_media_tree_listener_id
{
    const struct vlc_media_tree_callbacks *cbs;
    void *userdata;
    struct vlc_list node; /**< node of media_tree_private_t.listeners */
};

typedef struct
{
    vlc_media_tree_t public_data;

    struct vlc_list listeners; /**< list of vlc_media_tree_listener_id.node */
    vlc_mutex_t lock;
    vlc_atomic_rc_t rc;
} media_tree_private_t;

#define mt_priv(mt) container_of(mt, media_tree_private_t, public_data)

vlc_media_tree_t *
vlc_media_tree_New(void)
{
    media_tree_private_t *priv = malloc(sizeof(*priv));
    if (unlikely(!priv))
        return NULL;

    vlc_mutex_init(&priv->lock);
    vlc_atomic_rc_init(&priv->rc);
    vlc_list_init(&priv->listeners);

    vlc_media_tree_t *tree = &priv->public_data;
    input_item_node_t *root = &tree->root;
    root->p_item = NULL;
    TAB_INIT(root->i_children, root->pp_children);

    return tree;
}

static inline void
vlc_media_tree_AssertLocked(vlc_media_tree_t *tree)
{
    media_tree_private_t *priv = mt_priv(tree);
    vlc_mutex_assert(&priv->lock);
}

#define vlc_media_tree_listener_foreach(listener, tree) \
    vlc_list_foreach(listener, &mt_priv(tree)->listeners, node)

#define vlc_media_tree_NotifyListener(tree, listener, event, ...) \
do { \
    if (listener->cbs->event) \
        listener->cbs->event(tree, ##__VA_ARGS__, listener->userdata); \
} while(0)

#define vlc_media_tree_Notify(tree, event, ...) \
do { \
    vlc_media_tree_AssertLocked(tree); \
    vlc_media_tree_listener_id *listener; \
    vlc_media_tree_listener_foreach(listener, tree) \
        vlc_media_tree_NotifyListener(tree, listener, event, ##__VA_ARGS__); \
} while (0)

static bool
vlc_media_tree_FindNodeByMedia(input_item_node_t *parent,
                               const input_item_t *media,
                               input_item_node_t **result,
                               input_item_node_t **result_parent)
{
    for (int i = 0; i < parent->i_children; ++i)
    {
        input_item_node_t *child = parent->pp_children[i];
        if (child->p_item == media)
        {
            *result = child;
            if (result_parent)
                *result_parent = parent;
            return true;
        }

        if (vlc_media_tree_FindNodeByMedia(child, media, result, result_parent))
            return true;
    }

    return false;
}

static input_item_node_t *
vlc_media_tree_AddChild(input_item_node_t *parent, input_item_t *media);

static void
vlc_media_tree_AddSubtree(input_item_node_t *to, input_item_node_t *from)
{
    for (int i = 0; i < from->i_children; ++i)
    {
        input_item_node_t *child = from->pp_children[i];
        input_item_node_t *node = vlc_media_tree_AddChild(to, child->p_item);
        if (unlikely(!node))
            break; /* what could we do? */

        vlc_media_tree_AddSubtree(node, child);
    }
}

static void
vlc_media_tree_ClearChildren(input_item_node_t *root)
{
    for (int i = 0; i < root->i_children; ++i)
        input_item_node_Delete(root->pp_children[i]);

    free(root->pp_children);
    root->pp_children = NULL;
    root->i_children = 0;
}

static void
media_subtree_changed(input_item_t *media, input_item_node_t *node,
                      void *userdata)
{
    vlc_media_tree_t *tree = userdata;

    vlc_media_tree_Lock(tree);
    input_item_node_t *subtree_root;
    /* TODO retrieve the node without traversing the tree */
    bool found = vlc_media_tree_FindNodeByMedia(&tree->root, media,
                                                &subtree_root, NULL);
    if (!found) {
        /* the node probably failed to be allocated */
        vlc_media_tree_Unlock(tree);
        return;
    }

    vlc_media_tree_ClearChildren(subtree_root);
    vlc_media_tree_AddSubtree(subtree_root, node);
    vlc_media_tree_Notify(tree, on_children_reset, subtree_root);
    vlc_media_tree_Unlock(tree);
}

static void
media_subtree_preparse_ended(input_item_t *media,
                             enum input_item_preparse_status status,
                             void *user_data)
{
    vlc_media_tree_t *tree = user_data;

    vlc_media_tree_Lock(tree);
    input_item_node_t *subtree_root;
    /* TODO retrieve the node without traversing the tree */
    bool found = vlc_media_tree_FindNodeByMedia(&tree->root, media,
                                                &subtree_root, NULL);
    if (!found) {
        /* the node probably failed to be allocated */
        vlc_media_tree_Unlock(tree);
        return;
    }
    vlc_media_tree_Notify(tree, on_preparse_end, subtree_root, status);
    vlc_media_tree_Unlock(tree);
}

static inline void
vlc_media_tree_DestroyRootNode(vlc_media_tree_t *tree)
{
    vlc_media_tree_ClearChildren(&tree->root);
}

static void
vlc_media_tree_Delete(vlc_media_tree_t *tree)
{
    media_tree_private_t *priv = mt_priv(tree);
    vlc_media_tree_listener_id *listener;
    vlc_list_foreach(listener, &priv->listeners, node)
        free(listener);
    vlc_list_init(&priv->listeners); /* reset */
    vlc_media_tree_DestroyRootNode(tree);
    free(tree);
}

void
vlc_media_tree_Hold(vlc_media_tree_t *tree)
{
    media_tree_private_t *priv = mt_priv(tree);
    vlc_atomic_rc_inc(&priv->rc);
}

void
vlc_media_tree_Release(vlc_media_tree_t *tree)
{
    media_tree_private_t *priv = mt_priv(tree);
    if (vlc_atomic_rc_dec(&priv->rc))
        vlc_media_tree_Delete(tree);
}

void
vlc_media_tree_Lock(vlc_media_tree_t *tree)
{
    media_tree_private_t *priv = mt_priv(tree);
    vlc_mutex_lock(&priv->lock);
}

void
vlc_media_tree_Unlock(vlc_media_tree_t *tree)
{
    media_tree_private_t *priv = mt_priv(tree);
    vlc_mutex_unlock(&priv->lock);
}

static input_item_node_t *
vlc_media_tree_AddChild(input_item_node_t *parent, input_item_t *media)
{
    input_item_node_t *node = input_item_node_Create(media);
    if (unlikely(!node))
        return NULL;

    input_item_node_AppendNode(parent, node);

    return node;
}

static void
vlc_media_tree_NotifyCurrentState(vlc_media_tree_t *tree,
                                  vlc_media_tree_listener_id *listener)
{
    vlc_media_tree_NotifyListener(tree, listener, on_children_reset,
                                  &tree->root);
}

vlc_media_tree_listener_id *
vlc_media_tree_AddListener(vlc_media_tree_t *tree,
                           const struct vlc_media_tree_callbacks *cbs,
                           void *userdata, bool notify_current_state)
{
    vlc_media_tree_listener_id *listener = malloc(sizeof(*listener));
    if (unlikely(!listener))
        return NULL;
    listener->cbs = cbs;
    listener->userdata = userdata;

    media_tree_private_t *priv = mt_priv(tree);
    vlc_media_tree_Lock(tree);

    vlc_list_append(&listener->node, &priv->listeners);

    if (notify_current_state)
        vlc_media_tree_NotifyCurrentState(tree, listener);

    vlc_media_tree_Unlock(tree);
    return listener;
}

void
vlc_media_tree_RemoveListener(vlc_media_tree_t *tree,
                              vlc_media_tree_listener_id *listener)
{
    vlc_media_tree_Lock(tree);
    vlc_list_remove(&listener->node);
    vlc_media_tree_Unlock(tree);

    free(listener);
}

input_item_node_t *
vlc_media_tree_Add(vlc_media_tree_t *tree, input_item_node_t *parent,
                   input_item_t *media)
{
    vlc_media_tree_AssertLocked(tree);

    input_item_node_t *node = vlc_media_tree_AddChild(parent, media);
    if (unlikely(!node))
        return NULL;

    vlc_media_tree_Notify(tree, on_children_added, parent, &node, 1);

    return node;
}

bool
vlc_media_tree_Find(vlc_media_tree_t *tree, const input_item_t *media,
                    input_item_node_t **result,
                    input_item_node_t **result_parent)
{
    vlc_media_tree_AssertLocked(tree);

    /* quick & dirty depth-first O(n) implementation, with n the number of nodes
     * in the tree */
    return vlc_media_tree_FindNodeByMedia(&tree->root, media, result,
                                          result_parent);
}

bool
vlc_media_tree_Remove(vlc_media_tree_t *tree, input_item_t *media)
{
    vlc_media_tree_AssertLocked(tree);

    input_item_node_t *node;
    input_item_node_t *parent;
    if (!vlc_media_tree_FindNodeByMedia(&tree->root, media, &node, &parent))
        return false;

    input_item_node_RemoveNode(parent, node);
    vlc_media_tree_Notify(tree, on_children_removed, parent, &node, 1);
    input_item_node_Delete(node);
    return true;
}

static const input_preparser_callbacks_t input_preparser_callbacks = {
    .on_subtree_added = media_subtree_changed,
    .on_preparse_ended = media_subtree_preparse_ended
};

void
vlc_media_tree_Preparse(vlc_media_tree_t *tree, libvlc_int_t *libvlc,
                        input_item_t *media, void* id)
{
#ifdef TEST_MEDIA_SOURCE
    VLC_UNUSED(tree);
    VLC_UNUSED(libvlc);
    VLC_UNUSED(media);
    VLC_UNUSED(id);
    VLC_UNUSED(input_preparser_callbacks);
#else
    media->i_preparse_depth = 1;
    vlc_MetadataRequest(libvlc, media, META_REQUEST_OPTION_SCOPE_ANY |
                        META_REQUEST_OPTION_DO_INTERACT,
                        &input_preparser_callbacks, tree, 0, id);
#endif
}


void
vlc_media_tree_PreparseCancel(libvlc_int_t *libvlc, void* id)
{
#ifdef TEST_MEDIA_SOURCE
    VLC_UNUSED(libvlc);
    VLC_UNUSED(id);
#else
    libvlc_MetadataCancel(libvlc, id);
#endif
}
