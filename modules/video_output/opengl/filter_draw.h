/*****************************************************************************
 * filter_draw.h
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 * Copyright (C) 2020 Videolabs
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

#ifndef VLC_GL_FILTER_DRAW_H
#define VLC_GL_FILTER_DRAW_H

#include "filter.h"

#define DRAW_VFLIP_SHORTTEXT "VFlip the video"
#define DRAW_VFLIP_LONGTEXT \
    "Apply a vertical flip to the video"

#define DRAW_CFG_PREFIX "draw-"

#define add_opengl_submodule_draw() \
    add_submodule() \
    add_shortcut("draw") \
    set_shortname("draw") \
    set_capability("opengl filter", 0) \
    set_callback(vlc_gl_filter_draw_Open) \
    add_bool(DRAW_CFG_PREFIX "vflip", false, \
             DRAW_VFLIP_SHORTTEXT, DRAW_VFLIP_LONGTEXT)

vlc_gl_filter_open_fn vlc_gl_filter_draw_Open;

#endif
