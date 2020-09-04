/*****************************************************************************
 * hw_pool.h: hw based picture pool
 *****************************************************************************
 * Copyright (C) 2019-2020 VLC authors and VideoLAN
 *
 * Authors: Jai Luthra <me@jailuthra.in>
 *          Quentin Chateau <quentin.chateau@deepskycorp.com>
 *          Steve Lhomme <robux4@videolabs.io>
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
#include <vlc_picture.h>
#include <vlc_codec.h>

typedef struct nvdec_pool_t  nvdec_pool_t;
typedef struct nvdec_pool_owner  nvdec_pool_owner_t;

struct nvdec_pool_owner
{
    void *sys;
    void (*release_resources)(nvdec_pool_owner_t *, void *buffers[], size_t pics_count);
    picture_context_t * (*attach_picture)(nvdec_pool_owner_t *, nvdec_pool_t *, void *surface);
};

nvdec_pool_t* nvdec_pool_Create(nvdec_pool_owner_t *,
                                const video_format_t *, vlc_video_context *,
                                void *buffers[], size_t pics_count);
void nvdec_pool_AddRef(nvdec_pool_t *);
void nvdec_pool_Release(nvdec_pool_t *);

/**
 * Wait for a new picture to be available from the pool.
 *
 * The picture.p_sys is always NULL.
 */
picture_t* nvdec_pool_Wait(nvdec_pool_t *);
