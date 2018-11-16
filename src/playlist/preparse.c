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
#include "libvlc.h" /* for vlc_MetadataRequest() */

static void
vlc_playlist_CollectChildren(playlist_item_vector_t *dest,
                             input_item_node_t *node)
{
    for (int i = 0; i < node->i_children; ++i)
    {
        input_item_node_t *child = node->pp_children[i];
        vlc_playlist_item_t *item = vlc_playlist_item_New(child->p_item);
        if (item)
        {
            if (!vlc_vector_push(dest, item))
                vlc_playlist_item_Release(item);
        }
        vlc_playlist_CollectChildren(dest, child);
    }
}

bool
vlc_playlist_ExpandItem(vlc_playlist_t *playlist, size_t index,
                        input_item_node_t *node)
{
    vlc_playlist_AssertLocked(playlist);
    vlc_playlist_RemoveOne(playlist, index);

    playlist_item_vector_t flatten = VLC_VECTOR_INITIALIZER;
    vlc_playlist_CollectChildren(&flatten, node);

    if (vlc_vector_insert_all(&playlist->items, index, flatten.data,
                              flatten.size))
        vlc_playlist_ItemsInserted(playlist, index, flatten.size);

    vlc_vector_destroy(&flatten);
    return true;
}

bool
vlc_playlist_ExpandItemFromNode(vlc_playlist_t *playlist,
                                input_item_node_t *subitems)
{
    vlc_playlist_AssertLocked(playlist);
    input_item_t *media = subitems->p_item;
    ssize_t index = vlc_playlist_IndexOfMedia(playlist, media);
    if (index == -1)
        return false;

    /* replace the item by its flatten subtree */
    vlc_playlist_ExpandItem(playlist, index, subitems);
    return true;
}

static void
on_subtree_added(input_item_t *media, input_item_node_t *subtree,
                 void *userdata)
{
    VLC_UNUSED(media); /* retrieved by subtree->p_item */
    vlc_playlist_t *playlist = userdata;

    vlc_playlist_Lock(playlist);
    vlc_playlist_ExpandItemFromNode(playlist, subtree);
    vlc_playlist_Unlock(playlist);
}

static const input_preparser_callbacks_t input_preparser_callbacks = {
    .on_subtree_added = on_subtree_added,
};

void
vlc_playlist_Preparse(vlc_playlist_t *playlist, libvlc_int_t *libvlc,
                      input_item_t *input)
{
#ifdef TEST_PLAYLIST
    VLC_UNUSED(playlist);
    VLC_UNUSED(libvlc);
    VLC_UNUSED(input);
    VLC_UNUSED(input_preparser_callbacks);
#else
    /* vlc_MetadataRequest is not exported */
    vlc_MetadataRequest(libvlc, input, META_REQUEST_OPTION_NONE,
                        &input_preparser_callbacks, playlist, -1, NULL);
#endif
}
