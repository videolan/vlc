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

#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE 0x812F
#endif

#if defined(USE_OPENGL_ES2) || defined(__APPLE__)
#   define PFNGLGETPROGRAMIVPROC             typeof(glGetProgramiv)*
#   define PFNGLGETPROGRAMINFOLOGPROC        typeof(glGetProgramInfoLog)*
#   define PFNGLGETSHADERIVPROC              typeof(glGetShaderiv)*
#   define PFNGLGETSHADERINFOLOGPROC         typeof(glGetShaderInfoLog)*
#   define PFNGLGETUNIFORMLOCATIONPROC       typeof(glGetUniformLocation)*
#   define PFNGLGETATTRIBLOCATIONPROC        typeof(glGetAttribLocation)*
#   define PFNGLVERTEXATTRIBPOINTERPROC      typeof(glVertexAttribPointer)*
#   define PFNGLENABLEVERTEXATTRIBARRAYPROC  typeof(glEnableVertexAttribArray)*
#   define PFNGLUNIFORMMATRIX4FVPROC         typeof(glUniformMatrix4fv)*
#   define PFNGLUNIFORM4FVPROC               typeof(glUniform4fv)*
#   define PFNGLUNIFORM4FPROC                typeof(glUniform4f)*
#   define PFNGLUNIFORM1IPROC                typeof(glUniform1i)*
#   define PFNGLCREATESHADERPROC             typeof(glCreateShader)*
#   define PFNGLSHADERSOURCEPROC             typeof(glShaderSource)*
#   define PFNGLCOMPILESHADERPROC            typeof(glCompileShader)*
#   define PFNGLDELETESHADERPROC             typeof(glDeleteShader)*
#   define PFNGLCREATEPROGRAMPROC            typeof(glCreateProgram)*
#   define PFNGLLINKPROGRAMPROC              typeof(glLinkProgram)*
#   define PFNGLUSEPROGRAMPROC               typeof(glUseProgram)*
#   define PFNGLDELETEPROGRAMPROC            typeof(glDeleteProgram)*
#   define PFNGLATTACHSHADERPROC             typeof(glAttachShader)*
#   define PFNGLGENBUFFERSPROC               typeof(glGenBuffers)*
#   define PFNGLBINDBUFFERPROC               typeof(glBindBuffer)*
#   define PFNGLBUFFERDATAPROC               typeof(glBufferData)*
#   define PFNGLDELETEBUFFERSPROC            typeof(glDeleteBuffers)*
#if defined(__APPLE__)
#   import <CoreFoundation/CoreFoundation.h>
#endif
#endif

#if defined(USE_OPENGL_ES2)
#   define GLSL_VERSION "100"
#   define VLCGL_TEXTURE_COUNT 1
#   define PRECISION "precision highp float;"
#   define VLCGL_PICTURE_MAX 128
#   define glClientActiveTexture(x)
#else
#   define GLSL_VERSION "120"
#   define VLCGL_TEXTURE_COUNT 1
#   define VLCGL_PICTURE_MAX 128
#   define PRECISION ""
#endif

#ifndef GL_RED
#define GL_RED 0
#endif
#ifndef GL_R16
#define GL_R16 0
#endif

#define SPHERE_RADIUS 1.f

typedef struct {
    GLuint   texture;
    unsigned format;
    unsigned type;
    unsigned width;
    unsigned height;

    float    alpha;

    float    top;
    float    left;
    float    bottom;
    float    right;

    float    tex_width;
    float    tex_height;
} gl_region_t;

struct vout_display_opengl_t {

    vlc_gl_t   *gl;

    video_format_t fmt;
    const vlc_chroma_description_t *chroma;
    const vlc_chroma_description_t *sub_chroma;

    int        tex_target;
    int        tex_format;
    int        tex_internal;
    int        tex_type;

    int        tex_width[PICTURE_PLANE_MAX];
    int        tex_height[PICTURE_PLANE_MAX];

    GLuint     texture[VLCGL_TEXTURE_COUNT][PICTURE_PLANE_MAX];

    int         region_count;
    gl_region_t *region;


    picture_pool_t *pool;

    /* One YUV program and/or one RGBA program (for subpics) */
    GLuint     program[2];
    /* One YUV fragment shader and/or one RGBA fragment shader and
     * one vertex shader */
    GLint      shader[3];
    GLfloat    local_value[16];

    /* Index of main picture program */
    unsigned   program_idx;
    /* Index of subpicture program */
    unsigned   program_sub_idx;

    GLuint vertex_buffer_object;
    GLuint index_buffer_object;
    GLuint texture_buffer_object[PICTURE_PLANE_MAX];

    GLuint *subpicture_buffer_object;
    int    subpicture_buffer_object_count;

    /* Shader variables commands*/
    PFNGLGETUNIFORMLOCATIONPROC      GetUniformLocation;
    PFNGLGETATTRIBLOCATIONPROC       GetAttribLocation;
    PFNGLVERTEXATTRIBPOINTERPROC     VertexAttribPointer;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;

    PFNGLUNIFORMMATRIX4FVPROC   UniformMatrix4fv;
    PFNGLUNIFORM4FVPROC         Uniform4fv;
    PFNGLUNIFORM4FPROC          Uniform4f;
    PFNGLUNIFORM1IPROC          Uniform1i;

    /* Shader command */
    PFNGLCREATESHADERPROC CreateShader;
    PFNGLSHADERSOURCEPROC ShaderSource;
    PFNGLCOMPILESHADERPROC CompileShader;
    PFNGLDELETESHADERPROC   DeleteShader;

    PFNGLCREATEPROGRAMPROC CreateProgram;
    PFNGLLINKPROGRAMPROC   LinkProgram;
    PFNGLUSEPROGRAMPROC    UseProgram;
    PFNGLDELETEPROGRAMPROC DeleteProgram;

    PFNGLATTACHSHADERPROC  AttachShader;

    /* Shader log commands */
    PFNGLGETPROGRAMIVPROC  GetProgramiv;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog;
    PFNGLGETSHADERIVPROC   GetShaderiv;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;

    PFNGLGENBUFFERSPROC    GenBuffers;
    PFNGLBINDBUFFERPROC    BindBuffer;
    PFNGLBUFFERDATAPROC    BufferData;
    PFNGLDELETEBUFFERSPROC DeleteBuffers;

#if defined(_WIN32)
    PFNGLACTIVETEXTUREPROC  ActiveTexture;
    PFNGLCLIENTACTIVETEXTUREPROC  ClientActiveTexture;
#endif

    /* Non-power-of-2 texture size support */
    bool supports_npot;

    uint8_t *texture_temp_buf;
    size_t   texture_temp_buf_size;

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

static inline int GetAlignedSize(unsigned size)
{
    /* Return the smallest larger or equal power of 2 */
    unsigned align = 1 << (8 * sizeof (unsigned) - clz(size));
    return ((align >> 1) == size) ? size : align;
}

#if !defined(USE_OPENGL_ES2)
static int GetTexFormatSize(int target, int tex_format, int tex_internal,
                            int tex_type)
{
    GLint tex_param_size;
    switch (tex_format)
    {
        case GL_RED:
            tex_param_size = GL_TEXTURE_RED_SIZE;
            break;
        case GL_LUMINANCE:
            tex_param_size = GL_TEXTURE_LUMINANCE_SIZE;
            break;
        default:
            return -1;
    }
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(target, texture);
    glTexImage2D(target, 0, tex_internal, 64, 64, 0, tex_format, tex_type, NULL);
    GLint size = 0;
    glGetTexLevelParameteriv(target, 0, tex_param_size, &size);

    glDeleteTextures(1, &texture);
    return size;
}
#endif

static void BuildVertexShader(vout_display_opengl_t *vgl,
                              GLint *shader)
{
    /* Basic vertex shader */
    const char *vertexShader =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "varying vec4 TexCoord0,TexCoord1, TexCoord2;"
        "attribute vec4 MultiTexCoord0,MultiTexCoord1,MultiTexCoord2;"
        "attribute vec3 VertexPosition;"
        "uniform mat4 OrientationMatrix;"
        "uniform mat4 ProjectionMatrix;"
        "uniform mat4 XRotMatrix;"
        "uniform mat4 YRotMatrix;"
        "uniform mat4 ZRotMatrix;"
        "uniform mat4 ZoomMatrix;"
        "void main() {"
        " TexCoord0 = MultiTexCoord0;"
        " TexCoord1 = MultiTexCoord1;"
        " TexCoord2 = MultiTexCoord2;"
        " gl_Position = ProjectionMatrix * OrientationMatrix * ZoomMatrix * ZRotMatrix * XRotMatrix * YRotMatrix * vec4(VertexPosition, 1.0);"
        "}";

    *shader = vgl->CreateShader(GL_VERTEX_SHADER);
    vgl->ShaderSource(*shader, 1, &vertexShader, NULL);
    vgl->CompileShader(*shader);
}

static void BuildYUVFragmentShader(vout_display_opengl_t *vgl,
                                   GLint *shader,
                                   GLfloat *local_value,
                                   const video_format_t *fmt,
                                   float yuv_range_correction)

{
    /* [R/G/B][Y U V O] from TV range to full range
     * XXX we could also do hue/brightness/constrast/gamma
     * by simply changing the coefficients
     */
    const float matrix_bt601_tv2full[12] = {
        1.164383561643836,  0.0000,             1.596026785714286, -0.874202217873451 ,
        1.164383561643836, -0.391762290094914, -0.812967647237771,  0.531667823499146 ,
        1.164383561643836,  2.017232142857142,  0.0000,            -1.085630789302022 ,
    };
    const float matrix_bt709_tv2full[12] = {
        1.164383561643836,  0.0000,             1.792741071428571, -0.972945075016308 ,
        1.164383561643836, -0.21324861427373,  -0.532909328559444,  0.301482665475862 ,
        1.164383561643836,  2.112401785714286,  0.0000,            -1.133402217873451 ,
    };
    const float *matrix;
    switch( fmt->space )
    {
        case COLOR_SPACE_BT601:
            matrix = matrix_bt601_tv2full;
            break;
        default:
            matrix = matrix_bt709_tv2full;
    };

    /* Basic linear YUV -> RGB conversion using bilinear interpolation */
    const char *template_glsl_yuv =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "uniform sampler2D Texture0;"
        "uniform sampler2D Texture1;"
        "uniform sampler2D Texture2;"
        "uniform vec4      Coefficient[4];"
        "varying vec4      TexCoord0,TexCoord1,TexCoord2;"

        "void main(void) {"
        " vec4 x,y,z,result;"

        /* The texture format can be GL_RED: vec4(R,0,0,1) or GL_LUMINANCE:
         * vec4(L,L,L,1). The following transform a vec4(x, y, z, w) into a
         * vec4(x, x, x, 1) (we may want to use texture swizzling starting
         * OpenGL 3.3). */
        " float val0 = texture2D(Texture0, TexCoord0.st).x;"
        " float val1 = texture2D(Texture1, TexCoord1.st).x;"
        " float val2 = texture2D(Texture2, TexCoord2.st).x;"
        " x  = vec4(val0, val0, val0, 1);"
        " %c = vec4(val1, val1, val1, 1);"
        " %c = vec4(val2, val2, val2, 1);"

        " result = x * Coefficient[0] + Coefficient[3];"
        " result = (y * Coefficient[1]) + result;"
        " result = (z * Coefficient[2]) + result;"
        " gl_FragColor = result;"
        "}";
    bool swap_uv = fmt->i_chroma == VLC_CODEC_YV12 ||
                   fmt->i_chroma == VLC_CODEC_YV9;

    char *code;
    if (asprintf(&code, template_glsl_yuv,
                 swap_uv ? 'z' : 'y',
                 swap_uv ? 'y' : 'z') < 0)
        return;

    for (int i = 0; i < 4; i++) {
        float correction = i < 3 ? yuv_range_correction : 1.f;
        /* We place coefficient values for coefficient[4] in one array from matrix values.
           Notice that we fill values from top down instead of left to right.*/
        for (int j = 0; j < 4; j++)
            local_value[i*4+j] = j < 3 ? correction * matrix[j*4+i] : 0.f;
    }

    *shader = vgl->CreateShader(GL_FRAGMENT_SHADER);
    vgl->ShaderSource(*shader, 1, (const char **)&code, NULL);
    vgl->CompileShader(*shader);

    free(code);
}

#if 0
static void BuildRGBFragmentShader(vout_display_opengl_t *vgl,
                                   GLint *shader)
{
    // Simple shader for RGB
    const char *code =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "uniform sampler2D Texture[3];"
        "varying vec4 TexCoord0,TexCoord1,TexCoord2;"
        "void main()"
        "{ "
        "  gl_FragColor = texture2D(Texture[0], TexCoord0.st);"
        "}";
    *shader = vgl->CreateShader(GL_FRAGMENT_SHADER);
    vgl->ShaderSource(*shader, 1, &code, NULL);
    vgl->CompileShader(*shader);
}
#endif

static void BuildRGBAFragmentShader(vout_display_opengl_t *vgl,
                                   GLint *shader)
{
    // Simple shader for RGBA
    const char *code =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "uniform sampler2D Texture;"
        "uniform vec4 FillColor;"
        "varying vec4 TexCoord0;"
        "void main()"
        "{ "
        "  gl_FragColor = texture2D(Texture, TexCoord0.st) * FillColor;"
        "}";
    *shader = vgl->CreateShader(GL_FRAGMENT_SHADER);
    vgl->ShaderSource(*shader, 1, &code, NULL);
    vgl->CompileShader(*shader);
}

static void BuildXYZFragmentShader(vout_display_opengl_t *vgl,
                                   GLint *shader)
{
    /* Shader for XYZ to RGB correction
     * 3 steps :
     *  - XYZ gamma correction
     *  - XYZ to RGB matrix conversion
     *  - reverse RGB gamma correction
     */
      const char *code =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "uniform sampler2D Texture0;"
        "uniform vec4 xyz_gamma = vec4(2.6);"
        "uniform vec4 rgb_gamma = vec4(1.0/2.2);"
        // WARN: matrix Is filled column by column (not row !)
        "uniform mat4 matrix_xyz_rgb = mat4("
        "    3.240454 , -0.9692660, 0.0556434, 0.0,"
        "   -1.5371385,  1.8760108, -0.2040259, 0.0,"
        "    -0.4985314, 0.0415560, 1.0572252,  0.0,"
        "    0.0,      0.0,         0.0,        1.0 "
        " );"

        "varying vec4 TexCoord0;"
        "void main()"
        "{ "
        " vec4 v_in, v_out;"
        " v_in  = texture2D(Texture0, TexCoord0.st);"
        " v_in = pow(v_in, xyz_gamma);"
        " v_out = matrix_xyz_rgb * v_in ;"
        " v_out = pow(v_out, rgb_gamma) ;"
        " v_out = clamp(v_out, 0.0, 1.0) ;"
        " gl_FragColor = v_out;"
        "}";
    *shader = vgl->CreateShader(GL_FRAGMENT_SHADER);
    vgl->ShaderSource(*shader, 1, &code, NULL);
    vgl->CompileShader(*shader);
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
    const bool oglv3 = strverscmp((const char *)ogl_version, "3.0") >= 0;
    const int yuv_plane_texformat = oglv3 ? GL_RED : GL_LUMINANCE;
    const int yuv_plane_texformat_16 = oglv3 ? GL_R16 : GL_LUMINANCE16;
#else
    bool supports_shaders = true;
    const int yuv_plane_texformat = GL_LUMINANCE;
#endif

#if defined(USE_OPENGL_ES2)
#define GET_PROC_ADDR(name) name
#else
#define GET_PROC_ADDR(name) vlc_gl_GetProcAddress(gl, #name)
#endif
    vgl->CreateShader       = GET_PROC_ADDR(glCreateShader);
    vgl->ShaderSource       = GET_PROC_ADDR(glShaderSource);
    vgl->CompileShader      = GET_PROC_ADDR(glCompileShader);
    vgl->AttachShader       = GET_PROC_ADDR(glAttachShader);

    vgl->GetProgramiv       = GET_PROC_ADDR(glGetProgramiv);
    vgl->GetShaderiv        = GET_PROC_ADDR(glGetShaderiv);
    vgl->GetProgramInfoLog  = GET_PROC_ADDR(glGetProgramInfoLog);
    vgl->GetShaderInfoLog   = GET_PROC_ADDR(glGetShaderInfoLog);

    vgl->DeleteShader       = GET_PROC_ADDR(glDeleteShader);

    vgl->GetUniformLocation      = GET_PROC_ADDR(glGetUniformLocation);
    vgl->GetAttribLocation       = GET_PROC_ADDR(glGetAttribLocation);
    vgl->VertexAttribPointer     = GET_PROC_ADDR(glVertexAttribPointer);
    vgl->EnableVertexAttribArray = GET_PROC_ADDR(glEnableVertexAttribArray);
    vgl->UniformMatrix4fv        = GET_PROC_ADDR(glUniformMatrix4fv);
    vgl->Uniform4fv              = GET_PROC_ADDR(glUniform4fv);
    vgl->Uniform4f               = GET_PROC_ADDR(glUniform4f);
    vgl->Uniform1i               = GET_PROC_ADDR(glUniform1i);

    vgl->CreateProgram = GET_PROC_ADDR(glCreateProgram);
    vgl->LinkProgram   = GET_PROC_ADDR(glLinkProgram);
    vgl->UseProgram    = GET_PROC_ADDR(glUseProgram);
    vgl->DeleteProgram = GET_PROC_ADDR(glDeleteProgram);

    vgl->GenBuffers    = GET_PROC_ADDR(glGenBuffers);
    vgl->BindBuffer    = GET_PROC_ADDR(glBindBuffer);
    vgl->BufferData    = GET_PROC_ADDR(glBufferData);
    vgl->DeleteBuffers = GET_PROC_ADDR(glDeleteBuffers);
#undef GET_PROC_ADDR

    if (!vgl->CreateShader || !vgl->ShaderSource || !vgl->CreateProgram)
        supports_shaders = false;
    if (!supports_shaders)
    {
        msg_Err(gl, "shaders not supported");
        free(vgl);
        return NULL;
    }

#if defined(_WIN32)
    vgl->ActiveTexture = (PFNGLACTIVETEXTUREPROC)vlc_gl_GetProcAddress(gl, "glActiveTexture");
    vgl->ClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC)vlc_gl_GetProcAddress(gl, "glClientActiveTexture");
#   define glActiveTexture vgl->ActiveTexture
#   define glClientActiveTexture vgl->ClientActiveTexture
#endif

    vgl->supports_npot = HasExtension(extensions, "GL_ARB_texture_non_power_of_two") ||
                         HasExtension(extensions, "GL_APPLE_texture_2D_limited_npot");

#if defined(USE_OPENGL_ES2)
    /* OpenGL ES 2 includes support for non-power of 2 textures by specification
     * so checks for extensions are bound to fail. Check for OpenGL ES version instead. */
    vgl->supports_npot = true;
#endif

    GLint max_texture_units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);

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
    vgl->tex_target   = GL_TEXTURE_2D;
    vgl->tex_format   = GL_RGBA;
    vgl->tex_internal = GL_RGBA;
    vgl->tex_type     = GL_UNSIGNED_BYTE;
    /* Use YUV if possible and needed */
    float yuv_range_correction = 1.0;

    if (max_texture_units >= 3 && vlc_fourcc_IsYUV(fmt->i_chroma)) {
        const vlc_fourcc_t *list = vlc_fourcc_GetYUVFallback(fmt->i_chroma);
        while (*list) {
            const vlc_chroma_description_t *dsc = vlc_fourcc_GetChromaDescription(*list);
            if (dsc && dsc->plane_count == 3 && dsc->pixel_size == 1) {
                vgl->fmt          = *fmt;
                vgl->fmt.i_chroma = *list;
                vgl->tex_format   = yuv_plane_texformat;
                vgl->tex_internal = yuv_plane_texformat;
                vgl->tex_type     = GL_UNSIGNED_BYTE;
                yuv_range_correction = 1.0;
                break;
#if !defined(USE_OPENGL_ES2)
            } else if (dsc && dsc->plane_count == 3 && dsc->pixel_size == 2 &&
                       GetTexFormatSize(vgl->tex_target,
                                        yuv_plane_texformat,
                                        yuv_plane_texformat_16,
                                        GL_UNSIGNED_SHORT) == 16) {
                vgl->fmt          = *fmt;
                vgl->fmt.i_chroma = *list;
                vgl->tex_format   = yuv_plane_texformat;
                vgl->tex_internal = yuv_plane_texformat_16;
                vgl->tex_type     = GL_UNSIGNED_SHORT;
                yuv_range_correction = (float)((1 << 16) - 1) / ((1 << dsc->pixel_bits) - 1);
                break;
#endif
            }
            list++;
        }
    }

    if (fmt->i_chroma == VLC_CODEC_XYZ12) {
        vgl->fmt          = *fmt;
        vgl->fmt.i_chroma = VLC_CODEC_XYZ12;
        vgl->tex_format   = GL_RGB;
        vgl->tex_internal = GL_RGB;
        vgl->tex_type     = GL_UNSIGNED_SHORT;
    }

    /* Build program if needed */
    vgl->program[0] =
    vgl->program[1] = 0;
    vgl->shader[0] =
    vgl->shader[1] =
    vgl->shader[2] = -1;
    unsigned nb_shaders = 0;
    int vertex_shader_idx = -1, fragment_shader_idx = -1,
        rgba_fragment_shader_idx = -1;

    if (vgl->fmt.i_chroma == VLC_CODEC_XYZ12)
    {
        fragment_shader_idx = nb_shaders++;
        BuildXYZFragmentShader(vgl, &vgl->shader[fragment_shader_idx]);
    }
    else if (vlc_fourcc_IsYUV(vgl->fmt.i_chroma))
    {
        fragment_shader_idx = nb_shaders++;
        BuildYUVFragmentShader(vgl, &vgl->shader[fragment_shader_idx],
                               vgl->local_value, fmt, yuv_range_correction);
    }

    rgba_fragment_shader_idx = nb_shaders++;
    BuildRGBAFragmentShader(vgl, &vgl->shader[rgba_fragment_shader_idx]);

    vertex_shader_idx = nb_shaders++;
    BuildVertexShader(vgl, &vgl->shader[vertex_shader_idx]);

    /* Check shaders messages */
    for (unsigned j = 0; j < nb_shaders; j++) {
        int infoLength;
        vgl->GetShaderiv(vgl->shader[j], GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength <= 1)
            continue;

        char *infolog = malloc(infoLength);
        if (infolog != NULL)
        {
            int charsWritten;
            vgl->GetShaderInfoLog(vgl->shader[j], infoLength, &charsWritten,
                                  infolog);
            msg_Err(gl, "shader %d: %s", j, infolog);
            free(infolog);
        }
    }
    assert(vertex_shader_idx != -1 && rgba_fragment_shader_idx != -1);

    unsigned nb_programs = 0;
    GLuint program;
    int program_idx = -1, rgba_program_idx = -1;

    /* YUV/XYZ & Vertex shaders */
    if (fragment_shader_idx != -1)
    {
        program_idx = nb_programs++;

        program = vgl->program[program_idx] = vgl->CreateProgram();
        vgl->AttachShader(program, vgl->shader[fragment_shader_idx]);
        vgl->AttachShader(program, vgl->shader[vertex_shader_idx]);
        vgl->LinkProgram(program);
    }

    /* RGB & Vertex shaders */
    rgba_program_idx = nb_programs++;
    program = vgl->program[rgba_program_idx] = vgl->CreateProgram();
    vgl->AttachShader(program, vgl->shader[rgba_fragment_shader_idx]);
    vgl->AttachShader(program, vgl->shader[vertex_shader_idx]);
    vgl->LinkProgram(program);

    vgl->program_idx = program_idx != -1 ? program_idx : rgba_program_idx;
    vgl->program_sub_idx = rgba_program_idx;

    /* Check program messages */
    for (GLuint i = 0; i < nb_programs; i++) {
        int infoLength = 0;
        vgl->GetProgramiv(vgl->program[i], GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength <= 1)
            continue;
        char *infolog = malloc(infoLength);
        if (infolog != NULL)
        {
            int charsWritten;
            vgl->GetProgramInfoLog(vgl->program[i], infoLength, &charsWritten,
                                   infolog);
            msg_Err(gl, "shader program %d: %s", i, infolog);
            free(infolog);
        }

        /* If there is some message, better to check linking is ok */
        GLint link_status = GL_TRUE;
        vgl->GetProgramiv(vgl->program[i], GL_LINK_STATUS, &link_status);
        if (link_status == GL_FALSE) {
            msg_Err(gl, "Unable to use program %d\n", i);
            vout_display_opengl_Delete(vgl);
            return NULL;
        }
    }

    vgl->chroma = vlc_fourcc_GetChromaDescription(vgl->fmt.i_chroma);
    vgl->sub_chroma = vlc_fourcc_GetChromaDescription(VLC_CODEC_RGB32);
    assert(vgl->chroma != NULL && vgl->sub_chroma != NULL);

    /* Texture size */
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        int w = vgl->fmt.i_visible_width  * vgl->chroma->p[j].w.num
              / vgl->chroma->p[j].w.den;
        int h = vgl->fmt.i_visible_height * vgl->chroma->p[j].h.num
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

    vgl->GenBuffers(1, &vgl->vertex_buffer_object);
    vgl->GenBuffers(1, &vgl->index_buffer_object);
    vgl->GenBuffers(vgl->chroma->plane_count, vgl->texture_buffer_object);

    /* Initial number of allocated buffer objects for subpictures, will grow dynamically. */
    int subpicture_buffer_object_count = 8;
    vgl->subpicture_buffer_object = malloc(subpicture_buffer_object_count * sizeof(GLuint));
    if (!vgl->subpicture_buffer_object) {
        vout_display_opengl_Delete(vgl);
        return NULL;
    }
    vgl->subpicture_buffer_object_count = subpicture_buffer_object_count;
    vgl->GenBuffers(vgl->subpicture_buffer_object_count, vgl->subpicture_buffer_object);

    /* */
    for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++) {
        for (int j = 0; j < PICTURE_PLANE_MAX; j++)
            vgl->texture[i][j] = 0;
    }
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

    for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++)
        glDeleteTextures(vgl->chroma->plane_count, vgl->texture[i]);
    for (int i = 0; i < vgl->region_count; i++) {
        if (vgl->region[i].texture)
            glDeleteTextures(1, &vgl->region[i].texture);
    }
    free(vgl->region);

    for (int i = 0; i < 2 && vgl->program[i] != 0; i++)
        vgl->DeleteProgram(vgl->program[i]);
    for (int i = 0; i < 3 && vgl->shader[i] != 0; i++)
        vgl->DeleteShader(vgl->shader[i]);
    vgl->DeleteBuffers(1, &vgl->vertex_buffer_object);
    vgl->DeleteBuffers(1, &vgl->index_buffer_object);
    vgl->DeleteBuffers(vgl->chroma->plane_count, vgl->texture_buffer_object);
    if (vgl->subpicture_buffer_object_count > 0)
        vgl->DeleteBuffers(vgl->subpicture_buffer_object_count, vgl->subpicture_buffer_object);
    free(vgl->subpicture_buffer_object);

    free(vgl->texture_temp_buf);

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
}

static void GenTextures(GLenum tex_target, GLint tex_internal,
                        GLenum tex_format, GLenum tex_type, GLsizei n,
                        GLsizei *tex_width, GLsizei *tex_height,
                        GLuint * textures)
{
    glGenTextures(n, textures);
    for (GLsizei j = 0; j < n; j++) {
        glActiveTexture(GL_TEXTURE0 + j);
        glClientActiveTexture(GL_TEXTURE0 + j);
        glBindTexture(tex_target, textures[j]);

#if !defined(USE_OPENGL_ES2)
        /* Set the texture parameters */
        glTexParameterf(tex_target, GL_TEXTURE_PRIORITY, 1.0);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

        glTexParameteri(tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        /* Call glTexImage2D only once, and use glTexSubImage2D later */
        glTexImage2D(tex_target, 0, tex_internal, tex_width[j], tex_height[j],
                     0, tex_format, tex_type, NULL);
    }
}

picture_pool_t *vout_display_opengl_GetPool(vout_display_opengl_t *vgl, unsigned requested_count)
{
    if (vgl->pool)
        return vgl->pool;

    /* Allocates our textures */
    for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++)
        GenTextures(vgl->tex_target, vgl->tex_internal, vgl->tex_format,
                    vgl->tex_type, vgl->chroma->plane_count,
                    vgl->tex_width, vgl->tex_height, vgl->texture[i]);

    /* Allocate our pictures */
    picture_t *picture[VLCGL_PICTURE_MAX] = {NULL, };
    unsigned count;
    for (count = 0; count < __MIN(VLCGL_PICTURE_MAX, requested_count); count++)
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
    for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++)
    {
        glDeleteTextures(vgl->chroma->plane_count, vgl->texture[i]);
        memset(vgl->texture[i], 0, PICTURE_PLANE_MAX * sizeof(GLuint));
    }
    return NULL;
}

#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))
static void UploadPlane(vout_display_opengl_t *vgl,
                        unsigned width, unsigned height,
                        unsigned pitch, unsigned pixel_pitch,
                        const void *pixels,
                        int tex_target, int tex_format, int tex_type)
{
    // This unpack alignment is the default, but setting it just in case.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#ifndef GL_UNPACK_ROW_LENGTH
    unsigned dst_width = width;
    unsigned dst_pitch = ALIGN(dst_width * pixel_pitch, 4);
    if ( pitch != dst_pitch )
    {
        size_t buf_size = dst_pitch * height * pixel_pitch;
        const uint8_t *source = pixels;
        uint8_t *destination;
        if( vgl->texture_temp_buf_size < buf_size )
        {
            vgl->texture_temp_buf =
                realloc_or_free( vgl->texture_temp_buf, buf_size );
            if (vgl->texture_temp_buf == NULL)
            {
                vgl->texture_temp_buf_size = 0;
                return;
            }
            vgl->texture_temp_buf_size = buf_size;
        }
        destination = vgl->texture_temp_buf;

        for( unsigned h = 0; h < height ; h++ )
        {
            memcpy( destination, source, width * pixel_pitch );
            source += pitch;
            destination += dst_pitch;
        }
        glTexSubImage2D( tex_target, 0, 0, 0, width, height,
                         tex_format, tex_type, vgl->texture_temp_buf );
    } else {
#else
    (void) vgl;
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / pixel_pitch);
#endif
        glTexSubImage2D(tex_target, 0, 0, 0, width, height,
                        tex_format, tex_type, pixels);
    }
}

static void UpdatePic(vout_display_opengl_t *vgl,
                      const vlc_chroma_description_t *chroma,
                      GLuint *textures, unsigned width, unsigned height,
                      const picture_t *pic, const size_t *plane_offset,
                      int tex_target, int tex_format, int tex_type)
{
    for (unsigned j = 0; j < chroma->plane_count; j++)
    {
        glActiveTexture(GL_TEXTURE0 + j);
        glClientActiveTexture(GL_TEXTURE0 + j);
        glBindTexture(tex_target, textures[j]);
        const void *pixels = plane_offset != NULL ?
                             &pic->p[j].p_pixels[plane_offset[j]] :
                             pic->p[j].p_pixels;

        UploadPlane(vgl, width * chroma->p[j].w.num / chroma->p[j].w.den,
                    height * chroma->p[j].h.num / chroma->p[j].h.den,
                    pic->p[j].i_pitch, pic->p[j].i_pixel_pitch, pixels,
                    tex_target, tex_format, tex_type);
    }
}

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture, subpicture_t *subpicture)
{
    /* Update the texture */
    UpdatePic(vgl, vgl->chroma, vgl->texture[0],
              vgl->fmt.i_visible_width, vgl->fmt.i_visible_height,
              picture, NULL, vgl->tex_target, vgl->tex_format, vgl->tex_type);

    int         last_count = vgl->region_count;
    gl_region_t *last = vgl->region;

    vgl->region_count = 0;
    vgl->region       = NULL;

    if (subpicture) {

        int count = 0;
        for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next)
            count++;

        vgl->region_count = count;
        vgl->region       = calloc(count, sizeof(*vgl->region));

        int i = 0;
        for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next, i++) {
            gl_region_t *glr = &vgl->region[i];

            glr->format = GL_RGBA;
            glr->type   = GL_UNSIGNED_BYTE;
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
                    last[j].height == glr->height &&
                    last[j].format == glr->format &&
                    last[j].type   == glr->type) {
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
                GLsizei tex_width = glr->width, tex_height = glr->height;
                GenTextures(GL_TEXTURE_2D, glr->format, glr->format, glr->type,
                            1, &tex_width, &tex_height, &glr->texture);
            }
            UpdatePic(vgl, vgl->sub_chroma, &glr->texture,
                      r->fmt.i_visible_width, r->fmt.i_visible_height,
                      r->p_picture, &pixels_offset,
                      GL_TEXTURE_2D, glr->format, glr->type);
        }
    }
    for (int i = 0; i < last_count; i++) {
        if (last[i].texture)
            glDeleteTextures(1, &last[i].texture);
    }
    free(last);

    VLC_UNUSED(subpicture);
    return VLC_SUCCESS;
}

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

void orientationTransformMatrix(GLfloat matrix[static 16], video_orientation_t orientation) {
    memcpy(matrix, identity, sizeof(identity));

    const int k_cos_pi = -1;
    const int k_cos_pi_2 = 0;
    const int k_cos_n_pi_2 = 0;

    const int k_sin_pi = 0;
    const int k_sin_pi_2 = 1;
    const int k_sin_n_pi_2 = -1;

    bool rotate = false;
    int cos = 0, sin = 0;

    switch (orientation) {

        case ORIENT_ROTATED_90:
            cos = k_cos_pi_2;
            sin = k_sin_pi_2;
            rotate = true;
            break;
        case ORIENT_ROTATED_180:
            cos = k_cos_pi;
            sin = k_sin_pi;
            rotate = true;
            break;
        case ORIENT_ROTATED_270:
            cos = k_cos_n_pi_2;
            sin = k_sin_n_pi_2;
            rotate = true;
            break;
        case ORIENT_HFLIPPED:
            matrix[0 * 4 + 0] = -1;
            break;
        case ORIENT_VFLIPPED:
            matrix[1 * 4 + 1] = -1;
            break;
        case ORIENT_TRANSPOSED:
            matrix[0 * 4 + 0] = 0;
            matrix[0 * 4 + 1] = -1;
            matrix[1 * 4 + 0] = -1;
            matrix[1 * 4 + 1] = 0;
            break;
        case ORIENT_ANTI_TRANSPOSED:
            matrix[0 * 4 + 0] = 0;
            matrix[0 * 4 + 1] = 1;
            matrix[1 * 4 + 0] = 1;
            matrix[1 * 4 + 1] = 0;
            break;
        default:
            break;
    }

    if (rotate) {

        matrix[0 * 4 + 0] = cos;
        matrix[0 * 4 + 1] = -sin;
        matrix[1 * 4 + 0] = sin;
        matrix[1 * 4 + 1] = cos;
    }
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

static void DrawWithShaders(vout_display_opengl_t *vgl,
                            const float *left, const float *top,
                            const float *right, const float *bottom,
                            unsigned int program_idx)
{
    GLuint program = vgl->program[program_idx];
    vgl->UseProgram(program);
    if (vlc_fourcc_IsYUV(vgl->fmt.i_chroma)
     || vgl->fmt.i_chroma == VLC_CODEC_XYZ12) { /* FIXME: ugly */
        if (vgl->chroma->plane_count == 3) {
            vgl->Uniform4fv(vgl->GetUniformLocation(program,
                            "Coefficient"), 4, vgl->local_value);
            vgl->Uniform1i(vgl->GetUniformLocation(program, "Texture0"), 0);
            vgl->Uniform1i(vgl->GetUniformLocation(program, "Texture1"), 1);
            vgl->Uniform1i(vgl->GetUniformLocation(program, "Texture2"), 2);
        }
        else if (vgl->chroma->plane_count == 1) {
            vgl->Uniform1i(vgl->GetUniformLocation(program, "Texture0"), 0);
        }
    } else {
        vgl->Uniform1i(vgl->GetUniformLocation(program, "Texture0"), 0);
        vgl->Uniform4f(vgl->GetUniformLocation(program, "FillColor"),
                       1.0f, 1.0f, 1.0f, 1.0f);
    }

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
        return;

    GLfloat projectionMatrix[16],
            zRotMatrix[16], yRotMatrix[16], xRotMatrix[16],
            zoomMatrix[16], orientationMatrix[16];

    orientationTransformMatrix(orientationMatrix, vgl->fmt.orientation);

    if (vgl->fmt.projection_mode == PROJECTION_MODE_EQUIRECTANGULAR
        || vgl->fmt.projection_mode == PROJECTION_MODE_CUBEMAP_LAYOUT_STANDARD)
    {
        float sar = (float) vgl->f_sar;
        getProjectionMatrix(sar, vgl->f_fovy, projectionMatrix);
        getYRotMatrix(vgl->f_teta, yRotMatrix);
        getXRotMatrix(vgl->f_phi, xRotMatrix);
        getZRotMatrix(vgl->f_roll, zRotMatrix);
        getZoomMatrix(vgl->f_z, zoomMatrix);
    }
    else
    {
        memcpy(projectionMatrix, identity, sizeof(identity));
        memcpy(zRotMatrix, identity, sizeof(identity));
        memcpy(yRotMatrix, identity, sizeof(identity));
        memcpy(xRotMatrix, identity, sizeof(identity));
        memcpy(zoomMatrix, identity, sizeof(identity));
    }

    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        glActiveTexture(GL_TEXTURE0+j);
        glClientActiveTexture(GL_TEXTURE0+j);
        glBindTexture(vgl->tex_target, vgl->texture[0][j]);

        vgl->BindBuffer(GL_ARRAY_BUFFER, vgl->texture_buffer_object[j]);
        vgl->BufferData(GL_ARRAY_BUFFER, nbVertices * 2 * sizeof(GLfloat),
                        textureCoord + j * nbVertices * 2, GL_STATIC_DRAW);

        char attribute[20];
        snprintf(attribute, sizeof(attribute), "MultiTexCoord%1d", j);
        vgl->EnableVertexAttribArray(vgl->GetAttribLocation(program, attribute));
        vgl->VertexAttribPointer(vgl->GetAttribLocation(program, attribute), 2,
                                 GL_FLOAT, 0, 0, 0);
    }
    free(textureCoord);
    glActiveTexture(GL_TEXTURE0 + 0);
    glClientActiveTexture(GL_TEXTURE0 + 0);

    vgl->BindBuffer(GL_ARRAY_BUFFER, vgl->vertex_buffer_object);
    vgl->BufferData(GL_ARRAY_BUFFER, nbVertices * 3 * sizeof(GLfloat), vertexCoord, GL_STATIC_DRAW);
    free(vertexCoord);
    vgl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vgl->index_buffer_object);
    vgl->BufferData(GL_ELEMENT_ARRAY_BUFFER, nbIndices * sizeof(GLushort), indices, GL_STATIC_DRAW);
    free(indices);
    vgl->EnableVertexAttribArray(vgl->GetAttribLocation(program,
                                 "VertexPosition"));
    vgl->VertexAttribPointer(vgl->GetAttribLocation(program, "VertexPosition"),
                             3, GL_FLOAT, 0, 0, 0);

    vgl->UniformMatrix4fv(vgl->GetUniformLocation(program, "OrientationMatrix"),
                          1, GL_FALSE, orientationMatrix);
    vgl->UniformMatrix4fv(vgl->GetUniformLocation(program, "ProjectionMatrix"),
                          1, GL_FALSE, projectionMatrix);
    vgl->UniformMatrix4fv(vgl->GetUniformLocation(program, "ZRotMatrix"),
                          1, GL_FALSE, zRotMatrix);
    vgl->UniformMatrix4fv(vgl->GetUniformLocation(program, "YRotMatrix"),
                          1, GL_FALSE, yRotMatrix);
    vgl->UniformMatrix4fv(vgl->GetUniformLocation(program, "XRotMatrix"),
                          1, GL_FALSE, xRotMatrix);
    vgl->UniformMatrix4fv(vgl->GetUniformLocation(program, "ZoomMatrix"),
                          1, GL_FALSE, zoomMatrix);

    vgl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vgl->index_buffer_object);
    glDrawElements(GL_TRIANGLES, nbIndices, GL_UNSIGNED_SHORT, 0);
}

int vout_display_opengl_Display(vout_display_opengl_t *vgl,
                                const video_format_t *source)
{
    /* Why drawing here and not in Render()? Because this way, the
       OpenGL providers can call vout_display_opengl_Display to force redraw.i
       Currently, the OS X provider uses it to get a smooth window resizing */
    glClear(GL_COLOR_BUFFER_BIT);

    /* Draw the picture */
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

    DrawWithShaders(vgl, left, top, right, bottom, vgl->program_idx);

    /* Draw the subpictures */
    // Change the program for overlays
    GLuint sub_program = vgl->program[vgl->program_sub_idx];
    vgl->UseProgram(sub_program);
    vgl->Uniform1i(vgl->GetUniformLocation(sub_program, "Texture"), 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* We need two buffer objects for each region: for vertex and texture coordinates. */
    if (2 * vgl->region_count > vgl->subpicture_buffer_object_count) {
        if (vgl->subpicture_buffer_object_count > 0)
            vgl->DeleteBuffers(vgl->subpicture_buffer_object_count, vgl->subpicture_buffer_object);
        vgl->subpicture_buffer_object_count = 0;

        int new_count = 2 * vgl->region_count;
        vgl->subpicture_buffer_object = realloc_or_free(vgl->subpicture_buffer_object, new_count * sizeof(GLuint));
        if (!vgl->subpicture_buffer_object)
            return VLC_ENOMEM;

        vgl->subpicture_buffer_object_count = new_count;
        vgl->GenBuffers(vgl->subpicture_buffer_object_count, vgl->subpicture_buffer_object);
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

        glBindTexture(GL_TEXTURE_2D, glr->texture);
        vgl->Uniform4f(vgl->GetUniformLocation(sub_program, "FillColor"),
                       1.0f, 1.0f, 1.0f, glr->alpha);

        vgl->BindBuffer(GL_ARRAY_BUFFER, vgl->subpicture_buffer_object[2 * i]);
        vgl->BufferData(GL_ARRAY_BUFFER, sizeof(textureCoord), textureCoord, GL_STATIC_DRAW);
        vgl->EnableVertexAttribArray(vgl->GetAttribLocation(sub_program,
                                     "MultiTexCoord0"));
        vgl->VertexAttribPointer(vgl->GetAttribLocation(sub_program,
                                 "MultiTexCoord0"), 2, GL_FLOAT, 0, 0, 0);

        vgl->BindBuffer(GL_ARRAY_BUFFER, vgl->subpicture_buffer_object[2 * i + 1]);
        vgl->BufferData(GL_ARRAY_BUFFER, sizeof(vertexCoord), vertexCoord, GL_STATIC_DRAW);
        vgl->EnableVertexAttribArray(vgl->GetAttribLocation(sub_program,
                                     "VertexPosition"));
        vgl->VertexAttribPointer(vgl->GetAttribLocation(sub_program,
                                 "VertexPosition"), 2, GL_FLOAT, 0, 0, 0);

        // Subpictures have the correct orientation:
        vgl->UniformMatrix4fv(vgl->GetUniformLocation(sub_program,
                              "OrientationMatrix"), 1, GL_FALSE, identity);
        vgl->UniformMatrix4fv(vgl->GetUniformLocation(sub_program,
                              "ProjectionMatrix"), 1, GL_FALSE, identity);
        vgl->UniformMatrix4fv(vgl->GetUniformLocation(sub_program,
                              "ZRotMatrix"), 1, GL_FALSE, identity);
        vgl->UniformMatrix4fv(vgl->GetUniformLocation(sub_program,
                              "YRotMatrix"), 1, GL_FALSE, identity);
        vgl->UniformMatrix4fv(vgl->GetUniformLocation(sub_program,
                              "XRotMatrix"), 1, GL_FALSE, identity);
        vgl->UniformMatrix4fv(vgl->GetUniformLocation(sub_program,
                              "ZoomMatrix"), 1, GL_FALSE, identity);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    glDisable(GL_BLEND);

    /* Display */
    vlc_gl_Swap(vgl->gl);

    return VLC_SUCCESS;
}

