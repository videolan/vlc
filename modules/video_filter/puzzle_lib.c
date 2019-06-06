/*****************************************************************************
 * puzzle_lib.c : Useful functions used by puzzle game filter
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 * Copyright (C) 2013      Vianney Boyer
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
 *          Vianney Boyer <vlcvboyer -at- gmail -dot- com>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include <vlc_rand.h>

#include "filter_picture.h"

#include "puzzle_lib.h"

const char *ppsz_shuffle_button[SHUFFLE_LINES] =
{
"ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo",
"oooooooooooooo  oooooooooooooooooooooooooooo   oooooooo   oooooo  ooooooooooooooo",
"oooooooooooooo  ooooooooooooooooooooooooooo  ooooooooo  oooooooo  ooooooooooooooo",
"oooooooooooooo  ooooooooooooooooooooooooooo  ooooooooo  oooooooo  ooooooooooooooo",
"oo     ooooooo  o    ooooooo  oooo  oooooo     oooooo     oooooo  oooooooo    ooo",
"o  oooo oooooo   ooo  oooooo  oooo  ooooooo  ooooooooo  oooooooo  ooooooo  oo  oo",
"o  ooooooooooo  oooo  oooooo  oooo  ooooooo  ooooooooo  oooooooo  oooooo  oooo  o",
"o      ooooooo  oooo  oooooo  oooo  ooooooo  ooooooooo  oooooooo  oooooo        o",
"oo      oooooo  oooo  oooooo  oooo  ooooooo  ooooooooo  oooooooo  oooooo  ooooooo",
"oooooo  oooooo  oooo  oooooo  oooo  ooooooo  ooooooooo  oooooooo  oooooo  ooooooo",
"o oooo  oooooo  oooo  oooooo  ooo   ooooooo  ooooooooo  oooooooo  ooooooo  oooo o",
"oo     ooooooo  oooo  ooooooo    o  ooooooo  ooooooooo  oooooooo  oooooooo     oo",
"ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
};

const char *ppsz_rot_arrow_sign[ARROW_LINES] =
{
"    .ooo.    ",
"   .o. .oo.  ",
"  .o.    .o. ",
" .o.      .o.",
" o.        .o",
".o          .",
".o   .       ",
" o. .o.      ",
" .o..o.      ",
"  o..o       ",
"   .o.       ",
"ooooo.       ",
"  ..         "
};

const char *ppsz_mir_arrow_sign[ARROW_LINES] =
{
"             ",
"             ",
"    .   .    ",
"  .o.   .o.  ",
" .o.     .o. ",
".o.       .o.",
"ooooooooooooo",
".o.       .o.",
" .o.     .o. ",
"  .o.   .o.  ",
"    .   .    ",
"             ",
"             "
};

/*****************************************************************************
 * fill target image (clean memory)
 *****************************************************************************/
void puzzle_preset_desk_background( picture_t *p_pic_out, uint8_t Y, uint8_t U, uint8_t V)
{
    uint8_t i_c;

    for( uint8_t i_plane = 0; i_plane < p_pic_out->i_planes; i_plane++ ) {
        if (i_plane == Y_PLANE)
            i_c = Y;
        else if (i_plane == U_PLANE)
            i_c = U;
        else if (i_plane == V_PLANE)
            i_c = V;

        const int32_t i_dst_pitch = p_pic_out->p[i_plane].i_pitch;
        const int32_t i_dst_lines = p_pic_out->p[i_plane].i_lines;

        uint8_t *p_dst = p_pic_out->p[i_plane].p_pixels;

        for (int32_t y = 0; y < i_dst_lines; y++)
            memset(&p_dst[y * i_dst_pitch], i_c, i_dst_pitch);
    }
}

/*****************************************************************************
 * draw the borders around the visible desk
 *****************************************************************************/
void puzzle_draw_borders( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    for( uint8_t i_plane = 0; i_plane < p_pic_out->i_planes; i_plane++ ) {
        const int32_t i_in_pitch      = p_sys->ps_pict_planes[i_plane].i_pitch;
        const int32_t i_out_pitch     = p_sys->ps_desk_planes[i_plane].i_pitch;
        const int32_t i_lines         = p_sys->ps_desk_planes[i_plane].i_lines;
        const int32_t i_visible_pitch = p_sys->ps_desk_planes[i_plane].i_visible_pitch;
        const int32_t i_border_pitch  = p_sys->ps_desk_planes[i_plane].i_border_width * p_sys->ps_desk_planes[i_plane].i_pixel_pitch;
        const int32_t i_border_lines  = p_sys->ps_desk_planes[i_plane].i_border_lines;

        uint8_t *p_src = p_pic_in->p[i_plane].p_pixels;
        uint8_t *p_dst = p_pic_out->p[i_plane].p_pixels;

        for (int32_t y = 0 ; y < i_border_lines; y++)
            memcpy( &p_dst[y * i_out_pitch], &p_src[y * i_in_pitch], i_visible_pitch);

        for (int32_t y = i_lines - i_border_lines ; y < i_lines; y++)
            memcpy( &p_dst[y * i_out_pitch], &p_src[y * i_in_pitch], i_visible_pitch);

        for (int32_t y = i_border_lines ; y < i_lines - i_border_lines; y++) {
            memcpy( &p_dst[y * i_out_pitch], &p_src[y * i_in_pitch], i_border_pitch);
            memcpy( &p_dst[y * i_out_pitch + i_visible_pitch - i_border_pitch], &p_src[y * i_in_pitch + i_visible_pitch - i_border_pitch], i_border_pitch);
        }
    }
}

/*****************************************************************************
 * draw preview in a corner of the desk
 *****************************************************************************/
void puzzle_draw_preview( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    for( uint8_t  i_plane = 0; i_plane < p_pic_out->i_planes; i_plane++ ) {
        int32_t i_preview_offset = 0;
        int32_t i_preview_width  = p_sys->ps_desk_planes[i_plane].i_width * p_sys->s_current_param.i_preview_size / 100;
        int32_t i_preview_lines  = p_pic_out->p[i_plane].i_visible_lines * p_sys->s_current_param.i_preview_size / 100;
        int32_t i_pixel_pitch    = p_pic_out->p[i_plane].i_pixel_pitch;

        const int32_t i_src_pitch  = p_pic_in->p[i_plane].i_pitch;
        const int32_t i_dst_pitch  = p_pic_out->p[i_plane].i_pitch;

        uint8_t *p_src = p_pic_in->p[i_plane].p_pixels;
        uint8_t *p_dst = p_pic_out->p[i_plane].p_pixels;

        switch ( p_sys->i_preview_pos ) {
        case 0:
            i_preview_offset = 0;
            break;
        case 1:
            i_preview_offset =
                (p_sys->ps_desk_planes[i_plane].i_width - 1 - i_preview_width) * i_pixel_pitch;
            break;
        case 2:
            i_preview_offset =
                (p_sys->ps_desk_planes[i_plane].i_width - 1 - i_preview_width) * i_pixel_pitch
                + ((int32_t) ( p_sys->ps_desk_planes[i_plane].i_lines - 1 - i_preview_lines )) * i_dst_pitch;
            break;
        case 3:
            i_preview_offset = ((int32_t) ( p_sys->ps_desk_planes[i_plane].i_lines - 1 - i_preview_lines )) * i_dst_pitch;
            break;
        default:
            i_preview_offset = 0;
            break;
        }

        for ( int32_t y = 0; y < i_preview_lines; y++ )
            for ( int32_t x = 0; x < i_preview_width; x++ )
                memcpy( &p_dst[ y * i_dst_pitch + x * i_pixel_pitch + i_preview_offset ],
                        &p_src[ ( y * 100 / p_sys->s_current_param.i_preview_size ) * i_src_pitch
                                + ( x * 100 / p_sys->s_current_param.i_preview_size ) * i_pixel_pitch ],
                        i_pixel_pitch );
    }
}

/*****************************************************************************
 * draw sign/icon/symbol in the output picture
 *****************************************************************************/
void puzzle_draw_sign(picture_t *p_pic_out, int32_t i_x, int32_t i_y, int32_t i_width, int32_t i_lines, const char **ppsz_sign, bool b_reverse)
{
    plane_t *p_out = &p_pic_out->p[Y_PLANE];
    int32_t i_pixel_pitch    = p_pic_out->p[Y_PLANE].i_pixel_pitch;

    uint8_t i_Y;

    i_Y = ( p_out->p_pixels[ i_y * p_out->i_pitch + i_x ] >= 0x7F ) ? 0x00 : 0xFF;

    for( int32_t y = 0; y < i_lines ; y++ )
        for( int32_t x = 0; x < i_width; x++ ) {
            int32_t i_dst_x = ( x + i_x ) * i_pixel_pitch;
            int32_t i_dst_y = y + i_y;
            if ( ppsz_sign[y][b_reverse?i_width-1-x:x] == 'o' ) {
                if ((i_dst_x < p_out->i_visible_pitch) && (i_dst_y < p_out->i_visible_lines) && (i_dst_x >= 0 ) && (i_dst_y >= 0))
                    memset( &p_out->p_pixels[ i_dst_y * p_out->i_pitch + i_dst_x ],   i_Y,  p_out->i_pixel_pitch );
            }
            else if ( ppsz_sign[y][b_reverse?i_width-1-x:x] == '.' ) {
                if ((i_dst_x < p_out->i_visible_pitch) && (i_dst_y < p_out->i_visible_lines) && (i_dst_x >= 0 ) && (i_dst_y >= 0))
                    p_out->p_pixels[ i_dst_y * p_out->i_pitch + i_dst_x ] = p_out->p_pixels[ i_dst_y * p_out->i_pitch + i_dst_x ] / 2 + i_Y / 2;
            }
        }
}

/*****************************************************************************
 * draw outline rectangle in output picture
 *****************************************************************************/
void puzzle_draw_rectangle(picture_t *p_pic_out, int32_t i_x, int32_t i_y, int32_t i_w, int32_t i_h, uint8_t i_Y, uint8_t i_U, uint8_t i_V )
{
    uint8_t i_c;

    for( uint8_t i_plane = 0; i_plane < p_pic_out->i_planes; i_plane++ ) {
        plane_t *p_oyp = &p_pic_out->p[i_plane];
        int32_t i_pixel_pitch    = p_pic_out->p[i_plane].i_pixel_pitch;

        if (i_plane == Y_PLANE)
            i_c = i_Y;
        else if (i_plane == U_PLANE)
            i_c = i_U;
        else if (i_plane == V_PLANE)
            i_c = i_V;

        int32_t i_x_min = (      i_x    * p_oyp->i_visible_pitch / p_pic_out->p[0].i_visible_pitch ) * i_pixel_pitch;
        int32_t i_x_max = ( (i_x + i_w) * p_oyp->i_visible_pitch / p_pic_out->p[0].i_visible_pitch ) * i_pixel_pitch;
        int32_t i_y_min =        i_y    * p_oyp->i_visible_lines / p_pic_out->p[0].i_visible_lines;
        int32_t i_y_max =   (i_y + i_h) * p_oyp->i_visible_lines / p_pic_out->p[0].i_visible_lines;

        /* top line */
        memset( &p_oyp->p_pixels[i_y_min * p_oyp->i_pitch + i_x_min], i_c,  i_x_max - i_x_min);

        /* left and right */
        for( int32_t i_dy = 1; i_dy < i_y_max - i_y_min - 1; i_dy++ ) {
            memset( &p_oyp->p_pixels[ (i_y_min + i_dy) * p_oyp->i_pitch + i_x_min ],   i_c,  p_oyp->i_pixel_pitch );
            memset( &p_oyp->p_pixels[(i_y_min + i_dy) * p_oyp->i_pitch + i_x_max - 1], i_c,  p_oyp->i_pixel_pitch );
        }

        /* bottom line */
        memset( &p_oyp->p_pixels[(i_y_max - 1) * p_oyp->i_pitch + i_x_min], i_c,  i_x_max - i_x_min);
    }
}

/*****************************************************************************
 * draw bold rectangle in output picture
 *****************************************************************************/
void puzzle_fill_rectangle(picture_t *p_pic_out, int32_t i_x, int32_t i_y, int32_t i_w, int32_t i_h, uint8_t i_Y, uint8_t i_U, uint8_t i_V )
{
    uint8_t i_c;

    for( uint8_t i_plane = 0; i_plane < p_pic_out->i_planes; i_plane++ ) {
        plane_t *p_oyp = &p_pic_out->p[i_plane];
        int32_t i_pixel_pitch    = p_pic_out->p[i_plane].i_pixel_pitch;

        if (i_plane == Y_PLANE)
            i_c = i_Y;
        else if (i_plane == U_PLANE)
            i_c = i_U;
        else if (i_plane == V_PLANE)
            i_c = i_V;

        int32_t i_x_min = (     i_x     * p_oyp->i_visible_pitch / p_pic_out->p[0].i_visible_pitch ) * i_pixel_pitch;
        int32_t i_x_max = ( (i_x + i_w) * p_oyp->i_visible_pitch / p_pic_out->p[0].i_visible_pitch ) * i_pixel_pitch;
        int32_t i_y_min =       i_y     * p_oyp->i_visible_lines / p_pic_out->p[0].i_visible_lines;
        int32_t i_y_max =   (i_y + i_h) * p_oyp->i_visible_lines / p_pic_out->p[0].i_visible_lines;

        for( int32_t i_dy = 0; i_dy < i_y_max - i_y_min; i_dy++ )
            memset( &p_oyp->p_pixels[(i_y_min + i_dy) * p_oyp->i_pitch + i_x_min], i_c,  i_x_max - i_x_min);
    }
}
