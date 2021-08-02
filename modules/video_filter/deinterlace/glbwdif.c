/*****************************************************************************
 * glbwdif.c
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
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_opengl.h>
#include <vlc_filter.h>

#include "video_output/opengl/gl_api.h"
#include "video_output/opengl/gl_common.h"
#include "video_output/opengl/gl_util.h"
#include "video_output/opengl/interop.h"
#include "video_output/opengl/sampler.h"

/**
 * The input picture contains 3 planes YUV (typically I420).
 *
 * The bwdif algorithm applies to individual planes separately. It only impacts
 * half of the rows.
 *
 * For performances reasons, it is applied in two passes:
 *  1. the bwdif filter is applied on half of the rows (it produces a half
 *     height result)
 *  2. a "gather" filter produces the final filtered plane by combining the
 *     result of the bwdif filter and the missing rows from the input plane.
 *
 * There are two slightly different OpenGL programs for the first pass, which
 * differs by only the value of a constant (YADIF_ORDER). For bwdif1x, only the
 * first program is used. For bwdif2x, the two programs are used alternatively.
 *
 * Once the 3 intermediate filtered planes are produced, a final "draw" filter
 * converts them to a single RGB texture.
 */

struct program_bwdif {
    GLuint id[2];
    GLuint id_gather;
    GLuint vbo;

    struct {
        GLint vertex_pos;
        GLint prev;
        GLint cur;
        GLint next;
        GLint width;
        GLint height;
        GLint field;
    } loc_order[2];

    struct {
        GLint vertex_pos;
        GLint cur;
        GLint computed;
        GLint height;
        GLint field;
    } loc_gather;
};

struct program_draw
{
    GLuint id;
    GLuint vbo;
    struct
    {
        GLint vertex_pos;
        GLint matrix_yuv_rgb;
        GLint samplers[3];
    } loc;
    GLfloat matrix_yuv_rgb[4*4];
};

struct sys
{
    vlc_gl_t *gl;
    struct vlc_gl_api api;
    const opengl_vtable_t *vt; // &api->vt
    struct vlc_gl_interop *interop;

    GLsizei tex_widths[3];
    GLsizei tex_heights[3];

    struct {
        GLuint textures[3];
    } frames_in[3];
    unsigned next; // index of the next frame to receive
    unsigned missing_frames;
    unsigned order;
    vlc_tick_t last_pts;
    vlc_tick_t previous_pts;
    bool top_field_first;

    bool is_bwdif2x;

    GLuint framebuffers_tmp[3];
    GLuint textures_tmp[3];

    GLuint framebuffers_out[3];
    GLuint textures_out[3];

    struct program_bwdif program_bwdif;
    struct program_draw program_draw;
};

static int
DrawPlane(filter_t *filter, unsigned plane)
{
    struct sys *sys = filter->p_sys;
    struct program_bwdif *program_bwdif = &sys->program_bwdif;
    const opengl_vtable_t *vt = sys->vt;
    GLenum tex_target = sys->interop->tex_target;

    unsigned width = sys->tex_widths[plane];
    unsigned height = sys->tex_heights[plane];

    GLuint next = sys->next;
    GLuint prev = (sys->next + 1) % 3;
    GLuint cur = (sys->next + 2) % 3;
    if (sys->missing_frames)
    {
        if (sys->missing_frames == 2)
            cur = next;
        prev = cur;
    }

    /**
     * order == 0 &&  top_field_first  ==>  field = 0
     * order == 0 && !top_field_first  ==>  field = 1
     * order == 1 &&  top_field_first  ==>  field = 1
     * order == 1 && !top_field_first  ==>  field = 0
     */
    unsigned order = sys->order;
    unsigned field = order ^ !sys->top_field_first;

    vt->UseProgram(program_bwdif->id[order]);
    vt->Uniform1i(program_bwdif->loc_order[order].field, field);

    vt->Uniform1f(program_bwdif->loc_order[order].width, width);
    vt->Uniform1f(program_bwdif->loc_order[order].height, height);

    vt->Uniform1i(program_bwdif->loc_order[order].prev, 0);
    vt->ActiveTexture(GL_TEXTURE0);
    vt->BindTexture(tex_target, sys->frames_in[prev].textures[plane]);

    vt->Uniform1i(program_bwdif->loc_order[order].cur, 1);
    vt->ActiveTexture(GL_TEXTURE1);
    vt->BindTexture(tex_target, sys->frames_in[cur].textures[plane]);

    vt->Uniform1i(program_bwdif->loc_order[order].next, 2);
    vt->ActiveTexture(GL_TEXTURE2);
    vt->BindTexture(tex_target, sys->frames_in[next].textures[plane]);

    vt->BindBuffer(GL_ARRAY_BUFFER, program_bwdif->vbo);
    vt->EnableVertexAttribArray(program_bwdif->loc_order[order].vertex_pos);
    vt->VertexAttribPointer(program_bwdif->loc_order[order].vertex_pos, 2,
                            GL_FLOAT, GL_FALSE, 0, (const void *) 0);

    /* Interpolate missing lines in the intermediate buffer (with
     * size / 2). */
    vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, sys->framebuffers_tmp[plane]);

    vt->Viewport(0, 0, sys->tex_widths[plane], sys->tex_heights[plane] / 2);

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    /* Combine results with current picture */
    vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, sys->framebuffers_out[plane]);

    vt->UseProgram(program_bwdif->id_gather);

    vt->Uniform1i(program_bwdif->loc_gather.field, field);

    vt->Uniform1f(program_bwdif->loc_gather.height, height);

    /* Binded in previous step */
    vt->Uniform1i(program_bwdif->loc_gather.cur, 1);

    /* We can drop the previous binding */
    vt->ActiveTexture(GL_TEXTURE2);
    vt->BindTexture(GL_TEXTURE_2D, sys->textures_tmp[plane]);
    vt->Uniform1i(program_bwdif->loc_gather.computed, 2);

    vt->BindBuffer(GL_ARRAY_BUFFER, program_bwdif->vbo);
    vt->EnableVertexAttribArray(program_bwdif->loc_gather.vertex_pos);
    vt->VertexAttribPointer(program_bwdif->loc_gather.vertex_pos, 2,
                            GL_FLOAT, GL_FALSE, 0, (const void *) 0);

    vt->Viewport(0, 0, sys->tex_widths[plane], sys->tex_heights[plane]);

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    return VLC_SUCCESS;
}

static picture_t *
Filter(filter_t *filter, picture_t *input)
{
    struct sys *sys = filter->p_sys;
    struct vlc_gl_interop *interop = sys->interop;

    const opengl_vtable_t *vt = sys->vt;

    picture_t *output = NULL;

    if (!input && (!sys->is_bwdif2x || sys->last_pts == VLC_TICK_INVALID || sys->order != 0))
    {
        /* Called with NULL picture but nothing else to draw */
        goto finally_1;
    }

    if (vlc_gl_MakeCurrent(sys->gl) != VLC_SUCCESS)
        goto finally_1;

    GLint value;
    vt->GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    GLuint final_draw_framebuffer = value; /* as GLuint */

    if (input)
    {
        sys->next = (sys->next + 1) % 3;

        GLuint *textures_in = sys->frames_in[sys->next].textures;
        int ret = interop->ops->update_textures(interop, textures_in,
                                                sys->tex_widths,
                                                sys->tex_heights,
                                                input, NULL);
        if (ret != VLC_SUCCESS)
            goto finally_2;

        if (sys->missing_frames)
            --sys->missing_frames;

        sys->order = 0;
        sys->top_field_first = input->b_top_field_first;
    }
    else
    {
        assert(sys->order == 0);
        sys->order = 1;
    }

    if (sys->missing_frames >= 2)
    {
        assert(sys->missing_frames == 2);
        /* Not enough input frame to produce an output */
        assert(!output);
        goto finally_2;
    }

    for (unsigned i = 0; i < 3; ++i)
    {
        int ret = DrawPlane(filter, i);
        if (ret != VLC_SUCCESS)
            goto finally_2;
    }

    struct program_draw *program_draw = &sys->program_draw;

    vt->UseProgram(program_draw->id);

    vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, final_draw_framebuffer);

    for (unsigned i = 0; i < 3; ++i)
    {
        vt->Uniform1i(program_draw->loc.samplers[i], i);

        assert(sys->textures_out[i] != 0);
        vt->ActiveTexture(GL_TEXTURE0 + i);
        vt->BindTexture(GL_TEXTURE_2D, sys->textures_out[i]);
    }

    vt->UniformMatrix4fv(program_draw->loc.matrix_yuv_rgb, 1, GL_FALSE,
                         program_draw->matrix_yuv_rgb);

    vt->BindBuffer(GL_ARRAY_BUFFER, program_draw->vbo);
    vt->EnableVertexAttribArray(program_draw->loc.vertex_pos);
    vt->VertexAttribPointer(program_draw->loc.vertex_pos, 2, GL_FLOAT, GL_FALSE,
                            0, (const void *) 0);

    vt->Viewport(0, 0, sys->tex_widths[0], sys->tex_heights[0]);

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    output = vlc_gl_SwapOffscreen(sys->gl);

    if (input) {
        output->date = input->date;

        sys->previous_pts = sys->last_pts;
        sys->last_pts = input->date;
    } else {
        assert(sys->is_bwdif2x);
        assert(sys->last_pts != VLC_TICK_INVALID);
        if (sys->previous_pts != VLC_TICK_INVALID)
        {
            /*
             *                      output->date
             *                       v
             *        |----.----|----.----|
             *        ^         ^
             * previous_pts    last_pts
             */
            output->date = (3 * sys->last_pts - sys->previous_pts) / 2;
        }
        else
        {
            output->date = sys->last_pts + 1; /* what could we do? */
        }
    }
    output->format.i_frame_rate =
        filter->fmt_out.video.i_frame_rate;
    output->format.i_frame_rate_base =
        filter->fmt_out.video.i_frame_rate_base;

finally_2:
    vlc_gl_ReleaseCurrent(sys->gl);

finally_1:
    if (input)
        picture_Release(input);

    return output;
}

static void
Flush(filter_t *filter)
{
    struct sys *sys = filter->p_sys;

    sys->missing_frames = 3;
    sys->order = 0;
    sys->last_pts = VLC_TICK_INVALID;
    sys->previous_pts = VLC_TICK_INVALID;
}

static const char *
GetDefines(GLenum tex_target)
{
    switch (tex_target)
    {
        case GL_TEXTURE_EXTERNAL_OES:
            return "#define VLC_SAMPLER samplerExternalOES\n"
                   "#define VLC_TEXTURE texture2D\n";
        case GL_TEXTURE_2D:
            return "#define VLC_SAMPLER sampler2D\n"
                   "#define VLC_TEXTURE texture2D\n";
        case GL_TEXTURE_RECTANGLE:
            return "#define VLC_SAMPLER sampler2DRect\n"
                   "#define VLC_TEXTURE texture2DRect\n";
        default:
            vlc_assert_unreachable();
    }
}

static int
InitFramebufferTexture(const opengl_vtable_t *vt, GLuint framebuffer,
                       GLuint texture, GLsizei width, GLsizei height)
{
    vt->BindTexture(GL_TEXTURE_2D, texture);
    vt->TexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB,
                   GL_UNSIGNED_BYTE, NULL);
    vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    /* iOS needs GL_CLAMP_TO_EDGE or power-of-two textures */
    vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Attach the texture to the frame buffer */
    vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
    vt->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, texture, 0);

    GLenum status = vt->CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static int
InitPlane(filter_t *filter, unsigned plane, GLsizei width, GLsizei height)
{
    struct sys *sys = filter->p_sys;
    const opengl_vtable_t *vt = sys->vt;

    /* Save bindings */
    GLint draw_framebuffer;
    vt->GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);

    int ret = InitFramebufferTexture(vt, sys->framebuffers_out[plane],
                                     sys->textures_out[plane], width, height);
    if (ret != VLC_SUCCESS)
        return VLC_EGENERIC;

    /* Intermediate have height / 2 */
    ret = InitFramebufferTexture(vt, sys->framebuffers_tmp[plane],
                                 sys->textures_tmp[plane], width, height / 2);
    if (ret != VLC_SUCCESS)
    {
        vt->DeleteTextures(3, sys->textures_out);
        vt->DeleteFramebuffers(3, sys->framebuffers_out);
        return VLC_EGENERIC;
    }

    /* Restore bindings */
    vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_framebuffer);

    return VLC_SUCCESS;
}

static void
DeletePlanes(filter_t *filter)
{
    struct sys *sys = filter->p_sys;
    const opengl_vtable_t *vt = sys->vt;

    vt->DeleteTextures(3, sys->textures_out);
    vt->DeleteFramebuffers(3, sys->framebuffers_out);

    vt->DeleteTextures(3, sys->textures_tmp);
    vt->DeleteFramebuffers(3, sys->framebuffers_tmp);
}

static int
InitPlanes(filter_t *filter)
{
    struct sys *sys = filter->p_sys;
    const opengl_vtable_t *vt = sys->vt;

    vt->GenFramebuffers(3, sys->framebuffers_out);
    vt->GenTextures(3, sys->textures_out);

    vt->GenFramebuffers(3, sys->framebuffers_tmp);
    vt->GenTextures(3, sys->textures_tmp);

    for (unsigned i = 0; i < 3; ++i)
    {
        int ret = InitPlane(filter, i, sys->tex_widths[i], sys->tex_heights[i]);
        if (ret != VLC_SUCCESS)
            goto error;
    }

    return VLC_SUCCESS;

error:
    DeletePlanes(filter);

    return VLC_EGENERIC;
}

static int
CreateProgramBwdif(filter_t *filter)
{
    static const char *const VERTEX_SHADER_BODY =
        "attribute vec2 vertex_pos;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  gl_Position = vec4(vertex_pos, 0.0, 1.0);\n"
        "  tex_coords = vec2((vertex_pos.x + 1.0) / 2.0,\n"
        "                    (vertex_pos.y + 1.0) / 2.0);\n"
        "}\n";

    static const char *const FRAGMENT_SHADER_BODY =
        "varying vec2 tex_coords;\n"
        "uniform VLC_SAMPLER prev;\n"
        "uniform VLC_SAMPLER cur;\n"
        "uniform VLC_SAMPLER next;\n"
        "uniform float width;\n"
        "uniform float height;\n"
        "uniform int field;\n"
        "\n"
        "float pix(sampler2D sampler, float x, float y) {\n"
        "  return VLC_TEXTURE(sampler, vec2(x / width, y / height)).x;\n"
        "}\n"
        "\n"
        "float compute_score(float x, float y, float j) {\n"
        "  return abs(pix(cur, x-1.0+j, y+1.0) - pix(cur, x-1.0-j, y-1.0))\n"
        "       + abs(pix(cur, x    +j, y+1.0) - pix(cur, x    -j, y-1.0))\n"
        "       + abs(pix(cur, x+1.0+j, y+1.0) - pix(cur, x+1.0-j, y-1.0));\n"
        "}\n"
        "\n"
        "float compute_pred(float x, float y, float j) {\n"
        "  return (pix(cur, x+j, y+1.0) + pix(cur, x-j, y-1.0)) / 2.0;"
        "}\n"
        "\n"
        "float filter_internal(float x, float y,\n"
        "                      sampler2D prev2, sampler2D next2) {\n"
        "  float prev_pix = pix(prev, x, y);\n"
        "  float cur_pix = pix(cur, x, y);\n"
        "  float next_pix = pix(next, x, y);\n"
        "\n"
        "  float prev2_pix;\n"
        "  float next2_pix;\n"
        "# if YADIF_ORDER == 0\n"
        "    prev2_pix = prev_pix;\n"
        "    next2_pix = cur_pix;\n"
        "# else\n"
        "    prev2_pix = cur_pix;\n"
        "    next2_pix = next_pix;\n"
        "# endif\n"
        "\n"
        "  float c = pix(cur, x, y+1.0);\n"
        "  float d = (prev2_pix + next2_pix) / 2.0;\n"
        "  float e = pix(cur, x, y-1.0);\n"
        "  float temporal_diff0 = abs(prev2_pix - next2_pix) / 2.0;\n"
        "  float temporal_diff1 = (abs(pix(prev, x, y+1.0) - c)\n"
        "                        + abs(pix(prev, x, y-1.0) - e)) / 2.0;\n"
        "  float temporal_diff2 = (abs(pix(next, x, y+1.0) - c)\n"
        "                        + abs(pix(next, x, y-1.0) - e)) / 2.0;\n"
        "  float diff = max(temporal_diff0,\n"
        "                   max(temporal_diff1, temporal_diff2));\n"
        "  if (diff == 0.0) {\n"
        "      return d;\n"
        "  }\n"
        "\n"
        "  float b = (pix(prev2, x, y+2.0) + pix(next2, x, y+2.0)) / 2.0 - c;\n"
        "  float f = (pix(prev2, x, y-2.0) + pix(next2, x, y-2.0)) / 2.0 - e;\n"
        "  float dc = d - c;\n"
        "  float de = d - e;\n"
        "\n"
        "#define max3(a,b,c) max(max(a,b),c)\n"
        "#define min3(a,b,c) min(min(a,b),c)\n"
        "\n"
        "  float vmax = max3(de, dc, min(b, f));\n"
        "  float vmin = min3(de, dc, max(b, f));\n"
        "  diff = max3(diff, vmin, -vmax);\n"
        "  float interpol;\n"
        "\n"
        "#define coef_lf0 4309.0\n"
        "#define coef_lf1  213.0\n"
        "#define coef_hf0 5570.0\n"
        "#define coef_hf1 3801.0\n"
        "#define coef_hf2 1016.0\n"
        "#define coef_sp0 5077.0\n"
        "#define coef_sp1  981.0\n"
        "\n"
        "  if (abs(c - e) > temporal_diff0) {\n"
        "    interpol = (\n"
        "                 (\n"
        "                     coef_hf0 * (prev2_pix + next2_pix)\n"
        "                   - coef_hf1 * ( pix(prev2, x, y+2.0)\n"
        "                                + pix(next2, x, y+2.0)\n"
        "                                + pix(prev2, x, y-2.0)\n"
        "                                + pix(next2, x, y-2.0) )\n"
        "                   + coef_hf2 * ( pix(prev2, x, y+4.0)\n"
        "                                + pix(next2, x, y+4.0)\n"
        "                                + pix(prev2, x, y-4.0)\n"
        "                                + pix(next2, x, y-4.0) )\n"
        "                 ) / 4.0\n"
        "                 + coef_lf0 * (c + e)\n"
        "                 - coef_lf1 * ( pix(cur, x, y+3.0)\n"
        "                              + pix(cur, x, y-3.0) )\n"
        "               ) / 8192.0;\n"
        "  } else {\n"
        "    interpol = (\n"
        "                   coef_sp0 * (c + e)\n"
        "                 - coef_sp1 * ( pix(cur, x, y+3.0)\n"
        "                              + pix(cur, x, y-3.0) )\n"
        "               ) / 8192.0;\n"
        "  }\n"
        "  interpol = max( min(interpol, d + diff), d - diff );\n"
        "  return interpol;\n"
        "}\n"
        "\n"
        "float bwdif_filter(float x, float y) {\n"
        "# if YADIF_ORDER == 0\n"
        "  return filter_internal(x, y, prev, cur);\n"
        "# else\n"
        "  return filter_internal(x, y, cur, next);\n"
        "# endif\n"
        "}\n"
        "\n"
        "void main() {\n"
        "  float x = tex_coords.x * width;\n"
        "  float y = floor(tex_coords.y * height / 2.0) * 2.0 + 0.5;\n"
        /* bwdif is applied upside-down, because the texture is uploaded
         * upside-down, that's why the condition is (field == 0) instead of
         * (field == 1) */
        "  if (field == 0) y += 1.0;\n"
        "  float result = bwdif_filter(x, y);\n"
        "  gl_FragColor = vec4(result, 0.0, 0.0, 1.0);\n"
        "}\n";

    static const char *const GATHER_FRAGMENT_SHADER_BODY =
        "varying vec2 tex_coords;\n"
        "uniform VLC_SAMPLER cur;\n"
        "uniform sampler2D computed;\n"
        "uniform float height;\n"
        "uniform int field;\n"
        "\n"
        "void main() {\n"
        /* The line number */
        "  float line = floor(tex_coords.y * height);\n"
        "\n"
        "  float cur_pix = VLC_TEXTURE(cur, tex_coords).x;\n"
        "  float cur_computed = texture2D(computed, tex_coords).x;\n"

        "  float result;\n"
        "  if (int(mod(line, 2.0)) == field) {\n"
        "    result = cur_pix;\n"
        "  } else {\n"
        "    result = cur_computed;\n"
        "  }\n"
        "  gl_FragColor = vec4(result, 0.0, 0.0, 1.0);\n"
        "}\n";

    struct sys *sys = filter->p_sys;
    const opengl_vtable_t *vt = sys->vt;

    const char *shader_version;
    const char *shader_precision;
    if (sys->api.is_gles)
    {
        shader_version = "#version 100\n";
        shader_precision = "precision highp float;\n";
    }
    else
    {
        shader_version = "#version 120\n";
        shader_precision = "";
    }

    const char *defines = GetDefines(sys->interop->tex_target);

    struct program_bwdif *program_bwdif = &sys->program_bwdif;

    const char *vertex_shader[] = {
        shader_version,
        shader_precision,
        VERTEX_SHADER_BODY,
    };

    const char *fragment_shader_order0[] = {
        shader_version,
        shader_precision,
        defines,
        "#define YADIF_ORDER 0\n",
        FRAGMENT_SHADER_BODY,
    };

    const char *fragment_shader_order1[] = {
        shader_version,
        shader_precision,
        defines,
        "#define YADIF_ORDER 1\n",
        FRAGMENT_SHADER_BODY,
    };

    program_bwdif->id[0] =
        vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                            ARRAY_SIZE(vertex_shader), vertex_shader,
                            ARRAY_SIZE(fragment_shader_order0),
                            fragment_shader_order0);
    if (!program_bwdif->id[0])
        return VLC_EGENERIC;

    if (sys->is_bwdif2x)
    {
        program_bwdif->id[1] =
            vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                                ARRAY_SIZE(vertex_shader), vertex_shader,
                                ARRAY_SIZE(fragment_shader_order1),
                                fragment_shader_order1);
        if (!program_bwdif->id[1])
        {
            vt->DeleteProgram(program_bwdif->id[0]);
            return VLC_EGENERIC;
        }
    }

    for (unsigned i = 0; i < 1u + sys->is_bwdif2x; ++i)
    {
        program_bwdif->loc_order[i].vertex_pos =
            vt->GetAttribLocation(program_bwdif->id[i], "vertex_pos");
        assert(program_bwdif->loc_order[i].vertex_pos != -1);

        program_bwdif->loc_order[i].prev =
            vt->GetUniformLocation(program_bwdif->id[i], "prev");
        assert(program_bwdif->loc_order[i].prev != -1);

        program_bwdif->loc_order[i].cur =
            vt->GetUniformLocation(program_bwdif->id[i], "cur");
        assert(program_bwdif->loc_order[i].cur != -1);

        program_bwdif->loc_order[i].next =
            vt->GetUniformLocation(program_bwdif->id[i], "next");
        assert(program_bwdif->loc_order[i].next != -1);

        program_bwdif->loc_order[i].width =
            vt->GetUniformLocation(program_bwdif->id[i], "width");
        assert(program_bwdif->loc_order[i].width != -1);

        program_bwdif->loc_order[i].height =
            vt->GetUniformLocation(program_bwdif->id[i], "height");
        assert(program_bwdif->loc_order[i].height != -1);

        program_bwdif->loc_order[i].field =
            vt->GetUniformLocation(program_bwdif->id[i], "field");
        assert(program_bwdif->loc_order[i].field != -1);
    }

    const char *fragment_shader_gather[] = {
        shader_version,
        shader_precision,
        defines,
        GATHER_FRAGMENT_SHADER_BODY,
    };

    program_bwdif->id_gather =
        vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                            ARRAY_SIZE(vertex_shader), vertex_shader,
                            ARRAY_SIZE(fragment_shader_gather),
                            fragment_shader_gather);
    if (!program_bwdif->id_gather)
    {
        vt->DeleteProgram(program_bwdif->id[0]);
        if (sys->is_bwdif2x)
            vt->DeleteProgram(program_bwdif->id[1]);
        return VLC_EGENERIC;
    }

    program_bwdif->loc_gather.vertex_pos =
        vt->GetAttribLocation(program_bwdif->id_gather, "vertex_pos");
    assert(program_bwdif->loc_gather.vertex_pos != -1);

    program_bwdif->loc_gather.cur =
        vt->GetUniformLocation(program_bwdif->id_gather, "cur");
    assert(program_bwdif->loc_gather.cur != -1);

    program_bwdif->loc_gather.computed =
        vt->GetUniformLocation(program_bwdif->id_gather, "computed");
    assert(program_bwdif->loc_gather.computed != -1);

    program_bwdif->loc_gather.height =
        vt->GetUniformLocation(program_bwdif->id_gather, "height");
    assert(program_bwdif->loc_gather.height != -1);

    program_bwdif->loc_gather.field =
        vt->GetUniformLocation(program_bwdif->id_gather, "field");
    assert(program_bwdif->loc_gather.field != -1);

    vt->GenBuffers(1, &program_bwdif->vbo);

    static const GLfloat vertex_pos[] = {
        -1,  1,
        -1, -1,
         1,  1,
         1, -1,
    };

    vt->BindBuffer(GL_ARRAY_BUFFER, program_bwdif->vbo);
    vt->BufferData(GL_ARRAY_BUFFER, sizeof(vertex_pos), vertex_pos,
                   GL_STATIC_DRAW);

    vt->BindBuffer(GL_ARRAY_BUFFER, 0);

    return VLC_SUCCESS;
}

static void
DeleteProgramBwdif(filter_t *filter)
{
    struct sys *sys = filter->p_sys;
    struct program_bwdif *program_bwdif = &sys->program_bwdif;
    const opengl_vtable_t *vt = sys->vt;

    vt->DeleteProgram(program_bwdif->id[0]);
    vt->DeleteProgram(program_bwdif->id[1]);
    vt->DeleteProgram(program_bwdif->id_gather);
    vt->DeleteBuffers(1, &program_bwdif->vbo);
}

static int
CreateProgramDraw(filter_t *filter, video_color_space_t yuv_space, bool vflip)
{
    static const char *const VERTEX_SHADER_BODY =
        "attribute vec2 vertex_pos;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  gl_Position = vec4(vertex_pos, 0.0, 1.0);\n"
        "  tex_coords = vec2((vertex_pos.x + 1.0) / 2.0,\n"
        "                    (vertex_pos.y + 1.0) / 2.0);\n"
        "}\n";

    static const char *const VERTEX_SHADER_BODY_VFLIP =
        "attribute vec2 vertex_pos;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  gl_Position = vec4(vertex_pos, 0.0, 1.0);\n"
        "  tex_coords = vec2((vertex_pos.x + 1.0) / 2.0,\n"
        "                    (1.0 - vertex_pos.y) / 2.0);\n"
        "}\n";

    static const char *const FRAGMENT_SHADER_BODY =
        "uniform sampler2D samplers[3];\n"
        "uniform mat4 matrix_yuv_rgb;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  vec4 yuv = vec4(\n"
        "      texture2D(samplers[0], tex_coords).x,\n"
        "      texture2D(samplers[1], tex_coords).x,\n"
        "      texture2D(samplers[2], tex_coords).x,\n"
        "      1.0);\n"
        "  gl_FragColor = matrix_yuv_rgb * yuv;\n"
        "}\n";

    struct sys *sys = filter->p_sys;
    const opengl_vtable_t *vt = sys->vt;

    const char *shader_version;
    const char *shader_precision;
    if (sys->api.is_gles)
    {
        shader_version = "#version 100\n";
        shader_precision = "precision highp float;\n";
    }
    else
    {
        shader_version = "#version 120\n";
        shader_precision = "";
    }

    struct program_draw *program_draw = &sys->program_draw;

    const char *vertex_shader[] = {
        shader_version,
        shader_precision,
        vflip ? VERTEX_SHADER_BODY_VFLIP : VERTEX_SHADER_BODY,
    };

    const char *fragment_shader[] = {
        shader_version,
        shader_precision,
        FRAGMENT_SHADER_BODY,
    };

    program_draw->id =
        vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                            ARRAY_SIZE(vertex_shader), vertex_shader,
                            ARRAY_SIZE(fragment_shader), fragment_shader);
    if (!program_draw->id)
        return VLC_EGENERIC;

    const video_color_range_t range = COLOR_RANGE_LIMITED;
    vlc_sampler_yuv2rgb_matrix(program_draw->matrix_yuv_rgb, yuv_space, range);

    program_draw->loc.vertex_pos =
        vt->GetAttribLocation(program_draw->id, "vertex_pos");
    assert(program_draw->loc.vertex_pos != -1);

    program_draw->loc.matrix_yuv_rgb =
        vt->GetUniformLocation(program_draw->id, "matrix_yuv_rgb");
    assert(program_draw->loc.matrix_yuv_rgb != -1);

    program_draw->loc.samplers[0] =
        vt->GetUniformLocation(program_draw->id, "samplers[0]");
    assert(program_draw->loc.samplers[0] != -1);

    program_draw->loc.samplers[1] =
        vt->GetUniformLocation(program_draw->id, "samplers[1]");
    assert(program_draw->loc.samplers[1] != -1);

    program_draw->loc.samplers[2] =
        vt->GetUniformLocation(program_draw->id, "samplers[2]");
    assert(program_draw->loc.samplers[2] != -1);

    vt->GenBuffers(1, &program_draw->vbo);

    static const GLfloat vertex_pos[] = {
        -1,  1,
        -1, -1,
         1,  1,
         1, -1,
    };

    vt->BindBuffer(GL_ARRAY_BUFFER, program_draw->vbo);
    vt->BufferData(GL_ARRAY_BUFFER, sizeof(vertex_pos), vertex_pos,
                   GL_STATIC_DRAW);

    vt->BindBuffer(GL_ARRAY_BUFFER, 0);

    return VLC_SUCCESS;
}

static void
DeleteProgramDraw(filter_t *filter)
{
    struct sys *sys = filter->p_sys;
    struct program_draw *program_draw = &sys->program_draw;
    const opengl_vtable_t *vt = sys->vt;

    vt->DeleteProgram(program_draw->id);
    vt->DeleteBuffers(1, &program_draw->vbo);
}

static void
Close(filter_t *filter)
{
    struct sys *sys = filter->p_sys;
    assert(sys);

    vlc_gl_MakeCurrent(sys->gl);

    DeleteProgramBwdif(filter);
    DeleteProgramDraw(filter);

    DeletePlanes(filter);

    for (unsigned i = 0; i < 3; ++i) {
        vlc_gl_interop_DeleteTextures(sys->interop, sys->frames_in[i].textures);
    }

    vlc_gl_interop_Delete(sys->interop);
    vlc_gl_ReleaseCurrent(sys->gl);

    vlc_gl_Release(sys->gl);
    free(sys);
}

static int
Open(vlc_object_t *obj, const char *name)
{
    char *mode = var_InheritString(obj, "deinterlace-mode");
    bool is_supported =
        mode == NULL ||
        strcmp(mode, "auto") == 0 ||
        strcmp(mode, name) == 0;

    free(mode);

    if (!is_supported)
        return VLC_EGENERIC;

    filter_t *filter = (filter_t *) obj;

    struct sys *sys = filter->p_sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_ENOMEM;

    sys->is_bwdif2x = !strcmp(name, "glbwdif2x") || !strcmp(name, "gl_bwdif2x") || !strcmp(name, "bwdif2x");

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

    struct vlc_decoder_device *device = filter_HoldDecoderDevice(filter);
    sys->gl = vlc_gl_CreateOffscreen(obj, device, width, height, VLCGLAPI, NULL);

    /* The vlc_gl_t instance must have hold the device if it needs it. */
    if (device)
        vlc_decoder_device_Release(device);

    if (sys->gl == NULL)
    {
        msg_Err(obj, "Failed to create opengl context\n");
        goto gl_create_failure;
    }

    sys->next = 0;
    sys->missing_frames = 3;
    sys->order = 0;
    sys->last_pts = VLC_TICK_INVALID;
    sys->previous_pts = VLC_TICK_INVALID;

    if (vlc_gl_MakeCurrent(sys->gl) != VLC_SUCCESS)
    {
        msg_Err(obj, "Failed to gl make current");
        assert(false);
        goto make_current_failure;
    }

    if (vlc_gl_api_Init(&sys->api, sys->gl) != VLC_SUCCESS)
    {
        msg_Err(obj, "Failed to initialize gl_api");
        goto gl_api_failure;
    }

    sys->vt = &sys->api.vt;

    struct vlc_gl_interop *interop = sys->interop =
        vlc_gl_interop_New(sys->gl, &sys->api, filter->vctx_in,
                           &filter->fmt_in.video);
    if (!interop)
    {
        msg_Err(obj, "Could not create interop");
        goto gl_interop_failure;
    }

    if (interop->tex_count != 3)
    {
        msg_Err(obj, "Expected 3 planes, got %u\n", interop->tex_count);
        goto unexpected_tex_count;
    }

    for (unsigned i = 0; i < 3; i++) {
        sys->tex_widths[i] = interop->fmt_out.i_visible_width
                           * interop->texs[i].w.num / interop->texs[i].w.den;
        sys->tex_heights[i] = interop->fmt_out.i_visible_height
                            * interop->texs[i].h.num / interop->texs[i].h.den;
    }

    for (unsigned i = 0; i < 3; ++i) {
        int ret = vlc_gl_interop_GenerateTextures(interop, sys->tex_widths,
                                                  sys->tex_heights,
                                                  sys->frames_in[i].textures);
        if (ret != VLC_SUCCESS) {
            for (unsigned j = i; j; --j)
            {
                vlc_gl_interop_DeleteTextures(sys->interop,
                                              sys->frames_in[j - 1].textures);
            }
            goto unexpected_tex_count;
        }
    }

    int ret = CreateProgramBwdif(filter);
    if (ret != VLC_SUCCESS)
        goto create_program_bwdif_failure;

    ret = InitPlanes(filter);
    if (ret != VLC_SUCCESS)
        goto init_planes_failure;

    /* The input picture is uploaded upside-down, so we must add an additional
     * vflip if and only if the offscreen does not adds its own vflip */
    bool must_vflip = !sys->gl->offscreen_vflip;
    ret = CreateProgramDraw(filter, filter->fmt_in.video.space, must_vflip);
    if (ret != VLC_SUCCESS)
        goto create_program_draw_failure;

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

    filter->fmt_out.video.i_frame_rate = filter->fmt_in.video.i_frame_rate;
    filter->fmt_out.video.i_frame_rate_base =
        filter->fmt_in.video.i_frame_rate_base;
    assert(filter->fmt_out.video.i_frame_rate_base != 0);

    filter->vctx_out = sys->gl->offscreen_vctx_out;

    if (sys->is_bwdif2x)
        filter->fmt_out.video.i_frame_rate *= 2;

    vlc_gl_ReleaseCurrent(sys->gl);

    return VLC_SUCCESS;

create_program_draw_failure:
    DeletePlanes(filter);
init_planes_failure:
    DeleteProgramBwdif(filter);
create_program_bwdif_failure:
    for (unsigned i = 0; i < 3; ++i) {
        vlc_gl_interop_DeleteTextures(sys->interop, sys->frames_in[i].textures);
    }
unexpected_tex_count:
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

static int
Open1x(vlc_object_t *obj)
{ return Open(obj, "gl_bwdif"); }

static int
Open2x(vlc_object_t *obj)
{ return Open(obj, "gl_bwdif2x"); }

vlc_module_begin()
    set_shortname("bwdif")
    set_description("OpenGL btv bwdif deinterlace filter")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)

    add_submodule()
        set_capability("video filter", 1)
        set_callback(Open1x)
        add_shortcut("deinterlace", "glbwdif", "gl_bwdif")

    add_submodule()
        set_capability("video filter", 1)
        set_callback(Open2x)
        add_shortcut("deinterlace", "glbwdif2x", "gl_bwdif2x")
vlc_module_end()
