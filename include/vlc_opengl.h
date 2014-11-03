/*****************************************************************************
 * vlc_opengl.h: VLC GL API
 *****************************************************************************
 * Copyright (C) 2009 Laurent Aimar
 * Copyright (C) 2011 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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

#ifndef VLC_GL_H
#define VLC_GL_H 1

/**
 * \file
 * This file defines GL structures and functions.
 */

struct vout_window_t;
struct vout_window_cfg_t;

/**
 * A VLC GL context (and its underlying surface)
 */
typedef struct vlc_gl_t vlc_gl_t;

struct vlc_gl_t
{
    VLC_COMMON_MEMBERS

    struct vout_window_t *surface;
    module_t *module;
    void *sys;

    int  (*makeCurrent)(vlc_gl_t *);
    void (*releaseCurrent)(vlc_gl_t *);
    void (*resize)(vlc_gl_t *, unsigned, unsigned);
    void (*swap)(vlc_gl_t *);
#ifdef __APPLE__
    int  (*lock)(vlc_gl_t *);
    void (*unlock)(vlc_gl_t *);
#endif
    void*(*getProcAddress)(vlc_gl_t *, const char *);
};

enum {
    VLC_OPENGL,
    VLC_OPENGL_ES,
    VLC_OPENGL_ES2,
};

VLC_API vlc_gl_t *vlc_gl_Create(struct vout_window_t *, unsigned, const char *) VLC_USED;
VLC_API void vlc_gl_Destroy(vlc_gl_t *);

static inline int vlc_gl_MakeCurrent(vlc_gl_t *gl)
{
    return gl->makeCurrent(gl);
}

static inline void vlc_gl_ReleaseCurrent(vlc_gl_t *gl)
{
    gl->releaseCurrent(gl);
}

static inline int vlc_gl_Lock(vlc_gl_t *gl)
{
#ifdef __APPLE__
    return (gl->lock != NULL) ? gl->lock(gl) : VLC_SUCCESS;
#else
    (void) gl; return VLC_SUCCESS;
#endif
}

static inline void vlc_gl_Unlock(vlc_gl_t *gl)
{
#ifdef __APPLE__
    if (gl->unlock != NULL)
        gl->unlock(gl);
#else
    (void) gl;
#endif
}

static inline void vlc_gl_Resize(vlc_gl_t *gl, unsigned w, unsigned h)
{
    if (gl->resize != NULL)
        gl->resize(gl, w, h);
}

static inline void vlc_gl_Swap(vlc_gl_t *gl)
{
    gl->swap(gl);
}

static inline void *vlc_gl_GetProcAddress(vlc_gl_t *gl, const char *name)
{
    return (gl->getProcAddress != NULL) ? gl->getProcAddress(gl, name) : NULL;
}

VLC_API vlc_gl_t *vlc_gl_surface_Create(vlc_object_t *,
                                        const struct vout_window_cfg_t *,
                                        struct vout_window_t **) VLC_USED;
VLC_API bool vlc_gl_surface_CheckSize(vlc_gl_t *, unsigned *w, unsigned *h);
VLC_API void vlc_gl_surface_Destroy(vlc_gl_t *);

#endif /* VLC_GL_H */
