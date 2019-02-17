/*****************************************************************************
 * puzzle_pce.h : Puzzle game filter - pieces functions
 *****************************************************************************
 * Copyright (C) 2013 Vianney Boyer
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
#ifndef VLC_LIB_PUZZLE_PCE_H
#define VLC_LIB_PUZZLE_PCE_H 1

#include <vlc_common.h>
#include <vlc_filter.h>

#include "puzzle_bezier.h"
#include "puzzle_lib.h"

#define SHAPES_QTY 20
#define PIECE_TYPE_NBR (4*2*(1+SHAPES_QTY))

#define puzzle_SHAPE_TOP   1
#define puzzle_SHAPE_LEFT  2
#define puzzle_SHAPE_RIGHT 4
#define puzzle_SHAPE_BTM   8

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct {
    uint8_t i_type;  /* 0 = fill ; 1 = offset */
    int32_t i_width;
} row_section_t;

typedef struct {
    int32_t i_section_nbr;
    row_section_t *ps_row_section;
} piece_shape_row_t;

typedef struct {
    int32_t i_row_nbr;
    int32_t i_first_row_offset;
    piece_shape_row_t *ps_piece_shape_row;
} piece_shape_t;

typedef struct {
    int32_t i_original_x, i_original_y;
    int32_t i_actual_x, i_actual_y;
    int32_t i_width, i_lines;
} piece_in_plane_t;

typedef struct {
    int32_t i_original_row, i_original_col;

    int32_t i_top_shape, i_btm_shape, i_right_shape, i_left_shape;

    piece_in_plane_t *ps_piece_in_plane;

    bool b_finished;
    bool b_overlap;

    int8_t i_actual_angle;                   /* 0 = 0°, 1 = 90°... rotation center = top-left corner            */
    int32_t i_actual_mirror;                 /* +1 = without mirror ; -1 = with mirror                          */
    int32_t i_step_x_x, i_step_x_y, i_step_y_y, i_step_y_x;
    int32_t i_ORx, i_OTy, i_OLx, i_OBy;      /* location of original piece's edges                              */
    int32_t i_TLx, i_TLy, i_TRx, i_TRy, i_BLx, i_BLy, i_BRx, i_BRy; /* location of grabed piece's corners       */
    int32_t i_max_x, i_min_x, i_max_y, i_min_y, i_center_x, i_center_y;

    uint32_t i_group_ID;
} piece_t;

int  puzzle_bake_pieces_shapes ( filter_t * );
void puzzle_free_ps_pieces_shapes ( filter_t * );

int  puzzle_find_piece( filter_t *p_filter, int32_t i_x, int32_t i_y, int32_t i_except);
void puzzle_calculate_corners( filter_t *,  int32_t i_piece );
void puzzle_rotate_pce( filter_t *p_filter, int32_t i_piece, int8_t i_rotate_mirror, int32_t i_center_x, int32_t i_center_y, bool b_avoid_mirror );
void puzzle_move_group( filter_t *p_filter, int32_t i_piece, int32_t i_dx, int32_t i_dy);

void puzzle_drw_basic_pce_in_plane( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out, uint8_t i_plane, piece_t *ps_piece);
void puzzle_drw_adv_pce_in_plane( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out, uint8_t i_plane, piece_t *ps_piece);
void puzzle_drw_complex_pce_in_plane( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out, uint8_t i_plane, piece_t *ps_piece, uint32_t i_pce);
void puzzle_draw_pieces( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out);
int32_t puzzle_diagonal_limit( filter_t *p_filter, int32_t i_y, bool b_left, uint8_t i_plane );

int puzzle_generate_sect_border( filter_t *p_filter, piece_shape_t *ps_piece_shape, uint8_t i_plane, uint8_t i_border);
int puzzle_generate_sect_bezier( filter_t *p_filter, piece_shape_t *ps_piece_shape, uint8_t i_pts_nbr, point_t *ps_pt, uint8_t i_plane, uint8_t i_border);
void puzzle_get_min_bezier(float *f_min_curve_x, float *f_min_curve_y, float f_x_ratio, float f_y_ratio, point_t *ps_pt, uint8_t i_pts_nbr);
int puzzle_generate_shape_lines( filter_t *p_filter, piece_shape_t *ps_piece_shape, int32_t i_min_y, int32_t i_nb_y, float f_x_ratio, float f_y_ratio, point_t *ps_pt, uint8_t i_pts_nbr, uint8_t i_border, uint8_t i_plane);
int puzzle_detect_curve( filter_t *p_filter, int32_t i_y, float f_x_ratio, float f_y_ratio, point_t *ps_pt, uint8_t i_pts_nbr, uint8_t i_border, uint8_t i_plane, int32_t *pi_sects);
int puzzle_generate_sectLeft2Right( filter_t *p_filter, piece_shape_t *ps_piece_shape, piece_shape_t *ps_left_piece_shape, uint8_t i_plane);
int puzzle_generate_sectTop2Btm( filter_t *p_filter, piece_shape_t *ps_piece_shape, piece_shape_t *ps_top_piece_shape, uint8_t i_plane);

#endif
