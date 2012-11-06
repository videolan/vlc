/*****************************************************************************
 * adjust_sat_hue.h : Hue/Saturation executive part of adjust plugin for vlc
 *****************************************************************************
 * Copyright (C) 2011 VideoLAN
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

#include <vlc_common.h>
#include <vlc_cpu.h>

/**
 * Functions processing saturation and hue of adjust filter.
 * Prototype and parameters stay the same across different variations.
 *
 * @param p_pic Source picture
 * @param p_outpic Destination picture
 * @param i_sin Sinus value of hue
 * @param i_cos Cosinus value of hue
 * @param i_sat Saturation
 * @param i_x Additional value of saturation
 * @param i_y Additional value of saturation
 */

/**
 * Basic C compiler generated function for planar format, i_sat > 256
 */
int planar_sat_hue_clip_C( picture_t * p_pic, picture_t * p_outpic,
                           int i_sin, int i_cos, int i_sat, int i_x, int i_y );

/**
 * Basic C compiler generated function for planar format, i_sat <= 256
 */
int planar_sat_hue_C( picture_t * p_pic, picture_t * p_outpic,
                      int i_sin, int i_cos, int i_sat, int i_x, int i_y );

/**
 * Basic C compiler generated function for packed format, i_sat > 256
 */
int packed_sat_hue_clip_C( picture_t * p_pic, picture_t * p_outpic,
                           int i_sin, int i_cos, int i_sat, int i_x, int i_y );

/**
 * Basic C compiler generated function for packed format, i_sat <= 256
 */
int packed_sat_hue_C( picture_t * p_pic, picture_t * p_outpic,
                      int i_sin, int i_cos, int i_sat, int i_x, int i_y );
