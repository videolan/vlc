/*****************************************************************************
 * Copyright (C) 2023 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef COMPOSITOR_WAYLAND_MODULE_H
#define COMPOSITOR_WAYLAND_MODULE_H

#include <vlc_window.h>

typedef struct qtwayland_t
{
    struct vlc_object_t obj;

    module_t* p_module;
    void* p_sys;

    bool (*init)(struct qtwayland_t*, void* display);

    int (*setupInterface)(struct qtwayland_t*, void* video_surface, double scale);

    int (*setupVoutWindow)(struct qtwayland_t*, vlc_window_t* p_wnd);
    void (*teardownVoutWindow)(struct qtwayland_t*);

    void (*enable)(struct qtwayland_t*, const vlc_window_cfg_t *);
    void (*disable)(struct qtwayland_t*);
    void (*move)(struct qtwayland_t*, int x, int y, bool commitSurface);
    void (*resize)(struct qtwayland_t*, size_t width, size_t height, bool commitSurface);
    void (*rescale)(struct qtwayland_t*, double scale);

    void (*commitSurface)(struct qtwayland_t*);

    void (*close)(struct qtwayland_t*);
} qtwayland_t;

int OpenCompositor(vlc_object_t* p_this);

#endif // COMPOSITOR_WAYLAND_MODULE_H
