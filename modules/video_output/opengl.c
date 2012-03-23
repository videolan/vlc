/*****************************************************************************
 * opengl.c: OpenGL and OpenGL ES output common code
 *****************************************************************************
 * Copyright (C) 2004-2011 VLC authors and VideoLAN
 * Copyright (C) 2009, 2011 Laurent Aimar
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

#ifdef __APPLE__
# define PFNGLGENPROGRAMSARBPROC              typeof(glGenProgramsARB)*
# define PFNGLBINDPROGRAMARBPROC              typeof(glBindProgramARB)*
# define PFNGLPROGRAMSTRINGARBPROC            typeof(glProgramStringARB)*
# define PFNGLDELETEPROGRAMSARBPROC           typeof(glDeleteProgramsARB)*
# define PFNGLPROGRAMLOCALPARAMETER4FVARBPROC typeof(glProgramLocalParameter4fvARB)*
# define PFNGLACTIVETEXTUREPROC               typeof(glActiveTexture)*
# define PFNGLCLIENTACTIVETEXTUREPROC         typeof(glClientActiveTexture)*
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
    PFNGLACTIVETEXTUREPROC   ActiveTexture;
    PFNGLCLIENTACTIVETEXTUREPROC ClientActiveTexture;
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
#if !defined(MACOS_OPENGL)
        vgl->GenProgramsARB    = (PFNGLGENPROGRAMSARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glGenProgramsARB");
        vgl->BindProgramARB    = (PFNGLBINDPROGRAMARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glBindProgramARB");
        vgl->ProgramStringARB  = (PFNGLPROGRAMSTRINGARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glProgramStringARB");
        vgl->DeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glDeleteProgramsARB");
        vgl->ProgramLocalParameter4fvARB = (PFNGLPROGRAMLOCALPARAMETER4FVARBPROC)vlc_gl_GetProcAddress(vgl->gl, "glProgramLocalParameter4fvARB");
#else
        vgl->GenProgramsARB = glGenProgramsARB;
        vgl->BindProgramARB = glBindProgramARB;
        vgl->ProgramStringARB = glProgramStringARB;
        vgl->DeleteProgramsARB = glDeleteProgramsARB;
        vgl->ProgramLocalParameter4fvARB = glProgramLocalParameter4fvARB;
#endif
        supports_fp = vgl->GenProgramsARB &&
                      vgl->BindProgramARB &&
                      vgl->ProgramStringARB &&
                      vgl->DeleteProgramsARB &&
                      vgl->ProgramLocalParameter4fvARB;
    }

    bool supports_multitexture = false;
    GLint max_texture_units = 0;
    if (HasExtension(extensions, "GL_ARB_multitexture")) {
#if !defined(MACOS_OPENGL)
        vgl->ActiveTexture   = (PFNGLACTIVETEXTUREPROC)vlc_gl_GetProcAddress(vgl->gl, "glActiveTexture");
        vgl->ClientActiveTexture = (PFNGLCLIENTACTIVETEXTUREPROC)vlc_gl_GetProcAddress(vgl->gl, "glClientActiveTexture");
#else
        vgl->ActiveTexture = glActiveTexture;
        vgl->ClientActiveTexture = glClientActiveTexture;
#endif
        supports_multitexture = vgl->ActiveTexture &&
                                vgl->ClientActiveTexture;
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
                { 1.164383561643836,  0.0000,             1.596026785714286, -0.874202217873451 },
                { 1.164383561643836, -0.391762290094914, -0.812967647237771,  0.531667823499146 },
                { 1.164383561643836,  2.017232142857142,  0.0000,            -1.085630789302022 },
            };
            const float matrix_bt709_tv2full[3][4] = {
                { 1.164383561643836,  0.0000,             1.792741071428571, -0.972945075016308 },
                { 1.164383561643836, -0.21324861427373,  -0.532909328559444,  0.301482665475862 },
                { 1.164383561643836,  2.112401785714286,  0.0000,            -1.133402217873451 },
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
#if !USE_OPENGL_ES
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
                vgl->ActiveTexture(GL_TEXTURE0 + j);
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
            vgl->ActiveTexture(GL_TEXTURE0 + j);
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
            vgl->ActiveTexture(GL_TEXTURE0 + 0);
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

    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisable(vgl->tex_target);
#else

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
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glBindTexture(vgl->tex_target, vgl->texture[0][j]);
        glTexCoordPointer(2, GL_FLOAT, 0, texCoord);
    }
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



    if (vgl->program)
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
    else
        glDisable(vgl->tex_target);

    if (vgl->use_multitexture)
        vgl->ActiveTexture(GL_TEXTURE0 + 0);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnableClientState(GL_VERTEX_ARRAY);
    const GLfloat textureCoord[] = {
        0.0, 0.0,
        0.0, 1.0,
        1.0, 0.0,
        1.0, 1.0,
    };
    for (int i = 0; i < vgl->region_count; i++) {
        gl_region_t *glr = &vgl->region[i];
        const GLfloat vertexCoord[] = {
            glr->left, glr->top,
            glr->left, glr->bottom,
            glr->right, glr->top,
            glr->right,glr->bottom,
        };
        glColor4f(1.0f, 1.0f, 1.0f, glr->alpha);
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
#endif

    vlc_gl_Swap(vgl->gl);

    vlc_gl_Unlock(vgl->gl);
    return VLC_SUCCESS;
}

