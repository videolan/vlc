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
#include <vlc_vout_display.h>
#include "libvlc.h"
#include <vlc_modules.h>

struct vlc_gl_priv_t
{
    vlc_gl_t gl;
    vlc_atomic_rc_t rc;
};

static int vlc_gl_start(void *func, bool forced, va_list ap)
{
    int (*activate)(vlc_gl_t *, unsigned, unsigned) = func;
    vlc_gl_t *gl = va_arg(ap, vlc_gl_t *);
    unsigned width = va_arg(ap, unsigned);
    unsigned height = va_arg(ap, unsigned);

    int ret = activate(gl, width, height);
    if (ret)
        vlc_objres_clear(VLC_OBJECT(gl));
    (void) forced;
    return ret;
}

vlc_gl_t *vlc_gl_Create(const struct vout_display_cfg *restrict cfg,
                        unsigned flags, const char *name)
{
    vout_window_t *wnd = cfg->window;
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

    glpriv = vlc_custom_create(VLC_OBJECT(wnd), sizeof (*glpriv), "gl");
    if (unlikely(glpriv == NULL))
        return NULL;

    vlc_gl_t *gl = &glpriv->gl;
    gl->surface = wnd;
    gl->module = vlc_module_load(gl, type, name, true, vlc_gl_start, gl,
                                 cfg->display.width, cfg->display.height);
    if (gl->module == NULL)
    {
        vlc_object_delete(gl);
        return NULL;
    }
    assert(gl->make_current && gl->release_current && gl->swap
        && gl->get_proc_address);
    vlc_atomic_rc_init(&glpriv->rc);

    return &glpriv->gl;
}

void vlc_gl_Hold(vlc_gl_t *gl)
{
    struct vlc_gl_priv_t *glpriv = (struct vlc_gl_priv_t *)gl;
    vlc_atomic_rc_inc(&glpriv->rc);
}

void vlc_gl_Release(vlc_gl_t *gl)
{
    struct vlc_gl_priv_t *glpriv = (struct vlc_gl_priv_t *)gl;
    if (!vlc_atomic_rc_dec(&glpriv->rc))
        return;

    if (gl->destroy != NULL)
        gl->destroy(gl);
    vlc_objres_clear(VLC_OBJECT(gl));
    vlc_object_delete(gl);
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

    static const struct vout_window_callbacks cbs = {
        .resized = vlc_gl_surface_ResizeNotify,
    };
    vout_window_owner_t owner = {
        .cbs = &cbs,
        .sys = sys,
    };
    char *modlist = var_InheritString(obj, "window");

    vout_window_t *surface = vout_window_New(obj, modlist, &owner);
    free(modlist);
    if (surface == NULL)
        goto error;
    if (vout_window_Enable(surface, cfg)) {
        vout_window_Delete(surface);
        goto error;
    }
    if (wp != NULL)
        *wp = surface;

    /* TODO: support ES? */
    struct vout_display_cfg dcfg = {
        .window = surface,
        .display = { .width = cfg->width, cfg->height },
    };

    vlc_mutex_lock(&sys->lock);
    if (sys->width >= 0 && sys->height >= 0) {
        dcfg.display.width = sys->width;
        dcfg.display.height = sys->height;
        sys->width = -1;
        sys->height = -1;
    }
    vlc_mutex_unlock(&sys->lock);

    vlc_gl_t *gl = vlc_gl_Create(&dcfg, VLC_OPENGL, NULL);
    if (gl == NULL) {
        vout_window_Delete(surface);
        goto error;
    }

    return gl;

error:
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
    vout_window_Disable(surface);
    vout_window_Delete(surface);
    free(sys);
}
