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

#include <vlc_common.h>
#include <vlc_picture_pool.h>
#include <vlc_opengl.h>

#include "opengl.h"
// Define USE_OPENGL_ES to the GL ES Version you want to select

#if !defined (__APPLE__)
# if USE_OPENGL_ES == 2
#  include <GLES2/gl2ext.h>
# elif USE_OPENGL_ES == 1
#  include <GLES/glext.h>
//# else
//# include <GL/glext.h>
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
#endif

#ifndef YCBCR_MESA
# define YCBCR_MESA 0x8757
#endif
#ifndef UNSIGNED_SHORT_8_8_MESA
# define UNSIGNED_SHORT_8_8_MESA 0x85BA
#endif
/* RV16 */
#ifndef GL_UNSIGNED_SHORT_5_6_5
# define GL_UNSIGNED_SHORT_5_6_5 0x8363
#endif
#ifndef GL_CLAMP_TO_EDGE
# define GL_CLAMP_TO_EDGE 0x812F
#endif


#if USE_OPENGL_ES
# define VLCGL_TARGET GL_TEXTURE_2D

# define VLCGL_RGB_FORMAT GL_RGB
# define VLCGL_RGB_TYPE   GL_UNSIGNED_SHORT_5_6_5

// Use RGB with OpenGLES
# define VLCGL_FORMAT VLCGL_RGB_FORMAT
# define VLCGL_TYPE   VLCGL_RGB_TYPE

#elif defined(MACOS_OPENGL)

/* On OS X, use GL_TEXTURE_RECTANGLE_EXT instead of GL_TEXTURE_2D.
   This allows sizes which are not powers of 2 */
# define VLCGL_TARGET GL_TEXTURE_RECTANGLE_EXT

/* OS X OpenGL supports YUV. Hehe. */
# define VLCGL_FORMAT GL_YCBCR_422_APPLE
# define VLCGL_TYPE   GL_UNSIGNED_SHORT_8_8_APPLE

#else

# define VLCGL_TARGET GL_TEXTURE_2D

/* RV32 */
# define VLCGL_RGB_FORMAT GL_RGBA
# define VLCGL_RGB_TYPE GL_UNSIGNED_BYTE

/* YUY2 */
# define VLCGL_YUV_FORMAT YCBCR_MESA
# define VLCGL_YUV_TYPE UNSIGNED_SHORT_8_8_MESA

/* Use RGB on Win32/GLX */
# define VLCGL_FORMAT VLCGL_RGB_FORMAT
# define VLCGL_TYPE   VLCGL_RGB_TYPE
#endif

static inline int GetAlignedSize(unsigned size)
{
    /* Return the smallest larger or equal power of 2 */
    unsigned align = 1 << (8 * sizeof (unsigned) - clz(size));
    return ((align >> 1) == size) ? size : align;
}

int vout_display_opengl_Init(vout_display_opengl_t *vgl,
                             video_format_t *fmt,
                             vlc_gl_t *gl)
{
    vgl->gl = gl;

    /* Find the chroma we will use and update fmt */
    /* TODO: We use YCbCr on Mac which is Y422, but on OSX it seems to == YUY2. Verify */
#if (defined(WORDS_BIGENDIAN) && VLCGL_FORMAT == GL_YCBCR_422_APPLE) || (VLCGL_FORMAT == YCBCR_MESA)
    fmt->i_chroma = VLC_CODEC_YUYV;
    vgl->tex_pixel_size = 2;
#elif defined(GL_YCBCR_422_APPLE) && (VLCGL_FORMAT == GL_YCBCR_422_APPLE)
    fmt->i_chroma = VLC_CODEC_UYVY;
    vgl->tex_pixel_size = 2;
#elif VLCGL_FORMAT == GL_RGB
#   if VLCGL_TYPE == GL_UNSIGNED_BYTE
    fmt->i_chroma = VLC_CODEC_RGB24;
#       if defined(WORDS_BIGENDIAN)
    fmt->i_rmask = 0x00ff0000;
    fmt->i_gmask = 0x0000ff00;
    fmt->i_bmask = 0x000000ff;
#       else
    fmt->i_rmask = 0x000000ff;
    fmt->i_gmask = 0x0000ff00;
    fmt->i_bmask = 0x00ff0000;
#       endif
    vgl->tex_pixel_size = 3;
#   else
    fmt->i_chroma = VLC_CODEC_RGB16;
#       if defined(WORDS_BIGENDIAN)
    fmt->i_rmask = 0x001f;
    fmt->i_gmask = 0x07e0;
    fmt->i_bmask = 0xf800;
#       else
    fmt->i_rmask = 0xf800;
    fmt->i_gmask = 0x07e0;
    fmt->i_bmask = 0x001f;
#       endif
    vgl->tex_pixel_size = 2;
#   endif
#else
    fmt->i_chroma = VLC_CODEC_RGB32;
#       if defined(WORDS_BIGENDIAN)
    fmt->i_rmask = 0xff000000;
    fmt->i_gmask = 0x00ff0000;
    fmt->i_bmask = 0x0000ff00;
#       else
    fmt->i_rmask = 0x000000ff;
    fmt->i_gmask = 0x0000ff00;
    fmt->i_bmask = 0x00ff0000;
#       endif
    vgl->tex_pixel_size = 4;
#endif

    vgl->fmt = *fmt;

    /* */
    for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++) {
        vgl->texture[i] = 0;
        vgl->buffer[i]  = NULL;
    }
    vgl->pool = NULL;

    bool supports_npot = false;
#if USE_OPENGL_ES == 2
    supports_npot = true;
#elif defined(MACOS_OPENGL)
    supports_npot = true;
#endif

#if defined(__APPLE__) && USE_OPENGL_ES == 1
    if (!vlc_gl_Lock(vgl->gl)) {
        const char* extensions = (char*) glGetString(GL_EXTENSIONS);
        if (extensions) {
            bool npot = strstr(extensions, "GL_APPLE_texture_2D_limited_npot") != 0;
            if (npot)
                supports_npot = true;
        }
        vlc_gl_Unlock(vgl->gl);
    }
#endif

    /* Texture size */
    if (supports_npot) {
        vgl->tex_width  = fmt->i_width;
        vgl->tex_height = fmt->i_height;
    }
    else {
        /* A texture must have a size aligned on a power of 2 */
        vgl->tex_width  = GetAlignedSize(fmt->i_width);
        vgl->tex_height = GetAlignedSize(fmt->i_height);
    }


    /* */
    if (!vlc_gl_Lock(vgl->gl)) {

        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_CULL_FACE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        vlc_gl_Unlock(vgl->gl);
    }
    return VLC_SUCCESS;
}

void vout_display_opengl_Clean(vout_display_opengl_t *vgl)
{
    /* */
    if (!vlc_gl_Lock(vgl->gl)) {

        glFinish();
        glFlush();
        glDeleteTextures(VLCGL_TEXTURE_COUNT, vgl->texture);

        vlc_gl_Unlock(vgl->gl);
    }
    if (vgl->pool) {
        picture_pool_Delete(vgl->pool);
        for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++)
            free(vgl->buffer[i]);
    }
}

int vout_display_opengl_ResetTextures(vout_display_opengl_t *vgl)
{
    if (vlc_gl_Lock(vgl->gl))
        return VLC_EGENERIC;

    glDeleteTextures(VLCGL_TEXTURE_COUNT, vgl->texture);

    glGenTextures(VLCGL_TEXTURE_COUNT, vgl->texture);
    for (int i = 0; i < VLCGL_TEXTURE_COUNT; i++) {
        glBindTexture(VLCGL_TARGET, vgl->texture[i]);

#if !USE_OPENGL_ES
        /* Set the texture parameters */
        glTexParameterf(VLCGL_TARGET, GL_TEXTURE_PRIORITY, 1.0);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
#endif

        glTexParameteri(VLCGL_TARGET, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(VLCGL_TARGET, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(VLCGL_TARGET, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(VLCGL_TARGET, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

#ifdef MACOS_OPENGL
        /* Tell the driver not to make a copy of the texture but to use
           our buffer */
        glEnable(GL_UNPACK_CLIENT_STORAGE_APPLE);
        glPixelStorei(GL_UNPACK_CLIENT_STORAGE_APPLE, GL_TRUE);

#if 0
        /* Use VRAM texturing */
        glTexParameteri(VLCGL_TARGET, GL_TEXTURE_STORAGE_HINT_APPLE,
                         GL_STORAGE_CACHED_APPLE);
#else
        /* Use AGP texturing */
        glTexParameteri(VLCGL_TARGET, GL_TEXTURE_STORAGE_HINT_APPLE,
                         GL_STORAGE_SHARED_APPLE);
#endif
#endif

        /* Call glTexImage2D only once, and use glTexSubImage2D later */
        if (vgl->buffer[i]) {
            glTexImage2D(VLCGL_TARGET, 0, VLCGL_FORMAT, vgl->tex_width,
                         vgl->tex_height, 0, VLCGL_FORMAT, VLCGL_TYPE,
                         vgl->buffer[i]);
        }
    }

    vlc_gl_Unlock(vgl->gl);
    return VLC_SUCCESS;
}

#ifdef MACOS_OPENGL
/* XXX See comment vout_display_opengl_Prepare */
struct picture_sys_t {
    vout_display_opengl_t *vgl;
    GLuint *texture;
};

/* Small helper */
static inline GLuint get_texture(picture_t *picture)
{
    return *picture->p_sys->texture;
}

static int PictureLock(picture_t *picture)
{
    if (!picture->p_sys)
        return VLC_SUCCESS;

    vout_display_opengl_t *vgl = picture->p_sys->vgl;
    if (!vlc_gl_Lock(vgl->gl)) {
        glBindTexture(VLCGL_TARGET, get_texture(picture));
        glTexSubImage2D(VLCGL_TARGET, 0, 0, 0,
                        vgl->fmt.i_width, vgl->fmt.i_height,
                        VLCGL_FORMAT, VLCGL_TYPE, picture->p[0].p_pixels);

        vlc_gl_Unlock(vgl->gl);
    }
    return VLC_SUCCESS;
}

static void PictureUnlock(picture_t *picture)
{
    VLC_UNUSED(picture);
}
#endif

picture_pool_t *vout_display_opengl_GetPool(vout_display_opengl_t *vgl)
{
    picture_t *picture[VLCGL_TEXTURE_COUNT];

    int i;
    for (i = 0; i < VLCGL_TEXTURE_COUNT; i++) {

        /* TODO memalign would be way better */
        vgl->buffer[i] = malloc(vgl->tex_width * vgl->tex_height * vgl->tex_pixel_size);
        if (!vgl->buffer[i])
            break;

        picture_resource_t rsc;
        memset(&rsc, 0, sizeof(rsc));
#ifdef MACOS_OPENGL
        rsc.p_sys = malloc(sizeof(*rsc.p_sys));
        if (rsc.p_sys)
        {
            rsc.p_sys->vgl = vgl;
            rsc.p_sys->texture = &vgl->texture[i];
        }
#endif
        rsc.p[0].p_pixels = vgl->buffer[i];
        rsc.p[0].i_pitch  = vgl->fmt.i_width * vgl->tex_pixel_size;
        rsc.p[0].i_lines  = vgl->fmt.i_height;

        picture[i] = picture_NewFromResource(&vgl->fmt, &rsc);
        if (!picture[i]) {
            free(vgl->buffer[i]);
            vgl->buffer[i] = NULL;
            break;
        }
    }
    if (i < VLCGL_TEXTURE_COUNT)
        goto error;

    /* */
    picture_pool_configuration_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.picture_count = i;
    cfg.picture = picture;
#ifdef MACOS_OPENGL
    cfg.lock = PictureLock;
    cfg.unlock = PictureUnlock;
#endif
    vgl->pool = picture_pool_NewExtended(&cfg);
    if (!vgl->pool)
        goto error;

    vout_display_opengl_ResetTextures(vgl);

    return vgl->pool;

error:
    for (int j = 0; j < i; j++) {
        picture_Delete(picture[j]);
        vgl->buffer[j] = NULL;
    }
    return NULL;
}

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture)
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

#ifdef MACOS_OPENGL
    /* Bind to the texture for drawing */
    glBindTexture(VLCGL_TARGET, get_texture(picture));
#else
    /* Update the texture */
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    vgl->fmt.i_width, vgl->fmt.i_height,
                    VLCGL_FORMAT, VLCGL_TYPE, picture->p[0].p_pixels);
#endif

    vlc_gl_Unlock(vgl->gl);
    return VLC_SUCCESS;
}

int vout_display_opengl_Display(vout_display_opengl_t *vgl,
                                const video_format_t *source)
{
    if (vlc_gl_Lock(vgl->gl))
        return VLC_EGENERIC;

    /* glTexCoord works differently with GL_TEXTURE_2D and
       GL_TEXTURE_RECTANGLE_EXT */
#if VLCGL_TARGET == GL_TEXTURE_2D
    const float f_normw = vgl->tex_width;
    const float f_normh = vgl->tex_height;
#elif defined (GL_TEXTURE_RECTANGLE_EXT) \
   && (VLCGL_TARGET == GL_TEXTURE_RECTANGLE_EXT)
    const float f_normw = 1.0;
    const float f_normh = 1.0;
#else
# error Unknown texture type!
#endif

    float f_x      = (source->i_x_offset +                       0 ) / f_normw;
    float f_y      = (source->i_y_offset +                       0 ) / f_normh;
    float f_width  = (source->i_x_offset + source->i_visible_width ) / f_normw;
    float f_height = (source->i_y_offset + source->i_visible_height) / f_normh;

    /* Why drawing here and not in Render()? Because this way, the
       OpenGL providers can call vout_display_opengl_Display to force redraw.i
       Currently, the OS X provider uses it to get a smooth window resizing */

    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(VLCGL_TARGET);

#if USE_OPENGL_ES
    static const GLfloat vertexCoord[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };

    const GLfloat textureCoord[8] = {
        f_x,     f_height,
        f_width, f_height,
        f_x,     f_y,
        f_width, f_y
    };

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertexCoord);
    glTexCoordPointer(2, GL_FLOAT, 0, textureCoord);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
#else
    glBegin(GL_POLYGON);
    glTexCoord2f(f_x,      f_y);      glVertex2f(-1.0,  1.0);
    glTexCoord2f(f_width,  f_y);      glVertex2f( 1.0,  1.0);
    glTexCoord2f(f_width,  f_height); glVertex2f( 1.0, -1.0);
    glTexCoord2f(f_x,      f_height); glVertex2f(-1.0, -1.0);
    glEnd();
#endif

    glDisable(VLCGL_TARGET);

    vlc_gl_Swap(vgl->gl);

    vlc_gl_Unlock(vgl->gl);
    return VLC_SUCCESS;
}

