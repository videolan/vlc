#version 440

/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

#define LAYER_MAX 1.5
#define LAYER_INCREMENT 0.2 // increment
#define ANTIALIASING

layout(location = 0) out vec4 fragColor; // premultiplied

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;

    vec2 windowSize;
    float time; // seed
    vec4 color; // snowflake color
};

vec3 voronoi( in vec2 x )
{
    // Pending implementation
    return vec3(0.0, 0.0, 0.0);
}

float sdSnowflake( in vec2 p )
{
    // Pending implementation
    return 0.0;
}

void main()
{
    vec2 p = gl_FragCoord.xy / windowSize.xx;

    vec3 col = vec3(0.0, 0.0, 0.0);

    // Multiple layers
    for (float i = 1.0; i <= LAYER_MAX; i += LAYER_INCREMENT)
    {
        vec3 c = voronoi((6.0 * p + sign(qt_Matrix[3][1]) * vec2(sin(time) / 2.0, time)) * i);

        // Snowflake size depends on the layer:
        float dist = sdSnowflake(c.yz * 8.0 * i);

        // additive:
#ifdef ANTIALIASING
        float fw = fwidth(dist) * 0.75; // for AA
        col += (1.0 - smoothstep(-fw , fw, dist)) * color.rgb * color.a;
#else
        col += (1.0 - step(0.0, dist)) * color.rgb * color.a;
#endif
    }

    col *= qt_Opacity;

    fragColor = vec4(col, 0.0); // premultiplied additive
}
