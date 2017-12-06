/*****************************************************************************
 * vout_helper.h: OpenGL vout_display helpers
 *****************************************************************************
 * Copyright (C) 2004-2016 VLC authors and VideoLAN
 * Copyright (C) 2009 Laurent Aimar
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Rémi Denis-Courmont
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Ilkka Ollakka <ileoo@videolan.org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          Rafaël Carré <funman@videolanorg>
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

#ifndef VLC_OPENGL_VOUT_HELPER_H
#define VLC_OPENGL_VOUT_HELPER_H

#include <vlc_common.h>
#include <vlc_picture_pool.h>
#include <vlc_opengl.h>

/* if USE_OPENGL_ES2 is defined, OpenGL ES version 2 will be used, otherwise
 * normal OpenGL will be used */
#ifdef __APPLE__
# include <TargetConditionals.h>
# if !TARGET_OS_IPHONE
#  undef USE_OPENGL_ES2
#  define MACOS_OPENGL
#  include <OpenGL/gl.h>
# else /* Force ESv2 on iOS */
#  define USE_OPENGL_ES2
#  include <OpenGLES/ES1/gl.h>
#  include <OpenGLES/ES2/gl.h>
#  include <OpenGLES/ES2/glext.h>
# endif
#else /* !defined (__APPLE__) */
# if defined (USE_OPENGL_ES2)
#  include <GLES2/gl2.h>
# else
#  ifdef _WIN32
#   include <GL/glew.h>
#  endif
#  include <GL/gl.h>
# endif
#endif

#define GLCONV_TEXT N_("Open GL/GLES hardware converter")
#define GLCONV_LONGTEXT N_( \
    "Force a \"glconv\" module.")
#define add_glconv() add_module ("glconv", "glconv", NULL, GLCONV_TEXT, GLCONV_LONGTEXT, true)

static const vlc_fourcc_t gl_subpicture_chromas[] = {
    VLC_CODEC_RGBA,
    0
};

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
                                               vlc_gl_t *gl,
                                               const vlc_viewpoint_t *viewpoint);
void vout_display_opengl_Delete(vout_display_opengl_t *vgl);

picture_pool_t *vout_display_opengl_GetPool(vout_display_opengl_t *vgl, unsigned);

int vout_display_opengl_SetViewpoint(vout_display_opengl_t *vgl, const vlc_viewpoint_t*);

void vout_display_opengl_SetWindowAspectRatio(vout_display_opengl_t *vgl,
                                              float f_sar);

void vout_display_opengl_Viewport(vout_display_opengl_t *vgl, int x, int y,
                                  unsigned width, unsigned height);

int vout_display_opengl_Prepare(vout_display_opengl_t *vgl,
                                picture_t *picture, subpicture_t *subpicture);
int vout_display_opengl_Display(vout_display_opengl_t *vgl,
                                const video_format_t *source);

#endif
