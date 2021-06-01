/*****************************************************************************
 * opengl.c: OpenGL filter
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
#include <vlc_vout_window.h>
#include <vlc_vout_display.h>
#include <vlc_atomic.h>
#include "../video_output/opengl/vout_helper.h"
#include "../video_output/opengl/filters.h"
#include "../video_output/opengl/gl_api.h"
#include "../video_output/opengl/gl_common.h"
#include "../video_output/opengl/interop.h"

#define OPENGL_CFG_PREFIX "opengl-"
static const char *const opengl_options[] = { "filter", NULL };

typedef struct
{
    vlc_gl_t *gl;
    struct vlc_gl_filters *filters;
    struct vlc_gl_interop *interop;
    struct vlc_gl_api api;
} filter_sys_t;

static picture_t *Filter(filter_t *filter, picture_t *input)
{
    filter_sys_t *sys = filter->p_sys;
    picture_t *output = NULL;
    int ret;

    if (vlc_gl_MakeCurrent(sys->gl) != VLC_SUCCESS)
    {
        if (input)
            picture_Release(input);
        return NULL;
    }

    if (input)
    {
        ret = vlc_gl_filters_UpdatePicture(sys->filters, input);
        if (ret != VLC_SUCCESS)
            goto end;
    }

    output = vlc_gl_SwapOffscreen(sys->gl);
    vlc_gl_ReleaseCurrent(sys->gl);
    if (output == NULL)
        goto end;
    if (!vlc_gl_filters_WillUpdate(sys->filters, input != NULL))
        goto end;

    /* Will store the information of the output frame */
    struct vlc_gl_input_meta meta;

    ret = vlc_gl_filters_Draw(sys->filters, &meta);
    if (ret != VLC_SUCCESS)
        goto end;

    output = vlc_gl_SwapOffscreen(sys->gl);
    if (output == NULL)
        goto end;

    output->date = meta.pts;

    /* TODO
    output->b_force = input->b_force;
    output->b_still = input->b_still;
    */

    output->format.i_frame_rate =
        filter->fmt_out.video.i_frame_rate;
    output->format.i_frame_rate_base =
        filter->fmt_out.video.i_frame_rate_base;
end:
    vlc_gl_ReleaseCurrent(sys->gl);

    if (input)
        picture_Release(input);
    return output;
}

static int
AppendVFlip(struct vlc_gl_filters *filters)
{
    config_chain_t *cfg = NULL;
    char *name;
    char *leftover = config_ChainCreate(&name, &cfg, "draw{vflip}");
    free(leftover);
    free(name);

    /* The OpenGL filter will do the real job, this file is just a filter_t
     * wrapper */
    struct vlc_gl_filter *glfilter =
        vlc_gl_filters_Append(filters, "draw", cfg);
    config_ChainDestroy(cfg);
    if (!glfilter)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int
LoadFilters(filter_sys_t *sys, const char *glfilters_config)
{
    struct vlc_gl_filters *filters = sys->filters;
    assert(glfilters_config);

    const char *string = glfilters_config;
    char *next_module = NULL;
    do
    {
        char *name;
        config_chain_t *config = NULL;
        char *leftover = config_ChainCreate(&name, &config, string);

        free(next_module);
        next_module = leftover;
        string = next_module; /* const view of next_module */

        if (name)
        {
            struct vlc_gl_filter *filter =
                vlc_gl_filters_Append(filters, name, config);
            config_ChainDestroy(config);
            if (!filter)
            {
                msg_Err(sys->gl, "Could not load GL filter: %s", name);
                free(name);
                return VLC_EGENERIC;
            }

            free(name);
        }
    } while (string);

    return VLC_SUCCESS;
}

static void
Flush(filter_t *filter)
{
    filter_sys_t *sys = filter->p_sys;
    vlc_gl_filters_Flush(sys->filters);
}

static void Close( filter_t *filter )
{
    filter_sys_t *sys = filter->p_sys;

    if (sys != NULL)
    {
        vlc_gl_MakeCurrent(sys->gl);
        vlc_gl_filters_Delete(sys->filters);
        vlc_gl_interop_Delete(sys->interop);
        vlc_gl_ReleaseCurrent(sys->gl);

        vlc_gl_Release(sys->gl);
        free(sys);
    }
}

static int Open( vlc_object_t *obj )
{
    filter_t *filter = (filter_t *)obj;

    filter_sys_t *sys
        = filter->p_sys
        = malloc(sizeof *sys);
    if (sys == NULL)
        return VLC_ENOMEM;

    unsigned width = filter->fmt_out.video.i_visible_width;
    unsigned height = filter->fmt_out.video.i_visible_height;

    // TODO: other than BGRA format ?
#ifdef USE_OPENGL_ES2
# define VLCGLAPI VLC_OPENGL_ES2
#else
# define VLCGLAPI VLC_OPENGL
#endif

    struct vlc_decoder_device *device = filter_HoldDecoderDevice(filter);
    sys->gl = vlc_gl_CreateOffscreen(obj, device, width, height, VLCGLAPI,
                                     NULL);

    /* The vlc_gl_t instance must have hold the device if it needs it. */
    if (device)
        vlc_decoder_device_Release(device);

    if (sys->gl == NULL)
    {
        msg_Err(obj, "Failed to create opengl context\n");
        goto gl_create_failure;
    }

    if (vlc_gl_MakeCurrent (sys->gl) != VLC_SUCCESS)
    {
        msg_Err(obj, "Failed to gl make current");
        assert(false);
        goto make_current_failure;
    }

    struct vlc_gl_api *api = &sys->api;
    if (vlc_gl_api_Init(api, sys->gl) != VLC_SUCCESS)
    {
        msg_Err(obj, "Failed to init vlc_gl_api");
        goto gl_api_failure;
    }

    sys->interop = vlc_gl_interop_New(sys->gl, api, filter->vctx_in,
                                      &filter->fmt_in.video);
    if (!sys->interop)
    {
        msg_Err(obj, "Could not create interop");
        goto gl_interop_failure;
    }

    config_ChainParse(filter, OPENGL_CFG_PREFIX, opengl_options, filter->p_cfg);

    char *glfilters_config =
        var_InheritString(filter, OPENGL_CFG_PREFIX "filter");
    if (!glfilters_config)
    {
        msg_Err(obj, "No filters requested");
        goto filter_config_failure;
    }


    sys->filters = vlc_gl_filters_New(sys->gl, api, sys->interop);
    if (!sys->filters)
    {
        msg_Err(obj, "Could not create filters");
        free(glfilters_config);
        goto filters_new_failure;
    }

    int ret;

    ret = LoadFilters(sys, glfilters_config);
    if (ret != VLC_SUCCESS)
    {
        msg_Err(obj, "Could not load filters: %s", glfilters_config);
        free(glfilters_config);
        goto filters_load_failure;
    }
    free(glfilters_config);

    if (sys->gl->offscreen_vflip)
    {
        /* OpenGL renders upside-down, add a filter to get the pixels in the
         * normal orientation */
        ret = AppendVFlip(sys->filters);
        if (ret != VLC_SUCCESS)
            return VLC_EGENERIC;
    }

    ret = vlc_gl_filters_InitFramebuffers(sys->filters);
    if (ret != VLC_SUCCESS)
    {
        msg_Err(obj, "Could not init filters framebuffers");
        goto init_framebuffer_failure;
    }

    vlc_gl_filters_SetViewport(sys->filters, 0, 0, filter->fmt_out.video.i_visible_width, filter->fmt_out.video.i_visible_height);

    vlc_gl_ReleaseCurrent(sys->gl);

    static const struct vlc_filter_operations ops = {
        .filter_video = Filter,
        .flush = Flush,
        .close = Close,
    };
    filter->ops = &ops;
    filter->fmt_out.video.orientation = ORIENT_NORMAL;

    filter->fmt_out.video.i_chroma
        = filter->fmt_out.i_codec
        = sys->gl->offscreen_chroma_out;

    filter->vctx_out = sys->gl->offscreen_vctx_out;
    filter->fmt_out.video.i_frame_rate =
        filter->fmt_in.video.i_frame_rate;

    filter->fmt_out.video.i_frame_rate_base =
        filter->fmt_in.video.i_frame_rate_base;

    assert(filter->fmt_out.video.i_frame_rate_base != 0);
    return VLC_SUCCESS;

init_framebuffer_failure:
filters_load_failure:
    vlc_gl_filters_Delete(sys->filters);

filters_new_failure:
filter_config_failure:
    vlc_gl_interop_Delete(sys->interop);

gl_interop_failure:
gl_api_failure:
    vlc_gl_ReleaseCurrent(sys->gl);

make_current_failure:
    vlc_gl_Release(sys->gl);

gl_create_failure:
    free(sys);

    return VLC_EGENERIC;
}

vlc_module_begin()
    set_shortname( N_("opengl") )
    set_description( N_("Opengl filter executor") )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )
    set_capability( "video filter", 0 )
    add_shortcut( "opengl" )
    set_callback( Open )
    add_module_list( "opengl-filter", "opengl filter", NULL,
                     "opengl filter", "List of OpenGL filters to execute" )
vlc_module_end()
