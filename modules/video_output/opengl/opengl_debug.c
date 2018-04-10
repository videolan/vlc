/*****************************************************************************
 * openg_debug.c: OpenGL and OpenGL ES debug code
 *****************************************************************************
 * Copyright (C) 2018 VLC authors and VideoLAN
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

#include <vlc_common.h>
#include <vlc_opengl.h>

#include "internal.h"

GLint opengl_debug_CreateProgram(opengl_vtable_t vt)
{
    static const char* psz_vertex_shader =
        "uniform mat4 MatProj;\n"
        "uniform mat4 MatView;\n"
        "uniform mat4 MatModel;\n"
        "attribute vec3 VertexPosition;\n"
        "void main() {\n"
        " gl_Position = MatProj * MatView * MatModel * vec4(VertexPosition, 1);\n"
        "}\n";

    static const char* psz_fragment_shader =
        "uniform vec3 Color;\n"
        "void main() {\n"
        " gl_FragColor = vec4(Color, 1);\n"
        "}\n";

    static GLuint vertex_shader = 0;
    static GLuint fragment_shader = 0;
    static GLuint program = 0;

    if (!vertex_shader || !fragment_shader || !program)
    {
        vertex_shader = vt.CreateShader(GL_VERTEX_SHADER);
        GLuint fragment_shader = vt.CreateShader(GL_FRAGMENT_SHADER);

        vt.ShaderSource(vertex_shader, 1, &psz_vertex_shader, NULL);
        vt.ShaderSource(fragment_shader, 1, &psz_fragment_shader, NULL);

        GLint compile_success;
        vt.CompileShader(vertex_shader);
        vt.CompileShader(fragment_shader);

        vt.GetShaderiv(vertex_shader, GL_COMPILE_STATUS, &compile_success);
        if (compile_success == GL_FALSE)
        {
            return 0;
        }

        vt.GetShaderiv(fragment_shader, GL_COMPILE_STATUS, &compile_success);
        if (compile_success == GL_FALSE)
        {
            return 0;
        }

        program = vt.CreateProgram();
        vt.AttachShader(program, vertex_shader);
        vt.AttachShader(program, fragment_shader);
        vt.LinkProgram(program);

        GLint link_success;
        vt.GetProgramiv(program, GL_LINK_STATUS, &link_success);
        if (link_success == GL_FALSE)
        {
            return 0;
        }
    }

    return program;
}


void opengl_debug_DrawLine(opengl_vtable_t vt, float* proj, float* view, float* model,
                           float* from, float *to, float* color)
{
    GLuint previous_program;
    vt.GetIntegerv(GL_CURRENT_PROGRAM, &previous_program);

    GLuint program = opengl_debug_CreateProgram(vt);
    vt.UseProgram(program);

    float vCoords[6];
    memcpy(vCoords, from, 3 * sizeof(float));
    memcpy(vCoords+3, to, 3 * sizeof(float));

    GLint u_color, u_matproj, u_matview, u_matmodel;
    u_color = vt.GetUniformLocation(program, "Color");
    u_matproj = vt.GetUniformLocation(program, "MatProj");
    u_matview = vt.GetUniformLocation(program, "MatView");
    u_matmodel = vt.GetUniformLocation(program, "MatModel");

    GLint a_vertexposition = vt.GetAttribLocation(program, "VertexPosition");

    GLuint vbo;
    vt.GenBuffers(1, &vbo);
    vt.BindBuffer(GL_ARRAY_BUFFER, vbo);
    vt.BufferData(GL_ARRAY_BUFFER, 6 * sizeof(GLfloat), vCoords, GL_STATIC_DRAW);
    vt.EnableVertexAttribArray(a_vertexposition);
    vt.VertexAttribPointer(a_vertexposition, 3, GL_FLOAT, GL_FALSE, 0, 0);
    vt.VertexAttribDivisor(a_vertexposition, 0);

    vt.Uniform3fv(u_color, 1, color);
    vt.UniformMatrix4fv(u_matproj, 1, GL_FALSE, proj);
    vt.UniformMatrix4fv(u_matview, 1, GL_FALSE, view);
    vt.UniformMatrix4fv(u_matmodel, 1, GL_FALSE, model);

    vt.DrawArrays(GL_LINES, 0, 2);

    vt.UseProgram(previous_program);
}

//void opengl_debug_DrawAxes(opengl_vtable_t vt, float* position, float *angles)
//{
//    GLuint previous_program;
//    vt.GetIntegerv(GL_CURRENT_PROGRAM, &previous_program);
//
//    GLuint program = opengl_debug_CreateProgram(vt);
//    vt.UseProgram(program);
//
//    float vCoords[6];
//
//    GLuint vbo;
//    vt.GenBuffers(1, &vbo);
//    vt.BindBuffer(GL_ARRAY_BUFFER, vbo);
//    vt.BufferData(GL_ARRAY_BUFFER, 6 * sizeof(GLfloat), vCoords, GL_STATIC_DRAW);
//    vt.EnableVertexAttribArray(0);
//    vt.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
//
//    vt.Uniform3fv(0, 1, color);
//
//    vt.DrawArrays(GL_LINES, 0, 2);
//
//    vt.UseProgram(previous_program);
//
//}
