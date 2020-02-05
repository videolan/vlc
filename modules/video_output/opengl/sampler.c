/*****************************************************************************
 * sampler.c
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
#include <vlc_es.h>
#include <vlc_opengl.h>

#ifdef HAVE_LIBPLACEBO
#include <libplacebo/shaders.h>
#include <libplacebo/shaders/colorspace.h>
#include "../placebo_utils.h"
#endif

#include "gl_api.h"
#include "gl_common.h"
#include "gl_util.h"
#include "interop.h"

struct vlc_gl_sampler *
vlc_gl_sampler_New(struct vlc_gl_interop *interop)
{
    struct vlc_gl_sampler *sampler = calloc(1, sizeof(*sampler));
    if (!sampler)
        return NULL;

    sampler->uloc.pl_vars = NULL;
    sampler->pl_ctx = NULL;
    sampler->pl_sh = NULL;
    sampler->pl_sh_res = NULL;

    sampler->interop = interop;
    sampler->gl = interop->gl;
    sampler->vt = interop->vt;

    sampler->shader.body = NULL;

#ifdef HAVE_LIBPLACEBO
    // Create the main libplacebo context
    sampler->pl_ctx = vlc_placebo_Create(VLC_OBJECT(interop->gl));
    if (sampler->pl_ctx) {
#   if PL_API_VER >= 20
        sampler->pl_sh = pl_shader_alloc(sampler->pl_ctx, &(struct pl_shader_params) {
            .glsl = {
#       ifdef USE_OPENGL_ES2
                .version = 100,
                .gles = true,
#       else
                .version = 120,
#       endif
            },
        });
#   elif PL_API_VER >= 6
        sampler->pl_sh = pl_shader_alloc(sampler->pl_ctx, NULL, 0);
#   else
        sampler->pl_sh = pl_shader_alloc(sampler->pl_ctx, NULL, 0, 0);
#   endif
    }
#endif

    /* Texture size */
    for (unsigned j = 0; j < interop->tex_count; j++) {
        const GLsizei w = interop->fmt.i_visible_width  * interop->texs[j].w.num
                        / interop->texs[j].w.den;
        const GLsizei h = interop->fmt.i_visible_height * interop->texs[j].h.num
                        / interop->texs[j].h.den;
        if (interop->api->supports_npot) {
            sampler->tex_width[j]  = w;
            sampler->tex_height[j] = h;
        } else {
            sampler->tex_width[j]  = vlc_align_pot(w);
            sampler->tex_height[j] = vlc_align_pot(h);
        }
    }

    if (!interop->handle_texs_gen)
    {
        int ret = vlc_gl_interop_GenerateTextures(interop, sampler->tex_width,
                                                  sampler->tex_height,
                                                  sampler->textures);
        if (ret != VLC_SUCCESS)
        {
            free(sampler);
            return NULL;
        }
    }

    return sampler;
}

void
vlc_gl_sampler_Delete(struct vlc_gl_sampler *sampler)
{
    struct vlc_gl_interop *interop = sampler->interop;
    const opengl_vtable_t *vt = interop->vt;

    if (!interop->handle_texs_gen)
        vt->DeleteTextures(interop->tex_count, sampler->textures);

#ifdef HAVE_LIBPLACEBO
    FREENULL(sampler->uloc.pl_vars);
    if (sampler->pl_ctx)
        pl_context_destroy(&sampler->pl_ctx);
#endif

    free(sampler->shader.body);

    free(sampler);
}
