/*****************************************************************************
 * common.h : Common macros for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2000-2017 VLC authors and VideoLAN
 *
 * Author: Sam Hocevar <sam@zoy.org>
 *         Steve Lhomme <robux4@gmail.com>
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

#ifndef VLC_DEINTERLACE_COMMON_H
#define VLC_DEINTERLACE_COMMON_H 1

#include <vlc_common.h>
#include <vlc_filter.h>

#include <assert.h>

/**
 * \file
 * Common macros for the VLC deinterlacer.
 */

/* Needed for Yadif, but also some others. */
#define FFMAX(a,b)      __MAX(a,b)
#define FFMAX3(a,b,c)   FFMAX(FFMAX(a,b),c)
#define FFMIN(a,b)      __MIN(a,b)
#define FFMIN3(a,b,c)   FFMIN(FFMIN(a,b),c)

/**
 * Metadata history structure, used for framerate doublers.
 * This is used for computing field duration in Deinterlace().
 * @see Deinterlace()
 */
typedef struct {
    vlc_tick_t pi_date;
    int     pi_nb_fields;
    bool    pb_top_field_first;
} metadata_history_t;

#define METADATA_SIZE (3)
#define HISTORY_SIZE (3)

typedef struct  {
    bool b_double_rate;       /**< Shall we double the framerate? */
    bool b_use_frame_history; /**< Use the input frame history buffer? */
    bool b_custom_pts;        /**< for inverse telecine */
    bool b_half_height;       /**< Shall be divide the height by 2 */
} deinterlace_algo;

struct deinterlace_ctx
{
    /* Algorithm behaviour flags */
    deinterlace_algo   settings;

    /**
     * Metadata history (PTS, nb_fields, TFF). Used for framerate doublers.
     * @see metadata_history_t
     */
    metadata_history_t meta[METADATA_SIZE];

    /** Output frame timing / framerate doubler control
        (see extra documentation in deinterlace.h) */
    int i_frame_offset;

    /** Input frame history buffer for algorithms with temporal filtering. */
    picture_t *pp_history[HISTORY_SIZE];

    union {
        /**
         * @param i_order Temporal field number: 0 = first, 1 = second, 2 = repeat first.
         * @param i_field Keep which field? 0 = top field, 1 = bottom field.
         */
        int (*pf_render_ordered)(filter_t *, picture_t *p_dst, picture_t *p_pic,
                                 int order, int i_field);
        int (*pf_render_single_pic)(filter_t *, picture_t *p_dst, picture_t *p_pic);
    };
};

#define DEINTERLACE_DST_SIZE 3

void InitDeinterlacingContext( struct deinterlace_ctx * );

/**
 * @brief Get the field duration based on the previous fields or the frame rate
 * @param fmt output format of the deinterlacer with the frame rate
 * @param p_pic the picture which field we want the duration
 * @return the duration of the field or 0 if there's no known framerate
 */
vlc_tick_t GetFieldDuration( const struct deinterlace_ctx *,
                          const video_format_t *fmt, const picture_t *p_pic );

/**
 * @brief Get the output video_format_t configured for the deinterlacer
 * @param p_dst video_format_t to fill
 * @param p_src source video_format_t
 */
void GetDeinterlacingOutput( const struct deinterlace_ctx *,
                             video_format_t *p_dst, const video_format_t *p_src );

/**
 * @brief Do the deinterlacing of the picture using pf_render_ordered() or pf_render_single_pic() calls.
 * @return The deinterlaced picture or NULL if it failed
 */
picture_t *DoDeinterlacing( filter_t *, struct deinterlace_ctx *, picture_t * );

/**
 * @brief Flush the deinterlacer context
 */
void FlushDeinterlacing( struct deinterlace_ctx * );

picture_t *AllocPicture( filter_t * );

#endif
