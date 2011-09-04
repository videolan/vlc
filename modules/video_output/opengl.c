/*****************************************************************************
 * opengl.c: OpenGL and OpenGL ES output common code
 *****************************************************************************
 * Copyright (C) 2004 the VideoLAN team
 * Copyright (C) 2009 Laurent Aimar
 *
 * Authors: Cyril Deguet <asmax@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Eric Petit <titer@m0k.org>
 *          Cedric Cocquebert <cedric.cocquebert@supelec.fr>
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
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
// Define USE_OPENGL_ES to the GL ES Version you want to select

#if !defined (__APPLE__)
# if USE_OPENGL_ES == 2
#  include <GLES2/gl2ext.h>
# elif USE_OPENGL_ES == 1
#  include <GLES/glext.h>
# else
#   include <GL/glext.h>
# endif
#else
# if USE_OPENGL_ES == 2
#  include <OpenGLES/ES2/gl.h>
# elif USE_OPENGL_ES == 1
#  include <OpenGLES/ES1/gl.h>
# else
#  define MACOS_OPENGL
#  include <OpenGL/glext.h>
# endif
# define PFNGLGENPROGRAMSARBPROC              typeof(glGenProgramsARB)*
# define PFNGLBINDPROGRAMARBPROC              typeof(glBindProgramARB)*
# define PFNGLPROGRAMSTRINGARBPROC            typeof(glProgramStringARB)*
# define PFNGLDELETEPROGRAMSARBPROC           typeof(glDeleteProgramsARB)*
# define PFNGLPROGRAMLOCALPARAMETER4FVARBPROC typeof(glProgramLocalParameter4fvARB)*
# define PFNGLACTIVETEXTUREARBPROC            typeof(glActiveTextureARB)*
# define PFNGLMULTITEXCOORD2FARBPROC          typeof(glMultiTexCoord2fARB)*
#endif

/* RV16 */
#ifndef GL_UNSIGNED_SHORT_5_6_5
# define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE 0x812F
#endif

#if USE_OPENGL_ES
#   define VLCGL_TEXTURE_COUNT 1
#   define VLCGL_PICTURE_MAX 1
#elif defined(BROKEN_MACOS_OPENGL)
#   define VLCGL_TEXTURE_COUNT 2
#   define VLCGL_PICTURE_MAX 2
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

    GLuint     program;
    int        local_count;
    GLfloat    local_value[16][4];

    /* fragment_program */
    PFNGLGENPROGRAMSARBPROC              GenProgramsARB;
    PFNGLBINDPROGRAMARBPROC              BindProgramARB;
    PFNGLPROGRAMSTRINGARBPROC            ProgramStringARB;
    PFNGLDELETEPROGRAMSARBPROC           DeleteProgramsARB;
    PFNGLPROGRAMLOCALPARAMETER4FVARBPROC ProgramLocalParameter4fvARB;

    /* multitexture */
    bool use_multitexture;
    PFNGLACTIVETEXTUREARBPROC   ActiveTextureARB;
    PFNGLMULTITEXCOORD2FARBPROC MultiTexCoord2fARB;
};

static inline int GetAlignedSize(unsigned size)
{
    /* Return the smallest larger or equal power of 2 */
    unsigned align = 1 << (8 * sizeof (unsigned) - clz(size));
    return ((align >> 1) == size) ? size : align;
}

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

    const char *extensions = (const char *)glGetString(GL_EXTENSIONS);

    /* Load extensions */
    bool supports_fp = false;
    if (HasExtension(extensions, "GL_ARB_fragment_program")) {
        vgl->GenProgramsARB    = (PFNGLGENPROGRAMSARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glGenProgramsARB");
        vgl->BindProgramARB    = (PFNGLBINDPROGRAMARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glBindProgramARB");
        vgl->ProgramStringARB  = (PFNGLPROGRAMSTRINGARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glProgramStringARB");
        vgl->DeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glDeleteProgramsARB");
        vgl->ProgramLocalParameter4fvARB = (PFNGLPROGRAMLOCALPARAMETER4FVARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glProgramLocalParameter4fvARB");

        supports_fp = vgl->GenProgramsARB &&
                      vgl->BindProgramARB &&
                      vgl->ProgramStringARB &&
                      vgl->DeleteProgramsARB &&
                      vgl->ProgramLocalParameter4fvARB;
    }

    bool supports_multitexture = false;
    GLint max_texture_units = 0;
    if (HasExtension(extensions, "GL_ARB_multitexture")) {
        vgl->ActiveTextureARB   = (PFNGLACTIVETEXTUREARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glActiveTextureARB");
        vgl->MultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glMultiTexCoord2fARB");

        supports_multitexture = vgl->ActiveTextureARB &&
                                vgl->MultiTexCoord2fARB;
        if (supports_multitexture)
            glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &max_texture_units);
    }

    /* Initialize with default chroma */
    vgl->fmt = *fmt;
#if USE_OPENGL_ES
    vgl->fmt.i_chroma = VLC_CODEC_RGB16;
#   if defined(WORDS_BIGENDIAN)
    vgl->fmt.i_rmask  = 0x001f;
    vgl->fmt.i_gmask  = 0x07e0;
    vgl->fmt.i_bmask  = 0xf800;
#   else
    vgl->fmt.i_rmask  = 0xf800;
    vgl->fmt.i_gmask  = 0x07e0;
    vgl->fmt.i_bmask  = 0x001f;
#   endif
    vgl->tex_target   = GL_TEXTURE_2D;
    vgl->tex_format   = GL_RGB;
    vgl->tex_internal = GL_RGB;
    vgl->tex_type     = GL_UNSIGNED_SHORT_5_6_5;
#elif defined(BROKEN_MACOS_OPENGL)
#   if defined(WORDS_BIGENDIAN)
    vgl->fmt.i_chroma = VLC_CODEC_YUYV;
#   else
    vgl->fmt.i_chroma = VLC_CODEC_UYVY;
#   endif
    vgl->tex_target   = GL_TEXTURE_RECTANGLE_EXT;
    vgl->tex_format   = GL_YCBCR_422_APPLE;
    vgl->tex_internal = GL_YCBCR_422_APPLE;
    vgl->tex_type     = GL_UNSIGNED_SHORT_8_8_APPLE;
#else
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
#endif
    /* Use YUV if possible and needed */
    bool need_fs_yuv = false;
    float yuv_range_correction = 1.0;
    if (supports_fp && supports_multitexture && max_texture_units >= 3 &&
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
            }
            list++;
        }
    }

    vgl->chroma = vlc_fourcc_GetChromaDescription(vgl->fmt.i_chroma);
    vgl->use_multitexture = vgl->chroma->plane_count > 1;

    bool supports_npot = false;
#if USE_OPENGL_ES == 2
    supports_npot = true;
#elif defined(BROKEN_MACOS_OPENGL)
    supports_npot = true;
#else
    supports_npot |= HasExtension(extensions, "GL_APPLE_texture_2D_limited_npot") ||
                     HasExtension(extensions, "GL_ARB_texture_non_power_of_two");
#endif

    /* Texture size */
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        int w = vgl->fmt.i_width  * vgl->chroma->p[j].w.num / vgl->chroma->p[j].w.den;
        int h = vgl->fmt.i_height * vgl->chroma->p[j].h.num / vgl->chroma->p[j].h.den;
        if (supports_npot) {
            vgl->tex_width[j]  = w;
            vgl->tex_height[j] = h;
        }
        else {
            /* A texture must have a size aligned on a power of 2 */
            vgl->tex_width[j]  = GetAlignedSize(w);
            vgl->tex_height[j] = GetAlignedSize(h);
        }
    }

    /* Build fragment program if needed */
    vgl->program = 0;
    vgl->local_count = 0;
    if (supports_fp) {
        char *code = NULL;

        if (need_fs_yuv) {
            /* [R/G/B][Y U V O] from TV range to full range
             * XXX we could also do hue/brightness/constrast/gamma
             * by simply changing the coefficients
             */
            const float matrix_bt601_tv2full[3][4] = {
                { 1.1640,  0.0000,  1.4030, -0.7773 },
                { 1.1640, -0.3440, -0.7140,  0.4580 },
                { 1.1640,  1.7730,  0.0000, -0.9630 },
            };
            const float matrix_bt709_tv2full[3][4] = {
                { 1.1640,  0.0000,  1.5701, -0.8612 },
                { 1.1640, -0.1870, -0.4664,  0.2549 },
                { 1.1640,  1.8556,  0.0000, -1.0045 },
            };
            const float (*matrix)[4] = fmt->i_height > 576 ? matrix_bt709_tv2full
                                                           : matrix_bt601_tv2full;

            /* Basic linear YUV -> RGB conversion using bilinear interpolation */
            const char *template_yuv =
                "!!ARBfp1.0"
                "OPTION ARB_precision_hint_fastest;"

                "TEMP src;"
                "TEX src.x,  fragment.texcoord[0], texture[0], 2D;"
                "TEX src.%c, fragment.texcoord[1], texture[1], 2D;"
                "TEX src.%c, fragment.texcoord[2], texture[2], 2D;"

                "PARAM coefficient[4] = { program.local[0..3] };"

                "TEMP tmp;"
                "MAD  tmp.rgb,          src.xxxx, coefficient[0], coefficient[3];"
                "MAD  tmp.rgb,          src.yyyy, coefficient[1], tmp;"
                "MAD  result.color.rgb, src.zzzz, coefficient[2], tmp;"
                "END";
            bool swap_uv = vgl->fmt.i_chroma == VLC_CODEC_YV12 ||
                           vgl->fmt.i_chroma == VLC_CODEC_YV9;
            if (asprintf(&code, template_yuv,
                         swap_uv ? 'z' : 'y',
                         swap_uv ? 'y' : 'z') < 0)
                code = NULL;

            for (int i = 0; i < 4; i++) {
                float correction = i < 3 ? yuv_range_correction : 1.0;
                for (int j = 0; j < 4; j++) {
                    vgl->local_value[vgl->local_count + i][j] = j < 3 ? correction * matrix[j][i] : 0.0;
                }
            }
            vgl->local_count += 4;
        }
        if (code) {
        // Here you have shaders
            vgl->GenProgramsARB(1, &vgl->program);
            vgl->BindProgramARB(GL_FRAGMENT_PROGRAM_ARB, vgl->program);
            vgl->ProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,
                                  GL_PROGRAM_FORMAT_ASCII_ARB,
                                  strlen(code), (const GLbyte*)code);
            if (glGetError() == GL_INVALID_OPERATION) {
                /* FIXME if the program was needed for YUV, the video will be broken */
#if 0
                GLint position;
                glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &position);

                const char *msg = (const char *)glGetString(GL_PROGRAM_ERROR_STRING_ARB);
                fprintf(stderr, "GL_INVALID_OPERATION: error at %d: %s\n", position, msg);
#endif
                vgl->DeleteProgramsARB(1, &vgl->program);
                vgl->program = 0;
            }
            free(code);
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
        *subpicture_chromas = NULL;
#if !defined(BROKEN_MACOS_OPENGL) && !USE_OPENGL_ES
        if (supports_npot)
            *subpicture_chromas = gl_subpicture_chromas;
#endif
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

        if (vgl->program)
            vgl->DeleteProgramsARB(1, &vgl->program);

        vlc_gl_Unlock(vgl->gl);
    }
    if (vgl->pool)
        picture_pool_Delete(vgl->pool);
    free(vgl);
}

#ifdef BROKEN_MACOS_OPENGL
struct picture_sys_t {
    vout_display_opengl_t *vgl;
    GLuint *texture;
};

/* Small helper */
static inline GLuint PictureGetTexture(picture_t *picture)
{
    return *picture->p_sys->texture;
}

static int PictureLock(picture_t *picture)
{
    if (!picture->p_sys)
        return VLC_SUCCESS;

    vout_display_opengl_t *vgl = picture->p_sys->vgl;
    if (!vlc_gl_Lock(vgl->gl)) {
        glBindTexture(vgl->tex_target, PictureGetTexture(picture));
        glTexSubImage2D(vgl->tex_target, 0,
                        0, 0, vgl->fmt.i_width, vgl->fmt.i_height,
                        vgl->tex_format, vgl->tex_type, picture->p[0].p_pixels);

        vlc_gl_Unlock(vgl->gl);
    }
    return VLC_SUCCESS;
}

static void PictureUnlock(picture_t *picture)
{
    VLC_UNUSED(picture);
}
#endif

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

#ifdef BROKEN_MACOS_OPENGL
        picture_sys_t *sys = picture[count]->p_sys = malloc(sizeof(*sys));
        if (sys) {
            sys->vgl = vgl;
            sys->texture = vgl->texture[count];
        }
#endif
    }
    if (count <= 0)
        return NULL;

    /* Wrap the pictures into a pool */
    picture_pool_configuration_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.picture_count = count;
    cfg.picture       = picture;
#ifdef BROKEN_MACOS_OPENGL
    cfg.lock          = PictureLock;
    cfg.unlock        = PictureUnlock;
#endif
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
                vgl->ActiveTextureARB(GL_TEXTURE0_ARB + j);
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

#ifdef BROKEN_MACOS_OPENGL
            /* Tell the driver not to make a copy of the texture but to use
               our buffer */
            glEnable(GL_UNPACK_CLIENT_STORAGE_APPLE);
            glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);

            /* Use AGP texturing */
            glTexParameteri(vgl->tex_target, GL_TEXTURE_STORAGE_HINT_APPLE,
                             GL_STORAGE_SHARED_APPLE);
#endif

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
        picture_Delete(picture[i]);
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

#ifdef BROKEN_MACOS_OPENGL
    /* Bind to the texture for drawing */
    glBindTexture(vgl->tex_target, PictureGetTexture(picture));
#else
    /* Update the texture */
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        if (vgl->use_multitexture)
            vgl->ActiveTextureARB(GL_TEXTURE0_ARB + j);
        glBindTexture(vgl->tex_target, vgl->texture[0][j]);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, picture->p[j].i_pitch / picture->p[j].i_pixel_pitch);
        glTexSubImage2D(vgl->tex_target, 0,
                        0, 0,
                        vgl->fmt.i_width  * vgl->chroma->p[j].w.num / vgl->chroma->p[j].w.den,
                        vgl->fmt.i_height * vgl->chroma->p[j].h.num / vgl->chroma->p[j].h.den,
                        vgl->tex_format, vgl->tex_type, picture->p[j].p_pixels);
    }
#endif

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
            vgl->ActiveTextureARB(GL_TEXTURE0_ARB + 0);
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
                if (last[i].texture &&
                    last[i].width  == glr->width &&
                    last[i].height == glr->height &&
                    last[i].format == glr->format &&
                    last[i].type   == glr->type) {
                    glr->texture = last[i].texture;
                    memset(&last[i], 0, sizeof(last[i]));
                    break;
                }
            }

            if (glr->texture) {
                glBindTexture(GL_TEXTURE_2D, glr->texture);
                /* TODO set GL_UNPACK_ALIGNMENT */
                glPixelStorei(GL_UNPACK_ROW_LENGTH, r->p_picture->p->i_pitch / r->p_picture->p->i_pixel_pitch);
                glTexSubImage2D(GL_TEXTURE_2D, 0,
                                0, 0, glr->width, glr->height,
                                glr->format, glr->type, r->p_picture->p->p_pixels);
            } else {
                glGenTextures(1, &glr->texture);
                glBindTexture(GL_TEXTURE_2D, glr->texture);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1.0);
                glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                /* TODO set GL_UNPACK_ALIGNMENT */
                glPixelStorei(GL_UNPACK_ROW_LENGTH, r->p_picture->p->i_pitch / r->p_picture->p->i_pixel_pitch);
                glTexImage2D(GL_TEXTURE_2D, 0, glr->format,
                             glr->width, glr->height, 0, glr->format, glr->type,
                             r->p_picture->p->p_pixels);
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

    if (vgl->program) {
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
        for (int i = 0; i < vgl->local_count; i++)
            vgl->ProgramLocalParameter4fvARB(GL_FRAGMENT_PROGRAM_ARB, i, vgl->local_value[i]);
    } else {
        glEnable(vgl->tex_target);
    }

#if USE_OPENGL_ES
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
    glVertexPointer(2, GL_FLOAT, 0, vertexCoord);
    glTexCoordPointer(2, GL_FLOAT, 0, textureCoord);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#else
#if !defined(BROKEN_MACOS_OPENGL)
    for (unsigned j = 0; j < vgl->chroma->plane_count; j++) {
        if (vgl->use_multitexture)
            vgl->ActiveTextureARB(GL_TEXTURE0_ARB + j);
        glBindTexture(vgl->tex_target, vgl->texture[0][j]);
    }
#endif
    glBegin(GL_POLYGON);

    glTexCoord2f(left[0],  top[0]);
    for (unsigned j = 1; j < vgl->chroma->plane_count; j++)
        vgl->MultiTexCoord2fARB(GL_TEXTURE0_ARB + j, left[j], top[j]);
    glVertex2f(-1.0,  1.0);

    glTexCoord2f(right[0], top[0]);
    for (unsigned j = 1; j < vgl->chroma->plane_count; j++)
        vgl->MultiTexCoord2fARB(GL_TEXTURE0_ARB + j, right[j], top[j]);
    glVertex2f( 1.0,  1.0);

    glTexCoord2f(right[0], bottom[0]);
    for (unsigned j = 1; j < vgl->chroma->plane_count; j++)
        vgl->MultiTexCoord2fARB(GL_TEXTURE0_ARB + j, right[j], bottom[j]);
    glVertex2f( 1.0, -1.0);

    glTexCoord2f(left[0],  bottom[0]);
    for (unsigned j = 1; j < vgl->chroma->plane_count; j++)
        vgl->MultiTexCoord2fARB(GL_TEXTURE0_ARB + j, left[j], bottom[j]);
    glVertex2f(-1.0, -1.0);

    glEnd();
#endif

    if (vgl->program)
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
    else
        glDisable(vgl->tex_target);

#if !USE_OPENGL_ES
    if (vgl->use_multitexture)
        vgl->ActiveTextureARB(GL_TEXTURE0_ARB + 0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < vgl->region_count; i++) {
        gl_region_t *glr = &vgl->region[i];

        glBindTexture(GL_TEXTURE_2D, glr->texture);

        glBegin(GL_POLYGON);

        glColor4f(1.0, 1.0, 1.0, glr->alpha);

        glTexCoord2f(0.0, 0.0);
        glVertex2f(glr->left, glr->top);

        glTexCoord2f(1.0, 0.0);
        glVertex2f(glr->right, glr->top);

        glTexCoord2f(1.0, 1.0);
        glVertex2f(glr->right, glr->bottom);

        glTexCoord2f(0.0, 1.0);
        glVertex2f(glr->left, glr->bottom);

        glEnd();
    }
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
#endif

    vlc_gl_Swap(vgl->gl);

    vlc_gl_Unlock(vgl->gl);
    return VLC_SUCCESS;
}

