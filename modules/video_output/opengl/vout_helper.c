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
#include <vlc_picture_pool.h>
#include <vlc_subpicture.h>
#include <vlc_opengl.h>
#include <vlc_memory.h>
#include <vlc_vout.h>

#include "vout_helper.h"
#include "internal.h"

#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE 0x812F
#endif

#define SPHERE_RADIUS 1.f

static opengl_tex_converter_init_cb opengl_tex_converter_init_cbs[] =
{
    opengl_tex_converter_yuv_init,
    opengl_tex_converter_xyz12_init,
    opengl_tex_converter_rgba_init,
#ifdef __ANDROID__
    opengl_tex_converter_anop_init,
#endif
};

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

struct prgm
{
    GLuint id;
    opengl_tex_converter_t tc;

    struct {
        GLfloat OrientationMatrix[16];
        GLfloat ProjectionMatrix[16];
        GLfloat ZRotMatrix[16];
        GLfloat YRotMatrix[16];
        GLfloat XRotMatrix[16];
        GLfloat ZoomMatrix[16];
    } var;

    struct { /* UniformLocation */
        GLint OrientationMatrix;
        GLint ProjectionMatrix;
        GLint ZRotMatrix;
        GLint YRotMatrix;
        GLint XRotMatrix;
        GLint ZoomMatrix;
    } uloc;
    struct { /* AttribLocation */
        GLint MultiTexCoord[3];
        GLint VertexPosition;
    } aloc;
};

struct vout_display_opengl_t {

    vlc_gl_t   *gl;
    opengl_shaders_api_t api;

    video_format_t fmt;
    const vlc_chroma_description_t *chroma;

    GLsizei    tex_width[PICTURE_PLANE_MAX];
    GLsizei    tex_height[PICTURE_PLANE_MAX];

    GLuint     texture[PICTURE_PLANE_MAX];

    int         region_count;
    gl_region_t *region;


    picture_pool_t *pool;

    /* One YUV program and one RGBA program (for subpics) */
    struct prgm prgms[2];
    struct prgm *prgm; /* Main program */
    struct prgm *sub_prgm; /* Subpicture program */

    unsigned nb_indices;
    GLuint vertex_buffer_object;
    GLuint index_buffer_object;
    GLuint texture_buffer_object[PICTURE_PLANE_MAX];

    GLuint *subpicture_buffer_object;
    int    subpicture_buffer_object_count;

    struct {
        unsigned int i_x_offset;
        unsigned int i_y_offset;
        unsigned int i_visible_width;
        unsigned int i_visible_height;
    } last_source;

    /* Non-power-of-2 texture size support */
    bool supports_npot;

    /* View point */
    float f_teta;
    float f_phi;
    float f_roll;
    float f_fovx; /* f_fovx and f_fovy are linked but we keep both */
    float f_fovy; /* to avoid recalculating them when needed.      */
    float f_z;    /* Position of the camera on the shpere radius vector */
    float f_z_min;
    float f_sar;
};

static const GLfloat identity[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

/* rotation around the Z axis */
static void getZRotMatrix(float theta, GLfloat matrix[static 16])
{
    float st, ct;

    sincosf(theta, &st, &ct);

    const GLfloat m[] = {
    /*  x    y    z    w */
        ct,  -st, 0.f, 0.f,
        st,  ct,  0.f, 0.f,
        0.f, 0.f, 1.f, 0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

/* rotation around the Y axis */
static void getYRotMatrix(float theta, GLfloat matrix[static 16])
{
    float st, ct;

    sincosf(theta, &st, &ct);

    const GLfloat m[] = {
    /*  x    y    z    w */
        ct,  0.f, -st, 0.f,
        0.f, 1.f, 0.f, 0.f,
        st,  0.f, ct,  0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

/* rotation around the X axis */
static void getXRotMatrix(float phi, GLfloat matrix[static 16])
{
    float sp, cp;

    sincosf(phi, &sp, &cp);

    const GLfloat m[] = {
    /*  x    y    z    w */
        1.f, 0.f, 0.f, 0.f,
        0.f, cp,  sp,  0.f,
        0.f, -sp, cp,  0.f,
        0.f, 0.f, 0.f, 1.f
    };

    memcpy(matrix, m, sizeof(m));
}

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

static void getViewpointMatrixes(vout_display_opengl_t *vgl,
                                 video_projection_mode_t projection_mode,
                                 struct prgm *prgm)
{
    if (projection_mode == PROJECTION_MODE_EQUIRECTANGULAR
        || projection_mode == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD)
    {
        float sar = (float) vgl->f_sar;
        getProjectionMatrix(sar, vgl->f_fovy, prgm->var.ProjectionMatrix);
        getYRotMatrix(vgl->f_teta, prgm->var.YRotMatrix);
        getXRotMatrix(vgl->f_phi, prgm->var.XRotMatrix);
        getZRotMatrix(vgl->f_roll, prgm->var.ZRotMatrix);
        getZoomMatrix(vgl->f_z, prgm->var.ZoomMatrix);
    }
    else
    {
        memcpy(prgm->var.ProjectionMatrix, identity, sizeof(identity));
        memcpy(prgm->var.ZRotMatrix, identity, sizeof(identity));
        memcpy(prgm->var.YRotMatrix, identity, sizeof(identity));
        memcpy(prgm->var.XRotMatrix, identity, sizeof(identity));
        memcpy(prgm->var.ZoomMatrix, identity, sizeof(identity));
    }
}

static void getOrientationTransformMatrix(video_orientation_t orientation,
                                          GLfloat matrix[static 16])
{
    memcpy(matrix, identity, sizeof(identity));

    const int k_cos_pi = -1;
    const int k_cos_pi_2 = 0;
    const int k_cos_n_pi_2 = 0;

    const int k_sin_pi = 0;
    const int k_sin_pi_2 = 1;
    const int k_sin_n_pi_2 = -1;

    switch (orientation) {

        case ORIENT_ROTATED_90:
            matrix[0 * 4 + 0] = k_cos_pi_2;
            matrix[0 * 4 + 1] = -k_sin_pi_2;
            matrix[1 * 4 + 0] = k_sin_pi_2;
            matrix[1 * 4 + 1] = k_cos_pi_2;
            matrix[3 * 4 + 1] = 1;
            break;
        case ORIENT_ROTATED_180:
            matrix[0 * 4 + 0] = k_cos_pi;
            matrix[0 * 4 + 1] = -k_sin_pi;
            matrix[1 * 4 + 0] = k_sin_pi;
            matrix[1 * 4 + 1] = k_cos_pi;
            matrix[3 * 4 + 0] = 1;
            matrix[3 * 4 + 1] = 1;
            break;
        case ORIENT_ROTATED_270:
            matrix[0 * 4 + 0] = k_cos_n_pi_2;
            matrix[0 * 4 + 1] = -k_sin_n_pi_2;
            matrix[1 * 4 + 0] = k_sin_n_pi_2;
            matrix[1 * 4 + 1] = k_cos_n_pi_2;
            matrix[3 * 4 + 0] = 1;
            break;
        case ORIENT_HFLIPPED:
            matrix[0 * 4 + 0] = -1;
            matrix[3 * 4 + 0] = 1;
            break;
        case ORIENT_VFLIPPED:
            matrix[1 * 4 + 1] = -1;
            matrix[3 * 4 + 1] = 1;
            break;
        case ORIENT_TRANSPOSED:
            matrix[0 * 4 + 0] = 0;
            matrix[1 * 4 + 1] = 0;
            matrix[2 * 4 + 2] = -1;
            matrix[0 * 4 + 1] = 1;
            matrix[1 * 4 + 0] = 1;
            break;
        case ORIENT_ANTI_TRANSPOSED:
            matrix[0 * 4 + 0] = 0;
            matrix[1 * 4 + 1] = 0;
            matrix[2 * 4 + 2] = -1;
            matrix[0 * 4 + 1] = -1;
            matrix[1 * 4 + 0] = -1;
            matrix[3 * 4 + 0] = 1;
            matrix[3 * 4 + 1] = 1;
            break;
        default:
            break;
    }
}

static inline GLsizei GetAlignedSize(unsigned size)
{
    /* Return the smallest larger or equal power of 2 */
    unsigned align = 1 << (8 * sizeof (unsigned) - clz(size));
    return ((align >> 1) == size) ? size : align;
}

static GLuint BuildVertexShader(vout_display_opengl_t *vgl, unsigned plane_count)
{
    /* Basic vertex shader */
    static const char *template =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "varying vec4 TexCoord0;attribute vec4 MultiTexCoord0;"
        "%s%s"
        "attribute vec3 VertexPosition;"
        "uniform mat4 OrientationMatrix;"
        "uniform mat4 ProjectionMatrix;"
        "uniform mat4 XRotMatrix;"
        "uniform mat4 YRotMatrix;"
        "uniform mat4 ZRotMatrix;"
        "uniform mat4 ZoomMatrix;"
        "void main() {"
        " TexCoord0 = OrientationMatrix * MultiTexCoord0;"
        "%s%s"
        " gl_Position = ProjectionMatrix * ZoomMatrix * ZRotMatrix * XRotMatrix * YRotMatrix * vec4(VertexPosition, 1.0);"
        "}";

    const char *coord1_header = plane_count > 1 ?
        "varying vec4 TexCoord1;attribute vec4 MultiTexCoord1;" : "";
    const char *coord1_code = plane_count > 1 ?
        " TexCoord1 = OrientationMatrix * MultiTexCoord1;" : "";
    const char *coord2_header = plane_count > 2 ?
        "varying vec4 TexCoord2;attribute vec4 MultiTexCoord2;" : "";
    const char *coord2_code = plane_count > 2 ?
        " TexCoord2 = OrientationMatrix * MultiTexCoord2;" : "";

    char *code;
    if (asprintf(&code, template, coord1_header, coord2_header,
                 coord1_code, coord2_code) < 0)
        return 0;

    GLuint shader = vgl->api.CreateShader(GL_VERTEX_SHADER);
    vgl->api.ShaderSource(shader, 1, (const char **) &code, NULL);
    vgl->api.CompileShader(shader);
    free(code);
    return shader;
}

static int
GenTextures(const opengl_tex_converter_t *tc,
            const GLsizei *tex_width, const GLsizei *tex_height,
            GLuint *textures)
{
    glGenTextures(tc->desc->plane_count, textures);

    for (unsigned i = 0; i < tc->desc->plane_count; i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        glClientActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(tc->tex_target, textures[i]);

#if !defined(USE_OPENGL_ES2)
        /* Set the texture parameters */
        glTexParameterf(tc->tex_target, GL_TEXTURE_PRIORITY, 1.0);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

        glTexParameteri(tc->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(tc->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(tc->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(tc->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        if (tc->pf_allocate_texture != NULL)
        {
            int ret = tc->pf_allocate_texture(tc, textures[i], i, tex_width[i],
                                              tex_height[i]);
            if (ret != VLC_SUCCESS)
            {
                if (i > 0)
                {
                    glDeleteTextures(i, textures);
                    memset(textures, 0, tc->desc->plane_count * sizeof(GLuint));
                }
                return ret;
            }
        }
    }
    return VLC_SUCCESS;
}

static void
DelTextures(const opengl_tex_converter_t *tc, GLuint *textures)
{
    glDeleteTextures(tc->desc->plane_count, textures);
    memset(textures, 0, tc->desc->plane_count * sizeof(GLuint));
}

vout_display_opengl_t *vout_display_opengl_New(video_format_t *fmt,
                                               const vlc_fourcc_t **subpicture_chromas,
                                               vlc_gl_t *gl,
                                               const vlc_viewpoint_t *viewpoint)
{
    vout_display_opengl_t *vgl = calloc(1, sizeof(*vgl));
    if (!vgl)
        return NULL;

    vgl->gl = gl;

    if (gl->getProcAddress == NULL) {
        msg_Err(gl, "getProcAddress not implemented, bailing out\n");
        free(vgl);
        return NULL;
    }

    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
#if !defined(USE_OPENGL_ES2)
    const unsigned char *ogl_version = glGetString(GL_VERSION);
    bool supports_shaders = strverscmp((const char *)ogl_version, "2.0") >= 0;
#else
    bool supports_shaders = true;
#endif

    opengl_shaders_api_t *api = &vgl->api;

#if defined(USE_OPENGL_ES2)
#define GET_PROC_ADDR(name) name
#else
#define GET_PROC_ADDR(name) vlc_gl_GetProcAddress(gl, #name)
#endif
    api->CreateShader       = GET_PROC_ADDR(glCreateShader);
    api->ShaderSource       = GET_PROC_ADDR(glShaderSource);
    api->CompileShader      = GET_PROC_ADDR(glCompileShader);
    api->AttachShader       = GET_PROC_ADDR(glAttachShader);

    api->GetProgramiv       = GET_PROC_ADDR(glGetProgramiv);
    api->GetShaderiv        = GET_PROC_ADDR(glGetShaderiv);
    api->GetProgramInfoLog  = GET_PROC_ADDR(glGetProgramInfoLog);
    api->GetShaderInfoLog   = GET_PROC_ADDR(glGetShaderInfoLog);

    api->DeleteShader       = GET_PROC_ADDR(glDeleteShader);

    api->GetUniformLocation      = GET_PROC_ADDR(glGetUniformLocation);
    api->GetAttribLocation       = GET_PROC_ADDR(glGetAttribLocation);
    api->VertexAttribPointer     = GET_PROC_ADDR(glVertexAttribPointer);
    api->EnableVertexAttribArray = GET_PROC_ADDR(glEnableVertexAttribArray);
    api->UniformMatrix4fv        = GET_PROC_ADDR(glUniformMatrix4fv);
    api->Uniform4fv              = GET_PROC_ADDR(glUniform4fv);
    api->Uniform4f               = GET_PROC_ADDR(glUniform4f);
    api->Uniform1i               = GET_PROC_ADDR(glUniform1i);

    api->CreateProgram = GET_PROC_ADDR(glCreateProgram);
    api->LinkProgram   = GET_PROC_ADDR(glLinkProgram);
    api->UseProgram    = GET_PROC_ADDR(glUseProgram);
    api->DeleteProgram = GET_PROC_ADDR(glDeleteProgram);

    api->GenBuffers    = GET_PROC_ADDR(glGenBuffers);
    api->BindBuffer    = GET_PROC_ADDR(glBindBuffer);
    api->BufferData    = GET_PROC_ADDR(glBufferData);
    api->DeleteBuffers = GET_PROC_ADDR(glDeleteBuffers);
#ifdef VLCGL_HAS_PBO
    api->BufferStorage          = GET_PROC_ADDR(glBufferStorage);
    api->MapBufferRange         = GET_PROC_ADDR(glMapBufferRange);
    api->FlushMappedBufferRange = GET_PROC_ADDR(glFlushMappedBufferRange);
    api->UnmapBuffer            = GET_PROC_ADDR(glUnmapBuffer);
    api->FenceSync              = GET_PROC_ADDR(glFenceSync);
    api->DeleteSync             = GET_PROC_ADDR(glDeleteSync);
    api->ClientWaitSync         = GET_PROC_ADDR(glClientWaitSync);
#endif
#undef GET_PROC_ADDR

    if (!vgl->api.CreateShader || !vgl->api.ShaderSource || !vgl->api.CreateProgram)
        supports_shaders = false;
    if (!supports_shaders)
    {
        msg_Err(gl, "shaders not supported");
        free(vgl);
        return NULL;
    }

#if defined(_WIN32)
    api->ActiveTexture = vlc_gl_GetProcAddress(gl, "glActiveTexture");
    api->ClientActiveTexture = vlc_gl_GetProcAddress(gl, "glClientActiveTexture");
#   undef glActiveTexture
#   undef glClientActiveTexture
#   define glActiveTexture vgl->api.ActiveTexture
#   define glClientActiveTexture vgl->api.ClientActiveTexture
#endif

#if defined(USE_OPENGL_ES2)
    /* OpenGL ES 2 includes support for non-power of 2 textures by specification
     * so checks for extensions are bound to fail. Check for OpenGL ES version instead. */
    vgl->supports_npot = true;
#else
    vgl->supports_npot = HasExtension(extensions, "GL_ARB_texture_non_power_of_two") ||
                         HasExtension(extensions, "GL_APPLE_texture_2D_limited_npot");
#endif

    /* Initialize with default chroma */
    vgl->fmt = *fmt;
    vgl->fmt.i_chroma = VLC_CODEC_RGB32;
#   if defined(WORDS_BIGENDIAN)
    vgl->fmt.i_rmask  = 0xff000000;
    vgl->fmt.i_gmask  = 0x00ff0000;
    vgl->fmt.i_bmask  = 0x0000ff00;
#   else
    vgl->fmt.i_rmask  = 0x000000ff;
    vgl->fmt.i_gmask  = 0x0000ff00;
    vgl->fmt.i_bmask  = 0x00ff0000;
#   endif
    GLuint fragment_shader = 0, sub_fragment_shader;
    opengl_tex_converter_t tex_conv;
    opengl_tex_converter_t sub_tex_conv = {
        .parent = VLC_OBJECT(vgl->gl),
        .api = &vgl->api,
        .glexts = extensions,
        .orientation = fmt->orientation,
    };

    /* RGBA is needed for subpictures or for non YUV pictures */
    sub_fragment_shader = opengl_tex_converter_rgba_init(&vgl->fmt, &sub_tex_conv);
    if (sub_fragment_shader == 0)
    {
        msg_Err(gl, "RGBA shader failed");
        free(vgl);
        return NULL;
    }

    const video_format_t *fmts[2] = { fmt, &vgl->fmt };
    for (size_t i = 0; i < 2 && fragment_shader == 0; ++i)
    {
        /* Try first the untouched fmt, then the rgba fmt */
        for (size_t j = 0; j < ARRAY_SIZE(opengl_tex_converter_init_cbs); ++j)
        {
            tex_conv = (opengl_tex_converter_t) {
                .parent = VLC_OBJECT(vgl->gl),
                .api = &vgl->api,
                .glexts = extensions,
                .orientation = fmt->orientation,
            };
            fragment_shader = opengl_tex_converter_init_cbs[j](fmts[i], &tex_conv);
            if (fragment_shader != 0)
            {
                assert(tex_conv.chroma != 0 && tex_conv.tex_target != 0 &&
                       tex_conv.pf_update != NULL &&
                       tex_conv.pf_fetch_locations != NULL &&
                       tex_conv.pf_prepare_shader != NULL);
                vgl->fmt = *fmt;
                vgl->fmt.i_chroma = tex_conv.chroma;
                break;
            }
        }
    }
    if (fragment_shader == 0)
    {
        msg_Err(gl, "could not init tex converter");
        if (sub_tex_conv.pf_release != NULL)
            sub_tex_conv.pf_release(&sub_tex_conv);
        vgl->api.DeleteShader(sub_fragment_shader);
        free(vgl);
        return NULL;
    }

    /* Build program if needed */
    GLuint vertex_shader, sub_vertex_shader;
    vertex_shader = BuildVertexShader(vgl, tex_conv.desc->plane_count);
    sub_vertex_shader = BuildVertexShader(vgl, sub_tex_conv.desc->plane_count);

    const GLuint shaders[] = {
        fragment_shader,
        sub_fragment_shader,
        vertex_shader,
        sub_vertex_shader
    };

    /* Check shaders messages */
    for (unsigned j = 0; j < 4; j++) {
        int infoLength;
        vgl->api.GetShaderiv(shaders[j], GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength <= 1)
            continue;

        char *infolog = malloc(infoLength);
        if (infolog != NULL)
        {
            int charsWritten;
            vgl->api.GetShaderInfoLog(shaders[j], infoLength, &charsWritten,
                                      infolog);
            msg_Err(gl, "shader %d: %s", j, infolog);
            free(infolog);
        }
    }

    /* Main picture vertex shaders */
    vgl->prgm = &vgl->prgms[0];
    vgl->prgm->tc = tex_conv;
    vgl->prgm->id = vgl->api.CreateProgram();
    vgl->api.AttachShader(vgl->prgm->id, fragment_shader);
    vgl->api.AttachShader(vgl->prgm->id, vertex_shader);
    vgl->api.LinkProgram(vgl->prgm->id);

    /* Subpicture Vertex shaders */
    vgl->sub_prgm = &vgl->prgms[1];
    vgl->sub_prgm->tc = sub_tex_conv;
    vgl->sub_prgm->id = vgl->api.CreateProgram();
    vgl->api.AttachShader(vgl->sub_prgm->id, sub_fragment_shader);
    vgl->api.AttachShader(vgl->sub_prgm->id, sub_vertex_shader);
    vgl->api.LinkProgram(vgl->sub_prgm->id);

    vgl->api.DeleteShader(vertex_shader);
    vgl->api.DeleteShader(fragment_shader);
    vgl->api.DeleteShader(sub_vertex_shader);
    vgl->api.DeleteShader(sub_fragment_shader);

    /* Check program messages */
    for (GLuint i = 0; i < 2; i++) {
        int infoLength = 0;
        vgl->api.GetProgramiv(vgl->prgms[i].id, GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength <= 1)
            continue;
        char *infolog = malloc(infoLength);
        if (infolog != NULL)
        {
            int charsWritten;
            vgl->api.GetProgramInfoLog(vgl->prgms[i].id, infoLength, &charsWritten,
                                       infolog);
            msg_Err(gl, "shader program %d: %s", i, infolog);
            free(infolog);
        }

        /* If there is some message, better to check linking is ok */
        GLint link_status = GL_TRUE;
        vgl->api.GetProgramiv(vgl->prgms[i].id, GL_LINK_STATUS, &link_status);
        if (link_status == GL_FALSE) {
            msg_Err(gl, "Unable to use program %d\n", i);
            vout_display_opengl_Delete(vgl);
            return NULL;
        }
    }

    vgl->chroma = vgl->prgm->tc.desc;
    assert(vgl->chroma != NULL);

    /* Fetch UniformLocations and AttribLocations */
    for (GLuint i = 0; i < 2; i++)
    {
#define GET_LOC(type, x, str) do { \
    x = vgl->api.Get##type##Location(prgm->id, str); \
    assert(x != -1); \
    if (x == -1) { \
        msg_Err(gl, "Unable to Get"#type"Location(%s)\n", str); \
        vout_display_opengl_Delete(vgl); \
        return NULL; \
    } \
} while (0)
#define GET_ULOC(x, str) GET_LOC(Uniform, prgm->uloc.x, str)
#define GET_ALOC(x, str) GET_LOC(Attrib, prgm->aloc.x, str)
        struct prgm *prgm = &vgl->prgms[i];
        GET_ULOC(OrientationMatrix, "OrientationMatrix");
        GET_ULOC(ProjectionMatrix, "ProjectionMatrix");
        GET_ULOC(ZRotMatrix, "ZRotMatrix");
        GET_ULOC(YRotMatrix, "YRotMatrix");
        GET_ULOC(XRotMatrix, "XRotMatrix");
        GET_ULOC(ZoomMatrix, "ZoomMatrix");

        GET_ALOC(VertexPosition, "VertexPosition");
        GET_ALOC(MultiTexCoord[0], "MultiTexCoord0");
        /* MultiTexCoord 1 and 2 can be optimized out if not used */
        if (prgm->tc.desc->plane_count > 1)
            GET_ALOC(MultiTexCoord[1], "MultiTexCoord1");
        else
            prgm->aloc.MultiTexCoord[1] = -1;
        if (prgm->tc.desc->plane_count > 2)
            GET_ALOC(MultiTexCoord[2], "MultiTexCoord2");
        else
            prgm->aloc.MultiTexCoord[2] = -1;
#undef GET_LOC
#undef GET_ULOC
#undef GET_ALOC
        int ret = prgm->tc.pf_fetch_locations(&prgm->tc, prgm->id);
        assert(ret == VLC_SUCCESS);
        if (ret != VLC_SUCCESS)
        {
            msg_Err(gl, "Unable to get locations from %4.4s tex_conv\n",
                    (const char *) &prgm->tc.chroma);
            vout_display_opengl_Delete(vgl);
            return NULL;
        }
    }
    getOrientationTransformMatrix(vgl->prgm->tc.orientation,
                                  vgl->prgm->var.OrientationMatrix);
    getViewpointMatrixes(vgl, vgl->fmt.projection_mode, vgl->prgm);

    /* Normal orientation and no projection for subtitles */
    getOrientationTransformMatrix(ORIENT_NORMAL,
                                  vgl->sub_prgm->var.OrientationMatrix);
    getViewpointMatrixes(vgl, PROJECTION_MODE_RECTANGULAR, vgl->sub_prgm);

    /* Texture size */
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        const GLsizei w = vgl->fmt.i_visible_width  * vgl->chroma->p[j].w.num
                        / vgl->chroma->p[j].w.den;
        const GLsizei h = vgl->fmt.i_visible_height * vgl->chroma->p[j].h.num
                        / vgl->chroma->p[j].h.den;
        if (vgl->supports_npot) {
            vgl->tex_width[j]  = w;
            vgl->tex_height[j] = h;
        } else {
            vgl->tex_width[j]  = GetAlignedSize(w);
            vgl->tex_height[j] = GetAlignedSize(h);
        }
    }

    /* */
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    vgl->api.GenBuffers(1, &vgl->vertex_buffer_object);
    vgl->api.GenBuffers(1, &vgl->index_buffer_object);
    vgl->api.GenBuffers(vgl->chroma->plane_count, vgl->texture_buffer_object);

    /* Initial number of allocated buffer objects for subpictures, will grow dynamically. */
    int subpicture_buffer_object_count = 8;
    vgl->subpicture_buffer_object = malloc(subpicture_buffer_object_count * sizeof(GLuint));
    if (!vgl->subpicture_buffer_object) {
        vout_display_opengl_Delete(vgl);
        return NULL;
    }
    vgl->subpicture_buffer_object_count = subpicture_buffer_object_count;
    vgl->api.GenBuffers(vgl->subpicture_buffer_object_count, vgl->subpicture_buffer_object);

    /* */
    for (size_t i = 0; i < PICTURE_PLANE_MAX; i++)
        vgl->texture[i] = 0;
    vgl->region_count = 0;
    vgl->region = NULL;
    vgl->pool = NULL;

    if (vgl->fmt.projection_mode != PROJECTION_MODE_RECTANGULAR
     && vout_display_opengl_SetViewpoint(vgl, viewpoint) != VLC_SUCCESS)
    {
        vout_display_opengl_Delete(vgl);
        return NULL;
    }

    *fmt = vgl->fmt;
    if (subpicture_chromas) {
        *subpicture_chromas = gl_subpicture_chromas;
    }
    return vgl;
}

void vout_display_opengl_Delete(vout_display_opengl_t *vgl)
{
    /* */
    glFinish();
    glFlush();

    opengl_tex_converter_t *tc = &vgl->prgm->tc;
    DelTextures(tc, vgl->texture);

    tc = &vgl->sub_prgm->tc;
    for (int i = 0; i < vgl->region_count; i++)
    {
        if (vgl->region[i].texture)
            DelTextures(tc, &vgl->region[i].texture);
    }
    free(vgl->region);

    for (int i = 0; i < 2 && vgl->prgms[i].id != 0; i++)
    {
        vgl->api.DeleteProgram(vgl->prgms[i].id);
        opengl_tex_converter_t *tc = &vgl->prgms[i].tc;
        if (tc->pf_release != NULL)
            tc->pf_release(tc);
    }
    vgl->api.DeleteBuffers(1, &vgl->vertex_buffer_object);
    vgl->api.DeleteBuffers(1, &vgl->index_buffer_object);
    if (vgl->chroma != NULL)
        vgl->api.DeleteBuffers(vgl->chroma->plane_count, vgl->texture_buffer_object);
    if (vgl->subpicture_buffer_object_count > 0)
        vgl->api.DeleteBuffers(vgl->subpicture_buffer_object_count, vgl->subpicture_buffer_object);
    free(vgl->subpicture_buffer_object);

    if (vgl->pool)
        picture_pool_Release(vgl->pool);
    free(vgl);
}

static void UpdateZ(vout_display_opengl_t *vgl)
{
    /* Do trigonometry to calculate the minimal z value
     * that will allow us to zoom out without seeing the outside of the
     * sphere (black borders). */
    float tan_fovx_2 = tanf(vgl->f_fovx / 2);
    float tan_fovy_2 = tanf(vgl->f_fovy / 2);
    float z_min = - SPHERE_RADIUS / sinf(atanf(sqrtf(
                    tan_fovx_2 * tan_fovx_2 + tan_fovy_2 * tan_fovy_2)));

    /* The FOV value above which z is dynamically calculated. */
    const float z_thresh = 90.f;

    if (vgl->f_fovx <= z_thresh * M_PI / 180)
        vgl->f_z = 0;
    else
    {
        float f = z_min / ((FIELD_OF_VIEW_DEGREES_MAX - z_thresh) * M_PI / 180);
        vgl->f_z = f * vgl->f_fovx - f * z_thresh * M_PI / 180;
        if (vgl->f_z < z_min)
            vgl->f_z = z_min;
    }
}

static void UpdateFOVy(vout_display_opengl_t *vgl)
{
    vgl->f_fovy = 2 * atanf(tanf(vgl->f_fovx / 2) / vgl->f_sar);
}

int vout_display_opengl_SetViewpoint(vout_display_opengl_t *vgl,
                                     const vlc_viewpoint_t *p_vp)
{
#define RAD(d) ((float) ((d) * M_PI / 180.f))
    float f_fovx = RAD(p_vp->fov);
    if (f_fovx > FIELD_OF_VIEW_DEGREES_MAX * M_PI / 180 + 0.001f
        || f_fovx < -0.001f)
        return VLC_EBADVAR;

    vgl->f_teta = RAD(p_vp->yaw) - (float) M_PI_2;
    vgl->f_phi  = RAD(p_vp->pitch);
    vgl->f_roll = RAD(p_vp->roll);


    if (fabsf(f_fovx - vgl->f_fovx) >= 0.001f)
    {
        /* FOVx has changed. */
        vgl->f_fovx = f_fovx;
        UpdateFOVy(vgl);
        UpdateZ(vgl);
    }
    getViewpointMatrixes(vgl, vgl->fmt.projection_mode, vgl->prgm);

    return VLC_SUCCESS;
#undef RAD
}


void vout_display_opengl_SetWindowAspectRatio(vout_display_opengl_t *vgl,
                                              float f_sar)
{
    /* Each time the window size changes, we must recompute the minimum zoom
     * since the aspect ration changes.
     * We must also set the new current zoom value. */
    vgl->f_sar = f_sar;
    UpdateFOVy(vgl);
    UpdateZ(vgl);
    getViewpointMatrixes(vgl, vgl->fmt.projection_mode, vgl->prgm);
}

picture_pool_t *vout_display_opengl_GetPool(vout_display_opengl_t *vgl, unsigned requested_count)
{
    if (vgl->pool)
        return vgl->pool;

    /* Allocates our textures */
    opengl_tex_converter_t *tc = &vgl->prgm->tc;
    int ret = GenTextures(tc, vgl->tex_width, vgl->tex_height, vgl->texture);
    if (ret != VLC_SUCCESS)
        return NULL;

    requested_count = __MIN(VLCGL_PICTURE_MAX, requested_count);
    /* Allocate with tex converter pool callback if it exists */
    if (tc->pf_get_pool != NULL)
    {
        vgl->pool = tc->pf_get_pool(tc, &vgl->fmt, requested_count,
                                    vgl->texture);
        if (!vgl->pool)
            goto error;
        return vgl->pool;
    }

    /* Allocate our pictures */
    picture_t *picture[VLCGL_PICTURE_MAX] = {NULL, };
    unsigned count;
    for (count = 0; count < requested_count; count++)
    {
        picture[count] = picture_NewFromFormat(&vgl->fmt);
        if (!picture[count])
            break;
    }
    if (count <= 0)
        goto error;

    /* Wrap the pictures into a pool */
    vgl->pool = picture_pool_New(count, picture);
    if (!vgl->pool)
    {
        for (unsigned i = 0; i < count; i++)
            picture_Release(picture[i]);
        goto error;
    }

    return vgl->pool;

error:
    DelTextures(tc, vgl->texture);
    return NULL;
}

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture, subpicture_t *subpicture)
{
    opengl_tex_converter_t *tc = &vgl->prgm->tc;

    /* Update the texture */
    int ret = tc->pf_update(tc, vgl->texture, vgl->tex_width, vgl->tex_height,
                            picture, NULL);
    if (ret != VLC_SUCCESS)
        return ret;

    int         last_count = vgl->region_count;
    gl_region_t *last = vgl->region;

    vgl->region_count = 0;
    vgl->region       = NULL;

    tc = &vgl->sub_prgm->tc;
    if (subpicture) {

        int count = 0;
        for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
            count++;

        vgl->region_count = count;
        vgl->region       = calloc(count, sizeof(*vgl->region));

        int i = 0;
        for (subpicture_region_t *r = subpicture->p_region;
             r && ret == VLC_SUCCESS; r = r->p_next, i++) {
            gl_region_t *glr = &vgl->region[i];

            glr->width  = r->fmt.i_visible_width;
            glr->height = r->fmt.i_visible_height;
            if (!vgl->supports_npot) {
                glr->width  = GetAlignedSize(glr->width);
                glr->height = GetAlignedSize(glr->height);
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
                ret = GenTextures(tc, &glr->width, &glr->height, &glr->texture);
                if (ret != VLC_SUCCESS)
                    continue;
            }
            ret = tc->pf_update(tc, &glr->texture, &glr->width, &glr->height,
                                r->p_picture, &pixels_offset);
        }
    }
    for (int i = 0; i < last_count; i++) {
        if (last[i].texture)
            DelTextures(tc, &last[i].texture);
    }
    free(last);

    VLC_UNUSED(subpicture);
    return ret;
}

static int BuildSphere(unsigned nbPlanes,
                        GLfloat **vertexCoord, GLfloat **textureCoord, unsigned *nbVertices,
                        GLushort **indices, unsigned *nbIndices,
                        const float *left, const float *top,
                        const float *right, const float *bottom)
{
    unsigned nbLatBands = 128;
    unsigned nbLonBands = 128;

    *nbVertices = (nbLatBands + 1) * (nbLonBands + 1);
    *nbIndices = nbLatBands * nbLonBands * 3 * 2;

    *vertexCoord = malloc(*nbVertices * 3 * sizeof(GLfloat));
    if (*vertexCoord == NULL)
        return VLC_ENOMEM;
    *textureCoord = malloc(nbPlanes * *nbVertices * 2 * sizeof(GLfloat));
    if (*textureCoord == NULL)
    {
        free(*vertexCoord);
        return VLC_ENOMEM;
    }
    *indices = malloc(*nbIndices * sizeof(GLushort));
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

            for (unsigned p = 0; p < nbPlanes; ++p)
            {
                unsigned off2 = (p * (nbLatBands + 1) * (nbLonBands + 1)
                                + lat * (nbLonBands + 1) + lon) * 2;
                float width = right[p] - left[p];
                float height = bottom[p] - top[p];
                float u = (float)lon / nbLonBands * width;
                float v = (float)lat / nbLatBands * height;
                (*textureCoord)[off2] = u;
                (*textureCoord)[off2 + 1] = v;
            }
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


static int BuildCube(unsigned nbPlanes,
                     float padW, float padH,
                     GLfloat **vertexCoord, GLfloat **textureCoord, unsigned *nbVertices,
                     GLushort **indices, unsigned *nbIndices,
                     const float *left, const float *top,
                     const float *right, const float *bottom)
{
    *nbVertices = 4 * 6;
    *nbIndices = 6 * 6;

    *vertexCoord = malloc(*nbVertices * 3 * sizeof(GLfloat));
    if (*vertexCoord == NULL)
        return VLC_ENOMEM;
    *textureCoord = malloc(nbPlanes * *nbVertices * 2 * sizeof(GLfloat));
    if (*textureCoord == NULL)
    {
        free(*vertexCoord);
        return VLC_ENOMEM;
    }
    *indices = malloc(*nbIndices * sizeof(GLushort));
    if (*indices == NULL)
    {
        free(*textureCoord);
        free(*vertexCoord);
        return VLC_ENOMEM;
    }

    static const GLfloat coord[] = {
        -1.0,    1.0,    -1.0f, // front
        -1.0,    -1.0,   -1.0f,
        1.0,     1.0,    -1.0f,
        1.0,     -1.0,   -1.0f,

        -1.0,    1.0,    1.0f, // back
        -1.0,    -1.0,   1.0f,
        1.0,     1.0,    1.0f,
        1.0,     -1.0,   1.0f,

        -1.0,    1.0,    -1.0f, // left
        -1.0,    -1.0,   -1.0f,
        -1.0,     1.0,    1.0f,
        -1.0,     -1.0,   1.0f,

        1.0f,    1.0,    -1.0f, // right
        1.0f,   -1.0,    -1.0f,
        1.0f,   1.0,     1.0f,
        1.0f,   -1.0,    1.0f,

        -1.0,    -1.0,    1.0f, // bottom
        -1.0,    -1.0,   -1.0f,
        1.0,     -1.0,    1.0f,
        1.0,     -1.0,   -1.0f,

        -1.0,    1.0,    1.0f, // top
        -1.0,    1.0,   -1.0f,
        1.0,     1.0,    1.0f,
        1.0,     1.0,   -1.0f,
    };

    memcpy(*vertexCoord, coord, *nbVertices * 3 * sizeof(GLfloat));

    for (unsigned p = 0; p < nbPlanes; ++p)
    {
        float width = right[p] - left[p];
        float height = bottom[p] - top[p];

        float col[] = {left[p],
                       left[p] + width * 1.f/3,
                       left[p] + width * 2.f/3,
                       left[p] + width};

        float row[] = {top[p],
                       top[p] + height * 1.f/2,
                       top[p] + height};

        const GLfloat tex[] = {
            col[1] + padW, row[1] + padH, // front
            col[1] + padW, row[2] - padH,
            col[2] - padW, row[1] + padH,
            col[2] - padW, row[2] - padH,

            col[3] - padW, row[1] + padH, // back
            col[3] - padW, row[2] - padH,
            col[2] + padW, row[1] + padH,
            col[2] + padW, row[2] - padH,

            col[2] - padW, row[0] + padH, // left
            col[2] - padW, row[1] - padH,
            col[1] + padW, row[0] + padH,
            col[1] + padW, row[1] - padH,

            col[0] + padW, row[0] + padH, // right
            col[0] + padW, row[1] - padH,
            col[1] - padW, row[0] + padH,
            col[1] - padW, row[1] - padH,

            col[0] + padW, row[2] - padH, // bottom
            col[0] + padW, row[1] + padH,
            col[1] - padW, row[2] - padH,
            col[1] - padW, row[1] + padH,

            col[2] + padW, row[0] + padH, // top
            col[2] + padW, row[1] - padH,
            col[3] - padW, row[0] + padH,
            col[3] - padW, row[1] - padH,
        };

        memcpy(*textureCoord + p * *nbVertices * 2, tex,
               *nbVertices * 2 * sizeof(GLfloat));
    }

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
}

static int BuildRectangle(unsigned nbPlanes,
                          GLfloat **vertexCoord, GLfloat **textureCoord, unsigned *nbVertices,
                          GLushort **indices, unsigned *nbIndices,
                          const float *left, const float *top,
                          const float *right, const float *bottom)
{
    *nbVertices = 4;
    *nbIndices = 6;

    *vertexCoord = malloc(*nbVertices * 3 * sizeof(GLfloat));
    if (*vertexCoord == NULL)
        return VLC_ENOMEM;
    *textureCoord = malloc(nbPlanes * *nbVertices * 2 * sizeof(GLfloat));
    if (*textureCoord == NULL)
    {
        free(*vertexCoord);
        return VLC_ENOMEM;
    }
    *indices = malloc(*nbIndices * sizeof(GLushort));
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

    for (unsigned p = 0; p < nbPlanes; ++p)
    {
        const GLfloat tex[] = {
            left[p],  top[p],
            left[p],  bottom[p],
            right[p], top[p],
            right[p], bottom[p]
        };

        memcpy(*textureCoord + p * *nbVertices * 2, tex,
               *nbVertices * 2 * sizeof(GLfloat));
    }

    const GLushort ind[] = {
        0, 1, 2,
        2, 1, 3
    };

    memcpy(*indices, ind, *nbIndices * sizeof(GLushort));

    return VLC_SUCCESS;
}

static int SetupCoords(vout_display_opengl_t *vgl,
                       const float *left, const float *top,
                       const float *right, const float *bottom)
{
    GLfloat *vertexCoord, *textureCoord;
    GLushort *indices;
    unsigned nbVertices, nbIndices;

    int i_ret;
    switch (vgl->fmt.projection_mode)
    {
    case PROJECTION_MODE_RECTANGULAR:
        i_ret = BuildRectangle(vgl->chroma->plane_count,
                               &vertexCoord, &textureCoord, &nbVertices,
                               &indices, &nbIndices,
                               left, top, right, bottom);
        break;
    case PROJECTION_MODE_EQUIRECTANGULAR:
        i_ret = BuildSphere(vgl->chroma->plane_count,
                            &vertexCoord, &textureCoord, &nbVertices,
                            &indices, &nbIndices,
                            left, top, right, bottom);
        break;
    case PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD:
        i_ret = BuildCube(vgl->chroma->plane_count,
                          (float)vgl->fmt.i_cubemap_padding / vgl->fmt.i_width,
                          (float)vgl->fmt.i_cubemap_padding / vgl->fmt.i_height,
                          &vertexCoord, &textureCoord, &nbVertices,
                          &indices, &nbIndices,
                          left, top, right, bottom);
        break;
    default:
        i_ret = VLC_EGENERIC;
        break;
    }

    if (i_ret != VLC_SUCCESS)
        return i_ret;

    for (unsigned j = 0; j < vgl->chroma->plane_count; j++)
    {
        vgl->api.BindBuffer(GL_ARRAY_BUFFER, vgl->texture_buffer_object[j]);
        vgl->api.BufferData(GL_ARRAY_BUFFER, nbVertices * 2 * sizeof(GLfloat),
                        textureCoord + j * nbVertices * 2, GL_STATIC_DRAW);
    }

    vgl->api.BindBuffer(GL_ARRAY_BUFFER, vgl->vertex_buffer_object);
    vgl->api.BufferData(GL_ARRAY_BUFFER, nbVertices * 3 * sizeof(GLfloat),
                        vertexCoord, GL_STATIC_DRAW);

    vgl->api.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vgl->index_buffer_object);
    vgl->api.BufferData(GL_ELEMENT_ARRAY_BUFFER, nbIndices * sizeof(GLushort),
                        indices, GL_STATIC_DRAW);

    free(textureCoord);
    free(vertexCoord);
    free(indices);

    vgl->nb_indices = nbIndices;

    return VLC_SUCCESS;
}

static void DrawWithShaders(vout_display_opengl_t *vgl, struct prgm *prgm)
{
    opengl_tex_converter_t *tc = &prgm->tc;
    tc->pf_prepare_shader(tc, vgl->tex_width, vgl->tex_height, 1.0f);

    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        assert(vgl->texture[j] != 0);
        glActiveTexture(GL_TEXTURE0+j);
        glClientActiveTexture(GL_TEXTURE0+j);
        glBindTexture(tc->tex_target, vgl->texture[j]);

        vgl->api.BindBuffer(GL_ARRAY_BUFFER, vgl->texture_buffer_object[j]);

        assert(prgm->aloc.MultiTexCoord[j] != -1);
        vgl->api.EnableVertexAttribArray(prgm->aloc.MultiTexCoord[j]);
        vgl->api.VertexAttribPointer(prgm->aloc.MultiTexCoord[j], 2, GL_FLOAT,
                                     0, 0, 0);
    }

    vgl->api.BindBuffer(GL_ARRAY_BUFFER, vgl->vertex_buffer_object);
    vgl->api.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vgl->index_buffer_object);
    vgl->api.EnableVertexAttribArray(prgm->aloc.VertexPosition);
    vgl->api.VertexAttribPointer(prgm->aloc.VertexPosition, 3, GL_FLOAT, 0, 0, 0);

    vgl->api.UniformMatrix4fv(prgm->uloc.OrientationMatrix, 1, GL_FALSE,
                              prgm->var.OrientationMatrix);
    vgl->api.UniformMatrix4fv(prgm->uloc.ProjectionMatrix, 1, GL_FALSE,
                              prgm->var.ProjectionMatrix);
    vgl->api.UniformMatrix4fv(prgm->uloc.ZRotMatrix, 1, GL_FALSE,
                              prgm->var.ZRotMatrix);
    vgl->api.UniformMatrix4fv(prgm->uloc.YRotMatrix, 1, GL_FALSE,
                              prgm->var.YRotMatrix);
    vgl->api.UniformMatrix4fv(prgm->uloc.XRotMatrix, 1, GL_FALSE,
                              prgm->var.XRotMatrix);
    vgl->api.UniformMatrix4fv(prgm->uloc.ZoomMatrix, 1, GL_FALSE,
                              prgm->var.ZoomMatrix);

    glDrawElements(GL_TRIANGLES, vgl->nb_indices, GL_UNSIGNED_SHORT, 0);
}

int vout_display_opengl_Display(vout_display_opengl_t *vgl,
                                const video_format_t *source)
{
    /* Why drawing here and not in Render()? Because this way, the
       OpenGL providers can call vout_display_opengl_Display to force redraw.
       Currently, the OS X provider uses it to get a smooth window resizing */
    glClear(GL_COLOR_BUFFER_BIT);

    vgl->api.UseProgram(vgl->prgm->id);

    if (source->i_x_offset != vgl->last_source.i_x_offset
     || source->i_y_offset != vgl->last_source.i_y_offset
     || source->i_visible_width != vgl->last_source.i_visible_width
     || source->i_visible_height != vgl->last_source.i_visible_height)
    {
        float left[PICTURE_PLANE_MAX];
        float top[PICTURE_PLANE_MAX];
        float right[PICTURE_PLANE_MAX];
        float bottom[PICTURE_PLANE_MAX];
        for (unsigned j = 0; j < vgl->chroma->plane_count; j++)
        {
            float scale_w = (float)vgl->chroma->p[j].w.num / vgl->chroma->p[j].w.den
                          / vgl->tex_width[j];
            float scale_h = (float)vgl->chroma->p[j].h.num / vgl->chroma->p[j].h.den
                          / vgl->tex_height[j];

            /* Warning: if NPOT is not supported a larger texture is
               allocated. This will cause right and bottom coordinates to
               land on the edge of two texels with the texels to the
               right/bottom uninitialized by the call to
               glTexSubImage2D. This might cause a green line to appear on
               the right/bottom of the display.
               There are two possible solutions:
               - Manually mirror the edges of the texture.
               - Add a "-1" when computing right and bottom, however the
               last row/column might not be displayed at all.
            */
            left[j]   = (source->i_x_offset +                       0 ) * scale_w;
            top[j]    = (source->i_y_offset +                       0 ) * scale_h;
            right[j]  = (source->i_x_offset + source->i_visible_width ) * scale_w;
            bottom[j] = (source->i_y_offset + source->i_visible_height) * scale_h;
        }

        int ret = SetupCoords(vgl, left, top, right, bottom);
        if (ret != VLC_SUCCESS)
            return ret;

        vgl->last_source.i_x_offset = source->i_x_offset;
        vgl->last_source.i_y_offset = source->i_y_offset;
        vgl->last_source.i_visible_width = source->i_visible_width;
        vgl->last_source.i_visible_height = source->i_visible_height;
    }
    DrawWithShaders(vgl, vgl->prgm);

    /* Draw the subpictures */
    // Change the program for overlays
    struct prgm *prgm = vgl->sub_prgm;
    GLuint program = prgm->id;
    opengl_tex_converter_t *tc = &prgm->tc;
    vgl->api.UseProgram(program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* We need two buffer objects for each region: for vertex and texture coordinates. */
    if (2 * vgl->region_count > vgl->subpicture_buffer_object_count) {
        if (vgl->subpicture_buffer_object_count > 0)
            vgl->api.DeleteBuffers(vgl->subpicture_buffer_object_count,
                                   vgl->subpicture_buffer_object);
        vgl->subpicture_buffer_object_count = 0;

        int new_count = 2 * vgl->region_count;
        vgl->subpicture_buffer_object = realloc_or_free(vgl->subpicture_buffer_object, new_count * sizeof(GLuint));
        if (!vgl->subpicture_buffer_object)
            return VLC_ENOMEM;

        vgl->subpicture_buffer_object_count = new_count;
        vgl->api.GenBuffers(vgl->subpicture_buffer_object_count,
                            vgl->subpicture_buffer_object);
    }

    glActiveTexture(GL_TEXTURE0 + 0);
    glClientActiveTexture(GL_TEXTURE0 + 0);
    for (int i = 0; i < vgl->region_count; i++) {
        gl_region_t *glr = &vgl->region[i];
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
        glBindTexture(tc->tex_target, glr->texture);

        tc->pf_prepare_shader(tc, &glr->width, &glr->height, glr->alpha);

        vgl->api.BindBuffer(GL_ARRAY_BUFFER, vgl->subpicture_buffer_object[2 * i]);
        vgl->api.BufferData(GL_ARRAY_BUFFER, sizeof(textureCoord), textureCoord, GL_STATIC_DRAW);
        vgl->api.EnableVertexAttribArray(prgm->aloc.MultiTexCoord[0]);
        vgl->api.VertexAttribPointer(prgm->aloc.MultiTexCoord[0], 2, GL_FLOAT,
                                     0, 0, 0);

        vgl->api.BindBuffer(GL_ARRAY_BUFFER, vgl->subpicture_buffer_object[2 * i + 1]);
        vgl->api.BufferData(GL_ARRAY_BUFFER, sizeof(vertexCoord), vertexCoord, GL_STATIC_DRAW);
        vgl->api.EnableVertexAttribArray(prgm->aloc.VertexPosition);
        vgl->api.VertexAttribPointer(prgm->aloc.VertexPosition, 2, GL_FLOAT,
                                     0, 0, 0);

        vgl->api.UniformMatrix4fv(prgm->uloc.OrientationMatrix, 1, GL_FALSE,
                                  prgm->var.OrientationMatrix);
        vgl->api.UniformMatrix4fv(prgm->uloc.ProjectionMatrix, 1, GL_FALSE,
                                  prgm->var.ProjectionMatrix);
        vgl->api.UniformMatrix4fv(prgm->uloc.ZRotMatrix, 1, GL_FALSE,
                                  prgm->var.ZRotMatrix);
        vgl->api.UniformMatrix4fv(prgm->uloc.YRotMatrix, 1, GL_FALSE,
                                  prgm->var.YRotMatrix);
        vgl->api.UniformMatrix4fv(prgm->uloc.XRotMatrix, 1, GL_FALSE,
                                  prgm->var.XRotMatrix);
        vgl->api.UniformMatrix4fv(prgm->uloc.ZoomMatrix, 1, GL_FALSE,
                                  prgm->var.ZoomMatrix);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    glDisable(GL_BLEND);

    /* Display */
    vlc_gl_Swap(vgl->gl);

    return VLC_SUCCESS;
}

