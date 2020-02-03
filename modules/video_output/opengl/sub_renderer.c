/*****************************************************************************
 * sub_renderer.c
 *****************************************************************************
 * Copyright (C) 2004-2020 VLC authors and VideoLAN
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

#include "sub_renderer.h"

#include <assert.h>
#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_subpicture.h>

#include "gl_util.h"
#include "interop.h"
#include "vout_helper.h"

typedef struct {
    GLuint   texture;
    GLsizei  width;
    GLsizei  height;

    float    alpha;

    float    top;
    float    left;
    float    bottom;
    float    right;

    float    tex_width;
    float    tex_height;
} gl_region_t;

struct vlc_gl_sub_renderer
{
    vlc_gl_t *gl;
    const struct vlc_gl_api *api;
    const opengl_vtable_t *vt; /* for convenience, same as &api->vt */

    struct vlc_gl_interop *interop;

    gl_region_t *regions;
    unsigned region_count;

    GLuint program_id;
    struct {
        GLint vertex_pos;
        GLint tex_coords_in;
    } aloc;
    struct {
        GLint sampler;
        GLint alpha;
    } uloc;

    GLuint *buffer_objects;
    unsigned buffer_object_count;
};

static int
FetchLocations(struct vlc_gl_sub_renderer *sr)
{
    assert(sr->program_id);

    const opengl_vtable_t *vt = sr->vt;

#define GET_LOC(type, x, str) do { \
    x = vt->Get##type##Location(sr->program_id, str); \
    assert(x != -1); \
    if (x == -1) { \
        msg_Err(sr->gl, "Unable to Get"#type"Location(%s)", str); \
        return VLC_EGENERIC; \
    } \
} while (0)
#define GET_ULOC(x, str) GET_LOC(Uniform, x, str)
#define GET_ALOC(x, str) GET_LOC(Attrib, x, str)
    GET_ULOC(sr->uloc.sampler, "sampler");
    GET_ULOC(sr->uloc.alpha, "alpha");
    GET_ALOC(sr->aloc.vertex_pos, "vertex_pos");
    GET_ALOC(sr->aloc.tex_coords_in, "tex_coords_in");

#undef GET_LOC
#undef GET_ULOC
#undef GET_ALOC

    return VLC_SUCCESS;
}

struct vlc_gl_sub_renderer *
vlc_gl_sub_renderer_New(vlc_gl_t *gl, const struct vlc_gl_api *api,
                        struct vlc_gl_interop *interop)
{
    const opengl_vtable_t *vt = &api->vt;

    struct vlc_gl_sub_renderer *sr = malloc(sizeof(*sr));
    if (!sr)
        return NULL;

    video_format_t fmt;
    video_format_Init(&fmt, VLC_CODEC_RGB32);

    /* Allocates our textures */
    assert(!interop->handle_texs_gen);

    sr->interop = interop;
    sr->gl = gl;
    sr->api = api;
    sr->vt = vt;
    sr->region_count = 0;
    sr->regions = NULL;

    static const char *const VERTEX_SHADER_SRC =
#if defined(USE_OPENGL_ES2)
        "#version 100\n"
#else
        "#version 120\n"
#endif
        "attribute vec2 vertex_pos;\n"
        "attribute vec2 tex_coords_in;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  tex_coords = tex_coords_in;\n"
        "  gl_Position = vec4(vertex_pos, 0.0, 1.0);\n"
        "}\n";

    static const char *const FRAGMENT_SHADER_SRC =
#if defined(USE_OPENGL_ES2)
        "#version 100\n"
        "precision mediump float;\n"
#else
        "#version 120\n"
#endif
        "uniform sampler2D sampler;\n"
        "uniform float alpha;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  vec4 color = texture2D(sampler, tex_coords);\n"
        "  color.a *= alpha;\n"
        "  gl_FragColor = color;\n"
        "}\n";

    sr->program_id =
        vlc_gl_BuildProgram(VLC_OBJECT(sr->gl), vt,
                            1, (const char **) &VERTEX_SHADER_SRC,
                            1, (const char **) &FRAGMENT_SHADER_SRC);
    if (!sr->program_id)
        goto error_1;

    int ret = FetchLocations(sr);
    if (ret != VLC_SUCCESS)
        goto error_2;

    /* Initial number of allocated buffer objects for subpictures, will grow dynamically. */
    static const unsigned INITIAL_BUFFER_OBJECT_COUNT = 8;
    sr->buffer_objects = vlc_alloc(INITIAL_BUFFER_OBJECT_COUNT, sizeof(GLuint));
    if (!sr->buffer_objects)
        goto error_2;

    sr->buffer_object_count = INITIAL_BUFFER_OBJECT_COUNT;

    vt->GenBuffers(sr->buffer_object_count, sr->buffer_objects);

    return sr;

error_2:
    vt->DeleteProgram(sr->program_id);
error_1:
    free(sr);

    return NULL;
}

void
vlc_gl_sub_renderer_Delete(struct vlc_gl_sub_renderer *sr)
{
    if (sr->buffer_object_count)
        sr->vt->DeleteBuffers(sr->buffer_object_count, sr->buffer_objects);
    free(sr->buffer_objects);

    for (unsigned i = 0; i < sr->region_count; ++i)
    {
        if (sr->regions[i].texture)
            sr->vt->DeleteTextures(1, &sr->regions[i].texture);
    }
    free(sr->regions);

    free(sr);
}

int
vlc_gl_sub_renderer_Prepare(struct vlc_gl_sub_renderer *sr, subpicture_t *subpicture)
{
    GL_ASSERT_NOERROR(sr->vt);

    const struct vlc_gl_interop *interop = sr->interop;

    int last_count = sr->region_count;
    gl_region_t *last = sr->regions;

    if (subpicture) {
        int count = 0;
        for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
            count++;

        gl_region_t *regions = calloc(count, sizeof(*regions));
        if (!regions)
            return VLC_ENOMEM;

        sr->region_count = count;
        sr->regions = regions;

        int i = 0;
        for (subpicture_region_t *r = subpicture->p_region;
             r; r = r->p_next, i++) {
            gl_region_t *glr = &sr->regions[i];

            glr->width  = r->fmt.i_visible_width;
            glr->height = r->fmt.i_visible_height;
            if (!sr->api->supports_npot) {
                glr->width  = vlc_align_pot(glr->width);
                glr->height = vlc_align_pot(glr->height);
                glr->tex_width  = (float) r->fmt.i_visible_width  / glr->width;
                glr->tex_height = (float) r->fmt.i_visible_height / glr->height;
            } else {
                glr->tex_width  = 1.0;
                glr->tex_height = 1.0;
            }
            glr->alpha  = (float)subpicture->i_alpha * r->i_alpha / 255 / 255;
            glr->left   =  2.0 * (r->i_x                          ) / subpicture->i_original_picture_width  - 1.0;
            glr->top    = -2.0 * (r->i_y                          ) / subpicture->i_original_picture_height + 1.0;
            glr->right  =  2.0 * (r->i_x + r->fmt.i_visible_width ) / subpicture->i_original_picture_width  - 1.0;
            glr->bottom = -2.0 * (r->i_y + r->fmt.i_visible_height) / subpicture->i_original_picture_height + 1.0;

            glr->texture = 0;
            /* Try to recycle the textures allocated by the previous
               call to this function. */
            for (int j = 0; j < last_count; j++) {
                if (last[j].texture &&
                    last[j].width  == glr->width &&
                    last[j].height == glr->height) {
                    glr->texture = last[j].texture;
                    memset(&last[j], 0, sizeof(last[j]));
                    break;
                }
            }

            const size_t pixels_offset =
                r->fmt.i_y_offset * r->p_picture->p->i_pitch +
                r->fmt.i_x_offset * r->p_picture->p->i_pixel_pitch;
            if (!glr->texture)
            {
                /* Could not recycle a previous texture, generate a new one. */
                int ret = vlc_gl_interop_GenerateTextures(interop, &glr->width,
                                                          &glr->height,
                                                          &glr->texture);
                if (ret != VLC_SUCCESS)
                    break;
            }
            /* Use the visible pitch of the region */
            r->p_picture->p[0].i_visible_pitch = r->fmt.i_visible_width
                                               * r->p_picture->p[0].i_pixel_pitch;
            int ret = interop->ops->update_textures(interop, &glr->texture,
                                                    &glr->width, &glr->height,
                                                    r->p_picture, &pixels_offset);
            if (ret != VLC_SUCCESS)
                break;
        }
    }
    else
    {
        sr->region_count = 0;
        sr->regions = NULL;
    }

    for (int i = 0; i < last_count; i++) {
        if (last[i].texture)
            vlc_gl_interop_DeleteTextures(interop, &last[i].texture);
    }
    free(last);

    GL_ASSERT_NOERROR(sr->vt);

    return VLC_SUCCESS;
}

int
vlc_gl_sub_renderer_Draw(struct vlc_gl_sub_renderer *sr)
{
    const struct vlc_gl_interop *interop = sr->interop;
    const opengl_vtable_t *vt = sr->vt;

    GL_ASSERT_NOERROR(vt);

    assert(sr->program_id);
    vt->UseProgram(sr->program_id);

    vt->Enable(GL_BLEND);
    vt->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* We need two buffer objects for each region: for vertex and texture coordinates. */
    if (2 * sr->region_count > sr->buffer_object_count) {
        if (sr->buffer_object_count > 0)
            vt->DeleteBuffers(sr->buffer_object_count, sr->buffer_objects);
        sr->buffer_object_count = 0;

        int new_count = 2 * sr->region_count;
        sr->buffer_objects = realloc_or_free(sr->buffer_objects, new_count * sizeof(GLuint));
        if (!sr->buffer_objects)
            return VLC_ENOMEM;

        sr->buffer_object_count = new_count;
        vt->GenBuffers(sr->buffer_object_count, sr->buffer_objects);
    }

    vt->ActiveTexture(GL_TEXTURE0 + 0);
    for (unsigned i = 0; i < sr->region_count; i++) {
        gl_region_t *glr = &sr->regions[i];
        const GLfloat vertexCoord[] = {
            glr->left,  glr->top,
            glr->left,  glr->bottom,
            glr->right, glr->top,
            glr->right, glr->bottom,
        };
        const GLfloat textureCoord[] = {
            0.0, 0.0,
            0.0, glr->tex_height,
            glr->tex_width, 0.0,
            glr->tex_width, glr->tex_height,
        };

        assert(glr->texture != 0);
        vt->BindTexture(interop->tex_target, glr->texture);

        vt->Uniform1f(sr->uloc.alpha, glr->alpha);

        vt->BindBuffer(GL_ARRAY_BUFFER, sr->buffer_objects[2 * i]);
        vt->BufferData(GL_ARRAY_BUFFER, sizeof(textureCoord), textureCoord, GL_STATIC_DRAW);
        vt->EnableVertexAttribArray(sr->aloc.tex_coords_in);
        vt->VertexAttribPointer(sr->aloc.tex_coords_in, 2, GL_FLOAT, 0, 0, 0);

        vt->BindBuffer(GL_ARRAY_BUFFER, sr->buffer_objects[2 * i + 1]);
        vt->BufferData(GL_ARRAY_BUFFER, sizeof(vertexCoord), vertexCoord, GL_STATIC_DRAW);
        vt->EnableVertexAttribArray(sr->aloc.vertex_pos);
        vt->VertexAttribPointer(sr->aloc.vertex_pos, 2, GL_FLOAT, 0, 0, 0);

        vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    vt->Disable(GL_BLEND);

    GL_ASSERT_NOERROR(vt);

    return VLC_SUCCESS;
}
