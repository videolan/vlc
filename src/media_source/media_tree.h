/*****************************************************************************
 * media_tree.h
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

#ifndef MEDIA_TREE_H
#define MEDIA_TREE_H

#include <vlc_media_source.h>

vlc_media_tree_t *
vlc_media_tree_New(void);

void
vlc_media_tree_Hold(vlc_media_tree_t *tree);

void
vlc_media_tree_Release(vlc_media_tree_t *tree);

input_item_node_t *
vlc_media_tree_Add(vlc_media_tree_t *tree, input_item_node_t *parent,
                   input_item_t *media);

bool
vlc_media_tree_Remove(vlc_media_tree_t *tree, input_item_t *media);

#endif
