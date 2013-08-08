/*****************************************************************************
 * opengl.c: OpenGL and OpenGL ES output common code
 *****************************************************************************
 * Copyright (C) 2004-2013 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_picture_pool.h>
#include <vlc_subpicture.h>
#include <vlc_opengl.h>

#include "opengl.h"

#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE 0x812F
#endif

#if USE_OPENGL_ES == 2 || defined(__APPLE__)
#   define PFNGLGETPROGRAMIVPROC             typeof(glGetProgramiv)*
#   define PFNGLGETPROGRAMINFOLOGPROC        typeof(glGetProgramInfoLog)*
#   define PFNGLGETSHADERIVPROC              typeof(glGetShaderiv)*
#   define PFNGLGETSHADERINFOLOGPROC         typeof(glGetShaderInfoLog)*
#   define PFNGLGETUNIFORMLOCATIONPROC       typeof(glGetUniformLocation)*
#   define PFNGLGETATTRIBLOCATIONPROC        typeof(glGetAttribLocation)*
#   define PFNGLVERTEXATTRIBPOINTERPROC      typeof(glVertexAttribPointer)*
#   define PFNGLENABLEVERTEXATTRIBARRAYPROC  typeof(glEnableVertexAttribArray)*
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
#if defined(__APPLE__) && USE_OPENGL_ES
#   import <CoreFoundation/CoreFoundation.h>
#endif
#endif

#if USE_OPENGL_ES
#   define GLSL_VERSION "100"
#   define VLCGL_TEXTURE_COUNT 1
#   define VLCGL_PICTURE_MAX 1
#   define PRECISION "precision highp float;"
#if USE_OPENGL_ES == 2
#   define SUPPORTS_SHADERS
#   define glClientActiveTexture(x)
#else
#   define SUPPORTS_FIXED_PIPELINE
#   define GL_MAX_TEXTURE_IMAGE_UNITS GL_MAX_TEXTURE_UNITS
#endif
#else
#   define GLSL_VERSION "120"
#   define VLCGL_TEXTURE_COUNT 1
#   define VLCGL_PICTURE_MAX 128
#   define PRECISION ""
#   define SUPPORTS_SHADERS
#   define SUPPORTS_FIXED_PIPELINE
#endif

static const vlc_fourcc_t gl_subpicture_chromas[] = {
    VLC_CODEC_RGBA,
    0
};

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

    /* index 0 for normal and 1 for subtitle overlay */
    GLuint     program[2];
    GLint      shader[3]; //3. is for the common vertex shader
    int        local_count;
    GLfloat    local_value[16];

    /* Shader variables commands*/
#ifdef SUPPORTS_SHADERS
    PFNGLGETUNIFORMLOCATIONPROC      GetUniformLocation;
    PFNGLGETATTRIBLOCATIONPROC       GetAttribLocation;
    PFNGLVERTEXATTRIBPOINTERPROC     VertexAttribPointer;
    PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;

    PFNGLUNIFORM4FVPROC   Uniform4fv;
    PFNGLUNIFORM4FPROC    Uniform4f;
    PFNGLUNIFORM1IPROC    Uniform1i;

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
#endif

#if defined(_WIN32)
    PFNGLACTIVETEXTUREPROC  ActiveTexture;
    PFNGLCLIENTACTIVETEXTUREPROC  ClientActiveTexture;
#endif


    /* multitexture */
    bool use_multitexture;

    /* Non-power-of-2 texture size support */
    bool supports_npot;

    uint8_t *texture_temp_buf;
    int      texture_temp_buf_size;
};

static inline int GetAlignedSize(unsigned size)
{
    /* Return the smallest larger or equal power of 2 */
    unsigned align = 1 << (8 * sizeof (unsigned) - clz(size));
    return ((align >> 1) == size) ? size : align;
}

#if !USE_OPENGL_ES
static bool IsLuminance16Supported(int target)
{
    GLuint texture;

    glGenTextures(1, &texture);
    glBindTexture(target, texture);
    glTexImage2D(target, 0, GL_LUMINANCE16,
                 64, 64, 0, GL_LUMINANCE, GL_UNSIGNED_SHORT, NULL);
    GLint size = 0;
    glGetTexLevelParameteriv(target, 0, GL_TEXTURE_LUMINANCE_SIZE, &size);

    glDeleteTextures(1, &texture);

    return size == 16;
}
#endif

#ifdef SUPPORTS_SHADERS
static void BuildVertexShader(vout_display_opengl_t *vgl,
                              GLint *shader)
{
    /* Basic vertex shader */
    const char *vertexShader =
        "#version " GLSL_VERSION "\n"
        PRECISION
        "varying vec4 TexCoord0,TexCoord1, TexCoord2;"
        "attribute vec4 MultiTexCoord0,MultiTexCoord1,MultiTexCoord2;"
        "attribute vec4 VertexPosition;"
        "void main() {"
        " TexCoord0 = MultiTexCoord0;"
        " TexCoord1 = MultiTexCoord1;"
        " TexCoord2 = MultiTexCoord2;"
        " gl_Position = VertexPosition;"
        "}";

    *shader = vgl->CreateShader(GL_VERTEX_SHADER);
    vgl->ShaderSource(*shader, 1, &vertexShader, NULL);
    vgl->CompileShader(*shader);
}

static void BuildYUVFragmentShader(vout_display_opengl_t *vgl,
                                   GLint *shader,
                                   int *local_count,
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
    const float (*matrix) = fmt->i_height > 576 ? matrix_bt709_tv2full
                                                : matrix_bt601_tv2full;

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
        " x  = texture2D(Texture0, TexCoord0.st);"
        " %c = texture2D(Texture1, TexCoord1.st);"
        " %c = texture2D(Texture2, TexCoord2.st);"

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
        code = NULL;

    for (int i = 0; i < 4; i++) {
        float correction = i < 3 ? yuv_range_correction : 1.0;
        /* We place coefficient values for coefficient[4] in one array from matrix values.
           Notice that we fill values from top down instead of left to right.*/
        for (int j = 0; j < 4; j++)
            local_value[*local_count + i*4+j] = j < 3 ? correction * matrix[j*4+i]
                                                      : 0.0 ;
    }
    (*local_count) += 4;


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

#endif

vout_display_opengl_t *vout_display_opengl_New(video_format_t *fmt,
                                               const vlc_fourcc_t **subpicture_chromas,
                                               vlc_gl_t *gl)
{
    vout_display_opengl_t *vgl = calloc(1, sizeof(*vgl));
    if (!vgl)
        return NULL;

    vgl->gl = gl;
    if (vlc_gl_Lock(vgl->gl)) {
        free(vgl);
        return NULL;
    }

    if (vgl->gl->getProcAddress == NULL) {
        fprintf(stderr, "getProcAddress not implemented, bailing out\n");
        free(vgl);
        return NULL;
    }

    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
#if !USE_OPENGL_ES
    const unsigned char *ogl_version = glGetString(GL_VERSION);
    bool supports_shaders = strverscmp((const char *)ogl_version, "2.0") >= 0;
#else
    bool supports_shaders = false;
#endif

#if USE_OPENGL_ES == 2
    vgl->CreateShader  = glCreateShader;
    vgl->ShaderSource  = glShaderSource;
    vgl->CompileShader = glCompileShader;
    vgl->AttachShader  = glAttachShader;

    vgl->GetProgramiv  = glGetProgramiv;
    vgl->GetShaderiv   = glGetShaderiv;
    vgl->GetProgramInfoLog  = glGetProgramInfoLog;
    vgl->GetShaderInfoLog   = glGetShaderInfoLog;

    vgl->DeleteShader  = glDeleteShader;

    vgl->GetUniformLocation = glGetUniformLocation;
    vgl->GetAttribLocation  = glGetAttribLocation;
    vgl->VertexAttribPointer= glVertexAttribPointer;
    vgl->EnableVertexAttribArray = glEnableVertexAttribArray;
    vgl->Uniform4fv    = glUniform4fv;
    vgl->Uniform4f     = glUniform4f;
    vgl->Uniform1i     = glUniform1i;

    vgl->CreateProgram = glCreateProgram;
    vgl->LinkProgram   = glLinkProgram;
    vgl->UseProgram    = glUseProgram;
    vgl->DeleteProgram = glDeleteProgram;
    supports_shaders = true;
#elif defined(SUPPORTS_SHADERS)
    vgl->CreateShader  = (PFNGLCREATESHADERPROC)vlc_gl_GetProcAddress(vgl->gl, "glCreateShader");
    vgl->ShaderSource  = (PFNGLSHADERSOURCEPROC)vlc_gl_GetProcAddress(vgl->gl, "glShaderSource");
    vgl->CompileShader = (PFNGLCOMPILESHADERPROC)vlc_gl_GetProcAddress(vgl->gl, "glCompileShader");
    vgl->AttachShader  = (PFNGLATTACHSHADERPROC)vlc_gl_GetProcAddress(vgl->gl, "glAttachShader");

    vgl->GetProgramiv  = (PFNGLGETPROGRAMIVPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetProgramiv");
    vgl->GetShaderiv   = (PFNGLGETSHADERIVPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetShaderiv");
    vgl->GetProgramInfoLog  = (PFNGLGETPROGRAMINFOLOGPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetProgramInfoLog");
    vgl->GetShaderInfoLog   = (PFNGLGETSHADERINFOLOGPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetShaderInfoLog");

    vgl->DeleteShader  = (PFNGLDELETESHADERPROC)vlc_gl_GetProcAddress(vgl->gl, "glDeleteShader");

    vgl->GetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetUniformLocation");
    vgl->GetAttribLocation  = (PFNGLGETATTRIBLOCATIONPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetAttribLocation");
    vgl->VertexAttribPointer= (PFNGLVERTEXATTRIBPOINTERPROC)vlc_gl_GetProcAddress(vgl->gl, "glVertexAttribPointer");
    vgl->EnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)vlc_gl_GetProcAddress(vgl->gl, "glEnableVertexAttribArray");
    vgl->Uniform4fv    = (PFNGLUNIFORM4FVPROC)vlc_gl_GetProcAddress(vgl->gl,"glUniform4fv");
    vgl->Uniform4f     = (PFNGLUNIFORM4FPROC)vlc_gl_GetProcAddress(vgl->gl,"glUniform4f");
    vgl->Uniform1i     = (PFNGLUNIFORM1IPROC)vlc_gl_GetProcAddress(vgl->gl,"glUniform1i");

    vgl->CreateProgram = (PFNGLCREATEPROGRAMPROC)vlc_gl_GetProcAddress(vgl->gl, "glCreateProgram");
    vgl->LinkProgram   = (PFNGLLINKPROGRAMPROC)vlc_gl_GetProcAddress(vgl->gl, "glLinkProgram");
    vgl->UseProgram    = (PFNGLUSEPROGRAMPROC)vlc_gl_GetProcAddress(vgl->gl, "glUseProgram");
    vgl->DeleteProgram = (PFNGLDELETEPROGRAMPROC)vlc_gl_GetProcAddress(vgl->gl, "glDeleteProgram");

    if (!vgl->CreateShader || !vgl->ShaderSource || !vgl->CreateProgram)
        supports_shaders = false;
#endif

#if defined(_WIN32)
    vgl->ActiveTexture = (PFNGLACTIVETEXTUREPROC)vlc_gl_GetProcAddress(vgl->gl, "glActiveTexture");
    vgl->ClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC)vlc_gl_GetProcAddress(vgl->gl, "glClientActiveTexture");
#   define glActiveTexture vgl->ActiveTexture
#   define glClientActiveTexture vgl->ClientActiveTexture
#endif

    vgl->supports_npot = HasExtension(extensions, "GL_ARB_texture_non_power_of_two") ||
                         HasExtension(extensions, "GL_APPLE_texture_2D_limited_npot");

#if USE_OPENGL_ES == 2
    /* OpenGL ES 2 includes support for non-power of 2 textures by specification
     * so checks for extensions are bound to fail. Check for OpenGL ES version instead. */
    vgl->supports_npot = true;
#endif

    GLint max_texture_units = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);

#ifdef __APPLE__
#if USE_OPENGL_ES
    /* work-around an iOS 6 bug */
    if (kCFCoreFoundationVersionNumber >= 786.)
        max_texture_units = 8;
    supports_shaders = true;
#endif
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
    vgl->tex_target   = GL_TEXTURE_2D;
    vgl->tex_format   = GL_RGBA;
    vgl->tex_internal = GL_RGBA;
    vgl->tex_type     = GL_UNSIGNED_BYTE;
    /* Use YUV if possible and needed */
    bool need_fs_yuv = false;
    bool need_fs_xyz = false;
    bool need_fs_rgba = USE_OPENGL_ES == 2;
    float yuv_range_correction = 1.0;

    if (max_texture_units >= 3 && supports_shaders && vlc_fourcc_IsYUV(fmt->i_chroma)) {
        const vlc_fourcc_t *list = vlc_fourcc_GetYUVFallback(fmt->i_chroma);
        while (*list) {
            const vlc_chroma_description_t *dsc = vlc_fourcc_GetChromaDescription(*list);
            if (dsc && dsc->plane_count == 3 && dsc->pixel_size == 1) {
                need_fs_yuv       = true;
                vgl->fmt          = *fmt;
                vgl->fmt.i_chroma = *list;
                vgl->tex_format   = GL_LUMINANCE;
                vgl->tex_internal = GL_LUMINANCE;
                vgl->tex_type     = GL_UNSIGNED_BYTE;
                yuv_range_correction = 1.0;
                break;
#if !USE_OPENGL_ES
            } else if (dsc && dsc->plane_count == 3 && dsc->pixel_size == 2 &&
                       IsLuminance16Supported(vgl->tex_target)) {
                need_fs_yuv       = true;
                vgl->fmt          = *fmt;
                vgl->fmt.i_chroma = *list;
                vgl->tex_format   = GL_LUMINANCE;
                vgl->tex_internal = GL_LUMINANCE16;
                vgl->tex_type     = GL_UNSIGNED_SHORT;
                yuv_range_correction = (float)((1 << 16) - 1) / ((1 << dsc->pixel_bits) - 1);
                break;
#endif
            }
            list++;
        }
    }

    if (fmt->i_chroma == VLC_CODEC_XYZ12) {
        vlc_fourcc_GetChromaDescription(fmt->i_chroma);
        need_fs_xyz       = true;
        vgl->fmt          = *fmt;
        vgl->fmt.i_chroma = VLC_CODEC_XYZ12;
        vgl->tex_format   = GL_RGB;
        vgl->tex_internal = GL_RGB;
        vgl->tex_type     = GL_UNSIGNED_SHORT;
    }
    vgl->chroma = vlc_fourcc_GetChromaDescription(vgl->fmt.i_chroma);
    vgl->use_multitexture = vgl->chroma->plane_count > 1;

    /* Texture size */
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        int w = vgl->fmt.i_width  * vgl->chroma->p[j].w.num / vgl->chroma->p[j].w.den;
        int h = vgl->fmt.i_height * vgl->chroma->p[j].h.num / vgl->chroma->p[j].h.den;
        if (vgl->supports_npot) {
            vgl->tex_width[j]  = w;
            vgl->tex_height[j] = h;
        } else {
            vgl->tex_width[j]  = GetAlignedSize(w);
            vgl->tex_height[j] = GetAlignedSize(h);
        }
    }

    /* Build program if needed */
    vgl->program[0] =
    vgl->program[1] = 0;
    vgl->shader[0] =
    vgl->shader[1] =
    vgl->shader[2] = -1;
    vgl->local_count = 0;
    if (supports_shaders && (need_fs_yuv || need_fs_xyz|| need_fs_rgba)) {
#ifdef SUPPORTS_SHADERS
        if (need_fs_xyz)
            BuildXYZFragmentShader(vgl, &vgl->shader[0]);
        else
            BuildYUVFragmentShader(vgl, &vgl->shader[0], &vgl->local_count,
                                vgl->local_value, fmt, yuv_range_correction);

        BuildRGBAFragmentShader(vgl, &vgl->shader[1]);
        BuildVertexShader(vgl, &vgl->shader[2]);

        /* Check shaders messages */
        for (unsigned j = 0; j < 3; j++) {
            int infoLength;
            vgl->GetShaderiv(vgl->shader[j], GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength <= 1)
                continue;

            char *infolog = malloc(infoLength);
            int charsWritten;
            vgl->GetShaderInfoLog(vgl->shader[j], infoLength, &charsWritten, infolog);
            fprintf(stderr, "shader %d: %s\n", j, infolog);
            free(infolog);
        }

        vgl->program[0] = vgl->CreateProgram();
        vgl->AttachShader(vgl->program[0], vgl->shader[0]);
        vgl->AttachShader(vgl->program[0], vgl->shader[2]);
        vgl->LinkProgram(vgl->program[0]);

        vgl->program[1] = vgl->CreateProgram();
        vgl->AttachShader(vgl->program[1], vgl->shader[1]);
        vgl->AttachShader(vgl->program[1], vgl->shader[2]);
        vgl->LinkProgram(vgl->program[1]);

        /* Check program messages */
        for (GLuint i = 0; i < 2; i++) {
            int infoLength = 0;
            vgl->GetProgramiv(vgl->program[i], GL_INFO_LOG_LENGTH, &infoLength);
            if (infoLength <= 1)
                continue;
            char *infolog = malloc(infoLength);
            int charsWritten;
            vgl->GetProgramInfoLog(vgl->program[i], infoLength, &charsWritten, infolog);
            fprintf(stderr, "shader program %d: %s\n", i, infolog);
            free(infolog);

            /* If there is some message, better to check linking is ok */
            GLint link_status = GL_TRUE;
            vgl->GetProgramiv(vgl->program[i], GL_LINK_STATUS, &link_status);
            if (link_status == GL_FALSE) {
                fprintf(stderr, "Unable to use program %d\n", i);
                free(vgl);
                return NULL;
            }
        }
#else
        (void)yuv_range_correction;
#endif
    }

    /* */
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    vlc_gl_Unlock(vgl->gl);

    /* */
    for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++) {
        for (int j = 0; j < PICTURE_PLANE_MAX; j++)
            vgl->texture[i][j] = 0;
    }
    vgl->region_count = 0;
    vgl->region = NULL;
    vgl->pool = NULL;

    *fmt = vgl->fmt;
    if (subpicture_chromas) {
        *subpicture_chromas = gl_subpicture_chromas;
    }
    return vgl;
}

void vout_display_opengl_Delete(vout_display_opengl_t *vgl)
{
    /* */
    if (!vlc_gl_Lock(vgl->gl)) {
        glFinish();
        glFlush();
        for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++)
            glDeleteTextures(vgl->chroma->plane_count, vgl->texture[i]);
        for (int i = 0; i < vgl->region_count; i++) {
            if (vgl->region[i].texture)
                glDeleteTextures(1, &vgl->region[i].texture);
        }
        free(vgl->region);

#ifdef SUPPORTS_SHADERS
        if (vgl->program[0]) {
            for (int i = 0; i < 2; i++)
                vgl->DeleteProgram(vgl->program[i]);
            for (int i = 0; i < 3; i++)
                vgl->DeleteShader(vgl->shader[i]);
        }
#endif

        free(vgl->texture_temp_buf);
        vlc_gl_Unlock(vgl->gl);
    }
    if (vgl->pool)
        picture_pool_Delete(vgl->pool);
    free(vgl);
}

picture_pool_t *vout_display_opengl_GetPool(vout_display_opengl_t *vgl, unsigned requested_count)
{
    if (vgl->pool)
        return vgl->pool;

    /* Allocate our pictures */
    picture_t *picture[VLCGL_PICTURE_MAX] = {NULL, };
    unsigned count;

    for (count = 0; count < __MIN(VLCGL_PICTURE_MAX, requested_count); count++) {
        picture[count] = picture_NewFromFormat(&vgl->fmt);
        if (!picture[count])
            break;
    }
    if (count <= 0)
        return NULL;

    /* Wrap the pictures into a pool */
    picture_pool_configuration_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.picture_count = count;
    cfg.picture       = picture;
    vgl->pool = picture_pool_NewExtended(&cfg);
    if (!vgl->pool)
        goto error;

    /* Allocates our textures */
    if (vlc_gl_Lock(vgl->gl))
        return vgl->pool;

    for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++) {
        glGenTextures(vgl->chroma->plane_count, vgl->texture[i]);
        for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
            if (vgl->use_multitexture) {
                glActiveTexture(GL_TEXTURE0 + j);
                glClientActiveTexture(GL_TEXTURE0 + j);
            }
            glBindTexture(vgl->tex_target, vgl->texture[i][j]);

#if !USE_OPENGL_ES
            /* Set the texture parameters */
            glTexParameterf(vgl->tex_target, GL_TEXTURE_PRIORITY, 1.0);
            glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

            glTexParameteri(vgl->tex_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(vgl->tex_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(vgl->tex_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(vgl->tex_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            /* Call glTexImage2D only once, and use glTexSubImage2D later */
            glTexImage2D(vgl->tex_target, 0,
                         vgl->tex_internal, vgl->tex_width[j], vgl->tex_height[j],
                         0, vgl->tex_format, vgl->tex_type, NULL);
        }
    }

    vlc_gl_Unlock(vgl->gl);

    return vgl->pool;

error:
    for (unsigned i = 0; i < count; i++)
        picture_Release(picture[i]);
    return NULL;
}

#define ALIGN(x, y) (((x) + ((y) - 1)) & ~((y) - 1))
static void Upload(vout_display_opengl_t *vgl, int in_width, int in_height,
                   int in_full_width, int in_full_height,
                   int w_num, int w_den, int h_num, int h_den,
                   int pitch, int pixel_pitch,
                   int full_upload, const uint8_t *pixels,
                   int tex_target, int tex_format, int tex_type)
{
    int width       =       in_width * w_num / w_den;
    int full_width  =  in_full_width * w_num / w_den;
    int height      =      in_height * h_num / h_den;
    int full_height = in_full_height * h_num / h_den;
    // This unpack alignment is the default, but setting it just in case.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
#ifndef GL_UNPACK_ROW_LENGTH
    int dst_width = full_upload ? full_width : width;
    int dst_pitch = ALIGN(dst_width * pixel_pitch, 4);
    if ( pitch != dst_pitch )
    {
        int buf_size = dst_pitch * full_height * pixel_pitch;
        const uint8_t *source = pixels;
        uint8_t *destination;
        if( !vgl->texture_temp_buf || vgl->texture_temp_buf_size < buf_size )
        {
            free( vgl->texture_temp_buf );
            vgl->texture_temp_buf = xmalloc( buf_size );
            vgl->texture_temp_buf_size = buf_size;
        }
        destination = vgl->texture_temp_buf;

        for( int h = 0; h < height ; h++ )
        {
            memcpy( destination, source, width * pixel_pitch );
            source += pitch;
            destination += dst_pitch;
        }
        if (full_upload)
            glTexImage2D( tex_target, 0, tex_format,
                          full_width, full_height,
                          0, tex_format, tex_type, vgl->texture_temp_buf );
        else
            glTexSubImage2D( tex_target, 0,
                             0, 0,
                             width, height,
                             tex_format, tex_type, vgl->texture_temp_buf );
    } else {
#else
    (void) width;
    (void) height;
    (void) vgl;
    {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / pixel_pitch);
#endif
        if (full_upload)
            glTexImage2D(tex_target, 0, tex_format,
                         full_width, full_height,
                         0, tex_format, tex_type, pixels);
        else
            glTexSubImage2D(tex_target, 0,
                            0, 0,
                            width, height,
                            tex_format, tex_type, pixels);
    }
}

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture, subpicture_t *subpicture)
{
    if (vlc_gl_Lock(vgl->gl))
        return VLC_EGENERIC;

    /* Update the texture */
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        if (vgl->use_multitexture) {
            glActiveTexture(GL_TEXTURE0 + j);
            glClientActiveTexture(GL_TEXTURE0 + j);
        }
        glBindTexture(vgl->tex_target, vgl->texture[0][j]);

        Upload(vgl, picture->format.i_visible_width, vgl->fmt.i_visible_height,
               vgl->fmt.i_width, vgl->fmt.i_height,
               vgl->chroma->p[j].w.num, vgl->chroma->p[j].w.den, vgl->chroma->p[j].h.num, vgl->chroma->p[j].h.den,
               picture->p[j].i_pitch, picture->p[j].i_pixel_pitch, 0, picture->p[j].p_pixels, vgl->tex_target, vgl->tex_format, vgl->tex_type);
    }

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

        if (vgl->use_multitexture) {
            glActiveTexture(GL_TEXTURE0 + 0);
            glClientActiveTexture(GL_TEXTURE0 + 0);
        }
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

            const int pixels_offset = r->fmt.i_y_offset * r->p_picture->p->i_pitch +
                                      r->fmt.i_x_offset * r->p_picture->p->i_pixel_pitch;
            if (glr->texture) {
                glBindTexture(GL_TEXTURE_2D, glr->texture);
                Upload(vgl, r->fmt.i_visible_width, r->fmt.i_visible_height, glr->width, glr->height, 1, 1, 1, 1,
                       r->p_picture->p->i_pitch, r->p_picture->p->i_pixel_pitch, 0,
                       &r->p_picture->p->p_pixels[pixels_offset], GL_TEXTURE_2D, glr->format, glr->type);
            } else {
                glGenTextures(1, &glr->texture);
                glBindTexture(GL_TEXTURE_2D, glr->texture);
#if !USE_OPENGL_ES
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1.0);
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                Upload(vgl, r->fmt.i_visible_width, r->fmt.i_visible_height, glr->width, glr->height, 1, 1, 1, 1,
                       r->p_picture->p->i_pitch, r->p_picture->p->i_pixel_pitch, 1,
                       &r->p_picture->p->p_pixels[pixels_offset], GL_TEXTURE_2D, glr->format, glr->type);
            }
        }
    }
    for (int i = 0; i < last_count; i++) {
        if (last[i].texture)
            glDeleteTextures(1, &last[i].texture);
    }
    free(last);

    vlc_gl_Unlock(vgl->gl);
    VLC_UNUSED(subpicture);
    return VLC_SUCCESS;
}

#ifdef SUPPORTS_FIXED_PIPELINE
static void DrawWithoutShaders(vout_display_opengl_t *vgl,
                               float *left, float *top, float *right, float *bottom)
{
    static const GLfloat vertexCoord[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };

    const GLfloat textureCoord[] = {
        left[0],  bottom[0],
        right[0], bottom[0],
        left[0],  top[0],
        right[0], top[0]
    };

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glEnable(vgl->tex_target);
    glActiveTexture(GL_TEXTURE0 + 0);
    glClientActiveTexture(GL_TEXTURE0 + 0);

    glBindTexture(vgl->tex_target, vgl->texture[0][0]);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glTexCoordPointer(2, GL_FLOAT, 0, textureCoord);
    glVertexPointer(2, GL_FLOAT, 0, vertexCoord);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(vgl->tex_target);
}
#endif

#ifdef SUPPORTS_SHADERS
static void DrawWithShaders(vout_display_opengl_t *vgl,
                            float *left, float *top, float *right, float *bottom,
                            int program)
{
    vgl->UseProgram(vgl->program[program]);
    if (program == 0) {
        if (vgl->chroma->plane_count == 3) {
            vgl->Uniform4fv(vgl->GetUniformLocation(vgl->program[0], "Coefficient"), 4, vgl->local_value);
            vgl->Uniform1i(vgl->GetUniformLocation(vgl->program[0], "Texture0"), 0);
            vgl->Uniform1i(vgl->GetUniformLocation(vgl->program[0], "Texture1"), 1);
            vgl->Uniform1i(vgl->GetUniformLocation(vgl->program[0], "Texture2"), 2);
        }
        else if (vgl->chroma->plane_count == 1) {
            vgl->Uniform1i(vgl->GetUniformLocation(vgl->program[0], "Texture0"), 0);
        }
    } else {
        vgl->Uniform1i(vgl->GetUniformLocation(vgl->program[1], "Texture0"), 0);
        vgl->Uniform4f(vgl->GetUniformLocation(vgl->program[1], "FillColor"), 1.0f, 1.0f, 1.0f, 1.0f);
    }

    static const GLfloat vertexCoord[] = {
        -1.0,  1.0,
        -1.0, -1.0,
         1.0,  1.0,
         1.0, -1.0,
    };

    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        const GLfloat textureCoord[] = {
            left[j],  top[j],
            left[j],  bottom[j],
            right[j], top[j],
            right[j], bottom[j],
        };
        glActiveTexture(GL_TEXTURE0+j);
        glClientActiveTexture(GL_TEXTURE0+j);
        glBindTexture(vgl->tex_target, vgl->texture[0][j]);

        char attribute[20];
        snprintf(attribute, sizeof(attribute), "MultiTexCoord%1d", j);
        vgl->EnableVertexAttribArray(vgl->GetAttribLocation(vgl->program[program], attribute));
        vgl->VertexAttribPointer(vgl->GetAttribLocation(vgl->program[program], attribute), 2, GL_FLOAT, 0, 0, textureCoord);
    }
    glActiveTexture(GL_TEXTURE0 + 0);
    glClientActiveTexture(GL_TEXTURE0 + 0);
    vgl->EnableVertexAttribArray(vgl->GetAttribLocation(vgl->program[program], "VertexPosition"));
    vgl->VertexAttribPointer(vgl->GetAttribLocation(vgl->program[program], "VertexPosition"), 2, GL_FLOAT, 0, 0, vertexCoord);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
#endif

int vout_display_opengl_Display(vout_display_opengl_t *vgl,
                                const video_format_t *source)
{
    if (vlc_gl_Lock(vgl->gl))
        return VLC_EGENERIC;

    /* Why drawing here and not in Render()? Because this way, the
       OpenGL providers can call vout_display_opengl_Display to force redraw.i
       Currently, the OS X provider uses it to get a smooth window resizing */
    glClear(GL_COLOR_BUFFER_BIT);

    /* Draw the picture */
    float left[PICTURE_PLANE_MAX];
    float top[PICTURE_PLANE_MAX];
    float right[PICTURE_PLANE_MAX];
    float bottom[PICTURE_PLANE_MAX];
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        /* glTexCoord works differently with GL_TEXTURE_2D and
           GL_TEXTURE_RECTANGLE_EXT */
        float scale_w, scale_h;

        if (vgl->tex_target == GL_TEXTURE_2D) {
            scale_w = (float)vgl->chroma->p[j].w.num / vgl->chroma->p[j].w.den / vgl->tex_width[j];
            scale_h = (float)vgl->chroma->p[j].h.num / vgl->chroma->p[j].h.den / vgl->tex_height[j];

        } else {
            scale_w = 1.0;
            scale_h = 1.0;
        }
        left[j]   = (source->i_x_offset +                       0 ) * scale_w;
        top[j]    = (source->i_y_offset +                       0 ) * scale_h;
        right[j]  = (source->i_x_offset + source->i_visible_width ) * scale_w;
        bottom[j] = (source->i_y_offset + source->i_visible_height) * scale_h;
    }

#ifdef SUPPORTS_SHADERS
    if (vgl->program[0] && (vgl->chroma->plane_count == 3 || vgl->chroma->plane_count == 1))
        DrawWithShaders(vgl, left, top, right, bottom, 0);
    else if (vgl->program[1] && vgl->chroma->plane_count == 1)
        DrawWithShaders(vgl, left, top, right, bottom, 1);
    else
#endif
    {
#ifdef SUPPORTS_FIXED_PIPELINE
        DrawWithoutShaders(vgl, left, top, right, bottom);
#endif
    }

    /* Draw the subpictures */
    if (vgl->program[1]) {
#ifdef SUPPORTS_SHADERS
        // Change the program for overlays
        vgl->UseProgram(vgl->program[1]);
        vgl->Uniform1i(vgl->GetUniformLocation(vgl->program[1], "Texture"), 0);
#endif
    }

#ifdef SUPPORTS_FIXED_PIPELINE
    glEnable(GL_TEXTURE_2D);
#endif
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
        if (vgl->program[1]) {
#ifdef SUPPORTS_SHADERS
            vgl->Uniform4f(vgl->GetUniformLocation(vgl->program[1], "FillColor"), 1.0f, 1.0f, 1.0f, glr->alpha);
            vgl->EnableVertexAttribArray(vgl->GetAttribLocation(vgl->program[1], "MultiTexCoord0"));
            vgl->VertexAttribPointer(vgl->GetAttribLocation(vgl->program[1], "MultiTexCoord0"), 2, GL_FLOAT, 0, 0, textureCoord);
            vgl->EnableVertexAttribArray(vgl->GetAttribLocation(vgl->program[1], "VertexPosition"));
            vgl->VertexAttribPointer(vgl->GetAttribLocation(vgl->program[1], "VertexPosition"), 2, GL_FLOAT, 0, 0, vertexCoord);
#endif
        } else {
#ifdef SUPPORTS_FIXED_PIPELINE
            glEnableClientState(GL_VERTEX_ARRAY);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glColor4f(1.0f, 1.0f, 1.0f, glr->alpha);
            glTexCoordPointer(2, GL_FLOAT, 0, textureCoord);
            glVertexPointer(2, GL_FLOAT, 0, vertexCoord);
#endif
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        if (!vgl->program[1]) {
#ifdef SUPPORTS_FIXED_PIPELINE
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisableClientState(GL_VERTEX_ARRAY);
#endif
        }
    }
    glDisable(GL_BLEND);
#ifdef SUPPORTS_FIXED_PIPELINE
    glDisable(GL_TEXTURE_2D);
#endif

    /* Display */
    vlc_gl_Swap(vgl->gl);

    vlc_gl_Unlock(vgl->gl);
    return VLC_SUCCESS;
}

