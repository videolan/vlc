/*****************************************************************************
 * opengl_yadif.c
 *****************************************************************************
 * Copyright (C) 2020 Videolabs, VideoLAN and VLC authors
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

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>

#include "../video_output/opengl/filters.h"
#include "../video_output/opengl/gl_api.h"
#include "../video_output/opengl/gl_common.h"
#include "../video_output/opengl/interop.h"

#define FILTER_CFG_PREFIX "sout-deinterlace-"

struct sys {
    vlc_gl_t *gl;
    struct vlc_gl_api api;
    struct vlc_gl_interop *interop;
    struct vlc_gl_filters *filters;
};

static picture_t *
Filter(filter_t *filter, picture_t *pic)
{
    struct sys *sys = filter->p_sys;

    picture_t *output = NULL;

    int ret = vlc_gl_MakeCurrent(sys->gl);
    if (ret != VLC_SUCCESS)
        goto end;

    ret = vlc_gl_filters_UpdatePicture(sys->filters, pic);
    if (ret != VLC_SUCCESS)
        goto release_current;

    ret = vlc_gl_filters_Draw(sys->filters);
    if (ret != VLC_SUCCESS)
        goto release_current;

    output = vlc_gl_Swap(sys->gl);
    if (!output)
        goto release_current;

    output->date = pic->date;
    output->b_force = pic->b_force || true;
    output->b_still = pic->b_still;

release_current:
    vlc_gl_ReleaseCurrent(sys->gl);
end:
    picture_Release(pic);
    return output;
}

static void
Flush(filter_t *filter)
{
    struct sys *sys = filter->p_sys;

    vlc_gl_filters_Flush(sys->filters);
}

static int
Open(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *) obj;

    char *mode = var_InheritString(filter, FILTER_CFG_PREFIX "mode");
    bool expected_mode = !mode
                      || !strcmp(mode, "auto")
                      || !strcmp(mode, "gl_yadif");
    free(mode);
    if (!expected_mode)
        return VLC_EGENERIC;

    struct sys *sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    unsigned width
        = filter->fmt_out.video.i_visible_width
        = filter->fmt_in.video.i_visible_width;

    unsigned height
        = filter->fmt_out.video.i_visible_height
        = filter->fmt_in.video.i_visible_height;

#ifdef USE_OPENGL_ES2
# define VLCGLAPI VLC_OPENGL_ES2
#else
# define VLCGLAPI VLC_OPENGL
#endif
    vlc_fourcc_t chroma = VLC_CODEC_RGBA; /* unused by CreateOffscreen for now */
    sys->gl = vlc_gl_CreateOffscreen(obj, chroma, width, height, VLCGLAPI, NULL);
    if (!sys->gl)
    {
        msg_Err(obj, "Failed to create opengl context");
        goto free_sys;
    }

    if (vlc_gl_MakeCurrent (sys->gl) != VLC_SUCCESS)
    {
        msg_Err(obj, "Failed to gl make current");
        assert(false);
        goto delete_gl;
    }

    int ret = vlc_gl_api_Init(&sys->api, sys->gl);
    if (ret != VLC_SUCCESS)
    {
        msg_Err(obj, "Failed to initialize gl_api");
        goto release_current;
    }

    sys->interop = vlc_gl_interop_New(sys->gl, &sys->api, NULL,
                                      &filter->fmt_in.video);
    if (!sys->interop)
    {
        msg_Err(obj, "Could not create interop");
        goto release_current;
    }

    sys->filters = vlc_gl_filters_New(sys->gl, &sys->api, sys->interop);
    if (!sys->filters)
    {
        msg_Err(obj, "Could not create filters");
        goto delete_interop;
    }

    /* The OpenGL filter will do the real job, this file is just a filter_t
     * wrapper */
    struct vlc_gl_filter *glfilter =
        vlc_gl_filters_Append(sys->filters, "yadif", NULL);
    if (!glfilter)
    {
        msg_Err(obj, "Could not create OpenGL yadif filter");
        goto delete_filters;
    }

    ret = vlc_gl_filters_InitFramebuffers(sys->filters);
    if (ret != VLC_SUCCESS)
    {
        msg_Err(sys->gl, "Could not init filters framebuffers");
        goto delete_filters;
    }

    vlc_gl_filters_SetViewport(sys->filters, 0, 0, width, height);

    vlc_gl_ReleaseCurrent(sys->gl);

    filter->fmt_out.video.orientation = ORIENT_VFLIPPED;
    filter->fmt_out.video.i_chroma = filter->fmt_out.i_codec = VLC_CODEC_RGBA;
    filter->vctx_out = sys->gl->vctx_out;

    filter->pf_video_filter = Filter;
    filter->pf_flush = Flush;
    filter->p_sys = sys;

    return VLC_SUCCESS;

delete_filters:
    vlc_gl_filters_Delete(sys->filters);
delete_interop:
    vlc_gl_interop_Delete(sys->interop);
release_current:
    vlc_gl_ReleaseCurrent(sys->gl);
delete_gl:
    vlc_object_delete(sys->gl);
free_sys:
    free(sys);

    return VLC_EGENERIC;
}

static void
Close(vlc_object_t *obj)
{
    filter_t *filter = (filter_t *) obj;
    struct sys *sys = filter->p_sys;

    vlc_gl_MakeCurrent(sys->gl);
    vlc_gl_filters_Delete(sys->filters);
    vlc_gl_interop_Delete(sys->interop);
    vlc_gl_ReleaseCurrent(sys->gl);

    vlc_gl_Release(sys->gl);

    free(sys);
}

vlc_module_begin()
    set_shortname( N_("gl_yadif") )
    set_description(N_("OpenGL yadif filter"))
    set_capability("video filter", 1) /* priority greater than deinterlace.c */
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)
    set_callbacks(Open, Close)
    add_shortcut("deinterlace")
    add_shortcut("gl_yadif")
vlc_module_end()
