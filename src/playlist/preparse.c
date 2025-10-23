/*****************************************************************************
 * playlist/preparse.c
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

#include "preparse.h"

#include "content.h"
#include "item.h"
#include "playlist.h"
#include "notify.h"

typedef struct VLC_VECTOR(input_item_t *) media_vector_t;

static void
vlc_playlist_CollectChildren(vlc_playlist_t *playlist,
                             media_vector_t *dest,
                             const input_item_node_t *node)
{
    vlc_playlist_AssertLocked(playlist);
    for (int i = 0; i < node->i_children; ++i)
    {
        input_item_node_t *child = node->pp_children[i];
        input_item_t *media = child->p_item;
        vlc_vector_push(dest, media);
        vlc_playlist_CollectChildren(playlist, dest, child);
    }
}

int
vlc_playlist_ExpandItem(vlc_playlist_t *playlist, size_t index,
                        const input_item_node_t *node)
{
    vlc_playlist_AssertLocked(playlist);

    media_vector_t flatten = VLC_VECTOR_INITIALIZER;
    vlc_playlist_CollectChildren(playlist, &flatten, node);

    int ret = vlc_playlist_Expand(playlist, index, flatten.data, flatten.size);
    vlc_vector_destroy(&flatten);

    return ret;
}

int
vlc_playlist_ExpandItemFromNode(vlc_playlist_t *playlist,
                                const input_item_node_t *subitems)
{
    vlc_playlist_AssertLocked(playlist);
    input_item_t *media = subitems->p_item;
    ssize_t index = vlc_playlist_IndexOfMedia(playlist, media);
    if (index == -1)
        return VLC_ENOENT;

    /* replace the item by its flatten subtree */
    return vlc_playlist_ExpandItem(playlist, index, subitems);
}

static void
on_subtree_added(vlc_preparser_req *req, input_item_node_t *subtree,
                 void *userdata)
{
    VLC_UNUSED(req); /* retrieved by subtree->p_item */
    vlc_playlist_t *playlist = userdata;

    vlc_playlist_Lock(playlist);
    vlc_playlist_ExpandItemFromNode(playlist, subtree);
    vlc_playlist_Unlock(playlist);
    input_item_node_Delete(subtree);
}

static void
on_preparse_ended(vlc_preparser_req *req, int status, void *userdata)
{
    input_item_t *media = vlc_preparser_req_GetItem(req);
    vlc_playlist_t *playlist = userdata;

    if (status != VLC_SUCCESS)
        return;

    vlc_playlist_Lock(playlist);
    ssize_t index = vlc_playlist_IndexOfMedia(playlist, media);
    if (index != -1)
        vlc_playlist_Notify(playlist, on_items_updated, index,
                            &playlist->items.data[index], 1);
    vlc_playlist_Unlock(playlist);
    vlc_preparser_req_Release(req);
}

static const struct vlc_preparser_cbs preparser_callbacks = {
    .on_ended = on_preparse_ended,
    .on_subtree_added = on_subtree_added,
};

vlc_preparser_req *
vlc_playlist_AutoPreparse(vlc_playlist_t *playlist, input_item_t *input,
                          bool parse_subitems)
{
#ifdef TEST_PLAYLIST
    VLC_UNUSED(preparser_callbacks);
#endif

    if (playlist->parser != NULL && !input_item_IsPreparsed(input))
    {
        switch (playlist->recursive)
        {
            case VLC_PLAYLIST_PREPARSING_ENABLED:
                parse_subitems = false;
                break;
            case VLC_PLAYLIST_PREPARSING_COLLAPSE:
                break;
            case VLC_PLAYLIST_PREPARSING_RECURSIVE:
                parse_subitems = true;
                break;
            default: vlc_assert_unreachable();
        }

        bool input_net;
        enum input_item_type_e input_type = input_item_GetType(input, &input_net);

        if (input_net)
            return NULL;

        switch (input_type)
        {
            case ITEM_TYPE_NODE:
            case ITEM_TYPE_FILE:
            case ITEM_TYPE_DIRECTORY:
            case ITEM_TYPE_PLAYLIST:
                break;
            default:
                return NULL;
        }

        int options = VLC_PREPARSER_TYPE_PARSE | VLC_PREPARSER_TYPE_FETCHMETA_LOCAL;
        if (parse_subitems)
            options |= VLC_PREPARSER_OPTION_SUBITEMS;

        return vlc_preparser_Push(playlist->parser, input, options,
                                  &preparser_callbacks, playlist);
    }
    return NULL;
}
