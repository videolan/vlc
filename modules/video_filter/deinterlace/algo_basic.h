/*****************************************************************************
 * algo_basic.h : Basic algorithms for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2000-2011 VLC authors and VideoLAN
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Damien Lucas <nitrox@videolan.org>  (Bob, Blend)
 *         Laurent Aimar <fenrir@videolan.org> (Bob, Blend)
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

#ifndef VLC_DEINTERLACE_ALGO_BASIC_H
#define VLC_DEINTERLACE_ALGO_BASIC_H 1

/**
 * \file
 * Basic deinterlace algorithms: Discard, Bob, Linear, Mean and Blend.
 */

/* Forward declarations */
struct filter_t;
struct picture_t;

/*****************************************************************************
 * Functions
 *****************************************************************************/

/**
 * RenderDiscard: only keep top or bottom field, discard the other.
 *
 * For a 2x (framerate-doubling) near-equivalent, see RenderBob().
 *
 * @param p_outpic Output frame. Must be allocated by caller.
 * @param p_pic Input frame. Must exist.
 * @see RenderBob()
 * @see Deinterlace()
 */
int RenderDiscard( filter_t *, picture_t *p_outpic, picture_t *p_pic );

/**
 * RenderBob: basic framerate doubler.
 *
 * Creates an illusion of full vertical resolution while running.
 *
 * For a 1x (non-doubling) near-equivalent, see RenderDiscard().
 *
 * @param p_outpic Output frame. Must be allocated by caller.
 * @param p_pic Input frame. Must exist.
 * @param i_field Render which field? 0 = top field, 1 = bottom field.
 * @see RenderLinear()
 * @see Deinterlace()
 */
int RenderBob( filter_t *,
               picture_t *p_outpic, picture_t *p_pic, int order, int i_field );

/**
 * RenderLinear: Bob with linear interpolation.
 *
 * There is no 1x (non-doubling) equivalent for this filter.
 *
 * @param p_filter The filter instance. Must be non-NULL.
 * @param p_outpic Output frame. Must be allocated by caller.
 * @param p_pic Input frame. Must exist.
 * @param i_field Render which field? 0 = top field, 1 = bottom field.
 * @see RenderBob()
 * @see Deinterlace()
 */
int RenderLinear( filter_t *p_filter,
                  picture_t *p_outpic, picture_t *p_pic, int order, int i_field );

/**
 * RenderMean: half-resolution blender.
 *
 * Renders the mean of the top and bottom fields.
 *
 * Obviously, there is no 2x equivalent for this filter.
 *
 * @param p_filter The filter instance. Must be non-NULL.
 * @param p_outpic Output frame. Must be allocated by caller.
 * @param p_pic Input frame. Must exist.
 * @see Deinterlace()
 */
int RenderMean( filter_t *p_filter, picture_t *p_outpic, picture_t *p_pic );

/**
 * RenderBlend: full-resolution blender.
 *
 * The first line is copied; for the rest of the lines, line N
 * is the mean of lines N and N-1 in the input.
 *
 * Obviously, there is no 2x equivalent for this filter.
 *
 * @param p_filter The filter instance. Must be non-NULL.
 * @param p_outpic Output frame. Must be allocated by caller.
 * @param p_pic Input frame. Must exist.
 * @see Deinterlace()
 */
int RenderBlend( filter_t *p_filter, picture_t *p_outpic, picture_t *p_pic );

#endif
