/*****************************************************************************
 * puzzle_bezier.h : Bezier curves management
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

#ifndef VLC_LIB_BEZIER_H
#define VLC_LIB_BEZIER_H 1

typedef struct {
        float f_x, f_y;
 } point_t;

point_t *puzzle_scale_curve_H(int32_t i_width, int32_t i_lines, uint8_t i_pts_nbr, point_t *ps_pt, int32_t i_shape_size);
point_t *puzzle_H_2_scale_curve_V(int32_t i_width, int32_t i_lines, uint8_t i_pts_nbr, point_t *ps_pt, int32_t i_shape_size);
point_t *puzzle_curve_H_2_V(uint8_t i_pts_nbr, point_t *ps_pt);
point_t *puzzle_curve_H_2_negative(uint8_t i_pts_nbr, point_t *ps_pt);
point_t *puzzle_curve_V_2_negative(uint8_t i_pts_nbr, point_t *ps_pt);
point_t *puzzle_rand_bezier(uint8_t i_pts_nbr);

#define bezier_val(ps_pt,f_sub_t,i_main_t,axis) (( 1 - (f_sub_t))  * ( 1 - (f_sub_t) ) * ( 1 - (f_sub_t) ) * ps_pt[ 3 * (i_main_t)     ].f_ ## axis \
                                                +  3 * (f_sub_t)   * ( 1 - (f_sub_t) ) * ( 1 - (f_sub_t) ) * ps_pt[ 3 * (i_main_t) + 1 ].f_ ## axis \
                                                +  3 * (f_sub_t)   * (f_sub_t)         * ( 1 - (f_sub_t) ) * ps_pt[ 3 * (i_main_t) + 2 ].f_ ## axis \
                                                +      (f_sub_t)   * (f_sub_t)         * (f_sub_t)         * ps_pt[ 3 * (i_main_t) + 3 ].f_ ## axis )

#endif
