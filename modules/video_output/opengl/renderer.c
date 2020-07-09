/*****************************************************************************
 * renderer.c
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

#include "renderer.h"

#include <assert.h>
#include <vlc_common.h>
#include <vlc_es.h>
#include <vlc_picture.h>

#include "gl_util.h"
#include "internal.h"
#include "vout_helper.h"

#define SPHERE_RADIUS 1.f

static void getZoomMatrix(float zoom, GLfloat matrix[static 16]) {

    const GLfloat m[] = {
        /* x   y     z     w */
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, zoom, 1.0f
    };

    memcpy(matrix, m, sizeof(m));
}

/* perspective matrix see https://www.opengl.org/sdk/docs/man2/xhtml/gluPerspective.xml */
static void getProjectionMatrix(float sar, float fovy, GLfloat matrix[static 16]) {

    float zFar  = 1000;
    float zNear = 0.01;

    float f = 1.f / tanf(fovy / 2.f);

    const GLfloat m[] = {
        f / sar, 0.f,                   0.f,                0.f,
        0.f,     f,                     0.f,                0.f,
        0.f,     0.f,     (zNear + zFar) / (zNear - zFar), -1.f,
        0.f,     0.f, (2 * zNear * zFar) / (zNear - zFar),  0.f};

     memcpy(matrix, m, sizeof(m));
}

static void getViewpointMatrixes(struct vlc_gl_renderer *renderer,
                                 video_projection_mode_t projection_mode)
{
    if (projection_mode == PROJECTION_MODE_EQUIRECTANGULAR
        || projection_mode == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD)
    {
        getProjectionMatrix(renderer->f_sar, renderer->f_fovy,
                            renderer->var.ProjectionMatrix);
        getZoomMatrix(renderer->f_z, renderer->var.ZoomMatrix);

        /* renderer->vp has been reversed and is a world transform */
        vlc_viewpoint_to_4x4(&renderer->vp, renderer->var.ViewMatrix);
    }
    else
    {
        memcpy(renderer->var.ProjectionMatrix, MATRIX4_IDENTITY,
                                               sizeof(MATRIX4_IDENTITY));
        memcpy(renderer->var.ZoomMatrix, MATRIX4_IDENTITY,
                                         sizeof(MATRIX4_IDENTITY));
        memcpy(renderer->var.ViewMatrix, MATRIX4_IDENTITY,
                                         sizeof(MATRIX4_IDENTITY));
    }

}

static void
InitStereoMatrix(GLfloat matrix_out[static 3*3],
                 video_multiview_mode_t multiview_mode)
{
    /*
     * The stereo matrix transforms 2D pictures coordinates to crop the
     * content, in order to view only one eye.
     *
     * This 2D transformation is affine, so the matrix is 3x3 and applies to 3D
     * vectors in the form (x, y, 1).
     *
     * Note that since for now, we always crop the left eye, in practice the
     * offset is always 0, so the transform is actually linear (a 2x2 matrix
     * would be sufficient).
     */

    memcpy(matrix_out, MATRIX3_IDENTITY, sizeof(MATRIX3_IDENTITY));

#define COL(x) (x*3)
#define ROW(x) (x)

    switch (multiview_mode)
    {
        case MULTIVIEW_STEREO_SBS:
            /*
             * +----------+----------+
             * |          .          |
             * |  LEFT    .   RIGHT  |
             * |  EYE     .     EYE  |
             * |          .          |
             * +----------+----------+
             *
             * To crop the coordinates to the left eye, divide the x
             * coordinates by 2:
             *
             *            / 0.5  0    0 \
             *  matrix =  | 0    1    0 |
             *            \ 0    0    1 /
             */
            matrix_out[COL(0) + ROW(0)] = 0.5;
            break;
        case MULTIVIEW_STEREO_TB:
            /*
             * +----------+
             * |          |
             * |  LEFT    |
             * |  EYE     |
             * |          |
             * +..........+
             * |          |
             * |   RIGHT  |
             * |     EYE  |
             * |          |
             * +----------+
             *
             * To crop the coordinates to the left eye, divide the y
             * coordinates by 2:
             *
             *            / 1    0    0 \
             *  matrix =  | 0    0.5  0 |
             *            \ 0    0    1 /
             */
            matrix_out[COL(1) + ROW(1)] = 0.5;
            break;
        default:
            break;
    }
#undef COL
#undef ROW
}

/* https://en.wikipedia.org/wiki/OpenGL_Shading_Language#Versions */
#ifdef USE_OPENGL_ES2
# define SHADER_VERSION "#version 100\n"
  /* In OpenGL ES, the fragment language has no default precision qualifier for
   * floating point types. */
# define FRAGMENT_SHADER_PRECISION "precision highp float;\n"
#else
# define SHADER_VERSION "#version 120\n"
# define FRAGMENT_SHADER_PRECISION
#endif

static char *
BuildVertexShader(const struct vlc_gl_renderer *renderer)
{
    /* Basic vertex shader */
    static const char *template =
        SHADER_VERSION
        "attribute vec2 PicCoordsIn;\n"
        "varying vec2 PicCoords;\n"
        "attribute vec3 VertexPosition;\n"
        "uniform mat3 StereoMatrix;\n"
        "uniform mat4 ProjectionMatrix;\n"
        "uniform mat4 ZoomMatrix;\n"
        "uniform mat4 ViewMatrix;\n"
        "void main() {\n"
        " PicCoords = (StereoMatrix * vec3(PicCoordsIn, 1.0)).st;\n"
        " gl_Position = ProjectionMatrix * ZoomMatrix * ViewMatrix\n"
        "               * vec4(VertexPosition, 1.0);\n"
        "}";

    char *code = strdup(template);
    if (!code)
        return NULL;

    if (renderer->dump_shaders)
        msg_Dbg(renderer->gl, "\n=== Vertex shader for fourcc: %4.4s ===\n%s\n",
                (const char *) &renderer->sampler->fmt->i_chroma, code);
    return code;
}

static char *
BuildFragmentShader(struct vlc_gl_renderer *renderer)
{
    struct vlc_gl_sampler *sampler = renderer->sampler;

    static const char *template =
        SHADER_VERSION
        "%s" /* extensions */
        FRAGMENT_SHADER_PRECISION
        "%s" /* vlc_texture definition */
        "varying vec2 PicCoords;\n"
        "void main() {\n"
        " gl_FragColor = vlc_texture(PicCoords);\n"
        "}\n";

    const char *extensions = sampler->shader.extensions
                           ? sampler->shader.extensions : "";

    char *code;
    int ret = asprintf(&code, template, extensions, sampler->shader.body);
    if (ret < 0)
        return NULL;

    if (renderer->dump_shaders)
        msg_Dbg(renderer->gl, "\n=== Fragment shader for fourcc: %4.4s, colorspace: %d ===\n%s\n",
                              (const char *) &sampler->fmt->i_chroma,
                              sampler->fmt->space, code);

    return code;
}

static int
opengl_link_program(struct vlc_gl_renderer *renderer)
{
    struct vlc_gl_sampler *sampler = renderer->sampler;
    const opengl_vtable_t *vt = renderer->vt;

    char *vertex_shader = BuildVertexShader(renderer);
    if (!vertex_shader)
        return VLC_EGENERIC;

    char *fragment_shader = BuildFragmentShader(renderer);
    if (!fragment_shader)
    {
        free(vertex_shader);
        return VLC_EGENERIC;
    }

    assert(sampler->ops &&
           sampler->ops->fetch_locations &&
           sampler->ops->load);

    GLuint program_id =
        vlc_gl_BuildProgram(VLC_OBJECT(renderer->gl), vt,
                            1, (const char **) &vertex_shader,
                            1, (const char **) &fragment_shader);
    free(vertex_shader);
    free(fragment_shader);
    if (!program_id)
        return VLC_EGENERIC;

    /* Fetch UniformLocations and AttribLocations */
#define GET_LOC(type, x, str) do { \
    x = vt->Get##type##Location(program_id, str); \
    assert(x != -1); \
    if (x == -1) { \
        msg_Err(renderer->gl, "Unable to Get"#type"Location(%s)", str); \
        goto error; \
    } \
} while (0)
#define GET_ULOC(x, str) GET_LOC(Uniform, renderer->uloc.x, str)
#define GET_ALOC(x, str) GET_LOC(Attrib, renderer->aloc.x, str)
    GET_ULOC(StereoMatrix, "StereoMatrix");
    GET_ULOC(ProjectionMatrix, "ProjectionMatrix");
    GET_ULOC(ViewMatrix, "ViewMatrix");
    GET_ULOC(ZoomMatrix, "ZoomMatrix");

    GET_ALOC(PicCoordsIn, "PicCoordsIn");
    GET_ALOC(VertexPosition, "VertexPosition");
#undef GET_LOC
#undef GET_ULOC
#undef GET_ALOC

    vlc_gl_sampler_FetchLocations(sampler, program_id);

    renderer->program_id = program_id;

    return VLC_SUCCESS;

error:
    vt->DeleteProgram(program_id);
    renderer->program_id = 0;
    return VLC_EGENERIC;
}

void
vlc_gl_renderer_Delete(struct vlc_gl_renderer *renderer)
{
    const opengl_vtable_t *vt = renderer->vt;

    vt->DeleteBuffers(1, &renderer->vertex_buffer_object);
    vt->DeleteBuffers(1, &renderer->index_buffer_object);
    vt->DeleteBuffers(1, &renderer->texture_buffer_object);

    if (renderer->program_id != 0)
        vt->DeleteProgram(renderer->program_id);

    free(renderer);
}

static int SetupCoords(struct vlc_gl_renderer *renderer);

struct vlc_gl_renderer *
vlc_gl_renderer_New(vlc_gl_t *gl, const struct vlc_gl_api *api,
                    struct vlc_gl_sampler *sampler)
{
    const opengl_vtable_t *vt = &api->vt;
    const video_format_t *fmt = sampler->fmt;

    struct vlc_gl_renderer *renderer = calloc(1, sizeof(*renderer));
    if (!renderer)
        return NULL;

    renderer->sampler = sampler;

    renderer->gl = gl;
    renderer->api = api;
    renderer->vt = vt;
    renderer->dump_shaders = var_InheritInteger(gl, "verbose") >= 4;

    int ret = opengl_link_program(renderer);
    if (ret != VLC_SUCCESS)
    {
        vlc_gl_renderer_Delete(renderer);
        return NULL;
    }

    InitStereoMatrix(renderer->var.StereoMatrix, fmt->multiview_mode);

    getViewpointMatrixes(renderer, fmt->projection_mode);

    /* */
    vt->Disable(GL_BLEND);
    vt->Disable(GL_DEPTH_TEST);
    vt->DepthMask(GL_FALSE);
    vt->Enable(GL_CULL_FACE);
    vt->ClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    vt->GenBuffers(1, &renderer->vertex_buffer_object);
    vt->GenBuffers(1, &renderer->index_buffer_object);
    vt->GenBuffers(1, &renderer->texture_buffer_object);

    ret = SetupCoords(renderer);
    if (ret != VLC_SUCCESS)
    {
        vlc_gl_renderer_Delete(renderer);
        return NULL;
    }

    return renderer;
}

static void UpdateZ(struct vlc_gl_renderer *renderer)
{
    /* Do trigonometry to calculate the minimal z value
     * that will allow us to zoom out without seeing the outside of the
     * sphere (black borders). */
    float tan_fovx_2 = tanf(renderer->f_fovx / 2);
    float tan_fovy_2 = tanf(renderer->f_fovy / 2);
    float z_min = - SPHERE_RADIUS / sinf(atanf(sqrtf(
                    tan_fovx_2 * tan_fovx_2 + tan_fovy_2 * tan_fovy_2)));

    /* The FOV value above which z is dynamically calculated. */
    const float z_thresh = 90.f;

    if (renderer->f_fovx <= z_thresh * M_PI / 180)
        renderer->f_z = 0;
    else
    {
        float f = z_min / ((FIELD_OF_VIEW_DEGREES_MAX - z_thresh) * M_PI / 180);
        renderer->f_z = f * renderer->f_fovx - f * z_thresh * M_PI / 180;
        if (renderer->f_z < z_min)
            renderer->f_z = z_min;
    }
}

static void UpdateFOVy(struct vlc_gl_renderer *renderer)
{
    renderer->f_fovy = 2 * atanf(tanf(renderer->f_fovx / 2) / renderer->f_sar);
}

int
vlc_gl_renderer_SetViewpoint(struct vlc_gl_renderer *renderer,
                             const vlc_viewpoint_t *p_vp)
{
    if (p_vp->fov > FIELD_OF_VIEW_DEGREES_MAX
            || p_vp->fov < FIELD_OF_VIEW_DEGREES_MIN)
        return VLC_EBADVAR;

    // Convert degree into radian
    float f_fovx = p_vp->fov * (float)M_PI / 180.f;

    /* vgl->vp needs to be converted into world transform */
    vlc_viewpoint_reverse(&renderer->vp, p_vp);

    if (fabsf(f_fovx - renderer->f_fovx) >= 0.001f)
    {
        /* FOVx has changed. */
        renderer->f_fovx = f_fovx;
        UpdateFOVy(renderer);
        UpdateZ(renderer);
    }
    const video_format_t *fmt = renderer->sampler->fmt;
    getViewpointMatrixes(renderer, fmt->projection_mode);

    return VLC_SUCCESS;
}

void
vlc_gl_renderer_SetWindowAspectRatio(struct vlc_gl_renderer *renderer,
                                     float f_sar)
{
    /* Each time the window size changes, we must recompute the minimum zoom
     * since the aspect ration changes.
     * We must also set the new current zoom value. */
    renderer->f_sar = f_sar;
    UpdateFOVy(renderer);
    UpdateZ(renderer);

    const video_format_t *fmt = renderer->sampler->fmt;
    getViewpointMatrixes(renderer, fmt->projection_mode);
}

static int BuildSphere(GLfloat **vertexCoord, GLfloat **textureCoord, unsigned *nbVertices,
                       GLushort **indices, unsigned *nbIndices)
{
    unsigned nbLatBands = 128;
    unsigned nbLonBands = 128;

    *nbVertices = (nbLatBands + 1) * (nbLonBands + 1);
    *nbIndices = nbLatBands * nbLonBands * 3 * 2;

    *vertexCoord = vlc_alloc(*nbVertices * 3, sizeof(GLfloat));
    if (*vertexCoord == NULL)
        return VLC_ENOMEM;
    *textureCoord = vlc_alloc(*nbVertices * 2, sizeof(GLfloat));
    if (*textureCoord == NULL)
    {
        free(*vertexCoord);
        return VLC_ENOMEM;
    }
    *indices = vlc_alloc(*nbIndices, sizeof(GLushort));
    if (*indices == NULL)
    {
        free(*textureCoord);
        free(*vertexCoord);
        return VLC_ENOMEM;
    }

    for (unsigned lat = 0; lat <= nbLatBands; lat++) {
        float theta = lat * (float) M_PI / nbLatBands;
        float sinTheta, cosTheta;

        sincosf(theta, &sinTheta, &cosTheta);

        for (unsigned lon = 0; lon <= nbLonBands; lon++) {
            float phi = lon * 2 * (float) M_PI / nbLonBands;
            float sinPhi, cosPhi;

            sincosf(phi, &sinPhi, &cosPhi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            unsigned off1 = (lat * (nbLonBands + 1) + lon) * 3;
            (*vertexCoord)[off1] = SPHERE_RADIUS * x;
            (*vertexCoord)[off1 + 1] = SPHERE_RADIUS * y;
            (*vertexCoord)[off1 + 2] = SPHERE_RADIUS * z;

            unsigned off2 = (lat * (nbLonBands + 1) + lon) * 2;
            float u = (float)lon / nbLonBands;
            /* In OpenGL, the texture coordinates start at bottom left */
            float v = 1.0f - (float)lat / nbLatBands;
            (*textureCoord)[off2] = u;
            (*textureCoord)[off2 + 1] = v;
        }
    }

    for (unsigned lat = 0; lat < nbLatBands; lat++) {
        for (unsigned lon = 0; lon < nbLonBands; lon++) {
            unsigned first = (lat * (nbLonBands + 1)) + lon;
            unsigned second = first + nbLonBands + 1;

            unsigned off = (lat * nbLatBands + lon) * 3 * 2;

            (*indices)[off] = first;
            (*indices)[off + 1] = second;
            (*indices)[off + 2] = first + 1;

            (*indices)[off + 3] = second;
            (*indices)[off + 4] = second + 1;
            (*indices)[off + 5] = first + 1;
        }
    }

    return VLC_SUCCESS;
}


static int BuildCube(float padW, float padH,
                     GLfloat **vertexCoord, GLfloat **textureCoord, unsigned *nbVertices,
                     GLushort **indices, unsigned *nbIndices)
{
    *nbVertices = 4 * 6;
    *nbIndices = 6 * 6;

    *vertexCoord = NULL;
    *textureCoord = NULL;
    *indices = NULL;

    *vertexCoord = vlc_alloc(*nbVertices * 3, sizeof(GLfloat));
    if (*vertexCoord == NULL)
        goto error;

    *textureCoord = vlc_alloc(*nbVertices * 2, sizeof(GLfloat));
    if (*textureCoord == NULL)
        goto error;

    *indices = vlc_alloc(*nbIndices, sizeof(GLushort));
    if (*indices == NULL)
        goto error;

#define CUBEFACE(swap, value) \
    swap(value, -1.f,  1.f), \
    swap(value, -1.f, -1.f), \
    swap(value,  1.f,  1.f), \
    swap(value,  1.f, -1.f)

#define X_FACE(v, a, b) (v), (a), (b)
#define Y_FACE(v, a, b) (a), (v), (b)
#define Z_FACE(v, a, b) (a), (b), (v)

    static const GLfloat coord[] = {
        CUBEFACE(X_FACE, -1.f), // FRONT
        CUBEFACE(X_FACE, +1.f), // BACK
        CUBEFACE(Z_FACE, +1.f), // LEFT
        CUBEFACE(Z_FACE, -1.f), // RIGHT
        CUBEFACE(Y_FACE, -1.f), // BOTTOM
        CUBEFACE(Y_FACE, +1.f), // TOP
    };

#undef X_FACE
#undef Y_FACE
#undef Z_FACE
#undef CUBEFACE

    memcpy(*vertexCoord, coord, *nbVertices * 3 * sizeof(GLfloat));

    float col[] = {0.f, 1.f/3, 2.f/3, 1.f};
    float row[] = {0.f, 1.f/2, 1.0};

    const GLfloat tex[] = {
        col[1] + padW, row[0] - padH, // front
        col[2] + padW, row[0] + padH,
        col[1] - padW, row[1] - padH,
        col[2] - padW, row[1] + padH,

        col[3] - padW, row[0] - padH, // back
        col[2] - padW, row[0] + padH,
        col[3] + padW, row[1] - padH,
        col[2] + padW, row[1] + padH,

        col[2] - padW, row[2] - padH, // left
        col[2] - padW, row[1] + padH,
        col[1] + padW, row[2] - padH,
        col[1] + padW, row[1] + padH,

        col[0] + padW, row[2] - padH, // right
        col[0] + padW, row[1] + padH,
        col[1] - padW, row[2] - padH,
        col[1] - padW, row[1] + padH,

        col[0] + padW, row[1] + padH, // bottom
        col[1] + padW, row[1] - padH,
        col[0] - padW, row[0] + padH,
        col[1] - padW, row[0] - padH,

        col[2] + padW, row[1] - padH, // top
        col[3] + padW, row[1] + padH,
        col[2] - padW, row[2] - padH,
        col[3] - padW, row[2] + padH,
    };

    memcpy(*textureCoord, tex,
           *nbVertices * 2 * sizeof(GLfloat));

    const GLushort ind[] = {
        0, 1, 2,       2, 1, 3, // front
        6, 7, 4,       4, 7, 5, // back
        10, 11, 8,     8, 11, 9, // left
        12, 13, 14,    14, 13, 15, // right
        18, 19, 16,    16, 19, 17, // bottom
        20, 21, 22,    22, 21, 23, // top
    };

    memcpy(*indices, ind, *nbIndices * sizeof(GLushort));

    return VLC_SUCCESS;

error:
    free(*vertexCoord);
    free(*textureCoord);
    return VLC_ENOMEM;
}

static int BuildRectangle(GLfloat **vertexCoord, GLfloat **textureCoord, unsigned *nbVertices,
                          GLushort **indices, unsigned *nbIndices)
{
    *nbVertices = 4;
    *nbIndices = 6;

    *vertexCoord = vlc_alloc(*nbVertices * 3, sizeof(GLfloat));
    if (*vertexCoord == NULL)
        return VLC_ENOMEM;
    *textureCoord = vlc_alloc(*nbVertices * 2, sizeof(GLfloat));
    if (*textureCoord == NULL)
    {
        free(*vertexCoord);
        return VLC_ENOMEM;
    }
    *indices = vlc_alloc(*nbIndices, sizeof(GLushort));
    if (*indices == NULL)
    {
        free(*textureCoord);
        free(*vertexCoord);
        return VLC_ENOMEM;
    }

    static const GLfloat coord[] = {
       -1.0,    1.0,    -1.0f,
       -1.0,    -1.0,   -1.0f,
       1.0,     1.0,    -1.0f,
       1.0,     -1.0,   -1.0f
    };

    memcpy(*vertexCoord, coord, *nbVertices * 3 * sizeof(GLfloat));

    static const GLfloat tex[] = {
        0.0, 1.0,
        0.0, 0.0,
        1.0, 1.0,
        1.0, 0.0,
    };

    memcpy(*textureCoord, tex, *nbVertices * 2 * sizeof(GLfloat));

    const GLushort ind[] = {
        0, 1, 2,
        2, 1, 3
    };

    memcpy(*indices, ind, *nbIndices * sizeof(GLushort));

    return VLC_SUCCESS;
}

static int SetupCoords(struct vlc_gl_renderer *renderer)
{
    const opengl_vtable_t *vt = renderer->vt;
    const video_format_t *fmt = renderer->sampler->fmt;

    GLfloat *vertexCoord, *textureCoord;
    GLushort *indices;
    unsigned nbVertices, nbIndices;

    int i_ret;
    switch (fmt->projection_mode)
    {
    case PROJECTION_MODE_RECTANGULAR:
        i_ret = BuildRectangle(&vertexCoord, &textureCoord, &nbVertices,
                               &indices, &nbIndices);
        break;
    case PROJECTION_MODE_EQUIRECTANGULAR:
        i_ret = BuildSphere(&vertexCoord, &textureCoord, &nbVertices,
                            &indices, &nbIndices);
        break;
    case PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD:
        i_ret = BuildCube((float)fmt->i_cubemap_padding / fmt->i_width,
                          (float)fmt->i_cubemap_padding / fmt->i_height,
                          &vertexCoord, &textureCoord, &nbVertices,
                          &indices, &nbIndices);
        break;
    default:
        i_ret = VLC_EGENERIC;
        break;
    }

    if (i_ret != VLC_SUCCESS)
        return i_ret;

    vt->BindBuffer(GL_ARRAY_BUFFER, renderer->texture_buffer_object);
    vt->BufferData(GL_ARRAY_BUFFER, nbVertices * 2 * sizeof(GLfloat),
                   textureCoord, GL_STATIC_DRAW);

    vt->BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer_object);
    vt->BufferData(GL_ARRAY_BUFFER, nbVertices * 3 * sizeof(GLfloat),
                   vertexCoord, GL_STATIC_DRAW);

    vt->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->index_buffer_object);
    vt->BufferData(GL_ELEMENT_ARRAY_BUFFER, nbIndices * sizeof(GLushort),
                   indices, GL_STATIC_DRAW);

    free(textureCoord);
    free(vertexCoord);
    free(indices);

    renderer->nb_indices = nbIndices;

    return VLC_SUCCESS;
}

int
vlc_gl_renderer_Draw(struct vlc_gl_renderer *renderer)
{
    const opengl_vtable_t *vt = renderer->vt;

    vt->Clear(GL_COLOR_BUFFER_BIT);

    vt->UseProgram(renderer->program_id);

    vlc_gl_sampler_Load(renderer->sampler);

    vt->BindBuffer(GL_ARRAY_BUFFER, renderer->texture_buffer_object);
    assert(renderer->aloc.PicCoordsIn != -1);
    vt->EnableVertexAttribArray(renderer->aloc.PicCoordsIn);
    vt->VertexAttribPointer(renderer->aloc.PicCoordsIn, 2, GL_FLOAT, 0, 0, 0);

    vt->BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_buffer_object);
    vt->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer->index_buffer_object);
    vt->EnableVertexAttribArray(renderer->aloc.VertexPosition);
    vt->VertexAttribPointer(renderer->aloc.VertexPosition, 3, GL_FLOAT, 0, 0, 0);

    vt->UniformMatrix3fv(renderer->uloc.StereoMatrix, 1, GL_FALSE,
                         renderer->var.StereoMatrix);
    vt->UniformMatrix4fv(renderer->uloc.ProjectionMatrix, 1, GL_FALSE,
                         renderer->var.ProjectionMatrix);
    vt->UniformMatrix4fv(renderer->uloc.ViewMatrix, 1, GL_FALSE,
                         renderer->var.ViewMatrix);
    vt->UniformMatrix4fv(renderer->uloc.ZoomMatrix, 1, GL_FALSE,
                         renderer->var.ZoomMatrix);

    vt->DrawElements(GL_TRIANGLES, renderer->nb_indices, GL_UNSIGNED_SHORT, 0);

    return VLC_SUCCESS;
}
