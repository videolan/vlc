/*****************************************************************************
 * gl_api.h
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

#ifndef VLC_GL_API_H
#define VLC_GL_API_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdbool.h>
#include <vlc_common.h>
#include <vlc_opengl.h>

#include "gl_common.h"

struct vlc_gl_api {
    opengl_vtable_t vt;

    /* True if the current API is OpenGL ES, set by the caller */
    bool is_gles;

    /* Available gl extensions (from GL_EXTENSIONS) */
    const char *extensions;

    /* Non-power-of-2 texture size support */
    bool supports_npot;
};

int
vlc_gl_api_Init(struct vlc_gl_api *api, vlc_gl_t *gl);

#endif
