/*****************************************************************************
 * d3d_render.h: Direct3D Render callbacks
 *****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * Authors: Steve Lhomme <robux4@gmail.com>
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

#ifndef VLC_D3D_RENDER_H
#define VLC_D3D_RENDER_H

struct device_cfg_t {
    bool hardware_decoding;
};

struct device_setup_t {
    void *device_context;
};

struct direct3d_cfg_t {
    unsigned width;
    unsigned height;
};

struct output_cfg_t {
    int surface_format;
};

typedef bool (*d3d_device_setup_cb)(void* opaque, const struct device_cfg_t*, struct device_setup_t* );
typedef void (*d3d_device_cleanup_cb)(void* opaque);
typedef bool (*d3d_update_output_cb)(void* opaque, const struct direct3d_cfg_t *cfg, struct output_cfg_t *out);
typedef void (*d3d_swap_cb)(void* opaque);
typedef bool (*d3d_start_end_rendering_cb)(void* opaque, bool enter);


#endif /* VLC_D3D_RENDER_H */
