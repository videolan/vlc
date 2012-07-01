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

#ifdef __APPLE__
# include <TargetConditionals.h>
# if !TARGET_OS_IPHONE
#  define MACOS_OPENGL
#  include <OpenGL/gl.h>
# else
#  define USE_OPENGL_ES 1
#  include <OpenGLES/ES1/gl.h>
#  include <OpenGLES/ES2/gl.h>
#  include <OpenGLES/ES2/glext.h>
# endif
#endif

// Define USE_OPENGL_ES to the GL ES Version you want to select
#ifndef USE_OPENGL_ES
# define USE_OPENGL_ES 0
#endif

#if !defined (__APPLE__)
# if USE_OPENGL_ES == 2
#  include <GLES2/gl2.h>
# elif USE_OPENGL_ES == 1
#  include <GLES/gl.h>
# else
#  ifdef WIN32
#   include <GL/glew.h>
#  else
#   include <GL/glx.h>
#  endif
#  include <GL/gl.h>
# endif
#endif

static inline bool HasExtension(const char *apis, const char *api)
{
    size_t apilen = strlen(api);
    while (apis) {
        while (*apis == ' ')
            apis++;
        if (!strncmp(apis, api, apilen) && memchr(" ", apis[apilen], 2))
            return true;
        apis = strchr(apis, ' ');
    }
    return false;
}

typedef struct vout_display_opengl_t vout_display_opengl_t;

vout_display_opengl_t *vout_display_opengl_New(video_format_t *fmt,
                                               const vlc_fourcc_t **subpicture_chromas,
                                               vlc_gl_t *gl);
void vout_display_opengl_Delete(vout_display_opengl_t *vgl);

picture_pool_t *vout_display_opengl_GetPool(vout_display_opengl_t *vgl, unsigned);

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture, subpicture_t *subpicture);
int vout_display_opengl_Display(vout_display_opengl_t *vgl,
                                const video_format_t *source);
