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
#include <vlc_opengl.h>
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

/* In column-major order */
static const float MATRIX2x3_IDENTITY[2*3] = {
    1, 0,
    0, 1,
    0, 0,
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

/**
 * Wrap an OpenGL filter from a video filter
 *
 * Open an OpenGL filter (with capability "opengl filter") from a video filter
 * (with capability "video filter").
 *
 * This internally uses the "opengl" video filter to load the OpenGL filter
 * with the given name.
 */
module_t *
vlc_gl_WrapOpenGLFilter(filter_t *filter, const char *opengl_filter_name);

struct vlc_gl_extension_vt {
    PFNGLGETSTRINGPROC      GetString;
    PFNGLGETSTRINGIPROC     GetStringi;
    PFNGLGETINTEGERVPROC    GetIntegerv;
    PFNGLGETERRORPROC       GetError;
};

static inline unsigned
vlc_gl_GetVersionMajor(struct vlc_gl_extension_vt *vt)
{
    GLint version;
    vt->GetIntegerv(GL_MAJOR_VERSION, &version);
    uint32_t error = vt->GetError();

    if (error != GL_NO_ERROR)
        version = 2;

    /* Drain the errors before continuing. */
    while (error != GL_NO_ERROR)
        error = vt->GetError();

    return version;
}


static inline void
vlc_gl_LoadExtensionFunctions(vlc_gl_t *gl, struct vlc_gl_extension_vt *vt)
{
    vt->GetString = vlc_gl_GetProcAddress(gl, "glGetString");
    vt->GetIntegerv = vlc_gl_GetProcAddress(gl, "glGetIntegerv");
    vt->GetError = vlc_gl_GetProcAddress(gl, "glGetError");
    vt->GetStringi = NULL;

    unsigned version = vlc_gl_GetVersionMajor(vt);

    /* glGetStringi is available in OpenGL>=3 and GLES>=3.
     * https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glGetString.xhtml
     * https://www.khronos.org/registry/OpenGL-Refpages/es3/html/glGetString.xhtml
     */
    if (version >= 3)
        vt->GetStringi = vlc_gl_GetProcAddress(gl, "glGetStringi");
}

static inline bool
vlc_gl_HasExtension(
    struct vlc_gl_extension_vt *vt,
    const char *name
){
    if (vt->GetStringi == NULL)
    {
        const GLubyte *extensions = vt->GetString(GL_EXTENSIONS);
        return vlc_gl_StrHasToken((const char *)extensions, name);
    }

    int32_t count = 0;
    vt->GetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (int i = 0; i < count; ++i)
    {
        const uint8_t *extension = vt->GetStringi(GL_EXTENSIONS, i);
        if (strcmp((const char *)extension, name) == 0)
            return true;
    }
    return false;
}

#endif
