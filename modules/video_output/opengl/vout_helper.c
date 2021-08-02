/*****************************************************************************
 * vout_helper.c: OpenGL and OpenGL ES output common code
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
 * Copyright (C) 2009, 2011 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *          Rémi Denis-Courmont
 *          Adrien Maglo <magsoft at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont at videolan dot org>
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
# include "config.h"
#endif

#include <assert.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_list.h>
#include <vlc_subpicture.h>
#include <vlc_opengl.h>
#include <vlc_modules.h>
#include <vlc_vout.h>
#include <vlc_viewpoint.h>

#include "filters.h"
#include "gl_api.h"
#include "gl_util.h"
#include "vout_helper.h"
#include "internal.h"
#include "renderer.h"
#include "sampler.h"
#include "sampler_priv.h"
#include "sub_renderer.h"

struct vout_display_opengl_t {

    vlc_gl_t   *gl;
    struct vlc_gl_api api;

    struct vlc_gl_interop *interop;
    struct vlc_gl_renderer *renderer; /**< weak reference */

    struct vlc_gl_filters *filters;

    struct vlc_gl_interop *sub_interop;
    struct vlc_gl_sub_renderer *sub_renderer;

    /*
     * Some states are applied on subcomponents (like the renderer). When these
     * components are recreated, the state must be re-applied on the new
     * instance.
     */
    struct {
        vlc_viewpoint_t viewpoint;
        float window_aspect_ratio;
        struct {
            int x;
            int y;
            unsigned width;
            unsigned height;
        } viewport;
    } memory;
};

static const vlc_fourcc_t gl_subpicture_chromas[] = {
    VLC_CODEC_RGBA,
    0
};

static void
ResizeFormatToGLMaxTexSize(video_format_t *fmt, unsigned int max_tex_size)
{
    if (fmt->i_width > fmt->i_height)
    {
        unsigned int const  vis_w = fmt->i_visible_width;
        unsigned int const  vis_h = fmt->i_visible_height;
        unsigned int const  nw_w = max_tex_size;
        unsigned int const  nw_vis_w = nw_w * vis_w / fmt->i_width;

        fmt->i_height = nw_w * fmt->i_height / fmt->i_width;
        fmt->i_width = nw_w;
        fmt->i_visible_height = nw_vis_w * vis_h / vis_w;
        fmt->i_visible_width = nw_vis_w;
    }
    else
    {
        unsigned int const  vis_w = fmt->i_visible_width;
        unsigned int const  vis_h = fmt->i_visible_height;
        unsigned int const  nw_h = max_tex_size;
        unsigned int const  nw_vis_h = nw_h * vis_h / fmt->i_height;

        fmt->i_width = nw_h * fmt->i_width / fmt->i_height;
        fmt->i_height = nw_h;
        fmt->i_visible_width = nw_vis_h * vis_w / vis_h;
        fmt->i_visible_height = nw_vis_h;
    }
}

static struct vlc_gl_interop *
CreateInterop(vlc_gl_t *gl, const struct vlc_gl_api *api,
              vlc_video_context *context, video_format_t *fmt)
{
    const opengl_vtable_t *vt = &api->vt;

    /* Resize the format if it is greater than the maximum texture size
     * supported by the hardware */
    GLint max_tex_size;
    vt->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);

    if ((GLint)fmt->i_width > max_tex_size ||
        (GLint)fmt->i_height > max_tex_size)
        ResizeFormatToGLMaxTexSize(fmt, max_tex_size);

    struct vlc_gl_interop *interop = vlc_gl_interop_New(gl, api, context, fmt);
    if (!interop)
    {
        msg_Err(gl, "Could not create interop");
        return NULL;
    }

    return interop;
}

static struct vlc_gl_filters *
CreateFilters(vlc_gl_t *gl, const struct vlc_gl_api *api,
              struct vlc_gl_interop *interop,
              struct vlc_gl_renderer **out_renderer)
{
    struct vlc_gl_filters *filters = vlc_gl_filters_New(gl, api, interop);
    if (!filters)
    {
        msg_Err(gl, "Could not create filters");
        return NULL;
    }

    /* The renderer is the only filter, for now */
    struct vlc_gl_filter *renderer_filter =
        vlc_gl_filters_Append(filters, "renderer", NULL);
    if (!renderer_filter)
    {
        msg_Warn(gl, "Could not create renderer for %4.4s",
                 (const char *) &interop->fmt_out.i_chroma);
        goto error;
    }

    /* The renderer is a special filter: we need its concrete instance to
     * forward SetViewpoint() */
    *out_renderer = renderer_filter->sys;

    int ret = vlc_gl_filters_InitFramebuffers(filters);
    if (ret != VLC_SUCCESS)
    {
        msg_Err(gl, "Could not init filters framebuffers");
        goto error;
    }

    return filters;
error:
    vlc_gl_filters_Delete(filters);
    return NULL;
}

vout_display_opengl_t *vout_display_opengl_New(video_format_t *fmt,
                                               const vlc_fourcc_t **subpicture_chromas,
                                               vlc_gl_t *gl,
                                               const vlc_viewpoint_t *viewpoint,
                                               vlc_video_context *context)
{
    vout_display_opengl_t *vgl = calloc(1, sizeof(*vgl));
    if (!vgl)
        return NULL;

    vgl->gl = gl;

    int ret = vlc_gl_api_Init(&vgl->api, gl);
    if (ret != VLC_SUCCESS)
        goto free_vgl;

    const struct vlc_gl_api *api = &vgl->api;
    const opengl_vtable_t *vt = &api->vt;

#if !defined(USE_OPENGL_ES2)
    const unsigned char *ogl_version = vt->GetString(GL_VERSION);
    bool supports_shaders = strverscmp((const char *)ogl_version, "2.0") >= 0;
    if (!supports_shaders)
    {
        msg_Err(gl, "shaders not supported, bailing out");
        goto free_vgl;
    }
#endif

    /* Resize the format if it is greater than the maximum texture size
     * supported by the hardware */
    GLint       max_tex_size;
    vt->GetIntegerv(GL_MAX_TEXTURE_SIZE, &max_tex_size);

    if ((GLint)fmt->i_width > max_tex_size ||
        (GLint)fmt->i_height > max_tex_size)
        ResizeFormatToGLMaxTexSize(fmt, max_tex_size);

    vgl->interop = CreateInterop(gl, api, context, fmt);
    if (!vgl->interop)
        goto free_vgl;

    vgl->filters = CreateFilters(gl, api, vgl->interop, &vgl->renderer);
    if (!vgl->filters)
        goto delete_interop;

    vgl->sub_interop = vlc_gl_interop_NewForSubpictures(gl, api);
    if (!vgl->sub_interop)
    {
        msg_Err(gl, "Could not create sub interop");
        goto delete_filters;
    }

    vgl->sub_renderer =
        vlc_gl_sub_renderer_New(gl, api, vgl->sub_interop);
    if (!vgl->sub_renderer)
    {
        msg_Err(gl, "Could not create sub renderer");
        goto delete_sub_interop;
    }

    GL_ASSERT_NOERROR(vt);

    if (fmt->projection_mode != PROJECTION_MODE_RECTANGULAR
     && vout_display_opengl_SetViewpoint(vgl, viewpoint) != VLC_SUCCESS)
        goto delete_sub_renderer;

    /* Forward to the core the changes to the input format requested by the
     * interop */
    *fmt = vgl->interop->fmt_in;

    if (subpicture_chromas) {
        *subpicture_chromas = gl_subpicture_chromas;
    }

    GL_ASSERT_NOERROR(vt);
    return vgl;

delete_sub_renderer:
    vlc_gl_sub_renderer_Delete(vgl->sub_renderer);
delete_sub_interop:
    vlc_gl_interop_Delete(vgl->sub_interop);
delete_filters:
    vlc_gl_filters_Delete(vgl->filters);
delete_interop:
    vlc_gl_interop_Delete(vgl->interop);
free_vgl:
    free(vgl);

    return NULL;
}

int vout_display_opengl_UpdateFormat(vout_display_opengl_t *vgl,
                                     video_format_t *fmt,
                                     vlc_video_context *ctx)
{
    /* If the format can't be changed, the state must stay valid to accept the
     * initial format. */

    vlc_gl_t *gl = vgl->gl;
    const struct vlc_gl_api *api = &vgl->api;

    struct vlc_gl_interop *interop = CreateInterop(gl, api, ctx, fmt);
    if (!interop)
        return VLC_EGENERIC;

    struct vlc_gl_renderer *renderer;
    struct vlc_gl_filters *filters = CreateFilters(gl, api, interop, &renderer);
    if (!filters)
    {
        vlc_gl_interop_Delete(interop);
        return VLC_EGENERIC;
    }

    /* re-assign the state to the new filters and renderer instances */
    vlc_gl_filters_SetViewport(filters, vgl->memory.viewport.x,
                                        vgl->memory.viewport.y,
                                        vgl->memory.viewport.width,
                                        vgl->memory.viewport.height);

    vlc_gl_renderer_SetWindowAspectRatio(renderer,
                                         vgl->memory.window_aspect_ratio);

    vlc_gl_renderer_SetViewpoint(renderer, &vgl->memory.viewpoint);

    /* We created everything necessary, it worked, now the old ones could be
     * replaced. */
    vlc_gl_filters_Delete(vgl->filters);
    vlc_gl_interop_Delete(vgl->interop);

    vgl->interop = interop;
    vgl->filters = filters;
    vgl->renderer = renderer;

    return VLC_SUCCESS;
}

void vout_display_opengl_Delete(vout_display_opengl_t *vgl)
{
    const opengl_vtable_t *vt = &vgl->api.vt;

    GL_ASSERT_NOERROR(vt);

    /* */
    vt->Finish();
    vt->Flush();

    vlc_gl_sub_renderer_Delete(vgl->sub_renderer);
    vlc_gl_interop_Delete(vgl->sub_interop);

    vlc_gl_filters_Delete(vgl->filters);
    vlc_gl_interop_Delete(vgl->interop);

    GL_ASSERT_NOERROR(vt);

    free(vgl);
}

int vout_display_opengl_SetViewpoint(vout_display_opengl_t *vgl,
                                     const vlc_viewpoint_t *p_vp)
{
    vgl->memory.viewpoint = *p_vp;
    return vlc_gl_renderer_SetViewpoint(vgl->renderer, p_vp);
}

void vout_display_opengl_SetWindowAspectRatio(vout_display_opengl_t *vgl,
                                              float f_sar)
{
    vgl->memory.window_aspect_ratio = f_sar;
    vlc_gl_renderer_SetWindowAspectRatio(vgl->renderer, f_sar);
}

void vout_display_opengl_Viewport(vout_display_opengl_t *vgl, int x, int y,
                                  unsigned width, unsigned height)
{
    vgl->memory.viewport.x = x;
    vgl->memory.viewport.y = y;
    vgl->memory.viewport.width = width;
    vgl->memory.viewport.height = height;
    vlc_gl_filters_SetViewport(vgl->filters, x, y, width, height);
}

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture, subpicture_t *subpicture)
{
    GL_ASSERT_NOERROR(&vgl->api.vt);

    int ret = vlc_gl_filters_UpdatePicture(vgl->filters, picture);
    if (ret != VLC_SUCCESS)
        return ret;

    ret = vlc_gl_sub_renderer_Prepare(vgl->sub_renderer, subpicture);
    GL_ASSERT_NOERROR(&vgl->api.vt);
    return ret;
}
int vout_display_opengl_Display(vout_display_opengl_t *vgl)
{
    GL_ASSERT_NOERROR(&vgl->api.vt);

    /* Why drawing here and not in Render()? Because this way, the
       OpenGL providers can call vout_display_opengl_Display to force redraw.
       Currently, the OS X provider uses it to get a smooth window resizing */

    int ret = vlc_gl_filters_Draw(vgl->filters, NULL);
    if (ret != VLC_SUCCESS)
        return ret;

    ret = vlc_gl_sub_renderer_Draw(vgl->sub_renderer);
    if (ret != VLC_SUCCESS)
        return ret;

    GL_ASSERT_NOERROR(&vgl->api.vt);

    return VLC_SUCCESS;
}
