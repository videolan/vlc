/*****************************************************************************
 * opengl.c: OpenGL and OpenGL ES output common code
 *****************************************************************************
 * Copyright (C) 2004-2012 VLC authors and VideoLAN
 * Copyright (C) 2009, 2011 Laurent Aimar
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Eric Petit <titer@m0k.org>
 *          Cedric Cocquebert <cedric.cocquebert@supelec.fr>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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

#ifdef __APPLE__
#   define PFNGLGENBUFFERSPROC               typeof(glGenBuffers)*
#   define PFNGLBINDBUFFERPROC               typeof(glBindBuffer)*
#   define PFNGLDELETEBUFFERSPROC            typeof(glDeleteBuffers)*
#   define PFNGLBUFFERSUBDATAPROC            typeof(glBufferSubData)*
#   define PFNGLBUFFERDATAPROC               typeof(glBufferData)*
#   define PFNGLGETPROGRAMIVPROC             typeof(glGetProgramiv)*
#   define PFNGLGETPROGRAMINFOLOGPROC        typeof(glGetProgramInfoLog)*
#   define PFNGLGETSHADERIVPROC              typeof(glGetShaderiv)*
#   define PFNGLGETSHADERINFOLOGPROC         typeof(glGetShaderInfoLog)*
#   define PFNGLGETUNIFORMLOCATIONPROC       typeof(glGetUniformLocation)*
#   define PFNGLGETATTRIBLOCATIONPROC        typeof(glGetAttribLocation)*
#   define PFNGLUNIFORM4FVPROC               typeof(glUniform4fv)*
#   define PFNGLUNIFORM4FPROC                typeof(glUniform4f)*
#   define PFNGLUNIFORM3IPROC                typeof(glUniform3i)*
#   define PFNGLUNIFORM1IPROC                typeof(glUniform1i)*
#   define PFNGLCREATESHADERPROC             typeof(glCreateShader)*
#   define PFNGLSHADERSOURCEPROC             typeof(glShaderSource)*
#   define PFNGLCOMPILESHADERPROC            typeof(glCompileShader)*
#   define PFNGLDETACHSHADERPROC             typeof(glDetachShader)*
#   define PFNGLDELETESHADERPROC             typeof(glDeleteShader)*
#   define PFNGLCREATEPROGRAMPROC            typeof(glCreateProgram)*
#   define PFNGLLINKPROGRAMPROC              typeof(glLinkProgram)*
#   define PFNGLUSEPROGRAMPROC               typeof(glUseProgram)*
#   define PFNGLDELETEPROGRAMPROC            typeof(glDeleteProgram)*
#   define PFNGLATTACHSHADERPROC             typeof(glAttachShader)*
#   define PFNGLACTIVETEXTUREPROC            typeof(glActiveTexture)*
#   define PFNGLCLIENTACTIVETEXTUREPROC      typeof(glClientActiveTexture)*
#if USE_OPENGL_ES
#   define GL_UNPACK_ROW_LENGTH 0
#endif
#endif

#if USE_OPENGL_ES
#   define VLCGL_TEXTURE_COUNT 1
#   define VLCGL_PICTURE_MAX 1
#else
#   define VLCGL_TEXTURE_COUNT 1
#   define VLCGL_PICTURE_MAX 128
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

    /* Buffer commands */
    PFNGLGENBUFFERSPROC   GenBuffers;
    PFNGLBINDBUFFERPROC   BindBuffer;
    PFNGLDELETEBUFFERSPROC DeleteBuffers;
    PFNGLBUFFERSUBDATAPROC BufferSubData;

    PFNGLBUFFERDATAPROC   BufferData;

    /* Shader variables commands*/

    PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
    PFNGLGETATTRIBLOCATIONPROC  GetAttribLocation;

    PFNGLUNIFORM4FVPROC   Uniform4fv;
    PFNGLUNIFORM4FPROC    Uniform4f;
    PFNGLUNIFORM3IPROC    Uniform3i;
    PFNGLUNIFORM1IPROC    Uniform1i;

    /* Shader command */
    PFNGLCREATESHADERPROC CreateShader;
    PFNGLSHADERSOURCEPROC ShaderSource;
    PFNGLCOMPILESHADERPROC CompileShader;
    PFNGLDETACHSHADERPROC   DetachShader;
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


    /* multitexture */
    PFNGLACTIVETEXTUREPROC  ActiveTexture;
    PFNGLCLIENTACTIVETEXTUREPROC  ClientActiveTexture;
    bool use_multitexture;
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
#if defined(MACOS_OPENGL)
    /* OpenGL 1.x on OS X does _not_ support 16bit shaders, but pretends to.
     * That's why we enforce return false here, even though the actual code below
     * would return true.
     * This fixes playback of 10bit content on the Intel GMA 950 chipset, which is
     * the only "GPU" supported by 10.6 and 10.7 with just an OpenGL 1.4 driver.
     *
     * Presumely, this also improves playback on the GMA 3100, GeForce FX 5200,
     * GeForce4 Ti, GeForce3, GeForce2 MX/4 MX and the Radeon 8500 when
     * running OS X 10.5. */
    const GLubyte * p_glversion;
    float f_glversion;
    p_glversion = glGetString (GL_VERSION);
    sscanf((char *)p_glversion, "%f", &f_glversion);
    if (f_glversion < 2)
        return false;
#endif

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

    if( vgl->gl->getProcAddress == NULL )
    {
        fprintf(stderr, "getProcAddress not implemented, bailing out\n");
        free( vgl );
        return NULL;
    }


    const unsigned char *ogl_version = glGetString(GL_VERSION);
    bool supports_shaders = strverscmp((const char *)ogl_version, "2.0") >= 0;
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
    bool need_fs_yuv = false;
    float yuv_range_correction = 1.0;
    if ( max_texture_units >= 3 && supports_shaders &&
        vlc_fourcc_IsYUV(fmt->i_chroma) && !vlc_fourcc_IsYUV(vgl->fmt.i_chroma)) {
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
#if (defined (__ppc__) || defined (__ppc64__) || defined (__powerpc__)) && defined (__APPLE__)
    /* This is a work-around for dated PowerPC-based Macs, which run OpenGL 1.3 only and don't
     * support the GL_ARB_fragment_program extension.
     * Affected devices are all Macs built between 2002 and 2005 with an ATI Radeon 7500,
     * an ATI Radeon 9200 or a NVIDIA GeForceFX 5200 Ultra. */
    else
    {
        vgl->tex_format   = GL_YCBCR_422_APPLE;
        vgl->tex_type     = GL_UNSIGNED_SHORT_8_8_APPLE;
        vgl->fmt.i_chroma = VLC_CODEC_YUYV;
    }
#endif

    vgl->GenBuffers    = (PFNGLGENBUFFERSPROC)vlc_gl_GetProcAddress(vgl->gl, "glGenBuffers");
    vgl->BindBuffer    = (PFNGLBINDBUFFERPROC)vlc_gl_GetProcAddress(vgl->gl, "glBindBuffer");
    vgl->BufferData    = (PFNGLBUFFERDATAPROC)vlc_gl_GetProcAddress(vgl->gl, "glBufferData");
    vgl->BufferSubData = (PFNGLBUFFERSUBDATAPROC)vlc_gl_GetProcAddress(vgl->gl, "glBufferSubData");
    vgl->DeleteBuffers = (PFNGLDELETEBUFFERSPROC)vlc_gl_GetProcAddress(vgl->gl, "glDeleteBuffers");

    vgl->CreateShader  = (PFNGLCREATESHADERPROC)vlc_gl_GetProcAddress(vgl->gl, "glCreateShader");
    vgl->ShaderSource  = (PFNGLSHADERSOURCEPROC)vlc_gl_GetProcAddress(vgl->gl, "glShaderSource");
    vgl->CompileShader = (PFNGLCOMPILESHADERPROC)vlc_gl_GetProcAddress(vgl->gl, "glCompileShader");
    vgl->AttachShader  = (PFNGLATTACHSHADERPROC)vlc_gl_GetProcAddress(vgl->gl, "glAttachShader");
    vgl->GetProgramiv   = (PFNGLGETPROGRAMIVPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetProgramiv");
    vgl->GetProgramInfoLog   = (PFNGLGETPROGRAMINFOLOGPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetProgramInfoLog");
    vgl->GetShaderiv   = (PFNGLGETSHADERIVPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetShaderiv");
    vgl->GetShaderInfoLog   = (PFNGLGETSHADERINFOLOGPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetShaderInfoLog");
    vgl->DetachShader  = (PFNGLDETACHSHADERPROC)vlc_gl_GetProcAddress(vgl->gl, "glDetachShader");
    vgl->DeleteShader  = (PFNGLDELETESHADERPROC)vlc_gl_GetProcAddress(vgl->gl, "glDeleteShader");

    vgl->GetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetUniformLocation");
    vgl->GetAttribLocation  = (PFNGLGETATTRIBLOCATIONPROC)vlc_gl_GetProcAddress(vgl->gl, "glGetAttribLocation");
    vgl->Uniform4fv    = (PFNGLUNIFORM4FVPROC)vlc_gl_GetProcAddress(vgl->gl,"glUniform4fv");
    vgl->Uniform4f     = (PFNGLUNIFORM4FPROC)vlc_gl_GetProcAddress(vgl->gl,"glUniform4f");
    vgl->Uniform3i     = (PFNGLUNIFORM3IPROC)vlc_gl_GetProcAddress(vgl->gl,"glUniform3i");
    vgl->Uniform1i     = (PFNGLUNIFORM1IPROC)vlc_gl_GetProcAddress(vgl->gl,"glUniform1i");

    vgl->CreateProgram = (PFNGLCREATEPROGRAMPROC)vlc_gl_GetProcAddress(vgl->gl, "glCreateProgram");
    vgl->LinkProgram = (PFNGLLINKPROGRAMPROC)vlc_gl_GetProcAddress(vgl->gl, "glLinkProgram");
    vgl->UseProgram = (PFNGLUSEPROGRAMPROC)vlc_gl_GetProcAddress(vgl->gl, "glUseProgram");
    vgl->DeleteProgram = (PFNGLDELETEPROGRAMPROC)vlc_gl_GetProcAddress(vgl->gl, "glDeleteProgram");
    vgl->ActiveTexture = (PFNGLACTIVETEXTUREPROC)vlc_gl_GetProcAddress(vgl->gl, "glActiveTexture");
    vgl->ClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC)vlc_gl_GetProcAddress(vgl->gl, "glClientActiveTexture");

    vgl->chroma = vlc_fourcc_GetChromaDescription(vgl->fmt.i_chroma);
    vgl->use_multitexture = vgl->chroma->plane_count > 1;

    if( !vgl->CreateShader || !vgl->ShaderSource || !vgl->CreateProgram )
    {
        fprintf(stderr, "Looks like you don't have all the opengl we need. Driver is %s, giving up\n", glGetString(GL_VERSION));
        free( vgl );
        return NULL;
    }


    /* Texture size */
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        int w = vgl->fmt.i_width  * vgl->chroma->p[j].w.num / vgl->chroma->p[j].w.den;
        int h = vgl->fmt.i_height * vgl->chroma->p[j].h.num / vgl->chroma->p[j].h.den;
        vgl->tex_width[j]  = w;
        vgl->tex_height[j] = h;
    }

    /* Build fragment program if needed */
    vgl->program[0] = 0;
    vgl->program[1] = 0;
    vgl->local_count = 0;
    vgl->shader[0] = vgl->shader[1] = vgl->shader[2] = -1;
    if (supports_shaders) {
        char *code = NULL;

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
                "#version 120\n"
                "uniform sampler2D Texture0;"
                "uniform sampler2D Texture1;"
                "uniform sampler2D Texture2;"
                "uniform vec4      coefficient[4];"

                "void main(void) {"
                " vec4 x,y,z,result;"
                " x  = texture2D(Texture0, gl_TexCoord[0].st);"
                " %c = texture2D(Texture1, gl_TexCoord[1].st);"
                " %c = texture2D(Texture2, gl_TexCoord[2].st);"

                " result = x * coefficient[0] + coefficient[3];"
                " result = (y * coefficient[1]) + result;"
                " result = (z * coefficient[2]) + result;"
                " gl_FragColor = result;"
                "}";
            bool swap_uv = vgl->fmt.i_chroma == VLC_CODEC_YV12 ||
                           vgl->fmt.i_chroma == VLC_CODEC_YV9;
            if (asprintf(&code, template_glsl_yuv,
                         swap_uv ? 'z' : 'y',
                         swap_uv ? 'y' : 'z') < 0)
                code = NULL;

            for (int i = 0; i < 4; i++) {
                float correction = i < 3 ? yuv_range_correction : 1.0;
                /* We place coefficient values for coefficient[4] in one array from matrix values.
                   Notice that we fill values from top down instead of left to right.*/
                for( int j = 0; j < 4; j++ )
                    vgl->local_value[vgl->local_count + i*4+j] = j < 3 ? correction * matrix[j*4+i] : 0.0 ;
            }
            vgl->local_count += 4;

            // Basic vertex shader that we use in both cases
            const char *vertexShader =
            "#version 120\n"
            "void main() {"
            " gl_TexCoord[0] = gl_MultiTexCoord0;"
            " gl_TexCoord[1] = gl_MultiTexCoord1;"
            " gl_TexCoord[2] = gl_MultiTexCoord2;"
            " gl_Position = ftransform(); }";

            // Dummy shader for text overlay
            const char *helloShader =
            "#version 120\n"
            "uniform sampler2D Texture0;"
            "uniform vec4 fillColor;"
            "void main()"
            "{ "
            "  gl_FragColor = texture2D(Texture0, gl_TexCoord[0].st)*fillColor;}";

            vgl->shader[2] = vgl->CreateShader( GL_VERTEX_SHADER );
            vgl->ShaderSource( vgl->shader[2], 1, (const GLchar **)&vertexShader, NULL);
            vgl->CompileShader( vgl->shader[2] );

            /* Create 'dummy' shader that handles subpicture overlay for now*/
            vgl->shader[1] = vgl->CreateShader( GL_FRAGMENT_SHADER );
            vgl->ShaderSource( vgl->shader[1], 1, &helloShader, NULL);
            vgl->CompileShader( vgl->shader[1] );
            vgl->program[1] = vgl->CreateProgram();
            vgl->AttachShader( vgl->program[1], vgl->shader[1]);
            vgl->AttachShader( vgl->program[1], vgl->shader[2]);
            vgl->LinkProgram( vgl->program[1] );

            // Create shader from code
            vgl->shader[0] = vgl->CreateShader( GL_FRAGMENT_SHADER );
            vgl->program[0] = vgl->CreateProgram();
            if( need_fs_yuv )
            {
                vgl->ShaderSource( vgl->shader[0], 1, (const GLchar **)&code, NULL );
                vgl->CompileShader( vgl->shader[0]);
                vgl->AttachShader( vgl->program[0], vgl->shader[0] );
            } else {
                /* Use simpler shader if we don't need to to yuv -> rgb,
                   for example when input is allready rgb (.bmp image).*/
                vgl->AttachShader( vgl->program[0], vgl->shader[1] );
            }
            vgl->AttachShader( vgl->program[0], vgl->shader[2]);

            vgl->LinkProgram( vgl->program[0] );

            free(code);
            for( GLuint i = 0; i < 2; i++ )
            {
                int infoLength = 0;
                int charsWritten = 0;
                char *infolog;
                vgl->GetProgramiv( vgl->program[i], GL_INFO_LOG_LENGTH, &infoLength );
                if( infoLength > 1 )
                {
                    /* If there is some message, better to check linking is ok */
                    GLint link_status = GL_TRUE;
                    vgl->GetProgramiv( vgl->program[i], GL_LINK_STATUS, &link_status );

                    infolog = (char *)malloc(infoLength);
                    vgl->GetProgramInfoLog( vgl->program[i], infoLength, &charsWritten, infolog );
                    fprintf(stderr, "shader program %d:%s %d\n",i,infolog,infoLength);
                    free(infolog);

                    /* Check shaders messages too */
                    for( GLuint j = 0; j < 2; j++ )
                    {
                        vgl->GetShaderiv( vgl->shader[j], GL_INFO_LOG_LENGTH, &infoLength );
                        if( infoLength > 1 )
                        {
                            infolog = (char *)malloc(infoLength);
                            vgl->GetShaderInfoLog( vgl->shader[j], infoLength, &charsWritten, infolog );
                            fprintf(stderr, "shader %d: %s\n",j,infolog );
                            free( infolog );
                        }
                    }

                    if( link_status == GL_FALSE )
                    {
                        fprintf( stderr, "Unable to use program %d", i );
                        free( vgl );
                        return NULL;
                    }
                }
            }
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

        if (vgl->program[0])
        {
            for( int i = 0; i < 2; i++ )
                vgl->DeleteProgram( vgl->program[i] );
            for( int i = 0; i < 3; i++ )
                vgl->DeleteShader( vgl->shader[i] );
        }

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
    unsigned count = 0;

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
            if (vgl->use_multitexture)
            {
                vgl->ActiveTexture(GL_TEXTURE0 + j);
                vgl->ClientActiveTexture(GL_TEXTURE0 + j);
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

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture, subpicture_t *subpicture)
{
    /* On Win32/GLX, we do this the usual way:
       + Fill the buffer with new content,
       + Reload the texture,
       + Use the texture.

       On OS X with VRAM or AGP texturing, the order has to be:
       + Reload the texture,
       + Fill the buffer with new content,
       + Use the texture.

       (Thanks to gcc from the Arstechnica forums for the tip)

       Therefore on OSX, we have to use two buffers and textures and use a
       lock(/unlock) managed picture pool.
     */

    if (vlc_gl_Lock(vgl->gl))
        return VLC_EGENERIC;

    /* Update the texture */
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        if (vgl->use_multitexture)
        {
            vgl->ActiveTexture(GL_TEXTURE0 + j);
            vgl->ClientActiveTexture(GL_TEXTURE0 + j);
        }
        glBindTexture(vgl->tex_target, vgl->texture[0][j]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, picture->p[j].i_pitch / picture->p[j].i_pixel_pitch);
        glTexSubImage2D(vgl->tex_target, 0,
                        0, 0,
                        vgl->fmt.i_width  * vgl->chroma->p[j].w.num / vgl->chroma->p[j].w.den,
                        vgl->fmt.i_height * vgl->chroma->p[j].h.num / vgl->chroma->p[j].h.den,
                        vgl->tex_format, vgl->tex_type, picture->p[j].p_pixels);
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

        if (vgl->use_multitexture)
        {
            vgl->ActiveTexture(GL_TEXTURE0 + 0);
            vgl->ClientActiveTexture(GL_TEXTURE0 + 0);
        }
        int i = 0;
        for (subpicture_region_t *r = subpicture->p_region; r; r = r->p_next, i++) {
            gl_region_t *glr = &vgl->region[i];

            glr->format = GL_RGBA;
            glr->type   = GL_UNSIGNED_BYTE;
            glr->width  = r->fmt.i_visible_width;
            glr->height = r->fmt.i_visible_height;
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
                /* TODO set GL_UNPACK_ALIGNMENT */
                glPixelStorei(GL_UNPACK_ROW_LENGTH, r->p_picture->p->i_pitch / r->p_picture->p->i_pixel_pitch);
                glTexSubImage2D(GL_TEXTURE_2D, 0,
                                0, 0, glr->width, glr->height,
                                glr->format, glr->type, &r->p_picture->p->p_pixels[pixels_offset]);
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
                /* TODO set GL_UNPACK_ALIGNMENT */
                glPixelStorei(GL_UNPACK_ROW_LENGTH, r->p_picture->p->i_pitch / r->p_picture->p->i_pixel_pitch);
                glTexImage2D(GL_TEXTURE_2D, 0, glr->format,
                             glr->width, glr->height, 0, glr->format, glr->type,
                             &r->p_picture->p->p_pixels[pixels_offset]);
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

static void draw_without_shaders( vout_display_opengl_t *vgl, float *left, float *top, float *right, float *bottom )
{
    static const GLfloat vertexCoord[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };

    const GLfloat textureCoord[8] = {
        left[0],  bottom[0],
        right[0], bottom[0],
        left[0],  top[0],
        right[0], top[0]
    };

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_VERTEX_ARRAY);
    glEnable(GL_TEXTURE_COORD_ARRAY);

    vgl->ActiveTexture( GL_TEXTURE0);
    vgl->ClientActiveTexture( GL_TEXTURE0);

    glEnable(vgl->tex_target);
    glBindTexture(vgl->tex_target, vgl->texture[0][0]);
    glVertexPointer(2, GL_FLOAT, 0, vertexCoord);
    glTexCoordPointer(2, GL_FLOAT, 0, textureCoord);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisable(vgl->tex_target);
    glDisable(GL_TEXTURE_COORD_ARRAY);
    glDisable(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

static void draw_with_shaders( vout_display_opengl_t *vgl, float *left, float *top, float *right, float *bottom )
{
    vgl->UseProgram(vgl->program[0]);
    vgl->Uniform4fv( vgl->GetUniformLocation( vgl->program[0], "coefficient" ), 4, vgl->local_value);
    vgl->Uniform1i( vgl->GetUniformLocation( vgl->program[0], "Texture0" ), 0);
    vgl->Uniform1i( vgl->GetUniformLocation( vgl->program[0], "Texture1" ), 1);
    vgl->Uniform1i( vgl->GetUniformLocation( vgl->program[0], "Texture2" ), 2);

    const GLfloat vertexCoord[] = {
        -1.0, 1.0,
        -1.0, -1.0,
        1.0, 1.0,
        1.0, -1.0,
    };

    for( unsigned j = 0; j < vgl->chroma->plane_count; j++)
    {
        const GLfloat texCoord[] = {
            left[j], top[j],
            left[j], bottom[j],
            right[j], top[j],
            right[j], bottom[j],
        };
        vgl->ActiveTexture( GL_TEXTURE0+j);
        vgl->ClientActiveTexture( GL_TEXTURE0+j);
        glEnable(vgl->tex_target);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glBindTexture(vgl->tex_target, vgl->texture[0][j]);
        glTexCoordPointer(2, GL_FLOAT, 0, texCoord);
    }
    vgl->ActiveTexture(GL_TEXTURE0 + 0);
    vgl->ClientActiveTexture(GL_TEXTURE0 + 0);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertexCoord);
    glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);

    for( int j = vgl->chroma->plane_count; j >= 0;j--)
    {
        vgl->ActiveTexture( GL_TEXTURE0+j);
        vgl->ClientActiveTexture( GL_TEXTURE0+j);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }

    vgl->ActiveTexture(GL_TEXTURE0 + 0);
    vgl->ClientActiveTexture(GL_TEXTURE0 + 0);
}

int vout_display_opengl_Display(vout_display_opengl_t *vgl,
                                const video_format_t *source)
{
    if (vlc_gl_Lock(vgl->gl))
        return VLC_EGENERIC;

    /* glTexCoord works differently with GL_TEXTURE_2D and
       GL_TEXTURE_RECTANGLE_EXT */
    float left[PICTURE_PLANE_MAX];
    float top[PICTURE_PLANE_MAX];
    float right[PICTURE_PLANE_MAX];
    float bottom[PICTURE_PLANE_MAX];
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
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


    /* Why drawing here and not in Render()? Because this way, the
       OpenGL providers can call vout_display_opengl_Display to force redraw.i
       Currently, the OS X provider uses it to get a smooth window resizing */

    glClear(GL_COLOR_BUFFER_BIT);


    if( vgl->program[0] )
    {
        draw_with_shaders( vgl, left, top ,right, bottom );
        // Change the program for overlays
        vgl->UseProgram(vgl->program[1]);
        vgl->Uniform1i( vgl->GetUniformLocation( vgl->program[1], "Texture0" ), 0);
    } else {
        draw_without_shaders( vgl, left, top, right, bottom );
    }

    vgl->ActiveTexture(GL_TEXTURE0 + 0);
    vgl->ClientActiveTexture(GL_TEXTURE0 + 0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnableClientState(GL_VERTEX_ARRAY);

    for (int i = 0; i < vgl->region_count; i++) {
        gl_region_t *glr = &vgl->region[i];
        const GLfloat vertexCoord[] = {
            glr->left, glr->top,
            glr->left, glr->bottom,
            glr->right, glr->top,
            glr->right,glr->bottom,
        };
        static const GLfloat textureCoord[] = {
            0.0, 0.0,
            0.0, 1.0,
            1.0, 0.0,
            1.0, 1.0,
        };

        if( vgl->program[0] )
        {
            vgl->Uniform4f( vgl->GetUniformLocation( vgl->program[1], "fillColor"), 1.0f, 1.0f, 1.0f, glr->alpha);
        }
        else
        {
            glColor4f( 1.0f, 1.0f, 1.0f, glr->alpha );
            glEnable(GL_TEXTURE_COORD_ARRAY);
            glEnable(GL_VERTEX_ARRAY);
        }

        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        glBindTexture(GL_TEXTURE_2D, glr->texture);
        glVertexPointer(2, GL_FLOAT, 0, vertexCoord);
        glTexCoordPointer(2, GL_FLOAT, 0, textureCoord);
        glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);

    vlc_gl_Swap(vgl->gl);

    vlc_gl_Unlock(vgl->gl);
    return VLC_SUCCESS;
}

