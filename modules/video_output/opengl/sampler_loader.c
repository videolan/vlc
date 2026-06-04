/*****************************************************************************
 * sampler_loader.c: OpenGL sampler module loader
 *****************************************************************************
 * Copyright (C) 2020-2026 VLC authors and VideoLAN
 * Copyright (C) 2022-2026 Alexandre Janniaux <ajanni@videolabs.io>
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

#include "sampler.h"

#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc_opengl.h>

static int
ActivateSampler(void *func, bool forced, va_list args)
{
    (void) forced;
    vlc_gl_sampler_open_fn *activate = func;
    struct vlc_gl_sampler *sampler = va_arg(args, struct vlc_gl_sampler *);
    bool expose_planes = va_arg(args, int); /* bool promoted to int in va_args */

    return activate(sampler, expose_planes);
}

struct vlc_gl_sampler *
vlc_gl_sampler_New(struct vlc_gl_t *gl,
                   const struct vlc_gl_format *glfmt, bool expose_planes)
{
    struct vlc_gl_sampler *sampler = vlc_object_create(gl, sizeof(*sampler));
    if (!sampler)
        return NULL;

    sampler->gl = gl;

    /* Formats with palette are not supported. This also allows to copy
     * video_format_t without possibility of failure. */
    assert(!glfmt->fmt.p_palette);

    /* Populate public fields from glfmt */
    sampler->fmt_in = glfmt->fmt;
    sampler->tex_count = glfmt->tex_count;
    sampler->tex_target = glfmt->tex_target;
    sampler->half_float = glfmt->half_float;
    for (unsigned i = 0; i < glfmt->tex_count; i++)
    {
        sampler->tex_widths[i] = glfmt->tex_widths[i];
        sampler->tex_heights[i] = glfmt->tex_heights[i];
        sampler->formats[i] = glfmt->formats[i];
    }

    sampler->pic_to_tex_matrix = NULL;
    sampler->shader.version = NULL;
    sampler->shader.precision = NULL;
    sampler->shader.extensions = NULL;
    sampler->shader.body = NULL;
    sampler->ops = NULL;
    sampler->sys = NULL;

    struct vlc_logger *logger = vlc_object_logger(sampler);
    char *sampler_name = var_InheritString(gl, "gl-sampler");
    sampler->module = vlc_module_load(logger, "opengl sampler", sampler_name,
                                      true, ActivateSampler, sampler,
                                      (int)expose_planes);
    free(sampler_name);
    if (!sampler->module)
    {
        msg_Err(gl, "Could not load OpenGL sampler module");
        vlc_object_delete(sampler);
        return NULL;
    }

    return sampler;
}

void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler)
{
    if (sampler->ops && sampler->ops->close)
        sampler->ops->close(sampler);

    module_unneed(sampler, sampler->module);

    free(sampler->shader.precision);
    free(sampler->shader.extensions);
    free(sampler->shader.body);
    free(sampler->shader.version);

    vlc_object_delete(sampler);
}
