#version 440

/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

// Shared vertex shader of `BackdropBlendItem` subclasses.

layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;

// Position in item coordinates:
layout(location = 0) out vec2 coord;

// Only the common part of the uniform block is declared here, the
// fragment shaders declare the full block (see `BackdropBlendItem`):
layout(std140, binding = 0) uniform qt_buf {
    mat4 qt_Matrix;
    float qt_Opacity;
} qt_ubuf; // must use a name that does not clash with the fragment shader

void main()
{
    coord = qt_MultiTexCoord0;
    gl_Position = qt_ubuf.qt_Matrix * qt_Vertex;
}
