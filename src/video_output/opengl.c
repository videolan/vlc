/*****************************************************************************
 * opengl.c: VLC GL API
 *****************************************************************************
 * Copyright (C) 2011 Rémi Denis-Courmont
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
# include <config.h>
#endif

#include <vlc_common.h>
#include <vlc_opengl.h>
#include "libvlc.h"
#include <vlc_modules.h>

#undef vlc_gl_Create
/**
 * Creates an OpenGL context (and its underlying surface).
 *
 * @note In most cases, you should vlc_gl_MakeCurrent() afterward.
 *
 * @param wnd window to use as OpenGL surface
 * @param flags OpenGL context type
 * @param name module name (or NULL for auto)
 * @return a new context, or NULL on failure
 */
vlc_gl_t *vlc_gl_Create(struct vout_window_t *wnd, unsigned flags,
                        const char *name)
{
    vlc_object_t *parent = (vlc_object_t *)wnd;
    vlc_gl_t *gl;
    const char *type;

    switch (flags /*& VLC_OPENGL_API_MASK*/)
    {
        case VLC_OPENGL:
            type = "opengl";
            break;
        case VLC_OPENGL_ES:
            type = "opengl es";
            break;
        case VLC_OPENGL_ES2:
            type = "opengl es2";
            break;
        default:
            return NULL;
    }

    gl = vlc_custom_create(parent, sizeof (*gl), "gl");
    if (unlikely(gl == NULL))
        return NULL;

    gl->surface = wnd;
    gl->module = module_need(gl, type, name, true);
    if (gl->module == NULL)
    {
        vlc_object_release(gl);
        return NULL;
    }

    return gl;
}

void vlc_gl_Destroy(vlc_gl_t *gl)
{
    module_unneed(gl, gl->module);
    vlc_object_release(gl);
}
