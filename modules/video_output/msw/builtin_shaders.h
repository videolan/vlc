/*****************************************************************************
 * builtin_shaders.h: Builtin HLSL shader functions.
 *****************************************************************************
 * Copyright (C) 2014 the VideoLAN team
 *
 * Authors: Sasha Koruga <skoruga@gmail.com>,
 *          Felix Abecassis <felix.abecassis@gmail.com>
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

static const char shader_disabled_source[] =
    "sampler2D screen;\n"
    "float4 main(float2 screenCoords : TEXCOORD0) : COLOR\n"
    "{\n"
    "    return saturate(tex2D(screen, screenCoords.xy));\n"
    "}\n";

static const char shader_invert_source[] =
    "sampler2D screen;\n"
    "float4 main(float2 screenCoords : TEXCOORD0) : COLOR\n"
    "{\n"
    "    float4 color = tex2D(screen, screenCoords.xy);\n"
    "    color.r = 1.0 - color.r;\n"
    "    color.g = 1.0 - color.g;\n"
    "    color.b = 1.0 - color.b;\n"
    "    return color;\n"
    "}\n";

static const char shader_grayscale_source[] =
    "sampler2D screen;\n"
    "float4 main(float2 screenCoords : TEXCOORD0) : COLOR0\n"
    "{\n"
    "    float4 color = tex2D(screen, screenCoords.xy);\n"
    "    float gray = 0.2989 * color.r + 0.5870 * color.g + 0.1140 * color.b;\n"
    "    color.r = color.g = color.b = gray;\n"
    "    return color;\n"
    "}\n";

static const char shader_convert601to709_source[] =
    "sampler2D screen;\n"
    "float4 rgb_to_yuv601(float4 RGB)\n"
    "{\n"
    "    float Kr = 0.299;\n"
    "    float Kg = 0.587;\n"
    "    float Kb = 0.114;\n"
    "    float Y = Kr*RGB.r + Kg*RGB.g + Kb*RGB.b;\n"
    "    float V = (RGB.r-Y)/(1-Kr);\n"
    "    float U = (RGB.b-Y)/(1-Kb);\n"
    "    return float4(Y,U,V,1);\n"
    "}\n"

    "float4 yuv709_to_rgb(float4 YUV)\n"
    "{\n"
    "    float Kr = 0.2125;\n"
    "    float Kg = 0.7154;\n"
    "    float Kb = 0.0721;\n"
    "    float Y = YUV.x;\n"
    "    float U = YUV.y;\n"
    "    float V = YUV.z;\n"
    "    float R = Y + V*(1-Kr);\n"
    "    float G = Y - U*(1-Kb)*Kb/Kg - V*(1-Kr)*Kr/Kg;\n"
    "    float B = Y + U*(1-Kb);\n"
    "    return float4(R,G,B,1);\n"
    "}\n"

    "float4 main(float2 screenCoords : TEXCOORD0) : COLOR0\n"
    "{\n"
    "    float4 color = tex2D(screen, screenCoords.xy);\n"
    "    return yuv709_to_rgb(rgb_to_yuv601(color));\n"
    "}\n";

static const char shader_gammacorrection18_source[] =
    "sampler2D screen;\n"
    "float4 main(float2 screenCoords : TEXCOORD0) : COLOR0\n"
    "{\n"
    "    float4 color = tex2D( screen, screenCoords.xy);\n"
    "    color = pow(color,1.0/1.8);\n"
    "    return color;\n"
    "}\n";

static const char shader_gammacorrection22_source[] =
    "sampler2D screen;\n"
    "float4 main(float2 screenCoords : TEXCOORD0) : COLOR0\n"
    "{\n"
    "    float4 color = tex2D( screen, screenCoords.xy);\n"
    "    color = pow(color,1.0/2.2);\n"
    "    return color;\n"
    "}\n";

static const char shader_gammacorrectionbt709_source[] =
    "sampler2D screen;\n"
    "float4 main(float2 screenCoords : TEXCOORD0) : COLOR0\n"
    "{\n"
    "    float4 color = tex2D(screen, screenCoords.xy);\n"
    "    if(color.r > 0.018)\n"
    "        color.r = 1.099 * pow(color.r,0.45) - 0.099;\n"
    "    else\n"
    "        color.r = 4.5138 * color.r;\n"
    "    if(color.g > 0.018)\n"
    "        color.g = 1.099 * pow(color.g,0.45) - 0.099;\n"
    "    else\n"
    "        color.g = 4.5138 * color.g;\n"
    "    if(color.b > 0.018)\n"
    "        color.b = 1.099 * pow(color.b,0.45) - 0.099;\n"
    "    else\n"
    "        color.b = 4.5138 * color.b;\n"
    "    return color;\n"
    "}\n";

static const char shader_widencolorspace_source[] =
    "sampler2D screen;\n"
    "float4 main(float2 screenCoords : TEXCOORD0) : COLOR0\n"
    "{\n"
    "    float4 color = tex2D(screen, screenCoords.xy);\n"
    "    color.r = max(color.r - 0.0627450980392157,0) * 1.164383561643836;\n"
    "    color.g = max(color.g - 0.0627450980392157,0) * 1.164383561643836;\n"
    "    color.b = max(color.b - 0.0627450980392157,0) * 1.164383561643836;\n"
    "    return saturate(color);\n"
    "}\n";

typedef struct
{
    const char *name;
    const char *code;
} builtin_shader_t;

static const builtin_shader_t builtin_shaders[] =
{
    { "Disabled", shader_disabled_source },
    { "Invert", shader_invert_source },
    { "Grayscale", shader_grayscale_source },
    { "Convert601to709", shader_convert601to709_source },
    { "GammaCorrection18", shader_gammacorrection18_source },
    { "GammaCorrection22", shader_gammacorrection22_source },
    { "GammaCorrectionBT709", shader_gammacorrectionbt709_source },
    { "WidenColorSpace", shader_widencolorspace_source },
};
#define BUILTIN_SHADERS_COUNT (sizeof(builtin_shaders)/sizeof(builtin_shaders[0]))
