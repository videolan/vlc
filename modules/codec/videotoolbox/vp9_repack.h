/*****************************************************************************
 * vp9_repack.h: VP9 superframe repacker
 *****************************************************************************
 * Copyright (C) 2023-2026 VideoLabs, VLC authors and VideoLAN
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
#ifndef VIDEOTOOLBOX_VP9_REPACK_H
#define VIDEOTOOLBOX_VP9_REPACK_H

#include <vlc_common.h>
#include <vlc_block.h>

struct vp9_repacker
{
    block_t *p_head;
    block_t **pp_append;
    uint8_t count;
    size_t maxsize;
    size_t total;
};

void vp9_repacker_Clean(struct vp9_repacker *);

void vp9_repacker_Init(struct vp9_repacker *);

block_t * vp9_repacker_Push(struct vp9_repacker *, block_t *, bool show_frame);

#endif // VIDEOTOOLBOX_VP9_REPACK_H
