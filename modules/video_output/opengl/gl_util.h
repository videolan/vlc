/*****************************************************************************
 * gl_util.h
 *****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 * Copyright (C) 2020 Videolabs
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

#ifndef VLC_GL_UTIL_H
#define VLC_GL_UTIL_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "gl_common.h"

static const float MATRIX4_IDENTITY[4*4] = {
    1, 0, 0, 0,
    0, 1, 0, 0,
    0, 0, 1, 0,
    0, 0, 0, 1,
};

static const float MATRIX3_IDENTITY[3*3] = {
    1, 0, 0,
    0, 1, 0,
    0, 0, 1,
};

/** Return the smallest larger or equal power of 2 */
static inline unsigned vlc_align_pot(unsigned x)
{
    unsigned align = 1 << (8 * sizeof (unsigned) - vlc_clz(x));
    return ((align >> 1) == x) ? x : align;
}

/**
 * Build an OpenGL program
 *
 * Both the fragment shader and fragment shader are passed as a list of
 * strings, forming the shader source code once concatenated, like
 * glShaderSource().
 *
 * \param obj a VLC object, used to log messages
 * \param vt the OpenGL virtual table
 * \param vstring_count the number of strings in vstrings
 * \param vstrings a list of NUL-terminated strings containing the vertex
 *                 shader source code
 * \param fstring_count the number of strings in fstrings
 * \param fstrings a list of NUL-terminated strings containing the fragment
 *                 shader source code
 */
GLuint
vlc_gl_BuildProgram(vlc_object_t *obj, const opengl_vtable_t *vt,
                    GLsizei vstring_count, const GLchar **vstrings,
                    GLsizei fstring_count, const GLchar **fstrings);

#endif
