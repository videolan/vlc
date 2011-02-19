/*****************************************************************************
 * opengl.h: OpenGL vout_display helpers
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

// Define USE_OPENGL_ES to the GL ES Version you want to select
#ifndef USE_OPENGL_ES
# define USE_OPENGL_ES 0
#endif

#define VLCGL_TEXTURE_COUNT 1

#if !defined (__APPLE__)
# if USE_OPENGL_ES == 2
#  include <GLES2/gl2.h>
# elif USE_OPENGL_ES == 1
#  include <GLES/gl.h>
# else
#  include <GL/gl.h>
# endif
#else
# if USE_OPENGL_ES == 2
#  include <OpenGLES/ES2/gl.h>
# elif USE_OPENGL_ES == 1
#  include <OpenGLES/ES1/gl.h>
# else
#  define MACOS_OPENGL
#  include <OpenGL/gl.h>
#  undef VLCGL_TEXTURE_COUNT
#  define VLCGL_TEXTURE_COUNT 2
# endif
#endif

typedef struct {
    vlc_gl_t   *gl;

    video_format_t fmt;

    int        tex_pixel_size;
    int        tex_width;
    int        tex_height;

    GLuint     texture[VLCGL_TEXTURE_COUNT];
    uint8_t    *buffer[VLCGL_TEXTURE_COUNT];

    picture_pool_t *pool;
} vout_display_opengl_t;

int vout_display_opengl_Init(vout_display_opengl_t *vgl,
                             video_format_t *fmt, vlc_gl_t *gl);
void vout_display_opengl_Clean(vout_display_opengl_t *vgl);

int vout_display_opengl_ResetTextures(vout_display_opengl_t *vgl);
picture_pool_t *vout_display_opengl_GetPool(vout_display_opengl_t *vgl);

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture);
int vout_display_opengl_Display(vout_display_opengl_t *vgl,
                                const video_format_t *source);
