/*****************************************************************************
 * pl_scale.c
 *****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
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

#include "limits.h"

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_opengl.h>
#include <vlc_filter.h>

#include <libplacebo/context.h>
#include <libplacebo/gpu.h>
#include <libplacebo/opengl.h>
#include <libplacebo/renderer.h>

#include "video_output/opengl/filter.h"
#include "video_output/opengl/gl_api.h"
#include "video_output/opengl/gl_common.h"
#include "video_output/opengl/gl_scale.h"
#include "video_output/opengl/gl_util.h"
#include "video_output/opengl/sampler.h"
#include "video_output/libplacebo/utils.h"

// Without this commit, libplacebo as used by this filter makes VLC
// assert/crash by closing file descriptors:
// https://github.com/haasn/libplacebo/commit/39fc39d31d65968709b4a05c571a0d85c918058d
static_assert(PL_API_VER >= 167, "pl_scale requires libplacebo >= 4.167");

#define CFG_PREFIX "plscale-"

static const char *const filter_options[] = {
    "upscaler", "downscaler", NULL,
};

struct sys
{
    GLuint id;
    GLuint vbo;

    pl_log pl_log;
    pl_opengl pl_opengl;
    pl_renderer pl_renderer;

    /* Cached representation of pl_frame to wrap the raw textures */
    struct pl_frame frame_in;
    struct pl_frame frame_out;
    struct pl_render_params render_params;

    unsigned out_width;
    unsigned out_height;
};

static void
DestroyTextures(pl_gpu gpu, unsigned count, pl_tex textures[])
{
    for (unsigned i = 0; i < count; ++i)
        pl_tex_destroy(gpu, &textures[i]);
}

static int
WrapTextures(pl_gpu gpu, unsigned count, const GLuint textures[],
             const GLsizei tex_widths[], const GLsizei tex_heights[],
             GLenum tex_target, pl_tex out[])
{
    for (unsigned i = 0; i < count; ++i)
    {
        struct pl_opengl_wrap_params opengl_wrap_params = {
            .texture = textures[i],
            .width = tex_widths[i],
            .height = tex_heights[i],
            .target = tex_target,
            .iformat = GL_RGBA8,
        };

        out[i] = pl_opengl_wrap(gpu, &opengl_wrap_params);
        if (!out[i])
        {
            if (i)
                DestroyTextures(gpu, i - 1, out);
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

static pl_tex
WrapFramebuffer(pl_gpu gpu, GLuint framebuffer, unsigned width, unsigned height)
{
    struct pl_opengl_wrap_params opengl_wrap_params = {
        .framebuffer = framebuffer,
        .width = width,
        .height = height,
        .iformat = GL_RGBA8,
    };

    return pl_opengl_wrap(gpu, &opengl_wrap_params);
}

static int
Draw(struct vlc_gl_filter *filter, const struct vlc_gl_picture *pic,
     const struct vlc_gl_input_meta *meta)
{
    (void) meta;

    struct sys *sys = filter->sys;
    const opengl_vtable_t *vt = &filter->api->vt;
    const struct vlc_gl_format *glfmt = filter->glfmt_in;
    pl_gpu gpu = sys->pl_opengl->gpu;
    struct pl_frame *frame_in = &sys->frame_in;
    struct pl_frame *frame_out = &sys->frame_out;
    struct pl_render_params *render_params = &sys->render_params;

    if (pic->mtx_has_changed)
    {
        const float *mtx = pic->mtx;

        /* The direction is either horizontal or vertical, and the two vectors
         * are orthogonal */
        assert((!mtx[1] && !mtx[2]) || (!mtx[0] && !mtx[3]));

        /* Is the video rotated by 90° (or 270°)? */
        bool rotated90 = !mtx[0];

        /*
         * The same rotation+flip orientation may be encoded in different ways
         * in libplacebo. For example, hflip the crop rectangle and use a 90°
         * rotation is equivalent to vflip the crop rectangle and use a 270°
         * rotation.
         *
         * To get a unique solution, limit the rotation to be either 0 or 90,
         * and encode the remaining in the crop rectangle.
         */
        frame_in->rotation = rotated90 ? PL_ROTATION_90 : PL_ROTATION_0;

        /* Apply 90° to the coords if necessary */
        float coords[] = {
            rotated90 ? 1 : 0, 0,
            rotated90 ? 0 : 1, 1,
        };

        vlc_gl_picture_ToTexCoords(pic, 2, coords, coords);

        unsigned w = glfmt->tex_widths[0];
        unsigned h = glfmt->tex_heights[0];
        struct pl_rect2df *r = &frame_in->crop;
        r->x0 = coords[0] * w;
        r->y0 = coords[1] * h;
        r->x1 = coords[2] * w;
        r->y1 = coords[3] * h;
    }

    GLint value;
    vt->GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    GLuint final_draw_framebuffer = value; /* as GLuint */

    pl_tex texs_in[PICTURE_PLANE_MAX];
    int ret = WrapTextures(gpu, glfmt->tex_count, pic->textures,
                           glfmt->tex_widths, glfmt->tex_heights,
                           glfmt->tex_target, texs_in);
    if (ret != VLC_SUCCESS)
        goto end;

    /* Only changes the plane textures from the cached pl_frame */
    for (unsigned i = 0; i < glfmt->tex_count; ++i)
        frame_in->planes[i].texture = texs_in[i];

    pl_tex tex_out = WrapFramebuffer(gpu, final_draw_framebuffer,
                                     sys->out_width, sys->out_height);
    if (!tex_out)
        goto destroy_texs_in;

    frame_out->planes[0].texture = tex_out;

    bool ok = pl_render_image(sys->pl_renderer, frame_in, frame_out,
                              render_params);
    if (!ok)
        ret = VLC_EGENERIC;

    DestroyTextures(gpu, 1, &tex_out);
destroy_texs_in:
    DestroyTextures(gpu, glfmt->tex_count, texs_in);

end:
    vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, final_draw_framebuffer);

    return ret;
}

static int
RequestOutputSize(struct vlc_gl_filter *filter,
                  struct vlc_gl_tex_size *req,
                  struct vlc_gl_tex_size *optimal_in)
{
    struct sys *sys = filter->sys;

    sys->out_width = req->width;
    sys->out_height = req->height;

    /* Do not propagate resizing to previous filters */
    (void) optimal_in;

    return VLC_SUCCESS;
}

static void
Close(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;

    pl_renderer_destroy(&sys->pl_renderer);
    pl_opengl_destroy(&sys->pl_opengl);
    pl_log_destroy(&sys->pl_log);

    free(sys);
}

static vlc_gl_filter_open_fn Open;
static int
Open(struct vlc_gl_filter *filter, const config_chain_t *config,
     const struct vlc_gl_format *glfmt, struct vlc_gl_tex_size *size_out)
{
    (void) config;

    /* By default, do not scale. The dimensions will be modified dynamically by
     * request_output_size(). */
    unsigned width = glfmt->tex_widths[0];
    unsigned height = glfmt->tex_heights[0];

    config_ChainParse(filter, CFG_PREFIX, filter_options, config);
    int upscaler = var_InheritInteger(filter, CFG_PREFIX "upscaler");
    int downscaler = var_InheritInteger(filter, CFG_PREFIX "downscaler");

    if (upscaler < 0 || (size_t) upscaler >= ARRAY_SIZE(scale_values)
            || upscaler == SCALE_CUSTOM)
    {
        msg_Err(filter, "Unsupported upscaler: %d", upscaler);
        return VLC_EGENERIC;
    }

    if (downscaler < 0 || (size_t) downscaler >= ARRAY_SIZE(scale_values)
            || downscaler == SCALE_CUSTOM)
    {
        msg_Err(filter, "Unsupported downscaler: %d", downscaler);
        return VLC_EGENERIC;
    }

    struct sys *sys = filter->sys = calloc(1, sizeof(*sys));
    if (!sys)
        return VLC_EGENERIC;

    sys->pl_log = vlc_placebo_CreateLog(VLC_OBJECT(filter));

    struct pl_opengl_params opengl_params = {
        .debug = true,
    };
    sys->pl_opengl = pl_opengl_create(sys->pl_log, &opengl_params);

    if (!sys->pl_opengl)
        goto error;

    pl_gpu gpu = sys->pl_opengl->gpu;
    sys->pl_renderer = pl_renderer_create(sys->pl_log, gpu);
    if (!sys->pl_renderer)
        goto error;

    sys->frame_in = (struct pl_frame) {
        .num_planes = glfmt->tex_count,
        .repr = vlc_placebo_ColorRepr(&glfmt->fmt),
        .color = vlc_placebo_ColorSpace(&glfmt->fmt),
    };

    /* Initialize frame_in.planes */
    int plane_count =
        vlc_placebo_PlaneComponents(&glfmt->fmt, sys->frame_in.planes);
    if ((unsigned) plane_count != glfmt->tex_count) {
        msg_Err(filter, "Unexpected plane count (%d) != tex count (%u)",
                        plane_count, glfmt->tex_count);
        goto error;
    }

    sys->frame_out = (struct pl_frame) {
        .num_planes = 1,
        .planes = {
            {
                .components = 4,
                .component_mapping = {
                    PL_CHANNEL_R,
                    PL_CHANNEL_G,
                    PL_CHANNEL_B,
                    PL_CHANNEL_A,
                },
            },
        },
    };

    sys->render_params = pl_render_default_params;

    int upscaler_idx = libplacebo_scale_map[upscaler];
    sys->render_params.upscaler = scale_config[upscaler_idx];

    int downscaler_idx = libplacebo_scale_map[downscaler];
    sys->render_params.downscaler = scale_config[downscaler_idx];

    static const struct vlc_gl_filter_ops ops = {
        .draw = Draw,
        .close = Close,
        .request_output_size = RequestOutputSize,
    };
    filter->ops = &ops;

    sys->out_width = size_out->width = width;
    sys->out_height = size_out->height = height;

    return VLC_SUCCESS;

error:
    pl_renderer_destroy(&sys->pl_renderer);
    pl_opengl_destroy(&sys->pl_opengl);
    pl_log_destroy(&sys->pl_log);
    free(sys);

    return VLC_EGENERIC;
}

vlc_module_begin()
    set_shortname("pl_scale")
    set_description("OpenGL scaler")
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_capability("opengl filter", 0)
    set_callback(Open)
    add_shortcut("pl_scale");

#define UPSCALER_TEXT "OpenGL upscaler"
#define UPSCALER_LONGTEXT "Upscaler filter to apply during rendering"
    add_integer(CFG_PREFIX "upscaler", SCALE_BUILTIN, UPSCALER_TEXT, \
                UPSCALER_LONGTEXT) \
        change_integer_list(scale_values, scale_text) \

#define DOWNSCALER_TEXT "OpenGL downscaler"
#define DOWNSCALER_LONGTEXT "Downscaler filter to apply during rendering"
    add_integer(CFG_PREFIX "downscaler", SCALE_BUILTIN, DOWNSCALER_TEXT, \
                DOWNSCALER_LONGTEXT) \
        change_integer_list(scale_values, scale_text) \
vlc_module_end()
