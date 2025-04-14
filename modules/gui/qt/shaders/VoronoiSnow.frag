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

/*****************************************************************************
 * The MIT License
 * Copyright (C) 2015 Inigo Quilez
 * Copyright (C) 2017 Inigo Quilez
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *****************************************************************************/

#define ANIMATE

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

// Inigo Quilez's voronoi (https://iquilezles.org/articles/voronoilines):
/// <voronoi>
vec2 hash2( vec2 p )
{
    // procedural white noise
    return fract(sin(vec2(dot(p,vec2(127.1,311.7)),dot(p,vec2(269.5,183.3))))*43758.5453);
}

vec3 voronoi( in vec2 x )
{
    vec2 ip = floor(x);
    vec2 fp = fract(x);

    //----------------------------------
    // first pass: regular voronoi
    //----------------------------------
    vec2 mg, mr;

    float md = 8.0;
    for( int j=-1; j<=1; j++ )
    for( int i=-1; i<=1; i++ )
    {
        vec2 g = vec2(float(i),float(j));
        vec2 o = hash2( ip + g );
        #ifdef ANIMATE
        o = 0.5 + 0.5*sin( time + 6.2831*o );
        #endif
        vec2 r = g + o - fp;
        float d = dot(r,r);

        if( d<md )
        {
            md = d;
            mr = r;
            mg = g;
        }
    }

    //----------------------------------
    // second pass: distance to borders
    //----------------------------------
    md = 8.0;
    for( int j=-2; j<=2; j++ )
    for( int i=-2; i<=2; i++ )
    {
        vec2 g = mg + vec2(float(i),float(j));
        vec2 o = hash2( ip + g );
        #ifdef ANIMATE
        o = 0.5 + 0.5*sin( time + 6.2831*o );
        #endif
        vec2 r = g + o - fp;

        if( dot(mr-r,mr-r)>0.00001 )
        md = min( md, dot( 0.5*(mr+r), normalize(r-mr) ) );
    }

    return vec3( md, mr );
}
/// </voronoi>

// Inigo Quilez's snowflake SDF (https://www.shadertoy.com/view/wsGSD3):
/// <snowflake>
float sdLine( in vec2 p, in vec2 a, in vec2 b )
{
    vec2 pa = p-a, ba = b-a;
    float h = clamp( dot(pa,ba)/dot(ba,ba), 0.0, 1.0 );
    return length( pa - ba*h );
}

vec2 opModPolarMirrored( in vec2 p, float theta, float offset)
{
    float a = atan(p.y, p.x) - offset;
    a = abs(mod(a + .5 * theta, theta) - .5 * theta);
    return length(p) * vec2(cos(a), sin(a));
}

float sdSnowflake( in vec2 p )
{
    p = opModPolarMirrored(p, radians(360.) / 6., radians(90.));

    float d = sdLine( p, vec2(0, 0), vec2(.75, 0) );
    d = min(d, sdLine( p, vec2(.5, 0), vec2(.5, 0) + vec2(.1, .1) ));
    d = min(d, sdLine( p, vec2(.25, 0), vec2(.25, 0) + 1.5 * vec2(.1, .1) ));
    return d - .04;
}
/// </snowflake>

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
