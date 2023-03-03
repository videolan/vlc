/*****************************************************************************
 * egl_surfacetexture.c: OpenGL offscreen provider with SurfaceTexture
 *****************************************************************************
 * Copyright (C) 2021 Videolabs
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
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_picture.h>
#include <vlc_filter.h>
#include <vlc_opengl.h>
#include <vlc_vout_display.h>
#include <vlc_picture_pool.h>
#include <vlc_atomic.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "../video_output/android/utils.h"
#include "../video_output/opengl/gl_api.h"
#include "../video_output/opengl/gl_util.h"

#define BUFFER_COUNT 3

struct video_ctx
{
    android_video_context_t android;
    picture_pool_t *pool;
    picture_t *pictures[BUFFER_COUNT];

    EGLDisplay display;
    EGLContext context;
};

struct picture_ctx
{
    struct picture_context_t context;
    EGLSurface surface;

    struct vlc_asurfacetexture *texture;
};

#define PRIV(pic_ctx) container_of(pic_ctx, struct picture_ctx, context)

struct surfacetexture_sys
{
    android_video_context_t *avctx;

    video_format_t fmt_out;

    size_t current_flip;

    struct vlc_gl_api api;

    EGLConfig cfgv;

    picture_t *current_picture;
};

static inline struct video_ctx *GetVCtx(vlc_gl_t *gl)
{
    return vlc_video_context_GetPrivate(gl->offscreen_vctx_out,
                                        VLC_VIDEO_CONTEXT_AWINDOW);
}

static int MakeCurrent(vlc_gl_t *gl)
{
    struct surfacetexture_sys *sys = gl->sys;
    struct video_ctx *vctx = GetVCtx(gl);

    /* We must always have a surface mapped. */
    assert(sys->current_picture);
    assert(sys->current_picture->context);

    struct picture_ctx *ctx = PRIV(sys->current_picture->context);

    if (eglMakeCurrent (vctx->display, ctx->surface, ctx->surface,
                        vctx->context) != EGL_TRUE)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static void ReleaseCurrent(vlc_gl_t *gl)
{
    struct video_ctx *vctx = GetVCtx(gl);

    eglMakeCurrent(vctx->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                    EGL_NO_CONTEXT);
}

static void *GetSymbol(vlc_gl_t *gl, const char *procname)
{
    (void) gl;
    return (void *)eglGetProcAddress(procname);
}

static picture_context_t *CopyPictureContext(picture_context_t *input)
{
    vlc_video_context_Hold(input->vctx);
    return input;
}

static void DestroyPictureContext(picture_context_t *input)
{
    (void) input;
    /* video context is already released by picture_Release. */
}

static picture_context_t *CreatePictureContext(vlc_gl_t *gl)
{
    struct surfacetexture_sys *sys = gl->sys;
    struct picture_ctx *ctx = malloc(sizeof(*ctx));
    struct video_ctx *vctx = GetVCtx(gl);

    ctx->texture = vlc_asurfacetexture_New(gl->device->opaque, false);

    struct ANativeWindow *window = ctx->texture->window;
    native_window_api_t *api =
        AWindowHandler_getANativeWindowAPI(gl->device->opaque);
    api->setBuffersGeometry(window, sys->fmt_out.i_width, sys->fmt_out.i_height,
                            AHARDWAREBUFFER_FORMAT_BLOB);

    /* Create a drawing surface */
    ctx->surface = eglCreateWindowSurface(vctx->display, sys->cfgv, window,
                                          NULL);
    if (ctx->surface == EGL_NO_SURFACE)
    {
        msg_Err(gl, "cannot create EGL window surface");
        goto error;
    }

    ctx->context.vctx = gl->offscreen_vctx_out;
    ctx->context.copy = CopyPictureContext;
    ctx->context.destroy = DestroyPictureContext;

    return &ctx->context;

error:
    free(ctx);
    return NULL;
}

static int InitEGL(vlc_gl_t *gl)
{
    struct surfacetexture_sys *sys = gl->sys;
    struct video_ctx *vctx = GetVCtx(gl);

    vctx->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (vctx->display == EGL_NO_DISPLAY)
        return VLC_EGENERIC;

    /* Initialize EGL display */
    EGLint major, minor;
    if (eglInitialize(vctx->display, &major, &minor) != EGL_TRUE)
        goto error;
    msg_Dbg(gl, "EGL version %s by %s, API %s",
            eglQueryString(vctx->display, EGL_VERSION),
            eglQueryString(vctx->display, EGL_VENDOR),
            "OpenGL ES2"
            );

    const EGLint conf_attr[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_NONE,
    };

    EGLConfig *cfgv = &sys->cfgv;

    EGLint cfgc;
    if (eglChooseConfig(vctx->display, conf_attr, cfgv, 1, &cfgc) != EGL_TRUE
            || cfgc == 0)
    {
        msg_Err(gl, "cannot choose EGL configuration");
        goto error;
    }

    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)
    {
        msg_Err(gl, "cannot bind EGL OPENGL ES API");
        goto error;
    }

    const EGLint ctx_attr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLContext ctx
        = eglCreateContext(vctx->display, sys->cfgv, EGL_NO_CONTEXT, ctx_attr);
    if (ctx == EGL_NO_CONTEXT)
    {
        msg_Err(gl, "cannot create EGL context");
        goto error;
    }

    vctx->context = ctx;

    return VLC_SUCCESS;

error:
    eglTerminate(vctx->display);
    return VLC_EGENERIC;
}

static picture_t *SwapOffscreen(vlc_gl_t *gl)
{
    struct surfacetexture_sys *sys = gl->sys;
    struct video_ctx *vctx = GetVCtx(gl);

    picture_t *current_picture = sys->current_picture;
    assert(current_picture);
    assert(current_picture->context);

    /* Swap currently active surface */
    struct picture_ctx *current_ctx = PRIV(current_picture->context);

    eglSwapBuffers(vctx->display, current_ctx->surface);

    /* Change currently active surface to the next available one */
    picture_t *picture = picture_pool_Wait(vctx->pool);
    assert(picture->context);
    struct picture_ctx *ctx = PRIV(current_picture->context);

    eglMakeCurrent(vctx->display, ctx->surface, ctx->surface, vctx->context);
    sys->current_picture = picture;

    return current_picture;
}

static void Close(vlc_gl_t *gl)
{
    struct surfacetexture_sys *sys = gl->sys;

    picture_Release(sys->current_picture);
    vlc_video_context_Release(gl->offscreen_vctx_out);
    free(sys);
}

static void DestroyVideoContext(void *priv)
{
    struct video_ctx *vctx = priv;
    for (size_t i = 0; i < ARRAY_SIZE(vctx->pictures); ++i)
    {
        picture_t *picture = vctx->pictures[i];
        struct picture_ctx *ctx = PRIV(picture->context);

        eglDestroySurface(vctx->display, ctx->surface);
        vlc_asurfacetexture_Delete(ctx->texture);

        /* Delete context without calling the picture destructors */
        free(ctx);
        picture->context = NULL;
    }

    /* Picture pool will release the pictures too */
    picture_pool_Release(vctx->pool);

    /* Must be not be called from Close(), otherwise it might cause deadlocks
     * on eglDestroySurface() */
    eglDestroyContext(vctx->display, vctx->context);

    eglTerminate(vctx->display);
}

static struct vlc_asurfacetexture *
PictureContextGetTexture(picture_context_t *context)
{
    assert(context);

    struct picture_ctx *ctx = PRIV(context);
    return ctx->texture;
}

static int InitPicturePool(vlc_gl_t *gl)
{
    struct surfacetexture_sys *sys = gl->sys;
    struct video_ctx *vctx = GetVCtx(gl);

    size_t i;
    for (i = 0; i < ARRAY_SIZE(vctx->pictures); ++i)
    {
        picture_t *pic = picture_NewFromFormat(&sys->fmt_out);
        if (!pic)
            goto error;

        pic->context = CreatePictureContext(gl);
        if (!pic->context)
        {
            picture_Release(pic);
            goto error;
        }

        vctx->pictures[i] = pic;
    }

    vctx->pool = picture_pool_New(ARRAY_SIZE(vctx->pictures), vctx->pictures);
    if (!vctx->pool)
        goto error;

    return VLC_SUCCESS;

error:
    while (i--)
        picture_Release(vctx->pictures[i]);
    return VLC_EGENERIC;
}

static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    if (gl->device == NULL || gl->device->type != VLC_DECODER_DEVICE_AWINDOW)
    {
        msg_Err(gl, "Wrong decoder device");
        return VLC_EGENERIC;
    }

    struct surfacetexture_sys *sys = malloc(sizeof *sys);
    if (sys == NULL)
        return VLC_ENOMEM;

    sys->fmt_out.i_visible_width
        = sys->fmt_out.i_width
        = width;
    sys->fmt_out.i_visible_height
        = sys->fmt_out.i_height
        = height;

    sys->fmt_out.i_chroma
        = gl->offscreen_chroma_out
        = VLC_CODEC_ANDROID_OPAQUE;
    gl->sys = sys;

    static const struct vlc_video_context_operations ops = {
        .destroy = DestroyVideoContext,
    };

    gl->offscreen_vctx_out =
        vlc_video_context_Create(gl->device, VLC_VIDEO_CONTEXT_AWINDOW,
                                 sizeof(struct video_ctx), &ops);
    if (gl->offscreen_vctx_out == NULL)
        goto error1;

    if (InitEGL(gl) != VLC_SUCCESS)
    {
        msg_Err(gl, "Failed to create opengl context\n");
        goto error2;
    }

    static const struct vlc_gl_operations gl_ops =
    {
        .make_current = MakeCurrent,
        .release_current = ReleaseCurrent,
        .swap_offscreen = SwapOffscreen,
        .get_proc_address = GetSymbol,
        .close = Close,
    };
    gl->ops = &gl_ops;

    struct video_ctx *vctx = GetVCtx(gl);

    if (InitPicturePool(gl) != VLC_SUCCESS)
        goto error3;

    sys->current_picture = picture_pool_Get(vctx->pool);
    assert(sys->current_picture);
    assert(sys->current_picture->context);
    assert(sys->current_picture->context->vctx);

    vctx->android.texture = NULL;
    vctx->android.render = NULL;
    vctx->android.render_ts = NULL;
    vctx->android.get_texture = PictureContextGetTexture;

    int ret = vlc_gl_MakeCurrent(gl);
    if (ret != VLC_SUCCESS)
        goto error4;

    ret = vlc_gl_api_Init(&sys->api, gl);
    if (ret != VLC_SUCCESS)
    {
        msg_Err(gl, "Failed to initialize gl_api");
        vlc_gl_ReleaseCurrent(gl);
        goto error4;
    }

    struct vlc_gl_extension_vt extension_vt;
    vlc_gl_LoadExtensionFunctions(gl, &extension_vt);

    bool has_image_external =
        vlc_gl_HasExtension(&extension_vt, "GL_OES_EGL_image_external");
    vlc_gl_ReleaseCurrent(gl);

    if (!has_image_external)
    {
        msg_Warn(gl, "GL_OES_EGL_image_external is not available,"
                " disabling egl_surfacetexture.");
        goto error4;
    }

    return VLC_SUCCESS;

error4:
    picture_Release(sys->current_picture);
error3:
    eglTerminate(vctx->display);
error2:
    vlc_video_context_Release(gl->offscreen_vctx_out);
error1:
    free(sys);
    return VLC_EGENERIC;
}

vlc_module_begin()
    set_shortname( N_("egl_surfacetexture") )
    set_description( N_("EGL Android SurfaceTexture offscreen opengl provider") )
    set_callback_opengl_es2_offscreen(Open, 100)

    add_shortcut( "egl_surfacetexture" )
vlc_module_end()
