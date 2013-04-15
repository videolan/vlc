/*****************************************************************************
 * puzzle_lib.h : Useful functions used by puzzle game filter
 *****************************************************************************
 * Copyright (C) 2005-2009 VLC authors and VideoLAN
 * Copyright (C) 2013      Vianney Boyer
 * $Id$
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

#ifndef VLC_LIB_PUZZLE_H
#define VLC_LIB_PUZZLE_H 1


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

#include "puzzle.h"

void puzzle_preset_desk_background(picture_t *p_pic_out, uint8_t Y, uint8_t U, uint8_t V);
void puzzle_draw_borders( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out);
void puzzle_draw_preview( filter_t *p_filter, picture_t *p_pic_in, picture_t *p_pic_out);
void puzzle_draw_sign(picture_t *p_pic_out, int32_t i_x, int32_t i_y, int32_t i_width, int32_t i_lines, const char **ppsz_sign, bool b_reverse);
void puzzle_draw_rectangle(picture_t *p_pic_out, int32_t x, int32_t y, int32_t i_w, int32_t i_h, uint8_t Y, uint8_t U, uint8_t V );
void puzzle_fill_rectangle(picture_t *p_pic_out, int32_t x, int32_t y, int32_t i_w, int32_t i_h, uint8_t Y, uint8_t U, uint8_t V );
static inline int32_t init_countdown(int32_t init_val) {
    return ( ( __MAX( 1, 30000 - init_val)/20 ) / 2 + ((unsigned) vlc_mrand48() ) % ( __MAX( 1, ((30000 - init_val)/20) ) ) ); }

#define SHUFFLE_WIDTH 81
#define SHUFFLE_LINES 13
extern const char *ppsz_shuffle_button[SHUFFLE_LINES];

#define ARROW_WIDTH 13
#define ARROW_LINES 13
extern const char *ppsz_rot_arrow_sign[ARROW_LINES];
extern const char *ppsz_mir_arrow_sign[ARROW_LINES];

#endif
