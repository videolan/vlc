/*****************************************************************************
 * adjust_sat_hue.c : Hue/Saturation executive part of adjust plugin for vlc
 *****************************************************************************
 * Copyright (C) 2000-2011 VideoLAN
 *
 * Authors: Simon Latapie <garf@via.ecp.fr>
 *          Antoine Cellerier <dionoea -at- videolan d0t org>
 *          Martin Briza <gamajun@seznam.cz> (SSE)
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
#   include "config.h"
#endif

#include <vlc_filter.h>
#include "filter_picture.h"
#include "adjust_sat_hue.h"

#define PLANAR_WRITE_UV_CLIP() \
    i_u = *p_in++ ; i_v = *p_in_v++ ; \
    *p_out++ = clip_uint8_vlc( (( ((i_u * i_cos + i_v * i_sin - i_x) >> 8) \
                           * i_sat) >> 8) + 128); \
    *p_out_v++ = clip_uint8_vlc( (( ((i_v * i_cos - i_u * i_sin - i_y) >> 8) \
                           * i_sat) >> 8) + 128)

#define PLANAR_WRITE_UV() \
    i_u = *p_in++ ; i_v = *p_in_v++ ; \
    *p_out++ = (( ((i_u * i_cos + i_v * i_sin - i_x) >> 8) \
                       * i_sat) >> 8) + 128; \
    *p_out_v++ = (( ((i_v * i_cos - i_u * i_sin - i_y) >> 8) \
                       * i_sat) >> 8) + 128

#define PACKED_WRITE_UV_CLIP() \
    i_u = *p_in; p_in += 4; i_v = *p_in_v; p_in_v += 4; \
    *p_out = clip_uint8_vlc( (( ((i_u * i_cos + i_v * i_sin - i_x) >> 8) \
                           * i_sat) >> 8) + 128); \
    p_out += 4; \
    *p_out_v = clip_uint8_vlc( (( ((i_v * i_cos - i_u * i_sin - i_y) >> 8) \
                           * i_sat) >> 8) + 128); \
    p_out_v += 4

#define PACKED_WRITE_UV() \
    i_u = *p_in; p_in += 4; i_v = *p_in_v; p_in_v += 4; \
    *p_out = (( ((i_u * i_cos + i_v * i_sin - i_x) >> 8) \
                       * i_sat) >> 8) + 128; \
    p_out += 4; \
    *p_out_v = (( ((i_v * i_cos - i_u * i_sin - i_y) >> 8) \
                       * i_sat) >> 8) + 128; \
    p_out_v += 4

#define ADJUST_2_TIMES(x) x; x
#define ADJUST_4_TIMES(x) x; x; x; x
#define ADJUST_8_TIMES(x) x; x; x; x; x; x; x; x

/*****************************************************************************
 * Hue and saturation adjusting routines
 *****************************************************************************/

int planar_sat_hue_clip_C( picture_t * p_pic, picture_t * p_outpic, int i_sin, int i_cos,
                         int i_sat, int i_x, int i_y )
{
    uint8_t *p_in, *p_in_v, *p_in_end, *p_line_end;
    uint8_t *p_out, *p_out_v;

    p_in = p_pic->p[U_PLANE].p_pixels;
    p_in_v = p_pic->p[V_PLANE].p_pixels;
    p_in_end = p_in + p_pic->p[U_PLANE].i_visible_lines
                      * p_pic->p[U_PLANE].i_pitch - 8;

    p_out = p_outpic->p[U_PLANE].p_pixels;
    p_out_v = p_outpic->p[V_PLANE].p_pixels;

    uint8_t i_u, i_v;

    for( ; p_in < p_in_end ; )
    {
        p_line_end = p_in + p_pic->p[U_PLANE].i_visible_pitch - 8;

        for( ; p_in < p_line_end ; )
        {
            /* Do 8 pixels at a time */
            ADJUST_8_TIMES( PLANAR_WRITE_UV_CLIP() );
        }

        p_line_end += 8;

        for( ; p_in < p_line_end ; )
        {
            PLANAR_WRITE_UV_CLIP();
        }

        p_in += p_pic->p[U_PLANE].i_pitch
                - p_pic->p[U_PLANE].i_visible_pitch;
        p_in_v += p_pic->p[V_PLANE].i_pitch
                - p_pic->p[V_PLANE].i_visible_pitch;
        p_out += p_outpic->p[U_PLANE].i_pitch
                - p_outpic->p[U_PLANE].i_visible_pitch;
        p_out_v += p_outpic->p[V_PLANE].i_pitch
                    - p_outpic->p[V_PLANE].i_visible_pitch;
    }

    return VLC_SUCCESS;
}

int planar_sat_hue_C( picture_t * p_pic, picture_t * p_outpic, int i_sin, int i_cos,
                         int i_sat, int i_x, int i_y )
{
    uint8_t *p_in, *p_in_v, *p_in_end, *p_line_end;
    uint8_t *p_out, *p_out_v;

    p_in = p_pic->p[U_PLANE].p_pixels;
    p_in_v = p_pic->p[V_PLANE].p_pixels;
    p_in_end = p_in + p_pic->p[U_PLANE].i_visible_lines
                      * p_pic->p[U_PLANE].i_pitch - 8;

    p_out = p_outpic->p[U_PLANE].p_pixels;
    p_out_v = p_outpic->p[V_PLANE].p_pixels;

    uint8_t i_u, i_v;

    for( ; p_in < p_in_end ; )
    {
        p_line_end = p_in + p_pic->p[U_PLANE].i_visible_pitch - 8;

        for( ; p_in < p_line_end ; )
        {
            /* Do 8 pixels at a time */
            ADJUST_8_TIMES( PLANAR_WRITE_UV() );
        }

        p_line_end += 8;

        for( ; p_in < p_line_end ; )
        {
            PLANAR_WRITE_UV();
        }

        p_in += p_pic->p[U_PLANE].i_pitch
                - p_pic->p[U_PLANE].i_visible_pitch;
        p_in_v += p_pic->p[V_PLANE].i_pitch
                - p_pic->p[V_PLANE].i_visible_pitch;
        p_out += p_outpic->p[U_PLANE].i_pitch
                - p_outpic->p[U_PLANE].i_visible_pitch;
        p_out_v += p_outpic->p[V_PLANE].i_pitch
                    - p_outpic->p[V_PLANE].i_visible_pitch;
    }

    return VLC_SUCCESS;
}

int packed_sat_hue_clip_C( picture_t * p_pic, picture_t * p_outpic, int i_sin, int i_cos,
                         int i_sat, int i_x, int i_y )
{
    uint8_t *p_in, *p_in_v, *p_in_end, *p_line_end;
    uint8_t *p_out, *p_out_v;

    int i_y_offset, i_u_offset, i_v_offset;
    int i_visible_lines, i_pitch, i_visible_pitch;


    if ( GetPackedYuvOffsets( p_pic->format.i_chroma, &i_y_offset,
                              &i_u_offset, &i_v_offset ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    i_visible_lines = p_pic->p->i_visible_lines;
    i_pitch = p_pic->p->i_pitch;
    i_visible_pitch = p_pic->p->i_visible_pitch;

    p_in = p_pic->p->p_pixels + i_u_offset;
    p_in_v = p_pic->p->p_pixels + i_v_offset;
    p_in_end = p_in + i_visible_lines * i_pitch - 8 * 4;

    p_out = p_outpic->p->p_pixels + i_u_offset;
    p_out_v = p_outpic->p->p_pixels + i_v_offset;

    uint8_t i_u, i_v;

    for( ; p_in < p_in_end ; )
    {
            p_line_end = p_in + i_visible_pitch - 8 * 4;

        for( ; p_in < p_line_end ; )
        {
            /* Do 8 pixels at a time */
            ADJUST_8_TIMES( PACKED_WRITE_UV_CLIP() );
        }

        p_line_end += 8 * 4;

        for( ; p_in < p_line_end ; )
        {
            PACKED_WRITE_UV_CLIP();
        }

            p_in += i_pitch - i_visible_pitch;
            p_in_v += i_pitch - i_visible_pitch;
            p_out += i_pitch - i_visible_pitch;
            p_out_v += i_pitch - i_visible_pitch;
    }

    return VLC_SUCCESS;
}

int packed_sat_hue_C( picture_t * p_pic, picture_t * p_outpic, int i_sin,
                      int i_cos, int i_sat, int i_x, int i_y )
{
    uint8_t *p_in, *p_in_v, *p_in_end, *p_line_end;
    uint8_t *p_out, *p_out_v;

    int i_y_offset, i_u_offset, i_v_offset;
    int i_visible_lines, i_pitch, i_visible_pitch;


    if ( GetPackedYuvOffsets( p_pic->format.i_chroma, &i_y_offset,
                              &i_u_offset, &i_v_offset ) != VLC_SUCCESS )
        return VLC_EGENERIC;

    i_visible_lines = p_pic->p->i_visible_lines;
    i_pitch = p_pic->p->i_pitch;
    i_visible_pitch = p_pic->p->i_visible_pitch;

    p_in = p_pic->p->p_pixels + i_u_offset;
    p_in_v = p_pic->p->p_pixels + i_v_offset;
    p_in_end = p_in + i_visible_lines * i_pitch - 8 * 4;

    p_out = p_outpic->p->p_pixels + i_u_offset;
    p_out_v = p_outpic->p->p_pixels + i_v_offset;

    uint8_t i_u, i_v;

    for( ; p_in < p_in_end ; )
    {
            p_line_end = p_in + i_visible_pitch - 8 * 4;

        for( ; p_in < p_line_end ; )
        {
            /* Do 8 pixels at a time */
            ADJUST_8_TIMES( PACKED_WRITE_UV() );
        }

        p_line_end += 8 * 4;

        for( ; p_in < p_line_end ; )
        {
            PACKED_WRITE_UV();
        }

            p_in += i_pitch - i_visible_pitch;
            p_in_v += i_pitch - i_visible_pitch;
            p_out += i_pitch - i_visible_pitch;
            p_out_v += i_pitch - i_visible_pitch;
    }

    return VLC_SUCCESS;
}
