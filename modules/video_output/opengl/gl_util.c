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

#include <vlc_filter.h>
#include <vlc_modules.h>
#include <vlc_configuration.h>

#include <vlc_memstream.h>

static void
LogShader(vlc_object_t *obj, const char *prefix, const opengl_vtable_t *vt, GLuint id)
{
    GLint size;
    vt->GetShaderiv(id, GL_SHADER_SOURCE_LENGTH, &size);
    char *sources = malloc(size + 1);
    if (sources == NULL)
        return;

    vt->GetShaderSource(id, size + 1, NULL, sources);

    struct vlc_memstream stream;
    int ret = vlc_memstream_open(&stream);

    /* This is debugging code, we don't want an hard failure if
     * the allocation failed. */
    if (ret != 0)
    {
        free(sources);
        return;
    }

    const char *cursor = sources;
    size_t line = 1;
    while (cursor != NULL && *cursor != '\0')
    {
        const char *end = strchr(cursor, '\n');
        if (end != NULL)
        {
            vlc_memstream_printf(&stream, "%4zu: %.*s\n", line, (int)(ptrdiff_t)(end - cursor), cursor);
            cursor = end + 1;
        }
        else
        {
            vlc_memstream_printf(&stream, "%4zu: %s", line, cursor);
            break;
        }
        line++;
    }
    free(sources);

    ret = vlc_memstream_close(&stream);
    if (ret != 0)
    {
        return;
    }

    msg_Err(obj, "%s%s", prefix, stream.ptr);
    free(stream.ptr);
}

static void
LogShaderErrors(vlc_object_t *obj, const opengl_vtable_t *vt, GLuint id)
{
    GLint info_len;
    vt->GetShaderiv(id, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len <= 0)
        return;

    char *info_log = malloc(info_len);
    if (info_log == NULL)
        return;

    GLsizei written;
    vt->GetShaderInfoLog(id, info_len, &written, info_log);

    LogShader(obj, "Shader source:\n", vt, id);

    msg_Err(obj, "shader: %s", info_log);
    free(info_log);
}

static void
LogProgramErrors(vlc_object_t *obj, const opengl_vtable_t *vt, GLuint id)
{
    GLint info_len;
    vt->GetProgramiv(id, GL_INFO_LOG_LENGTH, &info_len);
    if (info_len <= 0)
        return;
    char *info_log = malloc(info_len);
    if (info_log == NULL)
        return;


    GLsizei shader_count;
    GLuint shaders[3];
    vt->GetAttachedShaders(id, 2, &shader_count, shaders);
    for (GLsizei i = 0; i < shader_count; ++i)
    {
        GLint shader_type;
        vt->GetShaderiv(shaders[i], GL_SHADER_TYPE, &shader_type);

        const char *prefix =
            shader_type == GL_VERTEX_SHADER ? "vertex shader:\n" :
            shader_type == GL_FRAGMENT_SHADER ? "fragment shader:\n" :
            "unknown shader:\n";
        LogShader(obj, prefix, vt, shaders[i]);
    }

    GLsizei written;
    vt->GetProgramInfoLog(id, info_len, &written, info_log);
    msg_Err(obj, "program: %s", info_log);
    free(info_log);
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

module_t *
vlc_gl_WrapOpenGLFilter(filter_t *filter, const char *opengl_filter_name)
{
    const config_chain_t *prev_chain = filter->p_cfg;
    var_Create(filter, "opengl-filter", VLC_VAR_STRING);
    var_SetString(filter, "opengl-filter", opengl_filter_name);

    filter->p_cfg = NULL;
    module_t *module = vlc_filter_LoadModule(filter, "video filter", "opengl", true);
    filter->p_cfg = prev_chain;

    var_Destroy(filter, "opengl-filter");

    return module;
}
