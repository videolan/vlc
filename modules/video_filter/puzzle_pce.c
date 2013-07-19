/*****************************************************************************
 * puzzle_pce.c : Puzzle game filter - pieces functions
 *****************************************************************************
 * Copyright (C) 2013 Vianney Boyer
 * $Id$
 *
 * Author:  Vianney Boyer <vlcvboyer -at- gmail -dot- com>
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
#include <vlc_rand.h>

#include "filter_picture.h"

#include "puzzle_bezier.h"
#include "puzzle_lib.h"
#include "puzzle_pce.h"

#define SHAPES_QTY 20
#define PIECE_TYPE_NBR (4*2*(1+SHAPES_QTY))

/*****************************************************************************
 * puzzle_bake_pieces_shapes: allocate and compute shapes
 *****************************************************************************/
int puzzle_bake_pieces_shapes( filter_t *p_filter)
{
/* note:
 *   piece_shape_t **ps_pieces_shapes;  * array [each piece type (PCE_TYPE_NBR  * 4 ( * negative ): top, left,right,btm)][each plane] of piece definition
 *   0 => left border
 *   1 => left border (negative, never used)
 *   2 => top border
 *   .....
 *   8 => bezier left
 *   9 => bezier left negative
 *  10 => bezier top
 *  11 => bezier top negative
 *  12 => bezier btm
 *  13 => bezier btm negative
 *  14 => bezier right
 *  15 => bezier right negative
 *  .....
 */

    filter_sys_t *p_sys = p_filter->p_sys;

    puzzle_free_ps_pieces_shapes(p_filter);
    p_sys->ps_pieces_shapes = malloc( sizeof( piece_shape_t *) * PIECE_TYPE_NBR );
    if( !p_sys->ps_pieces_shapes )
        return VLC_ENOMEM;

    for (int32_t i_piece = 0; i_piece < PIECE_TYPE_NBR; i_piece++) {
        p_sys->ps_pieces_shapes[i_piece] = malloc( sizeof( piece_shape_t) * p_sys->s_allocated.i_planes );
        if( !p_sys->ps_pieces_shapes[i_piece] )
            return VLC_ENOMEM;
        for (uint8_t i_plane = 0; i_plane < p_filter->p_sys->s_allocated.i_planes; i_plane++) {
            p_sys->ps_pieces_shapes[i_piece][i_plane].i_row_nbr = 0;
            p_sys->ps_pieces_shapes[i_piece][i_plane].ps_piece_shape_row = NULL;
        }
    }

    int32_t i_currect_shape = 0;

    for (uint8_t i_plane = 0; i_plane < p_filter->p_sys->s_allocated.i_planes; i_plane++) {
        int i_ret;
        i_ret = puzzle_generate_sect_border( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+0][i_plane], i_plane, puzzle_SHAPE_LEFT);
        if (i_ret != VLC_SUCCESS) return i_ret;
        i_ret = puzzle_generate_sect_border( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+1][i_plane], i_plane, puzzle_SHAPE_LEFT);
        if (i_ret != VLC_SUCCESS) return i_ret;
        i_ret = puzzle_generate_sect_border( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+2][i_plane], i_plane, puzzle_SHAPE_TOP);
        if (i_ret != VLC_SUCCESS) return i_ret;
        i_ret = puzzle_generate_sect_border( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+3][i_plane], i_plane, puzzle_SHAPE_TOP);
        if (i_ret != VLC_SUCCESS) return i_ret;
        i_ret = puzzle_generate_sect_border( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+4][i_plane], i_plane, puzzle_SHAPE_BTM);
        if (i_ret != VLC_SUCCESS) return i_ret;
        i_ret = puzzle_generate_sect_border( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+5][i_plane], i_plane, puzzle_SHAPE_BTM);
        if (i_ret != VLC_SUCCESS) return i_ret;
        i_ret = puzzle_generate_sect_border( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+6][i_plane], i_plane, puzzle_SHAPE_RIGHT);
        if (i_ret != VLC_SUCCESS) return i_ret;
        i_ret = puzzle_generate_sect_border( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+7][i_plane], i_plane, puzzle_SHAPE_RIGHT);
        if (i_ret != VLC_SUCCESS) return i_ret;
    }

    i_currect_shape += 8;

    int32_t i_width = p_sys->ps_desk_planes[0].i_pce_max_width;
    int32_t i_lines = p_sys->ps_desk_planes[0].i_pce_max_lines;

    for (int32_t i_shape = 0; i_shape<SHAPES_QTY; i_shape++) {

        point_t *ps_scale_pts_H = puzzle_scale_curve_H(i_width, i_lines,     7, p_sys->ps_bezier_pts_H[i_shape], p_sys->s_allocated.i_shape_size);
        point_t *ps_scale_pts_V = puzzle_H_2_scale_curve_V(i_width, i_lines, 7, p_sys->ps_bezier_pts_H[i_shape], p_sys->s_allocated.i_shape_size);
        point_t *ps_neg_pts_H =   puzzle_curve_H_2_negative(7, ps_scale_pts_H);
        point_t *ps_neg_pts_V =   puzzle_curve_V_2_negative(7, ps_scale_pts_V);

        if (!ps_scale_pts_H || !ps_scale_pts_V || !ps_neg_pts_H || !ps_neg_pts_V) {
            free(ps_scale_pts_H);
            free(ps_scale_pts_V);
            free(ps_neg_pts_H);
            free(ps_neg_pts_V);
            return VLC_EGENERIC;
        }

        int i_ret;
        for (uint8_t i_plane = 0; i_plane < p_filter->p_sys->s_allocated.i_planes; i_plane++) {
            i_ret = puzzle_generate_sect_bezier( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape][i_plane],   7, ps_scale_pts_V, i_plane, puzzle_SHAPE_LEFT);
            if (i_ret != VLC_SUCCESS) break;
            i_ret = puzzle_generate_sect_bezier( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+1][i_plane], 7, ps_neg_pts_V,   i_plane, puzzle_SHAPE_LEFT);
            if (i_ret != VLC_SUCCESS) break;
            i_ret = puzzle_generate_sect_bezier(  p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+2][i_plane], 7, ps_scale_pts_H, i_plane, puzzle_SHAPE_TOP);
            if (i_ret != VLC_SUCCESS) break;
            i_ret = puzzle_generate_sect_bezier(  p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+3][i_plane], 7, ps_neg_pts_H,   i_plane, puzzle_SHAPE_TOP);
            if (i_ret != VLC_SUCCESS) break;

            i_ret = puzzle_generate_sectTop2Btm( p_filter,    &p_sys->ps_pieces_shapes[i_currect_shape+4][i_plane], &p_sys->ps_pieces_shapes[i_currect_shape+2][i_plane], i_plane);
            if (i_ret != VLC_SUCCESS) break;
            i_ret = puzzle_generate_sectTop2Btm( p_filter,    &p_sys->ps_pieces_shapes[i_currect_shape+5][i_plane], &p_sys->ps_pieces_shapes[i_currect_shape+3][i_plane], i_plane);
            if (i_ret != VLC_SUCCESS) break;
            i_ret = puzzle_generate_sectLeft2Right( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+6][i_plane], &p_sys->ps_pieces_shapes[i_currect_shape][i_plane],   i_plane);
            if (i_ret != VLC_SUCCESS) break;
            i_ret = puzzle_generate_sectLeft2Right( p_filter, &p_sys->ps_pieces_shapes[i_currect_shape+7][i_plane], &p_sys->ps_pieces_shapes[i_currect_shape+1][i_plane], i_plane);
            if (i_ret != VLC_SUCCESS) break;
        }

        free(ps_scale_pts_H);
        free(ps_scale_pts_V);
        free(ps_neg_pts_H);
        free(ps_neg_pts_V);

        if (i_ret != VLC_SUCCESS) return i_ret;

        i_currect_shape += 8;
    }

    p_sys->b_shape_init = true;

    return VLC_SUCCESS;
}

/* free allocated shapes data */
void puzzle_free_ps_pieces_shapes( filter_t *p_filter)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if (p_sys->ps_pieces_shapes == NULL)
        return;

    for (int32_t p = 0; p < p_sys->s_allocated.i_piece_types; p++) {
        for (uint8_t i_plane = 0; i_plane < p_sys->s_allocated.i_planes; i_plane++) {
            for (int32_t r = 0; r < p_sys->ps_pieces_shapes[p][i_plane].i_row_nbr; r++)
                free( p_sys->ps_pieces_shapes[p][i_plane].ps_piece_shape_row[r].ps_row_section );
            free( p_sys->ps_pieces_shapes[p][i_plane].ps_piece_shape_row );
        }
        free( p_sys->ps_pieces_shapes[p] );
    }
    free( p_sys->ps_pieces_shapes );
    p_sys->ps_pieces_shapes = NULL;
}

/*****************************************************************************
 * puzzle_find_piece: use piece corners to find the piece selected
 *                    by mouse cursor
 *****************************************************************************/
int puzzle_find_piece( filter_t *p_filter, int32_t i_x, int32_t i_y, int32_t i_except) {
    filter_sys_t *p_sys = p_filter->p_sys;

    for (uint32_t i = 0; i < p_sys->s_allocated.i_pieces_nbr; i++) {
        piece_t *ps_current_piece = &p_sys->ps_pieces[i];
        if (( ps_current_piece->i_min_x <= i_x ) &&
            ( ps_current_piece->i_max_x >= i_x ) &&
            ( ps_current_piece->i_min_y <= i_y ) &&
            ( ps_current_piece->i_max_y  >= i_y ) &&
            ( (int32_t)i != i_except ) )
        {
            return i;
        }
    }
    return -1;
}

/*****************************************************************************
 * puzzle_calculate_corners: calculate corners location & regen geometry data
 *****************************************************************************/
void puzzle_calculate_corners( filter_t *p_filter,  int32_t i_piece )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    piece_t *ps_piece = &p_sys->ps_pieces[i_piece];

    switch ( ps_piece->i_actual_angle)
    {
      case 0:
        ps_piece->i_step_x_x = ps_piece->i_actual_mirror;
        ps_piece->i_step_x_y = 0;
        ps_piece->i_step_y_y = 1;
        ps_piece->i_step_y_x = 0;
        break;
      case 1:
        ps_piece->i_step_x_x = 0;
        ps_piece->i_step_x_y = -ps_piece->i_actual_mirror; /* x offset on original pict creates negative y offset on desk */
        ps_piece->i_step_y_y = 0;
        ps_piece->i_step_y_x = 1;
        break;
      case 2:
        ps_piece->i_step_x_x = -ps_piece->i_actual_mirror;
        ps_piece->i_step_x_y = 0;
        ps_piece->i_step_y_y = -1;
        ps_piece->i_step_y_x = 0;
        break;
      case 3:
        ps_piece->i_step_x_x = 0;
        ps_piece->i_step_x_y = ps_piece->i_actual_mirror;
        ps_piece->i_step_y_y = 0;
        ps_piece->i_step_y_x = -1;
        break;
    }

    /* regen geometry */
    for (uint8_t i_plane = 1; i_plane < p_sys->s_allocated.i_planes; i_plane++) {
        ps_piece->ps_piece_in_plane[i_plane].i_actual_x =
            ps_piece->ps_piece_in_plane[0].i_actual_x * p_sys->ps_desk_planes[i_plane].i_width / p_sys->ps_desk_planes[0].i_width;
        ps_piece->ps_piece_in_plane[i_plane].i_actual_y =
            ps_piece->ps_piece_in_plane[0].i_actual_y * p_sys->ps_desk_planes[i_plane].i_lines / p_sys->ps_desk_planes[0].i_lines;
    }

    /* regen location of grabed piece's corners */
    int32_t i_width = ps_piece->ps_piece_in_plane[0].i_width;
    int32_t i_lines = ps_piece->ps_piece_in_plane[0].i_lines;

    ps_piece->i_TLx = ps_piece->ps_piece_in_plane[0].i_actual_x;
    ps_piece->i_TLy = ps_piece->ps_piece_in_plane[0].i_actual_y;
    ps_piece->i_TRx = ps_piece->i_TLx + ( i_width - 1 ) * ps_piece->i_step_x_x;
    ps_piece->i_TRy = ps_piece->i_TLy + ( i_width - 1 ) * ps_piece->i_step_x_y;
    ps_piece->i_BRx = ps_piece->i_TLx + ( i_width - 1 ) * ps_piece->i_step_x_x + ( i_lines - 1 ) * ps_piece->i_step_y_x;
    ps_piece->i_BRy = ps_piece->i_TLy + ( i_width - 1 ) * ps_piece->i_step_x_y + ( i_lines - 1 ) * ps_piece->i_step_y_y;
    ps_piece->i_BLx = ps_piece->i_TLx + ( i_lines - 1 ) * ps_piece->i_step_y_x;
    ps_piece->i_BLy = ps_piece->i_TLy + ( i_lines - 1 ) * ps_piece->i_step_y_y;

    ps_piece->i_max_x = __MAX( __MAX( ps_piece->i_TLx, ps_piece->i_TRx ), __MAX( ps_piece->i_BLx, ps_piece->i_BRx ) );
    ps_piece->i_min_x = __MIN( __MIN( ps_piece->i_TLx, ps_piece->i_TRx ), __MIN( ps_piece->i_BLx, ps_piece->i_BRx ) );
    ps_piece->i_max_y = __MAX( __MAX( ps_piece->i_TLy, ps_piece->i_TRy ), __MAX( ps_piece->i_BLy, ps_piece->i_BRy ) );
    ps_piece->i_min_y = __MIN( __MIN( ps_piece->i_TLy, ps_piece->i_TRy ), __MIN( ps_piece->i_BLy, ps_piece->i_BRy ) );

    ps_piece->i_center_x = ( ps_piece->i_max_x + ps_piece->i_min_x ) / 2;
    ps_piece->i_center_y = ( ps_piece->i_max_y + ps_piece->i_min_y ) / 2;

    int32_t pce_overlap = puzzle_find_piece( p_filter, ps_piece->i_center_x, ps_piece->i_center_y, i_piece);

    if ( ( pce_overlap != NO_PCE ) && ( p_sys->pi_group_qty[ps_piece->i_group_ID] == 1 ) )
        ps_piece->b_overlap = true;
}

/*****************************************************************************
 * rotate piece when user click on mouse
 *****************************************************************************/
void puzzle_rotate_pce( filter_t *p_filter, int32_t i_piece, int8_t i_rotate_mirror, int32_t i_center_x, int32_t i_center_y, bool b_avoid_mirror )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    piece_t *ps_piece = &p_sys->ps_pieces[i_piece];

    if ( p_sys->s_current_param.i_rotate == 0 )
        return;

    if ( p_sys->s_current_param.i_rotate == 1 && (i_rotate_mirror != 2) )
        return;

    for ( uint8_t i=0; i < abs( i_rotate_mirror ); i++) {
        int32_t i_tempx, i_tempy;

        /* piece has to be rotated by 90° */
        if ( i_rotate_mirror > 0 ) {
            ps_piece->i_actual_angle++;
            ps_piece->i_actual_angle &= 0x03;

            i_tempx = -( i_center_y - ps_piece->ps_piece_in_plane[0].i_actual_y ) + i_center_x;
            i_tempy = +( i_center_x - ps_piece->ps_piece_in_plane[0].i_actual_x ) + i_center_y;
        }
        else {
            ps_piece->i_actual_angle--;
            ps_piece->i_actual_angle &= 0x03;

            i_tempx = +( i_center_y - ps_piece->ps_piece_in_plane[0].i_actual_y ) + i_center_x;
            i_tempy = -( i_center_x - ps_piece->ps_piece_in_plane[0].i_actual_x ) + i_center_y;
        }

        ps_piece->ps_piece_in_plane[0].i_actual_x = i_tempx;
        ps_piece->ps_piece_in_plane[0].i_actual_y = i_tempy;

        if ( ps_piece->i_actual_angle == 0 && p_sys->s_current_param.i_rotate == 3 && !b_avoid_mirror ) {
            ps_piece->ps_piece_in_plane[0].i_actual_x = 2 * i_center_x - ps_piece->ps_piece_in_plane[0].i_actual_x;
            ps_piece->i_actual_mirror *= -1;
        }
        puzzle_calculate_corners( p_filter, i_piece );
    }
}

/*****************************************************************************
 * move group of joined pieces when user drag'n drop it with mouse
 *****************************************************************************/
void puzzle_move_group( filter_t *p_filter, int32_t i_piece, int32_t i_dx, int32_t i_dy)
{
    filter_sys_t *p_sys = p_filter->p_sys;
    uint32_t i_group_ID = p_sys->ps_pieces[i_piece].i_group_ID;
    for (uint32_t i = 0; i < p_sys->s_allocated.i_pieces_nbr; i++) {
        piece_t *ps_piece = &p_sys->ps_pieces[i];
        if (ps_piece->i_group_ID == i_group_ID) {
            ps_piece->b_finished = false;
            ps_piece->ps_piece_in_plane[0].i_actual_x += i_dx;
            ps_piece->ps_piece_in_plane[0].i_actual_y += i_dy;

            puzzle_calculate_corners( p_filter, i );
        }
    }
}

/*****************************************************************************
 * draw straight rectangular piece in the specified plane
 *****************************************************************************/
void puzzle_drw_basic_pce_in_plane( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out, uint8_t i_plane, piece_t *ps_piece)
{
    /* basic version rectangular & angle = 0 */
    filter_sys_t *p_sys = p_filter->p_sys;

    if ((p_sys->ps_puzzle_array == NULL) || (p_sys->ps_pieces == NULL) || (ps_piece == NULL))
        return;

    const int32_t i_src_pitch    = p_pic_in->p[i_plane].i_pitch;
    const int32_t i_dst_pitch    = p_pic_out->p[i_plane].i_pitch;
    const int32_t i_src_width    = p_pic_in->p[i_plane].i_pitch / p_pic_in->p[i_plane].i_pixel_pitch;
    const int32_t i_dst_width    = p_pic_out->p[i_plane].i_pitch / p_pic_out->p[i_plane].i_pixel_pitch;
    const int32_t i_pixel_pitch  = p_pic_out->p[i_plane].i_pixel_pitch;
    const int32_t i_src_visible_lines    = p_pic_in->p[i_plane].i_visible_lines;
    const int32_t i_dst_visible_lines    = p_pic_out->p[i_plane].i_visible_lines;
    uint8_t *p_src = p_pic_in->p[i_plane].p_pixels;
    uint8_t *p_dst = p_pic_out->p[i_plane].p_pixels;

    const int32_t i_desk_start_x = ps_piece->ps_piece_in_plane[i_plane].i_actual_x;
    const int32_t i_desk_start_y = ps_piece->ps_piece_in_plane[i_plane].i_actual_y;
    const int32_t i_pic_start_x = ps_piece->ps_piece_in_plane[i_plane].i_original_x;
    const int32_t i_pic_start_y = ps_piece->ps_piece_in_plane[i_plane].i_original_y;
    const int32_t i_width = ps_piece->ps_piece_in_plane[i_plane].i_width;
    const int32_t i_lines = ps_piece->ps_piece_in_plane[i_plane].i_lines;

    const int32_t i_ofs_x   =           __MAX(0, __MAX(-i_desk_start_x,-i_pic_start_x));
    const int32_t i_count_x = i_width - __MAX(0, __MAX(i_desk_start_x + i_width - i_dst_width, i_pic_start_x + i_width - i_src_width ));
    const int32_t i_ofs_y   =           __MAX(0, __MAX(-i_desk_start_y,-i_pic_start_y));
    const int32_t i_count_y = i_lines - __MAX(0, __MAX(i_desk_start_y + i_lines - i_dst_visible_lines, i_pic_start_y + i_lines - i_src_visible_lines ));

    for (int32_t i_y = i_ofs_y; i_y < i_count_y; i_y++) {
        memcpy( p_dst + (i_desk_start_y + i_y) * i_dst_pitch + ( i_desk_start_x + i_ofs_x ) * i_pixel_pitch,
            p_src + (i_pic_start_y + i_y) * i_src_pitch + ( i_pic_start_x + i_ofs_x ) * i_pixel_pitch,
            ( i_count_x - i_ofs_x ) * i_pixel_pitch );
    }

    return;
}

/*****************************************************************************
 * draw oriented rectangular piece in the specified plane
 *****************************************************************************/
void puzzle_drw_adv_pce_in_plane( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out, uint8_t i_plane, piece_t *ps_piece)
{
    /* here we still have rectangular shape but angle is not 0 */
    filter_sys_t *p_sys = p_filter->p_sys;

    if ((p_sys->ps_puzzle_array == NULL) || (p_sys->ps_pieces == NULL) || (ps_piece == NULL))
        return;

    const int32_t i_src_pitch    = p_pic_in->p[i_plane].i_pitch;
    const int32_t i_dst_pitch    = p_pic_out->p[i_plane].i_pitch;
    const int32_t i_src_width    = p_pic_in->p[i_plane].i_pitch / p_pic_in->p[i_plane].i_pixel_pitch;
    const int32_t i_dst_width    = p_pic_out->p[i_plane].i_pitch / p_pic_out->p[i_plane].i_pixel_pitch;
    const int32_t i_pixel_pitch  = p_pic_out->p[i_plane].i_pixel_pitch;
    const int32_t i_src_visible_lines    = p_pic_in->p[i_plane].i_visible_lines;
    const int32_t i_dst_visible_lines    = p_pic_out->p[i_plane].i_visible_lines;
    uint8_t *p_src = p_pic_in->p[i_plane].p_pixels;
    uint8_t *p_dst = p_pic_out->p[i_plane].p_pixels;

    const int32_t i_desk_start_x = ps_piece->ps_piece_in_plane[i_plane].i_actual_x;
    const int32_t i_desk_start_y = ps_piece->ps_piece_in_plane[i_plane].i_actual_y;
    const int32_t i_pic_start_x = ps_piece->ps_piece_in_plane[i_plane].i_original_x;
    const int32_t i_pic_start_y = ps_piece->ps_piece_in_plane[i_plane].i_original_y;
    const int32_t i_width = ps_piece->ps_piece_in_plane[i_plane].i_width;
    const int32_t i_lines = ps_piece->ps_piece_in_plane[i_plane].i_lines;

    for (int32_t i_y = 0; i_y < i_lines; i_y++) {
        int32_t i_current_src_y = i_pic_start_y + i_y;

        if ( ( i_current_src_y >= 0 ) && ( i_current_src_y < i_src_visible_lines ) ) {
            for (int32_t i_x = 0; i_x < i_width; i_x++) {
                int32_t i_current_dst_x = i_desk_start_x + i_x * ps_piece->i_step_x_x + i_y * ps_piece->i_step_y_x;
                int32_t i_current_dst_y = i_desk_start_y + i_x * ps_piece->i_step_x_y + i_y * ps_piece->i_step_y_y;
                int32_t i_current_src_x = i_pic_start_x + i_x;

                if ( ( i_current_dst_x  >= 0 ) && ( i_current_src_x >= 0 )
                        && ( i_current_dst_x  < i_dst_width ) && ( i_current_src_x < i_src_width )
                        && ( i_current_dst_y >= 0 ) && ( i_current_dst_y < i_dst_visible_lines ) )
                {
                    memcpy( p_dst + i_current_dst_y * i_dst_pitch + i_current_dst_x * i_pixel_pitch,
                            p_src + i_current_src_y * i_src_pitch + i_current_src_x * i_pixel_pitch,
                           i_pixel_pitch );
                }
            }
        }
    }

    return;
}

/*****************************************************************************
 * draw complex shape in the specified plane
 *****************************************************************************/
void puzzle_drw_complex_pce_in_plane( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out, uint8_t i_plane, piece_t *ps_piece, uint32_t i_pce)
{
    /* "puzzle" shape and maybe angle != 0 */
    filter_sys_t *p_sys = p_filter->p_sys;

    if ((p_sys->ps_puzzle_array == NULL) || (p_sys->ps_pieces == NULL) || (ps_piece == NULL))
        return;

    const int32_t i_src_pitch    = p_pic_in->p[i_plane].i_pitch;
    const int32_t i_dst_pitch    = p_pic_out->p[i_plane].i_pitch;
    const int32_t i_src_width    = p_pic_in->p[i_plane].i_pitch / p_pic_in->p[i_plane].i_pixel_pitch;
    const int32_t i_dst_width    = p_pic_out->p[i_plane].i_pitch / p_pic_out->p[i_plane].i_pixel_pitch;
    const int32_t i_pixel_pitch  = p_pic_out->p[i_plane].i_pixel_pitch;
    const int32_t i_src_visible_lines    = p_pic_in->p[i_plane].i_visible_lines;
    const int32_t i_dst_visible_lines    = p_pic_out->p[i_plane].i_visible_lines;
    uint8_t *p_src = p_pic_in->p[i_plane].p_pixels;
    uint8_t *p_dst = p_pic_out->p[i_plane].p_pixels;

    const int32_t i_desk_start_x = ps_piece->ps_piece_in_plane[i_plane].i_actual_x;
    const int32_t i_desk_start_y = ps_piece->ps_piece_in_plane[i_plane].i_actual_y;
    const int32_t i_pic_start_x = ps_piece->ps_piece_in_plane[i_plane].i_original_x;
    const int32_t i_pic_start_y = ps_piece->ps_piece_in_plane[i_plane].i_original_y;

    piece_shape_t *ps_top_shape =   &p_sys->ps_pieces_shapes[ps_piece->i_top_shape][i_plane];
    piece_shape_t *ps_btm_shape =   &p_sys->ps_pieces_shapes[ps_piece->i_btm_shape][i_plane];
    piece_shape_t *ps_right_shape = &p_sys->ps_pieces_shapes[ps_piece->i_right_shape][i_plane];
    piece_shape_t *ps_left_shape =  &p_sys->ps_pieces_shapes[ps_piece->i_left_shape][i_plane];
    piece_shape_t *ps_shape;

    int32_t i_min_y = ps_top_shape->i_first_row_offset;
    int32_t i_max_y = ps_btm_shape->i_first_row_offset + ps_btm_shape->i_row_nbr - 1;

    for (int32_t i_y = i_min_y; i_y <= i_max_y; i_y++) {
        int32_t i_current_src_y = i_pic_start_y + i_y;

        if ( ( i_current_src_y >= 0 ) && ( i_current_src_y < i_src_visible_lines ) ) {
            int32_t i_sect_start_x = 0;

            /* process each sub shape (each quarter) */
            for (int8_t i_shape=0; i_shape < 4; i_shape++) {
                switch ( i_shape )
                {
                  case 0:
                    ps_shape = ps_left_shape;
                    break;
                  case 1:
                    ps_shape = ps_top_shape;
                    break;
                  case 2:
                    ps_shape = ps_btm_shape;
                    break;
                  case 3:
                    ps_shape = ps_right_shape;
                    break;
                }

                int32_t i_r = i_y - ps_shape->i_first_row_offset;

                if (i_r <0 || i_r >= ps_shape->i_row_nbr)
                    continue;

                piece_shape_row_t *ps_piece_shape_row = &ps_shape->ps_piece_shape_row[i_r];

                for (int32_t i_s = 0; i_s < ps_piece_shape_row->i_section_nbr; i_s++) {
                    uint8_t i_type = ps_piece_shape_row->ps_row_section[i_s].i_type;
                    int32_t i_width = ps_piece_shape_row->ps_row_section[i_s].i_width;
                    if (i_type == 0) {
                        /* copy pixel line from input image to puzzle desk */
                        for (int32_t i_x = 0; i_x < i_width; i_x++) {
                            int32_t i_current_dst_x = i_desk_start_x + (i_sect_start_x + i_x) * ps_piece->i_step_x_x + i_y * ps_piece->i_step_y_x;
                            int32_t i_current_dst_y = i_desk_start_y + (i_sect_start_x + i_x) * ps_piece->i_step_x_y + i_y * ps_piece->i_step_y_y;
                            int32_t i_current_src_x = i_pic_start_x + (i_sect_start_x + i_x);

                            if (    i_current_dst_x < 0 || i_current_dst_x >= i_dst_width
                                 || i_current_src_x < 0 || i_current_src_x >= i_src_width
                                 || i_current_dst_y < 0 || i_current_dst_y >= i_dst_visible_lines )
                                continue;

                            memcpy( p_dst + i_current_dst_y * i_dst_pitch + i_current_dst_x * i_pixel_pitch,
                                    p_src + i_current_src_y * i_src_pitch + i_current_src_x * i_pixel_pitch,
                                    i_pixel_pitch );

                            /* Check if mouse pointer is over this pixel
                             * Yes: set i_pointed_pce = current drawn piece
                             */
                            if ((i_plane == 0)  && (p_sys->i_mouse_x == i_current_dst_x )
                                                && (p_sys->i_mouse_y == i_current_dst_y ))
                                p_sys->i_pointed_pce = i_pce;
                        }
                    }
                    i_sect_start_x += i_width;
                }
            }
        }
    }

    return;
}

/*****************************************************************************
 * draw all puzzle pieces on the desk
 *****************************************************************************/
void puzzle_draw_pieces( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out)
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if ((p_sys->ps_puzzle_array == NULL) || (p_sys->ps_pieces == NULL))
        return;

    for( uint8_t i_plane = 0; i_plane < p_pic_out->i_planes; i_plane++ ) {
        for ( int32_t i = p_sys->s_allocated.i_pieces_nbr-1; i >= 0 ; i-- ) {
            piece_t *ps_piece = &p_sys->ps_pieces[i];

            if (!p_sys->s_current_param.b_advanced
                    || (ps_piece->i_actual_mirror == 1 && ps_piece->i_actual_angle == 0
                    && p_sys->s_current_param.i_shape_size == 0))
            {
                puzzle_drw_basic_pce_in_plane(p_filter, p_pic_in, p_pic_out, i_plane, ps_piece);
            }
            else if ( ( p_sys->s_current_param.i_shape_size == 0)  || !p_sys->b_shape_init
                    || (p_sys->ps_pieces_shapes == NULL) || (!p_sys->b_shape_init) )
            {
                puzzle_drw_adv_pce_in_plane(p_filter, p_pic_in, p_pic_out, i_plane, ps_piece);
            }
            else {
                puzzle_drw_complex_pce_in_plane(p_filter, p_pic_in, p_pic_out, i_plane, ps_piece, i);
            }
        }
    }

    return;
}

/*****************************************************************************
 * when generating shape data: determine limit between sectors to be drawn
 *****************************************************************************/
int32_t puzzle_diagonal_limit( filter_t *p_filter, int32_t i_y, bool b_left, uint8_t i_plane )
{
    filter_sys_t *p_sys = p_filter->p_sys;

    if (b_left ^ (i_y >= p_sys->ps_desk_planes[i_plane].i_pce_max_lines / 2))
        return ( i_y * p_sys->ps_desk_planes[i_plane].i_pce_max_width) / p_sys->ps_desk_planes[i_plane].i_pce_max_lines;
    else
        return p_sys->ps_desk_planes[i_plane].i_pce_max_width - ( ( i_y * p_sys->ps_desk_planes[i_plane].i_pce_max_width) / p_sys->ps_desk_planes[i_plane].i_pce_max_lines);
}

#define MAX_SECT 10

/*****************************************************************************
 * generate data which will be used to draw each line of a piece sector with
 *       flat border
 *****************************************************************************/
int puzzle_generate_sect_border( filter_t *p_filter, piece_shape_t *ps_piece_shape, uint8_t i_plane, uint8_t i_border)
{
    /* generate data required to draw a sector of border puzzle piece */
    if (!ps_piece_shape)
        return VLC_EGENERIC;

    filter_sys_t *p_sys = p_filter->p_sys;

    int32_t i_width = p_sys->ps_desk_planes[i_plane].i_pce_max_width;
    int32_t i_lines = p_sys->ps_desk_planes[i_plane].i_pce_max_lines;

    /* process each horizontal pixel lines */
    int32_t i_min_y = (i_border != puzzle_SHAPE_BTM) ? 0 : floor( i_lines / 2 );

    int32_t i_nb_y = (i_border != puzzle_SHAPE_TOP)?
                        (i_lines - i_min_y) : (i_lines /2 - i_min_y);

    /* allocate memory */
    ps_piece_shape->i_row_nbr = i_nb_y;
    ps_piece_shape->i_first_row_offset = i_min_y;
    ps_piece_shape->ps_piece_shape_row = malloc( sizeof( piece_shape_row_t ) * i_nb_y );
    if (!ps_piece_shape->ps_piece_shape_row)
        return VLC_ENOMEM;

    for (int32_t i_y = i_min_y; i_y < i_nb_y + i_min_y; i_y++) {
        uint8_t i_sect = 0;
        int32_t pi_sects[MAX_SECT];
        int32_t i_row = i_y - i_min_y;

        /* ...fill from border to next junction */
        switch (i_border)
        {
          case puzzle_SHAPE_TOP:
          case puzzle_SHAPE_BTM:
            pi_sects[i_sect] = puzzle_diagonal_limit( p_filter, i_y, false, i_plane ) - 1
                            - (puzzle_diagonal_limit( p_filter, i_y, true, i_plane ) - 1);
            break;
          case puzzle_SHAPE_RIGHT:
            pi_sects[i_sect] = i_width - puzzle_diagonal_limit( p_filter, i_y, false, i_plane );
            break;
          case puzzle_SHAPE_LEFT:
          default:
            pi_sects[i_sect] = puzzle_diagonal_limit( p_filter, i_y, true, i_plane );
        }
        i_sect++;

        /* ...allocate memory and copy final values */
        ps_piece_shape->ps_piece_shape_row[i_row].i_section_nbr = i_sect;
        ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section = malloc ( sizeof(row_section_t) * i_sect);
        if (!ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section) {
            for (uint8_t i=0; i<i_row;i++)
                free(ps_piece_shape->ps_piece_shape_row[i].ps_row_section);
            free(ps_piece_shape->ps_piece_shape_row);
            ps_piece_shape->ps_piece_shape_row = NULL;
            return VLC_ENOMEM;
        }

        for (uint8_t i=0; i < i_sect; i++) {
            ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i].i_type = i % 2; /* 0 = fill ; 1 = offset */
            ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i].i_width = pi_sects[i];
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * generate data which will be used to draw each line of a piece sector based
 *       on bezier curve
 *****************************************************************************/
int puzzle_generate_sect_bezier( filter_t *p_filter, piece_shape_t *ps_piece_shape, uint8_t i_pts_nbr, point_t *ps_pt, uint8_t i_plane, uint8_t i_border)
{
    /* generate data required to draw a sector of puzzle piece using bezier shape */
    if ((!ps_pt) || (!ps_piece_shape))
        return VLC_EGENERIC;

    filter_sys_t *p_sys = p_filter->p_sys;

    int32_t i_width = p_sys->ps_desk_planes[i_plane].i_pce_max_width;
    int32_t i_lines = p_sys->ps_desk_planes[i_plane].i_pce_max_lines;
    int32_t i_size_x_0 = p_sys->ps_desk_planes[0].i_pce_max_width;
    int32_t i_size_y_0 = p_sys->ps_desk_planes[0].i_pce_max_lines;

    float f_x_ratio =  ((float) i_width) / ((float) i_size_x_0);
    float f_y_ratio = ((float) i_lines) / ((float) i_size_y_0);

    /* first: get min x and min y */
    float f_min_curve_x, f_min_curve_y;
    puzzle_get_min_bezier(&f_min_curve_x, &f_min_curve_y, f_x_ratio, f_y_ratio, ps_pt, i_pts_nbr);

    f_min_curve_y = __MIN(0,floor(f_min_curve_y));
    f_min_curve_x = __MIN(0,floor(f_min_curve_x));

    /* next: process each horizontal pixel lines */
    int32_t i_min_y = (i_border==puzzle_SHAPE_TOP)?floor(f_min_curve_y):0;
    int32_t i_nb_y = (i_border==puzzle_SHAPE_TOP)?(i_lines / 2 - i_min_y):i_lines;

    /* allocate memory */
    ps_piece_shape->i_row_nbr = i_nb_y;
    ps_piece_shape->i_first_row_offset = i_min_y;
    ps_piece_shape->ps_piece_shape_row = malloc( sizeof( piece_shape_row_t ) * ps_piece_shape->i_row_nbr );
    if (!ps_piece_shape->ps_piece_shape_row)
        return VLC_ENOMEM;

    return puzzle_generate_shape_lines(p_filter, ps_piece_shape, i_min_y, i_nb_y, f_x_ratio, f_y_ratio, ps_pt, i_pts_nbr, i_border, i_plane);
}

/*****************************************************************************
 * when generating shape data: determine minimum bezier value
 *****************************************************************************/
void puzzle_get_min_bezier(float *f_min_curve_x, float *f_min_curve_y, float f_x_ratio, float f_y_ratio, point_t *ps_pt, uint8_t i_pts_nbr)
{
    *f_min_curve_y = ps_pt[0].f_y * f_y_ratio;
    *f_min_curve_x = ps_pt[0].f_x * f_x_ratio;

    for (float f_t = 0; f_t <= i_pts_nbr - 1; f_t += 0.1 ) {
        int8_t i_main_t = floor(f_t);
        if ( i_main_t == i_pts_nbr - 1 )
            i_main_t = i_pts_nbr - 2;
        float f_sub_t = f_t - i_main_t;

        *f_min_curve_x = __MIN(*f_min_curve_x,bezier_val(ps_pt,f_sub_t,i_main_t,x) * f_x_ratio);
        *f_min_curve_y = __MIN(*f_min_curve_y,bezier_val(ps_pt,f_sub_t,i_main_t,y) * f_y_ratio);
    }
}

/*****************************************************************************
 * proceed with each line in order to generate data which will be used
 *     to draw each line of a piece sector
 *****************************************************************************/
int puzzle_generate_shape_lines( filter_t *p_filter, piece_shape_t *ps_piece_shape, int32_t i_min_y, int32_t i_nb_y, float f_x_ratio, float f_y_ratio, point_t *ps_pt, uint8_t i_pts_nbr, uint8_t i_border, uint8_t i_plane)
{
    /* generate data required to draw a line of a piece sector */
    for (int32_t i_y = i_min_y; i_y < i_nb_y + i_min_y; i_y++) {
        int32_t i_row = i_y - i_min_y;

        int32_t pi_sects[MAX_SECT];

        uint8_t i_sect = puzzle_detect_curve( p_filter, i_y, f_x_ratio, f_y_ratio, ps_pt, i_pts_nbr, i_border, i_plane, pi_sects);

        /* ...we have to convert absolute values to offsets and take into account min_curve_x */
        int8_t i_s = 0;
        int32_t i_last_x = (i_border==puzzle_SHAPE_TOP && (i_y>=0))?puzzle_diagonal_limit( p_filter, i_y, true, i_plane ):0;

        for (i_s = 0; i_s<i_sect; i_s++) {
            int32_t i_current_x = pi_sects[i_s];
            int32_t i_delta = i_current_x - i_last_x;
            pi_sects[i_s] = i_delta;

            i_last_x = i_current_x;
        }

        switch (i_border)
        {
          case puzzle_SHAPE_TOP:
            /* ...allocate memory and copy final values */
            /* note for y > 0 we have to ignore the first offset as it is included in "Left" piece shape */
            if ( i_y >= 0 ) {
                ps_piece_shape->ps_piece_shape_row[i_row].i_section_nbr = i_sect;
                ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section = malloc (  sizeof(row_section_t) * i_sect);
                if (!ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section) {
                    for (uint8_t i=0; i<i_row;i++)
                        free(ps_piece_shape->ps_piece_shape_row[i].ps_row_section);
                    free(ps_piece_shape->ps_piece_shape_row);
                    ps_piece_shape->ps_piece_shape_row = NULL;
                    return VLC_ENOMEM;
                }
                for (uint8_t i=0; i < i_sect; i++) {
                    ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i].i_type = i % 2; /* 0 = fill ; 1 = offset */
                    ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i].i_width = pi_sects[i];
                }
            }
            else {
                ps_piece_shape->ps_piece_shape_row[i_row].i_section_nbr = i_sect;
                ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section = malloc (  sizeof(row_section_t) * i_sect);
                if (!ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section) {
                    for (uint8_t i=0; i<i_row;i++)
                        free(ps_piece_shape->ps_piece_shape_row[i].ps_row_section);
                    free(ps_piece_shape->ps_piece_shape_row);
                    ps_piece_shape->ps_piece_shape_row = NULL;
                    return VLC_ENOMEM;
                }
                for (uint8_t i=0; i < i_sect; i++) {
                    ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i].i_type = (i + 1) % 2; /* 0 = fill ; 1 = offset */
                    ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i].i_width = pi_sects[i];
                }
            }
            break;
          case puzzle_SHAPE_LEFT:
            /* ...allocate memory and copy final values */
            /* note for y > 0 we have to ignore the first offset as it is included in "Left" piece shape */
            ps_piece_shape->ps_piece_shape_row[i_row].i_section_nbr = i_sect;
            ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section = malloc (  sizeof(row_section_t) * i_sect);
            if (!ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section) {
                for (uint8_t i=0; i<i_row;i++)
                    free(ps_piece_shape->ps_piece_shape_row[i].ps_row_section);
                free(ps_piece_shape->ps_piece_shape_row);
                ps_piece_shape->ps_piece_shape_row = NULL;
                return VLC_ENOMEM;
            }
            for (uint8_t i=0; i < i_sect; i++) {
                ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i].i_type = (i+1) % 2; /* 0 = fill ; 1 = offset */
                ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i].i_width = pi_sects[i];
            }
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * when generating shape data: detect all bezier curve intersections with
 * current line
 *****************************************************************************/
int puzzle_detect_curve( filter_t *p_filter, int32_t i_y, float f_x_ratio, float f_y_ratio, point_t *ps_pt, uint8_t i_pts_nbr, uint8_t i_border, uint8_t i_plane, int32_t *pi_sects)
{
    int8_t i_main_t = 0;
    float f_xd, f_yd;
    float f_xo = ps_pt[0].f_x * f_x_ratio;
    float f_yo = ps_pt[0].f_y * f_y_ratio;
    int8_t i_sect = 0;

    for (float f_t = 0; f_t <= i_pts_nbr - 1; f_t += 0.1 ) {
        i_main_t = floor(f_t);
        if ( i_main_t == i_pts_nbr - 1 )
            i_main_t = i_pts_nbr - 2;
        float f_sub_t = f_t - i_main_t;

        f_xd = bezier_val(ps_pt,f_sub_t,i_main_t,x) * f_x_ratio;
        f_yd = bezier_val(ps_pt,f_sub_t,i_main_t,y) * f_y_ratio;

        if ((f_yo < (float)i_y+0.5 && f_yd >= (float)i_y+0.5) || (f_yo > (float)i_y+0.5 && f_yd <= (float)i_y+0.5)) {
            pi_sects[i_sect] = floor(((float)i_y+0.5 - f_yo) * (f_xd - f_xo) / (f_yd - f_yo) + f_xo);
            if (i_sect < MAX_SECT - 1)
                i_sect++;
        }

        f_xo = f_xd;
        f_yo = f_yd;
    }
    f_xd = ps_pt[i_pts_nbr - 1].f_x * f_x_ratio;
    f_yd = ps_pt[i_pts_nbr - 1].f_y * f_y_ratio;

    /* ...fill from this junction to next junction */
    if ( i_y >= 0 ) {
        /* last diagonal intersection */
        pi_sects[i_sect] = (i_border==puzzle_SHAPE_TOP)?puzzle_diagonal_limit( p_filter, i_y, false, i_plane )
                                                       :puzzle_diagonal_limit( p_filter, i_y, true,  i_plane );
        if (i_sect < MAX_SECT - 1)
            i_sect++;
    }

    /* ...reorder the list of intersection */
    int32_t i_s = 0;

    while (i_s < (i_sect - 1)) {
        if (pi_sects[i_s] > pi_sects[i_s+1]) {
            uint32_t i_temp = pi_sects[i_s];
            pi_sects[i_s] = pi_sects[i_s+1];
            pi_sects[i_s+1] = i_temp;
            i_s = 0;
        }
        else {
            i_s++;
        }
    }

    return i_sect;
}

/*****************************************************************************
 * generate Right shape data from Left shape data
 *****************************************************************************/
int puzzle_generate_sectLeft2Right( filter_t *p_filter, piece_shape_t *ps_piece_shape, piece_shape_t *ps_left_piece_shape, uint8_t i_plane)
{
    if ((!ps_piece_shape) || (!ps_left_piece_shape))
        return VLC_EGENERIC;

    filter_sys_t *p_sys = p_filter->p_sys;

    int32_t i_min_y = ps_left_piece_shape->i_first_row_offset;
    int32_t i_nb_y = ps_left_piece_shape->i_row_nbr;

    /* allocate memory */
    ps_piece_shape->i_row_nbr = i_nb_y;
    ps_piece_shape->i_first_row_offset = i_min_y;
    ps_piece_shape->ps_piece_shape_row = malloc( sizeof( piece_shape_row_t ) * i_nb_y );
    if (!ps_piece_shape->ps_piece_shape_row)
        return VLC_ENOMEM;

    for (int32_t i_y = i_min_y; i_y < i_nb_y + i_min_y; i_y++) {
        int32_t i_row = i_y - i_min_y;

        int32_t i_width = p_sys->ps_desk_planes[i_plane].i_pce_max_width;
        int32_t i_left_width = puzzle_diagonal_limit( p_filter, i_y, true, i_plane  );
        int32_t i_right_width = i_width - puzzle_diagonal_limit( p_filter, i_y, false, i_plane );
        int16_t i_section_nbr = ps_left_piece_shape->ps_piece_shape_row[i_row].i_section_nbr;

        ps_piece_shape->ps_piece_shape_row[i_row].i_section_nbr = i_section_nbr;
        ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section = malloc (  sizeof(row_section_t) * i_section_nbr);
        if (!ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section) {
            for (uint8_t i=0; i<i_row;i++)
                free(ps_piece_shape->ps_piece_shape_row[i].ps_row_section);
            free(ps_piece_shape->ps_piece_shape_row);
            ps_piece_shape->ps_piece_shape_row = NULL;
            return VLC_ENOMEM;
        }

        ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[0].i_type =
                ps_left_piece_shape->ps_piece_shape_row[i_row].ps_row_section[0].i_type;
        ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[0].i_width =
                ps_left_piece_shape->ps_piece_shape_row[i_row].ps_row_section[0].i_width + i_right_width - i_left_width;

        for (int8_t i_s=0; i_s<i_section_nbr;i_s++) {
            ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i_s].i_type =
                    ps_left_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i_section_nbr - 1 - i_s].i_type;
            ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i_s].i_width =
                    ps_left_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i_section_nbr - 1 - i_s].i_width
                    + (i_s == 0 ? i_right_width - i_left_width : 0);
        }
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * generates Bottom shape data from Top shape data
 *****************************************************************************/
int puzzle_generate_sectTop2Btm( filter_t *p_filter, piece_shape_t *ps_piece_shape, piece_shape_t *ps_top_piece_shape, uint8_t i_plane)
{
    if ((!ps_piece_shape) || (!ps_top_piece_shape))
        return VLC_EGENERIC;

    filter_sys_t *p_sys = p_filter->p_sys;

    int32_t i_top_min_y = ps_top_piece_shape->i_first_row_offset;
    int32_t i_top_nb_y = ps_top_piece_shape->i_row_nbr;
    int32_t i_lines = p_sys->ps_desk_planes[i_plane].i_pce_max_lines;
    int32_t i_max_y = p_sys->ps_desk_planes[i_plane].i_pce_max_lines - i_top_min_y;

    int32_t i_min_y = i_lines / 2;
    int32_t i_nb_y = i_max_y - i_min_y;

    /* allocate memory */
    ps_piece_shape->i_row_nbr = i_nb_y;
    ps_piece_shape->i_first_row_offset = i_min_y;
    ps_piece_shape->ps_piece_shape_row = malloc( sizeof( piece_shape_row_t ) * i_nb_y );
    if (!ps_piece_shape->ps_piece_shape_row)
        return VLC_ENOMEM;

    for (int32_t i_y = i_min_y; i_y < i_nb_y + i_min_y; i_y++) {
        int32_t i_top_y = 2 * i_min_y - i_y + (i_nb_y - i_top_nb_y);
        int32_t i_row = i_y - i_min_y;
        int32_t i_top_row = i_top_y - i_top_min_y;

        if ( i_top_row < 0 || i_top_row >= i_top_nb_y ) { /* the line does not exist in top */
            ps_piece_shape->ps_piece_shape_row[i_row].i_section_nbr = 1;
            ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section = malloc (  sizeof(row_section_t) * 1);
            if (!ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section) {
                for (uint8_t i=0; i<i_row;i++)
                    free(ps_piece_shape->ps_piece_shape_row[i].ps_row_section);
                free(ps_piece_shape->ps_piece_shape_row);
                ps_piece_shape->ps_piece_shape_row = NULL;
                return VLC_ENOMEM;
            }
            ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[0].i_type = 0;
            ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[0].i_width =
                puzzle_diagonal_limit( p_filter, i_y, false, i_plane ) - 1 - (puzzle_diagonal_limit( p_filter, i_y, true, i_plane ) - 1);
        }
        else { /* copy the line from TopShape */
            int32_t i_top_width =
                puzzle_diagonal_limit( p_filter, i_top_y, false, i_plane ) - 1 - (puzzle_diagonal_limit( p_filter, i_top_y, true, i_plane ) - 1);
            int32_t i_width =
                puzzle_diagonal_limit( p_filter, i_y, false, i_plane ) - 1 - (puzzle_diagonal_limit( p_filter, i_y, true, i_plane ) - 1);
            int32_t i_left_adjust = ( i_width - i_top_width ) / 2;
            int32_t i_right_adjust = ( i_width - i_top_width ) - i_left_adjust;

            int8_t i_section_nbr = ps_top_piece_shape->ps_piece_shape_row[i_top_row].i_section_nbr;
            ps_piece_shape->ps_piece_shape_row[i_row].i_section_nbr = i_section_nbr;
            ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section = malloc (  sizeof(row_section_t) * i_section_nbr);
            if (!ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section) {
                for (uint8_t i=0; i<i_row;i++)
                    free(ps_piece_shape->ps_piece_shape_row[i].ps_row_section);
                free(ps_piece_shape->ps_piece_shape_row);
                ps_piece_shape->ps_piece_shape_row = NULL;
                return VLC_ENOMEM;
            }

            for (int8_t i_s=0; i_s<i_section_nbr; i_s++) {
                ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i_s].i_type =
                        ps_top_piece_shape->ps_piece_shape_row[i_top_row].ps_row_section[i_s].i_type;
                ps_piece_shape->ps_piece_shape_row[i_row].ps_row_section[i_s].i_width =
                        ps_top_piece_shape->ps_piece_shape_row[i_top_row].ps_row_section[i_s].i_width
                        + (i_s == 0 ? i_left_adjust : (i_s == i_section_nbr-1 ? i_right_adjust : 0));
            }
        }
    }
    return VLC_SUCCESS;
}
