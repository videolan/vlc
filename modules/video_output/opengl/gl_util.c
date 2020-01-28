/*****************************************************************************
 * gl_util.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "gl_util.h"

static void
LogShaderErrors(vlc_object_t *obj, const opengl_vtable_t *vt, GLuint id)
{
    GLint info_len;
    vt->GetShaderiv(id, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len > 0)
    {
        char *info_log = malloc(info_len);
        if (info_log)
        {
            GLsizei written;
            vt->GetShaderInfoLog(id, info_len, &written, info_log);
            msg_Err(obj, "shader: %s", info_log);
            free(info_log);
        }
    }
}

static void
LogProgramErrors(vlc_object_t *obj, const opengl_vtable_t *vt, GLuint id)
{
    GLint info_len;
    vt->GetProgramiv(id, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len > 0)
    {
        char *info_log = malloc(info_len);
        if (info_log)
        {
            GLsizei written;
            vt->GetProgramInfoLog(id, info_len, &written, info_log);
            msg_Err(obj, "program: %s", info_log);
            free(info_log);
        }
    }
}

static GLuint
CreateShader(vlc_object_t *obj, const opengl_vtable_t *vt, GLenum type,
             GLsizei count, const GLchar **src)
{
    GLuint shader = vt->CreateShader(type);
    if (!shader)
        return 0;

    vt->ShaderSource(shader, count, src, NULL);
    vt->CompileShader(shader);

    LogShaderErrors(obj, vt, shader);

    GLint compiled;
    vt->GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        msg_Err(obj, "Failed to compile shader");
        vt->DeleteShader(shader);
        return 0;
    }

    return shader;
}


GLuint
vlc_gl_BuildProgram(vlc_object_t *obj, const opengl_vtable_t *vt,
                    GLsizei vstring_count, const GLchar **vstrings,
                    GLsizei fstring_count, const GLchar **fstrings)
{
    GLuint program = 0;

    GLuint vertex_shader = CreateShader(obj, vt, GL_VERTEX_SHADER,
                                        vstring_count, vstrings);
    if (!vertex_shader)
        return 0;

    GLuint fragment_shader = CreateShader(obj, vt, GL_FRAGMENT_SHADER,
                                          fstring_count, fstrings);
    if (!fragment_shader)
        goto finally_1;

    program = vt->CreateProgram();
    if (!program)
        goto finally_2;

    vt->AttachShader(program, vertex_shader);
    vt->AttachShader(program, fragment_shader);

    vt->LinkProgram(program);

    LogProgramErrors(obj, vt, program);

    GLint linked;
    vt->GetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        msg_Err(obj, "Failed to link program");
        vt->DeleteProgram(program);
        program = 0;
    }

finally_2:
    vt->DeleteShader(fragment_shader);
finally_1:
    vt->DeleteShader(vertex_shader);

    return program;
}
