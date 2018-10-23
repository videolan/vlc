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

typedef struct VLC_VECTOR(input_item_t *) media_vector_t;

static void
vlc_playlist_CollectChildren(vlc_playlist_t *playlist,
                             media_vector_t *dest,
                             input_item_node_t *node)
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
                        input_item_node_t *node)
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
                                input_item_node_t *subitems)
{
    vlc_playlist_AssertLocked(playlist);
    input_item_t *media = subitems->p_item;
    ssize_t index = vlc_playlist_IndexOfMedia(playlist, media);
    if (index == -1)
        return VLC_ENOITEM;

    /* replace the item by its flatten subtree */
    return vlc_playlist_ExpandItem(playlist, index, subitems);
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
