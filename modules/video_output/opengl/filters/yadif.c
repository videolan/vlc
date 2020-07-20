/*****************************************************************************
 * yadif.c
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

#include <vlc_common.h>
#include <vlc_picture.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_opengl.h>
#include <vlc_filter.h>

#include "../filter.h"
#include "../gl_api.h"
#include "../gl_common.h"
#include "../gl_util.h"

struct program_copy {
    GLuint id;
    GLuint vbo;
    GLuint framebuffer;

    struct {
        GLint vertex_pos;
    } loc;
};

struct program_yadif {
    GLuint id;
    GLuint vbo;

    struct {
        GLint vertex_pos;
        GLint prev;
        GLint cur;
        GLint next;
        GLint width;
        GLint height;
        GLint order;
        GLint field;
    } loc;
};

struct plane {
    /* prev, current and next */
    GLuint textures[3];
};

struct sys {
    struct program_copy program_copy;
    struct program_yadif program_yadif;

    struct vlc_gl_sampler *sampler; /* weak reference */

    struct plane planes[PICTURE_PLANE_MAX];
    unsigned next; /* next texture index */

    /* In theory, 3 frames are needed.
     * If we only received the first frame, 2 are missing.
     * If we only received the two first frames, 1 is missing.
     */
    unsigned missing_frames;

    bool is_yadif2x;
    unsigned order;

    vlc_tick_t last_pts;
};

static void
CopyInput(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;
    const opengl_vtable_t *vt = &filter->api->vt;
    struct program_copy *prog = &sys->program_copy;

    vt->UseProgram(prog->id);

    vlc_gl_sampler_Load(sys->sampler);

    vt->BindBuffer(GL_ARRAY_BUFFER, prog->vbo);
    vt->EnableVertexAttribArray(prog->loc.vertex_pos);
    vt->VertexAttribPointer(prog->loc.vertex_pos, 2, GL_FLOAT, GL_FALSE, 0,
                            (const void *) 0);

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static int
InitProgramCopy(struct vlc_gl_filter *filter, const char *shader_version,
                const char *shader_precision)
{

    static const char *const VERTEX_SHADER =
        /* SHADER_VERSION */
        "attribute vec2 vertex_pos;\n"
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  gl_Position = vec4(vertex_pos, 0.0, 1.0);\n"
        "  tex_coords = vec2((vertex_pos.x + 1.0) / 2.0,\n"
        "                    (vertex_pos.y + 1.0) / 2.0);\n"
        "}\n";

    static const char *const FRAGMENT_SHADER_TEMPLATE =
        /* SHADER_VERSION */
        /* extensions */
        /* FRAGMENT_SHADER_PRECISION */
        /* vlc_texture definition */
        "varying vec2 tex_coords;\n"
        "void main() {\n"
        "  gl_FragColor = vlc_texture(tex_coords);\n"
        "}\n";

    struct sys *sys = filter->sys;
    struct program_copy *prog = &sys->program_copy;
    const opengl_vtable_t *vt = &filter->api->vt;

    struct vlc_gl_sampler *sampler = vlc_gl_filter_GetSampler(filter);
    assert(sampler);

    const char *extensions = sampler->shader.extensions
                           ? sampler->shader.extensions : "";

    const char * const vertex_code[] = {
        shader_version,
        VERTEX_SHADER,
    };

    const char * const fragment_code[] = {
        shader_version,
        extensions,
        shader_precision,
        sampler->shader.body,
        FRAGMENT_SHADER_TEMPLATE,
    };

    GLuint program_id =
        vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                            ARRAY_SIZE(vertex_code), vertex_code,
                            ARRAY_SIZE(fragment_code), fragment_code);

    if (!program_id)
        return VLC_EGENERIC;

    vlc_gl_sampler_FetchLocations(sampler, program_id);

    prog->id = program_id;

    prog->loc.vertex_pos = vt->GetAttribLocation(program_id, "vertex_pos");
    assert(prog->loc.vertex_pos != -1);

    vt->GenBuffers(1, &prog->vbo);
    vt->GenFramebuffers(1, &prog->framebuffer);

    static const GLfloat vertex_pos[] = {
        -1,  1,
        -1, -1,
         1,  1,
         1, -1,
    };

    vt->BindBuffer(GL_ARRAY_BUFFER, prog->vbo);
    vt->BufferData(GL_ARRAY_BUFFER, sizeof(vertex_pos), vertex_pos,
                   GL_STATIC_DRAW);

    vt->BindBuffer(GL_ARRAY_BUFFER, 0);

    return VLC_SUCCESS;
}

static int
InitProgramYadif(struct vlc_gl_filter *filter, const char *shader_version,
                 const char *shader_precision)
{
    static const char *const VERTEX_SHADER =
        /* SHADER_VERSION */
        "attribute vec2 vertex_pos;\n"
        "void main() {\n"
        "  gl_Position = vec4(vertex_pos, 0.0, 1.0);\n"
        "}\n";

    // mrefs = y+1
    // prefs = y-1
    // prev2 = prev
    // next2 = cur
    static const char *const FRAGMENT_SHADER =
        /* SHADER_VERSION */
        /* FRAGMENT_SHADER_PRECISION */
        "uniform sampler2D prev;\n"
        "uniform sampler2D cur;\n"
        "uniform sampler2D next;\n"
        "uniform float width;\n"
        "uniform float height;\n"
        "uniform int order;\n"
        "uniform int field;\n"
        "\n"
        "float pix(sampler2D sampler, float x, float y) {\n"
        "  return texture2D(sampler, vec2(x / width, y / height)).x;\n"
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
        "  if (order == 0) {\n"
        "    prev2_pix = prev_pix;\n"
        "    next2_pix = cur_pix;\n"
        "  } else {\n"
        "    prev2_pix = cur_pix;\n"
        "    next2_pix = next_pix;\n"
        "  }\n"
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
        "  float spatial_pred = (c+e) / 2.0;\n"
        "  float spatial_score = abs(pix(cur, x-1.0, y+1.0)\n"
        "                          - pix(cur, x-1.0, y-1.0)) + abs(c-e)\n"
        "                      + abs(pix(cur, x+1.0, y+1.0)\n"
        "                          - pix(cur, x+1.0, y-1.0)) - 1.0/256.0;\n"
        "  float score;\n"
        "  score = compute_score(x, y, -1.0);\n"
        "  if (score < spatial_score) {\n"
        "    spatial_score = score;\n"
        "    spatial_pred = compute_pred(x, y, -1.0);\n"
        "    score = compute_score(x, y, -2.0);\n"
        "    if (score < spatial_score) {\n"
        "      spatial_score = score;\n"
        "      spatial_pred = compute_pred(x, y, -2.0);\n"
        "    }\n"
        "  }\n"
        "  score = compute_score(x, y, 1.0);\n"
        "  if (score < spatial_score) {\n"
        "    spatial_score = score;\n"
        "    spatial_pred = compute_pred(x, y, 1.0);\n"
        "    score = compute_score(x, y, 2.0);\n"
        "    if (score < spatial_score) {\n"
        "       spatial_score = score;\n"
        "       spatial_pred = compute_pred(x, y, 2.0);\n"
        "    }\n"
        "  }\n"
        "\n"
           // if mode < 2
        "  float b = (pix(prev2, x, y+2.0) + pix(next2, x, y+2.0)) / 2.0;\n"
        "  float f = (pix(prev2, x, y-2.0) + pix(next2, x, y-2.0)) / 2.0;\n"
        "  float vmax = max(max(d-e, d-c),\n"
        "                   min(b-c, f-e));\n"
        "  float vmin = min(min(d-e, d-c),\n"
        "                   max(b-c, f-e));\n"
        "  diff = max(diff, max(vmin, -vmax));\n"
           // endif
        "\n"
        "  spatial_pred = min(spatial_pred, d + diff);\n"
        "  spatial_pred = max(spatial_pred, d - diff);\n"
        "  return spatial_pred;\n"
        "}\n"
        "\n"
        "float filter(float x, float y) {\n"
        "  if (order == 0) {\n"
        "    return filter_internal(x, y, prev, cur);\n"
        "  }\n"
        "  return filter_internal(x, y, cur, next);\n"
        "}\n"
        "\n"
        "void main() {\n"
           /* bottom-left is (0.5, 0.5)
              top-right is (width-0.5, height-0.5) */
        "  float x = gl_FragCoord.x;\n"
        "  float y = gl_FragCoord.y;\n"
        /* The line number, expressed in non-flipped coordinates */
        "  float line = floor(height - y);\n"
        "\n"
        "  float result;\n"
        "  if (int(mod(line, 2.0)) == field) {\n"
        "    result = pix(cur, x, y);\n"
        "  } else {\n"
        "    result = filter(x, y);\n"
        "  }\n"
        "  gl_FragColor = vec4(result, 0.0, 0.0, 1.0);\n"
        "}\n";

    printf("====\n%s\n====\n", FRAGMENT_SHADER);

    struct sys *sys = filter->sys;
    struct program_yadif *prog = &sys->program_yadif;
    const opengl_vtable_t *vt = &filter->api->vt;

    const char * const vertex_code[] =
        { shader_version, VERTEX_SHADER };

    const char * const fragment_code[] =
        { shader_version, shader_precision, FRAGMENT_SHADER };

    GLuint program_id =
        vlc_gl_BuildProgram(VLC_OBJECT(filter), vt,
                            ARRAY_SIZE(vertex_code), vertex_code,
                            ARRAY_SIZE(fragment_code), fragment_code);

    if (!program_id)
        return VLC_EGENERIC;

    prog->id = program_id;

    prog->loc.vertex_pos = vt->GetAttribLocation(program_id, "vertex_pos");
    assert(prog->loc.vertex_pos != -1);

    prog->loc.prev = vt->GetUniformLocation(program_id, "prev");
    assert(prog->loc.prev != -1);

    prog->loc.cur = vt->GetUniformLocation(program_id, "cur");
    assert(prog->loc.cur != -1);

    prog->loc.next = vt->GetUniformLocation(program_id, "next");
    assert(prog->loc.next != -1);

    prog->loc.width = vt->GetUniformLocation(program_id, "width");
    assert(prog->loc.width != -1);

    prog->loc.height = vt->GetUniformLocation(program_id, "height");
    assert(prog->loc.height != -1);

    prog->loc.order = vt->GetUniformLocation(program_id, "order");
    assert(prog->loc.order != -1);

    prog->loc.field = vt->GetUniformLocation(program_id, "field");
    assert(prog->loc.field != -1);

    vt->GenBuffers(1, &prog->vbo);

    static const GLfloat vertex_pos[] = {
        -1,  1,
        -1, -1,
         1,  1,
         1, -1,
    };

    vt->BindBuffer(GL_ARRAY_BUFFER, prog->vbo);
    vt->BufferData(GL_ARRAY_BUFFER, sizeof(vertex_pos), vertex_pos,
                   GL_STATIC_DRAW);

    vt->BindBuffer(GL_ARRAY_BUFFER, 0);

    return VLC_SUCCESS;
}

static void
InitPlane(struct vlc_gl_filter *filter, unsigned plane_idx, GLsizei width,
          GLsizei height)
{
    struct sys *sys = filter->sys;
    const opengl_vtable_t *vt = &filter->api->vt;
    struct plane *plane = &sys->planes[plane_idx];

    vt->GenTextures(3, plane->textures);
    for (int i = 0; i < 3; ++i)
    {
        vt->BindTexture(GL_TEXTURE_2D, plane->textures[i]);
        vt->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                       GL_UNSIGNED_BYTE, NULL);
        vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        vt->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

static void
InitPlanes(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;
    struct vlc_gl_sampler *sampler = sys->sampler;

    for (unsigned i = 0; i < sampler->tex_count; ++i)
        InitPlane(filter, i, sampler->tex_widths[i], sampler->tex_heights[i]);
}

static void
DestroyPlanes(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;
    const opengl_vtable_t *vt = &filter->api->vt;
    struct vlc_gl_sampler *sampler = sys->sampler;

    for (unsigned i = 0; i < sampler->tex_count; ++i)
    {
        struct plane *plane = &sys->planes[i];
        vt->DeleteTextures(3, plane->textures);
    }
}

static void
DestroyProgramCopy(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;
    struct program_copy *prog = &sys->program_copy;
    const opengl_vtable_t *vt = &filter->api->vt;

    vt->DeleteProgram(prog->id);
    vt->DeleteFramebuffers(1, &prog->framebuffer);
    vt->DeleteBuffers(1, &prog->vbo);
}

static void
DestroyProgramYadif(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;
    struct program_yadif *prog = &sys->program_yadif;
    const opengl_vtable_t *vt = &filter->api->vt;
    vt->DeleteProgram(prog->id);
    vt->DeleteBuffers(1, &prog->vbo);
}

static void
Close(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;

    DestroyPlanes(filter);
    DestroyProgramYadif(filter);
    DestroyProgramCopy(filter);

    free(sys);
}

static void
Flush(struct vlc_gl_filter *filter)
{
    struct sys *sys = filter->sys;
    /* The next call to Draw will provide the "next" frame. The "prev" and
     * "cur" frames are missing. */
    sys->missing_frames = 2;
}

static inline GLuint
GetDrawFramebuffer(const opengl_vtable_t *vt)
{
    GLint value;
    vt->GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    return value; /* as GLuint */
}

static bool
WillUpdate(struct vlc_gl_filter *filter, bool new_frame)
{
    struct sys *sys = filter->sys;

    if (!sys->is_yadif2x)
        return new_frame;

    return new_frame || sys->order == 1;
}

static int
Draw(struct vlc_gl_filter *filter, struct vlc_gl_input_meta *meta)
{
    struct sys *sys = filter->sys;
    const opengl_vtable_t *vt = &filter->api->vt;

    struct plane *plane = &sys->planes[meta->plane];

    struct program_yadif *prog = &sys->program_yadif;
    unsigned next = sys->next;
    unsigned prev = (next + 1) % 3;
    unsigned cur = (next + 2) % 3;

    GLuint draw_fb = GetDrawFramebuffer(vt);
    vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, sys->program_copy.framebuffer);
    vt->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, plane->textures[next], 0);

    CopyInput(filter);

    vt->BindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fb);

    vt->UseProgram(prog->id);

    struct vlc_gl_sampler *sampler = sys->sampler;
    GLsizei width = sampler->tex_widths[meta->plane];
    GLsizei height = sampler->tex_heights[meta->plane];

    assert(sys->order == 0 || sys->order == 1);
    vt->Uniform1i(prog->loc.order, sys->order);

    /**
     * order == 0 &&  top_field_first  ==>  field = 0
     * order == 0 && !top_field_first  ==>  field = 1
     * order == 1 &&  top_field_first  ==>  field = 1
     * order == 1 && !top_field_first  ==>  field = 0
     */
    unsigned field = sys->order ^ !meta->top_field_first;
    assert(field == 0 || field == 1);

    vt->Uniform1i(prog->loc.field, field);

    vt->Uniform1f(prog->loc.width, width);
    vt->Uniform1f(prog->loc.height, height);

    vt->ActiveTexture(GL_TEXTURE0);
    vt->BindTexture(GL_TEXTURE_2D, plane->textures[prev]);
    vt->Uniform1i(prog->loc.prev, 0);

    vt->ActiveTexture(GL_TEXTURE1);
    vt->BindTexture(GL_TEXTURE_2D, plane->textures[cur]);
    vt->Uniform1i(prog->loc.cur, 1);

    vt->ActiveTexture(GL_TEXTURE2);
    vt->BindTexture(GL_TEXTURE_2D, plane->textures[next]);
    vt->Uniform1i(prog->loc.next, 2);

    vt->BindBuffer(GL_ARRAY_BUFFER, prog->vbo);
    vt->EnableVertexAttribArray(prog->loc.vertex_pos);
    vt->VertexAttribPointer(prog->loc.vertex_pos, 2, GL_FLOAT, GL_FALSE, 0,
                            (const void *) 0);

    vt->Clear(GL_COLOR_BUFFER_BIT);
    vt->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (sys->is_yadif2x)
        sys->order ^= 1; /* alternate between 0 and 1 */

    if (meta->plane == 2 && sys->order == 0) {
        /* This was the last pass of the last plane */
        sys->next = prev; /* rotate */

        if (sys->missing_frames)
        {
            if (sys->missing_frames == 2)
                /* cur is missing */
                cur = next;
            /* prev is missing */
            prev = cur;
            --sys->missing_frames;
        }

        if (false && sys->last_pts != VLC_TICK_INVALID)
        {
            /*
             *                       dup->date
             *                       v
             *        |----.----|----.----|
             *        ^         ^
             * last_pts       pic->date
             */
            meta->pts = (3 * meta->pts - sys->last_pts) / 2;
        }
        else if (meta->framerate.den != 0)
        {
            vlc_tick_t interval =
                vlc_tick_from_samples(meta->framerate.den, meta->framerate.num * 2);
            meta->pts += interval;
        }
        else
        {
            /* What could we do? */
            meta->pts += 1;
        }
    }
    sys->last_pts = meta->pts;

    return VLC_SUCCESS;
}

static int
Open(struct vlc_gl_filter *filter, const config_chain_t *config,
     struct vlc_gl_tex_size *size_out, bool is_yadif2x)
{
    (void) config;
    (void) size_out;

    struct sys *sys = filter->sys = malloc(sizeof(*sys));
    if (!sys)
        return VLC_EGENERIC;

    sys->next = 0;
    /* The first call to Draw will provide the "next" frame. The "prev" and
     * "cur" frames are missing. */
    sys->missing_frames = 2;
    sys->order = 0;
    sys->last_pts = VLC_TICK_INVALID;

    static const struct vlc_gl_filter_ops ops = {
        .will_update = WillUpdate,
        .draw = Draw,
        .flush = Flush,
        .close = Close,
    };
    filter->ops = &ops;
    filter->config.filter_planes = true;

    sys->is_yadif2x = is_yadif2x;

    sys->sampler = vlc_gl_filter_GetSampler(filter);
    assert(sys->sampler);

    if (sys->sampler->tex_count != 3) {
        msg_Err(filter, "Deinterlace assumes 1 component per plane");
        return VLC_EGENERIC;
    }

    const char *shader_version;
    const char *shader_precision;

    if (filter->api->is_gles)
    {
        shader_version = "#version 100\n";
        shader_precision = "precision highp float;\n";
    }
    else
    {
        shader_version = "#version 120\n";
        shader_precision = "";
    }

    int ret = InitProgramCopy(filter, shader_version, shader_precision);
    if (ret != VLC_SUCCESS)
        goto error1;

    ret = InitProgramYadif(filter, shader_version, shader_precision);
    if (ret != VLC_SUCCESS)
        goto error2;

    InitPlanes(filter);

    return VLC_SUCCESS;

error2:
    DestroyProgramCopy(filter);
error1:
    free(sys);
    return VLC_EGENERIC;
}

/* Ensure callback have the correct prototype */
static vlc_gl_filter_open_fn OpenYadif;
static vlc_gl_filter_open_fn OpenYadif2x;

static int
OpenYadif(struct vlc_gl_filter *filter, const config_chain_t *config,
          struct vlc_gl_tex_size *size_out)
{
    return Open(filter, config, size_out, false);
}

static int
OpenYadif2x(struct vlc_gl_filter *filter, const config_chain_t *config,
          struct vlc_gl_tex_size *size_out)
{
    return Open(filter, config, size_out, true);
}

static int OpenVideoFilter(vlc_object_t *obj, const char *name)
{
    filter_t *filter = (filter_t*)obj;

    char *mode = var_InheritString(obj, "deinterlace-mode");
    if (mode && strcmp(mode, "auto") && strcmp(mode, name))
        return VLC_EGENERIC;

    config_chain_t *prev_chain = filter->p_cfg;
    var_Create(filter, "opengl-filter", VLC_VAR_STRING);
    var_SetString(filter, "opengl-filter", name);

    filter->p_cfg = NULL;
    module_t *module = module_need(obj, "video filter", "opengl", true);
    filter->p_cfg = prev_chain;

    var_Destroy(filter, "opengl-filter");

    return module == NULL ? VLC_EGENERIC : VLC_SUCCESS;
}

static int OpenVideoFilterYadif(vlc_object_t *obj)
{
    return OpenVideoFilter(obj, "gl_yadif");
}

static int OpenVideoFilterYadif2x(vlc_object_t *obj)
{
    int ret = OpenVideoFilter(obj, "gl_yadif2x");

    if (ret != VLC_SUCCESS)
        return ret;

    filter_t *filter = (filter_t*)obj;
    filter->fmt_out.video.i_frame_rate *= 2;

    // TODO
    //if (sys->is_yadif2x)
    //{
    //    vlc_tick_t last_pts = sys->yadif2x.last_pts;
    //    sys->yadif2x.last_pts = pic->date;

    //    ret = vlc_gl_filters_Draw(sys->filters);
    //    if (ret != VLC_SUCCESS)
    //        goto end;

    //    picture_t *second = vlc_gl_Swap(sys->gl);
    //    if (second)
    //    {
    //        if (last_pts != VLC_TICK_INVALID)
    //        {
    //            /*
    //             *                       dup->date
    //             *                       v
    //             *        |----.----|----.----|
    //             *        ^         ^
    //             * last_pts       pic->date
    //             */
    //            second->date = (3 * pic->date - last_pts) / 2;
    //        }
    //        else if (filter->fmt_in.video.i_frame_rate != 0)
    //        {
    //            video_format_t *fmt = &filter->fmt_in.video;
    //            vlc_tick_t interval =
    //                vlc_tick_from_samples(fmt->i_frame_rate_base, fmt->i_frame_rate);
    //            second->date = pic->date + interval;
    //        }
    //        else
    //        {
    //            /* What could we do? */
    //            second->date = pic->date + 1;
    //        }

    //        output->p_next = second;
    //    }
    //}

    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("yadif")
    set_description("OpenGL yadif deinterlace filter")
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VFILTER)

    add_submodule()
        set_capability("opengl filter", 0)
        set_callback(OpenYadif)
        add_shortcut("gl_yadif")

    add_submodule()
        set_capability("opengl filter", 0)
        set_callback(OpenYadif2x)
        add_shortcut("gl_yadif2x")

    add_submodule()
        set_capability("video filter", 1)
        set_callback(OpenVideoFilterYadif)
        add_shortcut("deinterlace", "gl_yadif")

    add_submodule()
        set_capability("video filter", 1)
        set_callback(OpenVideoFilterYadif2x)
        add_shortcut("deinterlace", "gl_yadif2x")
vlc_module_end()
