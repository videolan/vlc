/*****************************************************************************
 * surfacetexture_gl: OpenGL offscreen provider with SurfaceTexture
 *****************************************************************************
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
#include <vlc_atomic.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "../video_output/android/utils.h"
#include "../opengl/gl_api.h"

#define BUFFER_COUNT 3

struct surfacetexture_picture_context
{
    struct picture_context_t context;
    EGLSurface surface;

    struct vlc_asurfacetexture *texture;
};

struct vlc_gl_surfacetexture
{
    struct vlc_asurfacetexture *surfacetexture;
    android_video_context_t *avctx;

    video_format_t          fmt_out;

    size_t                  current_flip;

    struct vlc_gl_api api;

    EGLDisplay display;
    EGLContext context;
    EGLConfig cfgv;

    PFNEGLCREATEIMAGEKHRPROC    eglCreateImageKHR;
    PFNEGLDESTROYIMAGEKHRPROC   eglDestroyImageKHR;

    picture_pool_t *pool;
    picture_t *pictures[BUFFER_COUNT];
    size_t pictures_count;

    picture_t *current_picture;

};

static int MakeCurrent (vlc_gl_t *gl)
{
    struct vlc_gl_surfacetexture *sys = gl->sys;

    /* We must always have a surface mapped. */
    assert(sys->current_picture);
    assert(sys->current_picture->context);

    struct surfacetexture_picture_context *context =
        container_of(sys->current_picture->context,
                     struct surfacetexture_picture_context,
                     context);

    if (eglMakeCurrent (sys->display, context->surface, context->surface,
                        sys->context) != EGL_TRUE)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static void ReleaseCurrent (vlc_gl_t *gl)
{
    struct vlc_gl_surfacetexture *sys = gl->sys;

    eglMakeCurrent (sys->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                    EGL_NO_CONTEXT);
}

static void *GetSymbol(vlc_gl_t *gl, const char *procname)
{
    (void) gl;
    return (void *)eglGetProcAddress (procname);
}

static const char *QueryString(vlc_gl_t *gl, int32_t name)
{
    struct vlc_gl_surfacetexture *sys = gl->sys;

    return eglQueryString(sys->display, name);
}

static void *CreateImageKHR(vlc_gl_t *gl, unsigned target, void *buffer,
                            const int32_t *attrib_list)
{
    struct vlc_gl_surfacetexture *sys = gl->sys;

    return sys->eglCreateImageKHR(sys->display, NULL, target, buffer,
                                  attrib_list);
}

static bool DestroyImageKHR(vlc_gl_t *gl, void *image)
{
    struct vlc_gl_surfacetexture *sys = gl->sys;

    return sys->eglDestroyImageKHR(sys->display, image);
}

static picture_context_t *CopyPictureContext(picture_context_t *input)
{
    vlc_video_context_Hold(input->vctx);
    return input;
}

static void DestroyPictureContext(picture_context_t *input)
{
    /* video context is already released by picture_t. */
}

static picture_context_t *CreatePictureContext(vlc_gl_t *gl)
{
    struct vlc_gl_surfacetexture *sys = gl->sys;
    struct surfacetexture_picture_context *context = malloc(sizeof *context);

    context->texture = vlc_asurfacetexture_New(gl->device->opaque, true);

    struct ANativeWindow *window = context->texture->window;
    native_window_api_t *api = AWindowHandler_getANativeWindowAPI(gl->device->opaque);
    api->setBuffersGeometry(window, sys->fmt_out.i_width, sys->fmt_out.i_height,
                            AHARDWAREBUFFER_FORMAT_BLOB);

    /* Create a drawing surface */
    context->surface = eglCreateWindowSurface(sys->display, sys->cfgv, window, NULL);
    if (context->surface == EGL_NO_SURFACE)
    {
        msg_Err (gl, "cannot create EGL window surface");
        goto error;
    }

    context->context.vctx = vlc_video_context_Hold(gl->vctx_out);
    context->context.copy = CopyPictureContext;
    context->context.destroy = DestroyPictureContext;

    return &context->context;

error:
    free(context);
    return NULL;
}

static int InitEGL(vlc_gl_t *gl)
{
    struct vlc_gl_surfacetexture *sys = gl->sys;

    sys->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (sys->display == EGL_NO_DISPLAY)
        return VLC_EGENERIC;

    /* Initialize EGL display */
    EGLint major, minor;
    if (eglInitialize(sys->display, &major, &minor) != EGL_TRUE)
        goto error;
    msg_Dbg(gl, "EGL version %s by %s, API %s",
            eglQueryString(sys->display, EGL_VERSION),
            eglQueryString(sys->display, EGL_VENDOR),
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
    EGLint cfgc;

    if (eglChooseConfig(sys->display, conf_attr, &sys->cfgv, 1, &cfgc) != EGL_TRUE
     || cfgc == 0)
    {
        msg_Err (gl, "cannot choose EGL configuration");
        goto error;
    }

    if (eglBindAPI (EGL_OPENGL_ES_API) != EGL_TRUE)
    {
        msg_Err (gl, "cannot bind EGL OPENGL ES API");
        goto error;
    }

    const EGLint ctx_attr[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    EGLContext ctx
        = sys->context
        = eglCreateContext(sys->display, sys->cfgv, EGL_NO_CONTEXT, ctx_attr);

    if (ctx == EGL_NO_CONTEXT)
    {
        msg_Err (gl, "cannot create EGL context");
        goto error;
    }

    return VLC_SUCCESS;

error:
    return VLC_EGENERIC;
}

static picture_t *Swap(vlc_gl_t *gl)
{
    struct vlc_gl_surfacetexture *sys = gl->sys;

    picture_t *current_picture = sys->current_picture;
    assert(current_picture);
    assert(current_picture->context);

    /* Swap currently active surface */
    struct surfacetexture_picture_context *current_context =
        container_of(current_picture->context, struct surfacetexture_picture_context, context);

    eglSwapBuffers(sys->display, current_context->surface);

    /* Change currently active surface to the next available one */
    picture_t *picture = picture_pool_Wait(sys->pool);
    assert(picture->context);
    struct surfacetexture_picture_context *context =
        container_of(picture->context, struct surfacetexture_picture_context, context);

    eglMakeCurrent(sys->display, context->surface, context->surface, sys->context);
    sys->current_picture = picture;

    return current_picture;
}

static void Close( vlc_gl_t *gl )
{
    struct vlc_gl_surfacetexture *sys = gl->sys;

    vlc_video_context_Release(gl->vctx_out);
    free(sys);
}

static bool PictureContextRenderPic(struct picture_context_t *context)
{
    struct surfacetexture_picture_context *apctx =
        container_of(context, struct surfacetexture_picture_context, context);

    // TODO check whether it changed and if we need to call Update/ReleaseTexture
    //SurfaceTexture_Rele
    return true;
}

static void CleanFromVideoContext(void *priv)
{
    android_video_context_t *avctx = priv;
}

static struct vlc_asurfacetexture *
PictureContextGetTexture(picture_context_t *context)
{
    assert(context);

    struct surfacetexture_picture_context *handle =
        container_of(context, struct surfacetexture_picture_context, context);

    return handle->texture;
}

static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    if (gl->device == NULL || gl->device->type != VLC_DECODER_DEVICE_AWINDOW)
    {
        msg_Err(gl, "Wrong decoder device");
        return VLC_EGENERIC;
    }

    struct vlc_gl_surfacetexture *sys = malloc(sizeof *sys);
    if (sys == NULL)
        return VLC_ENOMEM;

    sys->fmt_out.i_visible_width
        = sys->fmt_out.i_width
        = width;
    sys->fmt_out.i_visible_height
        = sys->fmt_out.i_height
        = height;

    sys->fmt_out.i_chroma
        = gl->chroma_out
        = VLC_CODEC_ANDROID_OPAQUE;
    gl->vctx_out = NULL;

    gl->sys = sys;

    if (InitEGL(gl) != VLC_SUCCESS)
    {
        msg_Err(gl, "Failed to create opengl context\n");
        goto error;
    }

    gl->ext = VLC_GL_EXT_EGL;
    gl->make_current = MakeCurrent;
    gl->release_current = ReleaseCurrent;
    gl->resize = NULL;
    gl->swap = Swap;
    gl->get_proc_address = GetSymbol;
    gl->destroy = Close;
    gl->egl.queryString = QueryString;

    static const struct vlc_video_context_operations ops =
    {
        .destroy = CleanFromVideoContext,
    };

    gl->vctx_out =
        vlc_video_context_Create(gl->device, VLC_VIDEO_CONTEXT_AWINDOW,
                                 sizeof(android_video_context_t), &ops);

    sys->eglCreateImageKHR = (void *)eglGetProcAddress("eglCreateImageKHR");
    sys->eglDestroyImageKHR = (void *)eglGetProcAddress("eglDestroyImageKHR");
    if (sys->eglCreateImageKHR != NULL && sys->eglDestroyImageKHR != NULL)
    {
        gl->egl.createImageKHR = CreateImageKHR;
        gl->egl.destroyImageKHR = DestroyImageKHR;
    }

    for (sys->pictures_count=0;
         sys->pictures_count<BUFFER_COUNT;
         ++sys->pictures_count)
    {
        sys->pictures[sys->pictures_count] =
            picture_NewFromFormat(&sys->fmt_out);
        if (sys->pictures[sys->pictures_count] == NULL)
            goto error;

        sys->pictures[sys->pictures_count]->context = CreatePictureContext(gl);
        if (sys->pictures[sys->pictures_count]->context == NULL)
            goto error;
    }

    sys->pool = picture_pool_New(BUFFER_COUNT, sys->pictures);
    if (sys->pool == NULL)
        goto error;

    sys->current_picture = picture_pool_Get(sys->pool);
    assert(sys->current_picture);
    assert(sys->current_picture->context == sys->pictures[0]->context);
    assert(sys->current_picture->context->vctx);

    android_video_context_t *avctx =
        vlc_video_context_GetPrivate(gl->vctx_out, VLC_VIDEO_CONTEXT_AWINDOW);
    avctx->texture = NULL;
    avctx->render = PictureContextRenderPic;
    avctx->render_ts = NULL; // TODO
    avctx->get_texture = PictureContextGetTexture;

    return VLC_SUCCESS;

error:
    if (sys->surfacetexture != NULL)
    {
        // TODO
    }

    free(sys);
    return VLC_EGENERIC;
}

vlc_module_begin()
    set_shortname( N_("egl_surfacetexture") )
    set_description( N_("EGL Android SurfaceTexture offscreen opengl provider") )
    set_capability( "opengl es2 offscreen", 100)

    add_shortcut( "egl_surfacetexture" )
    set_callback( Open )
vlc_module_end()
