/*****************************************************************************
 * puzzle.h : Puzzle game
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 * Copyright (C) 2013      Vianney Boyer
 * $Id$
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

#ifndef VLC_PUZZLE_H
#define VLC_PUZZLE_H 1

#include "puzzle_mgt.h"

struct filter_sys_t {
    bool b_init;
    bool b_bake_request;
    bool b_shape_init;
    bool b_change_param;
    bool b_finished;
    bool b_shuffle_rqst;
    bool b_mouse_drag;
    bool b_mouse_mvt;

    param_t s_allocated;
    param_t s_current_param;
    param_t s_new_param;

    uint32_t i_done_count, i_tmp_done_count;

    int32_t i_mouse_drag_pce;
    int32_t i_mouse_x, i_mouse_y;
    int16_t i_pointed_pce;
    int8_t  i_mouse_action;

    uint32_t i_solve_acc_loop, i_solve_grp_loop, i_calc_corn_loop;
    int32_t i_magnet_accuracy;
    int32_t *pi_group_qty;

    int32_t *pi_order;                 /* array which contains final pieces location (used in BASIC GAME MODE)                              */
    puzzle_array_t ***ps_puzzle_array; /* array [row][col][plane] preset of location & size of each piece in the original image             */
    piece_shape_t **ps_pieces_shapes;  /* array [each piece type (PCE_TYPE_NBR * negative * 4: top...)][each plane] of piece definition     */
    piece_t *ps_pieces;                /* list [piece] of pieces data.                                                                      */
    piece_t *ps_pieces_tmp;            /* used when sorting layers                                                                          */

    puzzle_plane_t *ps_desk_planes;
    puzzle_plane_t *ps_pict_planes;

    uint8_t i_preview_pos;
    int32_t i_selected;

    vlc_mutex_t lock, pce_lock;

    int32_t i_auto_shuffle_countdown_val, i_auto_solve_countdown_val;

    point_t **ps_bezier_pts_H;
};

picture_t *Filter( filter_t *, picture_t * );
int puzzle_Callback( vlc_object_t *, char const *, vlc_value_t, vlc_value_t, void * );
int puzzle_mouse( filter_t *, vlc_mouse_t *, const vlc_mouse_t *, const vlc_mouse_t * );

#endif
