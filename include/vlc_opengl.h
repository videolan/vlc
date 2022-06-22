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

#include "vlc_es.h"

/**
 * \file
 * This file defines GL structures and functions.
 */

struct vout_window_t;
struct vout_window_cfg_t;
struct vout_display_cfg;

/**
 * A VLC GL context (and its underlying surface)
 */
typedef struct vlc_gl_t vlc_gl_t;
struct vlc_decoder_device;
struct vlc_video_context;

enum vlc_gl_api_type {
    VLC_OPENGL,
    VLC_OPENGL_ES2,
};

struct vlc_gl_t
{
    struct vlc_object_t obj;

    module_t *module;
    void *sys;

    union {
        struct { /* on-screen */
            void (*swap)(vlc_gl_t *);

            struct vout_window_t *surface;
        };
        struct { /* off-screen */
            picture_t *(*swap_offscreen)(vlc_gl_t *);

            struct vlc_decoder_device *device;
            vlc_fourcc_t offscreen_chroma_out;
            struct vlc_video_context *offscreen_vctx_out;
            /* Flag to indicate if the OpenGL implementation produces upside-down
             * pictures */
            bool offscreen_vflip;
        };
    };

    int  (*make_current)(vlc_gl_t *);
    void (*release_current)(vlc_gl_t *);
    void (*resize)(vlc_gl_t *, unsigned, unsigned);
    void*(*get_proc_address)(vlc_gl_t *, const char *);
    void (*destroy)(vlc_gl_t *);

    enum {
        VLC_GL_EXT_DEFAULT,
        VLC_GL_EXT_EGL,
        VLC_GL_EXT_WGL,
    } ext;

    union {
        /* if ext == VLC_GL_EXT_EGL */
        struct {
            /* call eglQueryString() with current display */
            const char *(*queryString)(vlc_gl_t *, int32_t name);
            /* call eglCreateImageKHR() with current display and context, can
             * be NULL */
            void *(*createImageKHR)(vlc_gl_t *, unsigned target, void *buffer,
                                    const int32_t *attrib_list);
            /* call eglDestroyImageKHR() with current display, can be NULL */
            bool (*destroyImageKHR)(vlc_gl_t *, void *image);
        } egl;
        /* if ext == VLC_GL_EXT_WGL */
        struct
        {
            const char *(*getExtensionsString)(vlc_gl_t *);
        } wgl;
    };

    /* Defined by the core for libvlc_opengl API loading. */
    enum vlc_gl_api_type api_type;
};

/**
 * Creates an OpenGL context (and its underlying surface).
 *
 * @note In most cases, you should vlc_gl_MakeCurrent() afterward.
 *
 * @param cfg initial configuration (including window to use as OpenGL surface)
 * @param flags OpenGL context type
 * @param name module name (or NULL for auto)
 * @return a new context, or NULL on failure
 */
VLC_API vlc_gl_t *vlc_gl_Create(const struct vout_display_cfg *cfg,
                                unsigned flags, const char *name) VLC_USED;
VLC_API vlc_gl_t *vlc_gl_CreateOffscreen(vlc_object_t *parent,
                                         struct vlc_decoder_device *device,
                                         unsigned width, unsigned height,
                                         unsigned flags, const char *name);

VLC_API void vlc_gl_Release(vlc_gl_t *);
VLC_API void vlc_gl_Hold(vlc_gl_t *);

static inline int vlc_gl_MakeCurrent(vlc_gl_t *gl)
{
    return gl->make_current(gl);
}

static inline void vlc_gl_ReleaseCurrent(vlc_gl_t *gl)
{
    gl->release_current(gl);
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

static inline picture_t *vlc_gl_SwapOffscreen(vlc_gl_t *gl)
{
    return gl->swap_offscreen(gl);
}

/**
 * Fetch a symbol or pointer function from the OpenGL implementation.
 *
 * Return a pointer from the OpenGL implementation, which can be part of
 * either the underlying OpenGL provider or an OpenGL function matching
 * the version requested.
 *
 * If the symbol name is not matching the underlying implementation of
 * OpenGL, an invalid pointer or NULL can be returned.
 *
 * @note This function must be called between MakeCurrent and ReleaseCurrent.
 *
 * @param gl the OpenGL provider to fetch the function from
 * @param name the symbol name to fetch from the implementation
 *
 * @return A pointer corresponding to the symbol, or a potentially invalid
 *         value or NULL in case of error.
 */
static inline void *vlc_gl_GetProcAddress(vlc_gl_t *gl, const char *name)
{
    return gl->get_proc_address(gl, name);
}

VLC_API vlc_gl_t *vlc_gl_surface_Create(vlc_object_t *,
                                        const struct vout_window_cfg_t *,
                                        struct vout_window_t **) VLC_USED;
VLC_API bool vlc_gl_surface_CheckSize(vlc_gl_t *, unsigned *w, unsigned *h);
VLC_API void vlc_gl_surface_Destroy(vlc_gl_t *);

static inline bool vlc_gl_StrHasToken(const char *apis, const char *api)
{
    size_t apilen = strlen(api);
    while (apis) {
        while (*apis == ' ')
            apis++;
        if (!strncmp(apis, api, apilen) && memchr(" ", apis[apilen], 2))
            return true;
        apis = strchr(apis, ' ');
    }
    return false;
}

#endif /* VLC_GL_H */
