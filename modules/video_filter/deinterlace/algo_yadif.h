/*****************************************************************************
 * algo_yadif.h : Wrapper for FFmpeg's Yadif algorithm
 *****************************************************************************
 * Copyright (C) 2000-2011 the VideoLAN team
 *
 * Author: Laurent Aimar <fenrir@videolan.org>
 *         Juha Jeronen  <juha.jeronen@jyu.fi> (soft field repeat hack)
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

#ifndef VLC_DEINTERLACE_ALGO_YADIF_H
#define VLC_DEINTERLACE_ALGO_YADIF_H 1

/**
 * \file
 * Adapter to fit the Yadif (Yet Another DeInterlacing Filter) algorithm
 * from FFmpeg into VLC. The algorithm itself is implemented in yadif.h.
 */

/* Forward declarations */
struct filter_t;
struct picture_t;

/*****************************************************************************
 * Functions
 *****************************************************************************/

/**
 * Yadif (Yet Another DeInterlacing Filter) from FFmpeg.
 * One field is copied as-is (i_field), the other is interpolated.
 *
 * Comes with both interpolating and framerate doubling modes.
 *
 * If you do NOT want to use framerate doubling: use i_order = 0,
 * and either 0 or 1 for i_field (keep it constant),
 *
 * If you DO want framerate doubling, do as instructed below.
 *
 * See Deinterlace() for usage examples of both modes.
 *
 * Needs three frames in the history buffer to operate.
 * The first-ever frame is rendered using RenderX().
 * The second is dropped. At the third frame, Yadif starts.
 *
 * Once Yadif starts, the frame that is rendered corresponds to the *previous*
 * input frame (i_frame_offset = 1), complete with its original PTS.
 * The latest input frame is used as the future/next frame, as reference
 * for temporal interpolation.
 *
 * This wrapper adds support for soft field repeat (repeat_pict).
 * Note that the generated "repeated" output picture is unique because
 * of temporal interpolation.
 *
 * As many output frames should be requested for each input frame as is
 * indicated by p_src->i_nb_fields. This is done by calling this function
 * several times, first with i_order = 0, and then with all other parameters
 * the same, but a new p_dst, increasing i_order (1 for second field,
 * and then if i_nb_fields = 3, also i_order = 2 to get the repeated first
 * field), and alternating i_field (starting, at i_order = 0, with the field
 * according to p_src->b_top_field_first). See Deinterlace() for an example.
 *
 * @param p_filter The filter instance. Must be non-NULL.
 * @param p_dst Output frame. Must be allocated by caller.
 * @param p_src Input frame. Must exist.
 * @param i_order Temporal field number: 0 = first, 1 = second, 2 = rep. first.
 * @param i_field Keep which field? 0 = top field, 1 = bottom field.
 * @return VLC error code (int).
 * @retval VLC_SUCCESS The requested field was rendered into p_dst.
 * @retval VLC_EGENERIC Frame dropped; only occurs at the second frame after start.
 * @see Deinterlace()
 */
int RenderYadif( filter_t *p_filter, picture_t *p_dst, picture_t *p_src,
                 int i_order, int i_field );

/**
 * Same as RenderYadif() but with no temporal references
 */
int RenderYadifSingle( filter_t *p_filter, picture_t *p_dst, picture_t *p_src );

#endif
