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

#include <assert.h>
#include <stdlib.h>

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_opengl.h>
#include "libvlc.h"
#include <vlc_modules.h>

struct vlc_gl_priv_t
{
    vlc_gl_t gl;
    atomic_uint ref_count;
};
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
    struct vlc_gl_priv_t *glpriv;
    const char *type;

    switch (flags /*& VLC_OPENGL_API_MASK*/)
    {
        case VLC_OPENGL:
            type = "opengl";
            break;
        case VLC_OPENGL_ES2:
            type = "opengl es2";
            break;
        default:
            return NULL;
    }

    glpriv = vlc_custom_create(parent, sizeof (*glpriv), "gl");
    if (unlikely(glpriv == NULL))
        return NULL;

    glpriv->gl.surface = wnd;
    glpriv->gl.module = module_need(&glpriv->gl, type, name, true);
    if (glpriv->gl.module == NULL)
    {
        vlc_object_release(&glpriv->gl);
        return NULL;
    }
    atomic_init(&glpriv->ref_count, 1);

    return &glpriv->gl;
}

void vlc_gl_Hold(vlc_gl_t *gl)
{
    struct vlc_gl_priv_t *glpriv = (struct vlc_gl_priv_t *)gl;
    atomic_fetch_add(&glpriv->ref_count, 1);
}

void vlc_gl_Release(vlc_gl_t *gl)
{
    struct vlc_gl_priv_t *glpriv = (struct vlc_gl_priv_t *)gl;
    if (atomic_fetch_sub(&glpriv->ref_count, 1) != 1)
        return;
    module_unneed(gl, gl->module);
    vlc_object_release(gl);
}

#include <vlc_vout_window.h>

typedef struct vlc_gl_surface
{
    int width;
    int height;
    vlc_mutex_t lock;
} vlc_gl_surface_t;

static void vlc_gl_surface_ResizeNotify(vout_window_t *surface,
                                        unsigned width, unsigned height)
{
    vlc_gl_surface_t *sys = surface->owner.sys;

    msg_Dbg(surface, "resized to %ux%u", width, height);

    vlc_mutex_lock(&sys->lock);
    sys->width = width;
    sys->height = height;
    vlc_mutex_unlock(&sys->lock);
}

vlc_gl_t *vlc_gl_surface_Create(vlc_object_t *obj,
                                const vout_window_cfg_t *cfg,
                                struct vout_window_t **restrict wp)
{
    vlc_gl_surface_t *sys = malloc(sizeof (*sys));
    if (unlikely(sys == NULL))
        return NULL;

    sys->width = cfg->width;
    sys->height = cfg->height;
    vlc_mutex_init(&sys->lock);

    vout_window_owner_t owner = {
        .sys = sys,
        .resized = vlc_gl_surface_ResizeNotify,
    };

    vout_window_t *surface = vout_window_New(obj, "$window", cfg, &owner);
    if (surface == NULL)
        goto error;
    if (wp != NULL)
        *wp = surface;

    /* TODO: support ES? */
    vlc_gl_t *gl = vlc_gl_Create(surface, VLC_OPENGL, NULL);
    if (gl == NULL) {
        vout_window_Delete(surface);
        return NULL;
    }

    vlc_gl_Resize(gl, cfg->width, cfg->height);
    return gl;

error:
    vlc_mutex_destroy(&sys->lock);
    free(sys);
    return NULL;
}

/**
 * Checks if the dimensions of the surface used by the OpenGL context have
 * changed (since the previous call), and  the OpenGL viewport should be
 * updated.
 * \return true if at least one dimension has changed, false otherwise
 * \warning This function is intrinsically race-prone.
 * The dimensions can change asynchronously.
 */
bool vlc_gl_surface_CheckSize(vlc_gl_t *gl, unsigned *restrict width,
                              unsigned *restrict height)
{
    vout_window_t *surface = gl->surface;
    vlc_gl_surface_t *sys = surface->owner.sys;
    bool ret = false;

    vlc_mutex_lock(&sys->lock);
    if (sys->width >= 0 && sys->height >= 0)
    {
        *width = sys->width;
        *height = sys->height;
        sys->width = -1;
        sys->height = -1;

        vlc_gl_Resize(gl, *width, *height);
        ret = true;
    }
    vlc_mutex_unlock(&sys->lock);
    return ret;
}

void vlc_gl_surface_Destroy(vlc_gl_t *gl)
{
    vout_window_t *surface = gl->surface;
    vlc_gl_surface_t *sys = surface->owner.sys;

    vlc_gl_Release(gl);
    vout_window_Delete(surface);
    vlc_mutex_destroy(&sys->lock);
    free(sys);
}
