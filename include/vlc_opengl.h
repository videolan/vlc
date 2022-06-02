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

# ifdef __cplusplus
extern "C" {
# endif

/**
 * \file
 * This file defines GL structures and functions.
 */

struct vlc_window;
struct vlc_window_cfg;
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

struct vlc_gl_operations
{
    union {
        void (*swap)(vlc_gl_t *);
        picture_t *(*swap_offscreen)(vlc_gl_t *);
    };
    int  (*make_current)(vlc_gl_t *gl);
    void (*release_current)(vlc_gl_t *gl);
    void (*resize)(vlc_gl_t *gl, unsigned width, unsigned height);
    void*(*get_proc_address)(vlc_gl_t *gl, const char *symbol);
    void (*close)(vlc_gl_t *gl);
};

struct vlc_gl_t
{
    struct vlc_object_t obj;

    module_t *module;
    void *sys;

    struct vlc_decoder_device *device;
    union {
        struct { /* on-screen */
            struct vlc_window *surface;
        };
        struct { /* off-screen */
            vlc_fourcc_t offscreen_chroma_out;
            struct vlc_video_context *offscreen_vctx_out;
            /* Flag to indicate if the OpenGL implementation produces upside-down
             * pictures */
            bool offscreen_vflip;
        };
    };

    /* Defined by the core for libvlc_opengl API loading. */
    enum vlc_gl_api_type api_type;

    const struct vlc_gl_operations *ops;
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

VLC_API void vlc_gl_Delete(vlc_gl_t *);

static inline int vlc_gl_MakeCurrent(vlc_gl_t *gl)
{
    return gl->ops->make_current(gl);
}

static inline void vlc_gl_ReleaseCurrent(vlc_gl_t *gl)
{
    gl->ops->release_current(gl);
}

static inline void vlc_gl_Resize(vlc_gl_t *gl, unsigned w, unsigned h)
{
    if (gl->ops->resize != NULL)
        gl->ops->resize(gl, w, h);
}

static inline void vlc_gl_Swap(vlc_gl_t *gl)
{
    gl->ops->swap(gl);
}

static inline picture_t *vlc_gl_SwapOffscreen(vlc_gl_t *gl)
{
    return gl->ops->swap_offscreen(gl);
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
    return gl->ops->get_proc_address(gl, name);
}

VLC_API vlc_gl_t *vlc_gl_surface_Create(vlc_object_t *,
                                        const struct vlc_window_cfg *,
                                        struct vlc_window **) VLC_USED;
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

#ifdef __cplusplus
}
#endif /* C++ */

#endif /* VLC_GL_H */
