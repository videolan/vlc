/*****************************************************************************
 * puzzle_mgt.h : Puzzle game filter - game management
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 * Copyright (C) 2013      Vianney Boyer
 *
 * Authors: Vianney Boyer <vlcvboyer -at- gmail -dot- com>
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
#ifndef VLC_LIB_PUZZLE_MGT_H
#define VLC_LIB_PUZZLE_MGT_H 1

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_filter.h>
#include <vlc_rand.h>

#include "puzzle_bezier.h"
#include "puzzle_lib.h"
#include "puzzle_pce.h"
#include "puzzle_mgt.h"

#define NO_PCE -1


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct filter_sys_t filter_sys_t;

typedef struct {
    int32_t i_preview_width, i_preview_lines;
    int32_t i_border_width, i_border_lines;
    int32_t i_pce_max_width, i_pce_max_lines;
    int32_t i_width, i_lines, i_pitch, i_visible_pitch;
    uint8_t i_pixel_pitch;
} puzzle_plane_t;

typedef struct {
    int32_t i_x, i_y;
    int32_t i_width, i_lines;
} puzzle_array_t;

typedef struct {
    int32_t i_original_row, i_original_col;
    int32_t i_top_shape, i_btm_shape, i_right_shape, i_left_shape;
    float f_pos_x, f_pos_y;
    int8_t i_actual_angle;                   /* 0 = 0°, 1 = 90°... rotation center = top-left corner */
    int32_t i_actual_mirror;                 /* +1 = without mirror ; -1 = with mirror               */
} save_piece_t;

typedef struct {
    int32_t i_rows, i_cols;
    uint8_t i_rotate;
    save_piece_t *ps_pieces;
} save_game_t;

typedef struct {
    int32_t i_rows, i_cols;
    int32_t i_pict_width, i_pict_height;
    int32_t i_desk_width, i_desk_height;
    int32_t i_piece_types;
    uint32_t i_pieces_nbr;
    int32_t i_preview_size;
    int32_t i_shape_size;
    int32_t i_border;
    uint8_t i_planes;
    /* game settings */
    bool    b_preview;
    bool b_blackslot;
    bool b_near;
    bool b_advanced;
    uint8_t i_mode;
    uint8_t i_rotate;   /* 0=none, 1=0/180, 2=0/90/180/270, 3=0/90/180/270 w/ mirror */
    int32_t i_auto_shuffle_speed, i_auto_solve_speed;
} param_t;

int  puzzle_bake ( filter_t *, picture_t * , picture_t * );
void puzzle_free_ps_puzzle_array ( filter_t * );
int  puzzle_bake_piece ( filter_t * );
void puzzle_set_left_top_shapes( filter_t *p_filter);
void puzzle_random_rotate( filter_t *p_filter);
void puzzle_free_ps_pieces ( filter_t * );
int  puzzle_allocate_ps_pieces( filter_t *p_filter);

bool puzzle_is_valid( filter_sys_t *p_sys, int32_t *pi_pce_lst );
int  puzzle_shuffle( filter_t * );
int  puzzle_generate_rand_pce_list( filter_t *p_filter, int32_t **pi_pce_lst );
bool puzzle_is_finished( filter_sys_t *, int32_t *pi_pce_lst );
int  puzzle_piece_foreground( filter_t *p_filter, int32_t i_piece);
void puzzle_count_pce_group( filter_t *p_filter);
void puzzle_solve_pces_group( filter_t *p_filter);
void puzzle_solve_pces_accuracy( filter_t *p_filter);
int  puzzle_sort_layers( filter_t *p_filter);

void puzzle_auto_solve( filter_t *p_filter);
void puzzle_auto_shuffle( filter_t *p_filter);

save_game_t* puzzle_save(filter_t *p_filter);
void         puzzle_load( filter_t *p_filter, save_game_t *ps_save_game);

#endif
